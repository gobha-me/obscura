// The baked art plate, and what it costs to put on the wire (issue #21).
//
// This file OWNS the plate's measured transmit cost, the way test/10frame-bytes
// owns the full-repaint measurement. The docs point here; they do not restate
// the number, because a figure copied into four documents is a figure that will
// disagree with itself.
//
// Two things are being tested and they are easy to conflate.
//
// The first is the ENVELOPE. termforge is a courier for a pre-encoded payload:
// it does not decode a PNG, does not validate one, and has no opinion about
// what is inside. That is the only reason a compressed wire format can exist in
// a stdlib-only library at all — and it means the extent the driver enforces
// PlacementFit::Exact against, and ships as s=/v=, is the extent it was TOLD.
// Nothing upstream checks that claim against the payload. So this file parses
// the PNG itself. It is the only place that ever will.
//
// The second is the COST, asserted against an oracle derived from kitty's wire
// format rather than read back off the meter. Checking the meter against the
// sink's own growth proves very little — emit_frame() hands the same length to
// both, two lines apart, so that equality is arithmetic and not behaviour.
//
// All offline. set_output redirects every driver away from stdout, so nothing
// here needs a tty — with one documented exception, which is not ours to fix:
// ~KittyDriver calls delete_all(), and that writes its "delete every image"
// escape STRAIGHT TO STDOUT, deliberately bypassing the sink, because a
// destructor cannot assume a borrowed sink is still alive (termforge#144,
// #148). So a driver that uploaded anything leaves a few escape bytes in the
// ctest log, and running this binary by hand in a real terminal clears that
// terminal's images. Declaring the sink before the driver, as every case below
// does, is still required — it is what keeps the sink alive for the driver's
// lifetime — but it does not and cannot redirect those bytes.
//
// The other thing this suite cannot answer is whether a real terminal ACCEPTS
// a colour-type-3 payload. Current TermForge correlates image replies and can
// roll refused work back, but only a real emulator can prove that this exact
// indexed payload is accepted. If a plate renders blank, suspect the payload
// before the placement.

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <obscura/render/art_plate.hpp>
#include <termforge/core/types.hpp>
#include <termforge/drivers/fallback_driver.hpp>
#include <termforge/drivers/kitty_driver.hpp>

