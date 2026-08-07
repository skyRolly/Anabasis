# REPOSITORY_MAP.md

Directory and file map with per-component responsibilities. Entries marked **[P*n*]** are
scaffolded but not yet populated — they land in the phase named
(`docs/DEVELOPMENT_BRIEF.md` §11).

## Top level

```
Anabasis/
├── CMakeLists.txt          Build: JUCE FetchContent (9.x, pinned by commit SHA), AnabasisDSP
│                           INTERFACE lib, AnabasisHardening flags, plugin target
│                           (VST3 [+AU on macOS] [+Standalone]), two test console apps.
├── README.md               Project façade (scope, status, quick start, docs nav).
├── CHANGELOG.md            Version history (Keep a Changelog; evidence-cited). Empty until 0.1.0.
├── CLAUDE.md               AI/contributor entry point: mandatory policy pre-read + repo constraints.
├── src/                    Source (wrapper + GUI + DSP core) — P1 skeleton in place.
├── tests/                  Headless self-tests (DSP + state compatibility) and fixtures.
├── worklogs/               Session-local investigation records for future agents (NOT architecture
│                           docs; finalized decisions graduate to ADRs — worklogs are the raw
│                           evidence trail: measurements, rejected alternatives and why).
├── scripts/                setup / build / test / pluginval / docs lint.
├── packaging/              Per-platform install notes + installer assets (linux/, windows/,
│                           macos/) — deferred with the installer set to the first commercial
│                           release (OQ-007, resolved 2026-08-02; v0.1.0 ships plain zips).
├── .github/                CI + security tooling: workflows/ (build + validate on 3 OSes with
│                           retain-then-strip symbol pipeline; CodeQL; MSVC /analyze;
│                           Dependency Review; the weekly non-blocking C++23 canary) and
│                           dependabot.yml (github-actions ecosystem only).
└── docs/                   This documentation library.
```

Of the legal/attribution class, the **factual half is Present since 2026-08-05**: `NOTICE` and
`THIRD_PARTY_LICENSES.md` were produced against the real pinned dependency tree and this build's
own object files (the method is stated inside the inventory). **Since 0.1.1 they ship as
version-named assets on the release page** — `Anabasis-<version>-NOTICE.txt` and
`Anabasis-<version>-THIRD_PARTY_LICENSES.md` — and are deliberately NOT copied into the zips,
the `.pkg` or the Inno payload: the release page is the one carrier every distribution route
passes through, and a loose unversioned copy cannot be told apart from another build's once
extracted (`RELEASE_POLICY.md` §Third-party attribution, amended by **ADR-0021**).
`SUPPORT.md` **landed in 0.1.1** — the internal-testing guide
`DEVELOPMENT_BRIEF.md` §14.2 names, deliberately shorter than the sibling's because that
documentation class restates the legal class and Anabasis has none to restate. The
**owner-legal half** (`EULA.md`, `PRIVACY.md`, `TRADEMARKS.md`) remains absent: its wording is
owner-supplied (C8) and gated with OQ-002/OQ-009 — inventing it would violate constraint C7.

## `src/` — layout (P1 skeleton in the tree; stages marked P2+ are not yet implemented)

| Path | Responsibility |
|---|---|
| `PluginProcessor.{h,cpp}` | VST3/AU/Standalone wrapper: bus layouts, `processBlock`, state save/recall, PDC, A/B compare. |
| `PluginParameters.{h,cpp}` | APVTS layout, `pid::` ID constants, atomic cache, `toEngine` → `EngineParameters`. |
| `InternalState.h` | Host-hidden session/view params (Oversampling, phase mode, offline-render quality, window size, tooltips, animations, meter options). |
| `PresetManager.{h,cpp}` | Factory + user `.anabasis` presets (sound params only) + parameter lock. |
| `MacroEngine.{h,cpp}` | The §5.5 macro mapper and the §5.3 detach/re-engage grammar — message-thread only. |
| `src/gui/` | `PluginEditor.{h,cpp}` (Simple/Advanced UI, render context, timers) plus `LookAndFeel`, the primary knob, GR-history scope, the statistics panel, transfer-curve and EQ-response displays, the spectrum view. |
| `src/dsp/` | Format-agnostic DSP core (`AnabasisDSP` INTERFACE lib) — depends only on `juce_dsp` / `juce_audio_basics`, never on the plugin wrapper. |

