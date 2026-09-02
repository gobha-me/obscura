#include <catch2/catch_test_macros.hpp>

#include <obscura/cases/case_data.hpp>
#include <obscura/world/actors.hpp>
#include <obscura/world/hull.hpp>
#include <obscura/world/model.hpp>
#include <obscura/world/truth.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace {

using obscura::cases::ActorData;
using obscura::cases::CaseData;
using obscura::cases::ChainWitness;
using obscura::cases::EvidenceData;
using obscura::cases::RoomData;
using obscura::cases::ShipClass;
using obscura::cases::SolutionBatch;
using obscura::world::Action;
using obscura::world::ACTION_ANY;
using obscura::world::Actor;
using obscura::world::ACTOR_ANY;
using obscura::world::ActorId;
using obscura::world::ActorStep;
using obscura::world::Archetype;
using obscura::world::Commit;
using obscura::world::Damage;
using obscura::world::EvidenceId;
using obscura::world::EvidenceKind;
using obscura::world::Fact;
using obscura::world::IncidentArchetype;
using obscura::world::Instrument;
using obscura::world::InstrumentMask;
using obscura::world::Role;
using obscura::world::ROOM_ANY;
using obscura::world::TIME_ANY;
using obscura::world::Veracity;

auto cold_lantern() -> const CaseData& {
  const std::span<const CaseData> cases = obscura::cases::all();
  REQUIRE(cases.size() == 2);
  REQUIRE(cases[0].name == "derelict alpha");
  return cases[1];
}

struct RoomExpected {
  std::string_view name;
  Archetype archetype;
  Damage damage;
  std::uint16_t col;
  std::uint16_t row;
};

constexpr std::array<RoomExpected, 12> kRooms{{
    {"Bridge", Archetype::bridge, Damage::intact, 0, 0},
    {"Comms", Archetype::comms, Damage::intact, 24, 0},
    {"Berth A", Archetype::berth, Damage::intact, 72, 0},
    {"Berth B", Archetype::berth, Damage::intact, 96, 0},
    {"Medbay", Archetype::medbay, Damage::intact, 48, 0},
    {"Fwd airlock", Archetype::airlock, Damage::intact, 0, 11},
    {"Galley", Archetype::galley, Damage::intact, 24, 11},
    {"Hold 1", Archetype::hold, Damage::damaged, 48, 11},
    {"Hold 2", Archetype::hold, Damage::breached, 72, 11},
    {"Aft airlock", Archetype::airlock, Damage::intact, 96, 11},
    {"Pump bay", Archetype::engineering, Damage::damaged, 48, 22},
    {"Engine room", Archetype::engineering, Damage::intact, 72, 22},
}};

constexpr std::array<std::array<obscura::world::RoomId, 4>, 12> kNeighbors{{
    {{1, 5, ROOM_ANY, ROOM_ANY}},
    {{0, 4, 6, ROOM_ANY}},
    {{3, 4, ROOM_ANY, ROOM_ANY}},
    {{2, 9, ROOM_ANY, ROOM_ANY}},
    {{1, 2, 7, ROOM_ANY}},
    {{0, 6, ROOM_ANY, ROOM_ANY}},
    {{1, 5, 7, ROOM_ANY}},
    {{4, 6, 8, 10}},
    {{7, 9, 11, ROOM_ANY}},
    {{3, 8, ROOM_ANY, ROOM_ANY}},
    {{7, 11, ROOM_ANY, ROOM_ANY}},
    {{8, 10, ROOM_ANY, ROOM_ANY}},
}};

constexpr std::array<std::size_t, 12> kNeighborCounts{{
    2,
    3,
    2,
    2,
    3,
    2,
    3,
    4,
    3,
    2,
    2,
    2,
}};

struct ActorExpected {
  std::string_view name;
  Role role;
  std::span<const ActorStep> timeline;
};