namespace {

using termforge::EncodedImage;
using termforge::Extent;
using termforge::FallbackDriver;
using termforge::FrameBytes;
using termforge::ImageFormat;
using termforge::KittyDriver;
using termforge::PlacementFit;
using termforge::Rect;

// ── the oracle ──────────────────────────────────────────────────────────────
//
// What kitty's transmit path costs for a payload of P bytes, derived from the
// protocol rather than measured. Deliberately coupled to the wire format,
// exactly as test/10frame-bytes' full_repaint_bytes() is coupled to the
// fallback tier's: a change to it is not a cosmetic upstream detail, it moves
// the cost of the asset M0 gates on.
//
//   payload -> base64            4/3, rounded up to a whole quantum
//   first chunk   "\033_Ga=t,t=d,f=100,i=1,s=240,v=160,m=0,q=2;" + "\033\\"
//   continuation  "\033_Gm=0,q=0;"                               + "\033\\"
//
// The framing is ADDITIVE and sits outside the 4/3 factor — a distinction worth
// keeping, because conflating the two is how the plate's cost has been
// misquoted in both directions. `image_transmit` reports base64 PLUS framing:
// the driver tallies the whole byte range it appends around transmit(), not
// just the payload it encoded.
constexpr std::size_t kChunkSize = 4096;  // KittyDriver::transmit
constexpr std::uint64_t kContinuationFraming = 13;

// The first chunk's header is digit-dependent — image id, s= and v= all grow
// with their values — so it is counted from the literal rather than pinned to a
// bare number that silently stops being true for the second plate.
constexpr std::uint64_t first_chunk_framing(int w, int h, unsigned id,
                                            unsigned format_code) {
  auto digits = [](std::uint64_t n) -> std::uint64_t {
    std::uint64_t d = 1;
    while (n >= 10) {
      n /= 10;
      ++d;
    }
    return d;
  };
  // ESC _ G a=t, t=d, f=<fmt>, i=<id>, s=<w>, v=<h>, m=0, q=2 ;  ESC backslash
  //
  // f= is derived rather than fixed at the six characters "f=100," that Png
  // happens to cost. Rgba32 ships as f=32 and is one shorter, and a hardcode
  // would surface that as a one-byte discrepancy in a thousand-byte number —
  // the same trap the image id above is spelled out to avoid.
  return 1 + 2 + 4 + 4                              //  \033 _G a=t, t=d,
         + 2 + digits(format_code) + 1              //  f=<fmt>,
         + 2 + digits(id) + 1                       //  i=<id>,
         + 2 + digits(static_cast<std::uint64_t>(w)) + 1   //  s=<w>,
         + 2 + digits(static_cast<std::uint64_t>(h)) + 1   //  v=<h>,
         + 4 + 4                                    //  m=0, q=2;
         + 2;                                       //  \033 backslash
}

// termforge's wire code for ImageFormat::Png.
constexpr unsigned kFormatPng = 100;

constexpr auto expected_transmit(std::size_t payload, int w, int h, unsigned id,
                                 unsigned format_code = kFormatPng) -> std::uint64_t {
  const std::uint64_t b64 = (static_cast<std::uint64_t>(payload) + 2) / 3 * 4;
  const std::uint64_t chunks = (b64 + kChunkSize - 1) / kChunkSize;
  return b64 + first_chunk_framing(w, h, id, format_code) +
         (chunks > 1 ? chunks - 1 : 0) * kContinuationFraming;
}

// Issue #21's acceptance, and it is an ON-WIRE quantity: everything the frame
// emits to get the plate up, not just the payload bucket.
constexpr std::uint64_t kWireBudget = 8192;

// ── the measured cost, pinned ───────────────────────────────────────────────
//
// The oracle takes the payload's own length as an input, so on its own it
// checks the framing arithmetic and says nothing whatever about how big the
// plate is. These do. Without them a re-bake could quintuple the asset and stay
// green all the way to the far-off configure gate, while this file went on
// claiming to own "the plate's cost".
//
// They WILL need editing when the art changes, and that is the point: the wire
// cost of the signature moment is not something that should be able to move
// without a reviewer seeing the number move. Re-bake, run the test, paste in
// what it reports, and look at the delta before committing it.
constexpr std::size_t kPlateBytes = 756;         // assets/plates/hold-d0.png
constexpr std::uint64_t kMeasuredTransmit = 1051;  // base64 + APC framing
constexpr std::uint64_t kMeasuredEdit = 30;        // cursor address + a=p at 0,0
constexpr std::uint64_t kMeasuredTotal = kMeasuredTransmit + kMeasuredEdit;
static_assert(kMeasuredTotal <= kWireBudget,
              "the plate is over issue #21's on-wire budget");

// ── a driver and somewhere for its bytes to go ──────────────────────────────
//
// The member order is the point. TerminalDriver borrows its sink and does not
// own it (byte_sink.hpp), so the sink has to outlive the driver — which means
// being declared FIRST and therefore destroyed last. Written out at each of
// four call sites that invariant is a convention someone copies without the
// comment; here it is structural, and getting it wrong is not expressible.
//
// It is harmless against today's driver, which never writes through the sink
// during destruction. It stops being harmless the day termforge#144 gives the
// driver an owned sink and ~KittyDriver starts routing through it.
struct Probe {
  std::string sink;
  KittyDriver drv;

