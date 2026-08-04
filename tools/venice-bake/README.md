# venice-bake

Bakes the art plates in `assets/plates/`.

```sh
python3 tools/venice-bake/bake.py --out   assets/plates/hold-d0.png   # author it
python3 tools/venice-bake/bake.py --check assets/plates/hold-d0.png   # verify it
```

## What runs when

`--out` runs **at author time, by a human or an agent changing the art**. The
build does not run it, CI does not run it, and the game does not run it —
OBSCURA does not generate assets at runtime, and adding a Python dependency to
the build would be a worse trade than committing 756 bytes of PNG.

`--check` runs **on every `ctest`**, as the `plate-bake-check` case. It is the
reason `--out` can stay a manual step: editing the recipe and forgetting to
re-bake fails the suite instead of sitting in the tree looking correct.

`--check` compares the *decompressed* scanlines, palette and `tRNS` chunk, never
the compressed bytes. zlib's level-9 output is not guaranteed identical across
zlib versions, so a byte-for-byte comparison would go red on a good asset the
first time a runner's zlib moved.

## The format, and why

240x160, PNG colour type 3 (indexed) at bit depth 8. Five palette entries: index
0 is a fully transparent hole via a one-byte `tRNS`, indices 1-4 are the inks,
darkest first. A 4x4 Bayer matrix dithers between adjacent inks, so four colours
read as sixteen levels.

Two things in `bake.py` are load-bearing and easy to undo by accident:

- **Bit depth 8, not 4.** Packing two indices per byte halves the raw scanlines
  and measured *worse*: it destroys the byte-aligned runs deflate was already
  collapsing. Measure before changing it back.
- **The wall's level never drops below one full ink step.** Below that the
  dither starts mixing ink 1 with the transparent index and solid steel comes
  out half-see-through. Transparency in a plate is the rim's job.

## The budget

A plate has a hard size cap, and it will stop your build rather than yours
truly: `obscura_embed_asset()` refuses to configure above it. The number and the
derivation both live at the call site in `src/lib/CMakeLists.txt` — an asset
budget is a property of how the asset gets transmitted, not of baking it.

The cost that budget approximates is measured, not estimated, by
`test/11art-plate`, which owns that figure. Neither number is repeated here on
purpose: a figure copied into a second document is a figure that will disagree
with itself.

## Adding a plate

`Layer::plate` is archetype x damage (`docs/10-tile-grammar.md`), so plates are
named `<archetype>-d<level>.png`. A plate carries **interior art only**: the
compartment frame belongs to `Layer::hull` and decals to `Layer::overlay`, so
neither is baked in, and a plate carries no room label — one plate serves every
hold on the ship.
