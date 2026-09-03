#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include <obscura/input/key_map.hpp>
#include <obscura/replay/recorder.hpp>

#include <termforge/core/types.hpp>

namespace {

using obscura::input::Binding;
using obscura::input::InputMode;
using obscura::input::Intent;
using obscura::input::Trigger;
using termforge::Key;
using termforge::KeyAction;
using termforge::KeyEvent;

struct ExpectedBinding {
  Key key{Key::Unknown};
  char32_t ch{};
  InputMode mode{InputMode::Ship};
  KeyAction action{KeyAction::Press};
  Intent intent{Intent::Cancel};
};

constexpr std::array<ExpectedBinding, 28> kExpected{{
    {Key::Char, U'h', InputMode::Ship, KeyAction::Press, Intent::MoveLeft},
    {Key::Left, U'\0', InputMode::Ship, KeyAction::Press, Intent::MoveLeft},
    {Key::Char, U'j', InputMode::Ship, KeyAction::Press, Intent::MoveDown},
    {Key::Down, U'\0', InputMode::Ship, KeyAction::Press, Intent::MoveDown},
    {Key::Char, U'k', InputMode::Ship, KeyAction::Press, Intent::MoveUp},
    {Key::Up, U'\0', InputMode::Ship, KeyAction::Press, Intent::MoveUp},
    {Key::Char, U'l', InputMode::Ship, KeyAction::Press, Intent::MoveRight},
    {Key::Right, U'\0', InputMode::Ship, KeyAction::Press, Intent::MoveRight},
    {Key::Char, U'h', InputMode::Aim, KeyAction::Press,
     Intent::AimValuePrevious},
    {Key::Char, U'l', InputMode::Aim, KeyAction::Press, Intent::AimValueNext},
    {Key::Char, U'j', InputMode::Aim, KeyAction::Press, Intent::AimFieldNext},
    {Key::Char, U'k', InputMode::Aim, KeyAction::Press,
     Intent::AimFieldPrevious},
    {Key::Enter, U'\0', InputMode::Ship, KeyAction::Press, Intent::Survey},
    {Key::Tab, U'\0', InputMode::Ship, KeyAction::Press, Intent::CycleEvidence},
    {Key::Char, U'e', InputMode::Ship, KeyAction::Press, Intent::Examine},
    {Key::Char, U' ', InputMode::Ship, KeyAction::Press, Intent::ArmCommit},
    {Key::Char, U' ', InputMode::Ship, KeyAction::Release,
     Intent::ReleaseCommit},
    {Key::Char, U'1', InputMode::Ship, KeyAction::Press, Intent::ToggleSlot1},
    {Key::Char, U'2', InputMode::Ship, KeyAction::Press, Intent::ToggleSlot2},
    {Key::Char, U'3', InputMode::Ship, KeyAction::Press, Intent::ToggleSlot3},
    {Key::Char, U'R', InputMode::Ship, KeyAction::Press, Intent::ResolveBatch},
    {Key::Char, U'L', InputMode::Ship, KeyAction::Press, Intent::OpenLog},
    {Key::Char, U'i', InputMode::Ship, KeyAction::Press,
     Intent::OpenInstruments},
    {Key::Char, U'?', InputMode::Ship, KeyAction::Press, Intent::OpenManual},
    {Key::Char, U'q', InputMode::Ship, KeyAction::Press, Intent::Quit},
    {Key::Char, U' ', InputMode::Aim, KeyAction::Release,
     Intent::ReleaseCommit},
    {Key::Escape, U'\0', InputMode::Aim, KeyAction::Press, Intent::Cancel},
    {Key::Char, U'q', InputMode::Aim, KeyAction::Press, Intent::Quit},
}};

constexpr auto event_for(const ExpectedBinding& expected) -> KeyEvent {
  return {.key = expected.key,
          .ch = expected.ch,
          .shift =
              expected.ch == U'R' || expected.ch == U'L' || expected.ch == U'?',
          .action = expected.action};
}

constexpr auto event_for(const Binding& binding) -> KeyEvent {
  const KeyAction action = binding.trigger == Trigger::Release
                               ? KeyAction::Release
                               : KeyAction::Press;
  return {.key = binding.key, .ch = binding.ch, .action = action};
}

constexpr auto accepts(Trigger trigger, KeyAction action) -> bool {
  return trigger == Trigger::PressOrRepeat
             ? action == KeyAction::Press || action == KeyAction::Repeat
         : trigger == Trigger::Press ? action == KeyAction::Press
                                     : action == KeyAction::Release;
}

constexpr auto triggers_overlap(Trigger left, Trigger right) -> bool {
  return (accepts(left, KeyAction::Press) &&
          accepts(right, KeyAction::Press)) ||
         (accepts(left, KeyAction::Repeat) &&
          accepts(right, KeyAction::Repeat)) ||
         (accepts(left, KeyAction::Release) &&
          accepts(right, KeyAction::Release));
}

} // namespace

