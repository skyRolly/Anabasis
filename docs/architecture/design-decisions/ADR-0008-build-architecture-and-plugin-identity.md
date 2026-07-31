# ADR-0008 — Build architecture and plugin identity: CMake structure, JUCE pin, C++20, frozen identity codes

**Status:** Accepted (2026-07-31 — owner sign-off on `docs/DESIGN.md`)

## Context

`ADR_POLICY.md` makes an ADR mandatory for two of the things this file settles independently:
**build architecture** (CMake structure, JUCE pin, C++ standard baseline) and **plugin format
support**. They are recorded together because they are literally the same `juce_add_plugin` call —
the format list, the identity codes and the target graph cannot be decided apart from one another —
and because `ARCHITECTURE_REVIEW_GATE.md` treats them as one gate item ("Build System change —
CMake structure, JUCE version/pin, C++ standard baseline, dependency set").

Two open questions were resolved by the owner on 2026-07-30 and need a decision record rather than
an issue entry: **OQ-001** (which JUCE 9.x to pin) — whose standing obligation names *"the P0
build-decision ADR"*, i.e. this one — and **OQ-003** (plugin identity codes and bundle ID). Both are
carried in `DEPENDENCY_POLICY.md`'s dependency table and `docs/procedures/BUILD.md` today; neither
has ever been written into a `CMakeLists.txt`, because none exists.

`DESIGN.md` §11 makes the CMake skeleton the first P1 task after sign-off, and §1.1 already names
`AnabasisDSP` as an INTERFACE library in the architecture diagram. **ADR-0001 explicitly defers to
this ADR**: it fixes the *constraint* that the DSP core is a wrapper-free compilation-unit set
depending only on `juce_dsp` / `juce_audio_basics`, and states that "the exact target graph, options
and hardening flags that realise this are ADR-0008's scope".

The identity half is not a build detail. JUCE derives the **VST3 class UID** from
(manufacturer code, plugin code, plugin name), and the manufacturer code is the **AU component's
manufacturer field**. A host that recorded one identity in a session does not load a plugin that
changed it — it reports the plugin as *missing*. `COMPATIBILITY_POLICY.md` therefore freezes these
fields from the first build that leaves the repository, and Anabasis has no "before the first
release" exception available: the sibling product spent that exception on its own manufacturer-code
change (its ADR-0023, which names Anabasis explicitly).

## Problem

Nothing here is settled by "copy Anamorph", and three of the sub-questions have no cheap reversal:

- **The pin.** `FetchContent` accepts a tag *name*; a tag name can be re-pointed upstream. JUCE is
  the framework for the DSP, the parameter system, the GUI and every format wrapper, so a silent
  bump can move DSP behaviour, reported latency and the state ABI at once. Separately: pin the
  *newest* stable 9.x, or the exact revision the sibling product already pins?
- **The standard.** Anamorph is C++17. The brief sets C++20 as a deliberate Anabasis delta and asks
  for specific C++23 *library* features where they help — which raises the question of how a
  C++23-only feature is used in a C++20 build at all, and what stops that from becoming a
  repo-wide `#ifdef` thicket that has to be unpicked when the baseline moves.
- **The target graph.** `DSP_POLICY.md` invariant 13 has a **build-level** invariant→test row —
  `AnabasisDSP` must link without the wrapper — so the library kind is a correctness question, not
  packaging taste. And the state/parameter-compatibility suite has to exercise the *real*
  `AnabasisAudioProcessor`, which means some target other than the plugin must compile the wrapper
  and GUI sources; JUCE's generated shared-code target links its modules `PRIVATE`, so it cannot
  simply be reused.
- **Identity.** These four values must be correct **before the first build leaves the repository**,
  at a point where no user, no session and no host cache exists to validate them against. There is
  no experiment that makes them safer later; the only lever is choosing them once, deliberately.
- **Formats.** VST3 is required, AU is required for Logic Pro. Everything past that is a cost
  question that decides what CI has to build and what pluginval has to validate for the project's
  whole life.

## Options

**Acquiring and pinning JUCE**

- **A1. `FetchContent` with `GIT_TAG "9.0.0"` (the tag name).** Rejected. Readable, and wrong: an
  upstream re-point silently changes the framework under a green build, which is the exact failure
  `DEPENDENCY_POLICY.md`'s version-lock reasoning exists to prevent.
- **A2. Git submodule.** Rejected. It does pin by SHA, but it moves the pin out of CMake into
  `.gitmodules`, requires recursive-clone discipline from every contributor and CI job, leaves no
  natural place for a human-readable version variable, and turns the offline/restricted-CI escape
  hatch into a manual URL swap.
- **A3. Vendor JUCE into the tree.** Rejected. Repository bloat, an upgrade becomes an
  unreviewable diff, and the third-party licence discipline in the brief (§13) gets harder to
  audit, not easier.
- **A4. System-installed JUCE via `find_package`.** Rejected. Reproducibility is gone: three
  platforms would build against whatever each machine happens to carry.
- **A5. `FetchContent` with `GIT_TAG` set to the tag's IMMUTABLE COMMIT SHA, `GIT_SHALLOW TRUE`,
  plus an `ANABASIS_JUCE_PATH` local-checkout escape hatch and two cache variables
  (`ANABASIS_JUCE_VERSION` human-readable, `ANABASIS_JUCE_TAG` the SHA).** **Chosen.** The SHA
  cannot be re-pointed; the version variable keeps the pin legible in `README.md`, docs and error
  messages; the path hatch keeps network-restricted CI buildable without unpinning anything.
- **A6. Pin the newest stable JUCE 9.x point release rather than the sibling's revision.**
  Rejected (OQ-001). One framework revision across the product family means one dependency audit,
  one set of JUCE-attributable behaviour, one bump decision, and a Level-5 audition baseline that
  is comparable between the two plugins. Decisively: the sibling has already exercised **this
  exact commit** headlessly, so Anabasis inherits a revision with evidence behind it instead of an
  unexercised newer one. The brief's "check for a newer 9.x" instruction is thereby a deliberate
  deferral, not an oversight — re-run it if 9.0.0 turns out to lack something this project needs.

**C++ standard baseline**

- **B1. C++17, matching the sibling.** Rejected. It inherits a constraint that has no reason to
  exist here — Anabasis has no legacy toolchain obligation — and the brief records C++20 as an
  explicit delta from Anamorph.
- **B2. C++23 baseline outright.** Rejected. Toolchain and standard-library coverage across
  Windows / macOS / Linux CI is not evidence this repository holds (C7: no build exists), the
  library features in question are exactly the ones with uneven vendor support, and it would make
  the early-warning canary meaningless by making C++23 the thing that gates the main pipeline.
- **B3. C++20 including modules.** Rejected. The brief forbids modules outright; they remain a
  build-system liability in plugin projects, where JUCE's own module sources are unity `.cpp`
  translation units and every generated format target would have to agree on a module build order.
- **B4. C++20 with C++23 features used behind raw `#ifdef` at each call site.** Rejected. Two live
  code paths at every use site, only one of which any given CI leg compiles, and raising the
  baseline later becomes a repo-wide rewrite — precisely the outcome the guard is supposed to
  prevent.
- **B5. `CMAKE_CXX_STANDARD 20` + `CMAKE_CXX_STANDARD_REQUIRED ON` + `CMAKE_CXX_EXTENSIONS OFF`, no
  modules, C++23 library features admitted only behind feature-test macros **and** a thin
  first-party abstraction, plus a non-blocking C++23 canary CI job on all three platforms.**
  **Chosen.** The `#ifdef` lives in one first-party header per feature, so the fallback is written
  once and raising the baseline is a localised deletion; the canary reports breakage early without
  ever being able to block a merge.
- **B6. C++26.** Rejected — explicitly out of scope per the brief.

**Target graph**

- **C1. One monolithic plugin target with the DSP sources compiled straight into it.** Rejected.
  `DSP_POLICY.md` invariant 13's build-level test ("`AnabasisDSP` links without the wrapper") would
  have nothing to assert against, and every DSP acceptance test would have to link the wrapper and
  the editor to reach the engine.
- **C2. `AnabasisDSP` as a STATIC library.** Rejected — carried over from ADR-0001 option D. JUCE
  module headers compile module object code into each consuming target; a STATIC core built against
  its own copy duplicates that object code and breaks linking against the format targets, which
  carry their own.
- **C3. `AnabasisDSP` INTERFACE + `AnabasisHardening` INTERFACE + the `juce_add_plugin` target +
  two console test targets, with ONE shared wrapper/GUI source-list variable consumed by both the
  plugin target and the state-test target.** **Chosen.** The core compiles into each consuming
  final target with no duplicated module objects; a console test can link the engine without the
  wrapper; and the shared list makes "new file added to only one target" unrepresentable.
- **C4. Let the state tests link JUCE's generated shared-code target instead of recompiling the
  wrapper sources.** Rejected. That target links its JUCE modules `PRIVATE`, so it is not reusable
  by another executable — the recompile is not redundancy, it is the only route to the real
  processor.
- **C5. Duplicate the wrapper/GUI source list in the plugin target and the state-test target.**
  Rejected. The first file added to one list and not the other silently drops it out of the
  state/parameter-compatibility suite, and nothing fails until a compatibility bug ships.
- **C6. Hardening flags linked `PRIVATE` to the plugin target.** Rejected. They would not propagate
  into the generated format targets' compile *and* link steps, and the test targets would then
  validate a configuration the product does not ship.
- **C7. Fold performance flags (`-O3`, `-ffast-math`, extra LTO) into the hardening target.**
  Rejected on DSP-policy grounds: numerics-affecting flags are frozen, `-ffast-math` in particular
  changes denormal and NaN behaviour the realtime and DSP policies depend on, and LTO already
  arrives via JUCE's recommended-flags target. The hardening target is **behaviour-neutral by
  definition**, and that is what lets it be linked into the test targets at all.

**Identity codes**

- **D1. `Anmf` (the sibling's original vendor code).** Rejected. It abbreviates *Anamorph*, the
  first product — a name that does not survive a product line.
- **D2. `Roll`.** Rejected — a common English word, so the collision risk against another vendor's
  registered code is higher than for a coined string. **`RolT` / `RlyT`** also considered and
  rejected as less legible spellings of the same idea.
- **D3. `RTec` as a vendor-wide manufacturer code, `Anbs` as the per-product plugin code,
  `com.rollytech.anabasis` as the bundle ID, categories `Fx` / `Dynamics` / `Mastering`.**
  **Chosen.** `RTec` spells RollyTech, satisfies AU's ≥ 1-uppercase requirement, and is shared by
  every RollyTech plug-in; the sibling moved to it in its 0.9.1 precisely so the two products agree
  from the start. `Anbs` is unique against the sibling's `Anmr`. The categories describe a
  maximizer rather than the sibling's spatial effect (`Fx` / `Spatial` / `Stereo`).
- **D4. Defer identity until the first release.** Rejected. The freeze point is the first build
  that *leaves the repository*, not the first tag — a shared pre-release binary is already in
  someone's session — and the sibling has already spent the family's one "before the first release"
  identity exception.

**Format set**

- **E1. VST3 only.** Rejected — the brief requires AU for Logic Pro.
- **E2. VST3 + AU, no Standalone.** Rejected. Standalone is the debugging and manual-audition
  harness, and it costs one JUCE-generated target behind an option that CI can switch off.
- **E3. Add AAX / VST2 / CLAP.** Rejected for v1. AAX requires the Avid SDK plus signing
  infrastructure that does not exist here; VST2's SDK is unavailable and `JUCE_VST3_CAN_REPLACE_VST2`
  is deliberately `0`; CLAP would require a third-party wrapper, which is a dependency decision with
  no evidence base at P0 and its own ADR if it is ever wanted.
- **E4. VST3 (always) + AU (on `APPLE`) + Standalone (option `ANABASIS_BUILD_STANDALONE`, default
  ON).** **Chosen.**

## Decision

**Preamble.** `cmake_minimum_required(VERSION 3.22)`;
`project(Anabasis VERSION 0.1.0 LANGUAGES C CXX)`; default build type `Release` when the generator
supplies none.

**Language.** `CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`.
**C++20 modules are not used.** A C++23 library feature (`std::expected`, `std::mdspan`,
`std::float32_t`, `[[assume]]`, `std::print` in test tooling) may be used only when it is reached
through **both**: (a) a feature-test macro or `__has_include` guard (`__cpp_lib_expected`,
`__has_include(<mdspan>)`, …), and (b) a **thin first-party abstraction** — one header per feature,
carrying the guard and the C++20 fallback — so no call site outside that header ever spells the
guard. Raising the baseline is then a deletion inside those headers, not a rewrite. A **non-blocking
CI canary job** builds at C++23 on all three platforms; its failure must never block the main
pipeline, and its status is reported in each phase summary. C++26 is not a target.

**JUCE.** Version **9.0.0**, acquired by `FetchContent` with `GIT_SHALLOW TRUE` and `GIT_TAG` set to
the **immutable commit SHA `f8f8864172464b9adf9eba6101e1f784838d1597`** — never a branch, never a
mutable tag *name*. Two cache variables carry it: `ANABASIS_JUCE_VERSION "9.0.0"` (human-readable,
and the value `README.md` records) and `ANABASIS_JUCE_TAG "f8f8864…"` (the SHA `FetchContent`
actually uses).

> **`GIT_SHALLOW TRUE` + a commit SHA is a documented CMake trap, and dropping the shallow flag is
> the sanctioned fix.** CMake's own `ExternalProject`/`FetchContent` documentation says
> `GIT_SHALLOW` expects `GIT_TAG` to name a **branch or tag**; fetching an arbitrary SHA shallowly
> works only where the server permits `uploadpack.allowReachableSHA1InWant` (GitHub does today) and
> CMake is new enough to attempt it. Otherwise the outcome is a silent fallback to a full clone
> (slow, not wrong) or a hard configure failure against a stricter mirror.
> **If the combination misbehaves, drop `GIT_SHALLOW`, never the SHA** — the SHA is what makes the
> dependency immutable and is the whole point of the pin. This is stated *here*, in the decision
> record, because an ADR outranks `DEPENDENCY_POLICY.md` (`SOURCE_OF_TRUTH.md`): with the mandate
> recorded and the escape only in the policy, an author following the higher record would read the
> fallback as forbidden. Verify explicitly when `CMakeLists.txt` lands
> (`DEPENDENCY_POLICY.md` §Version-lock reasoning).

If `ANABASIS_JUCE_PATH` is set, a local checkout is `add_subdirectory`'d instead and
no fetch occurs — the escape hatch for offline or network-restricted CI.

**Targets.** Four declared targets, plus the format targets JUCE generates:

1. **`AnabasisHardening`** — INTERFACE library carrying behaviour-neutral binary hygiene as usage
   requirements, linked **`PUBLIC`** so the flags reach every format target's **compile and link**
   and the test targets too, which is what makes the self-tests validate the shipped flag
   configuration. MSVC: compile `/guard:cf`, Release `/Zi`; link `/guard:cf /DYNAMICBASE /NXCOMPAT`,
   Release `/DEBUG /OPT:REF /OPT:ICF`. GCC/Clang: `-fstack-protector-strong -ffunction-sections
   -fdata-sections`, Release `-g`; link `-Wl,--gc-sections -Wl,-z,relro -Wl,-z,now
   -Wl,-z,noexecstack` on Linux, `-Wl,-dead_strip` on Apple. **Deliberately absent, and this
   absence is part of the decision:** `-O3`, `-ffast-math` and extra LTO — numerics-affecting flags
   are frozen by DSP policy, and LTO already arrives through JUCE's recommended-LTO-flags target.
   Symbol stripping happens in CI packaging, never in this file.
2. **`AnabasisDSP`** — **INTERFACE** library (not STATIC — ADR-0001), include directory `src/dsp`,
   INTERFACE sources compiled into each consuming final target, linking `juce_dsp` and
   `juce_audio_basics` and nothing else.
3. **The plugin target** — `juce_add_plugin(Anabasis …)`, which spawns the shared-code and
   per-format targets. Links `PRIVATE`: `AnabasisDSP`, `juce::juce_audio_utils`, `juce::juce_dsp`.
   Links `PUBLIC`: `AnabasisHardening`, `juce::juce_recommended_config_flags`,
   `juce::juce_recommended_lto_flags`, `juce::juce_recommended_warning_flags`.
4. **`AnabasisTests`** (DSP acceptance; compiles the `AnabasisDSP` sources directly, no wrapper —
   this is what makes `DSP_POLICY.md` invariant 13's build-level test writable as stated) and
   **`AnabasisStateTests`** (state / parameter-compatibility; compiles `tests/state_tests.cpp` plus
   the shared wrapper/GUI source list to exercise the **real** `AnabasisAudioProcessor`). Both are
   `juce_add_console_app` targets behind a single option `ANABASIS_BUILD_TESTS` (default ON), and
   neither links the LTO-flags target.

**One source list, two consumers (binding).** The wrapper and GUI sources are declared **once**, in
a single CMake variable (`ANABASIS_PLUGIN_SOURCES`), used verbatim by both the plugin target and
`AnabasisStateTests`. Adding a wrapper or GUI file to only one of them must not be expressible. GUI
sources are compiled into the state-test binary because `createEditor()` references them; they are
never instantiated — the suite runs headlessly.

**Options** (all `ANABASIS_*`): `ANABASIS_BUILD_TESTS` (ON), `ANABASIS_BUILD_STANDALONE` (ON),
`ANABASIS_JUCE_PATH`, `ANABASIS_JUCE_VERSION`, `ANABASIS_JUCE_TAG`, `ANABASIS_BUILD_NUMBER`.

**Compile definitions are part of the build contract**, not incidental: `ANABASIS_VERSION_STRING`,
`ANABASIS_BUILD_NUMBER` (CI supplies the run number), `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`,
`JUCE_VST3_CAN_REPLACE_VST2=0`, `JUCE_DISPLAY_SPLASH_SCREEN=0`, `JUCE_REPORT_APP_USAGE=0`,
`JUCE_STRICT_REFCOUNTEDPOINTER=1`. `AnabasisStateTests` replicates the plugin's full definition set
plus a fixture-directory definition and `JUCE_STANDALONE_APPLICATION=1`.

**Identity — FROZEN from the first build that leaves this repository:**

| Field | Value |
|---|---|
| `COMPANY_NAME` | `RollyTech` |
| `PLUGIN_MANUFACTURER_CODE` | **`RTec`** (vendor-wide; the sibling moved to it in its 0.9.1 / its ADR-0023) |
| `PLUGIN_CODE` | **`Anbs`** |
| `BUNDLE_ID` | **`com.rollytech.anabasis`** |
| `VST3_CATEGORIES` | **`"Fx" "Dynamics" "Mastering"`** |

Supporting `juce_add_plugin` flags: `PRODUCT_NAME "Anabasis"`, `IS_SYNTH FALSE`,
`NEEDS_MIDI_INPUT FALSE`, `NEEDS_MIDI_OUTPUT FALSE`, `IS_MIDI_EFFECT FALSE`,
`EDITOR_WANTS_KEYBOARD_FOCUS FALSE`, `COPY_PLUGIN_AFTER_BUILD FALSE`. Because JUCE derives the VST3
class UID from (manufacturer code, plugin code, plugin name), **`PRODUCT_NAME` is an identity field
too** and freezes with the other three.

**Formats.** VST3 always; **AU** additionally on `APPLE`; **Standalone** when
`ANABASIS_BUILD_STANDALONE` is ON. No AAX, VST2, CLAP or AUv3 in v1; adding one is a format-support
change and therefore a new ADR.

**No packaging in CMake.** No `install()` rules, no CPack. Installers and per-OS packaging arrive at
P6 as CI-invoked scripts (OQ-007), and `COPY_PLUGIN_AFTER_BUILD` stays `FALSE`.

## Consequences

- The framework revision is reproducible and cannot move under a re-pointed upstream tag. The cost
  is that a JUCE bump is a deliberate act: a `ARCHITECTURE_REVIEW_GATE` Build System change needing
  a new ADR plus `DEPENDENCY_POLICY.md`'s rule-2 verification. That is the intent.
- **The pin is inherited as a settled Anabasis decision (brief §23 / OQ-001), not as a ratified
  sibling ADR** — the sibling's own 9.0.0 SHA-pin ADR is still `Proposed` upstream. Nothing about
  Anabasis's pin is contingent on that status; it is noted so a reader following the precedent
  citation is not surprised by it.
- **Identity is unrecoverable after the first escaped build.** A later change to `RTec`, `Anbs`,
  `Anabasis` (the product name) or `com.rollytech.anabasis` changes the VST3 class UID and the AU
  component identity, and every host session referencing the old one reports the plugin as missing.
  There is no exception left to spend. In exchange, Anabasis never carries an identity it has to
  change — the sibling absorbed that disruption once, for the family.
- `DSP_POLICY.md` invariant 13's build-level invariant→test row becomes writable at P1 exactly as
  stated, because `AnabasisTests` links the core with no wrapper present.
- The INTERFACE core means DSP sources are compiled once per consuming final target (VST3, AU,
  Standalone, `AnabasisTests`). Build time and object-code volume rise with the format count;
  accepted, because the alternative does not link at all.
- Hardening flags linked `PUBLIC` mean the test binaries are built the way the product is built, so
  a hardening flag that breaks a test breaks it *before* release rather than after. Cost: a
  toolchain-specific hardening flag failure takes the whole build down, including tests.
- The behaviour-neutral rule is what keeps that safe. It also forecloses using this target as a
  convenient home for optimisation flags later — any numerics-affecting flag is a DSP-policy matter
  and a separate decision.
- **C++20 forecloses modules for v1** and admits C++23 only through the abstraction seam. Two costs:
  a small amount of first-party wrapper code that exists purely to be deleted later, and the
  discipline that a bare `#include <expected>` in a DSP or wrapper source is a review rejection.
  Raising the baseline to C++23 later is a localised change *and* a Build System gate item.
- The canary can never gate a merge, which is the point — and also its limitation: a red canary is a
  report, so it depends on the phase-summary reporting obligation to be acted on at all.
- The single shared source list makes the state suite's coverage structural rather than a review
  checklist item; the cost is that every wrapper/GUI file compiles twice per build.
- Format support is now contract, not convention: pluginval scope, CI matrix and release artefacts
  all follow from VST3 + AU + Standalone for the project's life until an ADR changes it.
- **Boundary with ADR-0001:** ADR-0001 owns the *decoupling contract* — namespace, the POD
  `EngineParameters` snapshot, what `src/dsp/` may include, the maintenance rule. This ADR owns the
  *CMake target graph* that realises it: library kinds, link visibilities, options, flags,
  definitions, formats and identity. Neither may be changed to contradict the other; a change to the
  library kind of `AnabasisDSP` touches both and needs both amended.

**Still open, deliberately not decided here:**

- **OQ-011 (macOS deployment target)** — `Blocking P1`, checked and set at P1 as `DESIGN.md` §11
  plans. It is a user-visible support claim and interacts with the arm64 floor and JUCE 9's
  supported minimum; guessing it now would violate C7 (no build exists to measure against).
- **OQ-006 (C++23 canary scope and cadence)** — this ADR mandates that the job **exists**, builds
  C++23 on all three platforms, and is **non-blocking**. Whether it builds the full target set or
  only the DSP core plus tests, and whether it runs per-push or on a schedule, is confirmed at P2
  when there is DSP code for it to compile.
- **OQ-002 (JUCE licence tier)** — blocks commercial release, not the build; unaffected by this ADR.

## Related code

None yet — P1 onward. Planned: `CMakeLists.txt` (the whole of the above: preamble, standard,
`FetchContent` pin, `AnabasisHardening`, `AnabasisDSP`, `juce_add_plugin` identity + formats,
`ANABASIS_PLUGIN_SOURCES`, `AnabasisTests`, `AnabasisStateTests`), the `AnabasisDSP` sources from
`DESIGN.md` §1.3 — `src/dsp/AnabasisEngine.{h,cpp}`, `src/dsp/EngineParameters.h`,
`src/dsp/TiltEq.h`, `src/dsp/MasteringComp.{h,cpp}`, `src/dsp/ClipSat.{h,cpp}`,
`src/dsp/LookaheadLimiter.{h,cpp}`, `src/dsp/TruePeak.h`, `src/dsp/CeilingClamp.h`,
`src/dsp/Dither.h`, `src/dsp/LoudnessMeter.{h,cpp}`, `src/dsp/LoudnessComp.{h,cpp}`,
`src/dsp/AdaptiveEngine.{h,cpp}`, `src/dsp/GrHistoryBuffer.h` — and the shared wrapper/GUI list
`src/PluginProcessor.{h,cpp}`, `src/PluginParameters.{h,cpp}`, `src/InternalState.h`,
`src/MacroEngine.{h,cpp}`, `src/PresetManager.{h,cpp}`, `src/AbSlotIndex.h`,
`src/gui/PluginEditor.{h,cpp}`, `src/gui/LookAndFeel.{h,cpp}`, `src/gui/FrameClock.h`,
`src/gui/GrHistoryView`, `src/gui/LoudnessMeterView`, `src/gui/SpectrumView`, `src/gui/CurveView`.
Plus the C++23 abstraction headers this ADR mandates (one per adopted feature) and
`.github/workflows/` for the canary job.

Evidence [Unverified] — Anabasis has no `src/` and no `CMakeLists.txt`, so every claim above about
how Anabasis builds is the contract the P1 skeleton must satisfy, not an observation (constraint
C2/C7):

- Design: `docs/DESIGN.md` §11 (P1 skeleton — "CMake per brief §18 (identity `RTec`/`Anbs`/
  `com.rollytech.anabasis` frozen — OQ-003; JUCE 9.0.0 @ `f8f8864…` — OQ-001; C++20)"; OQ-011
  checked and set at P1), §1.1 (`AnabasisDSP` INTERFACE library; DSP core depends only on
  `juce_dsp` / `juce_audio_basics`), §10 row 0008 (the mandate: build architecture *and* format
  support; closes OQ-001 + OQ-003) and §10 row 0001 (the ADR-0008 boundary).
- Research: `worklogs/2026-07-30-p0-anamorph-research.md` — "CMake target graph — complete
  inventory" (four declared targets, hardening flag lists and the deliberately-absent numerics
  flags, INTERFACE-not-STATIC rationale, options set), "JUCE acquisition" (immutable-SHA
  `FetchContent` + `GIT_SHALLOW` + local-path escape hatch), "Identity + project settings", "Compile
  definitions", "How the tests reach the code" (shared source-list variable; shared-code target
  links its modules `PRIVATE` and cannot be reused), "Packaging" (no `install()`/CPack), and the
  `patterns_to_copy` / `decisions_anabasis_must_make` entries (identity pre-decided; C++ standard is
  a decision, not a copy; the sibling's 9.0.0 pin ADR still `Proposed`).
- Brief: `docs/DEVELOPMENT_BRIEF.md` §2 (JUCE 9, CMake ≥ 3.22, formats VST3 + AU required +
  Standalone optional, platforms), §2.1 (C++20 policy, no modules, C++23 behind feature-test macros
  and a thin abstraction, non-blocking canary, no C++26), §18 (build-system conventions 1–8), §23
  (the delta table: C++ standard, JUCE, plugin identity).
- Open questions: `docs/OPEN_QUESTIONS.md` OQ-001 (resolved 2026-07-30 — 9.0.0 at `f8f8864…`; its
  standing obligation names this ADR), OQ-003 (resolved 2026-07-30 — the identity table and the
  rejected `Anmf`/`Roll`/`RolT`/`RlyT` codes), OQ-006 (canary scope/cadence, open), OQ-011 (macOS
  deployment target, blocking P1), OQ-002 (licence tier).
- Policy: `docs/policies/ADR_POLICY.md` (build architecture and format support are both
  ADR-mandatory); `docs/policies/ARCHITECTURE_REVIEW_GATE.md` ("Build System change");
  `docs/policies/DEPENDENCY_POLICY.md` (the JUCE row, the two cache variables, version-lock
  reasoning, rule-2 verification on any change); `docs/policies/COMPATIBILITY_POLICY.md` (identity
  freeze); `docs/policies/DSP_POLICY.md` invariant 13 and its build-level invariant→test row.
- Procedure: `docs/procedures/BUILD.md` §Plugin identity (the frozen values as recorded pre-ADR).
- Precedent [Verified — read during the P0 research pass, cited via the worklog]:
  `Anamorph:CMakeLists.txt:14-22` (project preamble, `CMAKE_CXX_STANDARD` 17 — the value Anabasis
  deliberately departs from), `Anamorph:CMakeLists.txt:30-56` (local-path hatch vs `FetchContent`
  by immutable SHA, `GIT_SHALLOW`), `Anamorph:CMakeLists.txt:36-38` (the version + SHA this pin
  copies), `Anamorph:CMakeLists.txt:59-113` (hardening INTERFACE target, per-toolchain flags,
  deliberately absent numerics flags), `Anamorph:CMakeLists.txt:115-135` (INTERFACE DSP core),
  `Anamorph:CMakeLists.txt:150-165` (`juce_add_plugin` identity and format list),
  `Anamorph:CMakeLists.txt:167-182,251-254` (single shared source-list variable used by the plugin
  and the state-test target), `Anamorph:CMakeLists.txt:186-199` (compile-definition set),
  `Anamorph:CMakeLists.txt:219-275` (the two console test targets behind one option).
- Precedent [Verified — cited via the worklog]:
  `Anamorph:docs/architecture/design-decisions/ADR-0023-vendor-manufacturer-code.md:75-87,99-108` —
  `RTec` as the vendor-wide code, the statement that *"Anabasis adopts the same value at its P1
  skeleton, before it has ever built, so it never carries an identity it has to change"*, and the
  VST3-UID-derivation reason all three fields freeze together. Anamorph ADR-0021 (build hardening,
  numerics flags frozen) and ADR-0022 (immutable-SHA pinning; **still `Proposed` upstream**) are the
  other two precedents.
- Depends on: this repository's **ADR-0001** — the decoupling contract this target graph realises.