constexpr std::array<ActorStep, 4> kVantnerTimeline{{
    {.when = 0, .where = 0, .what = Action::move},
    {.when = 5, .where = 0, .what = Action::vent},
    {.when = 6, .where = 5, .what = Action::move},
    {.when = 8, .where = 5, .what = Action::abandon},
}};
constexpr std::array<ActorStep, 5> kBehnTimeline{{
    {.when = 0, .where = 0, .what = Action::move},
    {.when = 4, .where = 1, .what = Action::move},
    {.when = 4, .where = 1, .what = Action::broadcast},
    {.when = 6, .where = 5, .what = Action::move},
    {.when = 8, .where = 5, .what = Action::abandon},
}};
constexpr std::array<ActorStep, 5> kKarrTimeline{{
    {.when = 0, .where = 10, .what = Action::move},
    {.when = 3, .where = 10, .what = Action::isolate},
    {.when = 6, .where = 11, .what = Action::move},
    {.when = 7, .where = 8, .what = Action::move},
    {.when = 8, .where = 9, .what = Action::abandon},
}};
constexpr std::array<ActorStep, 4> kAchebeTimeline{{
    {.when = 0, .where = 11, .what = Action::move},
    {.when = 2, .where = 8, .what = Action::move},
    {.when = 3, .where = 8, .what = Action::fight_fire},
    {.when = 4, .where = 8, .what = Action::die},
}};
constexpr std::array<ActorStep, 6> kReyesTimeline{{
    {.when = 0, .where = 4, .what = Action::move},
    {.when = 3, .where = 7, .what = Action::move},
    {.when = 5, .where = 4, .what = Action::move},
    {.when = 6, .where = 4, .what = Action::treat},
    {.when = 7, .where = 5, .what = Action::move},
    {.when = 8, .where = 5, .what = Action::abandon},
}};
constexpr std::array<ActorStep, 5> kQuintTimeline{{
    {.when = 0, .where = 8, .what = Action::stow},
    {.when = 1, .where = 7, .what = Action::move},
    {.when = 3, .where = 6, .what = Action::move},
    {.when = 6, .where = 5, .what = Action::move},
    {.when = 8, .where = 5, .what = Action::abandon},
}};
constexpr std::array<ActorStep, 5> kHalimTimeline{{
    {.when = 0, .where = 6, .what = Action::move},
    {.when = 1, .where = 7, .what = Action::move},
    {.when = 3, .where = 7, .what = Action::seal},
    {.when = 5, .where = 4, .what = Action::move},
    {.when = 8, .where = 5, .what = Action::abandon},
}};

constexpr std::array<ActorExpected, 7> kActors{{
    {"Ilse Vantner", Role::master, kVantnerTimeline},
    {"Osric Behn", Role::mate, kBehnTimeline},
    {"Duna Karr", Role::engineer, kKarrTimeline},
    {"Fen Achebe", Role::oiler, kAchebeTimeline},
    {"Tobin Reyes", Role::medic, kReyesTimeline},
    {"Marisol Quint", Role::supercargo, kQuintTimeline},
    {"Yeo Halim", Role::deckhand, kHalimTimeline},
}};

struct EvidenceExpected {
  EvidenceId id;
  obscura::world::RoomId location;
  EvidenceKind kind;
  Veracity veracity;
  InstrumentMask required_instruments;
  Fact fact;
  std::string_view body;
};

constexpr InstrumentMask kLamp =
    obscura::world::instrument_mask(Instrument::spectral_lamp);
constexpr InstrumentMask kDecrypter =
    obscura::world::instrument_mask(Instrument::decrypter);
constexpr InstrumentMask kThermal =
    obscura::world::instrument_mask(Instrument::thermal_tap);
constexpr InstrumentMask kOssuary =
    obscura::world::instrument_mask(Instrument::ossuary_tag);

