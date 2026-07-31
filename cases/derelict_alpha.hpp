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
    {0, 1},
    {1, 2},
    {2, 3},
    {3, 0},
}};

inline constexpr CaseData kDerelictAlpha{
    "derelict alpha",
    4,   // rooms
    3,   // actors
    16,  // horizon, in ticks
    12,  // attention budget
    kDerelictAlphaLinks,
};

}  // namespace obscura::cases::authored
