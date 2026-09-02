// The case registry: every authored case, in one list, plus the build step that
// turns a case description into a world.
//
// This file is the single point of enumeration. Tiny fixtures may remain
// headers; full authored cases live in their own translation units and expose a
// const reference to their constexpr descriptor.

#include <obscura/cases/case_data.hpp>

#include <array>
#include <span>
#include <vector>

#include <obscura/world/actors.hpp>
#include <obscura/world/hull.hpp>
#include <obscura/world/model.hpp>
#include <obscura/world/truth.hpp>

#include "case001_cold_lantern.hpp"
#include "derelict_alpha.hpp"

namespace obscura::cases {

namespace {

// Append only. The index into this array is stored in a Recording, so inserting
// a case in the middle silently re-points every recording made before it.
const std::array<CaseData, 2> kCases{{
    authored::kDerelictAlpha,
    authored::cold_lantern(),
}};

} // namespace

auto build(const CaseData& data) -> World {
  World result{
      .cause = data.cause,
      .origin = data.origin,
  };

  if (data.rooms.empty()) {
    for (world::RoomId index = 0; index < data.room_count; ++index) {
      result.hull.add_room();
    }
  } else {
    for (const RoomData& room : data.rooms) {
      result.hull.add_room(world::Compartment{
          .id = room.id,
          .archetype = room.archetype,
          .damage = room.damage,
          .state = world::Resolution::unknown,
          .bounds = room.bounds,
      });
    }
  }

  for (const Link& link : data.links) {
    result.hull.connect(link.from, link.to);
  }

  if (data.actors.empty()) {
    for (world::ActorId index = 0; index < data.actor_count; ++index) {
      result.roster.add(0);
    }
  } else {
    for (const ActorData& actor : data.actors) {
      result.roster.add(world::Actor{
          .id = actor.id,
          .role = actor.role,
          .name = actor.name,
          .timeline = std::vector<world::ActorStep>(actor.timeline.begin(),
                                                    actor.timeline.end()),
      });
    }
  }

  result.served_evidence.reserve(data.served_evidence.size());
  for (const EvidenceData& item : data.served_evidence) {
    result.served_evidence.push_back(world::Evidence{
        .id = item.id,
        .location = item.location,
        .kind = item.kind,
        .asserts =
            std::vector<world::Fact>(item.asserts.begin(), item.asserts.end()),
        .veracity = item.veracity,
        .requires_ = item.requires_,
        .body = item.body,
    });
  }

  return result;
}

auto all() -> std::span<const CaseData> {
  return kCases;
}

} // namespace obscura::cases