constexpr std::array<EvidenceExpected, 16> kEvidence{{
    {0,
     8,
     EvidenceKind::cargo_seal,
     Veracity::true_,
     0,
     {ACTOR_ANY, 0, 8, Action::stow},
     "undeclared oxidiser-class seal stowed here"},
    {1,
     7,
     EvidenceKind::manifest,
     Veracity::true_,
     0,
     {5, TIME_ANY, ROOM_ANY, Action::stow},
     "stowage authority, her chop in the wax; seal number not in the declared "
     "list"},
    {2,
     8,
     EvidenceKind::corpse,
     Veracity::true_,
     kOssuary,
     {3, TIME_ANY, 8, Action::die},
     "without the tag: unidentified"},
    {3,
     8,
     EvidenceKind::physical_trace,
     Veracity::true_,
     kLamp,
     {ACTOR_ANY, TIME_ANY, 8, Action::fight_fire},
     "discharge from hand height, abandoned mid-sweep"},
    {4,
     11,
     EvidenceKind::personal_effect,
     Veracity::true_,
     0,
     {3, TIME_ANY, 11, ACTION_ANY},
     "oiler's tally board, shift signed on"},
    {5,
     7,
     EvidenceKind::damage_pattern,
     Veracity::true_,
     kThermal,
     {ACTOR_ANY, 3, 7, ACTION_ANY},
     "unreadable in case 1"},
    {6,
     7,
     EvidenceKind::physical_trace,
     Veracity::true_,
     kLamp,
     {6, TIME_ANY, 7, Action::seal},
     "palm salts on the dog-lever, one set of hands"},
    {7,
     6,
     EvidenceKind::personal_effect,
     Veracity::stale,
     0,
     {6, TIME_ANY, 6, ACTION_ANY},
     "half-finished provision list — true at T0 only"},
    {8,
     0,
     EvidenceKind::log_fragment,
     Veracity::true_,
     kDecrypter,
     {0, 5, 0, Action::vent},
     "MASTER: VENTING HOLD 1 ON MY ORDER"},
    {9,
     1,
     EvidenceKind::log_fragment,
     Veracity::true_,
     kDecrypter,
     {1, 4, 1, Action::broadcast},
     "distress traffic, mate's authentication"},
    {11,
     10,
     EvidenceKind::physical_trace,
     Veracity::true_,
     kLamp,
     {2, TIME_ANY, 10, Action::isolate},
     "valve wheel wiped, chalk mark on the isolation tag"},
    {12,
     11,
     EvidenceKind::personal_effect,
     Veracity::stale,
     0,
     {2, TIME_ANY, 11, ACTION_ANY},
     "engineer's tea can, still lashed"},
    {13,
     4,
     EvidenceKind::log_fragment,
     Veracity::true_,
     kDecrypter,
     {4, 6, 4, Action::treat},
     "smoke inhalation, one patient, deckhand"},
    {15,
     5,
     EvidenceKind::manifest,
     Veracity::true_,
     0,
     {ACTOR_ANY, 8, 5, Action::abandon},
     "boat log: five out"},
    {16,
     5,
     EvidenceKind::personal_effect,
     Veracity::true_,
     0,
     {5, 8, 5, Action::abandon},
     "supercargo's seal press, dropped in the cycle"},
    {17,
     8,
     EvidenceKind::personal_effect,
     Veracity::true_,
     0,
     {2, TIME_ANY, 8, ACTION_ANY},
     "left-cuff engineer's glove, scorched palm, eleven feet from the corpse — "
     "the trap"},
}};

struct RedactedExpected {
  EvidenceId id;
  std::string_view item;
  std::string_view reason;
};

constexpr std::array<RedactedExpected, 6> kRedacted{{
    {10, "Master's order book, R00",
     "Redundant with E09, which is stronger and instrument-gated"},
    {14, "Soot transfer on the medbay cot rail",
     "Redundant with E14 — \"patient: deckhand\" already identifies Halim"},
    {18, "Aft boat cycle log, R09",
     "Pins a fact no commit needs; its absence leaves Karr's exit slightly "
     "mysterious, which is a feature"},
    {19, "Second contraband ledger, R03",
     "Over-determines Quint; made commit 1 free"},
    {20, "Reaction-origin burn pattern, R08",
     "Needs the thermal tap, not in the published loadout. One unreadable item "
     "teaches the lesson; two is tax"},
    {21, "Behn's trace in comms", "Redundant with E10"},
}};

constexpr std::array<std::array<EvidenceId, 4>, 9> kChainEvidence{{
    {{0, 1, obscura::world::EVIDENCE_NONE, obscura::world::EVIDENCE_NONE}},
    {{3, 2, 4, 11}},
    {{2, obscura::world::EVIDENCE_NONE, obscura::world::EVIDENCE_NONE,
      obscura::world::EVIDENCE_NONE}},
    {{6, obscura::world::EVIDENCE_NONE, obscura::world::EVIDENCE_NONE,
      obscura::world::EVIDENCE_NONE}},
    {{8, obscura::world::EVIDENCE_NONE, obscura::world::EVIDENCE_NONE,
      obscura::world::EVIDENCE_NONE}},
    {{9, obscura::world::EVIDENCE_NONE, obscura::world::EVIDENCE_NONE,
      obscura::world::EVIDENCE_NONE}},
    {{11, obscura::world::EVIDENCE_NONE, obscura::world::EVIDENCE_NONE,
      obscura::world::EVIDENCE_NONE}},
    {{13, obscura::world::EVIDENCE_NONE, obscura::world::EVIDENCE_NONE,
      obscura::world::EVIDENCE_NONE}},
    {{16, 15, obscura::world::EVIDENCE_NONE, obscura::world::EVIDENCE_NONE}},
}};

