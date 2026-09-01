#pragma once

// Plates — one evidence item, drawn at the fidelity it has earned.
//
// What belongs here: the mapping from a world::Fidelity level to what the
// player actually sees. A plate is the atom of that mapping: given a projected
// evidence item and its level, produce the lines of text that represent it.
//
// This is where the premise becomes literal. A Sensed item is not "the same
// plate, greyed out" — it is a different plate, because at that level the game
// genuinely does not know what the item is. Rendering it dimmed would imply the
// information exists and is merely hard to read, which is the wrong lie.
//
// Nothing here touches the world. Plates read a projection (world::project) and
// never the complete evidence set; wiring one to the latter would let the
// screen show something the run has not paid for.

#include <string>
#include <vector>

#include <obscura/world/evidence.hpp>
#include <obscura/world/redaction.hpp>

namespace obscura::render {

struct Plate {
  // Rendered top to bottom. A vector rather than a fixed-height struct: the
  // plate for a Full item is taller than the plate for a Sensed one, and
  // padding to a common height is the band layout's job, not this one's.
  std::vector<std::string> lines{};
};

// Composes the plate for one item. `level` comes from the RedactionMask, not
// from the item — a projected item has already had its fields blanked, so it
// cannot report how much of itself is real.
[[nodiscard]] auto compose(const world::Evidence& item, world::Fidelity level)
    -> Plate;

} // namespace obscura::render
