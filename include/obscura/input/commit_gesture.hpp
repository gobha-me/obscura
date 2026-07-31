#pragma once

// The commit gesture — a small FSM standing between the player and an
// irreversible accusation.
//
// What belongs here: the states of the gesture and the transitions between
// them. An accusation ends the run, so it must not be reachable by one
// keystroke landing in the wrong frame; it takes an arm, a target selection and
// a confirm, and any stray input in between disarms.
//
// Deliberately its own FSM rather than three more phases in core::Session. The
// session tracks where a run *is*; this tracks a single intent being assembled,
// and the two change at completely different rates. Merging them is how the
// session's transition table ends up with a "half-armed" state in it.

#include <cstddef>
#include <cstdint>

namespace obscura::input {

enum class GestureState : std::uint8_t {
  Idle,      // nothing in progress
  Armed,     // the player asked to accuse; no target yet
  Targeted,  // a target is chosen; one confirm away
  Fired,     // the accusation was committed — terminal
};

class CommitGesture {
 public:
  CommitGesture() = default;

  [[nodiscard]] auto state() const -> GestureState { return m_state; }

  // The chosen target, meaningful only in Targeted and Fired.
  [[nodiscard]] auto target() const -> std::size_t { return m_target; }

  auto arm() -> bool;
  auto choose(std::size_t target) -> bool;
  auto confirm() -> bool;

  // Legal from any state and always succeeds, including from Fired — where it
  // resets rather than un-firing, because the run is over and the next one
  // starts clean. A cancel that could fail would need a caller to check it,
  // which is exactly the check that gets skipped.
  auto cancel() -> void;

 private:
  GestureState m_state{GestureState::Idle};
  std::size_t  m_target{0};
};

}  // namespace obscura::input