constexpr std::array<std::size_t, 9> kChainSizes{{2, 4, 1, 1, 1, 1, 1, 1, 2}};

auto same_fact(const Fact& lhs, const Fact& rhs) -> bool {
  return lhs.actor == rhs.actor && lhs.when == rhs.when &&
         lhs.where == rhs.where && lhs.what == rhs.what;
}

auto same_commit(const Commit& lhs, const Commit& rhs) -> bool {
  return lhs.actor == rhs.actor && lhs.where == rhs.where &&
         lhs.what == rhs.what;
}

} // namespace

TEST_CASE("authored case construction refuses malformed dense records",
          "[cases][failure]") {
  obscura::world::Hull hull{};
  CHECK(hull.add_room(obscura::world::Compartment{.id = 1}) == ROOM_ANY);
  CHECK(hull.room_count() == 0);

  obscura::world::Roster roster{};
  CHECK(roster.add(Actor{
            .id = 1,
            .timeline = {{.when = 0, .where = 0, .what = Action::move}}}) ==
        ACTOR_ANY);
  CHECK(roster.add(Actor{.id = 0, .timeline = {}}) == ACTOR_ANY);
  CHECK(roster.size() == 0);
}

TEST_CASE("redaction proof fails closed when its witness is damaged",
          "[cases][failure]") {
  const CaseData& source = cold_lantern();

  SECTION("a missing chain is rejected") {
    CaseData broken = source;
    broken.chains = broken.chains.first(8);
    CHECK_FALSE(obscura::cases::redaction_invariant(broken));
  }

  SECTION("an unknown evidence id is rejected") {
    std::array<ChainWitness, 9> chains{};
    std::ranges::copy(source.chains, chains.begin());
    constexpr std::array<EvidenceId, 1> unknown{{
        obscura::world::EVIDENCE_NONE,
    }};
    chains[0].evidence = unknown;
    CaseData broken = source;
    broken.chains = chains;
    CHECK_FALSE(obscura::cases::redaction_invariant(broken));
  }

  SECTION("evidence outside the published loadout is rejected") {
    CaseData broken = source;
    broken.published_loadout = 0;
    CHECK_FALSE(obscura::cases::redaction_invariant(broken));
  }

  SECTION("a commit absent from the actor timeline is rejected") {
    std::array<SolutionBatch, 3> solution{};
    std::ranges::copy(source.solution, solution.begin());
    solution[0].entries[0].commit.what = Action::hide;
    CaseData broken = source;
    broken.solution = solution;
    CHECK_FALSE(obscura::cases::redaction_invariant(broken));
  }

  SECTION("two witnesses for one commit leave another unproved") {
    std::array<ChainWitness, 9> chains{};
    std::ranges::copy(source.chains, chains.begin());
    chains[1].solution_index = 0;
    CaseData broken = source;
    broken.chains = chains;
    CHECK_FALSE(obscura::cases::redaction_invariant(broken));
  }

  SECTION("the canonical witness passes") {
    CHECK(obscura::cases::redaction_invariant(source));
  }
}

TEST_CASE("authored identities fail closed when the dense domain is damaged",
          "[cases][failure]") {
  const CaseData& source = cold_lantern();

  SECTION("a duplicate room id is rejected") {
    std::array<RoomData, 12> rooms{};
    std::ranges::copy(source.rooms, rooms.begin());
    rooms[1].id = 0;
    CaseData broken = source;
    broken.rooms = rooms;
    CHECK_FALSE(obscura::cases::ids_are_dense(broken));
  }

  SECTION("a missing evidence id is rejected") {
    CaseData broken = source;
    broken.redacted_evidence = broken.redacted_evidence.first(5);
    CHECK_FALSE(obscura::cases::ids_are_dense(broken));
  }

  SECTION("a duplicate evidence id is rejected") {
    std::array<EvidenceData, 16> evidence{};
    std::ranges::copy(source.served_evidence, evidence.begin());
    evidence[1].id = evidence[0].id;
    CaseData broken = source;
    broken.served_evidence = evidence;
    CHECK_FALSE(obscura::cases::ids_are_dense(broken));
  }

  SECTION("the canonical dense domains pass") {
    CHECK(obscura::cases::ids_are_dense(source));
  }
}

