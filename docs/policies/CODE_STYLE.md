# CODE_STYLE.md

Repository Governance Policy. C++ conventions, inherited from Anamorph so that the two products
read as one codebase. New code must read like the surrounding code.

## Language / build

- **C++20**, no compiler extensions (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_EXTENSIONS OFF`).
  *(Anamorph is C++17; the raise is a deliberate Anabasis decision — `DEVELOPMENT_BRIEF.md` §2.1.)*
- **No C++20 modules.** Header/TU structure only.
- A C++23 library feature may be used only behind a feature-test macro **and** a thin first-party
  abstraction (`DEPENDENCY_POLICY.md`).
- Builds clean under `juce::juce_recommended_warning_flags` — keep it warning-free.

## Naming

| Element | Convention | Example |
|---|---|---|
| Class / struct | PascalCase | `LimiterEngine`, `LoudnessMeter`, `AdaptiveMacro` |
| Method / function | camelCase | `processBlock`, `predictLatency`, `setCeilingDb` |
| Member variable | camelCase (**no `m_` prefix**) | `ceilingDb`, `lookaheadSamples`, `envState` |
| Constant | `k`-prefixed | `kVersion`, `kMaxLookaheadMs`, `kTruePeakOversample` |
| Namespace | lowercase | `anabasis`, `pid` |
| Parameter IDs | `pid::` string constants | `pid::ceiling` = `"ceiling"` |

Parameter IDs are a permanent contract: pick an ID vocabulary that is stable and *decoupled from
the display wording*, so UI copy stays revisable under constraint C8
(`PARAMETER_COMPATIBILITY_POLICY.md`).

## Structure

- DSP core lives in `namespace anabasis` under `src/dsp/`; the wrapper/editor are in the global
  namespace under `src/`.
- The DSP core never includes a plugin-client or GUI header — it depends only on `juce_dsp` /
  `juce_audio_basics` (`DSP_POLICY.md` invariant 13).
- Small DSP utilities are **header-only**; larger modules are `.h`/`.cpp` pairs.
- One responsibility per file; a banner comment block (`// ===== Name ===== ...`) documents each
  class's purpose at the top.
- Member initialisers in the header (`float ceilingDb = -1.0f;`).
- Use `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` on owning classes.

## Real-time discipline (cross-ref `REALTIME_AUDIO_POLICY.md`)

- Mark audio-path methods `noexcept`.
- Allocate only in `prepare()`. Never `new`/resize on the audio path — including lookahead and
  oversampling buffers, which are sized in `prepare()` for the maximum configuration.
- Qualify JUCE types (`juce::SmoothedValue`, `juce::dsp::Oversampling`) rather than a blanket
  `using namespace juce`.
- Every parameter that reaches the DSP is smoothed; a raw parameter read applied directly to a
  gain is a zipper-noise defect.

## Comments

- Comments explain **why**, not what — especially the rationale behind a click-free transition, an
  ordering choice, a smoothing time constant, or a compatibility quirk. Keep that traceability
  when extending such code, and reflect significant ones into an ADR or a postmortem.
- A numeric constant on the audio path (a time constant, a threshold, a slew cap) carries a
  comment saying where the number came from — a measurement, an ADR, or a specification clause.
  An unexplained magic number in DSP code is a future regression.
- Formatting: 4-space indent, braces on their own line (Allman), as in the sibling project.

## Tests

- New DSP behaviour gets a deterministic test in `tests/dsp_tests.cpp` using the `check(cond,
  "what")` harness; new state/serialization behaviour goes in `tests/state_tests.cpp`
  (`TESTING_POLICY.md`).

This is a descriptive style policy; non-structural style tweaks do not require an ADR, but they
must not change behaviour (a pure-formatting change is **not** a CHANGELOG entry — see
`CHANGELOG_POLICY.md`).