  Probe() { drv.set_output(&sink); }
};

// ── a PNG reader, for the one claim nothing else checks ─────────────────────
//
// plate-bake-check (tools/venice-bake/bake.py --check) also validates the
// plate's IHDR, palette and tRNS, and does it more strictly — exact bytes, and
// a better message when a palette entry moves. This is not that check repeated.
// The two look at different artefacts: the Python one reads the committed FILE,
// this one reads the bytes that were actually COMPILED IN, and only this one
// can compare them against the extent art_plate.cpp declares to the driver.
// A build embedding a stale or wrong asset is exactly the gap between them.

struct PngHeader {
  bool signature_ok{false};
  int width{0};
  int height{0};
  int bit_depth{0};
  int colour_type{-1};
  int palette_entries{0};
  bool has_trns{false};
  bool has_idat{false};
  // The chunk walk reached IEND and consumed the payload exactly — no trailing
  // slack, no truncation. This is the field that catches a bad EMBED rather
  // than a bad bake: `std::array<unsigned char, N>{...}` with fewer than N
  // initialisers is well-formed C++ and zero-fills the tail, so a generator
  // that under-produced would hand us a header-shaped prefix followed by
  // silence. Every field above would still be correct.
  bool well_formed{false};
};

auto read_u32(std::span<const std::byte> b, std::size_t at) -> std::uint32_t {
  return (static_cast<std::uint32_t>(b[at]) << 24) |
         (static_cast<std::uint32_t>(b[at + 1]) << 16) |
         (static_cast<std::uint32_t>(b[at + 2]) << 8) |
         static_cast<std::uint32_t>(b[at + 3]);
}

auto parse_png(std::span<const std::byte> blob) -> PngHeader {
  constexpr unsigned char kSig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  PngHeader out;
  if (blob.size() < sizeof(kSig)) return out;
  out.signature_ok = true;
  for (std::size_t i = 0; i < sizeof(kSig); ++i) {
    if (static_cast<unsigned char>(blob[i]) != kSig[i]) out.signature_ok = false;
  }
  if (!out.signature_ok) return out;

  std::size_t at = sizeof(kSig);
  while (at + 12 <= blob.size()) {
    const std::uint32_t length = read_u32(blob, at);
    // The PNG spec caps a chunk length at 2^31-1. Enforcing it keeps
    // `data + length` and the advance below from wrapping on a 32-bit size_t,
    // where a hostile 0xFFFFFFFF would otherwise pass the bounds check by
    // overflowing it and then walk backwards forever.
    if (length > 0x7FFFFFFFU) break;
    const auto kind = blob.subspan(at + 4, 4);
    const auto tag = [&](const char (&name)[5]) {
      for (int i = 0; i < 4; ++i) {
        if (static_cast<char>(kind[static_cast<std::size_t>(i)]) != name[i]) return false;
      }
      return true;
    };
    const std::size_t data = at + 8;
    if (data + length > blob.size()) break;

    if (tag("IHDR") && length >= 13) {
      out.width = static_cast<int>(read_u32(blob, data));
      out.height = static_cast<int>(read_u32(blob, data + 4));
      out.bit_depth = static_cast<int>(blob[data + 8]);
      out.colour_type = static_cast<int>(blob[data + 9]);
    } else if (tag("PLTE")) {
      out.palette_entries = static_cast<int>(length / 3);
    } else if (tag("tRNS")) {
      out.has_trns = true;
    } else if (tag("IDAT")) {
      out.has_idat = true;
    } else if (tag("IEND")) {
      // IEND is the last chunk by definition, so a walk that lands here having
      // consumed everything handed to it saw a whole file.
      out.well_formed = (data + length + 4 == blob.size());
      return out;
    }
    at = data + length + 4;  // + CRC
  }
  return out;
}

}  // namespace

// ── failures first ──────────────────────────────────────────────────────────

TEST_CASE("plate: the payload's own header agrees with what we declare",
          "[plate][failure]") {
  // The extent in art_plate.cpp is an unchecked assertion everywhere else in
  // the system. termforge ships it as s=/v= without looking, and enforces
  // PlacementFit::Exact against it rather than against the image — so a plate
  // re-baked at a different size, with the declaration left alone, passes the
  // fit guard and then paints outside the rect that was approved. This section
  // is the only thing standing between that and a corrupted screen.
  const EncodedImage plate = obscura::render::hold_d0();
  const PngHeader png = parse_png(plate.bytes);

  SECTION("it is a PNG at all") {
    CHECK(png.signature_ok);
    REQUIRE_FALSE(plate.bytes.empty());
    CHECK_FALSE(plate.empty());
  }

  SECTION("the whole payload arrived, not just a header-shaped prefix") {
    // The one check aimed at the EMBED rather than the bake. cmake generates
    // the byte array from file(READ ... HEX) through two regex passes, and
    // std::array zero-fills a short initialiser without complaint — so an
    // under-producing generator yields a valid-looking IHDR followed by
    // nothing. The chunk walk reaching IEND at exactly the array's end is what
    // rules that out.
    CHECK(png.has_idat);
    CHECK(png.well_formed);
  }

  SECTION("the declared extent is the encoded extent") {
    CHECK(png.width == plate.pixels.w);
    CHECK(png.height == plate.pixels.h);
    // And that extent is the spec's, not merely self-consistent: two wrong
    // numbers agreeing with each other is exactly the failure above.
    CHECK(plate.pixels.w == 240);
    CHECK(plate.pixels.h == 160);
  }

  SECTION("it is the four-colour indexed format the spec calls for") {
    CHECK(png.colour_type == 3);  // indexed
    CHECK(png.bit_depth == 8);
    // Four inks plus the transparent index. The count is asserted rather than
    // bounded: a fifth ink is an art-direction change, and it should have to be
    // made here as well as in the baker.
    CHECK(png.palette_entries == 5);
  }

  SECTION("the transparent index is actually declared") {
    // Without tRNS the palette's index 0 is opaque black, and the plate ends on
    // a hard rectangle over the tint band instead of feathering into it. The
    // baker writes a one-byte tRNS; nothing downstream would notice its loss.
    CHECK(png.has_trns);
  }

  SECTION("the format the payload claims is the format the courier is told") {
    CHECK(plate.format == ImageFormat::Png);
  }
}

