# OBSCURA

[![CI](https://github.com/gobha-me/obscura/actions/workflows/ci.yml/badge.svg)](https://github.com/gobha-me/obscura/actions/workflows/ci.yml)

A seeded deduction roguelike aboard a derelict, where rendering fidelity IS the
game state.

You board a dead ship with a fixed budget of attention and a case that is
already fully determined: someone did something, somewhere, on some tick, and
the ship remembers all of it. What you do not have is permission to see it. Every
piece of evidence carries a fidelity level, and the run is the process of
spending attention to raise those levels until exactly one suspect survives the
elimination — or until you accuse the wrong one.

The premise is the architecture. Fidelity is not a display setting layered over
the truth; it is simulation state, recorded and replayed and hashed like
everything else. A plate at `Sensed` is not the `Full` plate greyed out — at that
level the game genuinely does not know what the item is, and drawing it dimmed
would imply information you have not paid for. That is the one design rule
everything else follows from.

What that buys, and what it costs:

* **A run is `(seed, case, intents)` and nothing else.** The world is a pure
  function of those three. `src/replay/` records them, plays them back against a
  fresh world, and compares a digest — so "it desynced" reports the step index
  where determinism broke, rather than the fact that the ending differed.
* **`src/world/` is `[SIM]` and is linted for it.** No floating point, no wall
  clock, no ambient entropy, no hashed-container iteration order. Enforced
  structurally by `tools/lint/sim_purity.sh`, which runs as the ctest case
  `sim-purity` — determinism bugs reproduce perfectly until the day they do not,
  so they are prevented rather than tested for. `tools/lint/sim_purity.sh`
  explains what each ban is protecting.
* **Cases are compiled-in constexpr data** (`cases/`), not files to be parsed.
  A case that can fail to load is a case that can fail to load in front of a
  player, and the reference solver (`src/world/solver.hpp`) can be run over every
  shipped case at test time with nothing to mock. A case is playable only when
  exactly one candidate survives the evidence.
* **The renderer is downstream of state, always.** `src/render/` reads a
  redaction projection, never the complete evidence set, so the screen cannot
  show something the run has not paid for. Transitions are parameterised by step
  index rather than elapsed time, so a replay renders identically to the run that
  recorded it.
* **The terminal is TermForge.** `src/core/` subclasses `termforge::App` and
  routes; it decides nothing. That is what lets the whole game be driven headless
  by the replay player, with no terminal attached.

Layout, and the direction of dependency between the pieces:

```
include/obscura/   public headers, mirroring the source areas below
src/world/         [SIM] hull, actors, incident, evidence, redaction, solver
src/core/          the TermForge App subclass, the session FSM, the ledger
src/render/        bands, plates, dissolve, log view
src/input/         key map, commit gesture FSM
src/audio/         sink interface, NullSink
src/replay/        recorder, player, state hashing
cases/             authored cases, as C++ constexpr data
tools/lint/        sim_purity.sh
```

`world/` depends on nothing else here. Everything else may read it; nothing else
may be read *by* it.

Built on the cpp-template starter kit (same GitHub owner as this repo):
CMake 3.28 minimum, C++23, GCC 13+ / Clang 19+, dependencies through
`find_package` with a `FetchContent` fallback and no package manager. The
build-system conventions inherited from it are documented below and in
`AGENTS.md`.

## Cheat sheet

**Configure, build, test — and picking a toolchain**

```bash
cmake -B build                                                            # $CXX, C++23, Debug
cmake -B build-clang -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/clang.cmake   # clang
CXX=clang++ cmake -B build-asan -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/address.cmake

cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run these from the repo root — the toolchain paths are relative. You cannot pass
two toolchain files, so a sanitizer composes with clang via `CXX=`, as above.

**Add a dependency**

```bash
cp cmake/deps/catch2.cmake cmake/deps/<name>.cmake   # catch2.cmake is the annotated recipe template
$EDITOR cmake/deps/<name>.cmake                      # find_package first, FetchContent fallback
$EDITOR CMakeLists.txt                               # add <name> to obscura_DEPS
cmake -B build
```

The recipe file alone does nothing — the list is the switch. A name on the list
with no recipe is a hard configure error; a recipe not on the list is inert. To
remove a dependency, delete its name from the list; the file can stay.

If the **library** links the new dependency — rather than the executable or the
tests — it needs two more lines, because it becomes part of what this project
exports: `set(<DEP>_INSTALL ${${PROJECT_NAME}_INSTALL})` in the recipe, and a
`find_dependency(<dep>)` in `cmake/project-config.cmake.in`. Without the first,
installing fails during generation; without the second, consumers get a package
whose targets refer to something they cannot find. A `PRIVATE` link counts —
visibility is not the test. The annotated recipe (`cmake/deps/catch2.cmake`)
explains both, and `cmake/deps/termforge.cmake` is this project's worked example
of the case: TermForge is linked into `obscura_lib`, so it needs both lines.

**Add a test**

```bash
mkdir test/40myfeature
$EDITOR test/40myfeature/test.cpp    # TEST_CASEs only — test/main.cpp provides main()
cmake -B build && cmake --build build --parallel && ctest --test-dir build
```

It becomes the target and ctest name `40myfeature-test`. **Re-run `cmake -B`
after adding a directory** — discovery is a configure-time glob. The numeric
prefix only sorts that glob; it does not order the run. The target links Catch2
and, when the library target exists, `obscura::lib` — so
`#include <obscura/obscura.hpp>` and call into the game directly, with nothing to
wire up. If a test needs custom build control, give it its own
`test/<dir>/CMakeLists.txt`: it inherits `TEST_NAME` and `SRCS` from the parent
scope, must define a target named exactly `${TEST_NAME}`, and must do its own
linking — the discovery loop's link lines do not reach it.

A test that is a *script* rather than a Catch2 binary is registered by hand at
the top of `test/CMakeLists.txt`, next to `sim-purity` and
`version-parse-selftest`. The discovery loop looks for a `test.cpp`, so a
directory holding only a shell script would be skipped silently.

**Run one test**

```bash
ctest --test-dir build -N                                              # list them
ctest --test-dir build -R 20failure-testing-test --output-on-failure   # one, plus its fixtures
ctest --test-dir build -R 20failure-testing-test -FS . -FC .           # one, fixtures skipped

./build/test/20failure-testing-test --list-tests                       # Catch2 cases within it
./build/test/20failure-testing-test "[failure]"                        # one case, or a tag
./build/test/20failure-testing-test -s                                 # show successful assertions too
```

`-R` is a regex match on the test name. Every discovered test carries
`FIXTURES_REQUIRED runners`, so ctest re-adds `startup` and `shutdown` even when
you filter — `-R` alone reports **three** tests, not one. `-FS . -FC .` excludes
the fixtures. Tests with their own `CMakeLists.txt` build into
`build/test/<dir>/`; the rest land in `build/test/`.

**Consume this project from another project**

OBSCURA is an application, not a library anyone is expected to depend on — there
is no `example/consumer/` harness here and no CI job proving downstream use. The
packaging inherited from the template is still real and still tested by the
install below, so if you do want to link `obscura::lib` (to drive a run headless,
say), all three acquisition modes work and spell the target identically:

```cmake
add_subdirectory(third_party/obscura)              # 1. vendored / submoduled

include(FetchContent)                              # 2. FetchContent
FetchContent_Declare(obscura
  GIT_REPOSITORY https://github.com/gobha-me/obscura.git
  GIT_TAG        v0.1.0
  SOURCE_DIR     ${FETCHCONTENT_BASE_DIR}/obscura  # ← see below
)
FetchContent_MakeAvailable(obscura)

find_package(obscura CONFIG REQUIRED)              # 3. installed

target_link_libraries(app PRIVATE obscura::lib)    # all three, unchanged
```

⚠ **Pin `SOURCE_DIR` in the FetchContent case.** This project takes its name from
its directory, and FetchContent checks out into `<base>/obscura-src` — so without
that line the project comes out named `obscura-src` and the target you have to
link is `obscura-src::lib`. This applies to any directory-named project, not just
this one.

You inherit the include directory, C++23, *and* TermForge as usage requirements
of the target; a consumer sets none of them. The TermForge half is why
`cmake/project-config.cmake.in` carries `find_dependency(termforge)` — a
consumer of an installed OBSCURA needs TermForge findable, and `find_package`
says so up front rather than failing at generate time.

**Install it**

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/opt/obscura
cmake --build build --parallel
cmake --install build
```

Installs the library, `include/obscura/*.hpp`, the executable, and a package
config at `<prefix>/lib/cmake/obscura/`. Build with `-Dobscura_BUILD_BIN=OFF
-Dobscura_TESTS=OFF` for a library-only install. TermForge is installed into the
same prefix — it is part of what this package exports, so leaving it out would
produce a package that cannot be used.

Three things worth knowing before you depend on it:

* The package exists **only in an install prefix**. Pointing
  `CMAKE_PREFIX_PATH` at a build directory finds the config file that was
  generated there and gets a directed refusal, not a package: this project
  exports its targets at install time only. For side-by-side development use
  `add_subdirectory` — same target name, no packaging in the way.
* The version in the package config comes from `git describe` at configure time.
  A build with no reachable tags reports `0.0.0`, and a consumer's
  `find_package(obscura 1.2.3 CONFIG REQUIRED)` is then refused — correctly,
  but the real cause is usually a shallow clone. Compatibility is
  `SameMajorVersion`.
* Public headers live under `include/obscura/` and install there, which keeps
  them out of a consumer's top-level include namespace. The generated
  `version.hpp` is the exception: it lands flat in `<prefix>/include` and
  declares *unprefixed* constants (`PROGRAM_NAME`, `VERSION_MAJOR`, …), so it can
  collide. It is internal — nothing in `include/obscura/` includes it.

**Cut a release tag**

```bash
git tag -a v1.2.3 -m "v1.2.3"
git push origin v1.2.3
cmake -B build          # the version is read at CONFIGURE time — re-configure or it is stale
cmake --build build --parallel
```

The format is enforced: optionally `v`- or `r`-prefixed, then exactly three
numeric components. `v1.2`, `v1.2.3.4` and `v1.2.3-rc1` are rejected by design
and fall back to `0.0.0` with a `STATUS` line naming the reason. Between tags,
`VERSION_TWEAK` counts commits since the tag and `VERSION_DIRTY` flags an unclean
tree; both land in `include/version.hpp`.

## Continuous integration

`.github/workflows/ci.yml` builds and tests on every push to `main` and every
pull request, enforcing the "both compilers, always" rule:

* **GCC and Clang** ×
* the **default** toolchain plus every sanitizer (**address**, **thread**,
  **undefined**) — 8 build/test jobs in all,
* a **library disabled** job, covering the `-Dobscura_BUILD_LIB=OFF` path the
  matrix never takes — it installs as well as builds, and asserts the prefix
  gets the executable and nothing else,
* plus a fast, dependency-free `version-parse-selftest` job.

A change that only builds on one compiler turns that compiler's jobs red, so a
one-sided break is visible on the PR.

The workflow hardcodes nothing project-specific — the project name is derived
from the checkout directory. **Keep `fetch-depth: 0`** or `git describe` stops
finding tags and every build reports version `0.0.0`, which also makes the
installed package refuse a versioned `find_package`.
