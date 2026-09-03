# TESTING.md

How to run and interpret the validation suite. Acceptance levels and the hard gate are defined in
`docs/policies/TESTING_POLICY.md`.

> **Status:** the two suites exist and are green (P1 skeleton, 2026-07-31) — `AnabasisTests`
> (DSP acceptance) and `AnabasisStateTests` (state / parameter compatibility). Rows in the
> invariant→test map still marked `TODO (P2+)` are the ones whose DSP does not exist yet; the
> P1 rows are live. The structure below describes the suites as built.

## Headless self-tests


## Realtime enforcement (0.2.0, ADR-0029)

Three tiers, and none of them subsumes another. Run them in this order when touching the audio path:

1. `python3 scripts/check-realtime.py --self-test && python3 scripts/check-realtime.py` — seconds,
   no build, every platform. It reads the branches the suite never executes.

   **It is fail-closed on its own inputs since 0.2.10, and the success line says so.** The gate
   used to report "N ordering requirement(s) met" from `len(REQUIRED_ORDER)` — the number of rules
   it was *asked* to check, not the number it *proved* — so a rule whose file had been renamed or
   deleted passed vacuously: nothing matched, the rule was never evaluated, and the count was
   printed anyway. It now reports "**N of M ordering requirement(s) verified**" from what was
   actually reached, fails when a required rule goes unreached, and refuses an empty input set
   outright (exit 2) rather than reporting "0 file(s) scanned" as success. Four of the self-test's
   cases drive `scan_repo` against real temporary trees for exactly these paths.
2. `scripts/run-tests.sh` — `testTheAudioPathAllocatesNothing` arms `tests/AllocationGuard.h` around
   `AnabasisEngine::process` across the configuration matrix. **Read its two `note:` lines**: they
   say how many calls were armed and which counters were live. A run that skips the assertions says
   so; it never passes them silently.
3. RealtimeSanitizer, which needs the pinned Clang (`scripts/setup-llvm-apt.sh <major>`; the major
   is `ANABASIS_CLANG_VERSION` in `build.yml`):

   ```
   cmake -B build-rt -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
     -DCMAKE_C_COMPILER=clang-<n> -DCMAKE_CXX_COMPILER=clang++-<n> \
     -DCMAKE_C_FLAGS="-fsanitize=realtime -fno-omit-frame-pointer" \
     -DCMAKE_CXX_FLAGS="-fsanitize=realtime -fno-omit-frame-pointer -DANABASIS_RTSAN_LANE=1" \
     -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=realtime" -DANABASIS_BUILD_STANDALONE=OFF
   cmake --build build-rt --target AnabasisTests
   ./build-rt/AnabasisTests_artefacts/RelWithDebInfo/AnabasisTests
   ```

   **Do not set `RTSAN_OPTIONS`.** The default halts on the first violation and exits 43;
   `halt_on_error=false` makes the process print violations and exit 0, which is a gate that cannot
   fail.

**A comparator proves it discriminates before its agreement counts.** `AnabasisChannelProbe
--assert-discriminating` refuses to print a baseline when two configurations produce identical
output and the pair is not declared in its source. A twin-build comparison over a collapsed scenario
set makes any two builds agree while testing nothing — which is exactly the evidence a dependency
bump is judged on.

```bash
scripts/build.sh                 # build (produces AnabasisTests + AnabasisStateTests)
scripts/run-tests.sh             # runs BOTH console apps (fail-closed: a missing binary fails)
```

`run-tests.sh` finds both binaries under `build/` and runs them; it exits non-zero on any failed
`check` **or a missing binary**. The missing-binary case matters: without it, a build that
produced nothing would pass the gate silently.

## Raw scanner output (SARIF artifacts)

Layer 1 of `TESTING_POLICY.md` (static analysis) runs two scanners in CI, and both publish their
findings twice: to GitHub Code Scanning, and — since this change — as an Actions artifact carrying
the scanner's own raw SARIF.

