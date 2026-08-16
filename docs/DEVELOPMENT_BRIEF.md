# RollyTech Anabasis — Mastering Loudness Maximizer: Development Brief

> **How to use:** hand this entire document to Claude Code as the task prompt.
> **Repository access:** (1) Anamorph repo — read-only reference: `<Anamorph repo URL>`; (2) this plugin's new repo — working directory: `<Anabasis repo URL>`.
> **Naming:** the plugin is named **Anabasis** (Greek, "an ascent / a going up" — sharing the *Ana-* root with Anamorph as a product family). If the name changes, do a global replace.

---

## 0. Your Role and Overall Goal

You are a senior audio DSP engineer and JUCE plugin developer. You are building the second product for the brand **RollyTech**: **Anabasis**, a stereo mastering loudness maximizer that forms a product line together with the existing **Anamorph** (stereo width / stereo field expander).

Three governing principles:

1. **Research before you build.** Read the Anamorph repository thoroughly before writing any code. Produce a design document and an open-questions list (see §11, phase P0) and wait for my sign-off before implementing.
2. **Sonic quality comes first.** The "one-knob loudness" experience must match and ideally exceed Musik Hack's **Master Plan** in transparency and musicality.
3. **Engineering quality.** Realtime-safe audio code, passing pluginval, reproducible cross-platform builds, and verifiable tests.

---

## 1. Brand and Product-Line Consistency

### 1.1 What to learn from the Anamorph repository (during P0)

- UI framework and component architecture (custom `LookAndFeel`, control classes, layout system).
- Brand visual system: colour palette, typography, logo usage, spacing rules.
- **About page layout, Settings page layout, and Bypass button placement** — these three must remain structurally consistent with Anamorph.
- Preset system and A/B comparison interaction logic.
- Build scripts, CMake structure, and the phased pluginval strategy.

### 1.2 Consistency rules

- **Inherit:** overall frame layout, About/Settings page structure, Bypass placement, typography and brand colour system, preset and A/B interaction conventions.
- **Differentiate:** the main view must express the character of a maximizer — the visual focus is one large primary knob plus metering visualisation (gain-reduction history, LUFS meters), not Anamorph's stereo field display. A distinct accent colour within the shared design language is encouraged to separate the two products.
- **Code reuse:** you may copy and adapt code from the Anamorph repository (first-party assets). Assess whether it is worth extracting a shared UI module (e.g. `rollytech-ui`) and give a recommendation in the design document, but prioritise shipping on schedule.
- **Do not:** copy any third-party code or assets bound by copyright or restrictive licences (GPL code in particular). Master Plan and Ozone serve only as behavioural benchmarks and visual-style references — no reverse engineering of any kind.

---

## 2. Tech Stack and Engineering Constraints

- **Framework: JUCE 9.** JUCE 9 was released on 21 July 2026. **Before starting, check the JUCE GitHub repository (`github.com/juce-framework/JUCE`) releases/tags for the newest stable 9.x point release and pin the project to that exact tag** (record the tag in `README.md`; do not track `develop`). If a newer major version has appeared by the time you start, report it and ask before adopting it.
  - JUCE 9 features directly relevant to this project: the new SVG parser with substantially better spec conformance (radial gradients, blend modes, clip paths, stroked text), variable-font support, and the removal of `juceaide`'s UI dependencies, which makes headless Linux CI builds straightforward. Prefer vector/SVG assets and variable fonts for the UI.
  - Confirm which JUCE licence tier Anamorph ships under and keep this project on the same tier. Flag any licensing implication before it becomes a release blocker.
- **Language: C++20** as the project baseline. See §2.1 for the forward-compatibility policy.
- **Build:** CMake ≥ 3.22, structured consistently with Anamorph.
- **Plugin formats:** VST3 (primary) **plus AU (macOS, required for Logic Pro)**. Standalone optional, for debugging convenience.
- **Platforms:** Windows x64; macOS universal (arm64 + x86_64); Linux headless build must succeed for CI, following Anamorph's existing workflow.
- **Realtime safety:** no allocations, no locks, and no system calls on the audio thread. All parameters smoothed to prevent zipper noise. Denormal protection (FTZ/DAZ). Latency introduced by lookahead and oversampling must be reported accurately to the host for correct plugin delay compensation (PDC).
- **State management:** `AudioProcessorValueTreeState` (APVTS). Versioned state serialisation. Simple and Advanced modes share a single parameter model (see §5.3 — this is a key architectural constraint).
- **CI:** build + unit tests + pluginval. Escalate pluginval strictness by phase (level 5 early, level 10 before release), following Anamorph's approach.

### 2.1 C++ standard policy

- Set `CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`.
- Do **not** use C++20 modules — they remain a build-system liability in plugin projects.
- Where a C++23 library feature would clearly improve the code (`std::expected`, `std::mdspan`, `std::float32_t`, `[[assume]]`, `std::print` in test tooling), guard it behind feature-test macros (`__cpp_lib_expected`, `__has_include(<mdspan>)`, …) and a thin first-party abstraction, so that raising the baseline later is a localised change rather than a rewrite.
- Add a **non-blocking CI "canary" job** that builds the project at C++23 on all three platforms. Its purpose is early warning, not gating; a failure must never block the main pipeline. Report its status in each phase summary.
- Do not target C++26.

---

## 3. Signal Chain

Fixed order for v1:

```
Input Gain → EQ (pre by default) → Compressor → Clipper + Saturation (colour)
→ Limiter (lookahead + true peak) → Ceiling → Dither → Output
```

