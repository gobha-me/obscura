#!/usr/bin/env python3
"""venice-bake — the art plate baker.

A plate is the pixel art a compartment draws once its Resolution has been
earned: 240x160, four inks over a transparent index, ordered dither, PNG
colour type 3.  This script is where one is authored.  It runs at AUTHOR time
and only at author time — the build never invokes it, CI never invokes it, and
the game certainly never invokes it, because OBSCURA does not generate assets
at runtime.  What ships is the committed PNG under assets/plates/.

Why a script and not a C++ tool: a real PNG needs DEFLATE, and the repo has no
package manager and no compression dependency.  python3's standard library has
zlib, this file needs nothing else, and termforge's tools/png_repro.sh set the
precedent for exactly this trade.  Nothing downstream of the committed PNG can
tell how it was made.

Two modes, and the second one is the load-bearing one:

    bake.py --out   assets/plates/hold-d0.png     # author it
    bake.py --check assets/plates/hold-d0.png     # prove it is still the one

Without --check, editing the palette here and forgetting to re-bake is green
forever: the committed asset is the single source of truth and nothing would
compare it to its own recipe.  ctest runs --check as `plate-bake-check`.

--check compares the DECOMPRESSED scanlines, palette and transparency chunk,
never the compressed bytes.  zlib's output at level 9 is not guaranteed stable
across zlib versions, so a byte-for-byte comparison would go red on a perfectly
good asset the first time a runner's zlib moved.

No floating point anywhere below.  Not because this is [SIM] — it is not, it is
offline authoring — but because a plate that bakes differently on two machines
is a diff nobody can review, and integer dither is no harder to write.
"""

import argparse
import os
import struct
import sys
import zlib

# ── the plate ───────────────────────────────────────────────────────────────
#
# This file holds ONE recipe. `level()` below draws the hold at damage 0 and
# nothing else, so the path passed to --out or --check is checked against this
# name rather than trusted: without that, baking to corridor-d2.png would
# cheerfully write a byte-identical hold plate under the corridor's name, and
# the corridor would render as a cargo hold with nothing anywhere reporting a
# problem.
#
# Adding the second plate means giving this module a recipe table keyed on the
# name, not editing the constants in place — editing in place turns
# plate-bake-check red on the plate you did not touch.
PLATE_NAME = "hold-d0"

WIDTH = 240
HEIGHT = 160

# Index 0 is the hole, 1..4 are the inks, darkest first.  A cold desaturated
# ramp: this is a cargo hold lit by whatever the deck lamps still manage, and
# four colours is an art direction before it is a compression trick — it is
# what makes a resolved compartment read as a plate rather than a photograph.
PALETTE = [
    (0x00, 0x00, 0x00),  # 0  transparent — the RGB is never seen, tRNS zeroes it
    (0x12, 0x16, 0x1A),  # 1  shadow
    (0x23, 0x2A, 0x30),  # 2  wall
    (0x3A, 0x44, 0x4C),  # 3  deck
    (0x59, 0x66, 0x6E),  # 4  highlight
]

# 4x4 Bayer.  Sixteen thresholds, so a level carries four bits of sub-ink
# detail and four colours read as sixteen.
BAYER = (
    (0, 8, 2, 10),
    (12, 4, 14, 6),
    (3, 11, 1, 9),
    (15, 7, 13, 5),
)

# A level is in [0, 64]: 0 is fully transparent, 16/32/48/64 land exactly on
# inks 1/2/3/4, and everything between dithers the two it sits amongst.  The
# 0..16 band therefore dithers ink 1 against the hole, which is how the rim
# feathers out instead of ending on a hard rectangle.
LEVEL_MAX = 64
LEVEL_STEP = 16


def clamp(v, lo, hi):
    return lo if v < lo else (hi if v > hi else v)


