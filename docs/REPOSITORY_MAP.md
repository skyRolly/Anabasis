# REPOSITORY_MAP.md

Directory and file map with per-component responsibilities. Entries marked **[P*n*]** are
scaffolded but not yet populated — they land in the phase named
(`docs/DEVELOPMENT_BRIEF.md` §11).

## Top level

```
Anabasis/
├── CMakeLists.txt          [P1] Build: JUCE FetchContent (9.x, pinned by commit SHA), AnabasisDSP
│                           INTERFACE lib, AnabasisHardening flags, plugin target
│                           (VST3 [+AU on macOS] [+Standalone]), two test console apps.
├── README.md               Project façade (scope, status, quick start, docs nav).
├── CHANGELOG.md            Version history (Keep a Changelog; evidence-cited). Empty until 0.1.0.
├── CLAUDE.md               AI/contributor entry point: mandatory policy pre-read + repo constraints.
├── src/                    [P1] Source (wrapper + GUI + DSP core).
├── tests/                  [P1] Headless self-tests (DSP + state compatibility) and fixtures.
├── worklogs/               Session-local investigation records for future agents (NOT architecture
│                           docs; finalized decisions graduate to ADRs — worklogs are the raw
│                           evidence trail: measurements, rejected alternatives and why).
├── scripts/                setup / build / test / pluginval / docs lint.
├── packaging/              [P6] Per-platform install notes + installer assets (linux/, windows/, macos/).
├── .github/                CI + security tooling: workflows/ (build + validate on 3 OSes with
│                           retain-then-strip symbol pipeline; CodeQL; MSVC /analyze;
│                           Dependency Review) and dependabot.yml (github-actions ecosystem only).
└── docs/                   This documentation library.
```

Legal/attribution documents (`NOTICE`, `THIRD_PARTY_LICENSES.md`, `EULA.md`, `PRIVACY.md`,
`TRADEMARKS.md`) and the internal-testing guide (`SUPPORT.md`) are **[P6]** — they are produced
against a real dependency tree and a real binary, and inventing them earlier would violate
constraint C7.

## `src/` — planned layout [P1]

| Path | Responsibility |
|---|---|
| `PluginProcessor.{h,cpp}` | VST3/AU/Standalone wrapper: bus layouts, `processBlock`, state save/recall, PDC, A/B compare. |
| `PluginParameters.{h,cpp}` | APVTS layout, `pid::` ID constants, atomic cache, `toEngine` → `EngineParameters`. |
| `InternalState.h` | Host-hidden session/view params (Oversampling, phase mode, offline-render quality, window size, tooltips, animations, meter options). |
| `PresetManager.{h,cpp}` | Factory + user `.anabasis` presets (sound params only) + parameter lock. |
| `PluginEditor.{h,cpp}` | Simple/Advanced UI, render context, timers. |
| `src/gui/` | `LookAndFeel`, the primary knob, GR-history scope, LUFS/dBTP meters, transfer-curve display, spectrum overlay. |
| `src/dsp/` | Format-agnostic DSP core (`AnabasisDSP` INTERFACE lib) — depends only on `juce_dsp` / `juce_audio_basics`, never on the plugin wrapper. |

Planned `src/dsp/` modules, one per signal-chain stage (`DEVELOPMENT_BRIEF.md` §3–§4):
`EngineParameters.h` (the wrapper↔engine POD boundary), the chain orchestrator, input gain,
EQ, compressor, clipper/saturation (ADAA), limiter (lookahead + true peak), ceiling clamp,
dither, plus the measurement side: LUFS/BS.1770 meter, true-peak meter, loudness compensation,
delta monitoring, feature extraction for the adaptive engine, and a lock-free scope ring.

## `tests/` [P1]

| Path | Responsibility |
|---|---|
| `tests/dsp_tests.cpp` | Headless DSP acceptance suite (`check(cond, "...")` harness; `main` runs all; non-zero exit on any failure). Console target `AnabasisTests`. |
| `tests/state_tests.cpp` | State-compatibility suite — schema shape, parameter-registry snapshot, raw-exact round-trip, legacy migrations, corrupt-state robustness, preset round-trip. Own console target `AnabasisStateTests`, compiling the real plugin sources. |
| `tests/fixtures/` | `parameter_registry.snapshot` (re-frozen only via `AnabasisStateTests --write-snapshot` for INTENTIONAL parameter changes) + frozen legacy session XMLs, one per shipped state format. |

## `scripts/`

| Path | Responsibility |
|---|---|
| `setup-linux.sh` | Ubuntu build dependencies (+ xvfb). |
| `build.sh` | CMake + Ninja build; prints artifact paths. |
| `run-tests.sh` | Runs `AnabasisTests` + `AnabasisStateTests` (fail-closed: a missing binary fails the gate). |
| `run-pluginval.sh` | pluginval on Linux/macOS (strictness + mode args — `deterministic` \| `randomise`, each ×3; signal-only crash retry). |
| `run-pluginval.ps1` | pluginval on Windows (same strictness/mode/×3 structure; waits on the GUI-subsystem process for a trustworthy exit code). |
| `check-docs.py` | Structural lint for the documentation set: GFM table integrity (a block inserted mid-table silently un-tables the rows after it), broken relative links, blockquote lazy continuation, and unclosed code fences (which make the rest of a file render as code *and* exempt it from the other three checks). No arguments = whole repo; `--self-test` pins both directions — the shapes that must be reported and the valid markup that must not — and prints the case count it actually ran, so no figure is duplicated here to go stale; exit 1 on any finding. Run by the **docs** job in `build.yml` on every push, and by hand on documentation-affecting changes. |

