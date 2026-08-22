# ARCHITECTURE_REVIEW_GATE.md

Repository Governance Policy. Some changes are too consequential to merge on a green build alone.

## Rule

The following changes **require human Architecture Review and must NOT be auto-merged even if CI,
the self-tests, and pluginval all pass**:

- **DSP Graph change** — adding/removing/reordering a DSP node (`DSP_GRAPH_REFERENCE.md`).
- **Signal Flow change** — altering the order or placement of any stage (`SIGNAL_FLOW.md`). The
  v1 chain order is fixed by `DSP_POLICY.md` invariant 1 and by `DEVELOPMENT_BRIEF.md` §3.
- **Thread Model change** — new thread, new cross-thread path, new atomic ordering (`THREAD_MODEL.md`).
- **Parameter Registry change** — adding/removing/renaming any parameter ID, changing range/
  default/automatable/exclusion (`PARAMETER_REGISTRY.md`).
- **Serialization Registry change** — any field add/remove/semantic change (`SERIALIZATION_REGISTRY.md`).
- **Latency change** — sources, engagement condition, or reported value (`LATENCY_MODEL.md`).
  Lookahead and oversampling both feed this; host PDC depends on it being exact.
- **Macro-layer contract change** — any change to how Simple maps onto the Advanced parameter
  model, or to the guarantee that switching modes does not change the sound
  (`MODE_AND_ADAPTATION_POLICY.md`).
- **Ceiling guarantee change** — anything that weakens "output never exceeds the ceiling"
  (`DSP_POLICY.md` invariant 4).
- **Plugin Format change** — adding/removing a format (VST3/AU/AAX/Standalone).
- **Build System change** — CMake structure, JUCE version/pin, C++ standard baseline, dependency set.

### Compiler and toolchain versions

Added by **ADR-0031** (0.2.0), after the `macos-14` → `macos-latest` move had to answer this
question ad hoc and the answer was not written down anywhere. The **Build System change** item
above covers compiler versions; what decides whether a particular change is gated is **who chooses
the version** — not which platform it is on, and not whether that compiler's output ships:

1. **A version this repository pins is gated.** `ANABASIS_CLANG_VERSION` in
   `.github/workflows/build.yml` and the `CMAKE_CXX_STANDARD` line in `CMakeLists.txt` are both
   this case, and both carry an ADR (0031 and 0030). The Clang pin stays gated even though the Clang
   jobs upload no artifact: the pin is a repository *decision*, and a decision is what an ADR
   records.
2. **A version the runner image supplies is not gated, because it cannot be.** GitHub re-points
   `macos-latest`, `windows-latest` and `ubuntu-latest` with no commit here, so AppleClang, MSVC,
   GCC, CMake and glibc can all move with no pull request to review. A rule demanding review for
   those is one this repository is unable to obey. What is required instead is **detection and
   record** — the Linux ABI floor (`scripts/check-linux-abi.py`) is the detector for the glibc
   half, the Clang warning gate is the detector for diagnostic changes, and the consequences are
   written up in `docs/procedures/CI_CD.md` when they land.
3. **Changing which of those two a toolchain is** — pinning a floating label, or unpinning a
   pinned one — **is gated when that toolchain builds shipped artifacts**, because that is the
   repository taking or handing over control of the shipped bytes.

**The `macos-14` → `macos-latest` move (2026-08-20) is reconciled by rule 2, not exempted from
rule 1.** It changed no version this repository had pinned — AppleClang has never been pinned here
— and the compiler that followed was GitHub's choice, not a value in this tree, so it was handled
as a CI change with a write-up. That is what rule 2 asks for. Under rule 3 the *label* move would
be gated today, because it handed a shipping toolchain to the image; the rule is stated here so
that answer is available next time instead of being re-argued. Numeric symmetry between platforms
is explicitly **not** required: Apple's compiler versions are not upstream LLVM's, and the two are
chosen by different parties.

## Why these specifically

Each maps to a field-breaking risk (compatibility, real-time safety, host PDC, or — for the
ceiling — a downstream clipping failure in the user's master) that a passing test cannot rule out.
Tests cannot prove a renamed ID won't break a user's saved session, that a new thread path is
race-free under every host, or that a ceiling exception won't surface on material the suite
never saw.

## Procedure

1. The author flags the change as gated (it touches one of the areas above).
2. A human reviewer with DSP/audio context reviews against the relevant Policy + ADR.
3. If the change is a decision, an **ADR** is added/updated (`ADR_POLICY.md`).
4. Compatibility-affecting changes additionally run the
   `procedures/RELEASE_COMPATIBILITY_CHECKLIST.md`.

## Relationship to the AI Agent

Detecting any gated change is an **AI Agent Hard Stop** — the agent stops and requests human
review rather than proceeding (`AI_AGENT_POLICY.md`).