TEST_CASE("Cold Lantern hull and crew match the authored source", "[cases]") {
  const CaseData& data = cold_lantern();
  CHECK(data.name == "Cold Lantern");
  CHECK(data.ship_class == ShipClass::freighter);
  CHECK(data.room_count == 12);
  CHECK(data.actor_count == 7);
  CHECK(data.evidence_count == 22);
  CHECK(data.horizon == 9);
  CHECK(data.attention == 120);
  CHECK(data.links.size() == 15);
  CHECK(data.cause == IncidentArchetype::contraband_reaction);
  CHECK(data.origin == 8);
  CHECK(obscura::cases::text(data, obscura::world::STRING_NONE).empty());

  REQUIRE(data.rooms.size() == kRooms.size());
  for (std::size_t index = 0; index < data.rooms.size(); ++index) {
    const RoomData& actual = data.rooms[index];
    const RoomExpected& expected = kRooms[index];
    CHECK(actual.id == index);
    CHECK(obscura::cases::text(data, actual.name) == expected.name);
    CHECK(actual.archetype == expected.archetype);
    CHECK(actual.damage == expected.damage);
    CHECK(actual.bounds.col == expected.col);
    CHECK(actual.bounds.row == expected.row);
    CHECK(actual.bounds.width == 22);
    CHECK(actual.bounds.height == 9);
  }

  REQUIRE(data.actors.size() == kActors.size());
  for (std::size_t index = 0; index < data.actors.size(); ++index) {
    const ActorData& actual = data.actors[index];
    const ActorExpected& expected = kActors[index];
    CHECK(actual.id == index);
    CHECK(obscura::cases::text(data, actual.name) == expected.name);
    CHECK(actual.role == expected.role);
    REQUIRE(actual.timeline.size() == expected.timeline.size());
    CHECK(std::ranges::is_sorted(actual.timeline, {}, &ActorStep::when));
    for (std::size_t step = 0; step < actual.timeline.size(); ++step) {
      CHECK(actual.timeline[step].when == expected.timeline[step].when);
      CHECK(actual.timeline[step].where == expected.timeline[step].where);
      CHECK(actual.timeline[step].what == expected.timeline[step].what);
    }
  }

  const obscura::cases::World built = obscura::cases::build(data);
  CHECK(built.cause == data.cause);
  CHECK(built.origin == data.origin);
  REQUIRE(built.hull.room_count() == data.room_count);
  REQUIRE(built.roster.size() == data.actor_count);

  for (std::size_t room = 0; room < kNeighbors.size(); ++room) {
    const auto& actual =
        built.hull.neighbors(static_cast<obscura::world::RoomId>(room));
    REQUIRE(actual.size() == kNeighborCounts[room]);
    CHECK(std::ranges::is_sorted(actual));
    for (std::size_t neighbor = 0; neighbor < actual.size(); ++neighbor) {
      CHECK(actual[neighbor] == kNeighbors[room][neighbor]);
    }
  }

  for (std::size_t actor = 0; actor < data.actors.size(); ++actor) {
    const Actor& actual = built.roster.all()[actor];
    CHECK(actual.id == data.actors[actor].id);
    CHECK(actual.role == data.actors[actor].role);
    CHECK(actual.name == data.actors[actor].name);
    REQUIRE(actual.timeline.size() == data.actors[actor].timeline.size());
    for (std::size_t step = 0; step < actual.timeline.size(); ++step) {
      CHECK(actual.timeline[step].when ==
            data.actors[actor].timeline[step].when);
      CHECK(actual.timeline[step].where ==
            data.actors[actor].timeline[step].where);
      CHECK(actual.timeline[step].what ==
            data.actors[actor].timeline[step].what);
    }
  }
}

