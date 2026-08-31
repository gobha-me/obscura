#pragma once

// Baked art plates — the pixel art a compartment draws once its Resolution has
// been earned. One plate is one PNG: authored offline by tools/venice-bake/,
// committed under assets/plates/, and compiled into this library as a constexpr
// byte array. Nothing here opens a file, and nothing here generates an image.
//
// NOT `Plate`, and deliberately not in plates.hpp. obscura::render::Plate
// already exists there and means something else — the lines of TEXT one
// evidence item composes to. The two are the halves of "fidelity is state, not
// style": plates.hpp is what a compartment looks like when the run has not
// earned pixels, this is what it looks like when it has. Different bands
// (docs/10-tile-grammar.md: Layer::plate), so different names rather than one
// overloaded one.
//
// The accessors hand back a termforge::EncodedImage complete — payload and
// declared extent together, never separately. EncodedImage's own header is
// blunt about why: the library never parses a PNG, so the extent it is told is
// the source extent it ships as s=/v=. The compartment renderer deliberately
// scales that source into a fixed cell rect, but the terminal still needs the
// source dimensions to interpret the payload correctly. An API that returns
// the payload and extent together makes a stale declaration unspellable at a
// call site.
//
// One caveat on lifetime, because it inverts the usual contract: EncodedImage's
// `bytes` is documented as borrowed and valid only for the call it is passed
// to. These point at constexpr storage with no dynamic initialisation, so the
// returned value outlives every caller and may be held.

#include <termforge/core/types.hpp>

namespace obscura::render {

// The hold archetype at damage level 0 — one cell of the archetype x damage
// matrix that Layer::plate is made of.
//
// Interior art only: no compartment frame (Layer::hull draws that around it),
// no decals (Layer::overlay), and no room label, because one plate serves every
// hold on the ship.
//
// 240x160, PNG colour type 3, four inks over a transparent index. The extent is
// declared here and checked against the payload's own IHDR in test/11art-plate
// — stated twice on purpose, in two places that cannot both be edited by
// accident.
[[nodiscard]] auto hold_d0() -> termforge::EncodedImage;

}  // namespace obscura::render
