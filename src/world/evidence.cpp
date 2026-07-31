// [SIM] Implementation of evidence derivation. See
// include/obscura/world/evidence.hpp for what belongs in this file; see
// tools/lint/sim_purity.sh for what may not.

#include <obscura/world/evidence.hpp>

namespace obscura::world {

auto derive(const Hull& hull, const Roster& roster, const Incident& incident) -> EvidenceSet {
  EvidenceSet items{};

  if (!is_well_formed(incident, hull, roster)) {
    return items;
  }

  // One trace at the scene, always. The skeleton derives exactly this much: it
  // is the minimum that makes the set non-empty for a well-formed incident, so
  // the solver has something to be uniquely-solvable *about* before the real
  // derivations land.
  items.push_back(Evidence{
      EvidenceKind::Trace,
      kNoActor,
      incident.scene,
      incident.when,
      "disturbance",
  });

  // Iterated in roster order, not by any associative container's ordering — the
  // sequence of this vector is part of what state hashing compares.
  for (const Actor& actor : roster.all()) {
    if (actor.id == incident.culprit) {
      continue;
    }
    items.push_back(Evidence{
        EvidenceKind::Absence,
        actor.id,
        incident.scene,
        incident.when,
        "elsewhere",
    });
  }

  return items;
}

auto is_consistent(const Evidence& item, const Roster& roster, const Incident& incident) -> bool {
  switch (item.kind) {
    case EvidenceKind::Presence:
      return item.subject == incident.culprit && item.where == incident.scene && item.when == incident.when;

    case EvidenceKind::Absence:
      return item.subject != incident.culprit || item.where != incident.scene || item.when != incident.when;

    case EvidenceKind::Trace:
      return item.where == incident.scene && item.when == incident.when;

    case EvidenceKind::Testimony:
      // Deliberately unchecked. An actor may lie, and the subject only has to
      // be someone who exists.
      return item.subject < roster.size();
  }

  return false;
}

}  // namespace obscura::world
