#pragma once

// OBSCURA — umbrella header for the public API of ${PROJECT}::lib.
//
// Include this to get the whole surface, or reach for the individual headers
// under include/obscura/<area>/ when you only need one. The areas map 1:1 onto
// the source directories, and the direction of dependency between them is the
// one architectural rule this project has:
//
// clang-format off
//   world/   [SIM]  the deterministic simulation. Depends on NOTHING else here.
//   core/           session lifecycle, the ledger, the TermForge App subclass.
//   render/         turns simulation state into cells. Reads world/, never writes.
//   input/          keys and gestures in, intents out.
//   audio/          a sink interface and a do-nothing implementation.
//   replay/         records intents and replays them against a fresh world.
//   cases/          authored scenarios, as constexpr data.
// clang-format on
//
// The premise the layering protects: rendering fidelity IS the game state. What
// the player can see is a redaction applied to a fully determined world, so the
// world has to be reconstructible from a seed and a list of intents alone —
// which is what makes replay/ and the sim-purity lint
// (tools/lint/sim_purity.sh) worth having.

#include <obscura/audio/sink.hpp>
#include <obscura/cases/case_data.hpp>
#include <obscura/core/app.hpp>
#include <obscura/core/ledger.hpp>
#include <obscura/core/session.hpp>
#include <obscura/input/commit_gesture.hpp>
#include <obscura/input/key_map.hpp>
#include <obscura/render/bands.hpp>
#include <obscura/render/dissolve.hpp>
#include <obscura/render/log_view.hpp>
#include <obscura/render/plates.hpp>
#include <obscura/replay/player.hpp>
#include <obscura/replay/recorder.hpp>
#include <obscura/replay/state_hash.hpp>
#include <obscura/world/actors.hpp>
#include <obscura/world/evidence.hpp>
#include <obscura/world/hull.hpp>
#include <obscura/world/incident.hpp>
#include <obscura/world/redaction.hpp>
#include <obscura/world/solver.hpp>

namespace obscura {

// The library's own version, as a NUL-terminated string. Declared here and
// defined in src/core/version.cpp so that linking the library is proved by
// something other than a constexpr header — an all-header API would let a test
// pass with the archive missing entirely.
auto version_string() -> const char*;

} // namespace obscura
