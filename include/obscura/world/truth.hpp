#pragma once

// [SIM] Ground truth. This header is intentionally outside the dependency
// closure of render/ and input/; tools/lint/ui_firewall.sh enforces that rule.

#include <cstdint>
#include <vector>

#include <obscura/world/model.hpp>

namespace obscura::world {

enum class Veracity : std::uint8_t {
  true_,
  stale,
  misleading,
};

struct Evidence {
  EvidenceId id{EVIDENCE_NONE};
  RoomId location{ROOM_ANY};
  EvidenceKind kind{EvidenceKind::physical_trace};
  std::vector<Fact> asserts{};
  Veracity veracity{Veracity::true_};
  InstrumentMask requires_{};
  StringId body{STRING_NONE};
};

struct Truth {
  std::vector<Actor> crew{};
  IncidentArchetype cause{IncidentArchetype::fire};
  RoomId origin{ROOM_ANY};
  std::vector<Compartment> hull{};
  std::vector<Evidence> all{};
  std::vector<Evidence> served{};
};

using EvidenceSet = std::vector<Evidence>;

} // namespace obscura::world
