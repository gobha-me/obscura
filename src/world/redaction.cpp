// [SIM] Implementation of the redaction mask and the projection it drives. See
// include/obscura/world/redaction.hpp for what belongs in this file; see
// tools/lint/sim_purity.sh for what may not.

#include <obscura/world/redaction.hpp>

#include <algorithm>
#include <cstddef>

#include <obscura/world/projection.hpp>
#include <obscura/world/truth.hpp>

namespace obscura::world {

RedactionMask::RedactionMask(std::size_t item_count)
    : m_levels(item_count, Fidelity::Hidden) {
}

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
  return std::ranges::all_of(
      m_levels, [](Fidelity level) { return level == Fidelity::Full; });
}

auto project(const EvidenceSet& complete, const RedactionMask& mask)
    -> EvidenceProjectionSet {
  EvidenceProjectionSet visible{};
  visible.reserve(complete.size());

  for (std::size_t index = 0; index < complete.size(); ++index) {
    const Fidelity level = mask.level_of(index);
    if (level == Fidelity::Hidden) {
      continue;
    }

    // Build up from a closed projection type. A field later added to Evidence
    // remains hidden until it is deliberately copied here.
    const Evidence& source = complete[index];
    EvidenceProjection item{.id = source.id};

    if (level >= Fidelity::Partial) {
      item.location = source.location;
      item.kind = source.kind;
      item.requires_ = source.requires_;
    }
    if (level >= Fidelity::Full) {
      item.asserts = source.asserts;
      item.body = source.body;
    }

    visible.push_back(item);
  }

  return visible;
}

} // namespace obscura::world
