#pragma once

// Authored case data — the tutorial derelict.
//
// Content, not code. A case is a constexpr table so that it is checked by the
// compiler, shipped inside the binary, and available to the test suite without
// a fixture file or a loader. Add a case by adding a header like this one and
// naming it in cases/registry.cpp.
//
// The layout below is a ring of four compartments:
//
//     0 ── 1
//     │    │
//     3 ── 2
//
// A ring on purpose: every room has exactly two neighbours, so no actor's
// position is forced by topology alone and the deduction has to come from the
// evidence rather than from the map.

#include <array>

#include <obscura/cases/case_data.hpp>

namespace obscura::cases::authored {

inline constexpr std::array<Link, 4> kDerelictAlphaLinks{{
    {.from = 0, .to = 1},
    {.from = 1, .to = 2},
    {.from = 2, .to = 3},
    {.from = 3, .to = 0},
}};

inline constexpr CaseData kDerelictAlpha{
    .name = "derelict alpha",
    .room_count = 4,
    .actor_count = 3,
    .horizon = 16,
    .attention = 12,
    .links = kDerelictAlphaLinks,
};

} // namespace obscura::cases::authored