def level(x, y):
    """The plate's value field at (x, y), in [0, LEVEL_MAX]. Integers only."""
    # ── the rear wall ───────────────────────────────────────────────────────
    # The darkest thing in the frame, and deliberately: everything else has to
    # separate from it by more than the dither's own noise, or the crates read
    # as wireframe instead of as mass.
    #
    # It stays at or above LEVEL_STEP, and that floor is not a style choice.
    # Below one full step the dither starts mixing ink 1 with the HOLE, and the
    # wall would come out half-transparent — the tint band showing through the
    # middle of solid steel.  Transparency in this plate is the rim's job and
    # only the rim's.
    v = LEVEL_STEP + 4 - (y * 4) // HEIGHT

    # Structural ribs every 48 px, four wide, with a lit left edge.  These are
    # the wall's own framing, not the compartment frame — that belongs to
    # Layer::hull and is drawn around this art, never baked into it.
    rib = x % 48
    if rib < 4:
        v = 34 if rib == 0 else 28

    # ── the deck ────────────────────────────────────────────────────────────
    # Plating, well clear of the wall, darkening toward the viewer.  Seams
    # every 40 px catch the light.
    if y >= 118:
        v = 36 - (y - 118) // 5
        if x % 40 == 0:
            v += 12
        if y == 118:
            v += 16  # the deck/wall joint, the brightest line in the frame

    # ── the cargo ───────────────────────────────────────────────────────────
    # Two crate stacks.  Each crate is a solid mid-tone block with a lit top
    # and left edge and a shadowed right and bottom — a flat fill plus two
    # one-pixel borders is the entire read at this size, and the fill has to
    # be the thing you see first.
    for ox, oy, cols, rows in ((28, 46, 3, 3), (150, 70, 2, 2)):
        cw, ch = 24, 24
        for cx in range(cols):
            for cy in range(rows):
                x0 = ox + cx * cw
                y0 = oy + cy * ch
                if x0 <= x < x0 + cw - 2 and y0 <= y < y0 + ch - 2:
                    v = 46
                    if y == y0:
                        v = 63  # lit top
                    elif x == x0:
                        v = 56  # lit left
                    elif y == y0 + ch - 3 or x == x0 + cw - 3:
                        v = 24  # shadowed bottom / right
                    elif 10 <= (y - y0) <= 13:
                        v = 33  # the strap, four pixels so it reads as a band

        # The stack's cast shadow on the deck.
        sx0 = ox + 6
        sx1 = ox + cols * cw + 6
        sy0 = oy + rows * ch - 2
        if sx0 <= x < sx1 and sy0 <= y < sy0 + 8:
            v = clamp(v - 20, 0, LEVEL_MAX)

    # ── the rim ─────────────────────────────────────────────────────────────
    # Feather the outer six pixels down to nothing.  The dither turns that
    # ramp into a scatter of holes rather than a fading edge, so the tint band
    # underneath shows through the seam where the hull frame meets the art
    # instead of the plate ending on a visible rectangle.
    edge = min(x, y, WIDTH - 1 - x, HEIGHT - 1 - y)
    if edge < 6:
        v = (v * (edge + 1)) // 7

    return clamp(v, 0, LEVEL_MAX)


def index_rows():
    """The plate as HEIGHT rows of WIDTH palette indices."""
    rows = []
    for y in range(HEIGHT):
        threshold_row = BAYER[y % 4]
        row = bytearray(WIDTH)
        for x in range(WIDTH):
            v = level(x, y)
            base, frac = divmod(v, LEVEL_STEP)
            # frac/16 of the pixels in this neighbourhood step up one ink.
            if frac > threshold_row[x % 4]:
                base += 1
            row[x] = clamp(base, 0, len(PALETTE) - 1)
        rows.append(bytes(row))
    return rows


# ── PNG ─────────────────────────────────────────────────────────────────────
#
# Colour type 3 at bit depth 8.  Depth 4 would halve the raw scanlines and is
# the obvious saving, but it measured WORSE here: packing two indices per byte
# destroys the byte-aligned runs zlib was collapsing, and the deflate stream
# grows by more than the packing saves.  Measure before changing it back.

SIGNATURE = b"\x89PNG\r\n\x1a\x0a"
BIT_DEPTH = 8
COLOUR_TYPE = 3  # indexed


def chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def raw_scanlines(rows):
    # Filter type 0 on every scanline.  The art is flat fills and vertical
    # ribs, which deflate's own matcher handles; a per-row filter search would
    # trade determinism-by-inspection for bytes this plate does not need.
    return b"".join(b"\x00" + row for row in rows)


def encode(rows):
    ihdr = struct.pack(">IIBBBBB", WIDTH, HEIGHT, BIT_DEPTH, COLOUR_TYPE, 0, 0, 0)
    plte = b"".join(struct.pack("BBB", *rgb) for rgb in PALETTE)
    # tRNS is one byte long on purpose: PNG defines every palette entry past
    # the end of the tRNS array as fully opaque, so four trailing 0xFF bytes
    # would say nothing and cost four bytes.
    trns = b"\x00"
    return (
        SIGNATURE
        + chunk(b"IHDR", ihdr)
        + chunk(b"PLTE", plte)
        + chunk(b"tRNS", trns)
        + chunk(b"IDAT", zlib.compress(raw_scanlines(rows), 9))
        + chunk(b"IEND", b"")
    )


def parse_chunks(blob):
    """Yield (kind, data) for a PNG. Raises ValueError on anything malformed."""
    if not blob.startswith(SIGNATURE):
        raise ValueError("not a PNG: signature mismatch")
    offset = len(SIGNATURE)
    while offset < len(blob):
        if offset + 8 > len(blob):
            raise ValueError("truncated chunk header")
        (length,) = struct.unpack(">I", blob[offset : offset + 4])
        kind = blob[offset + 4 : offset + 8]
        data = blob[offset + 8 : offset + 8 + length]
        if len(data) != length:
            raise ValueError(f"truncated {kind!r} chunk")
        crc_bytes = blob[offset + 8 + length : offset + 12 + length]
        if len(crc_bytes) != 4:
            raise ValueError(f"truncated CRC on {kind!r}")
        (stored_crc,) = struct.unpack(">I", crc_bytes)
        if stored_crc != zlib.crc32(kind + data) & 0xFFFFFFFF:
            raise ValueError(f"bad CRC on {kind!r}")
        yield kind, data
        offset += 12 + length


