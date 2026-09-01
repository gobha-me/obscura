// The replay recorder. See include/obscura/replay/recorder.hpp for what belongs
// in this file.

#include <obscura/replay/recorder.hpp>

#include <cstddef>

#include <obscura/input/key_map.hpp>
#include <obscura/replay/state_hash.hpp>
#include <obscura/world/incident.hpp>

namespace obscura::replay {

Recorder::Recorder(std::size_t case_index, world::Seed seed) {
  m_recording.case_index = case_index;
  m_recording.seed = seed;
}

auto Recorder::record(input::Intent intent, std::size_t subject) -> void {
  // Silently ignored after sealing rather than asserted: the last frame of a
  // run can deliver input after the verdict, and dying on it would turn an
  // ordinary race into a crash. The recording is already correct — it ends
  // where the run ended.
  if (m_sealed) {
    return;
  }
  m_recording.steps.push_back(Step{.intent = intent, .subject = subject});
}

auto Recorder::seal(Digest digest) -> void {
  if (m_sealed) {
    return;
  }
  m_recording.final_digest = digest;
  m_sealed = true;
}

} // namespace obscura::replay
