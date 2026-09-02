#pragma once

// [SIM] Who is aboard, and where they are.
//
// What belongs here: actor identity, per-tick position on the hull, and the
// movement rules that advance a roster one tick. An actor's *history* is what
// the evidence system reads, so this is the module that has to be exactly
// reproducible: same seed, same schedule, same positions, tick for tick.
//
// What does not belong here: anything about what the player can see. Visibility
// is a redaction applied later (redaction.hpp), never a property an actor
// carries around.

#include <cstddef>
#include <vector>

#include <obscura/world/hull.hpp>
#include <obscura/world/model.hpp>

namespace obscura::world {

// The crew, indexed by ActorId. A vector for the same determinism reason the
// hull is one: position in the roster is the tie-break for every rule that has
// to pick between two equally valid actors.
class Roster {
 public:
  Roster() = default;

  [[nodiscard]] auto size() const -> std::size_t;

  auto add(RoomId start) -> ActorId;

  // kNoRoom for an unknown actor, so callers can compare rather than branch on
  // a separate "exists" query.
  [[nodiscard]] auto room_of(ActorId id) const -> RoomId;

  // Moves an actor to an adjacent room. Refuses (and returns false) when the
  // destination is not connected to where the actor stands — an illegal move is
  // a bug in the schedule, not something to clamp silently.
  auto move_to(const Hull& hull, ActorId id, RoomId destination) -> bool;

  [[nodiscard]] auto all() const -> const std::vector<Actor>&;

 private:
  std::vector<Actor> m_actors{};
};

} // namespace obscura::world
