#pragma once

// [SIM] The derelict itself: rooms, and which rooms connect to which.
//
// What belongs here: the static topology a case is played on — room identity,
// adjacency, and the queries that walk it (reachability, distance,
// chokepoints). The hull does not change during a run; anything that moves
// belongs in actors.hpp, anything that happened belongs in incident.hpp.
//
// Sim-purity applies to this whole directory and is enforced by
// tools/lint/sim_purity.sh (ctest: sim-purity). In short: integers only, no
// wall clock, no ambient entropy, no iteration order that varies between
// standard library implementations. The simulation must be reproducible from a
// seed alone, or replay/ and the case solver are both lying.

#include <cstddef>
#include <vector>

#include <obscura/world/model.hpp>

namespace obscura::world {

// The hull is a plain adjacency list indexed by RoomId. Deliberately a vector
// rather than a hashed container: a hash map's iteration order is an
// implementation detail, and this is exactly the kind of place where letting
// one leak into the simulation produces a run that replays differently on
// another standard library.
class Hull {
 public:
  Hull() = default;

  [[nodiscard]] auto room_count() const -> std::size_t;

  // Adds a room with no connections and returns its id. Ids are handed out in
  // sequence from 0.
  auto add_room() -> RoomId;

  // Adds authored metadata for the next dense id. Adjacency is deliberately
  // ignored here and remains owned by connect(), which keeps it sorted and
  // duplicate-free. A non-dense id is refused.
  auto add_room(Compartment room) -> RoomId;

  // Records a two-way connection. Ignores an edge to a room that does not
  // exist, and ignores a duplicate.
  auto connect(RoomId lhs, RoomId rhs) -> void;

  [[nodiscard]] auto adjacent(RoomId lhs, RoomId rhs) const -> bool;

  // Empty when `id` is out of range, so a caller can iterate without a bounds
  // check of its own.
  [[nodiscard]] auto neighbors(RoomId id) const -> const std::vector<RoomId>&;

  [[nodiscard]] auto all() const -> const std::vector<Compartment>&;

 private:
  std::vector<Compartment> m_rooms{};
};

} // namespace obscura::world
