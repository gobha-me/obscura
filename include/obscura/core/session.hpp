#pragma once

// The session state machine: one run of OBSCURA, from boot to verdict.
//
// What belongs here: the phases a run passes through, the events that move it
// between them, and nothing else. The FSM is deliberately dumb — it does not
// own the world, the ledger or the renderer, it only says which of them is
// currently in charge. That separation is what lets replay/ drive a session
// through a recorded event list without a terminal attached.
//
// Adding a phase means adding it to the enum AND to the transition table in
// src/core/session.cpp. The switch there is exhaustive and unguarded by a
// default label, so the compiler names the omission instead of the run silently
// falling through to the current phase.

#include <cstdint>

namespace obscura::core {

enum class Phase : std::uint8_t {
  Boot,      // nothing loaded yet
  Briefing,  // the case is loaded; the player has not begun
  Survey,    // the interactive body of a run: spend attention, raise fidelity
  Accuse,    // the commit gesture has armed; a verdict is being composed
  Verdict,   // the accusation resolved, right or wrong
  Closed,    // terminal state; the run may be recorded but not continued
};

enum class Signal : std::uint8_t {
  CaseLoaded,
  Begin,
  ArmCommit,
  CancelCommit,
  Commit,
  Dismiss,
  Abort,  // legal from anywhere: a player quitting mid-run is not an error
};

// Pure transition function. Returns the phase a session in `from` reaches on
// `signal`, or `from` itself when the signal does not apply there. A no-op
// rather than an error because input arrives asynchronously — a keystroke that
// arrives one frame after the phase moved is ordinary, not a fault.
[[nodiscard]] auto next_phase(Phase from, Signal signal) -> Phase;

// The FSM proper: a phase plus the one operation that changes it. Kept a class
// rather than a bare enum so that the invariants (terminal states stay terminal)
// live in one place instead of at every call site.
class Session {
 public:
  Session() = default;

  [[nodiscard]] auto phase() const -> Phase { return m_phase; }

  // True when the phase actually changed.
  auto dispatch(Signal signal) -> bool;

  [[nodiscard]] auto is_closed() const -> bool { return m_phase == Phase::Closed; }

 private:
  Phase m_phase{Phase::Boot};
};

}  // namespace obscura::core
