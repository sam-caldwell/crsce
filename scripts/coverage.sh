#!/usr/bin/env bash
set -euo pipefail

# Yes, folks.  I'm now adding ❌ to my scripts like a hipster.  Nothing keeps ya young like adopting stupid things
# we see the next generation doing.  (At least these youngsters aren't spraying hairspray on their pants and lighting
# it on fire like we did in the 80s.)  I even added a few ✅.

# Usage: scripts/coverage.sh <build_dir> <project_root>
BUILD_DIR=${1:-build}
ROOT=${2:-$(pwd)}

echo "--- Coverage: configure, build, run, and check >=95% ---"

# Find a GNU g++ and matching gcov
choose_gxx() {
  for c in g++-15 g++-14 g++-13 g++-12 g++-11 g++; do
    if command -v "$c" >/dev/null 2>&1; then
      echo "$c"
      return 0
    fi
  done
  return 1
}

CXX=$(choose_gxx || true)
if [[ -z "${CXX}" ]]; then
  echo "❌ No g++ found. Install Homebrew gcc and retry." >&2
  exit 1
fi

if "$CXX" --version 2>/dev/null | grep -qi clang; then
  echo "❌ Coverage requires GNU g++ (gcov). Found clang-compatible '$CXX'." >&2
  echo "   Install Homebrew gcc (e.g., 'brew install gcc') and retry." >&2
  exit 1
fi

echo "Using C++ compiler for coverage: $CXX"
# shellcheck disable=SC2001 # readability acceptable for tool substitution
GCOV=$(echo "$CXX" | sed 's/g++/gcov/')

cov_build="$BUILD_DIR/arm64-debug-coverage"

cmake -S "$ROOT" -B "$cov_build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_CXX_STANDARD=23 \
  -DCMAKE_CXX_FLAGS="--coverage -O0 -g" \
  -DCMAKE_EXE_LINKER_FLAGS="--coverage" >/dev/null

cmake --build "$cov_build" >/dev/null
(cd "$cov_build" && ctest --output-on-failure)

if ! command -v gcovr >/dev/null 2>&1; then
  echo "❌ gcovr not found. Run 'make ready/fix' to install." >&2
  exit 1
fi

echo "--- Generating coverage report (threshold: 95%) ---"

gcovr --gcov-executable "$GCOV" \
  --object-directory "$cov_build" \
  --root "$ROOT" \
  --filter '^(cmd|src|include)/' \
  --exclude 'src/common/FileBitSerializer.cpp' \
  --exclude 'test/' \
  --txt --fail-under-line=95 \
  > "$cov_build/coverage.txt"

cat "$cov_build/coverage.txt"
echo "--- ✅ Coverage check complete ✅"
