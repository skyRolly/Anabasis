#!/usr/bin/env bash
# ============================================================================
#  Anabasis -- pluginval validation (Tracktion's open-source validator)
#
#  Downloads a pluginval release if not present, then validates the built VST3.
#  Works on Linux and macOS (the Windows job uses scripts/run-pluginval.ps1 with
#  the SAME structure). Editor open/close tests need a display, so we run under
#  xvfb-run on Linux when available.
#
#  Usage: scripts/run-pluginval.sh [strictness] [mode] [format]
#           strictness : 5 dev (P1-P2) / 8 standard (P3-P5) / 10 pre-release gold
#                        (P6 + every release) -- default 8
#           mode       : deterministic (default) | randomise
#           format     : vst3 (default) | au   -- `au` is macOS-only (Logic loads
#                        only AU, and KI-009 was reported in AU as well as VST3, so
#                        validating only the VST3 left the format the report names
#                        ungated). On a non-Darwin host `au` is an ERROR rather than
#                        a silent skip: a gate that quietly does nothing is the
#                        failure mode this script's fail-closed `find` exists to
#                        prevent.
#
#  Both modes run 3 CONSECUTIVE passes; ALL must pass:
#    deterministic -- fixed `--random-seed $PLUGINVAL_SEED` (NONZERO -- see below),
#                     reproducible.
#    randomise     -- `--randomise` (randomised test ORDER) with NO seed, so each
#                     run also draws a fresh seed; a value-/order-dependent defect
#                     surfaces here even when the deterministic pass is green.
#
#  SEED 0 IS NOT A SEED. pluginval treats 0 as "generate a random one":
#  `Source/PluginTests.h` -- "randomSeed = 0; the seed to use for the tests, 0
#  signifies a randomly generated seed" -- and `Source/CommandLine.cpp` only
#  forwards --random-seed to the validator when it differs from that default.
#  Passing `--random-seed 0` is therefore EXACTLY equivalent to passing nothing,
#  and the "deterministic" mode was not deterministic. Verified against pluginval
#  1.0.4: `--random-seed 0` printed a different `Random seed:` on every run, while
#  `--random-seed 1` printed `0x1` every time. Any nonzero value works; keep it
#  nonzero and keep it pinned.
#
#  The seed is meaningful WITHOUT --randomise: it seeds the RNG the tests
#  themselves draw from (`Validator.cpp` passes it to `UnitTestRunner::runTests`),
#  whereas --randomise only shuffles test ORDER. The two flags are independent.
#
#  Release gate: strictness 10, BOTH modes, on all three platforms
#  (docs/policies/TESTING_POLICY.md).
#
#  Network domain needed: github.com (pluginval release download).
# ============================================================================
set -euo pipefail

STRICTNESS="${1:-8}"
MODE="${2:-deterministic}"
FORMAT="${3:-vst3}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"
TOOLS_DIR="$ROOT/.tools"
mkdir -p "$TOOLS_DIR"

# Fail closed on ABSENCE and on AMBIGUITY, matching scripts/run-tests.sh. Taking
# `find ... | head -n1` would validate whichever bundle find happened to emit
# first: with a multi-config layout or a leftover build tree the release gate
# could pass on a different .vst3 than the one just built. CI always has a single
# fresh tree, so this only bites locally -- which is exactly where it would go
# unnoticed. P1 note: replace with the explicit expected path once CMakeLists.txt
# fixes the artefact layout.
case "$FORMAT" in
    vst3) BUNDLE_NAME="Anabasis.vst3" ;;
    au)
        if [ "$(uname -s)" != "Darwin" ]; then
            echo "format 'au' is macOS-only (this host is $(uname -s)) -- refusing to pass silently."
            exit 2
        fi
        BUNDLE_NAME="Anabasis.component"
        ;;
    *) echo "Unknown format '$FORMAT' (expected vst3|au)"; exit 2 ;;
esac

# An explicit bundle path overrides discovery. This exists for ONE case and is
# fail-closed for it: macOS Audio Units are resolved by the system through
# `AudioComponentFindNext`, which only ever finds components the AudioComponent
# registry knows about — i.e. bundles under a Components directory. A freshly
# built, never-installed .component in the build tree may therefore report ZERO
# plugin types no matter how correct it is, so the macOS job installs it first
# and points here. Set-but-missing is an ERROR rather than a fall back to
# discovery: silently validating a DIFFERENT bundle than the caller named is the
# failure this script's ambiguity check exists to prevent.
if [ -n "${ANABASIS_PLUGINVAL_BUNDLE:-}" ]; then
    if [ ! -e "$ANABASIS_PLUGINVAL_BUNDLE" ]; then
        echo "ANABASIS_PLUGINVAL_BUNDLE is set to '$ANABASIS_PLUGINVAL_BUNDLE' but nothing is there."
        exit 1
    fi
    VST3_MATCHES="$ANABASIS_PLUGINVAL_BUNDLE"
else
VST3_MATCHES="$(find "$BUILD_DIR" -maxdepth 8 -name "$BUNDLE_NAME" 2>/dev/null || true)"
VST3_COUNT="$(printf '%s' "$VST3_MATCHES" | grep -c . || true)"
if [ "$VST3_COUNT" -eq 0 ]; then
    echo "$BUNDLE_NAME not found under $BUILD_DIR -- build first (scripts/build.sh)."
    exit 1
