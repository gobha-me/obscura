#pragma once

// [SIM] Canonical world vocabulary from the design specification, section 5.
//
// This header contains only types that are safe for a player-facing projection:
// identities, geometry, facts, actors, and commits. Ground-truth-only state is
// deliberately isolated in truth.hpp so render/ and input/ cannot learn whether
// evidence is correct merely by including the types they legitimately need.

#include <cstdint>
#include <vector>

namespace obscura::world {

// Every ID is a dense uint16_t index assigned in generation order. The all-ones
// values are outside that dense domain and are reserved for the wildcards used
// by partial evidence.
using RoomId = std::uint16_t;
using ActorId = std::uint16_t;
using EvidenceId = std::uint16_t;
using StringId = std::uint16_t;
using TimeStep = std::uint32_t;
using Tick = TimeStep;
using InstrumentMask = std::uint8_t;
using RoomIdList = std::vector<RoomId>;

inline constexpr RoomId ROOM_ANY = static_cast<RoomId>(0xFFFFU);
inline constexpr ActorId ACTOR_ANY = static_cast<ActorId>(0xFFFFU);
inline constexpr EvidenceId EVIDENCE_NONE = static_cast<EvidenceId>(0xFFFFU);
inline constexpr StringId STRING_NONE = static_cast<StringId>(0xFFFFU);
inline constexpr TimeStep TIME_ANY = static_cast<TimeStep>(0xFFFF'FFFFU);

// Compatibility spellings for the pre-M1 skeleton. They name the same
// sentinel, rather than preserving a second model of identity.
inline constexpr RoomId kNoRoom = ROOM_ANY;
inline constexpr ActorId kNoActor = ACTOR_ANY;

enum class Archetype : std::uint8_t {
  bridge,
  galley,
  berth,
  hold,
  engineering,
  medbay,
  airlock,
  comms,
};

enum class Damage : std::uint8_t {
  intact,
  damaged,
  breached,
};

enum class Resolution : std::uint8_t {
  unknown,
  surveyed,
  resolved,
};

enum class Action : std::uint8_t {
  move,
  stow,
  seal,
  isolate,
  vent,
  fight_fire,
  broadcast,
  treat,
  abandon,
  hide,
  die,
};

inline constexpr Action ACTION_ANY = static_cast<Action>(0xFFU);

enum class EvidenceKind : std::uint8_t {
  log_fragment,
  physical_trace,
  corpse,
  cargo_seal,
  manifest,
  damage_pattern,
  personal_effect,
};

// Section 5 references these two vocabularies; their closed values are given
// by section 6.2 and Appendix A of the same canonical document.
enum class Role : std::uint8_t {
  master,
  mate,
  engineer,
  oiler,
  medic,
  supercargo,
  deckhand,
};

enum class IncidentArchetype : std::uint8_t {
  hull_breach,
  fire,
  mutiny,
  contraband_reaction,
  contagion,
};

struct CellRect {
  std::uint16_t col{};
  std::uint16_t row{};
  std::uint16_t width{};
  std::uint16_t height{};
};

struct Compartment {
  RoomId id{ROOM_ANY};
  Archetype archetype{Archetype::bridge};
  Damage damage{Damage::intact};
  Resolution state{Resolution::unknown};
  CellRect bounds{};
  RoomIdList adjacent{};
};

struct Fact {
  ActorId actor{ACTOR_ANY};
  TimeStep when{TIME_ANY};
  RoomId where{ROOM_ANY};
  Action what{ACTION_ANY};
};

struct ActorStep {
  TimeStep when{};
  RoomId where{ROOM_ANY};
  Action what{Action::move};
};

struct Actor {
  ActorId id{ACTOR_ANY};
  Role role{Role::deckhand};
  StringId name{STRING_NONE};
  std::vector<ActorStep> timeline{};
};

struct Commit {
  ActorId actor{ACTOR_ANY};
  RoomId where{ROOM_ANY};
  Action what{ACTION_ANY};
};

} // namespace obscura::world
