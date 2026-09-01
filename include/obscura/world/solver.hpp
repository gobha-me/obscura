#pragma once

// [SIM] The check that a case is fair.
//
// What belongs here: the reference deduction. Given a hull, a roster and a set
// of evidence, enumerate the candidate culprits consistent with all of it. A
// case is playable only when exactly one candidate survives — a case with two
// is unwinnable by reasoning, and a case with none is broken.
//
// This is the module that turns "the authored case is correct" from a claim
// into a test. cases/ data is fed through it, and the assertion is uniqueness,
// not the identity of the answer.
//
// The solver reads the COMPLETE evidence set, never a projection. Running it
// against redacted evidence would answer a different question — "can the player
// solve it right now" — which is a fine thing to want and a different function.

#include <cstddef>
#include <vector>

#include <obscura/world/actors.hpp>
#include <obscura/world/evidence.hpp>
#include <obscura/world/hull.hpp>

namespace obscura::world {

struct Solution {
  // Every actor not excluded by the evidence, in ascending id order.
  std::vector<ActorId> candidates{};

  [[nodiscard]] auto is_unique() const -> bool {
    return candidates.size() == 1;
  }
  [[nodiscard]] auto is_broken() const -> bool { return candidates.empty(); }
};

// Eliminative, not constructive: start from every actor and strike out the ones
// some item rules out. That order matters — it makes "no candidate survived" a
// distinguishable outcome from "the search gave up", so a broken case reports
// as broken instead of as unsolved.
[[nodiscard]] auto solve(const Hull& hull, const Roster& roster,
                         const EvidenceSet& evidence) -> Solution;

} // namespace obscura::world
