#pragma once

// The shape of an authored case, and how a case becomes a world.
//
// What belongs here: the *type*. The data itself lives in cases/, as constexpr
// arrays — authored content compiled in rather than parsed at startup, because
// a case that can fail to load is a case that can fail to load in front of a
// player, and because the case solver can then be run over every shipped case
// at test time with nothing to mock.
//
// A CaseData is a description, not a world. build() turns one into a Hull and a
// Roster, deterministically: same case in, same world out, every time and on
// every platform. That is what lets a Recording name a case by index and a seed
// by value and nothing else.

#include <cstdint>
#include <span>
#include <string_view>

#include <obscura/world/actors.hpp>
#include <obscura/world/hull.hpp>

namespace obscura::cases {

// One connection in the hull. A flat pair rather than an adjacency list so the
// authored form stays readable as a table.
struct Link {
  world::RoomId from{world::kNoRoom};
  world::RoomId to{world::kNoRoom};
};

struct CaseData {
  std::string_view      name{};
  world::RoomId         room_count{0};
  world::ActorId        actor_count{0};
  // How many ticks the incident may fall within.
  world::Tick           horizon{0};
  // The run's attention budget, in whole looks.
  std::uint32_t         attention{0};
  std::span<const Link> links{};
};

// The hull and roster a case describes, built together because an actor's
// starting room is only meaningful against a hull.
struct World {
  world::Hull   hull{};
  world::Roster roster{};
};

// Pure. Every actor starts in room 0; a case that wants otherwise gets a
// starting-room table when the first one needs it, rather than an optional
// field nothing fills in.
[[nodiscard]] auto build(const CaseData& data) -> World;

// Every case that ships, in a stable order. The index into this span is what a
// Recording stores, so inserting a case in the middle invalidates old
// recordings — append.
[[nodiscard]] auto all() -> std::span<const CaseData>;

}  // namespace obscura::cases
