// Evidence plate composition. See include/obscura/render/plates.hpp for what
// belongs in this file.

#include <obscura/render/plates.hpp>

#include <string>

#include <obscura/world/model.hpp>
#include <obscura/world/projection.hpp>

namespace obscura::render {

namespace {

auto kind_label(world::EvidenceKind kind) -> const char* {
  switch (kind) {
    case world::EvidenceKind::log_fragment: return "LOG FRAGMENT";
    case world::EvidenceKind::physical_trace: return "PHYSICAL TRACE";
    case world::EvidenceKind::corpse: return "CORPSE";
    case world::EvidenceKind::cargo_seal: return "CARGO SEAL";
    case world::EvidenceKind::manifest: return "MANIFEST";
    case world::EvidenceKind::damage_pattern: return "DAMAGE PATTERN";
    case world::EvidenceKind::personal_effect: return "PERSONAL EFFECT";
  }
  return "EVIDENCE";
}

} // namespace

auto compose(const world::EvidenceProjection& item, world::Fidelity level)
    -> Plate {
  Plate plate{};

  switch (level) {
    case world::Fidelity::Hidden:
      // Nothing at all — not a placeholder. A box reading "[redacted]" tells
      // the player an item exists, which at this level is information they have
      // not earned.
      break;

    case world::Fidelity::Sensed: plate.lines.emplace_back("~~~~~~~~"); break;

    case world::Fidelity::Partial:
      plate.lines.emplace_back(kind_label(item.kind));
      plate.lines.emplace_back("room " + std::to_string(item.location));
      break;

    case world::Fidelity::Full:
      plate.lines.emplace_back(kind_label(item.kind));
      plate.lines.emplace_back("entry " + std::to_string(item.body));
      break;
  }

  return plate;
}

} // namespace obscura::render