fi
if [ "$VST3_COUNT" -ne 1 ]; then
    echo "$BUNDLE_NAME is ambiguous -- found $VST3_COUNT under $BUILD_DIR:"
    # Read line by line -- see the same guard in scripts/run-tests.sh: unquoted,
    # printf relied on word splitting (which also splits on spaces inside a path);
    # quoted, printf applies the format once so only the first line gets indented.
    while IFS= read -r m; do echo "  $m"; done <<< "$VST3_MATCHES"
    echo "Refusing to guess which bundle the release gate should validate. Remove the stale build tree."
    exit 1
fi
fi
VST3_PATH="$VST3_MATCHES"

# Platform-specific pluginval release + binary path (Linux vs macOS).
case "$(uname -s)" in
    Darwin) PV_ZIP="pluginval_macOS.zip"; PLUGINVAL="$TOOLS_DIR/pluginval.app/Contents/MacOS/pluginval" ;;
    *)      PV_ZIP="pluginval_Linux.zip"; PLUGINVAL="$TOOLS_DIR/pluginval" ;;
esac

if [ ! -x "$PLUGINVAL" ]; then
    echo "Fetching pluginval ($PV_ZIP)..."
    curl -L "https://github.com/Tracktion/pluginval/releases/latest/download/$PV_ZIP" -o "$TOOLS_DIR/pluginval.zip"
    (cd "$TOOLS_DIR" && unzip -o pluginval.zip >/dev/null)
    # NOT `|| true`: a failed chmod here resurfaces later as an opaque "cannot
    # execute" from the validation loop, which reads as a plugin problem rather
    # than the setup problem it is. Fail where the fault actually is.
    chmod +x "$PLUGINVAL"
fi

RUN_PREFIX=""
if command -v xvfb-run >/dev/null 2>&1; then
    RUN_PREFIX="xvfb-run -a"
fi

# Extra flags + pass count per mode. Both modes run 3 consecutive passes.
# PLUGINVAL_SEED must stay NONZERO -- 0 is pluginval's "pick a random seed"
# sentinel, not a seed (see the header). The exact value is arbitrary; that it is
# fixed and nonzero is not. Keep it identical to run-pluginval.ps1 so the three
# platforms validate against the same seed.
PLUGINVAL_SEED=1
case "$MODE" in
    randomise)     MODE_ARGS=(--randomise);                     PASSES=3 ;;
    deterministic) MODE_ARGS=(--random-seed "$PLUGINVAL_SEED"); PASSES=3 ;;
    *) echo "Unknown mode '$MODE' (expected deterministic|randomise)"; exit 2 ;;
esac

# ----------------------------------------------------------------------------
#  One validation pass. Retry ONLY on a signal-crash (segfault/abort), never on a
#  real validation failure. Editor/window tests embed the plugin via X11/XEmbed and
#  a crash can originate in the VALIDATOR's own JUCE host code rather than in the
#  plugin -- which cannot be fixed from here. A real plugin defect crashes
#  deterministically and still fails after the retries; a real test ASSERTION
#  returns a non-signal exit code and fails immediately.
#
#  THE RETRY IS LINUX-ONLY (0.2.0), and that scoping is the point rather than an
#  incidental detail. The flake it excuses is X11/XEmbed -- a mechanism that does
#  not exist on macOS, where this same script also runs (the `macos` and
#  `macos-intel` jobs, VST3 and AU). Retrying a signal crash there gave a genuine
#  crash three chances to disappear on a platform with no known host-side flake
#  to excuse it, which is the opposite of what a release gate is for. On macOS a
#  crash is a crash and fails the pass immediately.
#
#  (Windows has its own script and its own, DIFFERENT retry: `run-pluginval.ps1`
#  retries because a GUI-subsystem process can return a null `$LASTEXITCODE`,
#  which is an exit-code DETECTION problem rather than a crash it is excusing.
#  That rationale is unrelated to this one and is deliberately left alone.)
# ----------------------------------------------------------------------------
case "$(uname -s)" in
    Linux) CRASH_RETRY_ATTEMPTS=3 ;;   # the XEmbed flake documented above
    *)     CRASH_RETRY_ATTEMPTS=1 ;;   # no known host-side flake: fail on the first crash
esac

run_one_pass() {
    local label="$1"
    local attempts="$CRASH_RETRY_ATTEMPTS" attempt rc
    for attempt in $(seq 1 "$attempts"); do
        set +e
        $RUN_PREFIX "$PLUGINVAL" --strictness-level "$STRICTNESS" "${MODE_ARGS[@]}" \
            --validate "$VST3_PATH" --timeout-ms 600000
        rc=$?
        set -e

        if [ "$rc" -eq 0 ]; then
            echo "pluginval: PASSED ($label) at strictness $STRICTNESS (attempt $attempt/$attempts)"
            return 0
        fi
        if [ "$rc" -lt 128 ]; then
            echo "pluginval: FAILED ($label) at strictness $STRICTNESS (exit $rc) -- real validation failure, not a crash."
            return "$rc"
        fi
        if [ "$attempts" -eq 1 ]; then
            echo "pluginval: CRASHED ($label, exit $rc) -- no crash-retry on this platform (the retry exists for the Linux X11/XEmbed flake only)."
            return "$rc"
        fi
        echo "pluginval: crashed ($label, exit $rc -- signal crash). Retry $attempt/$attempts."
    done
    echo "pluginval: still crashing ($label) after $attempts attempts -- treating as a failure."
    return 139
}

echo "Validating $VST3_PATH at strictness $STRICTNESS -- format=$FORMAT mode=$MODE (${PASSES} consecutive pass(es) required)"
for pass in $(seq 1 "$PASSES"); do
    run_one_pass "$FORMAT $MODE pass $pass/$PASSES"
done
echo "pluginval: ALL ${PASSES} ${MODE} pass(es) succeeded for ${FORMAT} at strictness $STRICTNESS"