TEST_CASE("plate: a tier that cannot carry the format refuses, and emits nothing",
          "[plate][failure]") {
  // "No degraded mode." A driver with no out-of-band channel must say so, not
  // approximate the plate with whatever it can manage.
  const EncodedImage plate = obscura::render::hold_d0();

  // Not Probe: this case needs the floor tier, and one other driver type does
  // not earn a template. Same ordering rule and same reason — sink first, so it
  // outlives the driver that borrows it.
  std::string sink;
  FallbackDriver drv;
  drv.set_output(&sink);

  REQUIRE_FALSE(drv.supports_image_format(ImageFormat::Png));

  const auto result = drv.draw_image(Rect{0, 0, 30, 10}, plate);
  REQUIRE_FALSE(result.has_value());

  // Assert WHICH refusal. The fallback tier is 1 px per cell, so a 240x160
  // image into any sane rect would also fail the fit check — validate_encoded
  // simply happens to run first. Without naming the format, this section would
  // pass for the wrong reason on a driver whose format support had been fixed.
  CHECK(result.error().message.find("Png") != std::string::npos);

  drv.flush();
  CHECK(sink.empty());
  CHECK(drv.last_frame_bytes().total() == 0);
}

TEST_CASE("plate: a refused placement costs nothing", "[plate][failure]") {
  // Validation runs before the upload, and this is what says so. If that order
  // ever inverted, a rejected draw would still have paid for the payload — the
  // budget would be spent on pixels the terminal never sees, and the meter
  // would be the only witness.
  const EncodedImage plate = obscura::render::hold_d0();

  Probe p;

  SECTION("a rect one cell short of the image is not an Exact fit") {
    const Extent cells = p.drv.image_cell_extent(plate.pixels);
    REQUIRE(cells.w > 1);
    const auto result =
        p.drv.draw_image(Rect{0, 0, cells.w - 1, cells.h}, plate, PlacementFit::Exact);
    REQUIRE_FALSE(result.has_value());
    p.drv.flush();
    CHECK(p.sink.empty());
    CHECK(p.drv.last_frame_bytes().total() == 0);
  }

  SECTION("the fit answer moves when the placement mode does") {
    // CLAUDE.md: ask supports_image_format() and supports_placement_fit()
    // before committing to an art set, but only the format answer is stable.
    // This pins the instability rather than trusting the note.
    REQUIRE(p.drv.supports_placement_fit(PlacementFit::Exact));
    p.drv.set_placement_mode(KittyDriver::PlacementMode::UnicodePlaceholders);
    CHECK_FALSE(p.drv.supports_placement_fit(PlacementFit::Exact));
    // ...and the format answer does not.
    CHECK(p.drv.supports_image_format(ImageFormat::Png));
  }
}

// ── the measurement ─────────────────────────────────────────────────────────

TEST_CASE("plate: the wire cost is what base64 and the APC framing predict",
          "[plate][bytes]") {
  const EncodedImage plate = obscura::render::hold_d0();

  Probe p;
  auto& drv = p.drv;
  auto& sink = p.sink;

  REQUIRE(drv.supports_image_format(ImageFormat::Png));
  REQUIRE(drv.supports_placement_fit(PlacementFit::Exact));

  // No magic constants: the destination rect is derived from the image's own
  // extent and the driver's cell geometry. 240x160 does not divide evenly into
  // the 22x9 compartment box docs/10-tile-grammar.md specifies, and nothing in
  // docs/ reconciles those two numbers — so this test declines to bake either
  // of them in. (T-H5, termforge#143, is the ticket that would let the real
  // geometry be queried instead of assumed.)
  const Extent cells = drv.image_cell_extent(plate.pixels);
  REQUIRE(drv.draw_image(Rect{0, 0, cells.w, cells.h}, plate, PlacementFit::Exact));

  drv.flush();
  const FrameBytes f = drv.last_frame_bytes();

  // The oracle's framing width depends on the image id's digit count, so the id
  // is read off the wire rather than assumed. It is a termforge implementation
  // detail — nothing in its public API promises the first image is id 1 — and a
  // change to it would otherwise surface as a one-byte discrepancy in a
  // thousand-byte number, sending the next reader to the base64 arithmetic
  // before the id.
  constexpr unsigned kFirstImageId = 1;
  CHECK(sink.find("i=" + std::to_string(kFirstImageId) + ",") != std::string::npos);

  const std::uint64_t predicted = expected_transmit(plate.bytes.size(), plate.pixels.w,
                                                    plate.pixels.h, kFirstImageId);

  CHECK(f.image_transmit == predicted);
  CHECK(f.cells == 0);       // nothing here drew text

  // The asset, and what it actually cost. See kPlateBytes above for why these
  // are literals rather than derived.
  CHECK(plate.bytes.size() == kPlateBytes);
  CHECK(f.image_transmit == kMeasuredTransmit);
  CHECK(f.image_edit == kMeasuredEdit);
  CHECK(f.total() == kMeasuredTotal);

  // Issue #21's acceptance box. Against total(), not against image_transmit:
  // the issue says on-wire, and the placement escape is on the wire too.
  CHECK(f.total() <= kWireBudget);
}

