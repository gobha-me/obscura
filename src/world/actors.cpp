// [SIM] Implementation of the crew roster. See include/obscura/world/actors.hpp
// for what belongs in this file; see tools/lint/sim_purity.sh for what may not.

#include <obscura/world/actors.hpp>

#include <cstddef>
#include <vector>

#include <obscura/world/hull.hpp>
#include <obscura/world/model.hpp>

namespace obscura::world {

auto Roster::size() const -> std::size_t {
  return m_actors.size();
}

auto Roster::add(RoomId start) -> ActorId {
  if (m_actors.size() >= ACTOR_ANY) {
    return ACTOR_ANY;
  }

  const auto id = static_cast<ActorId>(m_actors.size());
  m_actors.push_back(Actor{
      .id = id,
      .role = Role::deckhand,
      .name = static_cast<StringId>(id),
      .timeline = {{.when = 0, .where = start, .what = Action::move}},
  });
  return id;
}

auto Roster::room_of(ActorId id) const -> RoomId {
  if (id >= m_actors.size()) {
    return kNoRoom;
  }
  if (m_actors[id].timeline.empty()) {
    return ROOM_ANY;
  }
  return m_actors[id].timeline.back().where;
}

auto Roster::move_to(const Hull& hull, ActorId id, RoomId destination) -> bool {
  if (id >= m_actors.size()) {
    return false;
  }
  const RoomId current = room_of(id);
  if (!hull.adjacent(current, destination)) {
    return false;
  }

  const TimeStep last = m_actors[id].timeline.back().when;
  if (last >= TIME_ANY - 1U) {
    return false;
  }

  m_actors[id].timeline.push_back(ActorStep{
      .when = last + 1U,
      .where = destination,
      .what = Action::move,
  });
  return true;
}

auto Roster::all() const -> const std::vector<Actor>& {
  return m_actors;
}

} // namespace obscura::world
