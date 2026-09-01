// [SIM] Implementation of the reference deduction. See
// include/obscura/world/solver.hpp for what belongs in this file; see
// tools/lint/sim_purity.sh for what may not.

#include <obscura/world/solver.hpp>

#include <algorithm>

#include <obscura/world/actors.hpp>
#include <obscura/world/evidence.hpp>
#include <obscura/world/hull.hpp>
#include <obscura/world/incident.hpp>

namespace obscura::world {

auto solve(const Hull& hull, const Roster& roster, const EvidenceSet& evidence)
    -> Solution {
  Solution result{};

  if (hull.room_count() == 0 || roster.size() == 0) {
    return result;
  }

  // The scene and the tick come from the first Trace: a trace is the one kind
  // that asserts "something happened here, then" without naming anyone, so it
  // is what fixes the coordinates the candidates are tested against. Without
  // one there is nothing to deduce and the honest answer is "broken", not
  // "everyone".
  const auto trace = std::ranges::find_if(evidence, [](const Evidence& item) {
    return item.kind == EvidenceKind::Trace;
  });

  if (trace == evidence.end()) {
    return result;
  }

  for (const Actor& actor : roster.all()) {
    const Incident hypothesis{
        .culprit = actor.id,
        .scene = trace->where,
        .when = trace->when,
    };

    const bool survives =
        std::ranges::all_of(evidence, [&](const Evidence& item) {
          return is_consistent(item, roster, hypothesis);
        });

    if (survives) {
      result.candidates.push_back(actor.id);
    }
  }

  return result;
}

} // namespace obscura::world
