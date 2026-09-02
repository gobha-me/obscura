#!/usr/bin/env bash
#
# ui-firewall — keep ground truth out of render/ and input/.
#
# The scan starts from every source and public header in those two layers and
# follows first-party includes transitively. A direct token check alone would
# miss the dangerous case where a harmless-looking projection header later
# starts including the ground-truth model.

set -euo pipefail

here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${here}/../.." && pwd)"

scan() {
  local root="$1"
  shift
  local -a search_roots=("$@") queue=()
  local -A seen=()
  local path file line header candidate current resolved
  local status=0 count=0

  for path in "${search_roots[@]}"; do
    if [ ! -d "${path}" ]; then
      echo "ui-firewall: not a directory (or does not exist): ${path}" >&2
      return 1
    fi
    while IFS= read -r -d '' file; do
      queue+=("${file}")
    done < <(find "${path}" -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) -print0)
  done

  while ((${#queue[@]} > 0)); do
    file="${queue[0]}"
    queue=("${queue[@]:1}")
    current="$(realpath -m -- "${file}")"

    if [[ -n "${seen[${current}]:-}" ]]; then
      continue
    fi
    seen["${current}"]=1
    ((count += 1))

    if [ "${current}" = "${root}/include/obscura/world/truth.hpp" ]; then
      echo "ui-firewall: FAIL — reached ground-truth header: ${current#"${root}/"}" >&2
      status=1
    fi

    if grep -nE -- '\<(Truth|Veracity)\>' "${current}" >/dev/null; then
      echo "ui-firewall: FAIL — ${current#"${root}/"} names a ground-truth-only type:" >&2
      grep -nE -- '\<(Truth|Veracity)\>' "${current}" >&2
      status=1
    fi

    while IFS= read -r line; do
      header="$(printf '%s\n' "${line}" | sed -nE 's/^.*#[[:space:]]*include[[:space:]]*[<"]([^>"]+)[>"].*$/\1/p')"
      if [ -z "${header}" ]; then
        echo "ui-firewall: FAIL — opaque include in ${current#"${root}/"}: ${line}" >&2
        status=1
        continue
      fi

      candidate=""
      case "${header}" in
        obscura/*)
          candidate="${root}/include/${header}"
          ;;
        *)
          if [[ "${line}" == *'"'* ]]; then
            candidate="$(dirname -- "${current}")/${header}"
          fi
          ;;
      esac

      if [ -n "${candidate}" ] && [ -f "${candidate}" ]; then
        resolved="$(realpath -m -- "${candidate}")"
        case "${resolved}" in
          "${root}"/*) queue+=("${resolved}") ;;
          *)
            echo "ui-firewall: FAIL — first-party include escapes the repository: ${header}" >&2
            status=1
            ;;
        esac
      fi
    done < <(grep -nE -- '^[[:space:]]*#[[:space:]]*include' "${current}" || true)
  done

  if [ "${status}" -ne 0 ]; then
    return 1
  fi

  echo "ui-firewall: CLEAN — ${count} file(s) reachable from render/ and input/"
}

selftest() {
  local fixture
  fixture="$(mktemp -d)"
  trap 'rm -rf -- "${fixture}"' RETURN

  mkdir -p "${fixture}/include/obscura/world" "${fixture}/src/render" \
    "${fixture}/src/input"
  printf '#pragma once\nstruct Projection {};\n' \
    >"${fixture}/include/obscura/world/projection.hpp"
  printf '#pragma once\nstruct Truth {};\n' \
    >"${fixture}/include/obscura/world/truth.hpp"
  printf '#include <obscura/world/projection.hpp>\n' \
    >"${fixture}/src/render/clean.cpp"
  printf '#include <obscura/world/projection.hpp>\n' \
    >"${fixture}/src/input/clean.cpp"

  scan "${fixture}" "${fixture}/src/render" "${fixture}/src/input" \
    >/dev/null

  printf '#include <obscura/world/truth.hpp>\n' \
    >"${fixture}/include/obscura/world/projection.hpp"
  if scan "${fixture}" "${fixture}/src/render" "${fixture}/src/input" \
      >/dev/null 2>&1; then
    echo "ui-firewall selftest: transitive truth include was accepted" >&2
    return 1
  fi

  printf '#pragma once\nstruct Projection {};\n' \
    >"${fixture}/include/obscura/world/projection.hpp"
  printf 'void inspect(Veracity value);\n' >"${fixture}/src/input/leak.cpp"
  if scan "${fixture}" "${fixture}/src/render" "${fixture}/src/input" \
      >/dev/null 2>&1; then
    echo "ui-firewall selftest: direct ground-truth type was accepted" >&2
    return 1
  fi

  rm -- "${fixture}/src/input/leak.cpp"
  printf '#include PROJECT_HEADER\n' >"${fixture}/src/render/opaque.cpp"
  if scan "${fixture}" "${fixture}/src/render" "${fixture}/src/input" \
      >/dev/null 2>&1; then
    echo "ui-firewall selftest: opaque include was accepted" >&2
    return 1
  fi

  echo "ui-firewall selftest: PASS"
}

if [ "${1:-}" = "--selftest" ]; then
  if [ "$#" -ne 1 ]; then
    echo "usage: tools/lint/ui_firewall.sh [--selftest]" >&2
    exit 2
  fi
  selftest
  exit 0
fi

if [ "$#" -ne 0 ]; then
  echo "usage: tools/lint/ui_firewall.sh [--selftest]" >&2
  exit 2
fi

scan "${repo_root}" \
  "${repo_root}/src/render" \
  "${repo_root}/include/obscura/render" \
  "${repo_root}/src/input" \
  "${repo_root}/include/obscura/input"
