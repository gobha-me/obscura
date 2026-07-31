#!/bin/bash
#
# sim-purity — the [SIM] discipline, enforced.
#
# src/world/ is the simulation. A run of OBSCURA has to be reconstructible from
# a seed and a list of intents alone, or replay is a lie and the case solver is
# checking a world the player never saw. That property cannot be tested for
# directly — a determinism bug reproduces perfectly until the day it does not —
# so it is enforced structurally instead: the simulation may not name the
# constructs that break it.
#
# What is banned, and why each one:
#
#   float, double     Binary floating point is deterministic per-machine but not
#                     across build flags: -ffast-math, x87 excess precision and
#                     FMA contraction all change results, and none of them is
#                     visible in the source. Integers are exact everywhere.
#   std::chrono       The wall clock is the definitive non-reproducible input.
#                     Simulation advances in ticks, which the caller supplies.
#   random_device     Ambient entropy. The seed is the only source of randomness
#                     the simulation is allowed, and it arrives as a parameter.
#   rand(             Same, plus hidden global state shared with everything else
#                     in the process.
#   time(             Same as std::chrono, by the older spelling.
#   unordered_        Hashed containers have implementation-defined iteration
#                     order. Iterate one to break a tie and the run replays
#                     differently under a different standard library — on the
#                     same machine, from the same seed.
#
# Registered as the ctest case `sim-purity` (see test/CMakeLists.txt), so a
# violation fails the suite rather than waiting for review.
#
# Usage: tools/lint/sim_purity.sh [directory]
#   Scans src/world/ by default. Exit 0 clean, exit 1 on any violation.
#
# Keep the executable bit (mode 100755): ctest runs this by path with no
# interpreter, and cmake/check_artifacts.cmake rule B5 enforces it.

set -euo pipefail

here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${here}/../.." && pwd)"
target="${1:-${repo_root}/src/world}"

if [ ! -d "${target}" ]; then
  echo "sim-purity: ${target} does not exist or is not a directory" >&2
  exit 1
fi

# Word boundaries on the identifier patterns, so "floating-point" in a comment
# and a function called brand() are not violations. `rand(` and `time(` allow
# whitespace before the paren, because a formatter may insert it.
patterns=(
  '\<float\>'
  '\<double\>'
  'std::chrono'
  'random_device'
  '\<rand[[:space:]]*\('
  '\<time[[:space:]]*\('
  'unordered_'
)

status=0
found=""

for pattern in "${patterns[@]}"; do
  # -r over the directory rather than a glob, so a new subdirectory is covered
  # the moment it exists. || true because grep exits 1 on "no matches", which is
  # the outcome we want and `set -e` would treat as a failure.
  hits="$(grep -rnE --include='*.cpp' --include='*.hpp' --include='*.h' -- "${pattern}" "${target}" || true)"
  if [ -n "${hits}" ]; then
    found="${found}
--- ${pattern}
${hits}"
    status=1
  fi
done

if [ "${status}" -ne 0 ]; then
  echo "sim-purity: FAIL — ${target} names constructs the simulation may not use:" >&2
  echo "${found}" >&2
  echo "" >&2
  echo "Each of these makes a run irreproducible from its seed. See the header of" >&2
  echo "this script for why, and move the offending code out of src/world/ if it" >&2
  echo "genuinely needs one." >&2
  exit 1
fi

file_count="$(find "${target}" -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) | wc -l)"
echo "sim-purity: CLEAN — ${file_count} file(s) under ${target}"
exit 0
