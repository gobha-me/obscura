#pragma once

// The key map — keystrokes in, intents out.
//
// What belongs here: the single table that says which key means which Intent,
// and the lookup over it. One table, in one place, so that a rebind is a data
// change and a help screen can be generated from the same source the dispatcher
// reads. A `switch` scattered through the App is how those two drift apart.
//
// Intents, not actions. The map does not know what "Inspect" does; it knows the
// player asked for it. That is what lets replay/ record a run as a list of
// intents — a recording of raw keystrokes would stop reproducing the moment a
// binding changed.

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace obscura::input {

enum class Intent : std::uint8_t {
  MoveUp,
  MoveDown,
  MoveLeft,
  MoveRight,
  Inspect,   // spend attention on the selection
  Mark,      // flag the selection in the ledger
  ArmCommit, // begin the accusation gesture
  Cancel,
  Quit,
};

// The bound keys, as data. constexpr so the table is available at compile time
// to anything that wants to generate a key legend from it.
struct Binding {
  char             key{};
  Intent           intent{Intent::Cancel};
  std::string_view help{};
};

// The lookup. std::nullopt for an unbound key — an unbound key is ordinary
// (the terminal delivers plenty), not an error, so this must not be a
// throw-or-assert interface.
[[nodiscard]] auto intent_for(char key) -> std::optional<Intent>;

// The table itself, for help screens and for tests that want to assert the map
// is total and unambiguous.
[[nodiscard]] auto bindings() -> std::span<const Binding>;

}  // namespace obscura::input
