#pragma once

// The TermForge App subclass — OBSCURA's entry into the render loop.
//
// What belongs here: the wiring between TermForge's loop and this project's
// modules, and nothing that could live one layer down. Concretely, App is
// allowed to:
//
//   * hold the session FSM, the ledger, and the world for the current run,
//   * translate a TermForge Event into an input/ intent and hand it on,
//   * ask render/ to draw the current visible state into the Screen.
//
// It is NOT allowed to decide anything. Every rule — what an intent means, what
// is visible, what a fidelity level looks like — belongs to input/, world/ or
// render/ respectively. Keeping App a router is what lets replay/ drive a whole
// run with no terminal and no App at all.
//
// TermForge calls on_event -> on_tick -> on_render in that order every frame,
// on the loop thread. on_render must not mutate the world: it is the one place
// a frame drop would otherwise turn into a change in simulation state, which is
// the bug the whole [SIM] discipline exists to make impossible.

#include <termforge/core/app.hpp>
#include <termforge/core/screen.hpp>
#include <termforge/widgets/dialogs.hpp>

#include <obscura/core/ledger.hpp>
#include <obscura/core/session.hpp>
#include <obscura/input/commit_gesture.hpp>

namespace obscura::core {

class App : public termforge::App {
 public:
  App();
  ~App() override;

  App(const App&) = delete;
  auto operator=(const App&) -> App& = delete;
  App(App&&) = delete;
  auto operator=(App&&) -> App& = delete;

  [[nodiscard]] auto session() const -> const Session& { return m_session; }
  [[nodiscard]] auto ledger() const -> const Ledger& { return m_ledger; }
  [[nodiscard]] auto gesture_state() const noexcept -> input::GestureState {
    return m_commit_gesture.state();
  }
  [[nodiscard]] auto input_mode() const noexcept -> input::InputMode {
    return m_commit_gesture.mode();
  }

 protected:
  // Draw the current frame. Pure virtual in termforge::App, so this override is
  // the minimum that makes the class instantiable — everything else on the base
  // has a usable default.
  auto on_event(const termforge::Event& event) -> void override;
  auto on_render(termforge::Screen& screen) -> void override;

 private:
  auto apply_gesture_effect(input::GestureEffect effect) -> void;
  auto show_protocol_lost() -> void;

  Session m_session{};
  Ledger m_ledger{};
  input::CommitGesture m_commit_gesture{};
  termforge::MessageDialog m_protocol_lost{
      "INPUT PROTOCOL LOST",
      "Keyboard release reporting was lost. This run cannot continue safely.",
      "Exit run"};
  bool m_protocol_lost_visible{false};
};

} // namespace obscura::core
