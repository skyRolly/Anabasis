# POSTMORTEMS.md

Incident records: things that went wrong, why, how they were fixed, and what now prevents a
recurrence. An entry is written when a defect was **shipped, or nearly shipped, and cost real
time** — not for every bug.

The purpose is the last field. A postmortem whose "prevention" is "be more careful" has failed;
the prevention is a test, a policy, an ADR, or a CI gate.

## Entry format

```
## INC-0NN — <one-line summary>

**Symptom:** <what was observed, by whom>
**Root cause:** <the actual mechanism, not the proximate trigger>
**Fix:** <what changed>
**Prevention:** <the regression test / policy / gate that now blocks a recurrence>

Evidence [Verified | Partially Verified]:
- Source: <file:lines>
- Test:   <the regression test that would have caught it>
- Commit: <sha> / PR #NN
```

Numbering is sequential and permanent; entries are append-only and never deleted.

## Incidents

## INC-001 — KI-001 closed: discrete transitions ran unducked through the P1/P2 skeleton

**Symptom:** Switching A/B slots — and later the `eqPosition` rewire, the `colourModel` rewire
and the oversampling factor/phase latch — performed a plain swap with no transition handling, so
a switch during playback could produce an audible step. Recorded as KI-001 (opened at P1,
extended twice during P2) rather than observed in a host: no UI path triggered it yet.
**Root cause:** The §2.8 transition layer did not exist; each rewire landed at whatever output
gain the moment had.
**Fix:** The §2.8 asymmetric raised-cosine duck (~6 ms out / ~28 ms in) in `AnabasisEngine`:
engine-side discrete rewires (eqPosition, colourModel, OS factor/phase) are held in applied-state
fields and execute ONLY at the silent bottom, at a block boundary; wrapper bulk swaps (A/B,
preset apply, session load) call `requestForcedDuck()` BEFORE the swap — the
THREADING_POLICY momentary-request atomic — so the duck's envelope covers the smoothed glide.
The first block after prepare/reset adopts directly (a duck there would dip the head of every
render for no transition at all).
**Prevention:** `testDuckWrapsDiscreteRewires`, `testDuckWrapsOsLatch`, `testDuckOnWrapperRequest`
(DSP suite) and `testAbSwitchRequestsDuck` (state suite, wrapper wiring) — each mutation-verified:
unducked rewires, an ignored request atomic, and a removed wrapper call all fail named checks.
One defect was caught during the build: a spurious `1−p` in the duck-out phase inversion sent a
fresh duck to the bottom in one sample — the smoothness test caught it before commit.

Evidence [Verified]:
- Source: `src/dsp/AnabasisEngine.{h,cpp}` (DuckState machine, applied-config flow),
  `src/PluginProcessor.cpp` (the three requestForcedDuck call sites)
- Test:   the four tests above
- Commit: PR #5, §2.8 transition-layer commit

## INC-002 — KI-002 closed: Loudness Comp and Delta were parameter-surface-only

**Symptom:** The Loudness Comp and Delta toggles existed on the parameter surface from v0.1.0's
freeze but did nothing — carried through the engine boundary and ignored (KI-002, opened at P1).
**Root cause:** Monitoring features scheduled for the metering phase; the parameters existed early
because the surface froze first (PARAMETER_COMPATIBILITY_POLICY rule 1).
**Fix:** The §2.7 monitor layer in `AnabasisEngine`: Measure (K-weighted short-term loudness of
delay-aligned dry vs processed, frozen under the BS.1770 −70 LUFS absolute gate) + Predict (the
deterministic gain lift, GR-corrected, floor-only) combined as min(), smoothed 200 ms, applied
POST-mix so the bypass leg carries the same compensation — the loudness-matched bypass. Delta =
(delay-aligned dry − processed) behind its own always-running ~10 ms crossfade. Both are
MONITOR-ONLY: inert whenever `nonRealtime` is set (DSP_POLICY invariant 10).
**Prevention:** `testLoudnessCompensationDoesNotAlterRender` (offline render bit-identical with
comp on vs off; realtime pulled to the dry loudness; the predict floor acts before the measure
exists) and `testDeltaMonitor` (transparent chain → exact silence; pushed chain → the removed
material; offline → inert). Mutants killed: comp/delta ignoring nonRealtime, predict floor
dropped.

