// The case registry: every authored case, in one list, plus the build step that
// turns a case description into a world.
//
// This is the only translation unit in cases/. The cases themselves are headers
// of constexpr data (derelict_alpha.hpp and friends); this file exists to give
// them a single point of enumeration and an address, so that `all()` is a real
// symbol a test can call rather than a header that every unit re-instantiates.

#include <obscura/cases/case_data.hpp>

#include <array>
#include <span>

#include <obscura/world/actors.hpp>
#include <obscura/world/hull.hpp>

#include "derelict_alpha.hpp"

namespace obscura::cases {

namespace {

// Append only. The index into this array is stored in a Recording, so inserting
// a case in the middle silently re-points every recording made before it.
constexpr std::array<CaseData, 1> kCases{{
    authored::kDerelictAlpha,
}};

}  // namespace

auto build(const CaseData& data) -> World {
  World world{};

  for (world::RoomId index = 0; index < data.room_count; ++index) {
    world.hull.add_room();
  }

  for (const Link& link : data.links) {
    world.hull.connect(link.from, link.to);
  }

  for (world::ActorId index = 0; index < data.actor_count; ++index) {
    world.roster.add(0);
  }

  return world;
}

auto all() -> std::span<const CaseData> {
  return kCases;
}

}  // namespace obscura::cases
