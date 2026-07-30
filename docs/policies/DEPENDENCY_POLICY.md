# DEPENDENCY_POLICY.md

Repository Governance Policy. Third-party dependency locking and upgrade safety.

## Current dependencies

| Dependency | Pin | Mechanism | Status |
|---|---|---|---|
| **JUCE** | **9.0.0**, pinned by the tag's **IMMUTABLE commit SHA** `f8f8864172464b9adf9eba6101e1f784838d1597` | CMake `FetchContent` (`GIT_SHALLOW`), overridable via `-DANABASIS_JUCE_PATH` | Decided (OQ-001, 2026-07-30). Must be written into `CMakeLists.txt` as `ANABASIS_JUCE_VERSION` + `ANABASIS_JUCE_TAG` when that file is created at P1 |
| **pluginval** | latest release (downloaded) | `scripts/run-pluginval.sh` / `.ps1` | pinning it is a tracked improvement, not yet done |
| **C++ standard** | **C++20** | `CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF` | per `DEVELOPMENT_BRIEF.md` §2.1 |
| Linux system libs | distro packages | `scripts/setup-linux.sh` (ALSA, JACK, X11, FreeType, GTK/WebKit, mesa, **EGL — required by JUCE 9's Linux GL context path**, xvfb) | scaffolded |

## Version-lock reasoning

- **JUCE is pinned to an exact IMMUTABLE COMMIT**, not a branch, `latest`, or a mutable tag
  *name*. A tag name can be re-pointed upstream; a SHA cannot. JUCE is the framework for the
  entire DSP (oversampling, filters, `dsp::AudioBlock`), the parameter system (APVTS), the GUI,
  and the plugin-format wrappers — an unpinned bump can silently change DSP behaviour, reported
  latency, the editor/X11 embedding path, and the parameter/state ABI. The pin makes builds
  reproducible and keeps audited behaviour stable.
  Two cache variables carry it: `ANABASIS_JUCE_VERSION` (human-readable) and `ANABASIS_JUCE_TAG`
  (the SHA).
- **`GIT_SHALLOW` + a commit SHA needs verifying at P1 — it is a documented CMake trap.** CMake's
  own `ExternalProject`/`FetchContent` documentation states that `GIT_SHALLOW` expects `GIT_TAG` to
  name a **branch or tag**. Fetching an arbitrary SHA shallowly works only where the server permits
  `uploadpack.allowReachableSHA1InWant` (GitHub does today) *and* CMake is new enough to attempt
  it. Otherwise the likely outcomes are a silent fallback to a full clone (slow, not wrong) or a
  hard configure failure against a mirror that disallows it (wrong, and confusing). The SHA pin is
  not negotiable — it is what makes the dependency immutable — so if the combination misbehaves,
  drop `GIT_SHALLOW`, never the SHA. Check this explicitly when `CMakeLists.txt` lands rather than
  discovering it on someone else's machine.
- **The pin is shared with Anamorph on purpose.** Both products sit on JUCE 9.0.0 at the same
  commit, so a JUCE-attributable behaviour difference between them is impossible by construction,
  one dependency audit covers both, and a Level-5 audition of one is a meaningful baseline for the
  other. The corollary is that a future bump is a **product-family decision**, not a per-repo one:
  bumping only Anabasis re-introduces exactly the divergence this pin removes.
- **C++20 is the baseline, not a floor to drift above.** C++20 **modules are not used** — they
  remain a build-system liability in plugin projects. Where a C++23 library feature would clearly
  improve the code (`std::expected`, `std::mdspan`, `std::float32_t`, `[[assume]]`, `std::print`
  in test tooling), it is guarded behind feature-test macros (`__cpp_lib_expected`,
  `__has_include(<mdspan>)`, …) **and a thin first-party abstraction**, so raising the baseline
  later is a localised change rather than a rewrite. C++26 is not targeted
  (`DEVELOPMENT_BRIEF.md` §2.1).
- **The C++23 canary CI job is early warning, never a gate.** Its failure must not block the main
  pipeline; its status is reported in each phase summary (`OPEN_QUESTIONS.md` OQ-006).

## Upgrade rules

1. A JUCE version bump — or a C++ standard baseline change — is a **Build System change** →
   `ARCHITECTURE_REVIEW_GATE.md` + an ADR.
2. After any bump: full self-tests + pluginval at the phase strictness in **both modes** ×3 on all
   three OSes, **and** a manual audition (Level 5). A JUCE change can move DSP/latency/editor
   behaviour invisibly to the headless gate.
3. Re-verify `RELEASE_COMPATIBILITY_CHECKLIST.md` (latency reporting, session reload) after a bump.
4. Prefer the offline path (`-DANABASIS_JUCE_PATH`) for reproducibility in restricted CI.
5. The `JUCE_*` compile definitions (no webview, no curl, no splash, strict ref-counted pointer)
   are part of the dependency contract; changing them is a build change.
6. **Re-verify third-party attribution after any JUCE bump** — the licence inventory is derived
   from the pinned tree, and components vendored inside JUCE's own dependencies do not all appear
   in JUCE's top-level licence file (`RELEASE_POLICY.md`).

## Adding a dependency

Anabasis does not add a third-party library casually. Before any new dependency:

1. State its **licence** and get owner approval (`DEVELOPMENT_BRIEF.md` §13). Copyleft
   (GPL/AGPL) is excluded by the closed-source product model.
2. Justify why JUCE + first-party code cannot do it — every added dependency is another thing to
   pin, audit, attribute and re-verify on every bump.
3. Anything on the audio path must satisfy `REALTIME_AUDIO_POLICY.md` — an allocating or locking
   library is disqualified regardless of its licence.
4. Pin it by immutable revision, the same way JUCE is pinned.

Dependabot covers the `github-actions` ecosystem only; CMake `FetchContent` is not a Dependabot
ecosystem, and a JUCE bump is deliberately manual and review-gated.

## Compliance log

*(empty — no dependency bump has occurred. Every future bump is recorded here with its ADR and
the rule-2 verification result.)*