| Analyzer | Workflow | Artifact | SARIF on the runner |
|---|---|---|---|
| CodeQL | `codeql.yml` | `codeql-sarif-<language>-<sha>` | `${{ runner.workspace }}/results/<database-language>.sarif` |
| PREfast (MSVC `/analyze`) | `msvc.yml` | `prefast-sarif-<sha>` | `build\results.sarif` |

**Why the artifact exists.** The Code Scanning alert and check-run annotation APIs are not reachable
from every audit context, so the dashboard could not always be read back. The artifact is the raw
scanner report, retrievable through the ordinary Actions artifact interface.

**Two things to know before reading one.**

- The CodeQL artifact is **strictly richer than the dashboard**. `paths-ignore: build` filters the
  fetched JUCE tree out of the *alerts*, but those results are still present in the raw SARIF — so a
  non-zero result count here does not mean a first-party finding. Check the `uri` of each location:
  anything under `build/_deps/` is third-party JUCE.
- The CodeQL filename is the **database** language, not the matrix language — the `c-cpp` entry
  writes `cpp.sarif`, the `actions` entry `actions.sarif`. The workflow globs `*.sarif` rather than
  hard-coding that mapping.

Retention and failure behaviour are documented under "Artifact safety rules" in `CI_CD.md`: the
upload is gated on `!cancelled()`, not on success, because a report Code Scanning rejects is exactly
when the raw SARIF is most worth keeping.

## Documentation structure lint

Not an audio test, but it is a CI gate — recorded here because
`DOCUMENTATION_LIFECYCLE_POLICY.md`'s trigger map routes CI-workflow changes through this file:

```bash
python3 scripts/check-docs.py --self-test   # the checker's own guarantees; it prints its own case count
python3 scripts/check-docs.py               # whole-repo scan; exit 1 on any finding
```

The `docs` job in `build.yml` runs exactly these two commands on every push, self-test first — a
clean corpus scan is not evidence unless the script's own guarantees were exercised in the same
run. What it checks, and the limits of what it can prove, are stated in the script's docstring.

## Evidence-anchor lint

The other documentation gate, in the `source-lint` job rather than `docs` because it reads SOURCE
as well as prose. Documents of record cite their evidence as `some/file.cpp:695-752`; an edit above such a line
re-aims it silently and the document keeps reading as though it were still correct. (That example
names an untracked path deliberately — an illustration spelled with a tracked one is a citation as
far as the tool is concerned, and gets re-anchored along with the real ones.)

```bash
python3 scripts/check-citations.py --check              # base defaults to origin/main; exit 1 on drift
python3 scripts/check-citations.py --check --base @{u}  # and against what CI will use — see below
python3 scripts/check-citations.py --fix                # re-anchor, then RE-READ what it moved
```

**`HEAD~1` is not that base, and reaching for it is how this gate went red twice in a row.** With
the drifting edits still uncommitted, `HEAD` IS the commit you are pushing on top of, so `HEAD~1`
is one too far back — it compares against a revision CI will never look at, passes, and says
nothing about the one it will. `@{u}` is right in both states and is the form to use.

Run `--check` before pushing any change that moves lines in a tracked source file, and `--fix` in
the SAME change set that moved them — that is the repository's re-anchoring rule, and the gate
exists because 0.1.4 proved it does not survive being remembered.

**A repair looks exactly like drift to this tool, so declare it in the same commit.** `--check`
compares the TEXT at the base line against the text at the current line; it has no way to know
that `:330 -> :342` was a correction rather than a slip. So the run AFTER a re-anchor asks for the
re-anchor to be reverted, and the gate is red on the commit that fixed it — unless the new
spelling is added to `DELIBERATE_REAIMS` in that same commit. Re-anchor and declare together;
never in two pushes.

**Run it against BOTH bases, and this is not optional pedantry.** `--check` alone uses
`origin/main`; CI compares against the PREVIOUS PUSH. Those two disagree about which branch the
tool takes: a document whose citation COUNT differs from a base falls to the ordinal-pairing
fallback, which only judges base spellings still present verbatim — so against one base a set of
re-aimed anchors is silently unjudgeable and against the other it is flagged. Round 7 passed
locally on `origin/main` and failed CI on nine anchors for exactly that reason.

