# TESTING.md

How to run and interpret the validation suite. Acceptance levels and the hard gate are defined in
`docs/policies/TESTING_POLICY.md`.

> **Status:** no tests exist yet — they land at P1. This document specifies how they are run and
> structured, so the P1 work implements a known shape rather than inventing one.

## Headless self-tests

```bash
scripts/build.sh                 # build (produces AnabasisTests + AnabasisStateTests)
scripts/run-tests.sh             # runs BOTH console apps (fail-closed: a missing binary fails)
```

`run-tests.sh` finds both binaries under `build/` and runs them; it exits non-zero on any failed
`check` **or a missing binary**. The missing-binary case matters: without it, a build that
produced nothing would pass the gate silently.

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
| true-peak accuracy (≤ 0.1 dB) | The **whole OS matrix** — Off / 2× / 4× / 8× / 16× **× both phase modes** (minimum / linear) — because the estimator's input path differs per setting: its own 4× interpolator, a further ≥ 2×, or the oversampled signal read directly. It must cover **both taps**: the limiter's detector *and* the ceiling clamp's (ADR-0002), which read at different points in the chain | ADR-0003 item 9 |
| `testReportedLatencyMatchesImpulse` | The impulse must land at **exactly `maxLookahead + OS` for every lookahead value**, not just at the range ends — the constant-allowance contract is what makes a padding bug a test failure | ADR-0004 |
| click-free transitions | Must include a **lookahead move** — it is the one switchable path with neither a duck nor a latch (`DSP_POLICY.md` invariant 8) | ADR-0004 |

### `tests/state_tests.cpp` → `AnabasisStateTests`

Compiles the **real** plugin sources into its own console target, so it exercises the actual
`AudioProcessor` rather than a mock. The editor sources compile (because `createEditor()`
references them) but are never instantiated — the tests run headlessly and open no window.

Planned coverage: serialized-schema shape; the **parameter-registry snapshot**; raw-exact
save → load → save round-trip (byte-identical); every legacy read path via a frozen fixture;
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
scripts/run-pluginval.sh 10 deterministic   # strictness 10, fixed nonzero seed (release gate, mode A)
scripts/run-pluginval.sh 10 randomise        # strictness 10, --randomise x3 (release gate, mode B)
scripts/run-pluginval.sh 5                   # development bar (P1–P2 default), deterministic
scripts/run-pluginval.sh                     # default strictness 8
```

Strictness targets: **5** development (P1–P2), **8** standard gate (P3–P5), **10** pre-release gold
(P6/release). Each mode runs **3 consecutive** passes; both modes must pass on all three platforms
at the release bar. Windows uses `run-pluginval.ps1`.

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