`src/dsp/` holds one module per signal-chain stage (`DEVELOPMENT_BRIEF.md` §3–§4), **all
implemented**: `EngineParameters.h` (the wrapper↔engine POD boundary), `AnabasisEngine` (the
chain orchestrator, which also carries input gain, dither, loudness compensation and delta
monitoring inline), `MasteringEQ`, `MasteringComp`, `ClipSat` (ADAA), `LookaheadLimiter`,
`CeilingClamp`, `Latency`, plus the measurement side: `LoudnessMeter` (BS.1770 + LRA),
`RmsMeter`, `TruePeak`, `AdaptiveEngine`, `GrHistoryBuffer` and `ScopeBuffer`. *(This paragraph
read "Planned … modules" from P0 through the 0.1.1 release round — every one of them had
shipped by P3. The table above likewise placed `PluginEditor` at top-level `src/`, which it has
never occupied, and omitted `MacroEngine` entirely.)*

## `tests/`

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
| `workflows/cxx23-canary.yml` | Weekly + `workflow_dispatch` **non-blocking** C++23 early warning: builds the `AnabasisTests` target at C++23 on all three OSes and runs the suite (OQ-006). Never a required check. |
| `workflows/release.yml` | Annotated `vX.Y.Z` tag → fail-closed metadata validation → reused `build.yml` gates → **draft** GitHub Release. Deferred to the first commercial release (**OQ-007**); not present. |
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
│                          architecture, the parameter table (49 at sign-off; 50 live per ADR-0019), macro curves, wireframes.
│                          Answers to the brief; the ADRs it spawned (ADR_INDEX.md) now outrank
│                          it and supersede it section by section as P1-P6 land.
│                          See SOURCE_OF_TRUTH.md §"Where DESIGN.md sits".
├── SOURCE_OF_TRUTH.md, REPOSITORY_MAP.md, OPEN_QUESTIONS.md, HANDOVER.md,
│   DOCUMENTATION_COVERAGE.md, KNOWN_ISSUES.md, FUTURE_RISKS.md, POSTMORTEMS.md,
│   BRAND_CONSISTENCY_CHECKLIST.md, TEST_REPORT.md (measured data + method, grows per phase)
├── user/           end-user class (derived; never evidence): USER_MANUAL.md,
│                   INSTALLATION.md — since 2026-08-05, written against the v0.1.0 surface
├── architecture/   design-decisions/ — ADR_INDEX.md + the ADR files it registers (the index is
│                   the roster; this map does not re-list them, so a new ADR needs no edit
│                   here). THREAD_MODEL.md (implemented model, from ADR-0011)
│                   and PARAMETER_REGISTRY.md (the surface ledger, from ADR-0010 +
│                   the frozen snapshot) exist since P1 close;
│                   REALTIME_SAFETY_AUDIT.md since P2 close; PERFORMANCE_BUDGET.md
│                   since P6 (2026-08-02); COMPATIBILITY_MATRIX.md,
│                   SERIALIZATION_REGISTRY.md and LATENCY_MODEL.md since 2026-08-05
│                   (the DAW-audition target list; the schema-v1 ledger and the
│                   latency reference COMPATIBILITY_POLICY cites).
│                   Still planned, no date claimed: ARCHITECTURE, SIGNAL_FLOW,
│                   DSP_GRAPH_REFERENCE, DSP_ALGORITHMS, API_REFERENCE,
│                   STATE_SERIALIZATION
├── procedures/     BUILD, DEVELOPMENT, CI_CD, TESTING, RELEASE_PROCESS,
│                   RELEASE_COMPATIBILITY_CHECKLIST, TROUBLESHOOTING. PACKAGING arrives
│                   with the OQ-007 installer set (first commercial release)
└── policies/       REALTIME_AUDIO, THREADING, DSP, MODE_AND_ADAPTATION, COMPATIBILITY family,
                    ARCHITECTURE_REVIEW_GATE, ADR, DOCUMENTATION_LIFECYCLE, AI_AGENT,
                    CHANGELOG, TESTING, RELEASE, DEPENDENCY, CODE_STYLE
```

## Deliverables named by the brief

**`docs/DESIGN.md` is signed off (P0 closed 2026-07-31)** — its research evidence trail is
`worklogs/2026-07-30-p0-anamorph-research.md`, and the decisions it carried are now the Accepted
ADRs registered in `ADR_INDEX.md`. `TEST_REPORT.md`
(§10, §12) and the factory preset bank are produced by their respective phases and are not
scaffolded — they carry measured content, and an empty shell would invite it being filled with
estimates (constraint C2).

Evidence [Verified]: the file tree of this repository at the bootstrap commit. Every **[P*n*]**
row is a *plan*, not a claim about existing files.
