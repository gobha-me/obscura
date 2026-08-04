#!/bin/bash
#
# sim-purity — the [SIM] discipline, enforced.
#
# src/world/ and include/obscura/world/ are the simulation: its sources and the
# public headers they declare their types in. A run of OBSCURA has to be
# reconstructible from a seed and a list of intents alone, or replay is a lie
# and the case solver is checking a world the player never saw. That cannot be
# tested directly — a determinism bug reproduces perfectly until the day it
# does not — so it is enforced structurally instead: the simulation may not
# name the constructs that break it.
#
# Both trees are scanned, and the header tree is not the lesser half. A `double`
# member, an `unordered_map` field or a `#include <chrono>` in one of the [SIM]
# public headers poisons every translation unit that includes it while no banned
# token ever appears under src/world/ — the same indirect-violation gap the
# include allow-list below closes for headers pulled in from elsewhere. A check
# that read only the .cpp files would pass, and the corpus would diverge anyway.
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
# The seven token bans above only catch a violation spelled in the scanned
# trees themselves. A header included from here can reintroduce every one of
# them without any banned token appearing in the simulation — so includes are
# checked the other way round, against an allow-list: anything not named in
# `allowed_std` or `allowed_prefixes` below is a violation, including a header
# that does not exist yet. Default-deny is the point. A deny-list of <chrono>
# and <random> would wave through <ratio>, <valarray>, <syncstream> and
# tomorrow's addition to the standard library, and each of those has to be
# considered once, at the moment someone reaches for it, rather than never.
#
# The allow-list also carries the one-way dependency rule (AGENTS.md): world/
# depends on nothing else in this project, so obscura/world/ is permitted and
# obscura/{core,render,input,replay,audio,cases}/ is not. That direction is what
# lets the replay player drive a run with no App and no terminal at all.
#
# Registered as the ctest case `sim-purity` (see test/CMakeLists.txt), so a
# violation fails the suite rather than waiting for review.
#
# Usage: tools/lint/sim_purity.sh [directory ...]
#   Scans src/world/ and include/obscura/world/ by default. Every directory
#   given is scanned under the same rules — there is one [SIM] standard, not a
#   stricter one for sources. Exit 0 clean, exit 1 on any violation.
#
# Keep the executable bit (mode 100755): ctest runs this by path with no
# interpreter, and cmake/check_artifacts.cmake rule B5 enforces it.

set -euo pipefail

here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${here}/../.." && pwd)"
# The [SIM] tree is two directories: the sources and the public headers they
# define their types in. Both are scanned by default; an explicit argument list
# replaces the pair rather than adding to it, so a caller can narrow to one.
if [ "$#" -gt 0 ]; then
  targets=("$@")
else
  targets=("${repo_root}/src/world" "${repo_root}/include/obscura/world")
fi

# Check every target before scanning any, so a typo in the second path is not
# reported only after the first has been walked. A missing directory is a hard
# error, not a skip: silently scanning nothing is how a lint rots into one that
# passes forever.
missing=""
for target in "${targets[@]}"; do
  if [ ! -d "${target}" ]; then
    missing="${missing}  ${target}
"
  fi
done
if [ -n "${missing}" ]; then
  echo "sim-purity: not a directory (or does not exist):" >&2
  printf '%s' "${missing}" >&2
  exit 1
fi

# Repo-relative paths for the report — absolute ones are noise when every path
# shares the same long prefix.
target_label="$(
  for target in "${targets[@]}"; do
    printf '%s ' "${target#"${repo_root}/"}"
  done
)"
target_label="${target_label% }"

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

# The include allow-list. Every entry is here because it cannot carry a clock,
# ambient entropy, floating point or a hash order into the simulation — not
# because it happened to be needed once. Adding a line is a determinism
# decision; make it deliberately, and say why in the commit.
allowed_std=(
  # Fixed-width integers and size types: the arithmetic substrate. Q16.16 is
  # built out of these.
  'cstdint'
  'cstddef'
  # Contiguous, ordered containers and views. Iteration order is insertion
  # order on every implementation, which is the whole requirement.
  'array'
  'vector'
  'span'
  # Text. Same story — order is the order of the characters.
  'string'
  'string_view'
  # Algorithms over the above. Deterministic given a deterministic comparator,
  # which is why the comparators here are always explicit.
  'algorithm'
  # Vocabulary types for "no value" and "failed, here is why". No state of
  # their own beyond what they are given.
  'optional'
  'expected'
  # std::move, std::pair, std::swap. No state, no order, no clock.
  'utility'
)

# Prefix matches, for whole subtrees. Exactly one entry, and it is the one-way
# dependency rule: world/ may include world/.
allowed_prefixes=(
  'obscura/world/'
)

