// The replay player. See include/obscura/replay/player.hpp for what belongs in
// this file.

#include <obscura/replay/player.hpp>

#include <span>

#include <obscura/cases/case_data.hpp>
#include <obscura/replay/recorder.hpp>
#include <obscura/replay/state_hash.hpp>
#include <obscura/world/evidence.hpp>
#include <obscura/world/incident.hpp>

namespace obscura::replay {

auto replay(const Recording& recording) -> Outcome {
  Outcome outcome{};
  outcome.expected = recording.final_digest;

  // An unsealed recording has no promise to check, so replaying it would be a
  // test that cannot fail. Refused rather than run — see the header.
  if (recording.final_digest == 0) {
    return outcome;
  }

  const std::span<const cases::CaseData> catalogue = cases::all();
  if (recording.case_index >= catalogue.size()) {
    // A recording naming a case this build does not ship. Reported as a
    // divergence rather than thrown: it is a data mismatch, and the caller
    // already has to handle "did not reproduce".
    return outcome;
  }

  const cases::World world = cases::build(catalogue[recording.case_index]);
  const world::Incident truth =
      world::generate(recording.seed, world.hull, world.roster,
                      catalogue[recording.case_index].horizon);

  // The steps are counted, not yet interpreted: applying an Intent needs the
  // run rules, which do not exist yet. Counting them keeps the Outcome honest
  // about how far the replay got, and this loop is where the dispatch lands.
  for (const Step& step : recording.steps) {
    static_cast<void>(step);
    ++outcome.steps_applied;
  }

  // Hashing the evidence set rather than the incident alone: the incident is
  // three integers straight out of the seed, so a digest over it would pass
  // even if every derivation downstream had broken.
  outcome.actual = hash(world::derive(world.hull, world.roster, truth));
  outcome.reproduced = outcome.actual == outcome.expected;
  return outcome;
}

} // namespace obscura::replay
