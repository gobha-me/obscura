// The key binding table. See include/obscura/input/key_map.hpp for what belongs
// in this file.

#include <obscura/input/key_map.hpp>

#include <array>
#include <optional>
#include <span>
#include <string_view>

#include <termforge/core/types.hpp>

namespace obscura::input {

namespace {

constexpr auto character(char32_t ch, InputMode mode, Trigger trigger,
                         Intent intent, std::string_view display,
                         std::string_view help) -> Binding {
  return {.key = termforge::Key::Char,
          .ch = ch,
          .mode = mode,
          .trigger = trigger,
          .intent = intent,
          .display = display,
          .help = help};
}

constexpr auto named(termforge::Key key, InputMode mode, Trigger trigger,
                     Intent intent, std::string_view display,
                     std::string_view help) -> Binding {
  return {.key = key,
          .mode = mode,
          .trigger = trigger,
          .intent = intent,
          .display = display,
          .help = help};
}

// This is the whole keyboard contract. Mode-specific duplicates are explicit:
// they make ambiguity testable and keep help generation faithful to dispatch.
constexpr std::array<Binding, 28> kBindings{{
    character(U'h', InputMode::Ship, Trigger::PressOrRepeat, Intent::MoveLeft,
              "h", "move left"),
    named(termforge::Key::Left, InputMode::Ship, Trigger::PressOrRepeat,
          Intent::MoveLeft, "Left", "move left"),
    character(U'j', InputMode::Ship, Trigger::PressOrRepeat, Intent::MoveDown,
              "j", "move down"),
    named(termforge::Key::Down, InputMode::Ship, Trigger::PressOrRepeat,
          Intent::MoveDown, "Down", "move down"),
    character(U'k', InputMode::Ship, Trigger::PressOrRepeat, Intent::MoveUp,
              "k", "move up"),
    named(termforge::Key::Up, InputMode::Ship, Trigger::PressOrRepeat,
          Intent::MoveUp, "Up", "move up"),
    character(U'l', InputMode::Ship, Trigger::PressOrRepeat, Intent::MoveRight,
              "l", "move right"),
    named(termforge::Key::Right, InputMode::Ship, Trigger::PressOrRepeat,
          Intent::MoveRight, "Right", "move right"),

    character(U'h', InputMode::Aim, Trigger::PressOrRepeat,
              Intent::AimValuePrevious, "h", "previous value"),
    character(U'l', InputMode::Aim, Trigger::PressOrRepeat,
              Intent::AimValueNext, "l", "next value"),
    character(U'j', InputMode::Aim, Trigger::PressOrRepeat,
              Intent::AimFieldNext, "j", "next field"),
    character(U'k', InputMode::Aim, Trigger::PressOrRepeat,
              Intent::AimFieldPrevious, "k", "previous field"),

    named(termforge::Key::Enter, InputMode::Ship, Trigger::Press,
          Intent::Survey, "Enter", "survey compartment"),
    named(termforge::Key::Tab, InputMode::Ship, Trigger::Press,
          Intent::CycleEvidence, "Tab", "cycle evidence"),
    character(U'e', InputMode::Ship, Trigger::Press, Intent::Examine, "e",
              "examine evidence"),
    character(U' ', InputMode::Ship, Trigger::Press, Intent::ArmCommit, "Space",
              "open AIM"),
    character(U' ', InputMode::Ship, Trigger::Release, Intent::ReleaseCommit,
              "Space", "release AIM gesture"),
    character(U'1', InputMode::Ship, Trigger::Press, Intent::ToggleSlot1, "1",
              "select or clear slot 1"),
    character(U'2', InputMode::Ship, Trigger::Press, Intent::ToggleSlot2, "2",
              "select or clear slot 2"),
    character(U'3', InputMode::Ship, Trigger::Press, Intent::ToggleSlot3, "3",
              "select or clear slot 3"),
    character(U'R', InputMode::Ship, Trigger::Press, Intent::ResolveBatch, "R",
              "resolve batch"),
    character(U'L', InputMode::Ship, Trigger::Press, Intent::OpenLog, "L",
              "open log"),
    character(U'i', InputMode::Ship, Trigger::Press, Intent::OpenInstruments,
              "i", "open instruments"),
    character(U'?', InputMode::Ship, Trigger::Press, Intent::OpenManual, "?",
              "open manual"),
    character(U'q', InputMode::Ship, Trigger::Press, Intent::Quit, "q", "quit"),

    character(U' ', InputMode::Aim, Trigger::Release, Intent::ReleaseCommit,
              "Space", "commit pending triple"),
    named(termforge::Key::Escape, InputMode::Aim, Trigger::Press,
          Intent::Cancel, "Esc", "cancel AIM"),
    character(U'q', InputMode::Aim, Trigger::Press, Intent::Quit, "q", "quit"),
}};

constexpr auto trigger_matches(Trigger trigger, termforge::KeyAction action)
    -> bool {
  switch (trigger) {
    case Trigger::Press: return action == termforge::KeyAction::Press;
    case Trigger::PressOrRepeat:
      return action == termforge::KeyAction::Press ||
             action == termforge::KeyAction::Repeat;
    case Trigger::Release: return action == termforge::KeyAction::Release;
  }
  return false;
}

constexpr auto key_matches(const Binding& binding,
                           const termforge::KeyEvent& event) -> bool {
  if (binding.key == termforge::Key::Char) {
    return event.key == termforge::Key::Char && event.ch != U'\0' &&
           event.ch == binding.ch;
  }
  return event.key == binding.key && event.ch == U'\0' && !event.shift;
}

} // namespace

auto intent_for(const termforge::KeyEvent& event, InputMode mode)
    -> std::optional<Intent> {
  // Text modifiers are represented by the decoded character itself (for
  // example Shift+R is U'R'). Control and Alt chords are never commands.
  if (event.ctrl || event.alt) {
    return std::nullopt;
  }

  for (const Binding& binding : kBindings) {
    if (binding.mode == mode && key_matches(binding, event) &&
        trigger_matches(binding.trigger, event.action)) {
      return binding.intent;
    }
  }
  return std::nullopt;
}

auto bindings() -> std::span<const Binding> {
  return kBindings;
}

} // namespace obscura::input
