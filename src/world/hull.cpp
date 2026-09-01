// [SIM] Implementation of the hull topology. See include/obscura/world/hull.hpp
// for what belongs in this file; see tools/lint/sim_purity.sh for what may not.

#include <obscura/world/hull.hpp>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace obscura::world {

namespace {

const std::vector<RoomId> kNoNeighbors{};

} // namespace

auto Hull::room_count() const -> std::size_t {
  return m_rooms.size();
}

auto Hull::add_room() -> RoomId {
  const auto id = static_cast<RoomId>(m_rooms.size());
  m_rooms.push_back(Room{.id = id, .neighbors = {}});
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

  link(m_rooms[lhs].neighbors, rhs);
  link(m_rooms[rhs].neighbors, lhs);
}

auto Hull::adjacent(RoomId lhs, RoomId rhs) const -> bool {
  if (lhs >= m_rooms.size()) {
    return false;
  }
  return std::ranges::binary_search(m_rooms[lhs].neighbors, rhs);
}

auto Hull::neighbors(RoomId id) const -> const std::vector<RoomId>& {
  if (id >= m_rooms.size()) {
    return kNoNeighbors;
  }
  return m_rooms[id].neighbors;
}

} // namespace obscura::world
