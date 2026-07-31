#pragma once

// [SIM] The thing that happened, and the seed it was generated from.
//
// What belongs here: the ground truth of a run — who did it, where, and on
// which tick — plus the seeded generation that produces it from a case
// definition. Exactly one Incident exists per run and it never changes after
// generation; everything downstream either observes it (evidence.hpp), hides
// part of it (redaction.hpp) or tries to recover it (solver.hpp).
//
// The seed is the whole contract with replay/: an Incident must be a pure
// function of (Seed, case definition). Reach for the wall clock or ambient
// entropy to break a tie here and a saved replay stops reproducing — which is
// precisely what the sim-purity lint exists to prevent.

#include <cstdint>

#include <obscura/world/actors.hpp>
#include <obscura/world/hull.hpp>

namespace obscura::world {

// 64 bits, and explicitly not derived from anything ambient. A run is named by
// its seed; the player is expected to be able to type one in.
using Seed = std::uint64_t;

struct Incident {
  ActorId culprit{kNoActor};
  RoomId  scene{kNoRoom};
  Tick    when{0};
};

// True when every field names something that exists in the given world. The
// generator below guarantees it; the solver and the authored cases both want to
// assert it rather than assume it.
[[nodiscard]] auto is_well_formed(const Incident& incident, const Hull& hull, const Roster& roster) -> bool;

// Deterministic mixing step. SplitMix64: one multiply-xor-shift round, no state
// beyond the value handed in, so a caller can derive an independent stream per
// decision by mixing a distinct salt instead of threading a generator object
// through the simulation.
[[nodiscard]] auto mix(Seed seed) -> Seed;

// The seeded generator. Returns a well-formed Incident for any non-empty world,
// and an all-sentinel one when the world has no actors or no rooms — the caller
// checks is_well_formed rather than being handed something that names a
// nonexistent culprit.
[[nodiscard]] auto generate(Seed seed, const Hull& hull, const Roster& roster, Tick horizon) -> Incident;

}  // namespace obscura::world