Requirements:

- **Loudness-compensated monitoring and loudness-matched bypass**, so "louder" and "better" can be told apart honestly. This is also the core tool you will use to evaluate your own adaptive algorithm.
- **Delta monitoring**: audition the difference signal (what the processing is removing) in isolation.
- **Sidechain HPF**: an adjustable 20–300 Hz high-pass on the compressor and limiter detectors, to prevent low-frequency content from causing pumping.

---

## 4. Per-Stage DSP Specification (parameters exposed in Advanced Mode)

### 4.1 Compressor (mastering glue)

Ratio 1.1–4:1 (default ≈ 1.5:1), Threshold, Attack 5–100 ms, Release 50–1000 ms plus Auto, Knee, RMS/Peak detector switch, Mix (parallel compression), gain-reduction meter.

### 4.2 Clipper / Saturation (colour)

- **Continuously variable curve**: hard ↔ soft (knee/shape morph), with a live curve visualisation.
- Drive (with automatic level compensation), Mix.
- **Colour models**: Clean / Tape / Tube / Transistor, with control over odd/even harmonic balance and a Tone control (spectral tilt of the colouration).
- **Anti-aliasing**: covered by global oversampling; use antiderivative antialiasing (ADAA) for the soft-clip stage.

### 4.3 Limiter

- Gain/Threshold, Ceiling (−20…0 dB, dBTP-aware), Lookahead 0.5–10 ms (default ≈ 2 ms).
- **Release 1–1000 ms plus Auto**: program-dependent, using a dual/multi-stage envelope — a fast stage to catch transients and a slow stage to hold the underlying level steady.
- **Style**: Transparent / Punchy / Loud (different attack shaping and envelope strategies).
- Stereo Link 0–100%; transient-preservation amount.
- **True-peak mode**: peak detection at ≥ 4× oversampling, compliant with ITU-R BS.1770-4.
- A final safety clamp: the output must never exceed the ceiling under any conditions.

### 4.4 EQ (spectral control)

- Tilt (±3 dB, pivot ≈ 700 Hz) + low shelf + high shelf + 2 × bell (Freq/Gain/Q).
- Position switch: Pre (default) / Post, i.e. before or after the limiter.
- Static EQ is sufficient for v1; dynamic EQ goes on the roadmap.

### 4.5 Output Stage

**Dither**: Off / 16-bit / 24-bit, TPDF with optional noise shaping. Off by default; intended for final-export use.

---

## 5. Simple Mode (the default view)

### 5.1 Controls

- **One large primary knob**: Loudness / Push (0–100).
- A small number of secondary controls: **Ceiling**, **Character** (Clean ↔ Colour, continuous), **Tone** (dark ↔ bright).
- Plus: loudness-compensated monitoring toggle, live output LUFS readout.

### 5.2 Adaptive Engine — the heart of this plugin

- **Realtime feature extraction**: short-term LUFS, crest factor, spectral tilt/centroid, transient density.
- **Macro mapping from the primary knob to per-stage parameters:**
  - Low range: mostly transparent limiting with light compression.
  - Mid range: bring in the clipper to absorb transients, easing the load on the limiter.
  - High range: add saturation/colour, and apply a dynamic tilt to suppress harshness in the highs.
  - Throughout: continuously trim release, stereo link, and sidechain HPF according to programme content.
- **Adaptation must be slow and smooth**: second-scale time constants plus hysteresis, with no audible modulation whatsoever. Provide a **Freeze** control to lock the current adaptive state.
- **Learn**: an optional button — play a passage, analyse it, and fix the internal reference targets. Keep the interaction consistent with Anamorph's Learn / Auto Gain so the family experience stays unified.

### 5.3 Mode-switching consistency (key architectural decision)

- Simple is a **macro layer on top of the Advanced parameters**; both share one parameter model.
- **Switching between Simple and Advanced must not change the sound at the moment of the switch.**
- If the user has edited parameters manually in Advanced and then returns to Simple, decide how macro values and manual values coexist. Argue for a strategy in the design document before implementing (suggested directions: macro takes precedence on return to Simple with a clear notice, or offer a "carry over" option).

### 5.4 Benchmark and Evaluation

- Benchmark behaviour against Musik Hack **Master Plan**: same source material, loudness-matched comparison.
- Evaluation axes: achievable loudness ceiling, distortion onset threshold, punch retention, tonal shift.
- Produce a formal listening-test report (see §10).

---

## 6. Metering and Visualisation

- **LUFS**: momentary / short-term / integrated (BS.1770-4 / EBU R128, with gating). Verify accuracy against EBU test tones; error ≤ 0.1 LU.
- **True peak (dBTP)** and **PLR** (true peak − integrated LUFS).
- **Gain-reduction history**: scrolling waveform with the gain-reduction trace overlaid (10–30 s window); Pro-L 2's presentation is a reasonable reference.
- **Spectrum**: overlaid input/output display, dismissible.
- **Streaming target lines**: Spotify −14, Apple Music −16, YouTube −14, club/CD, etc., plus a **loudness-penalty estimate** showing how far each platform will pull the track back down in dB.

---

## 7. Settings Page (layout aligned with Anamorph)

- **Oversampling**: Off / 2× / 4× / 8× / 16×. Phase mode: minimum phase (low latency) / linear phase. **Offline-render quality**: Follow / Force Max (automatically maximise during bounce).
- **UI Animation** toggle; **Tooltips** toggle; UI scaling (80–200%); metering options (target-line selection, true-peak toggle).
- Organise everything else the way Anamorph does.