TEST_CASE("malformed and modified key events fail closed", "[input][failure]") {
  SECTION("Control and Alt never become commands") {
    for (const auto event :
         std::array{KeyEvent{.key = Key::Char, .ch = U'q', .ctrl = true},
                    KeyEvent{.key = Key::Char, .ch = U'R', .alt = true},
                    KeyEvent{.key = Key::Left, .ctrl = true}}) {
      CHECK_FALSE(obscura::input::intent_for(event, InputMode::Ship));
    }
  }

  SECTION("named keys reject text and Shift metadata") {
    CHECK_FALSE(obscura::input::intent_for({.key = Key::Enter, .ch = U'\r'},
                                           InputMode::Ship));
    CHECK_FALSE(obscura::input::intent_for({.key = Key::Left, .shift = true},
                                           InputMode::Ship));
  }

  SECTION("character keys require a decoded character") {
    CHECK_FALSE(
        obscura::input::intent_for({.key = Key::Char}, InputMode::Ship));
    CHECK_FALSE(obscura::input::intent_for({.key = Key::Unknown, .ch = U'h'},
                                           InputMode::Ship));
  }

  SECTION("unbound and wrong-mode commands stay unbound") {
    CHECK_FALSE(obscura::input::intent_for({.key = Key::Char, .ch = U'x'},
                                           InputMode::Ship));
    CHECK_FALSE(
        obscura::input::intent_for({.key = Key::Enter}, InputMode::Aim));
    CHECK_FALSE(obscura::input::intent_for({.key = Key::Char, .ch = U'e'},
                                           InputMode::Aim));
    CHECK_FALSE(
        obscura::input::intent_for({.key = Key::Escape}, InputMode::Ship));
  }
}

TEST_CASE("discrete repeats and irrelevant releases are ignored",
          "[input][failure]") {
  for (const auto event : std::array{
           KeyEvent{.key = Key::Enter, .action = KeyAction::Repeat},
           KeyEvent{.key = Key::Char, .ch = U'e', .action = KeyAction::Repeat},
           KeyEvent{.key = Key::Char,
                    .ch = U'R',
                    .shift = true,
                    .action = KeyAction::Repeat},
           KeyEvent{.key = Key::Char, .ch = U'q', .action = KeyAction::Repeat},
           KeyEvent{.key = Key::Tab, .action = KeyAction::Release},
           KeyEvent{
               .key = Key::Char, .ch = U'h', .action = KeyAction::Release}}) {
    CHECK_FALSE(obscura::input::intent_for(event, InputMode::Ship));
  }
}

TEST_CASE("the complete keyboard contract resolves to the specified intents",
          "[input]") {
  REQUIRE(obscura::input::bindings().size() == kExpected.size());

  for (const ExpectedBinding& expected : kExpected) {
    const std::optional<Intent> intent =
        obscura::input::intent_for(event_for(expected), expected.mode);
    REQUIRE(intent.has_value());
    CHECK(*intent == expected.intent);
  }

  CHECK(obscura::input::intent_for({.key = Key::Char,
                                    .ch = U'R',
                                    .shift = true,
                                    .action = KeyAction::Press},
                                   InputMode::Ship) == Intent::ResolveBatch);
  CHECK(obscura::input::intent_for({.key = Key::Char,
                                    .ch = U'L',
                                    .shift = true,
                                    .action = KeyAction::Press},
                                   InputMode::Ship) == Intent::OpenLog);
  CHECK(obscura::input::intent_for({.key = Key::Char,
                                    .ch = U'?',
                                    .shift = true,
                                    .action = KeyAction::Press},
                                   InputMode::Ship) == Intent::OpenManual);
}

TEST_CASE("only continuous controls accept repeat events", "[input]") {
  for (const Binding& binding : obscura::input::bindings()) {
    KeyEvent event = event_for(binding);
    event.action = KeyAction::Repeat;
    const auto actual = obscura::input::intent_for(event, binding.mode);
    if (binding.trigger == Trigger::PressOrRepeat) {
      REQUIRE(actual.has_value());
      CHECK(*actual == binding.intent);
    } else {
      CHECK_FALSE(actual);
    }
  }
}

TEST_CASE("the authoritative table is well formed and unambiguous",
          "[input][failure]") {
  const std::span<const Binding> all = obscura::input::bindings();
  for (std::size_t index = 0; index < all.size(); ++index) {
    const Binding& binding = all[index];
    CHECK(binding.key != Key::Unknown);
    CHECK((binding.key == Key::Char) == (binding.ch != U'\0'));
    CHECK_FALSE(binding.display.empty());
    CHECK_FALSE(binding.help.empty());

    for (std::size_t other = index + 1; other < all.size(); ++other) {
      const Binding& candidate = all[other];
      const bool same_key = binding.key == candidate.key &&
                            binding.ch == candidate.ch &&
                            binding.mode == candidate.mode;
      CHECK_FALSE(
          (same_key && triggers_overlap(binding.trigger, candidate.trigger)));
    }
  }
}

TEST_CASE("every binding produces a replayable intent-level step",
          "[input][replay]") {
  obscura::replay::Recorder recorder{3, 0xC01D'1A47U};
  std::size_t subject = 0;

  for (const Binding& binding : obscura::input::bindings()) {
    const auto intent =
        obscura::input::intent_for(event_for(binding), binding.mode);
    REQUIRE(intent.has_value());
    recorder.record(*intent, subject);
    ++subject;
  }

  const auto& steps = recorder.recording().steps;
  REQUIRE(steps.size() == obscura::input::bindings().size());
  for (std::size_t index = 0; index < steps.size(); ++index) {
    CHECK(steps[index].intent == obscura::input::bindings()[index].intent);
    CHECK(steps[index].subject == index);
  }
}

TEST_CASE("an orphan Space release remains visible in either mode",
          "[input][replay][failure]") {
  const KeyEvent release{
      .key = Key::Char, .ch = U' ', .action = KeyAction::Release};
  CHECK(obscura::input::intent_for(release, InputMode::Ship) ==
        Intent::ReleaseCommit);
  CHECK(obscura::input::intent_for(release, InputMode::Aim) ==
        Intent::ReleaseCommit);
}
