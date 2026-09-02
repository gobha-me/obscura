// [SIM] Implementation of the reference deduction. See
// include/obscura/world/solver.hpp for what belongs in this file; see
// tools/lint/sim_purity.sh for what may not.

#include <obscura/world/solver.hpp>

#include <algorithm>

#include <obscura/world/actors.hpp>
#include <obscura/world/evidence.hpp>
#include <obscura/world/hull.hpp>
#include <obscura/world/incident.hpp>
#include <obscura/world/model.hpp>
#include <obscura/world/truth.hpp>

namespace obscura::world {

auto solve(const Hull& hull, const Roster& roster, const EvidenceSet& evidence)
    -> Solution {
  Solution result{};

  if (hull.room_count() == 0 || roster.size() == 0 || evidence.empty() ||
      evidence.front().asserts.empty()) {
    return result;
  }

  const Fact& anchor = evidence.front().asserts.front();

  for (const Actor& actor : roster.all()) {
    const Incident hypothesis{
        .culprit = actor.id,
        .scene = anchor.where,
        .when = anchor.when,
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
