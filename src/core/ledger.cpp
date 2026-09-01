// The run ledger. See include/obscura/core/ledger.hpp for what belongs in this
// file.

#include <obscura/core/ledger.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>

namespace obscura::core {

Ledger::Ledger(std::uint32_t attention_budget) : m_remaining(attention_budget) {}

auto Ledger::append(Entry entry) -> void {
  m_entries.push_back(std::move(entry));
}

auto Ledger::spend(std::size_t subject, std::uint32_t cost) -> bool {
  // Checked before the subtraction, not after: m_remaining is unsigned, so an
  // overdraw would wrap to an enormous budget rather than going negative and
  // being caught.
  if (cost > m_remaining) {
    return false;
  }

  m_remaining -= cost;
  append(Entry{.kind = EntryKind::Spend, .subject = subject, .text = {}});
  return true;
}

}  // namespace obscura::core