---

## 8. UI/UX Requirements

- **Style**: modern, flat, data-visualisation-driven; the overall character should reference iZotope Ozone 12. **Skeuomorphic hardware-panel design is explicitly forbidden.**
- **Animation**: fluid but restrained (meter and curve interpolation, mode-switch transitions), targeting 60 fps. Everything must respect the UI Animation toggle; disabling it must not affect functionality in any way.
- Vector drawing throughout, resizable, HiDPI-aware, with window size persistence. Take advantage of JUCE 9's improved SVG rendering and variable-font support.
- **Tooltips** on every control, governed by the toggle.
- Accessibility: complete parameter and automation names, keyboard operability, colour-blind-safe metering palette.
- **Layout**: in Simple Mode the large knob is the unambiguous visual focus. In Advanced Mode, use zones (Comp / Clip / Limiter / EQ) plus a shared metering strip along the bottom. Bypass sits where it sits in Anamorph.

---

## 9. Presets and State

- At least 12 factory presets, named by genre/purpose (e.g. Transparent Master, Loud Pop, EDM Club, Vocal Forward, Tape Glue).
- User presets; A/B comparison with the same interaction model as Anamorph.
- **Parameter lock**: at minimum, Ceiling must be lockable so that browsing presets does not change it.

---

## 10. Testing and Acceptance Criteria

- pluginval **level 10** fully green (Windows VST3, macOS VST3 + AU).
- Unit tests:
  - LUFS metering against the official EBU R128 test vectors, error ≤ 0.1 LU.
  - True peak against known inter-sample-peak test signals, error ≤ 0.1 dB.
  - In true-peak mode, output never exceeds the ceiling (tolerance ≤ 0.1 dBTP).
  - No clicks or pops when toggling bypass, loudness compensation, or oversampling factor. Reported latency matches measured latency (verified by impulse response).
  - Null test passes with all defaults and no processing engaged.
- **Aliasing measurement**: with 1 kHz sine and sweep input, quantify image suppression from the clipper at 4× oversampling; record measured dB figures in the report.
- **Performance budget**: at 48 kHz, stereo, 4× oversampling, target under roughly 5% of a single core on a modern desktop CPU. State the benchmark environment in the report.
- **Listening-test report**: loudness-matched blind comparison against Master Plan across at least five genres, with conclusions and identified gaps.
- **DAW smoke tests**: at least one full pass each in Reaper (Windows) and Logic Pro (macOS / AU) — loading, automation, offline render, state restoration.

---

## 11. Milestones (sequential; submit a summary at the end of each phase)

| Phase | Content | Exit criteria |
|---|---|---|
| **P0 Research & design** | Read Anamorph; produce `DESIGN.md` (architecture, full parameter table, draft macro-mapping curves, UI wireframes) and `OPEN_QUESTIONS.md` | Wait for my sign-off before P1 |
| **P1 Skeleton** | CMake/JUCE project, parameter model, pass-through chain plus basic limiter | pluginval L5 passes |
| **P2 DSP core** | Limiter (true peak) / Clipper (oversampling + ADAA) / Compressor / EQ / Dither | Per-module unit tests pass |
| **P3 Metering engine** | LUFS / true peak / PLR / GR history, loudness compensation, delta monitoring | Accuracy tests meet spec |
| **P4 Simple adaptive engine** | Feature extraction, macro mapping, Learn, mode-switch consistency | Verified: switching does not change the sound |
| **P5 UI** | Full Simple + Advanced interface, animation, Settings, tooltips | Brand-consistency checklist against Anamorph passes item by item |
| **P6 Polish & release** | Presets, performance optimisation, pluginval L10, DAW matrix, documentation | All acceptance criteria in §10 met |

---

## 12. Deliverables

- Source plus CMake build scripts (`README.md` covering Windows, macOS, and Linux headless builds, and the pinned JUCE tag).
- `DESIGN.md`, `OPEN_QUESTIONS.md`, `CHANGELOG.md`, `TEST_REPORT.md` (including listening-test notes and measured aliasing and performance data).
- Factory preset bank.
- Versioning starts at **v0.1.0**, following semantic versioning.

---

## 13. Communication Rules

- Wherever the specification is ambiguous, write the question into `OPEN_QUESTIONS.md` and ask. **Do not guess at key decisions** — for example, changing the signal-chain order, substantially altering parameter ranges, or deviating structurally from Anamorph's UI.
- At the end of each phase: a summary of changes, the plan for the next phase, and current risks.
- Before introducing any third-party code or asset, state its licence and get my approval.

---

## Appendix: v2 Roadmap (out of scope now; leave architectural room)

- **Codec preview**: AAC / Opus lossy-encode audition, plus post-encode true-peak headroom checking.
- **Reference matching**: import a reference track and match loudness and spectral tilt.
- **Dynamic EQ** and **multiband limiting**.

---
---

# Part II — Inherited Engineering Standard (extracted from the Anamorph repository)

> **Status of this part.** Everything above (§0–§13 + the v2 Roadmap appendix) is the original
> brief and is unchanged. Part II is **purely additive**: it records the engineering conventions,
> governance system and reusable workflows that already exist in **Anamorph**, so Anabasis starts
> from the same standard instead of rediscovering it. Where a convention cannot be carried over
> unchanged (different DSP, different C++ baseline, no shipped history yet), the delta is stated
> explicitly in §23.
>
> Part II is a *summary with pointers*. The binding, normative versions of these rules live in
> this repository under [`docs/policies/`](policies/) — those files, not this brief, are what an
> agent must obey. Read [`../CLAUDE.md`](../CLAUDE.md) first.

