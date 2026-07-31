#pragma once

// The player — replays a Recording and reports whether it reproduced.
//
// What belongs here: the loop that walks a recording's steps, and the verdict
// at the end. The player owns no game rules; it hands each Step to the same
// code a live run would, which is the only version of replay worth having. A
// player with its own copy of the rules tests itself.
//
// The result is a value, not a bool. "It diverged" is only useful with the step
// index attached — that index is the frame where determinism broke, and finding
// it by bisection is the alternative.

#include <cstddef>

#include <obscura/replay/recorder.hpp>
#include <obscura/replay/state_hash.hpp>

namespace obscura::replay {

struct Outcome {
  bool        reproduced{false};
  // Number of steps applied before the run ended. Equal to the recording's step
  // count on success.
  std::size_t steps_applied{0};
  Digest      expected{0};
  Digest      actual{0};
};

// Replays `recording`, driving a fresh world from its seed.
//
// An unsealed recording is refused rather than replayed: with no expected
// digest there is nothing to compare against, so a "successful" replay of one
// would be a test that cannot fail.
[[nodiscard]] auto replay(const Recording& recording) -> Outcome;

}  // namespace obscura::replay
