// Evidence plate composition. See include/obscura/render/plates.hpp for what
// belongs in this file.

#include <obscura/render/plates.hpp>

#include <obscura/world/evidence.hpp>
#include <obscura/world/redaction.hpp>

namespace obscura::render {

namespace {

auto kind_label(world::EvidenceKind kind) -> const char* {
  switch (kind) {
    case world::EvidenceKind::Presence: return "PRESENCE";
    case world::EvidenceKind::Absence: return "ABSENCE";
    case world::EvidenceKind::Trace: return "TRACE";
    case world::EvidenceKind::Testimony: return "TESTIMONY";
  }
  return "TRACE";
}

} // namespace

auto compose(const world::Evidence& item, world::Fidelity level) -> Plate {
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
      plate.lines.emplace_back("room ??");
      break;

    case world::Fidelity::Full:
      plate.lines.emplace_back(kind_label(item.kind));
      plate.lines.emplace_back(item.label);
      break;
  }

  return plate;
}

} // namespace obscura::render
