// The session FSM's transition table. See include/obscura/core/session.hpp for
// what belongs in this file.

#include <obscura/core/session.hpp>

namespace obscura::core {

auto next_phase(Phase from, Signal signal) -> Phase {
  // Abort first, and outside the per-phase switch: it is legal from every
  // phase, including the terminal one where it is a no-op. Folding it into each
  // arm below is how a phase added later ends up silently un-abortable.
  if (signal == Signal::Abort) {
    return Phase::Closed;
  }

  switch (from) {
    case Phase::Boot:
      return signal == Signal::CaseLoaded ? Phase::Briefing : from;

    case Phase::Briefing: return signal == Signal::Begin ? Phase::Survey : from;

    case Phase::Survey:
      return signal == Signal::ArmCommit ? Phase::Accuse : from;

    case Phase::Accuse:
      if (signal == Signal::CancelCommit) {
        return Phase::Survey;
      }
      if (signal == Signal::Commit) {
        return Phase::Verdict;
      }
      return from;

    case Phase::Verdict:
      return signal == Signal::Dismiss ? Phase::Closed : from;

    case Phase::Closed: return Phase::Closed;
  }

  return from;
}

auto Session::dispatch(Signal signal) -> bool {
  const Phase to = next_phase(m_phase, signal);
  if (to == m_phase) {
    return false;
  }
  m_phase = to;
  return true;
}

} // namespace obscura::core
