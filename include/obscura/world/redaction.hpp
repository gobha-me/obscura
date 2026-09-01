#pragma once

// [SIM] What the player is allowed to see, and at what fidelity.
//
// This is the module the premise hangs on. The world is fully determined from
// the first tick; the game is the process of removing redaction from it. So
// fidelity is *state*, not a display setting — it is recorded, replayed and
// hashed like everything else here, and the renderer is downstream of it rather
// than in charge of it.
//
// What belongs here: the per-item fidelity level, the rules for raising it, and
// the projection that turns a complete EvidenceSet into the partial one the
// rest of the game may read. What does not: anything about glyphs, colour or
// dissolve timing. render/ decides how a fidelity level looks; this decides
// what it is.

#include <cstddef>
#include <cstdint>
#include <vector>

#include <obscura/world/evidence.hpp>

namespace obscura::world {

// Ordered on purpose: a level is comparable, and fidelity is monotonic within a
// run — it is raised by spending attention, never lowered by an accident of
// drawing order.
enum class Fidelity : std::uint8_t {
  Hidden = 0,  // the item is not known to exist
  Sensed = 1,  // it exists; kind and location are noise
  Partial = 2, // kind and room resolve; the tick does not
  Full = 3,    // the item reads as derived
};

// A fidelity level per item, parallel to the EvidenceSet it was built from.
// Parallel rather than embedded so that the complete set stays immutable ground
// truth and a run's visible state is a separate, serializable thing.
class RedactionMask {
 public:
  RedactionMask() = default;

  explicit RedactionMask(std::size_t item_count);

  [[nodiscard]] auto size() const -> std::size_t;

  [[nodiscard]] auto level_of(std::size_t index) const -> Fidelity;

  // Monotonic: a lower level than the one already reached is ignored rather
  // than applied. Returns true when the level actually moved.
  auto raise(std::size_t index, Fidelity to) -> bool;

  [[nodiscard]] auto fully_resolved() const -> bool;

 private:
  std::vector<Fidelity> m_levels{};
};

// The projection: the items the player may currently reason about, with fields
// the current fidelity does not cover reset to their sentinels. The result is a
// value, not a view — the caller must not be able to reach past it into ground
// truth by holding a reference.
[[nodiscard]] auto project(const EvidenceSet& complete,
                           const RedactionMask& mask) -> EvidenceSet;

} // namespace obscura::world
