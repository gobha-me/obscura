#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <obscura/core/app.hpp>
#include <obscura/core/charge.hpp>
#include <obscura/core/session.hpp>
#include <obscura/input/commit_gesture.hpp>
#include <obscura/input/key_map.hpp>

#include <termforge/core/input.hpp>
#include <termforge/core/types.hpp>

namespace {

using obscura::input::CommitGesture;
using obscura::input::GestureEffect;
using obscura::input::GestureState;
using obscura::input::Intent;

class GestureHarness {
 public:
  auto feed(std::initializer_list<std::string_view> chunks) -> void {
    for (const auto chunk : chunks)
      m_input.feed(chunk);
    m_input.flush();
    for (const auto& event : m_input.poll()) {
      const auto* key = std::get_if<termforge::KeyEvent>(&event);
      if (key == nullptr) continue;
      const auto intent = obscura::input::intent_for(*key, m_gesture.mode());
      if (!intent) continue;
      const auto effect = m_gesture.dispatch(*intent, true);
      if (effect != GestureEffect::None) m_effects.push_back(effect);
    }
  }

  [[nodiscard]] auto gesture() noexcept -> CommitGesture& { return m_gesture; }
  [[nodiscard]] auto effects() const noexcept
      -> const std::vector<GestureEffect>& {
    return m_effects;
  }

 private:
  termforge::Input m_input;
  CommitGesture m_gesture;
  std::vector<GestureEffect> m_effects;
};

class AppProbe final : public obscura::core::App {
 public:
  auto start_offline() -> void {
    std::string sink;
    test_run_frames(0, 120, 40, &sink);
  }
  auto deliver(termforge::Event event) -> void { on_event(event); }
};

auto check_abort_is_free() -> void {
  CHECK(obscura::core::charge_delta(obscura::core::ChargeAction::Abort) == 0);
}

} // namespace

TEST_CASE("orphan Space release aborts without spending charge",
          "[input][gesture][failure]") {
  GestureHarness harness;
  harness.feed({"\033[32;1:3u"});

  REQUIRE(harness.effects().size() == 1);
  CHECK(harness.effects().front() == GestureEffect::Aborted);
  CHECK(harness.gesture().state() == GestureState::Idle);
  check_abort_is_free();
}

TEST_CASE("Escape during a hold aborts and the later release stays harmless",
          "[input][gesture][failure]") {
  GestureHarness harness;
  harness.feed({"\033[32;1", ":1u"});
  harness.feed({"\033[27;1:1u"});
  harness.feed({"\033[32;1:3u"});

  REQUIRE(harness.effects().size() == 3);
  CHECK(harness.effects()[0] == GestureEffect::AimOpened);
  CHECK(harness.effects()[1] == GestureEffect::Aborted);
  CHECK(harness.effects()[2] == GestureEffect::Aborted);
  CHECK(harness.gesture().state() == GestureState::Idle);
  check_abort_is_free();
}

TEST_CASE("missing release semantics blocks before a gesture can arm",
          "[input][gesture][failure]") {
  CommitGesture gesture;
  CHECK(gesture.dispatch(Intent::ArmCommit, false) == GestureEffect::Blocked);
  CHECK(gesture.state() == GestureState::Blocked);
  CHECK(gesture.dispatch(Intent::ReleaseCommit, true) == GestureEffect::None);
  CHECK(gesture.dispatch(Intent::Cancel, true) == GestureEffect::None);
  CHECK(gesture.state() == GestureState::Blocked);
}

TEST_CASE("protocol loss while held can never turn retirement into a commit",
          "[input][gesture][failure]") {
  GestureHarness harness;
  harness.feed({"\033[32;1:1u"});
  REQUIRE(harness.gesture().state() == GestureState::Aiming);

  // TermForge changes the effective capability before delivering its
  // synthetic release. Model that exact boundary: the release-shaped intent
  // arrives with release semantics already unavailable.
  CHECK(harness.gesture().dispatch(Intent::ReleaseCommit, false) ==
        GestureEffect::Blocked);
  harness.feed({"\033[32;1:3u"});

  REQUIRE(harness.effects().size() == 1);
  CHECK(harness.effects().front() == GestureEffect::AimOpened);
  CHECK(harness.gesture().state() == GestureState::Blocked);
  check_abort_is_free();
}

TEST_CASE(
    "App requires enhanced input and makes keyboard loss a terminal modal",
    "[app][input][gesture][failure]") {
  AppProbe app;
  app.start_offline();
  REQUIRE(app.running());
  CHECK(app.keyboard_mode() == termforge::KeyboardMode::Enhanced);
  CHECK(app.requirements().key_press);
  CHECK(app.requirements().key_repeat);
  CHECK(app.requirements().key_release);

  app.deliver(termforge::ErrorEvent{
      termforge::Severity::Warning, "keyboard",
      "keyboard protocol degraded: requested flags are no longer active"});
  CHECK(app.gesture_state() == GestureState::Blocked);
  CHECK(app.overlay_count() == 1);
  CHECK(app.running());

  // Repeated loss is one transition and one modal.
  app.deliver(termforge::ErrorEvent{
      termforge::Severity::Warning, "keyboard",
      "keyboard protocol degraded: requested flags are no longer active"});
  CHECK(app.overlay_count() == 1);

  // The only modal acknowledgement exits the run; it never resumes under a
  // later capability restoration.
  app.test_pump({"\033[13u"});
  CHECK_FALSE(app.running());
  CHECK(app.overlay_count() == 0);
  CHECK(app.session().phase() == obscura::core::Phase::Closed);
}

TEST_CASE("Space press then release is the sole commit path",
          "[input][gesture]") {
  GestureHarness harness;
  harness.feed({"\033[32;1:1u", "\033[32;1:2u", "\033[32;1:3u"});

  REQUIRE(harness.effects().size() == 2);
  CHECK(harness.effects()[0] == GestureEffect::AimOpened);
  CHECK(harness.effects()[1] == GestureEffect::Committed);
  CHECK(harness.gesture().state() == GestureState::Idle);
}
