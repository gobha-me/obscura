#pragma once

// The recorder — what a run writes down so it can be played back.
//
// What belongs here: the recording format and the append side of it. A
// recording is a seed plus an ordered list of intents, and nothing else. Not
// world snapshots, not rendered frames, not timestamps: the world is a pure
// function of (seed, intents), so anything more is both redundant and a second
// source of truth waiting to disagree with the first.
//
// The digest field is the exception, and it earns its place: it is not input to
// the replay, it is the assertion the replay checks itself against.

#include <cstddef>
#include <cstdint>
#include <vector>

#include <obscura/input/key_map.hpp>
#include <obscura/replay/state_hash.hpp>
#include <obscura/world/incident.hpp>

namespace obscura::replay {

struct Step {
  input::Intent intent{input::Intent::Cancel};
  // The selection the intent applied to. Recorded because the selection is
  // derived from screen layout, and layout depends on terminal size — replaying
  // "Inspect" without saying inspect *what* would reproduce differently in a
  // window of a different width.
  std::size_t   subject{0};
};

struct Recording {
  // Index into cases::all(). A case is authored, compiled-in data, so naming it
  // by index costs two bytes and cannot fail to resolve — unlike a path, which
  // is a way for a replay to break because a file moved.
  std::size_t       case_index{0};
  world::Seed       seed{0};
  std::vector<Step> steps{};
  // The expected digest at the end of the run. Zero means "not sealed" — a
  // recording still being written has no final state to promise.
  Digest            final_digest{0};
};

class Recorder {
 public:
  Recorder(std::size_t case_index, world::Seed seed);

  auto record(input::Intent intent, std::size_t subject) -> void;

  // Stamps the final digest and stops accepting steps. Sealing twice is a
  // no-op rather than an error: the run can end by accusation or by quitting,
  // and both paths seal.
  auto seal(Digest digest) -> void;

  [[nodiscard]] auto sealed() const -> bool { return m_sealed; }
  [[nodiscard]] auto recording() const -> const Recording& { return m_recording; }

 private:
  Recording m_recording{};
  bool      m_sealed{false};
};

}  // namespace obscura::replay