## `.github/`

| Path | Responsibility |
|---|---|
| `workflows/build.yml` | 3-OS build + self-tests + pluginval (both modes ×3, **blocking on all three platforms**); retain-then-strip symbol pipeline; fail-closed artifact staging; also callable (`workflow_call`) by a future `release.yml`. Strictness comes from one top-level `env` and escalates by phase. Also carries the **docs** job (`scripts/check-docs.py`), which runs pre-P1 and gates nothing. |
| `workflows/codeql.yml` | CodeQL (`c-cpp` manual build + `actions`); alerts scoped to repo-own code (`paths-ignore: build`). |
| `workflows/msvc.yml` | MSVC `/analyze` → SARIF; JUCE treated as external; path-filtered triggers. |
| `workflows/dependency-review.yml` | Dependency Review on PRs to `main` (GitHub Actions deps; comment on failure only). |
| `workflows/release.yml` | **[P6]** Annotated `vX.Y.Z` tag → fail-closed metadata validation → reused `build.yml` gates → **draft** GitHub Release. |
| `dependabot.yml` | Weekly grouped `github-actions` bumps; JUCE stays manually pinned (`DEPENDENCY_POLICY.md`). |
| `ISSUE_TEMPLATE/` | `bug_report.yml` (test-report form) + `config.yml` (doc links). |

All three build/analysis workflows are guarded by a `preflight` job that skips them while
`CMakeLists.txt` does not exist, so the P0 scaffold does not report a red build *for code that has
not been written*. The guard becomes a no-op the moment P1 lands. The **`docs` job** in `build.yml`
is deliberately outside it and runs on every push — pre-P1 the documentation is the deliverable, so
it is the one thing that *can* legitimately go red before `src/` exists. It gates no build job.

## `docs/` — documentation library

```
docs/
├── DEVELOPMENT_BRIEF.md   The owner-supplied product spec (Part I) + the inherited engineering
│                          standard (Part II). See SOURCE_OF_TRUTH.md §"Where the product brief sits".
├── DESIGN.md              The P0 design deliverable (brief §11/§24), SIGNED OFF 2026-07-31:
│                          architecture, the 49-parameter table, macro curves, wireframes.
│                          Answers to the brief; the eleven ADRs it spawned now outrank it and
│                          supersede it section by section as P1-P6 land.
│                          See SOURCE_OF_TRUTH.md §"Where DESIGN.md sits".
├── SOURCE_OF_TRUTH.md, REPOSITORY_MAP.md, OPEN_QUESTIONS.md, HANDOVER.md,
│   DOCUMENTATION_COVERAGE.md, KNOWN_ISSUES.md, FUTURE_RISKS.md, POSTMORTEMS.md,
│   BRAND_CONSISTENCY_CHECKLIST.md
├── user/           [P6] end-user class: USER_MANUAL, INSTALLATION
├── architecture/   design-decisions/ — ADR_INDEX.md + ADR-0001…0011 (all Accepted
│                   2026-07-31). [P1–P2] the descriptive set:
│                   ARCHITECTURE, SIGNAL_FLOW, DSP_GRAPH_REFERENCE, DSP_ALGORITHMS,
│                   THREAD_MODEL, API_REFERENCE, PARAMETER_REGISTRY/REFERENCE,
│                   SERIALIZATION_REGISTRY, STATE_SERIALIZATION, LATENCY_MODEL,
│                   PERFORMANCE_BUDGET, REALTIME_SAFETY_AUDIT, COMPATIBILITY_MATRIX
├── procedures/     BUILD, DEVELOPMENT, CI_CD, TESTING, RELEASE_PROCESS,
│                   RELEASE_COMPATIBILITY_CHECKLIST, TROUBLESHOOTING. [P6] PACKAGING
└── policies/       REALTIME_AUDIO, THREADING, DSP, MODE_AND_ADAPTATION, COMPATIBILITY family,
                    ARCHITECTURE_REVIEW_GATE, ADR, DOCUMENTATION_LIFECYCLE, AI_AGENT,
                    CHANGELOG, TESTING, RELEASE, DEPENDENCY, CODE_STYLE
```

## Deliverables named by the brief

**`docs/DESIGN.md` is signed off (P0 closed 2026-07-31)** — its research evidence trail is
`worklogs/2026-07-30-p0-anamorph-research.md`, and the decisions it carried are now the eleven
Accepted ADRs. `TEST_REPORT.md`
(§10, §12) and the factory preset bank are produced by their respective phases and are not
scaffolded — they carry measured content, and an empty shell would invite it being filled with
estimates (constraint C2).

Evidence [Verified]: the file tree of this repository at the bootstrap commit. Every **[P*n*]**
row is a *plan*, not a claim about existing files.
