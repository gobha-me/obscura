#pragma once

// Bands — the horizontal strips the screen is divided into.
//
// What belongs here: the layout arithmetic. A band is a named row range with a
// weight; the solver in src/render/bands.cpp distributes a terminal's rows
// across them and reports what each one got. No drawing happens here — a band
// is a rectangle and a name, and the widget that fills it is somebody else's
// concern.
//
// Integer rows throughout, and the remainder is distributed rather than
// rounded: a layout that depends on floating-point rounding produces a
// different frame at different terminal sizes for reasons nobody can reproduce.

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace obscura::render {

struct Band {
  std::string_view name{};
  // Relative share of the leftover rows. Zero means "exactly `min_rows`, no
  // more" — which is what a status line wants.
  std::uint16_t weight{1};
  std::uint16_t min_rows{1};
};

struct BandRect {
  std::string_view name{};
  std::uint16_t top{0};
  std::uint16_t rows{0};
};

// Lays `bands` out over `total_rows`, top to bottom.
//
// Guarantees, in this order of priority: every band gets at least min_rows if
// the terminal can afford it; the assigned rows sum to exactly total_rows when
// they can; leftover rows go to the highest weights, ties broken by declaration
// order. When the terminal is too short for the minimums, later bands are
// truncated to zero rows rather than every band being squeezed — a half-height
// log view is readable, a screen of one-row slivers is not.
[[nodiscard]] auto lay_out(const std::vector<Band>& bands,
                           std::uint16_t total_rows) -> std::vector<BandRect>;

} // namespace obscura::render