Evidence [Verified]:
- Source: `src/dsp/AnabasisEngine.{h,cpp}` (§2.7 block, stage E)
- Test:   the two tests above
- Commit: PR #5, P3 monitor-layer commit

## INC-003 — macOS CI was red for three days on a line no Linux compiler can reject

**Symptom:** From 2026-08-08 (commit `f50d6c2`) to 2026-08-10 the macOS "Build & Validate" job
failed to COMPILE `tests/state_tests.cpp` on every push, while the Linux and Windows jobs stayed
green on the same SHA. Every downstream macOS step — the self-tests, both pluginval modes,
packaging — was skipped for the whole period, so nothing macOS-specific was validated at all.
The failing line was
`return std::sqrt (s / juce::jmax<size_t> (1, v.size()));`.

**Root cause:** `juce::jmax` is an overload SET. `juce_dsp` declares a second overload taking
`dsp::SIMDRegister<Type>` (`juce_SIMDRegister_Impl.h:185`). An EXPLICIT template argument is
substituted into every candidate, so the compiler must form `SIMDRegister<size_t>`; a parameter
of incomplete class type must be completed, and completing it needs `SIMDNativeOps<size_t>`,
which exists only where `size_t` names one of the ten types JUCE specialises. The completion is
outside the immediate context, so it is a hard error, not a SFINAE removal.

    Linux (LP64, glibc):  uint64_t IS unsigned long IS size_t -> complete   -> compiles
    macOS (LP64, libc++): uint64_t is unsigned long long,
                          size_t is unsigned long             -> incomplete -> ERROR

The divergence is in the platform's `<cstdint>` TYPEDEFS, not in the compiler. GCC 13 and
Clang 18 on this Linux machine both accept the line and both reject the structurally identical
`juce::jmax<long long>(...)` — `long long` being on Linux exactly what `unsigned long` is on
Darwin. **A Linux+Clang job would not have caught this, and neither would `-Werror`: it is an
error, not a warning.**

**Fix:** the argument-deduced form, `juce::jmax ((size_t) 1, v.size())` — identical result;
deducing `Type` from a scalar against parameter `SIMDRegister<Type>` fails, which removes the
SIMD candidate before the class is ever completed. The ten other explicit-argument call sites in
the tree spelled `int64_t` and were correct on macOS for a principled reason (JUCE spells the
specialisation `SIMDNativeOps<int64_t>`, which tracks the platform typedef), but they were
changed too: the FORM is what admits the mistake.

**Prevention:** three gates, because one would not have been enough.
1. `scripts/check-portability.py` + the `source-lint` CI job — rejects an explicit template
   argument on the closed set `{jmin, jmax, snapToZero}`, the juce names `juce_dsp` also
   overloads for `SIMDRegister`. It is a LINT rather than a build job precisely because no
   Linux build can see this class. Mutation-verified: reinstating the exact line fails the lint
   at that line and nowhere else.
2. `--compile-canary`, run in the `linux-clang` job — compiles the deduced and explicit forms
   against the pinned JUCE and requires the first to succeed and the second to fail, so a JUCE
   bump that changes the overload set fails loudly instead of silently voiding the lint.
3. The `linux-clang` job's first-party warning gate — it does not catch THIS defect, and saying
   so is the point: it catches the AppleClang diagnostic set (`-Wshorten-64-to-32`,
   `-Wimplicit-int-float-conversion`, `-Wshadow-field`, …) that GCC does not apply, which is a
   different gap that was also only visible from the macOS runner.

