# CLAUDE.md — OBSCURA

Game repo conventions. The full spec is `docs/OBSCURA-design.html` (rev 2.0);
this file is the tactical version. If the two disagree, the spec wins and this
file is the bug.

> **Correction (2026-07-31).** The design bundle was authored against termforge
> **v0.1.7**. termforge is now at **v0.6.3** and three of the bundle's claims are
> stale — see "Upstream status" below before acting on any "termforge cannot do
> X" statement in `docs/`.
>
> **Correction (2026-08-04).** The pin is now **v0.7.1**, and **T-H4 (the byte
> meter) has landed**. The lesson of the 2026-07-31 block is that upstream moves
> faster than this file: check a tag before you trust a version number in it.

## Baseline
- **C++23**, **CMake >= 3.28**, GCC 13+ / **Clang 19+**. Both compilers, always.
  Clang 18 is not enough: `test/20failure-testing`'s `std::expected` canary
  cannot build against its libstdc++ pairing. CI installs a newer Clang for
  exactly this reason — do not "fix" that step by lowering the floor.
- **Catch2 v3** for tests. **No package manager** — `FetchContent` only.
- termforge consumed as `termforge::lib` at a pinned tag (**v0.7.1**), linked
  **`PUBLIC`** — `include/obscura/core/app.hpp` derives from `termforge::App`,
  so the usage requirement has to travel with the target.
  Never vendored, never patched locally. Missing library features are filed
  upstream as tickets and landed there first.
- Build options are `obscura_{BUILD_LIB,BUILD_BIN,TESTS,INSTALL}`. All but
  `BUILD_LIB` default to `PROJECT_IS_TOP_LEVEL`; the library defaults ON.
  `OBSCURA_AUDIO` arrives with the audio work in M1/M2 (ON in release, OFF in CI).
- Build dirs `build*/` are gitignored.
- Bootstrapped from the `cpp-template` CMake starter kit in this org.

## Hard rules
- **No floating-point in simulation state.** Integers, or Q16.16 with explicit
  rounding. Floats live in the render layer only, and the render layer never
  feeds back into simulation.
- **No wall-clock in simulation.** No `std::chrono`, no `random_device`, no
  `time()`. The tick is cosmetic: sim advances only on discrete input events.
- **No `unordered_*` iteration in any order-sensitive path.** `std::vector` or
  sorted flat maps with explicit comparators. IDs are dense integers assigned in
  generation order, never hash- or pointer-derived.
- **Named RNG streams only.** `rng(seed, stream_id)`. Never a global generator.
  Adding a subsystem must not perturb existing streams — old replays stay valid.
- **The UI layer cannot reach `Truth` or `Veracity`.** The AIM overlay's
  supporting/contradicting panel is computed from the player's own log by fact
  intersection. This firewall is tested. Breaking it silently ruins the game.
- **Fidelity is state, not style.** Whether a compartment draws as glyphs or as
  an image is a pure function of its `Resolution`. Never a render-side choice.
- **No audio-only information.** Audio is atmosphere. The determinism corpus runs
  with audio compiled out and must produce identical hashes.
- **No degraded mode.** Unmet capability requirements exit cleanly in cooked mode
  before alt-screen entry. Never fall back to timing heuristics for key release.
- **Never leave the terminal raw**, including on the exception path. termforge's
  `Terminal` is RAII; subclass resources go in the `on_start` / `on_stop` hooks.

## Layout
```
include/obscura/   public headers        src/core/      App subclass, session FSM, ledger
   world/   [SIM]  their public headers  src/render/    bands, plates, dissolve, log view
src/world/  [SIM]  hull, actors,         src/input/     key map, commit gesture FSM
                   incident, evidence,   src/audio/     sink iface, NullSink, RtAudioSink
                   redaction, solver     src/replay/    recorder, player, state hashing
cases/             authored cases as     test/corpus/   golden *.replay fixtures
                   C++ constexpr data
assets/plates/     baked art plates,     tools/lint/    sim_purity.sh
                   archetype x damage    tools/         venice-bake, replay-verify
```
`assets/plates/*.png` are committed and are the source of truth for the art.
They are compiled into the library as `constexpr` byte arrays by
`cmake/embed_asset.cmake` — nothing reads a file at runtime — and each one has a
size budget set at the call site in `src/lib/CMakeLists.txt`, enforced at
configure time. `tools/venice-bake/` is the recipe; `plate-bake-check` is the
CTest case that stops the two from drifting apart.
`[SIM]` is the determinism firewall, and it is **two** directories: the sources
in `src/world/` and the public headers they declare their types in,
`include/obscura/world/`. `tools/lint/sim_purity.sh` runs as a CTest case over
both and fails on `float`, `double`, `std::chrono`, `random_device`, `rand(`,
`time(`, `unordered_`, or any include outside the allow-list. The header tree is
not the lesser half — a `double` member or a `#include <chrono>` there poisons
every translation unit that includes it while `src/world/` stays clean. If the
lint is in your way, the code is wrong, not the lint.

## Testing philosophy (inherited from termforge)
Test how it fails, not just the happy path. The failures that matter here:
a dissolve interrupted by a resize or an eviction; a batch resolved from an
empty log; a replay recorded on one build verified against another; a redaction
that removed the last chain to a commit; a release event with no matching press.
Driver-facing tests are offline against an in-memory sink — no live TTY.

