// [SIM] Implementation of incident generation. See
// include/obscura/world/incident.hpp for what belongs in this file; see
// tools/lint/sim_purity.sh for what may not.

#include <obscura/world/incident.hpp>

#include <obscura/world/actors.hpp>
#include <obscura/world/hull.hpp>
#include <obscura/world/model.hpp>

namespace obscura::world {

auto is_well_formed(const Incident& incident, const Hull& hull,
                    const Roster& roster) -> bool {
  return incident.culprit < roster.size() && incident.scene < hull.room_count();
}

auto mix(Seed seed) -> Seed {
  // SplitMix64's finalizer. The constants are the published ones; they are not
  // interchangeable with "some large odd number" and should not be tuned.
  Seed value = seed + 0x9E3779B97F4A7C15ULL;
  value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31U);
}

auto generate(Seed seed, const Hull& hull, const Roster& roster, Tick horizon)
    -> Incident {
  if (roster.size() == 0 || hull.room_count() == 0 || horizon == 0) {
    return Incident{};
  }

  // Three independent draws off one seed, by mixing a distinct salt into each
  // rather than advancing shared state. Independent streams mean adding a
  // fourth decision later does not shift the first three, so an authored case
  // does not silently change meaning when the generator grows.
  const Seed culprit_draw = mix(seed ^ 0x0000'0000'0000'0001ULL);
  const Seed scene_draw = mix(seed ^ 0x0000'0000'0000'0002ULL);
  const Seed tick_draw = mix(seed ^ 0x0000'0000'0000'0003ULL);

  Incident incident{};
  incident.culprit = static_cast<ActorId>(culprit_draw % roster.size());
  incident.scene = static_cast<RoomId>(scene_draw % hull.room_count());
  incident.when = static_cast<Tick>(tick_draw % horizon);
  return incident;
}

} // namespace obscura::world
