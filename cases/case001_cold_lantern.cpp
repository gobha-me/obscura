// Case 001 — Cold Lantern.
//
// This is a transcription of docs/06-case-001-cold-lantern.md. The source
// document is the authority: changing this data is a content/design change,
// not a refactor. The six redacted items are recorded only at the fidelity the
// document supplies; their missing facts and bodies must not be invented.

#include <obscura/cases/case_data.hpp>

#include "case001_cold_lantern.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

#include <obscura/world/model.hpp>
#include <obscura/world/truth.hpp>

namespace obscura::cases::authored {

namespace {

using world::Action;
using world::ACTION_ANY;
using world::ACTOR_ANY;
using world::ActorId;
using world::ActorStep;
using world::Archetype;
using world::Damage;
using world::EvidenceId;
using world::EvidenceKind;
using world::Fact;
using world::IncidentArchetype;
using world::Instrument;
using world::InstrumentMask;
using world::Role;
using world::ROOM_ANY;
using world::RoomId;
using world::StringId;
using world::TIME_ANY;
using world::Veracity;

enum TextId : std::uint8_t {
  kBridge,
  kComms,
  kBerthA,
  kBerthB,
  kMedbay,
  kFwdAirlock,
  kGalley,
  kHold1,
  kHold2,
  kAftAirlock,
  kPumpBay,
  kEngineRoom,
  kIlseVantner,
  kOsricBehn,
  kDunaKarr,
  kFenAchebe,
  kTobinReyes,
  kMarisolQuint,
  kYeoHalim,
  kE01Body,
  kE02Body,
  kE03Body,
  kE04Body,
  kE05Body,
  kE06Body,
  kE07Body,
  kE08Body,
  kE09Body,
  kE10Body,
  kE12Body,
  kE13Body,
  kE14Body,
  kE16Body,
  kE17Body,
  kE18Body,
};

constexpr std::array<std::string_view, 35> kStrings{{
    "Bridge",
    "Comms",
    "Berth A",
    "Berth B",
    "Medbay",
    "Fwd airlock",
    "Galley",
    "Hold 1",
    "Hold 2",
    "Aft airlock",
    "Pump bay",
    "Engine room",
    "Ilse Vantner",
    "Osric Behn",
    "Duna Karr",
    "Fen Achebe",
    "Tobin Reyes",
    "Marisol Quint",
    "Yeo Halim",
    "undeclared oxidiser-class seal stowed here",
    "stowage authority, her chop in the wax; seal number not in the declared "
    "list",
    "without the tag: unidentified",
    "discharge from hand height, abandoned mid-sweep",
    "oiler's tally board, shift signed on",
    "unreadable in case 1",
    "palm salts on the dog-lever, one set of hands",
    "half-finished provision list — true at T0 only",
    "MASTER: VENTING HOLD 1 ON MY ORDER",
    "distress traffic, mate's authentication",
    "valve wheel wiped, chalk mark on the isolation tag",
    "engineer's tea can, still lashed",
    "smoke inhalation, one patient, deckhand",
    "boat log: five out",
    "supercargo's seal press, dropped in the cycle",
    "left-cuff engineer's glove, scorched palm, eleven feet from the corpse — "
    "the trap",
}};

constexpr std::array<RoomData, 12> kRooms{{
    {.id = 0,
     .name = kBridge,
     .archetype = Archetype::bridge,
     .damage = Damage::intact,
     .bounds = {.col = 0, .row = 0, .width = 22, .height = 9}},
    {.id = 1,
     .name = kComms,
     .archetype = Archetype::comms,
     .damage = Damage::intact,
     .bounds = {.col = 24, .row = 0, .width = 22, .height = 9}},
    {.id = 2,
     .name = kBerthA,
     .archetype = Archetype::berth,
     .damage = Damage::intact,
     .bounds = {.col = 72, .row = 0, .width = 22, .height = 9}},
    {.id = 3,
     .name = kBerthB,
     .archetype = Archetype::berth,
     .damage = Damage::intact,
     .bounds = {.col = 96, .row = 0, .width = 22, .height = 9}},
    {.id = 4,
     .name = kMedbay,
     .archetype = Archetype::medbay,
     .damage = Damage::intact,
     .bounds = {.col = 48, .row = 0, .width = 22, .height = 9}},
    {.id = 5,
     .name = kFwdAirlock,
     .archetype = Archetype::airlock,
     .damage = Damage::intact,
     .bounds = {.col = 0, .row = 11, .width = 22, .height = 9}},
    {.id = 6,
     .name = kGalley,
     .archetype = Archetype::galley,
     .damage = Damage::intact,
     .bounds = {.col = 24, .row = 11, .width = 22, .height = 9}},
    {.id = 7,
     .name = kHold1,
     .archetype = Archetype::hold,
     .damage = Damage::damaged,
     .bounds = {.col = 48, .row = 11, .width = 22, .height = 9}},
    {.id = 8,
     .name = kHold2,
     .archetype = Archetype::hold,
     .damage = Damage::breached,
     .bounds = {.col = 72, .row = 11, .width = 22, .height = 9}},
    {.id = 9,
     .name = kAftAirlock,
     .archetype = Archetype::airlock,
     .damage = Damage::intact,
     .bounds = {.col = 96, .row = 11, .width = 22, .height = 9}},
    {.id = 10,
     .name = kPumpBay,
     .archetype = Archetype::engineering,
     .damage = Damage::damaged,
     .bounds = {.col = 48, .row = 22, .width = 22, .height = 9}},
    {.id = 11,
     .name = kEngineRoom,
     .archetype = Archetype::engineering,
     .damage = Damage::intact,
     .bounds = {.col = 72, .row = 22, .width = 22, .height = 9}},
}};

constexpr std::array<Link, 15> kLinks{{
    {.from = 0, .to = 1},
    {.from = 0, .to = 5},
    {.from = 1, .to = 4},
    {.from = 1, .to = 6},
    {.from = 2, .to = 3},
    {.from = 2, .to = 4},
    {.from = 3, .to = 9},
    {.from = 4, .to = 7},
    {.from = 5, .to = 6},
    {.from = 6, .to = 7},
    {.from = 7, .to = 8},
    {.from = 7, .to = 10},
    {.from = 8, .to = 9},
    {.from = 8, .to = 11},
    {.from = 10, .to = 11},
}};

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

constexpr std::array<ActorData, 7> kActors{{
    {.id = 0,
     .role = Role::master,
     .name = kIlseVantner,
     .timeline = kVantnerTimeline},
    {.id = 1,
     .role = Role::mate,
     .name = kOsricBehn,
     .timeline = kBehnTimeline},
    {.id = 2,
     .role = Role::engineer,
     .name = kDunaKarr,
     .timeline = kKarrTimeline},
    {.id = 3,
     .role = Role::oiler,
     .name = kFenAchebe,
     .timeline = kAchebeTimeline},
    {.id = 4,
     .role = Role::medic,
     .name = kTobinReyes,
     .timeline = kReyesTimeline},
    {.id = 5,
     .role = Role::supercargo,
     .name = kMarisolQuint,
     .timeline = kQuintTimeline},
    {.id = 6,
     .role = Role::deckhand,
     .name = kYeoHalim,
     .timeline = kHalimTimeline},
}};

// Human-facing E01 maps to dense EvidenceId 0, through E22 -> 21.
constexpr EvidenceId E01 = 0;
constexpr EvidenceId E02 = 1;
constexpr EvidenceId E03 = 2;
constexpr EvidenceId E04 = 3;
constexpr EvidenceId E05 = 4;
constexpr EvidenceId E06 = 5;
constexpr EvidenceId E07 = 6;
constexpr EvidenceId E08 = 7;
constexpr EvidenceId E09 = 8;
constexpr EvidenceId E10 = 9;
constexpr EvidenceId E11 = 10;
constexpr EvidenceId E12 = 11;
constexpr EvidenceId E13 = 12;
constexpr EvidenceId E14 = 13;
constexpr EvidenceId E15 = 14;
constexpr EvidenceId E16 = 15;
constexpr EvidenceId E17 = 16;
constexpr EvidenceId E18 = 17;
constexpr EvidenceId E19 = 18;
constexpr EvidenceId E20 = 19;
constexpr EvidenceId E21 = 20;
constexpr EvidenceId E22 = 21;

constexpr InstrumentMask kSpectralLamp =
    world::instrument_mask(Instrument::spectral_lamp);
constexpr InstrumentMask kDecrypter =
    world::instrument_mask(Instrument::decrypter);
constexpr InstrumentMask kThermalTap =
    world::instrument_mask(Instrument::thermal_tap);
constexpr InstrumentMask kOssuaryTag =
    world::instrument_mask(Instrument::ossuary_tag);
constexpr InstrumentMask kPublishedLoadout =
    kSpectralLamp | kDecrypter | kOssuaryTag;

constexpr std::array<Fact, 1> kE01Facts{{
    {.actor = ACTOR_ANY, .when = 0, .where = 8, .what = Action::stow},
}};
constexpr std::array<Fact, 1> kE02Facts{{
    {.actor = 5, .when = TIME_ANY, .where = ROOM_ANY, .what = Action::stow},
}};
constexpr std::array<Fact, 1> kE03Facts{{
    {.actor = 3, .when = TIME_ANY, .where = 8, .what = Action::die},
}};
constexpr std::array<Fact, 1> kE04Facts{{
    {.actor = ACTOR_ANY,
     .when = TIME_ANY,
     .where = 8,
     .what = Action::fight_fire},
}};
constexpr std::array<Fact, 1> kE05Facts{{
    {.actor = 3, .when = TIME_ANY, .where = 11, .what = ACTION_ANY},
}};
constexpr std::array<Fact, 1> kE06Facts{{
    {.actor = ACTOR_ANY, .when = 3, .where = 7, .what = ACTION_ANY},
}};
constexpr std::array<Fact, 1> kE07Facts{{
    {.actor = 6, .when = TIME_ANY, .where = 7, .what = Action::seal},
}};
constexpr std::array<Fact, 1> kE08Facts{{
    {.actor = 6, .when = TIME_ANY, .where = 6, .what = ACTION_ANY},
}};
constexpr std::array<Fact, 1> kE09Facts{{
    {.actor = 0, .when = 5, .where = 0, .what = Action::vent},
}};
constexpr std::array<Fact, 1> kE10Facts{{
    {.actor = 1, .when = 4, .where = 1, .what = Action::broadcast},
}};
constexpr std::array<Fact, 1> kE12Facts{{
    {.actor = 2, .when = TIME_ANY, .where = 10, .what = Action::isolate},
}};
constexpr std::array<Fact, 1> kE13Facts{{
    {.actor = 2, .when = TIME_ANY, .where = 11, .what = ACTION_ANY},
}};
constexpr std::array<Fact, 1> kE14Facts{{
    {.actor = 4, .when = 6, .where = 4, .what = Action::treat},
}};
constexpr std::array<Fact, 1> kE16Facts{{
    {.actor = ACTOR_ANY, .when = 8, .where = 5, .what = Action::abandon},
}};
constexpr std::array<Fact, 1> kE17Facts{{
    {.actor = 5, .when = 8, .where = 5, .what = Action::abandon},
}};
constexpr std::array<Fact, 1> kE18Facts{{
    {.actor = 2, .when = TIME_ANY, .where = 8, .what = ACTION_ANY},
}};

constexpr std::array<EvidenceData, 16> kServedEvidence{{
    {.id = E01,
     .location = 8,
     .kind = EvidenceKind::cargo_seal,
     .asserts = kE01Facts,
     .veracity = Veracity::true_,
     .body = kE01Body},
    {.id = E02,
     .location = 7,
     .kind = EvidenceKind::manifest,
     .asserts = kE02Facts,
     .veracity = Veracity::true_,
     .body = kE02Body},
    {.id = E03,
     .location = 8,
     .kind = EvidenceKind::corpse,
     .asserts = kE03Facts,
     .veracity = Veracity::true_,
     .requires_ = kOssuaryTag,
     .body = kE03Body},
    {.id = E04,
     .location = 8,
     .kind = EvidenceKind::physical_trace,
     .asserts = kE04Facts,
     .veracity = Veracity::true_,
     .requires_ = kSpectralLamp,
     .body = kE04Body},
    {.id = E05,
     .location = 11,
     .kind = EvidenceKind::personal_effect,
     .asserts = kE05Facts,
     .veracity = Veracity::true_,
     .body = kE05Body},
    {.id = E06,
     .location = 7,
     .kind = EvidenceKind::damage_pattern,
     .asserts = kE06Facts,
     .veracity = Veracity::true_,
     .requires_ = kThermalTap,
     .body = kE06Body},
    {.id = E07,
     .location = 7,
     .kind = EvidenceKind::physical_trace,
     .asserts = kE07Facts,
     .veracity = Veracity::true_,
     .requires_ = kSpectralLamp,
     .body = kE07Body},
    {.id = E08,
     .location = 6,
     .kind = EvidenceKind::personal_effect,
     .asserts = kE08Facts,
     .veracity = Veracity::stale,
     .body = kE08Body},
    {.id = E09,
     .location = 0,
     .kind = EvidenceKind::log_fragment,
     .asserts = kE09Facts,
     .veracity = Veracity::true_,
     .requires_ = kDecrypter,
     .body = kE09Body},
    {.id = E10,
     .location = 1,
     .kind = EvidenceKind::log_fragment,
     .asserts = kE10Facts,
     .veracity = Veracity::true_,
     .requires_ = kDecrypter,
     .body = kE10Body},
    {.id = E12,
     .location = 10,
     .kind = EvidenceKind::physical_trace,
     .asserts = kE12Facts,
     .veracity = Veracity::true_,
     .requires_ = kSpectralLamp,
     .body = kE12Body},
    {.id = E13,
     .location = 11,
     .kind = EvidenceKind::personal_effect,
     .asserts = kE13Facts,
     .veracity = Veracity::stale,
     .body = kE13Body},
    {.id = E14,
     .location = 4,
     .kind = EvidenceKind::log_fragment,
     .asserts = kE14Facts,
     .veracity = Veracity::true_,
     .requires_ = kDecrypter,
     .body = kE14Body},
    {.id = E16,
     .location = 5,
     .kind = EvidenceKind::manifest,
     .asserts = kE16Facts,
     .veracity = Veracity::true_,
     .body = kE16Body},
    {.id = E17,
     .location = 5,
     .kind = EvidenceKind::personal_effect,
     .asserts = kE17Facts,
     .veracity = Veracity::true_,
     .body = kE17Body},
    {.id = E18,
     .location = 8,
     .kind = EvidenceKind::personal_effect,
     .asserts = kE18Facts,
     .veracity = Veracity::true_,
     .body = kE18Body},
}};

constexpr std::array<RedactedEvidenceData, 6> kRedactedEvidence{{
    {.id = E11,
     .item = "Master's order book, R00",
     .reason = "Redundant with E09, which is stronger and instrument-gated"},
    {.id = E15,
     .item = "Soot transfer on the medbay cot rail",
     .reason = "Redundant with E14 — \"patient: deckhand\" already identifies "
               "Halim"},
    {.id = E19,
     .item = "Aft boat cycle log, R09",
     .reason = "Pins a fact no commit needs; its absence leaves Karr's exit "
               "slightly mysterious, which is a feature"},
    {.id = E20,
     .item = "Second contraband ledger, R03",
     .reason = "Over-determines Quint; made commit 1 free"},
    {.id = E21,
     .item = "Reaction-origin burn pattern, R08",
     .reason = "Needs the thermal tap, not in the published loadout. One "
               "unreadable item teaches the lesson; two is tax"},
    {.id = E22,
     .item = "Behn's trace in comms",
     .reason = "Redundant with E10"},
}};

constexpr std::array<SolutionBatch, 3> kSolution{{
    {.entries =
         {{{.commit = {.actor = 5, .where = 8, .what = Action::stow},
            .reading = "The contraband went in under her chop."},
           {.commit = {.actor = 3, .where = 8, .what = Action::fight_fire},
            .reading = "Somebody stood and fought it."},
           {.commit = {.actor = 3, .where = 8, .what = Action::die},
            .reading = "He did not leave."}}}},
    {.entries =
         {{{.commit = {.actor = 6, .where = 7, .what = Action::seal},
            .reading = "Dogged from the forward side — with Achebe "
                       "behind it."},
           {.commit = {.actor = 0, .where = 0, .what = Action::vent},
            .reading = "The master vented Hold 1 to kill the fire."},
           {.commit = {.actor = 1, .where = 1, .what = Action::broadcast},
            .reading = "The mate sent the distress traffic."}}}},
    {.entries = {{{.commit = {.actor = 2, .where = 10, .what = Action::isolate},
                   .reading = "Why she was not in the hold."},
                  {.commit = {.actor = 4, .where = 4, .what = Action::treat},
                   .reading = "One patient, smoke inhalation, the deckhand."},
                  {.commit = {.actor = 5, .where = 5, .what = Action::abandon},
                   .reading =
                       "She left with the boat, and the second ledger."}}}},
}};

constexpr std::array<EvidenceId, 2> kChain01{{E01, E02}};
constexpr std::array<EvidenceId, 4> kChain02{{E04, E03, E05, E12}};
constexpr std::array<EvidenceId, 1> kChain03{{E03}};
constexpr std::array<EvidenceId, 1> kChain04{{E07}};
constexpr std::array<EvidenceId, 1> kChain05{{E09}};
constexpr std::array<EvidenceId, 1> kChain06{{E10}};
constexpr std::array<EvidenceId, 1> kChain07{{E12}};
constexpr std::array<EvidenceId, 1> kChain08{{E14}};
constexpr std::array<EvidenceId, 2> kChain09{{E17, E16}};

constexpr std::array<ChainWitness, 9> kChains{{
    {.solution_index = 0,
     .evidence = kChain01,
     .argument = "E01 places a stow of an undeclared seal in R08 at T0; E02 "
                 "makes Quint the only actor with stowage authority. Unique "
                 "intersection."},
    {.solution_index = 1,
     .evidence = kChain02,
     .argument = "E04 says someone fought the fire in R08. Two actors are "
                 "placed in R08 by evidence: Achebe (E03) and Karr (E18). E12 "
                 "places Karr in R10 doing isolate; E05 places Achebe's shift "
                 "in the adjacent R11. The firefighter is Achebe."},
    {.solution_index = 2,
     .evidence = kChain03,
     .argument = "Corpse in R08; the tag supplies identity and role. "
                 "Unreachable without the tag — which is why the tag is in "
                 "the published loadout."},
    {.solution_index = 3,
     .evidence = kChain04,
     .argument = "Palm salts on the dog-lever, one set, Halim. E08 (stale) "
                 "invites the wrong answer; E07 outranks it because it is an "
                 "instrument reading of the lever itself."},
    {.solution_index = 4,
     .evidence = kChain05,
     .argument = "Direct, decrypter-gated."},
    {.solution_index = 5,
     .evidence = kChain06,
     .argument = "Direct, decrypter-gated."},
    {.solution_index = 6,
     .evidence = kChain07,
     .argument = "Direct. Also the load-bearing step of chain 2 — one item, "
                 "two jobs, which is what good redaction leaves behind."},
    {.solution_index = 7,
     .evidence = kChain08,
     .argument = "\"One patient, deckhand\" — Halim is the only deckhand in "
                 "the roster the player already has."},
    {.solution_index = 8,
     .evidence = kChain09,
     .argument = "E17 places Quint's seal press in the forward airlock cycle; "
                 "E16's \"five out\" confirms an abandon, not a body."},
}};

constexpr CaseData kColdLantern{
    .name = "Cold Lantern",
    .room_count = 12,
    .actor_count = 7,
    .evidence_count = 22,
    .horizon = 9,
    .attention = 120,
    .links = kLinks,
    .ship_class = ShipClass::freighter,
    .cause = IncidentArchetype::contraband_reaction,
    .origin = 8,
    .published_loadout = kPublishedLoadout,
    .strings = kStrings,
    .rooms = kRooms,
    .actors = kActors,
    .served_evidence = kServedEvidence,
    .redacted_evidence = kRedactedEvidence,
    .solution = kSolution,
    .chains = kChains,
};

[[nodiscard]] constexpr auto strings_resolve() -> bool {
  for (const RoomData& room : kColdLantern.rooms) {
    if (text(kColdLantern, room.name).empty()) {
      return false;
    }
  }
  for (const ActorData& actor : kColdLantern.actors) {
    if (text(kColdLantern, actor.name).empty()) {
      return false;
    }
  }
  for (const EvidenceData& item : kColdLantern.served_evidence) {
    if (text(kColdLantern, item.body).empty()) {
      return false;
    }
  }
  return std::ranges::all_of(
      kColdLantern.redacted_evidence, [](const RedactedEvidenceData& item) {
        return !item.item.empty() && !item.reason.empty();
      });
}

static_assert(ids_are_dense(kColdLantern));
static_assert(strings_resolve());
static_assert(redaction_invariant(kColdLantern));
static_assert((kThermalTap & kPublishedLoadout) == 0U);

} // namespace

auto cold_lantern() noexcept -> const CaseData& {
  return kColdLantern;
}

} // namespace obscura::cases::authored
