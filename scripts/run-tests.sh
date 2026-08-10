#!/usr/bin/env bash
# Run the Anabasis headless self-tests built by build.sh:
#   1. AnabasisTests      -- DSP acceptance suite
#   2. AnabasisStateTests -- state-serialization / parameter-compatibility suite
#
# Both are required (FAIL-CLOSED): a missing binary fails the gate. Without that,
# a build that produced nothing would pass the gate silently.
#
# Discovery is also fail-closed on AMBIGUITY. Picking `find ... | head -n1` would
# take whichever path find happens to emit first, so a stale second build tree
# (or a multi-config layout) could silently gate on a different configuration's
# binary than the one just built -- a green report about the wrong artifact.
# Requiring exactly one match makes that a loud failure instead.
#
# P1 note: once CMakeLists.txt fixes the artefact layout, replace this search
# with the explicit expected path.
#
# ANABASIS_TEST_RUNNER prefixes both invocations (default: none). It exists so a
# caller can run the SAME fail-closed discovery under a different execution
# environment instead of hardcoding artefact paths: the macOS job uses
# `arch -x86_64` to execute the universal binary's OTHER slice, which no gate
# ran at all until 2026-08-10. Word-split on purpose -- it is a command prefix
# ("arch -x86_64"), not a single path.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"
read -r -a TEST_RUNNER <<< "${ANABASIS_TEST_RUNNER:-}"

find_one() {
    local name="$1" matches
    matches="$(find "$BUILD_DIR" -maxdepth 8 -name "$name" -type f 2>/dev/null || true)"
    local count
    count="$(printf '%s' "$matches" | grep -c . || true)"
    if [ "$count" -eq 0 ]; then
        echo "$name not found under $BUILD_DIR -- build first (scripts/build.sh)." >&2
        return 1
    fi
    if [ "$count" -ne 1 ]; then
        echo "$name is ambiguous -- found $count under $BUILD_DIR:" >&2
        # Read line by line rather than `printf '  %s\n' $matches`: unquoted, that
        # relied on word splitting, which also splits on spaces INSIDE a path; quoted,
        # printf applies the format once so only the first line gets indented. find
        # emits one path per line, so this prints each exactly, indented. Only the
        # diagnostic in an already-failing branch -- but a mangled path is a bad thing
        # to hand someone who is mid-debug.
        while IFS= read -r m; do echo "  $m" >&2; done <<< "$matches"
        echo "Refusing to guess which one the gate should run. Remove the stale build tree." >&2
        return 1
    fi
    printf '%s' "$matches"
}

TESTS="$(find_one AnabasisTests)"
STATE_TESTS="$(find_one AnabasisStateTests)"

echo "Running ${TEST_RUNNER[*]:+${TEST_RUNNER[*]} }$TESTS"
"${TEST_RUNNER[@]}" "$TESTS"

echo
echo "Running ${TEST_RUNNER[*]:+${TEST_RUNNER[*]} }$STATE_TESTS"
"${TEST_RUNNER[@]}" "$STATE_TESTS"