include_allowed() {
  local header="$1" entry
  for entry in "${allowed_std[@]}"; do
    if [ "${header}" = "${entry}" ]; then return 0; fi
  done
  for entry in "${allowed_prefixes[@]}"; do
    case "${header}" in
      "${entry}"*) return 0 ;;
    esac
  done
  return 1
}

# Annotation only — the allow-list is what rejects. This just answers "why not"
# for the headers someone is most likely to reach for, so the failure explains
# itself instead of sending the reader to this file.
deny_reason() {
  case "$1" in
    chrono | ctime | ratio)
      echo "the wall clock is the definitive non-reproducible input; sim advances in ticks the caller supplies" ;;
    random)
      echo "ambient entropy and global generator state; use rng(seed, stream_id) from the seed the run was given" ;;
    unordered_*)
      echo "implementation-defined iteration order; use std::vector or a sorted flat map with an explicit comparator" ;;
    cmath | cfloat | complex | numbers | valarray | cstdlib)
      echo "floating point (and, for cstdlib, rand/srand); simulation state is integers or Q16.16 with explicit rounding" ;;
    obscura/core/* | obscura/render/* | obscura/input/* | obscura/replay/* | obscura/audio/* | obscura/cases/*)
      echo "wrong direction: world/ depends on nothing else in this project (AGENTS.md); those layers read world/, not the reverse" ;;
    '(computed)')
      echo "a macro include is opaque to this check, which is the same as evading it" ;;
    *)
      echo "not on the [SIM] allow-list" ;;
  esac
}

status=0
found=""

for pattern in "${patterns[@]}"; do
  # -r over the directory rather than a glob, so a new subdirectory is covered
  # the moment it exists. || true because grep exits 1 on "no matches", which is
  # the outcome we want and `set -e` would treat as a failure.
  hits="$(grep -rnE --include='*.cpp' --include='*.hpp' --include='*.h' -- "${pattern}" "${targets[@]}" || true)"
  if [ -n "${hits}" ]; then
    found="${found}
--- ${pattern}
${hits}"
    status=1
  fi
done

# Includes, checked against the allow-list. Every `#include` line is collected
# as `header<TAB>location`, so the report can group by header without needing
# associative arrays.
include_lines="$(grep -rnE --include='*.cpp' --include='*.hpp' --include='*.h' \
  -- '^[[:space:]]*#[[:space:]]*include' "${targets[@]}" || true)"

include_records=""
if [ -n "${include_lines}" ]; then
  while IFS= read -r line; do
    # <foo> and "foo" both; anything else is a macro include, recorded under a
    # name of its own rather than skipped — an unreadable include is not a
    # clean one.
    header="$(printf '%s\n' "${line}" \
      | sed -nE 's/^.*#[[:space:]]*include[[:space:]]*[<"]([^>"]+)[>"].*$/\1/p')"
    if [ -z "${header}" ]; then
      header='(computed)'
    fi
    if include_allowed "${header}"; then
      continue
    fi
    include_records="${include_records}${header}	${line}
"
    status=1
  done <<< "${include_lines}"
fi

if [ -n "${found}" ]; then
  echo "sim-purity: FAIL — ${target_label} names constructs the simulation may not use:" >&2
  echo "${found}" >&2
  echo "" >&2
  echo "Each of these makes a run irreproducible from its seed. See the header of" >&2
  echo "this script for why, and move the offending code out of the [SIM] tree if" >&2
  echo "it genuinely needs one." >&2
fi

if [ -n "${include_records}" ]; then
  echo "sim-purity: FAIL — ${target_label} includes headers outside the [SIM] allow-list:" >&2
  while IFS= read -r header; do
    [ -n "${header}" ] || continue
    echo "" >&2
    echo "--- ${header} — $(deny_reason "${header}")" >&2
    printf '%s' "${include_records}" \
      | awk -F'\t' -v h="${header}" '$1 == h { print $2 }' >&2
  done <<< "$(printf '%s' "${include_records}" | cut -f1 | sort -u)"
  echo "" >&2
  echo "The allow-list is default-deny: a header is permitted only if it is named" >&2
  echo "in allowed_std or allowed_prefixes at the top of this script. If the" >&2
  echo "simulation genuinely needs one of these, the usual answer is that the code" >&2
  echo "belongs outside the [SIM] tree — extending the list is a determinism" >&2
  echo "decision, not a formality." >&2
fi

if [ "${status}" -ne 0 ]; then
  exit 1
fi

file_count="$(find "${targets[@]}" -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) | wc -l)"
echo "sim-purity: CLEAN — ${file_count} file(s) under ${target_label}"
exit 0
