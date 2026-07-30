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
chain order; reported latency == impulse-measured latency across the oversampling × lookahead
matrix; true-peak accuracy; **output never exceeds the ceiling** under hostile input; oversampling
scope; ADAA aliasing measurement; null-with-defaults and bypass-null; click-free transitions per
switchable path; no NaN/Inf/denormals across the feature × oversampling × sample-rate matrix;
loudness-compensation render neutrality; LUFS against the EBU R128 vectors; dither placement and
default.

### `tests/state_tests.cpp` → `AnabasisStateTests`

Compiles the **real** plugin sources into its own console target, so it exercises the actual
`AudioProcessor` rather than a mock. The editor sources compile (because `createEditor()`
references them) but are never instantiated — the tests run headlessly and open no window.

Planned coverage: serialized-schema shape; the **parameter-registry snapshot**; raw-exact
save → load → save round-trip (byte-identical); every legacy read path via a frozen fixture;
corrupt/foreign-state robustness; user-preset round-trip + exclusion rules; A/B and view-param
preservation.

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

**Every bug fix ships a regression test** that fails on the old code and passes on the fix
(`TESTING_POLICY.md` rule 1). A fix without one is not finished.

For this product specifically, a test that only uses well-behaved musical material is not a test.
Include hostile inputs: full-scale square waves, inter-sample-peak-heavy signals, DC, silence, and
parameters automated at audio rate.

## pluginval (VST3 conformance)

```bash
scripts/run-pluginval.sh 10 deterministic   # strictness 10, fixed seed (release gate, mode A)
scripts/run-pluginval.sh 10 randomise        # strictness 10, --randomise x3 (release gate, mode B)
scripts/run-pluginval.sh 5                   # development bar (P1–P2 default), deterministic
scripts/run-pluginval.sh                     # default strictness 8
```

Strictness targets: **5** development (P1–P2), **8** standard gate (P3–P5), **10** pre-release gold
(P6/release). Each mode runs **3 consecutive** passes; both modes must pass on all three platforms
at the release bar. Windows uses `run-pluginval.ps1`.

The randomise mode exercises state restoration under randomised test order and time-seeded
fuzzing — defects a fixed seed reproducibly misses.

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

Windows has no signals, and pluginval returns its assertion count directly — so a code in 128…255
there is a *real* failure and must not be retried. `run-pluginval.ps1` therefore classifies
differently from `run-pluginval.sh` by design.

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
