#pragma once

// Dissolve — the transition between two fidelity levels of the same plate.
//
// What belongs here: the interpolation. A dissolve is a sequence of frames
// between an old plate and a new one, driven by an integer step count rather
// than elapsed time — the caller owns the clock, this owns the picture.
//
// Frames, not seconds, on purpose. The renderer is downstream of state, and a
// transition parameterised by wall-clock duration would make the frame you see
// depend on how busy the terminal was. A step index is reproducible, which
// means a replay renders identically to the run it recorded.

#include <cstdint>
#include <string>
#include <string_view>

#include <obscura/render/plates.hpp>

namespace obscura::render {

// How many steps a dissolve takes. Small and odd so the midpoint is a real
// frame rather than an average of two.
inline constexpr std::uint8_t kDissolveSteps = 5;

// Blends `from` into `to` at `step` of kDissolveSteps. step == 0 is `from`
// unchanged; step >= kDissolveSteps is `to` unchanged. Anything between is a
// per-character mix, so a plate that grows taller does so a line at a time
// rather than appearing whole.
[[nodiscard]] auto blend(const Plate& from, const Plate& to, std::uint8_t step) -> Plate;

// The single-line kernel the blend is built from, exposed because it is the
// part worth testing directly: everything above it is bookkeeping.
[[nodiscard]] auto blend_line(std::string_view from, std::string_view to, std::uint8_t step) -> std::string;

}  // namespace obscura::render
