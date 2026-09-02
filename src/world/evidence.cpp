// [SIM] Implementation of evidence derivation. See
// include/obscura/world/evidence.hpp for what belongs in this file; see
// tools/lint/sim_purity.sh for what may not.

#include <obscura/world/evidence.hpp>

#include <algorithm>

#include <obscura/world/actors.hpp>
#include <obscura/world/hull.hpp>
#include <obscura/world/incident.hpp>
#include <obscura/world/model.hpp>
#include <obscura/world/truth.hpp>

namespace obscura::world {

auto derive(const Hull& hull, const Roster& roster, const Incident& incident)
    -> EvidenceSet {
  EvidenceSet items{};

  if (!is_well_formed(incident, hull, roster)) {
    return items;
  }

  // The pre-Case-001 skeleton emits one positive fact. It is intentionally
  // smaller than the authored evidence model: issue #31 replaces this fixture
  // data, while this function keeps replay and hashing useful in the meantime.
  items.push_back(Evidence{
      .id = 0,
      .location = incident.scene,
      .kind = EvidenceKind::physical_trace,
      .asserts = {{.actor = incident.culprit,
                   .when = incident.when,
                   .where = incident.scene,
                   .what = ACTION_ANY}},
      .veracity = Veracity::true_,
      .requires_ = 0,
      .body = 0,
  });

  return items;
}

auto is_consistent(const Evidence& item, const Roster& roster,
                   const Incident& incident) -> bool {
  if (item.veracity != Veracity::true_) {
    // Stale and misleading evidence is part of the player's problem, not a
    // ground-truth constraint the reference solver may use to reject a case.
    return true;
  }

  return std::ranges::all_of(item.asserts, [&](const Fact& fact) {
    const bool actor_exists =
        fact.actor == ACTOR_ANY || fact.actor < roster.size();
    const bool actor_matches =
        fact.actor == ACTOR_ANY || fact.actor == incident.culprit;
    const bool room_matches =
        fact.where == ROOM_ANY || fact.where == incident.scene;
    const bool time_matches =
        fact.when == TIME_ANY || fact.when == incident.when;
    return actor_exists && actor_matches && room_matches && time_matches;
  });
}

} // namespace obscura::world
