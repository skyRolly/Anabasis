#!/usr/bin/env bash
# ============================================================================
#  Anabasis -- headless command-line build (CMake + Ninja)
#
#  Usage: scripts/build.sh [Release|Debug]
#  Outputs the built .vst3 path on success.
# ============================================================================
set -euo pipefail

BUILD_TYPE="${1:-Release}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"

if [ ! -f "$ROOT/CMakeLists.txt" ]; then
    echo "No CMakeLists.txt at $ROOT -- the build lands at P1 (docs/DEVELOPMENT_BRIEF.md §11)."
    exit 1
fi

cmake -B "$BUILD_DIR" -S "$ROOT" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE"

echo
echo "=== Build artefacts ==="
VST3_PATH="$(find "$BUILD_DIR" -maxdepth 8 -name 'Anabasis.vst3' 2>/dev/null | head -n1 || true)"
if [ -n "$VST3_PATH" ]; then
    echo "VST3: $VST3_PATH"
else
    echo "WARNING: Anabasis.vst3 not found under $BUILD_DIR"
fi

# These three are OPTIONAL artefacts (a target set may omit the tests, or they may
# be configured off), so their absence is not an error -- but it must not look like
# one either. `[ -n "$VAR" ] && echo ...` is wrong here: `set -e` does not abort on
# the left-hand side of an `&&` list, yet the status of the LAST command in a script
# becomes the SCRIPT's exit status. With the state-test binary missing, the final
# test returned 1 and build.sh exited 1 after a completely successful build --
# turning `scripts/build.sh Debug && scripts/run-tests.sh`
# (docs/procedures/DEVELOPMENT.md) into a build that silently never runs the tests.
# `if ... fi` reports the same thing and always exits 0, whatever ends up last.
STANDALONE="$(find "$BUILD_DIR" -maxdepth 8 -name 'Anabasis' -type f 2>/dev/null | head -n1 || true)"
if [ -n "$STANDALONE" ]; then
    echo "Standalone: $STANDALONE"
fi

TESTS="$(find "$BUILD_DIR" -maxdepth 8 -name 'AnabasisTests' -type f 2>/dev/null | head -n1 || true)"
if [ -n "$TESTS" ]; then
    echo "Tests: $TESTS"
fi

STATE_TESTS="$(find "$BUILD_DIR" -maxdepth 8 -name 'AnabasisStateTests' -type f 2>/dev/null | head -n1 || true)"
if [ -n "$STATE_TESTS" ]; then
    echo "State tests: $STATE_TESTS"
fi