TEST_CASE("plate: the oracle holds across the chunk boundary", "[plate][bytes]") {
  // The committed plate is one chunk, so the continuation term in
  // expected_transmit() — and the 9 bytes it multiplies — is dead code as far
  // as the plate itself is concerned. That term is not idle: the configure-time
  // budget in src/lib/CMakeLists.txt derives its byte cap from the same
  // arithmetic. An unexercised constant underneath a gate that rejects real
  // work is worth a synthetic payload.
  //
  // Deliberately NOT a real PNG. termforge does not parse the payload, so what
  // is under test here is the courier's overhead over an arbitrary N bytes, and
  // a real asset would pin this to one plate's compression ratio for no gain.
  Probe p;
  auto& drv = p.drv;

  // Over one chunk's worth of base64 (4096 chars carries 3072 payload bytes)
  // and under two, so exactly one continuation is emitted.
  constexpr std::size_t kPayload = 5000;
  std::vector<std::byte> filler(kPayload);
  for (std::size_t i = 0; i < filler.size(); ++i) {
    filler[i] = static_cast<std::byte>((i * 31U + 13U) & 0xFFU);
  }

  constexpr int kW = 240;
  constexpr int kH = 160;
  const EncodedImage synthetic{ImageFormat::Png, std::span<const std::byte>{filler},
                               Extent{kW, kH}};

  const Extent cells = drv.image_cell_extent(synthetic.pixels);
  REQUIRE(drv.draw_image(Rect{0, 0, cells.w, cells.h}, synthetic, PlacementFit::Exact));
  drv.flush();

  // Two chunks: the sanity check that this payload actually crosses the
  // boundary, so the section cannot quietly stop testing what it names. The
  // final continuation requests the one correlated reply for the whole image;
  // dropping q=0 here makes the oracle four bytes short and loses the refusal
  // signal the driver now relies on.
  const std::uint64_t b64 = (kPayload + 2) / 3 * 4;
  REQUIRE(b64 > kChunkSize);
  REQUIRE(b64 < 2 * kChunkSize);

  CHECK(drv.last_frame_bytes().image_transmit == expected_transmit(kPayload, kW, kH, 1));
}

TEST_CASE("plate: transmitting it twice costs once", "[plate][bytes]") {
  // Identity is keyed on the payload bytes, so a plate already resident is not
  // re-uploaded. This is what makes a 15-compartment screen affordable, and it
  // is the property the dissolve's 40 KB budget (#23) is written against.
  const EncodedImage plate = obscura::render::hold_d0();

  Probe p;
  auto& drv = p.drv;

  const Extent cells = drv.image_cell_extent(plate.pixels);
  const Rect dest{0, 0, cells.w, cells.h};

  REQUIRE(drv.draw_image(dest, plate, PlacementFit::Exact));
  drv.flush();
  const std::uint64_t first = drv.last_frame_bytes().image_transmit;
  REQUIRE(first > 0);

  // Redraw before flushing. flush() garbage-collects any region NOT drawn in
  // the frame it is ending, so flushing twice without redrawing would emit a
  // delete and read as a regression in a driver behaving perfectly.
  REQUIRE(drv.draw_image(dest, plate, PlacementFit::Exact));
  drv.flush();
  CHECK(drv.last_frame_bytes().image_transmit == 0);
}