---

## 14. Documentation authority system (inherit wholesale)

Anabasis inherits Anamorph's documentation governance model. It is the single highest-value thing
to copy, because it is what keeps an AI-driven codebase from drifting.

### 14.1 Authority order

Highest wins; the lower source is corrected to match
([`docs/SOURCE_OF_TRUTH.md`](SOURCE_OF_TRUTH.md)):

```
Source Code → Verified Tests → ADR → Policy → Architecture → Procedures → README
```

ADR is the final *decision* record; Policy is the *enforcement* of decisions. A Policy outranks
descriptive Architecture. A Policy change is enacted only by a new/updated ADR.

### 14.2 Four documentation classes, kept separate

| Class | Location | Role |
|---|---|---|
| **Developer** | `docs/` (architecture, procedures, policies, status) + `CLAUDE.md` | binding + descriptive; the evidence chain |
| **User** | `docs/user/` | derived; never evidence |
| **Internal / testing** | `SUPPORT.md`, `.github/ISSUE_TEMPLATE/` | derived; restates the legal class, never diverges from it |
| **Legal** | `NOTICE`, `THIRD_PARTY_LICENSES.md`, `EULA.md`, `PRIVACY.md`, `TRADEMARKS.md` | `NOTICE` + `THIRD_PARTY_LICENSES.md` are authoritative for third-party attribution facts |

`worklogs/` sits **outside** all four classes: session-local investigation records for future
agents. Raw evidence trail; finalized decisions graduate to ADRs. Worklogs are never cited as
policy.

### 14.3 Confidence levels — used on every factual claim

| Level | Meaning |
|---|---|
| **Verified** | Provable from current source code, or code + a test case. |
| **Partially Verified** | Supported by README / commit / PR / code comment, not fully provable from code alone. |
| **Unverified** | No sufficient evidence; could be true but unproven (real-DAW behaviour, performance numbers). |
| **Not Supported** | A deliberate, evidence-backed exclusion (e.g. AAX). Distinct from Unverified. |

### 14.4 Evidence citation format (mandatory for historical / design / incident / risk claims)

```
Evidence [Verified]:
- Source: src/dsp/LimiterEngine.cpp:472-899
- Test:   tests/dsp_tests.cpp :: testTruePeakNeverExceedsCeiling
- Commit: 6a24b82
```

At least one source is mandatory. **Never infer history by reasoning backwards from current code.**

---

## 15. Binding policies and the Hard-Stop gate (inherit wholesale)

### 15.1 The Hard-Stop conditions

An agent must **stop and request human review** — not proceed, and not merge on a green build —
when it detects any of:

- parameter ID rename/removal
- serialization schema change
- threading-model change
- DSP signal-order change
- reported-latency change
- a conflict with an `Accepted` ADR

A passing build / test suite / pluginval run does **not** clear a Hard Stop. Only human review
does. See [`docs/policies/ARCHITECTURE_REVIEW_GATE.md`](policies/ARCHITECTURE_REVIEW_GATE.md) and
[`docs/policies/AI_AGENT_POLICY.md`](policies/AI_AGENT_POLICY.md).

The Architecture Review Gate additionally covers: plugin-format changes (VST3/AU/AAX/Standalone)
and build-system changes (CMake structure, JUCE version/pin, dependency set).

### 15.2 The named working constraints (C-numbers, as used throughout Anamorph's docs)

| ID | Constraint |
|---|---|
| **C1** | ADRs are **evidence-driven** — created only when backed by code/test/commit/PR evidence. No predefined quota. |
| **C2** | **Never invent numbers.** A performance figure without a recorded machine, matrix position and methodology is worse than an honest `TODO`. |
| **C4** | **Re-scan the workspace before each phase / on resume.** The filesystem is the authoritative execution state, not chat history. Continue incrementally; never regenerate existing work. |
| **C5** | **Update docs incrementally.** Smallest change that re-syncs; preserve hand-written content; never regenerate a doc wholesale unless explicitly asked. If a structural rewrite seems necessary — stop and ask. |
| **C6** | **Report drift, never silently fix it.** If docs and code disagree, state the drift with an evidence reference *before* editing. |
| **C7** | **Mark unknowns `TODO`, do not invent them.** No invented risks, owners, or status fields. |
| **C8** | **User-visible text is specified, never invented.** Tooltips, labels, menu items, dialog strings are product wording owned by the maintainer. Implementing a behaviour change does not license announcing it in the UI. |

*(`C3` is referenced nowhere in the Anamorph repository and is therefore not carried over. Do not
invent a definition for it.)*

### 15.3 Policy set carried into Anabasis

All of these exist in [`docs/policies/`](policies/), adapted to Anabasis:

