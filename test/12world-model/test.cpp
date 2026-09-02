#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <type_traits>
#include <vector>

#include <obscura/world/actors.hpp>
#include <obscura/world/hull.hpp>
#include <obscura/world/model.hpp>
#include <obscura/world/projection.hpp>
#include <obscura/world/redaction.hpp>
#include <obscura/world/truth.hpp>

namespace {

using namespace obscura::world;

template <typename T>
concept HasVeracity = requires(T value) { value.veracity; };

static_assert(std::is_same_v<RoomId, std::uint16_t>);
static_assert(std::is_same_v<ActorId, std::uint16_t>);
static_assert(std::is_same_v<EvidenceId, std::uint16_t>);
static_assert(std::is_same_v<StringId, std::uint16_t>);
static_assert(std::is_same_v<TimeStep, std::uint32_t>);
static_assert(std::is_same_v<InstrumentMask, std::uint8_t>);

static_assert(std::is_same_v<std::underlying_type_t<Archetype>, std::uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<Damage>, std::uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<Resolution>, std::uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<Action>, std::uint8_t>);
static_assert(
    std::is_same_v<std::underlying_type_t<EvidenceKind>, std::uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<Veracity>, std::uint8_t>);

static_assert(static_cast<std::uint8_t>(Archetype::comms) == 7);
static_assert(static_cast<std::uint8_t>(Damage::breached) == 2);
static_assert(static_cast<std::uint8_t>(Resolution::resolved) == 2);
static_assert(static_cast<std::uint8_t>(Action::die) == 10);
static_assert(static_cast<std::uint8_t>(EvidenceKind::personal_effect) == 6);
static_assert(static_cast<std::uint8_t>(Veracity::misleading) == 2);
static_assert(static_cast<std::uint8_t>(Role::deckhand) == 6);
static_assert(static_cast<std::uint8_t>(IncidentArchetype::contagion) == 4);
static_assert(static_cast<std::uint8_t>(ACTION_ANY) == 0xFFU);

static_assert(!HasVeracity<EvidenceProjection>);

TEST_CASE("world model rejects invalid identities and topology",
          "[world][failure]") {
  Hull hull{};
  const RoomId first = hull.add_room();
  const RoomId second = hull.add_room();

  hull.connect(first, first);
  hull.connect(first, ROOM_ANY);
  REQUIRE(hull.neighbors(first).empty());

  hull.connect(first, second);
  hull.connect(first, second);
  REQUIRE(hull.neighbors(first) == RoomIdList{second});
  REQUIRE(hull.neighbors(second) == RoomIdList{first});
  REQUIRE_FALSE(hull.adjacent(ROOM_ANY, first));

  Roster roster{};
  const ActorId actor = roster.add(first);
  REQUIRE_FALSE(roster.move_to(hull, ACTOR_ANY, second));
  REQUIRE_FALSE(roster.move_to(hull, actor, ROOM_ANY));
  REQUIRE(roster.room_of(ACTOR_ANY) == ROOM_ANY);
}

TEST_CASE("projection cannot carry evidence correctness", "[world][firewall]") {
  const Evidence secret{
      .id = 7,
      .location = 3,
      .kind = EvidenceKind::manifest,
      .asserts = {{.actor = 2, .when = 5, .where = 3, .what = Action::stow}},
      .veracity = Veracity::misleading,
      .requires_ = 4,
      .body = 11,
  };

  RedactionMask mask{1};
  CHECK(project(EvidenceSet{secret}, mask).empty());
  REQUIRE(mask.raise(0, Fidelity::Sensed));
  CHECK_FALSE(mask.raise(0, Fidelity::Hidden));
  auto visible = project(EvidenceSet{secret}, mask);
  REQUIRE(visible.size() == 1);
  CHECK(visible.front().id == secret.id);
  CHECK(visible.front().location == ROOM_ANY);
  CHECK(visible.front().asserts.empty());
  CHECK(visible.front().body == STRING_NONE);

  REQUIRE(mask.raise(0, Fidelity::Partial));
  visible = project(EvidenceSet{secret}, mask);
  REQUIRE(visible.size() == 1);
  CHECK(visible.front().location == secret.location);
  CHECK(visible.front().kind == secret.kind);
  CHECK(visible.front().requires_ == secret.requires_);
  CHECK(visible.front().asserts.empty());
  CHECK(visible.front().body == STRING_NONE);

  REQUIRE(mask.raise(0, Fidelity::Full));
  visible = project(EvidenceSet{secret}, mask);
  REQUIRE(visible.size() == 1);
  CHECK(visible.front().location == secret.location);
  CHECK(visible.front().kind == secret.kind);
  REQUIRE(visible.front().asserts.size() == 1);
  CHECK(visible.front().asserts.front().actor == 2);
  CHECK(visible.front().asserts.front().when == 5);
  CHECK(visible.front().asserts.front().where == 3);
  CHECK(visible.front().asserts.front().what == Action::stow);
  CHECK(visible.front().requires_ == secret.requires_);
  CHECK(visible.front().body == secret.body);
}

TEST_CASE("hull distance rejects invalid and disconnected rooms",
          "[world][distance][failure]") {
  Hull hull{};
  for (std::uint16_t room = 0; room < 5; ++room) {
    REQUIRE(hull.add_room() == room);
  }
  hull.connect(0, 1);
  hull.connect(1, 2);
  hull.connect(0, 3);
  hull.connect(3, 2);

  CHECK_FALSE(hull.distance(ROOM_ANY, 0).has_value());
  CHECK_FALSE(hull.distance(0, ROOM_ANY).has_value());
  CHECK_FALSE(hull.distance(0, 4).has_value());
  CHECK(hull.distance(2, 2) == std::optional<std::uint16_t>{0});
  CHECK(hull.distance(0, 2) == std::optional<std::uint16_t>{2});
  CHECK(hull.distance(2, 0) == std::optional<std::uint16_t>{2});
}

TEST_CASE("hull distance is independent of connection insertion order",
          "[world][distance][determinism]") {
  Hull ascending{};
  Hull descending{};
  for (std::uint16_t room = 0; room < 5; ++room) {
    REQUIRE(ascending.add_room() == room);
    REQUIRE(descending.add_room() == room);
  }
  ascending.connect(0, 1);
  ascending.connect(1, 2);
  ascending.connect(2, 3);
  ascending.connect(3, 4);
  descending.connect(3, 4);
  descending.connect(2, 3);
  descending.connect(1, 2);
  descending.connect(0, 1);

  for (std::uint16_t room = 0; room < 5; ++room) {
    CHECK(ascending.distance(0, room) == descending.distance(0, room));
  }
}

TEST_CASE("canonical records preserve dense source order", "[world][smoke]") {
  const Compartment compartment{
      .id = 4,
      .archetype = Archetype::hold,
      .damage = Damage::breached,
      .state = Resolution::surveyed,
      .bounds = {.col = 72, .row = 11, .width = 22, .height = 9},
      .adjacent = {3, 5},
  };
  const Actor actor{
      .id = 2,
      .role = Role::engineer,
      .name = 8,
      .timeline = {{.when = 3, .where = 4, .what = Action::isolate}},
  };
  const Commit commit{.actor = 2, .where = 4, .what = Action::isolate};
  CHECK(compartment.bounds.width == 22);
  CHECK(actor.timeline.front().what == commit.what);

  Hull hull{};
  CHECK(hull.add_room() == 0);
  CHECK(hull.add_room() == 1);
  CHECK(hull.all()[0].id == 0);
  CHECK(hull.all()[1].id == 1);
  hull.connect(0, 1);

  Roster roster{};
  CHECK(roster.add(0) == 0);
  CHECK(roster.add(1) == 1);
  REQUIRE(roster.move_to(hull, 0, 1));
  const ActorStep& moved = roster.all()[0].timeline.back();
  CHECK(moved.when == 1);
  CHECK(moved.where == 1);
  CHECK(moved.what == Action::move);

  const Truth truth{
      .crew = roster.all(),
      .cause = IncidentArchetype::contraband_reaction,
      .origin = 1,
      .hull = hull.all(),
      .all = {},
      .served = {},
  };
  CHECK(truth.crew.size() == 2);
  CHECK(truth.hull.size() == 2);
}

} // namespace
