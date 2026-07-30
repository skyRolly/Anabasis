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

STANDALONE="$(find "$BUILD_DIR" -maxdepth 8 -name 'Anabasis' -type f 2>/dev/null | head -n1 || true)"
[ -n "$STANDALONE" ] && echo "Standalone: $STANDALONE"

TESTS="$(find "$BUILD_DIR" -maxdepth 8 -name 'AnabasisTests' -type f 2>/dev/null | head -n1 || true)"
[ -n "$TESTS" ] && echo "Tests: $TESTS"

STATE_TESTS="$(find "$BUILD_DIR" -maxdepth 8 -name 'AnabasisStateTests' -type f 2>/dev/null | head -n1 || true)"
[ -n "$STATE_TESTS" ] && echo "State tests: $STATE_TESTS"
