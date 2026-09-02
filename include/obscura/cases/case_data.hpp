#pragma once

// The shape of an authored case, and how a case becomes a world.
//
// What belongs here: the *type*. The data itself lives in cases/, as constexpr
// arrays — authored content compiled in rather than parsed at startup, because
// a case that can fail to load is a case that can fail to load in front of a
// player, and because the case solver can then be run over every shipped case
// at test time with nothing to mock.
//
// A CaseData is a description, not a world. build() turns one into a Hull,
// Roster and served evidence set deterministically: same case in, same world
// out, every time and on every platform. That is what lets a Recording name a
// case by index and a seed by value and nothing else.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <obscura/world/actors.hpp>
#include <obscura/world/hull.hpp>
#include <obscura/world/model.hpp>
#include <obscura/world/truth.hpp>

namespace obscura::cases {

// One connection in the hull. A flat pair rather than an adjacency list so the
// authored form stays readable as a table.
struct Link {
  world::RoomId from{world::kNoRoom};
  world::RoomId to{world::kNoRoom};
};

enum class ShipClass : std::uint8_t {
  tender,
  freighter,
  liner_hauler,
};

struct RoomData {
  world::RoomId id{world::ROOM_ANY};
  world::StringId name{world::STRING_NONE};
  world::Archetype archetype{world::Archetype::bridge};
  world::Damage damage{world::Damage::intact};
  world::CellRect bounds{};
};

struct ActorData {
  world::ActorId id{world::ACTOR_ANY};
  world::Role role{world::Role::deckhand};
  world::StringId name{world::STRING_NONE};
  std::span<const world::ActorStep> timeline{};
};

struct EvidenceData {
  world::EvidenceId id{world::EVIDENCE_NONE};
  world::RoomId location{world::ROOM_ANY};
  world::EvidenceKind kind{world::EvidenceKind::physical_trace};
  std::span<const world::Fact> asserts{};
  world::Veracity veracity{world::Veracity::true_};
  world::InstrumentMask requires_{};
  world::StringId body{world::STRING_NONE};
};

struct RedactedEvidenceData {
  world::EvidenceId id{world::EVIDENCE_NONE};
  std::string_view item{};
  std::string_view reason{};
};

struct SolutionEntry {
  world::Commit commit{};
  std::string_view reading{};
};

struct SolutionBatch {
  std::array<SolutionEntry, 3> entries{};
};

struct ChainWitness {
  std::size_t solution_index{};
  std::span<const world::EvidenceId> evidence{};
  std::string_view argument{};
};

struct CaseData {
  std::string_view name{};
  world::RoomId room_count{0};
  world::ActorId actor_count{0};
  world::EvidenceId evidence_count{0};
  // How many ticks the incident may fall within.
  world::Tick horizon{0};
  // The run's attention budget, in whole looks.
  std::uint32_t attention{0};
  std::span<const Link> links{};
  ShipClass ship_class{ShipClass::tender};
  world::IncidentArchetype cause{world::IncidentArchetype::fire};
  world::RoomId origin{world::ROOM_ANY};
  world::InstrumentMask published_loadout{};
  std::span<const std::string_view> strings{};
  std::span<const RoomData> rooms{};
  std::span<const ActorData> actors{};
  std::span<const EvidenceData> served_evidence{};
  std::span<const RedactedEvidenceData> redacted_evidence{};
  std::span<const SolutionBatch> solution{};
  std::span<const ChainWitness> chains{};
};

// The runtime values a case describes, built together because an actor's
// starting room and evidence locations are only meaningful against its hull.
struct World {
  world::Hull hull{};
  world::Roster roster{};
  world::IncidentArchetype cause{world::IncidentArchetype::fire};
  world::RoomId origin{world::ROOM_ANY};
  world::EvidenceSet served_evidence{};
};

// Pure. Legacy skeleton cases without room/actor descriptors retain their
// original defaults; authored cases materialize the complete metadata above.
[[nodiscard]] auto build(const CaseData& data) -> World;

// Empty for STRING_NONE or an out-of-range id. Authored content uses a stable,
// dense table so Actor::name and Evidence::body remain small deterministic IDs.
[[nodiscard]] constexpr auto text(const CaseData& data, world::StringId id)
    -> std::string_view {
  if (id == world::STRING_NONE || id >= data.strings.size()) {
    return {};
  }
  return data.strings[id];
}

namespace detail {

[[nodiscard]] constexpr auto evidence_id_count(const CaseData& data,
                                               world::EvidenceId id)
    -> std::size_t {
  std::size_t count = 0;
  for (const EvidenceData& item : data.served_evidence) {
    count += item.id == id ? 1U : 0U;
  }
  for (const RedactedEvidenceData& item : data.redacted_evidence) {
    count += item.id == id ? 1U : 0U;
  }
  return count;
}

[[nodiscard]] constexpr auto commit_exists(const CaseData& data,
                                           const world::Commit& commit)
    -> bool {
  for (const ActorData& actor : data.actors) {
    if (actor.id != commit.actor) {
      continue;
    }
    for (const world::ActorStep& step : actor.timeline) {
      if (step.where == commit.where && step.what == commit.what) {
        return true;
      }
    }
  }
  return false;
}

[[nodiscard]] constexpr auto find_evidence(const CaseData& data,
                                           world::EvidenceId id)
    -> const EvidenceData* {
  for (const EvidenceData& item : data.served_evidence) {
    if (item.id == id) {
      return &item;
    }
  }
  return nullptr;
}

[[nodiscard]] constexpr auto chain_is_valid(const CaseData& data,
                                            const ChainWitness& chain,
                                            std::size_t solution_count)
    -> bool {
  if (chain.solution_index >= solution_count || chain.evidence.empty() ||
      chain.argument.empty()) {
    return false;
  }
  return std::ranges::all_of(
      chain.evidence, [&data](const world::EvidenceId id) {
        const EvidenceData* found = find_evidence(data, id);
        return found != nullptr &&
               (found->requires_ & data.published_loadout) == found->requires_;
      });
}

[[nodiscard]] constexpr auto has_witness(const CaseData& data,
                                         std::size_t solution_index) -> bool {
  return std::ranges::any_of(data.chains,
                             [solution_index](const ChainWitness& chain) {
                               return chain.solution_index == solution_index;
                             });
}

} // namespace detail

// Dense identities are a replay contract, not merely a storage choice. The
// legacy skeleton has no descriptors; once a case supplies them, every id must
// occur exactly once in its generation-order domain.
[[nodiscard]] constexpr auto ids_are_dense(const CaseData& data) -> bool {
  if ((!data.rooms.empty() && data.rooms.size() != data.room_count) ||
      (!data.actors.empty() && data.actors.size() != data.actor_count) ||
      data.served_evidence.size() + data.redacted_evidence.size() !=
          data.evidence_count) {
    return false;
  }
  world::RoomId room_id = 0;
  for (const RoomData& room : data.rooms) {
    if (room.id != room_id) {
      return false;
    }
    ++room_id;
  }
  world::ActorId actor_id = 0;
  for (const ActorData& actor : data.actors) {
    if (actor.id != actor_id) {
      return false;
    }
    ++actor_id;
  }
  for (world::EvidenceId id = 0; id < data.evidence_count; ++id) {
    if (detail::evidence_id_count(data, id) != 1U) {
      return false;
    }
  }
  return true;
}

// Checks the authored proof witness without becoming the M3 solver. It proves
// that each solution commit exists in the actor timeline and has at least one
// non-empty chain made only of served evidence readable by the published
// loadout. The argument prose remains the human proof of why each chain pins
// its commit.
[[nodiscard]] constexpr auto redaction_invariant(const CaseData& data) -> bool {
  const std::size_t solution_count = data.solution.size() * 3U;
  if (solution_count == 0 || data.chains.empty()) {
    return false;
  }

  for (const ChainWitness& chain : data.chains) {
    if (!detail::chain_is_valid(data, chain, solution_count)) {
      return false;
    }
  }

  std::size_t solution_index = 0;
  for (const SolutionBatch& batch : data.solution) {
    for (const SolutionEntry& entry : batch.entries) {
      if (entry.reading.empty() || !detail::commit_exists(data, entry.commit) ||
          !detail::has_witness(data, solution_index)) {
        return false;
      }
      ++solution_index;
    }
  }

  return true;
}

// Every case that ships, in a stable order. The index into this span is what a
// Recording stores, so inserting a case in the middle invalidates old
// recordings — append.
[[nodiscard]] auto all() -> std::span<const CaseData>;

} // namespace obscura::cases