TEST_CASE("Cold Lantern evidence, solution, and redaction are exact",
          "[cases]") {
  const CaseData& data = cold_lantern();
  const InstrumentMask expected_loadout = kLamp | kDecrypter | kOssuary;
  CHECK(data.published_loadout == expected_loadout);
  CHECK((data.published_loadout & kThermal) == 0);

  REQUIRE(data.served_evidence.size() == kEvidence.size());
  std::size_t stale = 0;
  std::size_t misleading = 0;
  for (std::size_t index = 0; index < data.served_evidence.size(); ++index) {
    const EvidenceData& actual = data.served_evidence[index];
    const EvidenceExpected& expected = kEvidence[index];
    CHECK(actual.id == expected.id);
    CHECK(actual.location == expected.location);
    CHECK(actual.kind == expected.kind);
    CHECK(actual.veracity == expected.veracity);
    CHECK(actual.requires_ == expected.required_instruments);
    REQUIRE(actual.asserts.size() == 1);
    CHECK(same_fact(actual.asserts.front(), expected.fact));
    CHECK(obscura::cases::text(data, actual.body) == expected.body);
    stale += actual.veracity == Veracity::stale ? 1U : 0U;
    misleading += actual.veracity == Veracity::misleading ? 1U : 0U;
  }
  CHECK(stale == 2);
  CHECK(misleading == 0);

  const obscura::cases::World built = obscura::cases::build(data);
  REQUIRE(built.served_evidence.size() == data.served_evidence.size());
  for (std::size_t index = 0; index < built.served_evidence.size(); ++index) {
    CHECK(built.served_evidence[index].id == data.served_evidence[index].id);
    REQUIRE(built.served_evidence[index].asserts.size() == 1);
    CHECK(same_fact(built.served_evidence[index].asserts.front(),
                    data.served_evidence[index].asserts.front()));
  }

  REQUIRE(data.redacted_evidence.size() == kRedacted.size());
  for (std::size_t index = 0; index < kRedacted.size(); ++index) {
    CHECK(data.redacted_evidence[index].id == kRedacted[index].id);
    CHECK(data.redacted_evidence[index].item == kRedacted[index].item);
    CHECK(data.redacted_evidence[index].reason == kRedacted[index].reason);
  }

  constexpr std::array<Commit, 9> solution{{
      {.actor = 5, .where = 8, .what = Action::stow},
      {.actor = 3, .where = 8, .what = Action::fight_fire},
      {.actor = 3, .where = 8, .what = Action::die},
      {.actor = 6, .where = 7, .what = Action::seal},
      {.actor = 0, .where = 0, .what = Action::vent},
      {.actor = 1, .where = 1, .what = Action::broadcast},
      {.actor = 2, .where = 10, .what = Action::isolate},
      {.actor = 4, .where = 4, .what = Action::treat},
      {.actor = 5, .where = 5, .what = Action::abandon},
  }};
  constexpr std::array<std::string_view, 9> readings{{
      "The contraband went in under her chop.",
      "Somebody stood and fought it.",
      "He did not leave.",
      "Dogged from the forward side — with Achebe behind it.",
      "The master vented Hold 1 to kill the fire.",
      "The mate sent the distress traffic.",
      "Why she was not in the hold.",
      "One patient, smoke inhalation, the deckhand.",
      "She left with the boat, and the second ledger.",
  }};
  REQUIRE(data.solution.size() == 3);
  REQUIRE(data.chains.size() == solution.size());
  for (std::size_t index = 0; index < solution.size(); ++index) {
    CHECK(same_commit(data.solution[index / 3].entries[index % 3].commit,
                      solution[index]));
    CHECK(data.solution[index / 3].entries[index % 3].reading ==
          readings[index]);
    CHECK(data.chains[index].solution_index == index);
    REQUIRE(data.chains[index].evidence.size() == kChainSizes[index]);
    for (std::size_t item = 0; item < data.chains[index].evidence.size();
         ++item) {
      CHECK(data.chains[index].evidence[item] == kChainEvidence[index][item]);
    }
    CHECK_FALSE(data.chains[index].argument.empty());
  }

  const auto thermal =
      std::ranges::find_if(data.served_evidence, [](const EvidenceData& item) {
        return item.id == 5;
      });
  REQUIRE(thermal != data.served_evidence.end());
  CHECK(thermal->kind == EvidenceKind::damage_pattern);
  CHECK((thermal->requires_ & data.published_loadout) != thermal->requires_);
  CHECK(obscura::cases::redaction_invariant(data));
}
