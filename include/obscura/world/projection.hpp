#pragma once

// [SIM] Evidence after the ground-truth-only fields have been removed.
// Renderers consume this value type; it cannot answer whether an item is true.

#include <cstdint>
#include <vector>

#include <obscura/world/model.hpp>

namespace obscura::world {

enum class Fidelity : std::uint8_t {
  Hidden = 0,
  Sensed = 1,
  Partial = 2,
  Full = 3,
};

struct EvidenceProjection {
  EvidenceId id{EVIDENCE_NONE};
  RoomId location{ROOM_ANY};
  EvidenceKind kind{EvidenceKind::physical_trace};
  std::vector<Fact> asserts{};
  InstrumentMask requires_{};
  StringId body{STRING_NONE};
};

using EvidenceProjectionSet = std::vector<EvidenceProjection>;

} // namespace obscura::world
