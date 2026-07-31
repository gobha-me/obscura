// [SIM] Implementation of the redaction mask and the projection it drives. See
// include/obscura/world/redaction.hpp for what belongs in this file; see
// tools/lint/sim_purity.sh for what may not.

#include <obscura/world/redaction.hpp>

#include <algorithm>

namespace obscura::world {

RedactionMask::RedactionMask(std::size_t item_count) : m_levels(item_count, Fidelity::Hidden) {}

auto RedactionMask::size() const -> std::size_t {
  return m_levels.size();
}

auto RedactionMask::level_of(std::size_t index) const -> Fidelity {
  if (index >= m_levels.size()) {
    return Fidelity::Hidden;
  }
  return m_levels[index];
}

auto RedactionMask::raise(std::size_t index, Fidelity to) -> bool {
  if (index >= m_levels.size() || to <= m_levels[index]) {
    return false;
  }
  m_levels[index] = to;
  return true;
}

auto RedactionMask::fully_resolved() const -> bool {
  // Vacuously true for an empty mask, which is the honest answer: there is
  // nothing left redacted.
  return std::ranges::all_of(m_levels, [](Fidelity level) { return level == Fidelity::Full; });
}

auto project(const EvidenceSet& complete, const RedactionMask& mask) -> EvidenceSet {
  EvidenceSet visible{};
  visible.reserve(complete.size());

  for (std::size_t index = 0; index < complete.size(); ++index) {
    const Fidelity level = mask.level_of(index);
    if (level == Fidelity::Hidden) {
      continue;
    }

    // Copy, then blank the fields this level does not cover. Copy-and-blank
    // rather than build-up, so that a field added to Evidence later is redacted
    // by default instead of leaking until someone remembers to handle it.
    Evidence item = complete[index];

    if (level < Fidelity::Full) {
      item.when = 0;
    }
    if (level < Fidelity::Partial) {
      item.kind    = EvidenceKind::Trace;
      item.where   = kNoRoom;
      item.subject = kNoActor;
      item.label.clear();
    }

    visible.push_back(std::move(item));
  }

  return visible;
}

}  // namespace obscura::world
