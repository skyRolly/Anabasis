# DEVELOPMENT.md

Local development and debugging. Prerequisites and build commands are in `BUILD.md`; this doc
covers the dev loop.

## Before you start

1. **Re-scan the workspace** (constraint C4). The filesystem is the authoritative execution state,
   not chat history or a summary. Never regenerate work that already exists on disk.
2. Read `CLAUDE.md` → `docs/SOURCE_OF_TRUTH.md` → `docs/policies/AI_AGENT_POLICY.md`.
3. Check `docs/OPEN_QUESTIONS.md`. If your task requires an answer that lives there, escalate —
   do not decide it yourself.
4. Check the current phase in `docs/HANDOVER.md`. Work outside the current phase's scope needs a
   reason.

## First-time setup (from P1)

```bash
scripts/setup-linux.sh                                   # Linux deps (Ubuntu)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug         # Debug for development
cmake --build build --config Debug
```

For a fast inner loop, prefer building a single target:

```bash
cmake --build build --target AnabasisTests        # DSP self-tests (fastest feedback)
cmake --build build --target Anabasis_VST3        # the plugin only
cmake --build build --target Anabasis_Standalone
```

## The dev loop

1. Edit DSP in `src/dsp/` (format-agnostic core) or wrapper/GUI in `src/`.
2. Build + run the self-tests (no DAW needed):
   ```bash
   scripts/build.sh Debug && scripts/run-tests.sh
   ```
3. For anything user-visible, validate the VST3 with pluginval (`TESTING.md`).
4. Audio quality must be auditioned in a DAW — it cannot be judged headlessly
   (`docs/policies/TESTING_POLICY.md` Level 5). For a maximizer this is not optional: transparency,
   punch retention and distortion onset are the product.
5. Sync the documentation the change triggers, **in the same unit of work**
   (`DOCUMENTATION_LIFECYCLE_POLICY.md`).

## Judging your own work honestly

The single most useful habit for this project, from the brief (§3, §5.4):

- **Always compare loudness-matched.** Use the plugin's own loudness-compensated monitoring and
  loudness-matched bypass. Uncompensated A/B always favours the louder version, so an
  uncompensated comparison cannot tell you whether a change improved anything.
- **Use delta monitoring** to hear what the processing is removing, not just what it leaves.
- Record what you measured, on what material, in `worklogs/` — including the alternatives you
  rejected and why. A decision without its rejected alternatives is not reviewable.

## Before you change DSP, parameters, threading, state, latency, or the macro layer

Read `docs/policies/AI_AGENT_POLICY.md` first. These areas are governed by binding policies and
the Architecture Review Gate; some changes are **Hard Stop** conditions requiring human review
(parameter ID changes, serialization schema changes, threading changes, DSP signal-order changes,
reported-latency changes, macro-layer contract changes, ADR conflicts).

## Debugging notes

- The audio thread is real-time (`docs/policies/REALTIME_AUDIO_POLICY.md`): do not add logging,
  allocation, or locks to `processBlock` or any engine `process`, **even temporarily in a
  committed change**. For temporary instrumentation, publish through an atomic and read it on the
  message thread.
- Denormals: the limiter release envelope and the compressor detector decay toward zero and are
  classic denormal generators. `juce::ScopedNoDenormals` must be active for the whole block, and
  envelope state should be flushed to zero below a threshold rather than left to underflow.
- If a NaN/Inf self-heal fires, a bad sample was produced upstream — fix the source, not the
  guard.
- A latency mismatch usually means an oversampling or lookahead change was applied mid-block
  instead of being latched (`DSP_POLICY.md` invariant 2).

## Standalone app for quick manual checks

The Standalone target opens the editor and runs the engine against the system audio device —
useful for a quick GUI/behaviour check without a DAW. It does **not** substitute for in-DAW
automation, offline-render or host-state testing.
