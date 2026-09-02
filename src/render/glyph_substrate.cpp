// Seeded glyph substrate. See include/obscura/render/glyph_substrate.hpp for
// the authority boundary and public shape.

#include <obscura/render/glyph_substrate.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include <obscura/world/model.hpp>

namespace obscura::render {

namespace {

inline constexpr std::uint64_t kGlyphStreamId = 7;
inline constexpr std::uint64_t kSplitMixIncrement = 0x9E37'79B9'7F4A'7C15ULL;
inline constexpr std::uint64_t kPcgMultiplier = 6'364'136'223'846'793'005ULL;
inline constexpr std::size_t kManifestRow = kGlyphSubstrateHeight / 2;

constexpr std::array<char32_t, 3> kShadeGlyphs{U'░', U'▒', U'▓'};
constexpr std::array<char32_t, 4> kStructureGlyphs{U'╫', U'╬', U'┼', U'╪'};

auto splitmix64(std::uint64_t& state) -> std::uint64_t {
  state += kSplitMixIncrement;
  std::uint64_t value = state;
  value = (value ^ (value >> 30U)) * 0xBF58'476D'1CE4'E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D0'49BB'1331'11EBULL;
  return value ^ (value >> 31U);
}

class Pcg32 {
 public:
  Pcg32(std::uint64_t initial_state, std::uint64_t sequence)
      : increment_{(sequence << 1U) | 1U} {
    static_cast<void>(next());
    state_ += initial_state;
    static_cast<void>(next());
  }

  auto next() -> std::uint32_t {
    const std::uint64_t previous = state_;
    state_ = previous * kPcgMultiplier + increment_;
    const auto shifted =
        static_cast<std::uint32_t>(((previous >> 18U) ^ previous) >> 27U);
    const auto rotation = static_cast<std::uint32_t>(previous >> 59U);
    return (shifted >> rotation) | (shifted << ((0U - rotation) & 31U));
  }

  auto bounded(std::uint32_t bound) -> std::uint32_t {
    if (bound == 0) {
      return 0;
    }

    const std::uint32_t threshold = (0U - bound) % bound;
    for (;;) {
      const std::uint32_t value = next();
      if (value >= threshold) {
        return value % bound;
      }
    }
  }

 private:
  std::uint64_t state_{};
  std::uint64_t increment_{};
};

auto glyph_rng(std::uint64_t seed, world::RoomId room) -> Pcg32 {
  std::uint64_t material = seed ^ (kGlyphStreamId * kSplitMixIncrement) ^
                           (static_cast<std::uint64_t>(room) << 32U) ^
                           static_cast<std::uint64_t>(room);
  const std::uint64_t initial_state = splitmix64(material);
  const std::uint64_t sequence = splitmix64(material);
  return Pcg32{initial_state, sequence};
}

auto archetype_token(world::Archetype archetype)
    -> std::optional<std::string_view> {
  switch (archetype) {
    case world::Archetype::bridge: return "BRIDGE";
    case world::Archetype::galley: return "GALLEY";
    case world::Archetype::berth: return "BERTH";
    case world::Archetype::hold: return "HOLD";
    case world::Archetype::engineering: return "ENGINEERING";
    case world::Archetype::medbay: return "MEDBAY";
    case world::Archetype::airlock: return "AIRLOCK";
    case world::Archetype::comms: return "COMMS";
  }
  return std::nullopt;
}

struct CorruptionTier {
  std::size_t span_numerator;
  std::size_t visible_numerator;
};

constexpr auto corruption_tier(std::uint16_t distance) -> CorruptionTier {
  if (distance == 0) {
    return {.span_numerator = 4, .visible_numerator = 3};
  }
  if (distance == 1) {
    return {.span_numerator = 3, .visible_numerator = 2};
  }
  return {.span_numerator = 2, .visible_numerator = 1};
}

constexpr auto quarter_ceil(std::size_t value, std::size_t numerator)
    -> std::size_t {
  return ((value * numerator) + 3U) / 4U;
}

auto background_cell(Pcg32& rng) -> GlyphCell {
  if (rng.bounded(2) == 0) {
    return {
        .glyph = kShadeGlyphs.at(rng.bounded(kShadeGlyphs.size())),
        .class_ = GlyphClass::shade,
    };
  }
  return {
      .glyph = kStructureGlyphs.at(rng.bounded(kStructureGlyphs.size())),
      .class_ = GlyphClass::structure,
  };
}

} // namespace

auto compose_glyph_substrate(std::uint64_t seed,
                             const world::Compartment& compartment,
                             std::uint16_t cursor_distance)
    -> std::optional<GlyphSubstrate> {
  if (compartment.state != world::Resolution::unknown ||
      compartment.id == world::ROOM_ANY) {
    return std::nullopt;
  }

  const std::optional<std::string_view> token =
      archetype_token(compartment.archetype);
  if (!token.has_value()) {
    return std::nullopt;
  }

  Pcg32 rng = glyph_rng(seed, compartment.id);
  GlyphSubstrate substrate{};
  for (GlyphRow& row : substrate.rows) {
    for (GlyphCell& cell : row) {
      cell = background_cell(rng);
    }
  }

  // Guarantee both background classes even for adversarial seeds. These cells
  // are outside the manifest row, so the guarantee cannot be overwritten.
  substrate.rows.front().at(0) = {.glyph = kShadeGlyphs.at(rng.bounded(3)),
                                  .class_ = GlyphClass::shade};
  substrate.rows.front().at(1) = {.glyph = kStructureGlyphs.at(rng.bounded(4)),
                                  .class_ = GlyphClass::structure};

  const CorruptionTier tier = corruption_tier(cursor_distance);
  const std::size_t span = quarter_ceil(token->size(), tier.span_numerator);
  const std::size_t visible =
      std::max<std::size_t>(1, quarter_ceil(span, tier.visible_numerator));
  const std::size_t first_column = (kGlyphSubstrateWidth - span) / 2U;

  std::array<std::size_t, kGlyphSubstrateWidth> reveal_order{};
  for (std::size_t index = 0; index < span; ++index) {
    reveal_order.at(index) = index;
  }
  for (std::size_t remaining = span; remaining > 1; --remaining) {
    const std::size_t selected =
        rng.bounded(static_cast<std::uint32_t>(remaining));
    std::swap(reveal_order.at(remaining - 1), reveal_order.at(selected));
  }

  std::array<bool, kGlyphSubstrateWidth> revealed{};
  for (std::size_t index = 0; index < visible; ++index) {
    revealed.at(reveal_order.at(index)) = true;
  }

  for (std::size_t index = 0; index < span; ++index) {
    const auto source = static_cast<unsigned char>(token->at(index));
    substrate.rows.at(kManifestRow).at(first_column + index) = {
        .glyph = revealed.at(index) ? static_cast<char32_t>(source) : U'?',
        .class_ = GlyphClass::manifest,
    };
  }

  return substrate;
}

} // namespace obscura::render
