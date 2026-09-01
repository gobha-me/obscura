// The key binding table. See include/obscura/input/key_map.hpp for what belongs
// in this file.

#include <obscura/input/key_map.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <span>

namespace obscura::input {

namespace {

// Vi keys for movement, because the hands are already there, and single letters
// for the verbs. The table is the only place a binding is written down; a test
// can assert it has no duplicate keys, which a scattered switch could not
// offer.
constexpr std::array<Binding, 9> kBindings{{
    {.key = 'k', .intent = Intent::MoveUp, .help = "move up"},
    {.key = 'j', .intent = Intent::MoveDown, .help = "move down"},
    {.key = 'h', .intent = Intent::MoveLeft, .help = "move left"},
    {.key = 'l', .intent = Intent::MoveRight, .help = "move right"},
    {.key = 'i',
     .intent = Intent::Inspect,
     .help = "spend attention on the selection"},
    {.key = 'm', .intent = Intent::Mark, .help = "mark the selection"},
    {.key = 'a', .intent = Intent::ArmCommit, .help = "arm an accusation"},
    {.key = '\x1b', .intent = Intent::Cancel, .help = "cancel"},
    {.key = 'q', .intent = Intent::Quit, .help = "quit"},
}};

} // namespace

auto intent_for(char key) -> std::optional<Intent> {
  const auto* const found = std::ranges::find(kBindings, key, &Binding::key);
  if (found == kBindings.end()) {
    return std::nullopt;
  }
  return found->intent;
}

auto bindings() -> std::span<const Binding> {
  return kBindings;
}

} // namespace obscura::input
