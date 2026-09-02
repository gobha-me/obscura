#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

#include <obscura/render/glyph_substrate.hpp>
#include <obscura/world/model.hpp>

namespace {

using obscura::render::GlyphClass;
using obscura::render::GlyphSubstrate;
using obscura::world::Archetype;
using obscura::world::Compartment;
using obscura::world::Resolution;

auto room(Archetype archetype = Archetype::hold) -> Compartment {
  return {
      .id = 7,
      .archetype = archetype,
      .state = Resolution::unknown,
  };
}

auto count_class(const GlyphSubstrate& substrate, GlyphClass wanted)
    -> std::size_t {
  std::size_t count = 0;
  for (const auto& row : substrate.rows) {
    count += static_cast<std::size_t>(std::ranges::count_if(
        row, [wanted](const auto& cell) { return cell.class_ == wanted; }));
  }
  return count;
}

auto count_legible_manifest(const GlyphSubstrate& substrate) -> std::size_t {
  std::size_t count = 0;
  for (const auto& row : substrate.rows) {
    count += static_cast<std::size_t>(
        std::ranges::count_if(row, [](const auto& cell) {
          return cell.class_ == GlyphClass::manifest && cell.glyph != U'?';
        }));
  }
  return count;
}

auto supported(char32_t glyph) -> bool {
  constexpr std::u32string_view glyphs = U"░▒▓╫╬┼╪?ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  return glyphs.find(glyph) != std::u32string_view::npos;
}

auto logical_hash(const GlyphSubstrate& substrate) -> std::uint64_t {
  constexpr std::uint64_t kOffset = 14'695'981'039'346'656'037ULL;
  constexpr std::uint64_t kPrime = 1'099'511'628'211ULL;
  std::uint64_t hash = kOffset;
  for (const auto& row : substrate.rows) {
    for (const auto& cell : row) {
      const auto codepoint = static_cast<std::uint32_t>(cell.glyph);
      for (std::uint32_t shift = 0; shift < 32; shift += 8) {
        hash ^= (codepoint >> shift) & 0xFFU;
        hash *= kPrime;
      }
      hash ^= static_cast<std::uint8_t>(cell.class_);
      hash *= kPrime;
    }
  }
  return hash;
}

TEST_CASE("glyph substrate rejects states it has no authority to render",
          "[render][glyph][failure]") {
  Compartment invalid = room();
  invalid.id = obscura::world::ROOM_ANY;
  CHECK_FALSE(obscura::render::compose_glyph_substrate(1, invalid, 0));

  invalid = room(static_cast<Archetype>(0xFFU));
  CHECK_FALSE(obscura::render::compose_glyph_substrate(1, invalid, 0));

  for (const Resolution state : {Resolution::surveyed, Resolution::resolved}) {
    invalid = room();
    invalid.state = state;
    CHECK_FALSE(obscura::render::compose_glyph_substrate(1, invalid, 0));
  }
}

TEST_CASE("glyph substrate has a fixed, terminal-safe semantic shape",
          "[render][glyph]") {
  const auto substrate =
      obscura::render::compose_glyph_substrate(0xC01D'1A47ULL, room(), 0);
  REQUIRE(substrate.has_value());
  STATIC_REQUIRE(std::tuple_size_v<decltype(substrate->rows)> == 9);
  STATIC_REQUIRE(std::tuple_size_v<decltype(substrate->rows)::value_type> ==
                 22);

  CHECK(count_class(*substrate, GlyphClass::shade) > 0);
  CHECK(count_class(*substrate, GlyphClass::structure) > 0);
  CHECK(count_class(*substrate, GlyphClass::manifest) > 0);
  for (const auto& row : substrate->rows) {
    CHECK(std::ranges::all_of(
        row, [](const auto& cell) { return supported(cell.glyph); }));
  }
}

TEST_CASE("manifest legibility falls as cursor distance grows",
          "[render][glyph][gradient]") {
  const Compartment hold = room();
  const auto near = obscura::render::compose_glyph_substrate(42, hold, 0);
  const auto middle = obscura::render::compose_glyph_substrate(42, hold, 1);
  const auto far = obscura::render::compose_glyph_substrate(42, hold, 2);
  const auto saturated = obscura::render::compose_glyph_substrate(
      42, hold, std::numeric_limits<std::uint16_t>::max());
  REQUIRE(near.has_value());
  REQUIRE(middle.has_value());
  REQUIRE(far.has_value());
  REQUIRE(saturated.has_value());

  CHECK(count_legible_manifest(*near) == 3);
  CHECK(count_legible_manifest(*middle) == 2);
  CHECK(count_legible_manifest(*far) == 1);
  CHECK(*saturated == *far);
}

TEST_CASE("manifest fragments can only reveal their compartment archetype",
          "[render][glyph][firewall]") {
  constexpr std::array<std::string_view, 8> tokens{
      "BRIDGE",      "GALLEY", "BERTH",   "HOLD",
      "ENGINEERING", "MEDBAY", "AIRLOCK", "COMMS",
  };

  for (std::size_t archetype = 0; archetype < tokens.size(); ++archetype) {
    const auto substrate = obscura::render::compose_glyph_substrate(
        77, room(static_cast<Archetype>(archetype)), 0);
    REQUIRE(substrate.has_value());

    const std::size_t span = tokens[archetype].size();
    const std::size_t first =
        (obscura::render::kGlyphSubstrateWidth - span) / 2U;
    for (std::size_t index = 0; index < span; ++index) {
      const auto& cell = substrate->rows[4][first + index];
      REQUIRE(cell.class_ == GlyphClass::manifest);
      CHECK((cell.glyph == U'?' ||
             cell.glyph == static_cast<char32_t>(tokens[archetype][index])));
    }
  }
}

TEST_CASE("glyph stream is stable and isolated by seed and room",
          "[render][glyph][determinism]") {
  const auto first =
      obscura::render::compose_glyph_substrate(0x5EEDULL, room(), 1);
  const auto repeated =
      obscura::render::compose_glyph_substrate(0x5EEDULL, room(), 1);
  const auto other_seed =
      obscura::render::compose_glyph_substrate(0x5EEEULL, room(), 1);
  Compartment other_room = room();
  other_room.id = 8;
  const auto other_id =
      obscura::render::compose_glyph_substrate(0x5EEDULL, other_room, 1);
  REQUIRE(first.has_value());
  REQUIRE(repeated.has_value());
  REQUIRE(other_seed.has_value());
  REQUIRE(other_id.has_value());

  CHECK(*first == *repeated);
  CHECK(*first != *other_seed);
  CHECK(*first != *other_id);

  CHECK(logical_hash(*first) == 0x152C'5BE6'57B9'D340ULL);

  constexpr std::array<char32_t, 8> expected{
      U'▒', U'╪', U'╬', U'╬', U'╫', U'╬', U'╬', U'▓',
  };
  for (std::size_t index = 0; index < expected.size(); ++index) {
    CHECK(first->rows[0][index].glyph == expected[index]);
  }
}

} // namespace
