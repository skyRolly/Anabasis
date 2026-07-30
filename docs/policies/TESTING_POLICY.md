# TESTING_POLICY.md

Repository Governance Policy. Test acceptance levels and the release gate.

## Acceptance levels

| Level | Name | What | Where |
|---|---|---|---|
| **1** | Static analysis | Compiler warnings (`juce::juce_recommended_warning_flags`), CodeQL, MSVC `/analyze` | CMake warning flags; `.github/workflows/codeql.yml`, `msvc.yml` |
| **2** | Unit / behaviour | Deterministic DSP assertions + state/parameter compatibility (schema shape, registry snapshot, raw-exact round-trip, legacy migrations, corrupt-state robustness, preset round-trip) | `tests/dsp_tests.cpp` (`AnabasisTests`) + `tests/state_tests.cpp` (`AnabasisStateTests`) |
| **3** | DSP validation | No NaN/Inf/denormals across the feature × oversampling × sample-rate matrix; latency == actual; bypass null; click-free transitions; **ceiling never exceeded**; metering accuracy | `tests/dsp_tests.cpp` |
| **4** | pluginval | VST3 conformance; editor open/close under `xvfb` | `scripts/run-pluginval.sh` / `.ps1` |
| **5** | Manual validation | Audio quality + GUI appearance + the loudness-matched listening test (cannot be judged headlessly) | Load in a DAW |

## Phase-escalating strictness (`DEVELOPMENT_BRIEF.md` §2, §11)

pluginval strictness escalates with the project phase. It is set in **one place** — the
`ANABASIS_PLUGINVAL_STRICTNESS` env at the top of `.github/workflows/build.yml` — so raising it is
a one-line change:

| Phase | Strictness | Rationale |
|---|---|---|
| P1–P2 (skeleton, DSP core) | **5** | development bar; catches gross conformance breakage without blocking on polish |
| P3–P5 (metering, adaptive, UI) | **8** | the standard gate |
| P6 / any release | **10** | pre-release gold standard — **required**, both modes ×3, all three platforms |

Lowering strictness below the phase value is a deliberate act that must be justified in the PR.

## Hard release gate

- **Level 2/3 self-tests must pass** — the headless gate (`scripts/run-tests.sh`) runs **both**
  `AnabasisTests` and `AnabasisStateTests` fail-closed: a *missing* binary fails the gate, so a
  broken build cannot silently pass by producing nothing.
- **pluginval must pass at strictness 10 in BOTH modes on ALL THREE platforms** (Linux, Windows,
  macOS), each mode run as **3 consecutive passes**:
  - **deterministic** (`run-pluginval.sh 10 deterministic`) — a **fixed, nonzero** `--random-seed`,
    so the run is reproducible. **`--random-seed 0` does not pin anything**: pluginval documents 0
    as "generate a random seed" (`Source/PluginTests.h`) and only forwards the flag when it differs
    from that default, so passing 0 is identical to passing nothing. The scripts pass a nonzero
    constant; changing it to 0 silently deletes this mode's only distinguishing property.
  - **randomise** (`run-pluginval.sh 10 randomise`, `--randomise`) — randomised test order, and no
    pinned seed, so each run also draws a fresh one. The two flags are independent: the seed feeds
    the RNG the tests draw from, `--randomise` only shuffles their order. Together they exercise
    state restoration in ways a fixed seed cannot.

  **All are blocking** — no `continue-on-error`; a non-zero pluginval exit fails the job on every
  platform.
- **The §10 acceptance criteria must be met** (see below) — they are release criteria, not
  aspirations.
- Level 5 is **required for final sign-off** but cannot gate CI; a green build + pluginval pass is
  "ready to audition," not "shipped."

## Anabasis measurement gates (`DEVELOPMENT_BRIEF.md` §10)

Beyond the inherited structure, the suites must assert — with the tolerance stated, not "close
enough":