**The second-order lesson, which is the expensive one.** The compile break was three days of a
red job; what it COST was three rounds of KI-009 investigation reasoning from a validation
surface that had silently lost a platform. `tests/state_tests.cpp:5694` sits inside
`testClipMixCannotChangeTheDefaultPresetsSound` — so the round-5, round-6 and round-7 KI-009
regressions, written specifically for a fault that reproduces ONLY on macOS, had never once
executed on macOS. A red job on another platform is not someone else's problem; while it is red,
every conclusion drawn from "the suites are green" is scoped to the platforms that still ran.

Evidence [Verified]:
- Source: `tests/state_tests.cpp:5694` (before the fix); `scripts/check-portability.py`;
  `.github/workflows/build.yml` (`source-lint`, `linux-clang`)
- Test:   `scripts/check-portability.py` (mutation-verified); `--compile-canary`
- Commit: `6f63573`, PR #14

## INC-004 — KI-009 closed: the engine's channel bound was undefined behaviour, and one compiler acted on it

**Symptom:** From the first shipped build until 2026-08-11 the plugin emitted **exact digital
silence on channel 0** — left channel dead, right channel normal — on macOS, in both AU and
VST3, at the plugin's own defaults as well as at the reported settings, and at every sample rate
and block size. Setting Clip Mix to 0 restored it; Bypass restored it. Nine investigation rounds
across five months could not reproduce it: every self-test, sanitizer run, valgrind pass and
pluginval gate was green on all three platforms while the artefact users installed was broken.

**Root cause:** `AnabasisEngine::processChunk` computed its channel bound as

    const int nCh = juce::jmin (buffer.getNumChannels(), wetRing.getNumChannels());

and indexed eight `float[kMaxChannels]` stack frames with it — `staged`, `frame`, `tapped`,
`gains`, `monFrameDry`, `monFrameWet`, `renderFrame`, `tp`. At runtime the value is always ≤ 2:
`prepare()` clamps `numChans` with `jlimit (1, kMaxChannels, …)` before sizing `wetRing`, and
`isBusesLayoutSupported` admits at most stereo. At **compile** time it is unbounded —
`AudioBuffer::getNumChannels()` returns a plain member — so each of those loops was, to the
optimiser, a possible out-of-bounds write to a two-element `alloca`: undefined behaviour it may
build on. Clang at `-flto` did. GCC did not, at any optimisation level, with or without LTO; nor
did Clang **without** LTO. Every leaf stage — `ClipSat`, `MasteringComp`, `LookaheadLimiter`,
`LoudnessMeter`, `TruePeak`, `RmsMeter`, `AdaptiveEngine` — already opened with
`jmin (numChannels, kMaxChannels)`. The engine's own loop was the single site that did not.

**Fix:** add the missing term — `juce::jmin (buffer.getNumChannels(),
wetRing.getNumChannels(), kMaxChannels)`. The Clang+LTO build then agrees with the GCC build
bit for bit across every probe configuration, which is what makes this a removed miscompilation
rather than a behaviour change.

**Why the whole validation surface missed it, which is the lesson.** ADR-0008 puts
`juce_recommended_lto_flags` on the **plugin target alone**; the two console suites, the
sanitizer builds, valgrind and the bench all omit them deliberately. The UB is inert without
cross-TU optimisation, so the only binary ever built with the fault was the product — and until
2026-08-10 nothing in CI ever loaded the product. "The suites are green" was true and irrelevant:
they were green in a configuration the customer never receives. Sanitizers could not have covered
the gap either — ASan and UBSan do not model this transformation, and instrumenting the build
suppresses it outright (a `-fsanitize=address,undefined` build of the reproduction passes clean).

The second lesson is about SCOPE claims. The report was recorded as "macOS-only", then narrowed
to "macOS x86_64", and both were wrong: the variable was the compiler, and macOS was implicated
only because AppleClang is the only compiler that had ever built the product there. Building the
plugin with Clang on **Linux** reproduces it identically, on a runner an order of magnitude
cheaper than the one the investigation had been reasoning about.

