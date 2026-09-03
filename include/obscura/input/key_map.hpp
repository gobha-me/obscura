#pragma once

// The key map — keystrokes in, intents out.
//
// What belongs here: the single table that says which key means which Intent,
// and the lookup over it. One table, in one place, so that a rebind is a data
// change and a help screen can be generated from the same source the dispatcher
// reads. A `switch` scattered through the App is how those two drift apart.
//
// Intents, not actions. The map does not know what "Survey" does; it knows the
// player asked for it. That is what lets replay/ record a run as a list of
// intents — a recording of raw keystrokes would stop reproducing the moment a
// binding changed.

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <termforge/core/types.hpp>

namespace obscura::input {

enum class Intent : std::uint8_t {
  MoveUp,
  MoveDown,
  MoveLeft,
  MoveRight,
  Survey,
  CycleEvidence,
  Examine,
  ArmCommit,
  AimValuePrevious,
  AimValueNext,
  AimFieldPrevious,
  AimFieldNext,
  ReleaseCommit,
  Cancel,
  ToggleSlot1,
  ToggleSlot2,
  ToggleSlot3,
  ResolveBatch,
  OpenLog,
  OpenInstruments,
  OpenManual,
  Quit,
};

enum class InputMode : std::uint8_t { Ship, Aim };

enum class Trigger : std::uint8_t { Press, PressOrRepeat, Release };

// The bound keys, as data. Character bindings carry the decoded character from
// TermForge; named keys leave `ch` at zero. Display and help text live beside
// the dispatch data so a manual or legend cannot silently drift from it.
struct Binding {
  termforge::Key key{termforge::Key::Unknown};
  char32_t ch{};
  InputMode mode{InputMode::Ship};
  Trigger trigger{Trigger::Press};
  Intent intent{Intent::Cancel};
  std::string_view display{};
  std::string_view help{};
};

// The lookup. std::nullopt for an unbound key — an unbound key is ordinary
// (the terminal delivers plenty), not an error, so this must not be a
// throw-or-assert interface.
[[nodiscard]] auto intent_for(const termforge::KeyEvent& event, InputMode mode)
    -> std::optional<Intent>;

// The table itself, for help screens and for tests that want to assert the map
// is total and unambiguous.
[[nodiscard]] auto bindings() -> std::span<const Binding>;

} // namespace obscura::input