| Gate | Tolerance | Level |
|---|---|---|
| LUFS vs the official **EBU R128** test vectors | ≤ **0.1 LU** | 2/3 |
| True peak vs known inter-sample-peak signals | ≤ **0.1 dB** | 2/3 |
| Output never exceeds the ceiling in true-peak mode | ≤ **0.1 dBTP** | 3 |
| Reported latency == measured latency (impulse response) | exact | 3 |
| No clicks toggling bypass / loudness compensation / oversampling factor | — | 3 |
| Null test with all defaults, no processing engaged | bit-exact (delay-aligned) | 3 |
| Clipper image suppression at 4× oversampling | **measured in dB, recorded** | reported |
| Performance at 48 kHz stereo 4× oversampling | target < ~5% of one modern desktop core; **benchmark environment stated** | reported |
| Loudness-matched blind listening test vs the benchmark, ≥ 5 genres | conclusions + identified gaps | 5 |
| DAW smoke tests: Reaper (Windows) + Logic Pro (macOS/AU) — load, automation, offline render, state restore | pass | 5 |

The measured figures go in `TEST_REPORT.md` **with their method and machine**. A number without
its methodology is not permitted (constraint C2).

## Harness conventions

- Dependency-free console apps, not a test framework: a `check(cond, "what")` counter harness,
  `main()` calling every test, non-zero exit on any failure.
- `AnabasisStateTests` compiles the **real** plugin sources into its own console target so it
  exercises the actual `AudioProcessor` — the editor sources compile but are never instantiated
  (the tests run headlessly and open no window).
- The frozen `tests/fixtures/parameter_registry.snapshot` is re-written **only** via an explicit
  `AnabasisStateTests --write-snapshot` invocation, and only for an *intentional* parameter
  change. Re-freezing to make a red test green is a compatibility break in disguise.

## Rules

1. **Every bug fix ships a regression test** that fails on the old code and passes on the fix.
2. **DSP-policy invariants must have a guarding test** where feasible — see the invariant → test
   map in `DSP_POLICY.md`. An untested invariant is listed as a gap, never left implicit.
3. A pluginval **crash retry** is permitted only for an abnormal termination of the host-side
   validator; a real validation failure fails immediately and is never retried. The boundary is
   **platform-specific**, because the exit-code conventions are:
   **pluginval's own exit code is only ever 0 or 1** — `Source/CommandLine.cpp` routes any nonzero
   internal result through `exitWithError`, which sets the return value to **1**; the failure
   *count* appears in the log text, never in the exit code. (A malformed command-line argument is
   the one exception: `ConsoleApplication::fail (…, -1)` → **255** on POSIX, **-1** on Windows.
   Both scripts build their own arguments, so that code means the *script* is broken; it is
   misclassified as an abnormal termination and still fails, after three wasted retries.)
   Everything above 1 therefore comes from the OS, and the OS conventions differ:
   - **Linux/macOS** — a signal crash is `exit ≥ 128` (128 + signal number). So `< 128` is a real
     failure and fails immediately; `≥ 128` may be retried.
   - **Windows** — there are no signals, so nothing the OS reports lands in 1…255; a code in that
     range came from pluginval itself and is a **real failure** that must **not** be retried —
     including **128…255**, which on the other two platforms would read as a crash. An abnormal
     termination surfaces instead as a Win32 exception code (`≥ 256`, e.g. `0xC0000005`), a
     negative value, or no code at all; those may be retried.

   `scripts/run-pluginval.ps1` therefore classifies differently from `run-pluginval.sh` **by
   design**. Recording the difference here is the point: a reader comparing the gate to the scripts
   must not have to reconstruct it, and the repository's own rule is that a script and the policy
   describing it never diverge silently.
4. **A skipped test category must be visible in this document, not only in a script.** The three
   platforms run the same pluginval test set; no `--skip-*` flag is in use. If an environmental
   limit later forces one — the likeliest is the GPU-less `windows-latest` runner being unable to
   host the editor once P5 exists, which is why the sibling product skips GUI tests there — the
   flag may be added **only** together with a `KNOWN_ISSUES.md` entry stating what is no longer
   verified and on which platform. A gate documented as "uniform and blocking" that quietly skips
   a category on one platform is worse than an honestly narrower gate.
5. **Hostile inputs are part of the suite**, not an afterthought: full-scale square waves,
   inter-sample-peak-heavy material, DC, silence, and automation swept at audio rate. A limiter
   that only holds its ceiling on well-behaved music does not satisfy `DSP_POLICY.md` invariant 4.

## Current status

**TODO (no code yet).** No test exists; the gate activates at P1. This section is updated with the
real suite sizes and their evidence as the phases land.
