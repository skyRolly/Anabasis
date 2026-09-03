# 2026-09-02 — GitHub Code Scanning surface audit

Scope: the whole Security & Quality surface, not one alert. Inventory, triage, one fix.

## 1. What could and could not be enumerated

The repository-level alert list is **not reachable from an agent session**:
`api.github.com/repos/skyRolly/Anabasis/code-scanning/alerts` answers `403 "GitHub access is not
enabled for this session"`, and the GitHub MCP server exposes no code-scanning tool. What is
enumerable is the PR-surfaced set: one open thread, **alert #192** (PREfast C6262). Alerts closed,
or open on `main` and not introduced by the PR, cannot be listed and remain subject to GitHub
confirmation. Everything below therefore reproduces the surface locally rather than reading it.

| Tool | Coverage | First-party result |
|---|---|---|
| clang-tidy 18 (`clang-analyzer-*`, `bugprone-*`, `cert-*`, `concurrency-*`, `misc-*`, `performance-*`) | all 13 first-party TUs | 373 distinct — 71 `src/`, 320 `tests/` |
| g++ 13.3 `-fanalyzer` | same | 0 in `src/`, 2 in `tests/`, 10 inside JUCE |
| `-fstack-usage` (the C6262 analogue) | same | 123 first-party frames ≥ 16 KB, all in `tests/` |
| valgrind memcheck | runtime probes, below | 0 errors |

`-flto` is in the compile database for `src/`, and it silently suppresses `.su` output — the stack
scan needs `-fno-lto` or it measures nothing and looks clean. Worth knowing before the next reader
repeats it.

## 2. The finding no scanner reports

`src/PluginProcessor.cpp` `reassertFromRaw` and `src/PresetManager.cpp` `applyOnePresetValue` both
looked clamped and were not. Three clamps sit on the path and **all three are comparison-based**:

| Clamp | Where | NaN |
|---|---|---|
| `juce::jlimit (0.0f, 1.0f, v)` | session `raw` | `v < lo` false, `hi < v` false → returns `v` |
| `NormalisableRange::snapToLegalValue` | preset `value` | `v <= start` false, `v >= end` false → returns `v` |
| `Parameter::setNormalized` | `VST3_SDK/public.sdk/source/vst/vstparameters.cpp:62-71` | `> 1.0` false, `< 0.` false → unchanged |

So nothing downstream rejects it, and `juce_audio_plugin_client_VST3.cpp:1470-1479` hands the value
to the host through `performEdit`. JUCE's reader returns a quiet NaN for the literal `nan`
(`juce_CharacterFunctions.h:254-265`), so a hand-edited or half-written document is the whole
delivery mechanism. **Infinities clamp correctly at every layer** — NaN is the only value that gets
through, which is why the repair is a finite test and not a tighter range.

Measured by linking a probe against the real plugin, before the fix:

| | session document | `.anabasis` preset |
|---|---|---|
| parameters left non-finite | **50 / 50** | **15 / 50** (the NaN third) |
| readouts printing `nan` | **31** | — |
| re-saved as `nan` | **50** | — |
| blocks of non-finite audio | 0 / 140 | 0 / 40 |

Audio is unaffected: the engine's own hygiene absorbs it, and a NaN *input* sample is gone within
the same block and has not returned 200 blocks later. The damage is to control state, and it
propagates with the file.

## 3. Why this was not a Hard Stop, after first being called one

The audit's first answer was that repairing it changed how a stored session is interpreted, which
`AI_AGENT_POLICY.md` makes a Hard Stop. `SERIALIZATION_REGISTRY.md` overturned that:

* §1.1 already states the semantics — "Restore prefers `raw` — **clamped to [0, 1] at the
  boundary**". The code was not implementing its own registered contract.
* §5's tolerance table already registers the pattern, and names the precedent:
  "trims per-field **finite-checked** in `injectTrims`".
* `AdaptiveEngine::injectTrims` (`src/dsp/AdaptiveEngine.h:224-233`) is
  `std::isfinite (x) ? juce::jlimit (lo, hi, x) : fallback` — this exact shape, on another
  deserialized number, since ADR-0014. `setLearnedTargets` (`:530-541`) carries the reasoning in
  full: *"a session file written by a build that did commit one (or an edited file).
  COMPATIBILITY_POLICY's read rule — a value that cannot be read is the default — applied to a
  value that can be read and cannot be used."*

