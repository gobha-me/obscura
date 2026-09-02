#pragma once

// Glyph substrate — the unresolved interior of one compartment.
//
// The substrate is data, not a screen operation. SHIP mode owns placement and
// graph distance; this component turns that distance plus the run seed and the
// projected compartment state into terminal cells. Resolution remains the
// authority boundary: once a compartment is surveyed, there is no substrate to
// draw.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <obscura/world/model.hpp>

namespace obscura::render {

inline constexpr std::size_t kGlyphSubstrateWidth = 22;
inline constexpr std::size_t kGlyphSubstrateHeight = 9;

enum class GlyphClass : std::uint8_t {
  shade,
  structure,
  manifest,
};

struct GlyphCell {
  char32_t glyph{U' '};
  GlyphClass class_{GlyphClass::shade};

  friend constexpr auto operator==(const GlyphCell&, const GlyphCell&)
      -> bool = default;
};

using GlyphRow = std::array<GlyphCell, kGlyphSubstrateWidth>;

struct GlyphSubstrate {
  std::array<GlyphRow, kGlyphSubstrateHeight> rows{};

  friend constexpr auto operator==(const GlyphSubstrate&, const GlyphSubstrate&)
      -> bool = default;
};

// Composes the fixed-size unresolved interior for one compartment. Distance is
// supplied by the caller so render/ never needs to traverse world topology.
// Distances of two or more share the most-corrupt tier.
[[nodiscard]] auto compose_glyph_substrate(
    std::uint64_t seed, const world::Compartment& compartment,
    std::uint16_t cursor_distance) -> std::optional<GlyphSubstrate>;

} // namespace obscura::render
