// The run ledger. See include/obscura/core/ledger.hpp for what belongs in this
// file.

#include <obscura/core/ledger.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

#include <obscura/core/charge.hpp>

namespace obscura::core {

Ledger::Ledger(std::uint32_t attention_budget) : m_remaining(attention_budget) {
}

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
  append(Entry{.kind = EntryKind::Spend,
               .subject = subject,
               .text = {},
               .charge_delta = -static_cast<std::int32_t>(cost),
               .remaining = m_remaining});
  return true;
}

auto Ledger::read_evidence(std::size_t subject) -> EvidenceReadResult {
  const bool examined =
      std::ranges::any_of(m_entries, [subject](const Entry& entry) {
        return entry.kind == EntryKind::Examine && entry.subject == subject;
      });
  const ChargeAction action =
      examined ? ChargeAction::Reread : ChargeAction::Examine;
  const std::int32_t delta = charge_delta(action);
  const auto cost = static_cast<std::uint32_t>(-delta);
  if (cost > m_remaining) {
    return {.status = EvidenceReadStatus::insufficient_charge,
            .cost = cost,
            .remaining = m_remaining};
  }

  m_remaining -= cost;
  const EvidenceReadStatus status =
      examined ? EvidenceReadStatus::reread : EvidenceReadStatus::examined;
  append(Entry{.kind = examined ? EntryKind::Reread : EntryKind::Examine,
               .subject = subject,
               .text = {},
               .charge_delta = delta,
               .remaining = m_remaining});
  return {.status = status, .cost = cost, .remaining = m_remaining};
}

} // namespace obscura::core
