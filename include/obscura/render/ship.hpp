#pragma once

// SHIP mode's fixed reference grid.
//
// The authored room coordinates are game data, not hints for a responsive
// layout. This renderer therefore validates the complete 120x40 projection
// before painting anything and only translates it as a centred letterbox.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

#include <obscura/world/hull.hpp>
#include <obscura/world/projection.hpp>

#include <termforge/core/screen.hpp>
#include <termforge/core/types.hpp>

namespace obscura::render {

inline constexpr int kShipReferenceColumns = 120;
inline constexpr int kShipReferenceRows = 40;
inline constexpr std::size_t kShipMaximumCompartments = 15;
inline constexpr std::size_t kShipMaximumEvidenceMarkers = 4;

enum class ShipRenderStatus : std::uint8_t {
  drawn,
  terminal_too_small,
  too_many_compartments,
  invalid_cursor,
  invalid_layout,
  invalid_projection,
  too_many_evidence_markers,
  unsupported_resolution,
};

struct ShipRoomLabel {
  world::RoomId id{world::ROOM_ANY};
  std::string_view text{};
};

// Everything SHIP mode may know about the current run. Labels and evidence are
// already player-facing projections: passing CaseData or the complete evidence
// set here would let render/ reach facts the run has not earned.
struct ShipRenderInput {
  std::reference_wrapper<const world::Hull> hull;
  std::span<const ShipRoomLabel> room_labels{};
  std::span<const world::EvidenceProjection> evidence{};
  world::InstrumentMask instruments{};
  std::uint64_t seed{};
  world::RoomId cursor{world::ROOM_ANY};
};

struct ShipRenderResult {
  ShipRenderStatus status{ShipRenderStatus::invalid_layout};
  termforge::Rect viewport{};

  constexpr auto operator==(const ShipRenderResult&) const -> bool = default;
};

// Draws only after every room, edge, label, evidence marker and substrate has
// been accepted. A refusal leaves `screen` byte-for-byte untouched and returns
// an empty viewport. Resolved rooms remain unsupported until the plate/dissolve
// contract lands.
[[nodiscard]] auto draw_ship(termforge::Screen& screen,
                             const ShipRenderInput& input) -> ShipRenderResult;

} // namespace obscura::render