| Policy | Priority / class | What it locks down |
|---|---|---|
| `REALTIME_AUDIO_POLICY.md` | System, P1 | audio thread is deterministic, lock-free, allocation-free |
| `THREADING_POLICY.md` | System, P2 | the only permitted cross-thread paths + atomic ordering rules |
| `DSP_POLICY.md` | System, P3 | the binding DSP invariants (chain order, ceiling clamp, identity at zero, …) |
| `MODE_AND_ADAPTATION_POLICY.md` | System, P4 — **Anabasis-specific** | Simple = macro layer over Advanced; switching must not change the sound; adaptation is slow, smooth, freezable |
| `COMPATIBILITY_POLICY.md` (+ `PARAMETER_` / `SESSION_` subsets) | highest compatibility authority | saved sessions must reload to the same sound, forever |
| `ARCHITECTURE_REVIEW_GATE.md` | Governance | what cannot be auto-merged |
| `AI_AGENT_POLICY.md` | Governance | the agent collaboration codex |
| `ADR_POLICY.md` | Governance | when a decision must become an ADR |
| `DOCUMENTATION_LIFECYCLE_POLICY.md` | Governance | the code-change → docs-to-update trigger map |
| `CHANGELOG_POLICY.md` | Governance | Keep a Changelog + evidence citation + no invented history |
| `TESTING_POLICY.md` | Governance | the 5 acceptance levels and the hard release gate |
| `RELEASE_POLICY.md` | Governance | the preconditions a version must satisfy to ship |
| `DEPENDENCY_POLICY.md` | Governance | JUCE pinned by immutable commit SHA; bumps are gated |
| `CODE_STYLE.md` | Governance | C++ conventions |

---

## 16. ADR system (inherit wholesale)

- Location: `docs/architecture/design-decisions/ADR-NNNN-<slug>.md`, registered in
  `ADR_INDEX.md`. **An unregistered ADR is invalid.**
- Required fields: **Status** (Proposed / Accepted / Deprecated / Superseded), Context, Problem,
  Options, Decision, Consequences, Related code, Evidence + confidence.
- **Append-only.** Never delete an ADR; a reversed decision adds a new ADR and marks the old one
  `Superseded` (cross-linked).
- Mandatory for: DSP signal-flow, parameter semantics, threading, plugin formats, build
  architecture, state serialization, latency, oversampling strategy, DSP-algorithm replacement.

Anabasis starts its ADR numbering at **ADR-0001** — the numbers are *not* shared with Anamorph.
The P0 design document is expected to spawn the first batch (signal-chain order, oversampling
strategy, macro-layer architecture, true-peak ceiling guarantee, latency contract).

---

## 17. Repository layout (inherit; already scaffolded)

```
Anabasis/
├── CMakeLists.txt        Build: JUCE FetchContent (SHA-pinned), AnabasisDSP INTERFACE lib,
│                         AnabasisHardening flags, plugin target (VST3 [+AU macOS] [+Standalone]),
│                         test targets.                                       [added at P1]
├── README.md             Project façade (features, status, quick start, docs nav).
├── CHANGELOG.md          Version history (Keep a Changelog; evidence-cited).
├── CLAUDE.md             AI/contributor entry point: mandatory policy pre-read + repo constraints.
├── src/                  Source: wrapper + GUI + format-agnostic DSP core.
│   ├── dsp/              AnabasisDSP INTERFACE lib — no dependency on the plugin wrapper.
│   └── gui/              LookAndFeel + visualisers (GR history, LUFS meters, curve display).
├── tests/                Headless self-tests (DSP + state compatibility) and fixtures.
├── worklogs/             Session-local investigation records (raw evidence trail, not architecture).
├── scripts/              setup-linux / build / run-tests / run-pluginval{.sh,.ps1}.
├── packaging/            Per-platform install notes + installer assets.        [populated at P6]
├── .github/              CI + security tooling (workflows/, dependabot.yml, ISSUE_TEMPLATE/).
└── docs/                 The documentation library (this file lives here).
```

`docs/` mirrors Anamorph exactly: root status docs + `user/` + `architecture/` (incl.
`design-decisions/`) + `procedures/` + `policies/`.

---

## 18. Build-system conventions (inherit, with the §2.1 C++20 delta)

Copy the structural pattern from Anamorph's `CMakeLists.txt`:

1. **`cmake_minimum_required(VERSION 3.22)`**, `project(Anabasis VERSION 0.1.0 LANGUAGES C CXX)`.
2. **C++ standard set explicitly**: `CMAKE_CXX_STANDARD 20` (Anamorph is 17 — this is a deliberate
   Anabasis delta, §2.1), `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`.
3. **JUCE via `FetchContent`, pinned to the tag's IMMUTABLE COMMIT SHA** — never a branch, never a
   mutable tag *name*. Two cache variables: `ANABASIS_JUCE_VERSION` (human-readable) and
   `ANABASIS_JUCE_TAG` (the SHA). An `ANABASIS_JUCE_PATH` escape hatch points at a local checkout
   for offline/restricted CI. *(Anamorph learned this the hard way — ADR-0022.)*
4. **A hardening INTERFACE target** (`AnabasisHardening`) carrying behaviour-neutral binary
   hygiene as usage requirements, linked `PUBLIC` so it reaches every format target's compile
   *and* link, and the test targets too — so the self-tests validate the shipped flag
   configuration:
   - MSVC: `/guard:cf`, Release `/Zi` + `/DEBUG /OPT:REF /OPT:ICF`, `/DYNAMICBASE /NXCOMPAT`.
   - GCC/Clang: `-fstack-protector-strong -ffunction-sections -fdata-sections`, Release `-g`;
     link `--gc-sections` + `-z relro -z now -z noexecstack` (Linux) / `-dead_strip` (macOS).
   - **Deliberately absent:** `-O3` / `-ffast-math` / extra LTO — numerics-affecting flags are
     frozen by DSP policy. Stripping happens in CI packaging, never locally.
5. **A format-agnostic DSP core as an `INTERFACE` library** (`AnabasisDSP`) whose sources compile
   into each final target. Deliberately *not* a `STATIC` lib — that would duplicate JUCE module
   object code and break linking. The DSP core depends only on `juce_dsp` / `juce_audio_basics`.