No field is added, removed or re-meant. An unusable `raw` takes the fallback the registry already
gives an **absent** one. The gate item is "any field add/remove/**semantic change**"; this is
conformance to the semantics already registered, applying a rule the schema already uses elsewhere.
Recording the reversal because the first answer was the defensible-sounding one.

## 4. Everything else, and why it is left alone

* **Alert #192 is mis-anchored.** `tests/state_tests.cpp:6530` holds
  `testGrHistoryReaderStaysInsideTheRingAndSeesEveryReset`, **880 bytes**. The 379,228 belongs to
  `testLearnCommitAndAdaptiveRoundTrip` at **:7141**, which holds five stack `AnabasisAudioProcessor`
  fixtures: 5 × 75,840 = 379,200, +48 scalar bytes under GCC (379,248), +28 under MSVC (379,228). A
  20-byte gap across two compilers on different platforms is the same five fixtures, not coincidence.
* **The C6262 class is test-only but not free.** `.github/workflows/build.yml:1126-1143` already
  raises the sanitizer lane to `ulimit -s 65536` because `testTeardownAndReengageInvariants`
  overflows 8 MB under ASan's use-after-scope instrumentation. Frames do **not** nest — every test
  function is a leaf called from `main()`, verified in both suites — so the worst single depth is
  379 KB, 36 % of Windows' default 1 MB reserve, with no `/STACK` override anywhere.
* **Five `src/` diagnostics dismissed with reasons**: the `memcmp`-on-float change detector
  (deliberate and documented, though no `static_assert` pins `sizeof(float) == 4`); C26486's
  lifetime inversion (documented at `src/PluginProcessor.h:96-114` and ADR-0015 §5); ten
  `cert-err58-cpp` statics; one widening cast that is provably in range; 56 bytes of padding.
* **Thirteen verified false positives in `tests/`**, including the `NewDelete` "double free" inside
  the replaced `operator delete` and seven null-reference reports on `Component::getChildren()`.
* **Actions surface: zero findings.** Top-level `permissions:` on all five workflows, every action
  pinned to a full commit SHA, no `pull_request_target` or `workflow_run`, and the one
  `github.event.*` value reaching a shell is a commit SHA, double-quoted and validated with
  `git cat-file -e` before use.
* **Two coverage gaps, documented not changed.** CodeQL's manual build is
  `--target Anabasis_VST3 AnabasisTests`, and `AnabasisTests` compiles only `tests/dsp_tests.cpp` —
  `AnabasisStateTests` is a separate target with no dependency edge — so CodeQL never extracts
  `state_tests.cpp`, `bench.cpp`, the realtime lanes or `tools/`. PREfast *did* report inside
  `state_tests.cpp`, which is direct proof the MSVC action analyses the CMake codemodel rather than
  the built target set: the two scanners have different scopes, and CodeQL's is narrower. Separately,
  ~4,849 lines of first-party Python under `scripts/` are analysed by nothing. None of it ships.

## 5. Runtime probes (valgrind memcheck, zero errors)

Over-long blocks against a small prepared size, channel-count mismatch (1/2/4 vs 2), `processBlock`
before `prepareToPlay`, zero-length and zero-channel buffers, `processBlock` after
`releaseResources`; and six rounds of editor create → pump the message loop → move every parameter →
load state under the open editor → destroy → keep pumping and rendering, plus reverse-order
teardown. That last one needed `juce_events.cpp` rebuilt with `JUCE_MODAL_LOOPS_PERMITTED=1`,
because **the suite creates editors but never pumps the loop** — the timer and async-updater paths
are not exercised by it today.

Every legitimate sample rate is safe, 1 Hz through 1 GHz, at block sizes 0…1048576. Only a
*negative* rate throws `std::bad_alloc` out of `prepareToPlay`, which no conforming VST3 or AU host
can produce; left alone as unreachable.

## 6. Regression coverage

`testAWellFormedDocumentCannotCarryAnUnusableNumber` is the third corruption case.
`testCorruptAndForeignState` covers random bytes and a foreign root — both rejected before anything
is read, neither reaching a parameter. This one is a document that is ours, with the right root and
right schema, whose numbers are out of domain.

Reverted to the code before the fix it reports **exactly four failures** — parameter value, printed
text, re-save, preset path — so no facet is pinned by assertion alone. `TESTING_POLICY.md` rule 1,
met without needing its ADR-0025 exception. The infinities in the crafted document pin that they
still clamp, so the guard is not later widened past what it is for.
