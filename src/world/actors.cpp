// [SIM] Implementation of the crew roster. See include/obscura/world/actors.hpp
// for what belongs in this file; see tools/lint/sim_purity.sh for what may not.

#include <obscura/world/actors.hpp>

#include <cstddef>
#include <vector>

#include <obscura/world/hull.hpp>

namespace obscura::world {

auto Roster::size() const -> std::size_t {
  return m_actors.size();
}

auto Roster::add(RoomId start) -> ActorId {
  const auto id = static_cast<ActorId>(m_actors.size());
  m_actors.push_back(Actor{.id = id, .room = start});
  return id;
}

auto Roster::room_of(ActorId id) const -> RoomId {
  if (id >= m_actors.size()) {
    return kNoRoom;
  }
  return m_actors[id].room;
}

auto Roster::move_to(const Hull& hull, ActorId id, RoomId destination) -> bool {
  if (id >= m_actors.size()) {
    return false;
  }
  if (!hull.adjacent(m_actors[id].room, destination)) {
    return false;
  }
  m_actors[id].room = destination;
  return true;
}

auto Roster::all() const -> const std::vector<Actor>& {
  return m_actors;
}

} // namespace obscura::world
