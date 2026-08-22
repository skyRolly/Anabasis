#!/usr/bin/env bash
# ============================================================================
#  Anabasis -- local preflight: the lint gates + the fast release gate, in one
#  command, before a push spends a CI round trip discovering the same thing.
#
#  WHAT THIS RUNS, in CI's own order (docs/procedures/CI_CD.md "Reproducing CI
#  locally"): every checker with its --self-test FIRST (seconds, no build,
#  historically the most-tripped gates), then the built test suites when a built
#  tree exists.
#
#  WHAT THIS CANNOT RUN, said out loud rather than implied. Two of the checkers
#  need something a bare checkout does not have, and each says so instead of
#  passing quietly:
#    * the Clang warning gate needs a fresh clang build log to classify;
#    * the Linux ABI floor needs the LINKED, STRIPPED artifacts.
#  Their --self-tests run here, and the ABI floor additionally runs for real
#  when a built VST3 is present, since that is the one of the two whose input an
#  ordinary local Release build already produces. A green preflight is therefore
#  "the checkers and suites pass", not "CI will be green".
#
#  THE CITATION GATE RUNS THREE TIMES, against every base that can disagree:
#  `origin/main` (the local default and the PR merge-base case), the branch's
#  merge base with it, and `HEAD~1` -- the PUSH PREDECESSOR, which is what CI
#  actually compares and which the other two do NOT approximate once a branch
#  has more than one commit. The third is not hypothetical here: the 0.1.6 round
#  shipped with four anchors into `src/PluginParameters.cpp` that had been stale
#  since before it, because no local check had ever read them against the base
#  CI uses. Several escape hatches open at once is how a stale anchor ships.
#
#  NO SILENT SKIPS. If there is no built tree the suites are SKIPPED WITH A
#  NOTE, never silently -- a preflight that quietly did less than the reader
#  assumed is the defect class `scripts/build.sh` documents (its gate once
#  "passed" by testing a stale binary).
# ============================================================================
set -euo pipefail

cd "$(dirname "$0")/.."

echo "== preflight: documentation + source lints =="
python3 scripts/check-docs.py --self-test
python3 scripts/check-docs.py
python3 scripts/check-portability.py --self-test
python3 scripts/check-portability.py
python3 scripts/check-realtime.py --self-test
python3 scripts/check-realtime.py
python3 scripts/check-clang-warnings.py --self-test
echo "note: the FULL Clang warning gate needs a build log from the pinned"
echo "      compiler (CI: linux-clang); only its self-test ran here."

python3 scripts/check-linux-abi.py --self-test
# The ONE of the two that can also run for real locally: an ordinary Release
# build produces the artifact it reads. Skipped WITH A NOTE when absent, never
# silently -- same rule as the suites below.
ABI_SO="build/Anabasis_artefacts/Release/VST3/Anabasis.vst3/Contents/x86_64-linux/Anabasis.so"
ABI_APP="build/Anabasis_artefacts/Release/Standalone/Anabasis"
ABI_TARGETS=()
[ -f "$ABI_SO" ]  && ABI_TARGETS+=("$ABI_SO")
[ -f "$ABI_APP" ] && ABI_TARGETS+=("$ABI_APP")
if [ ${#ABI_TARGETS[@]} -gt 0 ]; then
    python3 scripts/check-linux-abi.py "${ABI_TARGETS[@]}"
else
    echo "note: no built Linux artifact -- the ABI floor check needs one; only its"
    echo "      self-test ran here. CI runs it on the STRIPPED bytes, which a local"
    echo "      build does not produce anyway."
fi

echo "== preflight: citation gate (all three bases) =="
python3 scripts/check-citations.py --self-test
python3 scripts/check-citations.py --check --base origin/main
MERGE_BASE="$(git merge-base origin/main HEAD 2>/dev/null || true)"
if [ -n "$MERGE_BASE" ] && [ "$MERGE_BASE" != "$(git rev-parse origin/main 2>/dev/null)" ]; then
    python3 scripts/check-citations.py --check --base "$MERGE_BASE"
fi

# THE PUSH PREDECESSOR, which is the base CI ACTUALLY uses and which neither of
# the two above approximates on a branch with more than one commit. `HEAD~1` is
# what `github.event.before` will be for the next push -- and on a branch whose
# merge base IS `origin/main`, both checks above compare against the same commit
# and this is the only one that reads the change since the last push.
PREV="$(git rev-parse HEAD~1 2>/dev/null || true)"
if [ -n "$PREV" ] && [ "$PREV" != "$MERGE_BASE" ]; then
    echo "-- against the push predecessor ($PREV), which is what CI compares"
    python3 scripts/check-citations.py --check --base "$PREV"
fi

echo "== preflight: test suites =="
if [ -d build/AnabasisTests_artefacts ] && [ -d build/AnabasisStateTests_artefacts ]; then
    scripts/run-tests.sh
else
    echo "note: no built tree at ./build -- the test-suite half of preflight DID NOT RUN."
    echo "      Build first (docs/procedures/BUILD.md), then re-run for the full gate."
fi

echo "== preflight: done =="
