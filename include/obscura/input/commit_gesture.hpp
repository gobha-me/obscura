#pragma once

// The commit gesture — a small FSM standing between the player and an
// irreversible accusation.
//
// A gesture begins with Space press and commits only on the matching release.
// Escape and orphan releases abort. If release reporting disappears, Blocked
// is terminal for the run: silently changing the gesture to a timing heuristic
// would make both accidental commits and deterministic replay possible bugs.

#include <cstdint>

#include <obscura/input/key_map.hpp>

namespace obscura::input {

enum class GestureState : std::uint8_t {
  Idle,
  Aiming,
  Blocked,
};

enum class GestureEffect : std::uint8_t {
  None,
  AimOpened,
  Committed,
  Aborted,
  Blocked,
};

class CommitGesture {
 public:
  CommitGesture() = default;

  [[nodiscard]] auto state() const noexcept -> GestureState { return m_state; }
  [[nodiscard]] auto mode() const noexcept -> InputMode {
    return m_state == GestureState::Aiming ? InputMode::Aim : InputMode::Ship;
  }

  // Apply one intent under the input capability state that accompanied it.
  // The capability argument makes the fail-closed boundary explicit for
  // callers that receive a synthetic release while loss is being reported.
  auto dispatch(Intent intent, bool release_available) -> GestureEffect;

  // Report an asynchronous loss transition. Once blocked, no later intent or
  // capability restoration can resume this run.
  auto protocol_lost() -> GestureEffect;

 private:
  GestureState m_state{GestureState::Idle};
};

} // namespace obscura::input