```bash
python3 scripts/check-citations.py --check                 # origin/main
python3 scripts/check-citations.py --check --base @{u}     # what CI will use on the next push
```

`@{u}` is the upstream tip — the commit you are pushing ON TOP OF, which is exactly what
`github.event.before` will be. `--base HEAD` is only the same thing while the drifting edits are
still uncommitted; once they are committed it means "compare the tree against itself", which is
clean by construction and tells you nothing.

Three limits are worth knowing before trusting a clean run, all of them stated in the script's
header:

* It proves anchors did not MOVE, never that they were aimed correctly to begin with — and this
  is the limit that matters most, because the tool makes a mis-aimed anchor look MAINTAINED. It
  was first recorded here as "three citations", which was an under-count found by inspecting three;
  a full audit of the governed documents found the majority of anchors in the architecture set had
  been wrong since before the tool existed, each faithfully carried onto the same unrelated code by
  every re-anchoring since. Anchors are therefore spelled with the SYMBOL beside the line number
  wherever the claim names one: that is the half a reader can check, and the half that survives
  the tool being wrong.
* It judges only citations spelled from the repository root and naming one of its tracked files.
  A bare file name, a sibling checkout's path, or a `<rev>:`-pinned anchor is deliberately left
  alone — the ownership test is narrow because every misclassification is a corrupted document.
* Anchors it could not judge (re-spelled or removed since the base) are counted and reported
  separately, so "17 anchors verified" never quietly means "17 of 33".

`--fix` is not a substitute for reading. It preserves the TEXT an anchor named, which is exactly
how a citation that was aimed at the wrong code stays aimed at the wrong code.

## Suite structure

### `tests/dsp_tests.cpp` → `AnabasisTests`

Deterministic DSP acceptance checks using a `check(cond, "what")` counter harness; `main()` calls
every test and exits non-zero on any failure. No test framework, no dependencies.

Planned coverage, one test per `DSP_POLICY.md` invariant (see the invariant → test map there):
chain order; reported latency; true-peak accuracy; **output never exceeds the ceiling**;
oversampling scope; ADAA aliasing measurement; null-with-defaults and bypass-null; click-free
transitions per switchable path; no NaN/Inf/denormals across the feature × oversampling ×
sample-rate matrix; loudness-compensation render neutrality; LUFS against the EBU R128 vectors;
dither placement and default.

**Four of these have a stimulus mandated by an ADR, not left to the implementer.** A test name
alone does not carry the property; these are the cases where the wrong stimulus passes vacuously:

| Test | Mandated stimulus | Source |
|---|---|---|
| `testOutputNeverExceedsCeiling` | Run in **both EQ positions**, and the Post case must include a **+12 dB shelf after the limiter** — the exact signal the clamp placement exists to survive | ADR-0002 |
| true-peak accuracy (≤ 0.1 dB) | The **whole OS matrix** — Off / 2× / 4× / 8× / 16× **× both phase modes** (minimum / linear) — because the estimator's input path differs per setting: its own 4× interpolator, a further ≥ 2×, or the oversampled signal read directly. It must cover **both taps**: the limiter's detector *and* the ceiling clamp's (ADR-0002), which read at different points in the chain. The `Off × linear` cell is knowingly degenerate — no filter is instantiated at Off, so phase cannot reach the estimator; keep it (uniform sweep) but do not hunt for a difference there | ADR-0003 item 9 |
| `testReportedLatencyMatchesImpulse` | The impulse must land at **exactly `maxLookahead + OS` for every lookahead value**, not just at the range ends — the constant-allowance contract is what makes a padding bug a test failure | ADR-0004 |
| click-free transitions | Must include a **lookahead move** — it is the one switchable path with neither a duck nor a latch (`DSP_POLICY.md` invariant 8) | ADR-0004 |

### `tests/state_tests.cpp` → `AnabasisStateTests`

