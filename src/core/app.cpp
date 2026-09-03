// The TermForge App subclass. See include/obscura/core/app.hpp for what belongs
// in this file.

#include <obscura/core/app.hpp>

#include <variant>

#include <termforge/core/requirements.hpp>
#include <termforge/core/screen.hpp>
#include <termforge/core/types.hpp>

#include <obscura/core/session.hpp>
#include <obscura/input/commit_gesture.hpp>
#include <obscura/input/key_map.hpp>

namespace obscura::core {

// Defined out of line rather than defaulted in the header: the header would
// otherwise need the complete definition of everything the base class holds by
// pointer, and the compiler would emit the vtable into every translation unit
// that includes it.
App::App() {
  set_keyboard_mode(termforge::KeyboardMode::Enhanced);
  require(termforge::AppRequirements{
      .key_press = true,
      .key_repeat = true,
      .key_release = true,
  });
  m_protocol_lost.on_close([this] {
    clear_overlays();
    m_session.dispatch(Signal::Abort);
    quit();
  });
}
App::~App() = default;

auto App::on_event(const termforge::Event& event) -> void {
  if (const auto* key = std::get_if<termforge::KeyEvent>(&event)) {
    // Keep TermForge's break-glass path available even after this run has been
    // blocked. Modal overlays deliberately route Ctrl+C back to the App.
    if (key->ctrl && (key->ch == U'c' || key->ch == U'C')) {
      termforge::App::on_event(event);
      return;
    }

    // TermForge retires held keys before surfacing its Warning. The effective
    // capability has already changed, so the synthetic Space release reaches
    // this guard and blocks instead of becoming a commit.
    if (!input_capabilities().key_release) {
      apply_gesture_effect(m_commit_gesture.protocol_lost());
      return;
    }

    const auto intent = input::intent_for(*key, m_commit_gesture.mode());
    if (!intent) {
      termforge::App::on_event(event);
      return;
    }
    if (*intent == input::Intent::Quit) {
      m_session.dispatch(Signal::Abort);
      quit();
      return;
    }
    apply_gesture_effect(m_commit_gesture.dispatch(*intent, true));
    return;
  }

  if (const auto* error = std::get_if<termforge::ErrorEvent>(&event);
      error != nullptr && error->source == "keyboard" &&
      error->severity != termforge::Severity::Info) {
    apply_gesture_effect(m_commit_gesture.protocol_lost());
    return;
  }

  termforge::App::on_event(event);
}

auto App::apply_gesture_effect(input::GestureEffect effect) -> void {
  switch (effect) {
    case input::GestureEffect::None: return;
    case input::GestureEffect::AimOpened:
      // Session owns phase legality. If it rejects an arm request, return the
      // input FSM to Ship mode so App cannot invent an Accuse phase around it.
      if (!m_session.dispatch(Signal::ArmCommit)) {
        (void)m_commit_gesture.dispatch(input::Intent::Cancel, true);
      }
      return;
    case input::GestureEffect::Committed:
      m_session.dispatch(Signal::Commit);
      return;
    case input::GestureEffect::Aborted:
      m_session.dispatch(Signal::CancelCommit);
      return;
    case input::GestureEffect::Blocked: show_protocol_lost(); return;
  }
}

auto App::show_protocol_lost() -> void {
  if (m_protocol_lost_visible) {
    return;
  }
  m_protocol_lost_visible = true;
  push_overlay(m_protocol_lost);
}

auto App::on_render(termforge::Screen& screen) -> void {
  // TermForge's loop does not clear the Screen between frames; each widget
  // repaints its own rect. Until there are widgets, owning the whole background
  // is the honest thing to do — a stale frame underneath would read as state
  // that is no longer true, which in this game is a lie rather than a smudge.
  screen.clear();
}

} // namespace obscura::core
