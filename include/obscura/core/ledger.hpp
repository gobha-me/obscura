#pragma once

// The ledger: the player's own record of what they believe, and what it cost.
//
// What belongs here: the append-only log of a run — attention spent, fidelity
// raised, notes committed — and the queries the UI asks of it ("how much is
// left", "what did I already look at"). The ledger is the player's side of the
// world/redaction split: redaction.hpp says what is currently visible, the
// ledger says how it got that way.
//
// Append-only is not a style preference. replay/ reconstructs a run by feeding
// the same entries back in order, so an entry that can be edited after the fact
// is an entry the replay cannot reproduce.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace obscura::core {

enum class EntryKind : std::uint8_t {
  Spend,   // attention was spent on an evidence index
  Resolve, // an item's fidelity rose
  Note,    // the player wrote something down
  Accuse,  // an accusation was committed
};

struct Entry {
  EntryKind kind{EntryKind::Note};
  std::size_t subject{
      0}; // an evidence index for Spend/Resolve, an actor id for Accuse
  std::string text{};
};

class Ledger {
 public:
  // Attention is the run's budget: a whole number of looks, not a rate. An
  // integer because the whole game is a discrete-resource puzzle — a fractional
  // budget would make "can I afford this" a comparison with rounding in it.
  explicit Ledger(std::uint32_t attention_budget = 0);

  auto append(Entry entry) -> void;

  // Deducts `cost` and records a Spend. Returns false and records nothing when
  // the budget cannot cover it — a partial spend would leave the ledger and the
  // budget disagreeing, and the ledger is the one replay trusts.
  auto spend(std::size_t subject, std::uint32_t cost) -> bool;

  [[nodiscard]] auto remaining() const -> std::uint32_t { return m_remaining; }

  [[nodiscard]] auto entries() const -> const std::vector<Entry>& {
    return m_entries;
  }

  [[nodiscard]] auto size() const -> std::size_t { return m_entries.size(); }

 private:
  std::uint32_t m_remaining{0};
  std::vector<Entry> m_entries{};
};

} // namespace obscura::core