Compiles the **real** plugin sources into its own console target, so it exercises the actual
`AudioProcessor` rather than a mock — **and, since P5, the actual editor**: a growing set of tests
call `createEditor()` and walk the resulting component tree
(`testTheSettingsPanelFollowsAProjectLoad`, `testThePopupShieldActuallyCoversTheEditor`,
`testEveryComboMenuFitsItsControl`, `testTheSavePresetNameFieldIsTaggedForItsFocusGlow`, the R2
tooltip sweep, the knob-position sweep). The tests still run **headlessly** and open no window: the
editor is built, sized and inspected, never shown, and nothing here runs a message loop — which is
why a `juce::Value` change (asynchronous through that loop) and anything requiring a modal pop-up
are outside what this target can reach, and are carried in `DEPENDENCY_POLICY.md`'s JUCE-internals
register instead.

A view's own ARITHMETIC is reached a different way, and 0.1.6 is the case that shows why both are
needed. `GrHistoryView` publishes the parts of its draw that carry a correctness argument as pure
statics — `windowEntries`, `buckets`, `bucketX`, `drawsZeroRegion`, since 0.1.6 `grY`, and since
0.2.8 `tipFirst`, `entryPeriod`, `smoothedHead`, `phaseOf`, `parked`, `paintHead`, `frameFor`,
`readFloor` and `bucketReads`, plus the ring's own `GrHistoryBuffer::prepare`, `prepared` and
`batchIntact` (pinned by `grPrepared`, through the ring and through the wrapper) — because an
expression reachable only from `paint` is one no test can pin and no mutant can kill; the GR
trace's vertical mapping under-reported reduction past 12 dB for three rounds while it sat inline,
and its horizontal geometry stepped a non-integer pitch once per bucket for six (0.1.2 → 0.2.8)
while the pinned property was only *where* buckets land, never *how* they move — the 0.2.8 walk in
`testGrHistoryWindowNeverAsksForTheHeadSlot` now holds the per-entry motion at every head across
three buckets, which is the assertion the stepped form fails.
`testGrHistoryAndTheMeterLanesShareOneReductionSpan` pins that mapping through the statics **and**
renders a standalone `GrMiniMeter` into an image (`createComponentSnapshot`, no editor and no
window) to check the OTHER readout of the same quantity independently — a test that quoted the
shared constant twice would pass with the meter dividing by anything.

This paragraph said "never instantiated" until 2026-08-13, having gone stale at P5 —
`TESTING_POLICY.md`'s harness-conventions bullet was corrected on the same point at 0.1.1 and this
copy was missed. An under-described coverage claim is not harmless: it invites the next contributor
to add a test that already exists.

Planned coverage: serialized-schema shape; the **parameter-registry snapshot**; raw-exact
save → load → save round-trip (byte-identical) and its fixed-point precondition
(`testRawRoundTripIsIdempotent`); the §4.4 structural-tolerance read rules — a valid root that omits
`ANABASIS` or `ANABASIS_INTERNAL` reads as *defaults*, never as "keep the live values"
(`testMissingChildrenReadAsDefaults`, which also pins the same rule at **PARAM granularity**: a
missing individual child resets that one parameter — behaviour supplied by the pinned JUCE's
reconnection fallback, not by our code, which is exactly why it is pinned); every legacy read path
via a frozen fixture;
corrupt/foreign-state robustness; user-preset round-trip + exclusion rules; A/B and view-param
preservation; **`testMacroDefaultIsFixedPoint`** — the macro mapping at the default position must
equal every managed parameter's declared default (ADR-0005, `MODE_AND_ADAPTATION_POLICY.md`
invariant 1); **`testModeSwitchIsSoundNeutral`** (invariant 2).

### The registry snapshot — how it is used

`tests/fixtures/parameter_registry.snapshot` freezes the parameter surface (IDs, names, order,
ranges, automation flags). The test fails on any change. Re-freezing is an explicit, deliberate
act:

```bash
AnabasisStateTests --write-snapshot     # ONLY for an INTENTIONAL parameter change
```

Re-freezing to turn a red test green is a compatibility break in disguise
(`PARAMETER_COMPATIBILITY_POLICY.md`). This test is what automates the "Parameter IDs unchanged"
release-checklist item.

