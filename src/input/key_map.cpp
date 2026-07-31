// The key binding table. See include/obscura/input/key_map.hpp for what belongs
// in this file.

#include <obscura/input/key_map.hpp>

#include <algorithm>
#include <array>

namespace obscura::input {

namespace {

// Vi keys for movement, because the hands are already there, and single letters
// for the verbs. The table is the only place a binding is written down; a test
// can assert it has no duplicate keys, which a scattered switch could not offer.
constexpr std::array<Binding, 9> kBindings{{
    {'k', Intent::MoveUp,    "move up"},
    {'j', Intent::MoveDown,  "move down"},
    {'h', Intent::MoveLeft,  "move left"},
    {'l', Intent::MoveRight, "move right"},
    {'i', Intent::Inspect,   "spend attention on the selection"},
    {'m', Intent::Mark,      "mark the selection"},
    {'a', Intent::ArmCommit, "arm an accusation"},
    {'\x1b', Intent::Cancel, "cancel"},
    {'q', Intent::Quit,      "quit"},
}};

}  // namespace

auto intent_for(char key) -> std::optional<Intent> {
  const auto found = std::ranges::find(kBindings, key, &Binding::key);
  if (found == kBindings.end()) {
    return std::nullopt;
  }
  return found->intent;
}

auto bindings() -> std::span<const Binding> {
  return kBindings;
}

}  // namespace obscura::input
