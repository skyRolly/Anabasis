#!/usr/bin/env bash
# Run the Anabasis headless self-tests built by build.sh:
#   1. AnabasisTests      -- DSP acceptance suite
#   2. AnabasisStateTests -- state-serialization / parameter-compatibility suite
#
# Both are required (FAIL-CLOSED): a missing binary fails the gate. Without that,
# a build that produced nothing would pass the gate silently.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"

TESTS="$(find "$BUILD_DIR" -name 'AnabasisTests' -type f 2>/dev/null | head -n1 || true)"
if [ -z "$TESTS" ]; then
    echo "AnabasisTests not found -- build first (scripts/build.sh)."
    exit 1
fi

STATE_TESTS="$(find "$BUILD_DIR" -name 'AnabasisStateTests' -type f 2>/dev/null | head -n1 || true)"
if [ -z "$STATE_TESTS" ]; then
    echo "AnabasisStateTests not found -- build first (scripts/build.sh)."
    exit 1
fi

echo "Running $TESTS"
"$TESTS"

echo
echo "Running $STATE_TESTS"
"$STATE_TESTS"