6. **Options named `ANABASIS_*`**: `ANABASIS_BUILD_TESTS` (ON), `ANABASIS_BUILD_STANDALONE` (ON),
   `ANABASIS_JUCE_PATH`, `ANABASIS_JUCE_TAG`, `ANABASIS_BUILD_NUMBER`.
7. **Compile definitions are part of the build contract**: `ANABASIS_VERSION_STRING`,
   `ANABASIS_BUILD_NUMBER`, `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`,
   `JUCE_VST3_CAN_REPLACE_VST2=0`, `JUCE_DISPLAY_SPLASH_SCREEN=0`, `JUCE_REPORT_APP_USAGE=0`,
   `JUCE_STRICT_REFCOUNTEDPOINTER=1`.
8. **One source list, two consumers.** The wrapper/GUI source list is a single CMake variable used
   by both the plugin target and the state-test console target, so a new file can never be added
   to only one of them.
9. `juce::juce_recommended_config_flags` + `_lto_flags` + `_warning_flags`, and the build stays
   warning-free.

---

## 19. CI/CD conventions (inherit; already scaffolded in `.github/`)

### 19.1 `build.yml` — build & validate

- Triggers: `push` to `"**"`, `pull_request`, `workflow_dispatch`, **and `workflow_call`** so the
  release pipeline can reuse the exact same matrix and gates. `permissions: contents: read`.
- Matrix: `ubuntu-latest` (VST3 + Standalone), `windows-latest` (VST3 + Standalone, MSVC
  multi-config), `macos-14` (universal `arm64;x86_64` VST3 + **AU** + Standalone).
