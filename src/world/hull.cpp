// [SIM] Implementation of the hull topology. See include/obscura/world/hull.hpp
// for what belongs in this file; see tools/lint/sim_purity.sh for what may not.

#include <obscura/world/hull.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <obscura/world/model.hpp>

namespace obscura::world {

namespace {

const std::vector<RoomId> kNoNeighbors{};

} // namespace

auto Hull::room_count() const -> std::size_t {
  return m_rooms.size();
}

auto Hull::add_room() -> RoomId {
  return add_room(Compartment{.id = static_cast<RoomId>(m_rooms.size())});
}

auto Hull::add_room(Compartment room) -> RoomId {
  if (m_rooms.size() >= ROOM_ANY) {
    return ROOM_ANY;
  }

  const auto id = static_cast<RoomId>(m_rooms.size());
  if (room.id != id) {
    return ROOM_ANY;
  }
  room.adjacent.clear();
  m_rooms.push_back(room);
  return id;
}

auto Hull::connect(RoomId lhs, RoomId rhs) -> void {
  if (lhs == rhs || lhs >= m_rooms.size() || rhs >= m_rooms.size()) {
    return;
  }

  // Insert in sorted position rather than push_back + sort: it keeps the
  // invariant local to the one place that can break it, and the neighbor lists
  // are short enough that the linear search is not the interesting cost.
  const auto link = [](std::vector<RoomId>& into, RoomId what) {
    const auto at = std::ranges::lower_bound(into, what);
    if (at == into.end() || *at != what) {
      into.insert(at, what);
    }
  };

  link(m_rooms[lhs].adjacent, rhs);
  link(m_rooms[rhs].adjacent, lhs);
}

auto Hull::adjacent(RoomId lhs, RoomId rhs) const -> bool {
  if (lhs >= m_rooms.size()) {
    return false;
  }
  return std::ranges::binary_search(m_rooms[lhs].adjacent, rhs);
}

auto Hull::distance(RoomId from, RoomId to) const
    -> std::optional<std::uint16_t> {
  if (from >= m_rooms.size() || to >= m_rooms.size()) {
    return std::nullopt;
  }
  if (from == to) {
    return 0;
  }

  std::vector<std::uint16_t> distances(m_rooms.size(), ROOM_ANY);
  std::vector<RoomId> pending{};
  pending.reserve(m_rooms.size());
  distances[from] = 0;
  pending.push_back(from);

  for (std::size_t next = 0; next < pending.size(); ++next) {
    const RoomId room = pending[next];
    for (const RoomId neighbor : m_rooms[room].adjacent) {
      if (distances[neighbor] != ROOM_ANY) {
        continue;
      }
      distances[neighbor] = static_cast<std::uint16_t>(distances[room] + 1U);
      if (neighbor == to) {
        return distances[neighbor];
      }
      pending.push_back(neighbor);
    }
  }

  return std::nullopt;
}

auto Hull::neighbors(RoomId id) const -> const std::vector<RoomId>& {
  if (id >= m_rooms.size()) {
    return kNoNeighbors;
  }
  return m_rooms[id].adjacent;
}

auto Hull::all() const -> const std::vector<Compartment>& {
  return m_rooms;
}

} // namespace obscura::world
