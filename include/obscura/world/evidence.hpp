#pragma once

// [SIM] What the derelict remembers about the incident.
//
// What belongs here: the derivation of observable facts from ground truth — a
// scorch mark in a room, a door log, a body's last position — and the
// vocabulary the rest of the game reasons in. An Evidence item is a *claim
// about the world*, complete and true; it is not what the player has seen.
// Hiding is redaction.hpp's job, and keeping the two apart is what lets the
// solver check a case against the full set while the renderer draws only part
// of it.
//
// Everything here must be a pure function of (Hull, Roster, Incident). If a
// derivation needs a coin flip, it takes a Seed argument — it does not reach
// for one.

#include <cstdint>
#include <string>
#include <vector>

#include <obscura/world/actors.hpp>
#include <obscura/world/hull.hpp>
#include <obscura/world/incident.hpp>

namespace obscura::world {

// The kinds of claim the game knows how to make. Kept as a closed enum rather
// than a string tag so the solver can reason about them exhaustively and the
// compiler flags an unhandled case when a new kind is added.
enum class EvidenceKind : std::uint8_t {
  Presence,  // actor A was in room R at tick T
  Absence,   // actor A was NOT in room R at tick T
  Trace,     // something happened in room R at tick T, actor unknown
  Testimony, // actor A claims something — the only kind that may be false
};

struct Evidence {
  EvidenceKind kind{EvidenceKind::Trace};
  ActorId subject{kNoActor};
  RoomId where{kNoRoom};
  Tick when{0};
  // Human-facing label. Rendering may truncate or redact it; the solver never
  // reads it, so it can carry authored prose without becoming load-bearing.
  std::string label{};
};

using EvidenceSet = std::vector<Evidence>;

// Derives the complete, unredacted evidence set for a run. Deterministic and
// order-stable: the same inputs give the same items in the same sequence, which
// is what lets state hashing compare two runs item by item.
[[nodiscard]] auto derive(const Hull& hull, const Roster& roster,
                          const Incident& incident) -> EvidenceSet;

// True when an item is consistent with the given ground truth. Testimony is
// exempt — an actor is allowed to lie, and a rule that treated every claim as
// true would make the whole deduction trivial.
[[nodiscard]] auto is_consistent(const Evidence& item, const Roster& roster,
                                 const Incident& incident) -> bool;

} // namespace obscura::world