# ── modes ───────────────────────────────────────────────────────────────────


def check_name(path):
    """Refuse a path this recipe does not describe. Returns an error or None."""
    if not path:
        return "venice-bake: empty path. Pass the plate to bake or verify."
    stem = os.path.splitext(os.path.basename(path))[0]
    if stem != PLATE_NAME:
        return (
            f"venice-bake: this recipe bakes {PLATE_NAME!r}, not {stem!r}.\n"
            f"  {path} would have been written with the {PLATE_NAME} artwork under\n"
            f"  another plate's name. Add a recipe for {stem!r} rather than\n"
            f"  pointing this one at it."
        )
    return None


def bake(path):
    if (err := check_name(path)) is not None:
        print(err, file=sys.stderr)
        return 1
    blob = encode(index_rows())
    with open(path, "wb") as handle:
        handle.write(blob)
    print(f"venice-bake: wrote {path} — {len(blob)} bytes")
    return 0


def check(path):
    """Prove the committed PNG is still what this recipe produces.

    Compares decompressed content, not compressed bytes.  A mismatch means one
    of the two was edited without the other: re-run with --out, and look at the
    diff before committing it.
    """
    if (err := check_name(path)) is not None:
        print(err, file=sys.stderr)
        return 1

    try:
        with open(path, "rb") as handle:
            blob = handle.read()
    except OSError as err:
        print(f"venice-bake: cannot read {path}: {err}", file=sys.stderr)
        return 1

    try:
        chunks = list(parse_chunks(blob))
    except ValueError as err:
        print(f"venice-bake: {path} is malformed: {err}", file=sys.stderr)
        return 1

    found = {}
    idat = b""
    for kind, data in chunks:
        if kind == b"IDAT":
            idat += data  # a conforming PNG may split IDAT; ours does not, but read it either way
        else:
            found[kind] = data

    problems = []

    want_ihdr = struct.pack(">IIBBBBB", WIDTH, HEIGHT, BIT_DEPTH, COLOUR_TYPE, 0, 0, 0)
    if found.get(b"IHDR") != want_ihdr:
        problems.append(f"IHDR {found.get(b'IHDR')!r} != {want_ihdr!r}")

    want_plte = b"".join(struct.pack("BBB", *rgb) for rgb in PALETTE)
    got_plte = found.get(b"PLTE", b"")
    if got_plte != want_plte:
        if len(got_plte) != len(want_plte):
            problems.append(f"PLTE has {len(got_plte) // 3} entries, the recipe has {len(PALETTE)}")
        else:
            # Same length, so name the entries that moved — "PLTE differs" with
            # matching counts tells you nothing you can act on.
            for i in range(len(PALETTE)):
                got = tuple(got_plte[i * 3 : i * 3 + 3])
                if got != PALETTE[i]:
                    problems.append(
                        f"PLTE entry {i} is #{got[0]:02x}{got[1]:02x}{got[2]:02x}, "
                        f"the recipe says #{PALETTE[i][0]:02x}{PALETTE[i][1]:02x}{PALETTE[i][2]:02x}"
                    )

    if found.get(b"tRNS") != b"\x00":
        problems.append(f"tRNS {found.get(b'tRNS')!r} != b'\\x00' — index 0 must be the hole")

    committed = None
    if not idat:
        problems.append("no IDAT chunk — the file carries no pixels at all")
    else:
        try:
            committed = zlib.decompress(idat)
        except zlib.error as err:
            problems.append(f"IDAT will not decompress: {err}")

    if committed is not None:
        expected = raw_scanlines(index_rows())
        if committed != expected:
            differing = sum(1 for a, b in zip(committed, expected) if a != b)
            problems.append(
                f"pixels differ: {len(committed)} bytes committed vs {len(expected)} expected, "
                f"{differing} byte(s) unequal"
            )

    if problems:
        print(f"venice-bake: FAIL — {path} is not what this recipe bakes:", file=sys.stderr)
        for problem in problems:
            print(f"    {problem}", file=sys.stderr)
        print("  Re-bake it: tools/venice-bake/bake.py --out " + path, file=sys.stderr)
        return 1

    print(f"venice-bake: CLEAN — {path} matches the recipe ({len(blob)} bytes)")
    return 0


def main(argv):
    parser = argparse.ArgumentParser(description="Bake or verify an OBSCURA art plate.")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--out", metavar="PNG", help="bake the plate to this path")
    group.add_argument("--check", metavar="PNG", help="verify this plate still matches the recipe")
    args = parser.parse_args(argv)
    # `is not None`, not truthiness: --out "" is a bake request with a bad
    # path, and routing it into check(None) reports a verify failure for an
    # operation the caller never asked for.
    return bake(args.out) if args.out is not None else check(args.check)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
