// The commit gesture FSM. See include/obscura/input/commit_gesture.hpp for what
// belongs in this file.

#include <obscura/input/commit_gesture.hpp>
#include <obscura/input/key_map.hpp>

namespace obscura::input {

auto CommitGesture::dispatch(Intent intent, bool release_available)
    -> GestureEffect {
  if (m_state == GestureState::Blocked) {
    return GestureEffect::None;
  }
  if (!release_available) {
    return protocol_lost();
  }

  switch (intent) {
    case Intent::ArmCommit:
      if (m_state != GestureState::Idle) {
        return GestureEffect::None;
      }
      m_state = GestureState::Aiming;
      return GestureEffect::AimOpened;

    case Intent::ReleaseCommit:
      if (m_state == GestureState::Aiming) {
        m_state = GestureState::Idle;
        return GestureEffect::Committed;
      }
      // An orphan release must be observable as an abort even though the
      // stable state is already Idle. It can never commit a default target.
      m_state = GestureState::Idle;
      return GestureEffect::Aborted;

    case Intent::Cancel:
      if (m_state != GestureState::Aiming) {
        return GestureEffect::None;
      }
      m_state = GestureState::Idle;
      return GestureEffect::Aborted;

    default: return GestureEffect::None;
  }
}

auto CommitGesture::protocol_lost() -> GestureEffect {
  if (m_state == GestureState::Blocked) {
    return GestureEffect::None;
  }
  m_state = GestureState::Blocked;
  return GestureEffect::Blocked;
}

} // namespace obscura::input
