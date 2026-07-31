// The commit gesture FSM. See include/obscura/input/commit_gesture.hpp for what
// belongs in this file.

#include <obscura/input/commit_gesture.hpp>

namespace obscura::input {

auto CommitGesture::arm() -> bool {
  if (m_state != GestureState::Idle) {
    return false;
  }
  m_state = GestureState::Armed;
  return true;
}

auto CommitGesture::choose(std::size_t target) -> bool {
  // Re-choosing while already Targeted is allowed: the player is scrolling
  // through suspects with the gesture up, which is the whole point of a
  // separate target step.
  if (m_state != GestureState::Armed && m_state != GestureState::Targeted) {
    return false;
  }
  m_target = target;
  m_state  = GestureState::Targeted;
  return true;
}

auto CommitGesture::confirm() -> bool {
  // Only from Targeted. Confirming out of Armed would let a double-tap of the
  // arm key fire an accusation at whatever the default target happened to be.
  if (m_state != GestureState::Targeted) {
    return false;
  }
  m_state = GestureState::Fired;
  return true;
}

auto CommitGesture::cancel() -> void {
  m_state  = GestureState::Idle;
  m_target = 0;
}

}  // namespace obscura::input