**The fixture is pinned to LF by `.gitattributes`, and the comparison normalises line endings
anyway.** Git for Windows defaults to `core.autocrlf=true` — including on the GitHub-hosted
`windows` runner — so without the pin the fixture is checked out with CRLF there and all 49 lines
mismatch: a Windows-only red reporting a frozen-parameter-surface break that did not happen. Both
defences are kept because they fail differently: the pin fixes new checkouts, the normalisation
covers a clone made before the pin existed. On a real mismatch the test now prints the **first
differing line** and both sides — a bare `FAIL` costs a whole CI round to diagnose.

**Two defaults in the snapshot are knowingly off by ulps, and that is recorded here rather than
"fixed".** The dump writes `range.convertFrom0to1 (param->getDefaultValue())` — the value that
survives the *normalised* round trip, which is what a host actually restores — so a log taper's
`exp(log(x))` shows through: `limRelease` reads **100.000015** (declared 100 ms) and `eqBell2Freq`
reads **2999.999756** (declared 3000 Hz). The declared defaults in `PluginParameters.cpp` are the
round numbers; these are their images under the taper, correct to ~1e-7 relative and inaudible.
Rounding the dump to hide them would make the snapshot stop detecting a real taper change, which is
the one thing it exists to catch. `testRawRoundTripIsIdempotent` pins the property that actually
matters — that one save→load→save pass is a *fixed point*, so byte-identity holds — and it, not the
snapshot, is where a taper change is diagnosed.

## Writing a test

Use the existing harness and add the call in `main`. DSP behaviour → `dsp_tests.cpp`;
state/serialization/preset behaviour → `state_tests.cpp`.

