#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "usage: tools/format.sh [--check|--fix|--selftest]" >&2
}

if (($# > 1)); then
  usage
  exit 2
fi

mode=${1:---check}
formatter=${CLANG_FORMAT:-clang-format-20}

case "$mode" in
  --check | --fix | --selftest) ;;
  *)
    usage
    exit 2
    ;;
esac

if ! command -v "$formatter" >/dev/null 2>&1; then
  echo "error: $formatter was not found; install clang-format 20.x or set CLANG_FORMAT" >&2
  exit 1
fi

formatter=$(command -v "$formatter")

version=$($formatter --version)
if [[ ! "$version" =~ version[[:space:]]20\. ]]; then
  echo "error: OBSCURA formatting requires clang-format 20.x; found: $version" >&2
  exit 1
fi

format_repository() {
  local requested_mode=$1
  local repo_root
  local -a files

  repo_root=$(git rev-parse --show-toplevel)
  cd "$repo_root"

  mapfile -d '' files < <(
    git ls-files -z --cached --others --exclude-standard -- '*.cpp' '*.hpp'
  )
  if ((${#files[@]} == 0)); then
    echo "error: no first-party C++ files found" >&2
    return 1
  fi

  if [[ "$requested_mode" == "--fix" ]]; then
    "$formatter" -i "${files[@]}"
  else
    "$formatter" --dry-run --Werror "${files[@]}"
  fi
}

selftest() (
  local script_path
  local fixture
  local ignored_before

  script_path=$(realpath "${BASH_SOURCE[0]}")
  fixture=$(mktemp -d)
  trap 'rm -rf -- "$fixture"' EXIT

  git -C "$fixture" init --quiet

  printf 'int tracked( ){return 0;}\n' >"$fixture/tracked.cpp"
  "$formatter" -i "$fixture/tracked.cpp"
  git -C "$fixture" add tracked.cpp

  (
    cd "$fixture"
    CLANG_FORMAT="$formatter" "$script_path" --check
  )

  printf 'int untracked( ){return 1;}\n' >"$fixture/untracked.cpp"
  printf 'int header_value( ){return 2;}\n' >"$fixture/untracked.hpp"
  if (
    cd "$fixture"
    CLANG_FORMAT="$formatter" "$script_path" --check >/dev/null 2>&1
  ); then
    echo "error: self-test accepted a malformed untracked source" >&2
    return 1
  fi

  (
    cd "$fixture"
    CLANG_FORMAT="$formatter" "$script_path" --fix
    CLANG_FORMAT="$formatter" "$script_path" --check
  )
  "$formatter" --dry-run --Werror \
    "$fixture/untracked.cpp" "$fixture/untracked.hpp"

  printf 'ignored.cpp\n' >"$fixture/.gitignore"
  printf 'int ignored( ){return 3;}\n' >"$fixture/ignored.cpp"
  ignored_before=$(<"$fixture/ignored.cpp")
  (
    cd "$fixture"
    CLANG_FORMAT="$formatter" "$script_path" --fix
    CLANG_FORMAT="$formatter" "$script_path" --check
  )
  if [[ $(<"$fixture/ignored.cpp") != "$ignored_before" ]]; then
    echo "error: self-test formatted an ignored source" >&2
    return 1
  fi

  echo "format source-discovery self-test passed"
)

if [[ "$mode" == "--selftest" ]]; then
  selftest
else
  format_repository "$mode"
fi