- **Validation is uniform and BLOCKING on all three platforms.** No `continue-on-error`.
- **pluginval runs in two modes, 3 consecutive passes each**: `deterministic` (a fixed **nonzero**
  `--random-seed` — 0 is pluginval's "generate a random seed" sentinel and pins nothing) **and**
  `randomise` (`--randomise` — randomised test order, unpinned seed, which catches
  state-restoration defects a fixed seed misses).
- **Both** pluginval steps carry the same guard, so a *deterministic* failure never **skips** the
  randomise step — both modes always report independently, and the job fails if either fails —
  while a build that produced nothing to validate skips both. The guard is
  `if: ${{ !cancelled() && steps.build.outcome == 'success' }}` on Windows and macOS, and
  `… && steps.strip.outcome == 'success'` additionally on Linux, where the strip runs *before*
  validation: `steps.build.outcome` stays `success` when the strip fails, so without that term the
  gate could validate bytes that will never ship.
- **Retain-then-strip symbol pipeline**: every platform generates full debug info, uploads it as a
  separate `Anabasis-<OS>-debug` artifact for crash symbolication, and ships **stripped** public
  binaries. On Linux the strip runs *before* pluginval, so the gate validates the exact bytes
  users receive.
- **Fail-closed artifact hygiene**: customer uploads are gated on the self-tests *and* the
  staging/strip steps having **succeeded** (never `if: always()`); the staging step self-validates
  (no `.symtab`, no `.debug`/`.pdb`/`.dSYM` in the public copy) and asserts the VST3 entry point
  (`GetPluginFactory`) survives the strip.

### 19.2 Phased pluginval strictness (Anabasis-specific, per §2 and §11)

The workflow reads strictness from a single top-level `env` so the phase escalation is one edit:

```yaml
env:
  ANABASIS_PLUGINVAL_STRICTNESS: 5   # P1–P2: 5 · P3–P5: 8 · P6/release: 10
```

The **release gate is strictness 10, both modes ×3, on all three platforms** — do not ship below
that (`docs/policies/TESTING_POLICY.md`).

### 19.3 Security tooling (all four inherited)

| Workflow | Purpose | Key convention |
|---|---|---|
| `codeql.yml` | `c-cpp` + `actions` analysis | build-mode **manual** (JUCE arrives via FetchContent, so a no-build analysis resolves almost nothing); `paths-ignore: build` keeps FetchContent'd JUCE out of the alerts; docs-only changes skip the workflow |
| `msvc.yml` | MSVC `/analyze` → SARIF | a real build is required (juceaide generates consumed files); JUCE treated as external via `ignoredIncludePaths`/`ignoredTargetPaths`; path-filtered triggers; analyse **Release** (the shipped configuration) |
| `dependency-review.yml` | PR dependency gate | `comment-summary-in-pr: on-failure` only |
| `dependabot.yml` | weekly grouped `github-actions` bumps | **JUCE stays manually pinned** — CMake is not a Dependabot ecosystem and a JUCE bump is a review-gated Build System change |

### 19.4 `release.yml` (added at P6)

Annotated `vX.Y.Z` tag → fail-closed metadata validation (tag ⇄ `CMakeLists.txt` version ⇄
`CHANGELOG.md` section, annotated-tag check) → `build.yml` via `workflow_call` → **draft** GitHub
Release with versioned artifacts + SHA-256 sums + a traceability manifest. `workflow_dispatch` is
a no-release rehearsal. **Publishing the draft stays a manual maintainer action.**

---

## 20. Testing conventions (inherit, extended with Anabasis's measurement gates)

### 20.1 The five acceptance levels

| Level | Name | What |
|---|---|---|
| 1 | Static analysis | compiler warnings (`juce_recommended_warning_flags`), CodeQL, MSVC `/analyze` |
| 2 | Unit / behaviour | deterministic DSP assertions + state/parameter compatibility |
| 3 | DSP validation | no NaN/Inf/denormals across the feature matrix; latency == actual; bypass null; click-free transitions |
| 4 | pluginval | VST3 conformance; editor open/close under `xvfb` |
| 5 | Manual validation | audio quality + GUI appearance — **cannot be judged headlessly** |

A green build + pluginval pass is **"ready to audition," not "shipped."**

### 20.2 Harness style

Dependency-free console apps, not a test framework: a `check(cond, "what")` counter harness,
`main()` calls every test, non-zero exit on any failure. Two binaries, both required
(fail-closed — a *missing* binary fails the gate):

- `AnabasisTests` — DSP acceptance suite (`tests/dsp_tests.cpp`).
- `AnabasisStateTests` — state/parameter-compatibility suite (`tests/state_tests.cpp`), which
  compiles the **real** plugin sources into its own console target so it exercises the actual
  `AudioProcessor`.

### 20.3 The compatibility harness worth copying verbatim

Anamorph's single highest-leverage test asset. Reproduce all of it:

1. **A frozen parameter-registry snapshot** (`tests/fixtures/parameter_registry.snapshot`) — the
   test fails on any ID / name / order / range / automation-flag change, and is re-frozen **only**
   via an explicit `--write-snapshot` flag for an *intentional* change. This automates the
   "parameter IDs unchanged" release-checklist item.
2. **Raw-exact save → load → save round-trip** (byte-identical).
3. **Frozen legacy session fixtures** — one XML per historical state format, so every legacy read
   path stays permanently guarded.
4. **Corrupt / foreign-state robustness.**
5. **Preset round-trip + exclusion-rule tests.**

### 20.4 Anabasis-specific measurement gates (from §10)

Beyond the inherited structure, Anabasis's Level-2/3 suites must additionally assert:

- LUFS against the official **EBU R128** test vectors, error ≤ **0.1 LU**.
- True peak against known inter-sample-peak signals, error ≤ **0.1 dB**.
- In true-peak mode, output **never** exceeds the ceiling (tolerance ≤ 0.1 dBTP) — under every
  input, including hostile ones.
- No clicks when toggling bypass, loudness compensation, or the oversampling factor; reported
  latency == measured latency (impulse response).
- Null test with all defaults and no processing engaged.
- Clipper aliasing / image suppression measured in dB at 4× oversampling, recorded in
  `TEST_REPORT.md` with the measurement method (**C2**: no estimates).

### 20.5 Rules

1. **Every bug fix ships a regression test** that fails on the old code and passes on the fix.
2. **Every DSP-policy invariant has a guarding test** where feasible; `DSP_POLICY.md` carries the
   invariant → test map.
3. A pluginval retry is permitted **only** for an abnormal termination of the host-side validator,
   never for a real validation failure. The boundary is **platform-specific** — on Linux/macOS
   `exit < 128` fails immediately, but on Windows there are no signals, so **1…255 including
   128…255** is a real failure and only a Win32 exception code (`≥ 256`), a negative value or no
   code at all may be retried. `TESTING_POLICY.md` rule 3 is the binding statement.

---

## 21. Code style (inherit, with the C++20 delta)

| Element | Convention | Example |
|---|---|---|
| Class / struct | PascalCase | `LimiterEngine`, `LoudnessMeter` |
| Method / function | camelCase | `processBlock`, `predictLatency` |
| Member variable | camelCase, **no `m_` prefix** | `switchState`, `ceilingDb` |
| Constant | `k`-prefixed | `kVersion`, `kMaxLookaheadMs` |
| Namespace | lowercase | `anabasis`, `pid` |
| Parameter IDs | `pid::` string constants | `pid::ceiling` = `"ceiling"` |

- DSP core in `namespace anabasis` under `src/dsp/`; wrapper/editor in the global namespace under
  `src/`.
- Small DSP utilities **header-only**; larger modules are `.h`/`.cpp` pairs.
- One responsibility per file, with a banner comment block documenting the class's purpose.
- Member initialisers in the header. `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` on owning
  classes.
- Audio-path methods marked `noexcept`. Allocate only in `prepare()`.
- Qualify JUCE types (`juce::SmoothedValue`) — no blanket `using namespace juce`.
- Comments explain **why**, not what — especially the rationale behind a click-free transition, an
  ordering choice, or a compatibility quirk. Keep the traceability when extending such code.
- 4-space indent, Allman braces.
- A pure-formatting change is **not** a CHANGELOG entry.

---

## 22. Working method / session protocol (inherit)

1. **Before touching code**: read `CLAUDE.md` → `SOURCE_OF_TRUTH.md` → the relevant System
   Policies → the Architecture doc + any ADR governing the area.
2. **Re-scan the workspace at the start of every phase and on every resume** (C4). Never
   regenerate work that already exists on disk.
3. **During work**: report drift (C6), update docs incrementally (C5), never invent user-visible
   strings (C8) or numbers (C2).
4. **After changing code**: apply the `DOCUMENTATION_LIFECYCLE_POLICY.md` trigger map **in the
   same unit of work**; keep the self-tests green; add a regression test for any bug fix; update
   `CHANGELOG.md` for user-visible changes.
5. **Record investigations in `worklogs/`** — measurements, rejected alternatives, and *why* they
   were rejected. Anamorph's most valuable engineering artifacts are its worklogs (e.g. the
   five measured iterations behind ADR-0015). Finalized decisions graduate to ADRs; the worklog
   stays as the evidence trail.