**One documented exception to that split.** `testMacroDefaultIsFixedPoint` and
`testModeSwitchIsSoundNeutral` are *behavioural* guards but live in `state_tests.cpp`, because only
`AnabasisStateTests` compiles the wrapper sources (ADR-0008's target graph) and both need the APVTS
and the MacroEngine, which the DSP core deliberately cannot see (ADR-0001). Placement follows what
the target can link, not what the test measures — do not "fix" it by moving them.

**Every bug fix ships a regression test** that fails on the old code and passes on the fix
(`TESTING_POLICY.md` rule 1). A fix without one is not finished.

For this product specifically, a test that only uses well-behaved musical material is not a test.
Include hostile inputs: full-scale square waves, inter-sample-peak-heavy signals, DC, silence, and
parameters automated at audio rate.

## pluginval (VST3 conformance)

```bash
# The GATE value comes out of the one place that holds it, never pasted — same
# extraction README.md and CI_CD.md use, and for the reason CI_CD documents
# against itself: a literal here would go stale on the next raise while the
# comment beside it still claimed to be current.
STRICTNESS=$(sed -n 's/^  ANABASIS_PLUGINVAL_STRICTNESS:[[:space:]]*\([0-9][0-9]*\).*/\1/p' \
             .github/workflows/build.yml)
: "${STRICTNESS:?could not read ANABASIS_PLUGINVAL_STRICTNESS from build.yml}"
scripts/run-pluginval.sh "$STRICTNESS" deterministic   # fixed nonzero seed (gate, mode A)
scripts/run-pluginval.sh "$STRICTNESS" randomise       # --randomise x3    (gate, mode B)

scripts/run-pluginval.sh                    # no argument: the SCRIPT's own default (8) — a
                                            # convenience for a quick local pass, NOT the gate
```

The strictness ladder and the current value live in `ANABASIS_PLUGINVAL_STRICTNESS` at the top of
`.github/workflows/build.yml`, which carries the phase→strictness rows; `TESTING_POLICY.md` owns
what the gate REQUIRES and deliberately restates no number. This page used to spell the ladder out
as a third copy — correct at the time of writing, which is precisely how the README's copy survived
two raises. Each mode runs **3 consecutive** passes; both modes must pass on all three platforms at
the phase strictness. Windows uses `run-pluginval.ps1`.

The randomise mode exercises state restoration under randomised test order and an unpinned,
per-run seed — defects a fixed seed reproducibly misses.

**Do not "simplify" the deterministic mode's seed to 0.** pluginval treats `--random-seed 0` as
*"generate a random seed"* (`Source/PluginTests.h`), so 0 makes the deterministic mode identical to
the randomise mode minus the shuffle. The scripts pin a nonzero constant
(`PLUGINVAL_SEED` / `$PluginvalSeed`), and the same value on all three platforms. **Nothing
enforces that the two constants stay equal** — each script's comment names the other; that is the
whole mechanism.

**Reproducing a randomise-only failure.** pluginval logs the seed it drew as
`Random seed: 0x…` at the top of every run. Take that value from the failing CI log and replay it:

```bash
.tools/pluginval --strictness-level 10 --randomise --random-seed 0x4aeacb4 \
                 --validate build/…/Anabasis.vst3 --timeout-ms 600000
```

Test *order* is shuffled per repeat, so a pinned seed reproduces the draw, not necessarily the
interleaving of a 3-pass run.

**Copy the logged value verbatim — do not uppercase it.** pluginval accepts the `0x…` form
(`CommandLine.cpp`: `if (seedString.startsWith ("0x")) return seedString.getHexValue64();`) and
round-trips it exactly — verified against 1.0.4: `--random-seed 0x4aeacb4` logs
`Random seed: 0x4aeacb4`. But the character whitelist it is checked against
(`containsOnly ("x-0123456789acbdef")`) is **case-sensitive**, so `0X4AEACB4` is rejected with
*"Invalid random seed argument!"* and exit `-1` — which, per the retry table above, both scripts
misclassify as an abnormal termination and retry three times before failing. Decimal works too
(`78248628` logs `0x4a9fab4`), and is what the scripts themselves pass.

The script downloads pluginval if absent, finds the built `Anabasis.vst3`, and runs under
`xvfb-run` when available (Linux editor tests need a display).

### Crash retry — what it is and is not

An **abnormal termination** of the validator is retried up to 3 times; a **real validation
failure** fails immediately and is never retried. The retry exists to absorb host-side validator
crashes, not plugin defects — a real plugin defect crashes deterministically and still fails after
the retries.

**The boundary is platform-specific** (`docs/policies/TESTING_POLICY.md` rule 3 is the binding
statement):

| | abnormal termination → retried | real failure → immediate |
|---|---|---|
| **Linux / macOS** | `exit ≥ 128` (128 + signal number) | `exit < 128` |
| **Windows** | Win32 exception code (`≥ 256`), negative, or no code at all | **`1…255`, including 128…255** |

pluginval's own exit code is only ever **0 or 1** (`Source/CommandLine.cpp` funnels every failure
through `exitWithError`, which returns 1; the failure count goes to the log, not the exit code), so
anything larger comes from the OS. Windows has no signals — nothing the OS reports lands in 1…255
there — so a code in that range came from pluginval and is a *real* failure, **including 128…255**,
which on Linux/macOS would read as a crash. `run-pluginval.ps1` therefore classifies differently
from `run-pluginval.sh` by design.

The one code neither script classifies correctly is a **malformed command-line argument**:
pluginval exits `-1` (255 on POSIX), which both scripts read as an abnormal termination and retry
three times before failing. Both scripts construct their own arguments, so that code means the
script itself is broken — it still fails, just noisily.

On Windows, `run-pluginval.ps1` launches pluginval via `System.Diagnostics.Process` and
`WaitForExit()` rather than the call operator: pluginval is a **GUI-subsystem** app, so `& $pv`
returns immediately with a `$null` `$LASTEXITCODE`, which both false-greens the step and (with a
retry loop) spawns concurrent background validators. The exit code is the only trustworthy signal,
and it is only trustworthy after an explicit wait.

## What cannot be verified headlessly

- **Audio quality.** Transparency, punch retention, distortion onset and tonal shift — the entire
  point of the product — need a DAW, loudness-matched comparison, and ears.
- **GUI appearance.** Layout, animation smoothness, colour rendering, HiDPI.
- **Real-host behaviour.** Automation recording, offline render, plugin rescan, session
  restoration in an actual DAW.

These are Level 5 (`TESTING_POLICY.md`) and are a **required release precondition**, not an
optional extra. A green build + pluginval pass means "ready to audition," not "shipped."
