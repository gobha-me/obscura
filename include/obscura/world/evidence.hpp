#pragma once

// [SIM] What the derelict remembers about the incident.
//
// What belongs here: the derivation of observable facts from ground truth — a
// scorch mark in a room, a door log, a body's last position — and the
// vocabulary the rest of the game reasons in. An Evidence item is a *claim
// about the world*, complete and truth-owned; it is not what the player has
// seen, and its internal veracity may say that the claim is stale or
// misleading. Hiding is redaction.hpp's job, and keeping the two apart is what
// lets the solver check a case against the full set while the renderer draws
// only part of it.
//
// Everything here must be a pure function of (Hull, Roster, Incident). If a
// derivation needs a coin flip, it takes a Seed argument — it does not reach
// for one.

#include <obscura/world/actors.hpp>
#include <obscura/world/hull.hpp>
#include <obscura/world/incident.hpp>
#include <obscura/world/truth.hpp>

namespace obscura::world {

// Derives the complete, unredacted evidence set for a run. Deterministic and
// order-stable: the same inputs give the same items in the same sequence, which
// is what lets state hashing compare two runs item by item.
[[nodiscard]] auto derive(const Hull& hull, const Roster& roster,
                          const Incident& incident) -> EvidenceSet;

// True when an item's reliable facts are consistent with the given ground
// truth. Stale and misleading items are player-facing complications, not facts
// the reference solver may use to reject an otherwise valid case.
[[nodiscard]] auto is_consistent(const Evidence& item, const Roster& roster,
                                 const Incident& incident) -> bool;

} // namespace obscura::world