6. **At the end of each phase** (§11/§13): a summary of changes, the plan for the next phase, and
   the current risks — plus the C++23 canary job status (§2.1).
7. **Maintain the persistent audit files** every time documentation changes:
   `docs/DOCUMENTATION_COVERAGE.md` (coverage audit), `docs/KNOWN_ISSUES.md` (KI-NNN),
   `docs/FUTURE_RISKS.md` (RISK-NNN), `docs/POSTMORTEMS.md` (INC-NNN), `docs/HANDOVER.md`
   (status snapshot, refreshed every release).

---

## 23. Deltas — where Anabasis deliberately differs from Anamorph

| Area | Anamorph | Anabasis | Why |
|---|---|---|---|
| **C++ standard** | C++17 | **C++20** + a non-blocking C++23 canary CI job | §2.1 of this brief |
| **JUCE** | 9.0.0, SHA-pinned | **9.0.1**, SHA-pinned at `e18f7f5…` — §2 asked for the newest stable 9.x to be checked at P0; that check was made and resolved in favour of matching the sibling, so the line started on one framework baseline (**9.0.0** at `f8f8864…`, OQ-001). **ADR-0028 (2026-08-16) ended that**: Anabasis moved to 9.0.1 on the owner's directive and Anamorph did not, so this row is now a real delta rather than a shared value. The six modules that decide DSP, parameters, state and the format wrappers are byte-identical across the two tags, so the divergence cannot produce an audible or host-visible difference between the products | §2 |
| **Signal chain** | Input → engine → Mix → Mono Maker → Output → Solo | Input Gain → EQ → Comp → Clipper/Sat → Limiter → Ceiling → Dither → Output | §3 |
| **Oversampling scope** | wraps the nonlinear stages only | must additionally serve **true-peak detection at ≥ 4×** (BS.1770-4) | §4.3 |
| **Simple/Advanced** | Advanced *adds* modules; Advanced-only modules default-bypass | Simple is a **macro layer over the same parameter model**; switching must not change the sound | §5.3 — a new policy, `MODE_AND_ADAPTATION_POLICY.md` |
| **Metering** | vectorscope / correlation / L-R meters | LUFS (M/S/I) + dBTP + PLR + GR history + spectrum + streaming targets | §6 |
| **Main view** | stereo-field display | one large primary knob + GR/LUFS metering; distinct accent colour | §1.2 |
| **Compatibility legacy paths** | three frozen legacy formats | **none yet** — the contract begins at v0.1.0; freeze a fixture the moment the first format ships | §9 |
| **Presets** | `.anamorph` | `.anabasis`, ≥ 12 factory presets, **parameter lock (Ceiling at minimum)** | §9 |
| **Packaging/installers** | shipped (Inno Setup / `.pkg` / Linux scripts) | added at **P6** | §11 |
| **Plugin identity** | `Anmf` / `Anmr` / `com.rollytech.anamorph`; VST3 categories Fx, Spatial, Stereo | **`RTec`** (shared vendor code — Anamorph moved to it in its 0.9.1) / **`Anbs`** / `com.rollytech.anabasis`; VST3 categories Fx, **Dynamics, Mastering** | OQ-003, resolved — `docs/procedures/BUILD.md` §Plugin identity |

### 23.1 Shared with Anamorph (do NOT diverge)

- Brand: **RollyTech**. Overall frame layout, About page, Settings page, Bypass placement,
  typography, brand colour system, preset + A/B interaction conventions.
- The whole governance system in §14–§16, §22.
- The CI/CD shape in §19, the testing shape in §20, the code style in §21.
- Third-party licence discipline: **no GPL/copyleft code, no third-party asset without stated
  licence and prior approval** (§13). Master Plan and Ozone are behavioural/visual *benchmarks*
  only — no reverse engineering.

### 23.2 Decisions that are still open

Every unresolved item is tracked in [`docs/OPEN_QUESTIONS.md`](OPEN_QUESTIONS.md) — including the
licence tier and whether to extract a shared `rollytech-ui` module (§1.2). **Do not guess at any
of them** (§13).

Two are now **resolved** and are therefore *decisions*, not choices to revisit: the JUCE pin
(OQ-001 resolved it at **9.0.0**, commit `f8f8864…`, the revision Anamorph pins; **ADR-0028 moved
it to 9.0.1 at `e18f7f5…` on 2026-08-16**, through the Architecture Review Gate, and Anamorph
stayed — so the pin is still a decision and no longer a shared one) and the plugin identity
(**`RTec` / `Anbs` / `com.rollytech.anabasis`** — OQ-003; the vendor code spells RollyTech, and
Anamorph moved to it in its 0.9.1 so the product line agrees from the start). Both must be written
into `CMakeLists.txt` at P1 and are frozen from the first build that leaves the repository
(`docs/procedures/BUILD.md` §Plugin identity).

---

## 24. Bootstrap order (what to do first)

The scaffolding described above already exists in this repository. The remaining P0 work is:

1. Read the Anamorph repository in full (source, not just docs) — §1.1.
2. Resolve or escalate every item in `docs/OPEN_QUESTIONS.md`.
3. Produce `DESIGN.md`: architecture, the **full parameter table** (IDs are a permanent contract
   from the moment they ship — get them right *before* v0.1.0), draft macro-mapping curves, UI
   wireframes, and the proposed initial ADR set.
4. Wait for sign-off before P1.

Do not write DSP code before step 4 completes.