## Verify before every commit
```
cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure
cmake -B build-clang -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/clang.cmake \
  && cmake --build build-clang && ctest --test-dir build-clang
```
The determinism corpus must produce identical hashes across both, at -O0 and -O2,
and under ASan/UBSan. A divergence is undefined behaviour — chase it immediately.

Terminal-protocol behaviour also needs empirical verification on a real emulator.
You cannot see a terminal; ask the human to run the probe and report the bytes.

## Upstream status (verified 2026-08-04 against termforge v0.7.1)

You may open feature requests against termforge, and comment on open issues.
**Search for the *facility*, not the ticket name** — several T-series items
already existed under different titles.

| T | Facility | termforge issue | State |
|---|---|---|---|
| T-A1 | capability floor / refuse to start | #91 | open |
| T-A2 | virtual setup/teardown hooks | #97 | ✅ **landed** — `on_start` / `on_stop` |
| T-A3 | min grid size + pause-to-modal | #91 | floor half in #91; resize half raised as a comment |
| T-A4 | image cell rows/cols occupied | #100 | ✅ **landed** — `image_cell_extent()` |
| T-B1 | named layer API | #114 | open — **needs 5 planes, not the 4 proposed** |
| T-B2 | per-layer damage tracking | #142 | filed |
| T-C1 | rect block edits of a resident frame | #140 | filed |
| T-C2 | per-frame gaps, zero-gap bases | #116 | open — commented |
| T-C4 | compose N sources into one image | #141 | filed |
| T-D1 | image lifecycle across screen transitions | #113 | open — commented |
| T-D2 | quota accounting + eviction hooks | #112, #109 | open |
| T-D4 | shared-memory transfer path | #111 | open (optional, M3) |
| T-E1 | negotiate keyboard protocol | #60 | ✅ **landed** |
| T-E2 | press / repeat / release + modifiers | #60 | ✅ **landed** |
| T-E3 | protocol loss mid-session → `ErrorEvent` | — | unfiled |
| T-F1 | record/playback the event stream | #120 | open |
| T-F2 | injectable clock | #119, #118 | open |
| T-H4 | bytes-per-frame meter | #139 | ✅ **landed** — v0.6.8, `last_frame_bytes()` |
| T-H5 | cell pixel geometry query + change event | #143 | filed |

**T-E1/T-E2 landed.** `KeyAction{Press,Repeat,Release}`, `KeyEvent::action`,
`KeyboardMode{Legacy,Disambiguate,Enhanced}`, `Capabilities::kitty_keyboard`,
push-on-setup / pop-on-teardown including the crash path. Tested in
`test/04input` and `test/31keyboard`. The bundle says otherwise; the bundle is
stale. This retires the largest technical risk to the commit gesture.

**T-H4 landed** (#139, v0.6.8) — the ticket M0 flags "do this one first".
`FrameBytes{cells, image_transmit, image_edit}` with `.total()`, read off the
driver as `last_frame_bytes()` / `total_bytes()`. `App::driver()` is protected,
so an application reads its own frame cost from inside its own `App` subclass;
`test/10frame-bytes` is this repo's first consumer and shows the shape. Three
things to know before measuring anything:

- `test_run_frames` builds a **fresh driver on every call**, so a multi-frame
  measurement must be **one call with a frame count**, never N calls of one
  frame — otherwise the meter restarts while the sink keeps accumulating and a
  working meter looks broken.
- `cells` is the **remainder**, not a tallied quantity. The buckets sum to what
  the sink received by construction, so nothing goes uncounted — but a new emit
  path lands in `cells` until someone classifies it.
- **The 2 KB idle ceiling is not assertable yet.** Measured through our own
  `on_render`, an 80x24 full repaint costs **16,344 bytes** — the driver emits
  an absolute cursor address before every cell. Closing that needs T-B2 (#142)
  or run coalescing, neither of which is work this repo can do. Do not write the
  2 KB assertion before then; see `docs/08-determinism.md`. **`test/10frame-bytes`
  owns that number** — change it there, not in the four docs that point at it.

**A pre-encoded image path also landed** (#163, v0.6.9): `EncodedImage` +
`ImageFormat::Png`, plus `PlacementFit::Exact` (#137/#169). This retires the
"a 240x160 plate costs 205 KB against an 8 KB budget" risk in `docs/` — a plate
that size ships at ~5.3 KB. Ask `supports_image_format()` /
`supports_placement_fit()` before committing to an art set: both answer without
drawing, though only the format answer is stable — `supports_placement_fit()`
changes with `set_placement_mode()`.

Deliberately unfiled per the *no speculative tickets* rule, until their milestone
approaches: T-D3, T-D5, T-E3, T-F3, T-G1/G2/G3, T-H6.

## Filing library tickets
One landable change per ticket; body is Motivation / Proposed API / Acceptance /
Notes. Host-neutral bodies so the same text works for `gh` and `tea`. Never
propose a third-party dependency inside termforge's shipped library (it is
stdlib-only at runtime) and never propose a silent downgrade (degradation is an
event). Mirror every body into `docs/tickets/T-XX.md` — issue IDs are never the
source of truth.

## Do not
- Do not build a generator or solver before the M1 fun gate passes. This is the
  single most likely way the project dies.
- Do not add real-time combat, mouse-required interaction, synchronous
  multiplayer, sixel support, runtime asset generation, or any form of undo.
- Do not write `-1073741825` anywhere. Use the layer API (termforge#114).
- Do not tune balance by editing scattered constants: the economy is one
  `constexpr` table.
