# AGENTS.md — conventions for AI agents working in this repo

If you're an LLM (or an LLM-driven editor) about to make changes here, read this
first. This is **OBSCURA**: a seeded deduction roguelike aboard a derelict, where
rendering fidelity IS the game state. Most of what follows is not style — it is
the set of invariants the premise depends on, and breaking one of them produces a
bug that passes every test until the day someone tries to replay a run.

## What this repo is

A terminal game, built on [TermForge](https://github.com/gobha-me/termforge) and
bootstrapped from the
cpp-template starter kit (same GitHub owner as this repo). It is an
*application*: there is no downstream consumer to keep happy, and the packaging
that survives from the template is there because it is cheap and tested, not
because anyone is expected to link `obscura::lib`.

The design in one paragraph: a case is fully determined from the first tick —
someone did something, somewhere, on some tick — and the run is the process of
spending a fixed attention budget to lift redaction off the evidence until
exactly one suspect survives elimination. Fidelity is therefore **state**, not a
display setting. A plate at `Sensed` is a different plate from the same item at
`Full`, not a dimmed one, because at that level the game genuinely does not know
what the item is.

## Invariants (these are the ones worth being careful about)

- **`src/world/` and `include/obscura/world/` are `[SIM]`.** No floating point,
  no wall clock, no ambient entropy, no hashed-container iteration order.
  Enforced structurally by `tools/lint/sim_purity.sh` (ctest: `sim-purity`),
  which scans both trees and explains what each ban protects. The headers count
  for the same reason the include allow-list exists: a banned construct declared
  in a public header reaches every translation unit that includes it without
  ever appearing in a `.cpp` under `src/world/`. If code in `world/` genuinely
  needs one of them, the answer is that the code does not belong in `world/`.
- **A run is `(seed, case index, intents)` and nothing else.** The world is a
  pure function of those three, which is what makes `src/replay/` an assertion
  rather than a feature. Do not add a fourth input — not a timestamp, not a
  config value read at startup, not a terminal dimension.
- **The direction of dependency is one-way.** `world/` depends on nothing else in
  this project. `core/`, `render/`, `input/`, `replay/` and `cases/` may read it;
  it may not read them. A `#include <obscura/render/...>` under `src/world/` is a
  design error, not a layering preference.
- **`render/` reads a projection, never ground truth.** `world::project` is the
  only way the screen learns anything. Wiring a renderer to the complete evidence
  set would let it draw something the run has not paid for — which in this game
  is not a cosmetic bug.
- **`core::App` routes; it does not decide.** It translates TermForge events into
  intents and asks `render/` to draw. Every rule lives one layer down. That is
  what lets the replay player drive a whole run headless, with no terminal and no
  App at all.
- **Transitions are parameterised by step index, not elapsed time.** A dissolve
  driven by a wall-clock duration renders differently on a busy terminal, so a
  replay would not reproduce the frames of the run it recorded.
- **Cases are compiled-in constexpr data** (`cases/`), appended never inserted: a
  `Recording` names its case by index into `cases::all()`, so inserting one in the
  middle silently re-points every recording made before it.

## Current baseline (keep these in sync if you change them)

- **CMake ≥ 3.28**, **C++23** (GCC 13+ / Clang 19+ — see the Clang note under
  "How to verify a change" for why 19, not 17).
- **TermForge**, pinned in `cmake/deps/termforge.cmake`. The consumed target is
  `termforge::lib`.
- **Catch2 v3** for tests, fetched via `FetchContent` (see `cmake/deps/`).
- **Compiler respects the environment** by default. Do **not** re-introduce a
  forced compiler in `cmake/toolchain/default.cmake`. Prefer clang? That's what
  `cmake/toolchain/clang.cmake` is for (opt-in, like the sanitizer toolchains).
- Dependencies: `find_package` first, `FetchContent` fallback, **100% CMake**
  (no conan/vcpkg). Keep it that way unless the maintainer asks.
- **Deps are opt-in via a list, not the filesystem.** A recipe in `cmake/deps/`
  is fetched only if its name is in `${PROJECT_NAME}_DEPS` in the root
  `CMakeLists.txt`. Dropping a file in `cmake/deps/` does **not** activate it;
  adding a dep means a recipe file **and** a line in that list. The two
  `list(REMOVE_ITEM …)` blocks under that list drop deps whose only consumer is a
  component that is switched off — they subtract from the list, never add.
  `${PROJECT_NAME}_BUILD_LIB` is declared in the **root** `CMakeLists.txt`, above
  that list, because the termforge gate reads it; moving it back into
  `src/lib/CMakeLists.txt` makes the gate read an undefined variable and quietly
  drops TermForge from a default build.
- **Library sources are globbed by area** in `src/lib/CMakeLists.txt`
  (`CONFIGURE_DEPENDS`), so a new file in `src/world/` is `[SIM]` by virtue of
  where it sits and needs no second registration. Re-run `cmake -B build` after
  adding one.
- **The library installs and exports — at install time only.**
  `cmake/install.cmake` generates the package config, the target export set and
  the header install; the exported target must keep spelling
  `${PROJECT_NAME}::lib`, identical to the in-tree `ALIAS`. **Do not add an
  `export(EXPORT …)` build-tree export.** The file explains at length why one was
  removed and why it cannot be guarded for; read it before reintroducing the
  feature.
- **A fetched dependency's install rules follow where it is linked**, not a fixed
  default. Private to the executable or the tests → `<DEP>_INSTALL OFF`. Linked
  into the library → `set(<DEP>_INSTALL ${${PROJECT_NAME}_INSTALL})`, plus a
  `find_dependency()` line in `cmake/project-config.cmake.in`. TermForge is the
  worked example of the second case, and both halves are present; a fixed `OFF`
  there breaks `install(EXPORT)`, and dropping the `find_dependency` produces a
  package whose targets are "referenced, but are missing". Visibility is *not*
  the test — a PRIVATE link into a static library reaches the exported target
  too.

## Conventions that matter here

- **Toolchains are opt-in files** in `cmake/toolchain/`: `default.cmake`
  (respects env), `clang.cmake`, `address.cmake`, `thread.cmake`,
  `undefined.cmake`. To add a configuration, add a file that `include()`s
  `default.cmake` and layers its flags — don't edit `default.cmake` to force a
  specific setup.
- **Library pattern**: a compiled `STATIC` lib (toggle
  `${PROJECT_NAME}_BUILD_LIB`), wired in `src/lib/CMakeLists.txt` but with its
  sources living in `src/<area>/` and `cases/`. Public API under
  `include/obscura/`, mirroring those areas. `src/lib/` holds the CMakeLists and
  nothing else, on purpose — the areas are the architecture, and a file's
  directory is what decides which rules apply to it.
- **Consumer-clean is a rule, not a nicety.** This project has to keep working
  when it is *not* the top-level one. Concretely:
  - Never `CMAKE_SOURCE_DIR` / `CMAKE_PROJECT_VERSION` / `CMAKE_PROJECT_NAME` —
    the `CMAKE_`-prefixed forms describe the top-level build, which belongs to
    someone else the moment we are consumed. Use `PROJECT_SOURCE_DIR`,
    `PROJECT_VERSION`, `PROJECT_NAME`.
  - A new `option()` defaults to `${PROJECT_IS_TOP_LEVEL}` unless it gates the
    library itself. Anything that builds an application, registers tests, or
    writes install rules is the consumer's business, not ours.
  - Public include directories are always
    `$<BUILD_INTERFACE:…>` / `$<INSTALL_INTERFACE:…>` genexes. A bare source
    path in a `PUBLIC` include directory makes `install(EXPORT)` fail at
    generate time — that failure is the feature, not the bug.
  - Anything the public header needs in order to compile (the C++ standard,
    a public dependency) travels on the target via `target_compile_features` /
    `PUBLIC` links. A consumer uses their own toolchain, so
    `cmake/toolchain/default.cmake` reaches them not at all.
  - Nothing in this repo exercises the consumed path automatically — the
    template's consumer harnesses were dropped, since OBSCURA is an application.
    If you touch the build's shape, prove it by hand: install to a scratch
    prefix and build a two-line project against
    `find_package(obscura CONFIG REQUIRED)`.
- **Tests are auto-discovered**: `test/CMakeLists.txt` loops over `test/*/`.
  A new test is just `test/<name>/test.cpp` (no CMakeLists needed); it gets
  `main()` from `test/main.cpp`, plus Catch2 and `${PROJECT_NAME}::lib` behind an
  `if (TARGET ...)` guard, so `-Dobscura_BUILD_LIB=OFF` still configures. Add a
  `CMakeLists.txt` in the dir only if the test needs custom build control — that
  dir then owns its own wiring, the library link included. Directory names sort
  the glob, which sets registration order, not execution order; use fixtures or
  `DEPENDS` when order actually matters. After adding a test dir, re-run
  `cmake -B`. A test that is a *script* rather than a Catch2 binary does not go
  through discovery at all — register it by hand at the top of
  `test/CMakeLists.txt`, as `sim-purity` and `version-parse-selftest` are. The
  loop looks for a `test.cpp`, so a directory holding only a script is skipped
  silently.

## Testing philosophy (the important one)

**Test how code fails, not just that it produces the right output.** A
happy-path assertion (`REQUIRE(fun(10 / 5) == 2)`) only proves the code returns
what you already knew it returned, on input chosen because it works. The
valuable tests are the adversarial ones — bad input, boundaries, overflow,
malformed external data, error paths. Write the **failure matrix first**; the
happy-path check is the last, least-interesting test (a smoke check that the
harness runs). See `test/20failure-testing/` for the canonical example.

This repo has a second kind of test, and it is worth knowing why. Determinism
cannot be tested for directly — a desync reproduces perfectly until the day it
does not — so `tools/lint/sim_purity.sh` (ctest: `sim-purity`) enforces it
structurally instead, by refusing the constructs that break it. When you change
that script, re-prove it the way the template proves its Class-B rules: drop a
banned construct into `src/world/`, watch the test go red, revert — and do the
same in `include/obscura/world/`, which the lint also scans and which fails
differently (the header reaches every includer, so a green `src/world/` proves
nothing about it). A lint that has rotted into one which matches nothing passes
forever and says nothing.

## How to verify a change (do this before opening a PR)

```bash
cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure
# and cross-compiler, because both are supported and CI enforces it:
cmake -B build-clang -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/clang.cmake \
  && cmake --build build-clang && ctest --test-dir build-clang

# and, for anything touching install/export or the dependency recipes, the path
# no ctest covers — every test runs with this repo as the top-level project.
# One compiler is enough: it fails during generation, if it fails.
cmake --install build --prefix /tmp/obscura-prefix
# then configure a scratch project with
#   find_package(obscura CONFIG REQUIRED)
#   target_link_libraries(app PRIVATE obscura::lib)
# against -DCMAKE_PREFIX_PATH=/tmp/obscura-prefix

# and the library-disabled configuration, which the above never takes:
cmake -B build-liboff -Dobscura_BUILD_LIB=OFF -Dobscura_TESTS=OFF \
  && cmake --build build-liboff --parallel
```

Both compilers must build clean and pass all tests. A change that only builds on
one compiler is not done.

CI (`.github/workflows/ci.yml`) enforces this on every push and pull request:
GCC and Clang × {default, address, thread, undefined} toolchains, plus the
`library disabled` and `version-parse-selftest` jobs.
A one-compiler change turns that compiler's jobs red — run the commands above
locally first.

CI pins its Clang jobs to Clang 20: Ubuntu 24.04's stock Clang 18 cannot compile
the C++23 `std::expected` example (`test/20failure-testing`) against libstdc++ —
use Clang 19+ (libstdc++) or any Clang with libc++. If you develop with Clang,
verify with a version CI would accept, not just whatever `clang++` resolves to.

**The same applies to GCC, and it is the easier one to get wrong**, because
`g++` on a dev box is usually *newer* than CI's, so a change can pass locally and
fail on the supported floor. CI runs GCC 13; `g++-13` is packaged alongside
`g++-14` on Ubuntu 24.04, so reach for `CXX=g++-13` before opening a PR that
touches language-level or usage-requirement behaviour. Worked example: C++23 mode
on GCC 13 reports `__cplusplus` as **202100L**, not 202302L (CMake selects
`-std=c++2b` there), so a `__cplusplus >= 202302L` check passes on GCC 14 and
Clang 20 and fails on the floor. Prefer a feature-test macro to a `__cplusplus`
comparison.

## Attribution

Follow the convention used across this org's repos: agent-authored commits
carry a trailer naming the model, e.g.

```
Co-authored-by: <model name> <noreply@example.invalid>
Agent: <harness> / <model>
```

and PRs note what was actually run to verify (per "How to verify" above).

## Notes for agents

- `include/version.hpp.in.cmake` is configured into `include/version.hpp` at
  build time; edit the `.in.cmake` source, not the generated file. If you touch
  it, keep the `#include <cstdint>` (std::uint32_t needs it).
- Version parsing is pure string logic in `cmake/version_parse.cmake`
  (`parse_git_describe`); `cmake/version.cmake` just runs `git describe` and calls
  it. If you change the parsing, add a row to and re-run the self-test:
  `cmake -P cmake/version_selftest.cmake` (also runs in ctest as
  `version-parse-selftest`). Failure-matrix-first, like the other tests.
- `cmake/check_artifacts.cmake` runs here in plain enforcement mode (ctest:
  `artifact-check`): its Class-A rules must all report zero hits, and its Class-B
  rules check wiring that can drift — every listed dependency has a recipe, no
  dependency is fetched but unused, the UBSan define matches on both sides,
  target-guarded test dirs still exist, and every tracked `*.sh` keeps mode
  100755. Run it on its own with `cmake -P cmake/check_artifacts.cmake`.
  **Never write one of the tokens it searches for into prose.** A rule counts
  hits across all tracked files, so a doc that quotes the token it is hunting
  keeps that rule green forever. The checker excludes itself from its own scan
  for exactly this reason; the fix anywhere else is to describe the token, not
  spell it.
- `tools/lint/sim_purity.sh` is the other standalone check, and the more
  important one. It is the only thing standing between the `[SIM]` tree
  (`src/world/` and `include/obscura/world/`) and a determinism bug nobody can
  reproduce.
- Build dirs (`build*/`) are gitignored — don't commit them.
- The dep pins in `cmake/deps/` are only audited when something breaks on a
  supported compiler; bump deliberately and say why in the commit.
