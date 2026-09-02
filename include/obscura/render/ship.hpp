#pragma once

// SHIP mode's fixed reference grid.
//
// The authored room coordinates are game data, not hints for a responsive
// layout. This renderer therefore validates the complete 120x40 projection
// before painting anything and only translates it as a centred letterbox.

#include <cstddef>
#include <cstdint>

#include <obscura/world/hull.hpp>

#include <termforge/core/screen.hpp>
#include <termforge/core/types.hpp>

namespace obscura::render {

inline constexpr int kShipReferenceColumns = 120;
inline constexpr int kShipReferenceRows = 40;
inline constexpr std::size_t kShipMaximumCompartments = 15;

enum class ShipRenderStatus : std::uint8_t {
  drawn,
  terminal_too_small,
  too_many_compartments,
  invalid_cursor,
  invalid_layout,
  unsupported_resolution,
};

struct ShipRenderResult {
  ShipRenderStatus status{ShipRenderStatus::invalid_layout};
  termforge::Rect viewport{};

  constexpr auto operator==(const ShipRenderResult&) const -> bool = default;
};

// Draws only after every room, edge and substrate has been accepted. A
// refusal leaves `screen` byte-for-byte untouched and returns an empty
// viewport. Surveyed and resolved rooms deliberately remain unsupported until
// their projection contract lands.
[[nodiscard]] auto draw_ship(termforge::Screen& screen,
                             const world::Hull& hull, std::uint64_t seed,
                             world::RoomId cursor) -> ShipRenderResult;

} // namespace obscura::render