**What the five months of investigation excluded, kept because the round that finally closed this
one started from the list rather than re-deriving it.** Uninitialised memory (valgrind memcheck
clean, 0 errors from 0 contexts, on both suites; ASan + UBSan clean; `MALLOC_PERTURB_` at 1, 85,
165, 170 and 255 — including 0xFF, which fills with a NaN bit pattern). The wrapper, the bus layer
and the channel pointers (Bypass restores audio, and bypass feeds only the engine's output
selector, so the host was demonstrably delivering channel 0 and the dry ring was intact). The two
macOS narrowing warnings (value-preserving `int` → `size_t` → `int`). Every channel-asymmetric
expression in the engine and `LookaheadLimiter` (each one is an analysis tap, never the audible
path). And one FIX that carries the fingerprint exactly but is not this bug: `ClipSat`'s dynamic-
tame filter could latch a NaN into its own state from a sustained input above ~2.2e38 (+767 dBFS),
one channel, deferred, gated on the mix, cured by bypass — real, fixed, and unreachable by any
programme material, which is how a correct fix can still leave a field report open.

**Two vacuity traps were closed while building the probe that reproduced it**, and both are the
same failure this investigation kept hitting. Matching parameters by `getParameterID()` finds
NOTHING through a format wrapper — JUCE's VST3 client hashes each id, so the hosted instance
reports `id='773352680'` where the source says `"bypass"` — and the first probe therefore ran all
eight configurations at their DEFAULTS while labelling them by name. Matching is now by display
name, a missing parameter is FATAL rather than a warning, and the probe exits 2 (environment)
rather than 0 if it cannot reach the configuration it claims to test. The second was a state leak:
one shared instance let the "defaults" row inherit the previous row's pushed parameters, visible
in the output only to someone who already knew the number. A test that cannot fail is worse than
an absent one, because it is counted.

**Prevention:** `tools/engine_repro.cpp` drives the bare engine — no wrapper, no format, no host
— so a failure names the DSP core directly; `tools/channel_probe.cpp` hosts the built bundle; and
`linux-clang` now builds the **plugin** with the product's own LTO flags and runs both on every
push. `ANABASIS_NO_LTO` and `ANABASIS_STAGE_TRACE` are retained as the two bisection tools that
found it: the first separates "the compiler" from "the link-time optimiser", the second reports
per-channel energy at twelve taps of the chain and named the exact pair of taps between which the
channels diverged.

**Maintainer sign-off, 2026-08-11.** The owner has manually confirmed the behaviour on the
reported host. That closes the one thing this entry was still conditional on: the fix was
root-caused, reproduced and verified in CI, but a field report is not closed by CI — it is closed
by the person who filed it no longer seeing it. KI-009 is therefore CLOSED unconditionally rather
than "resolved pending confirmation", and `KNOWN_ISSUES.md` carries only the fixed-issue pointer
its own convention prescribes.

Evidence [Verified]:
- Source: `src/dsp/AnabasisEngine.cpp` (the `nCh` bound), `CMakeLists.txt` (`ANABASIS_NO_LTO`,
  `ANABASIS_STAGE_TRACE`), `src/dsp/StageTrace.h`
- Test:   `tools/engine_repro.cpp`, `tools/channel_probe.cpp`, `.github/workflows/build.yml`
  (`linux-clang` plugin build + both reproductions)
- Commit: this one, PR #14

## Relationship to the other status files

| File | Holds |
|---|---|
| `FUTURE_RISKS.md` | hasn't happened; might |
| `KNOWN_ISSUES.md` | is happening; confirmed and open |
| `POSTMORTEMS.md` | happened; fixed, with the mechanism recorded |

Every fixed bug ships a regression test that fails on the old code
(`TESTING_POLICY.md` rule 1) — that test is what an entry here cites in its **Prevention** field.
