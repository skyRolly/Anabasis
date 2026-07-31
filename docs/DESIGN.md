# DESIGN.md — Anabasis P0 Design Document

**Status: `Accepted` — signed off by the owner on 2026-07-31.** This closed the P0 exit criterion
(`DEVELOPMENT_BRIEF.md` §11); P1 implementation is unblocked.

**What the sign-off ratified**, and where each item now lives:

- Every **⊕ proposed** value in this document — the §4.2/§4.3 parameter surface (now frozen at
  v0.1.0 and recorded by **ADR-0010**), the ⊕ window geometry in §6.2/§6.3, the ⊕ shelf Q in §2.2,
  and the parameter **display names** as launch wording (still revisable under
  `PARAMETER_COMPATIBILITY_POLICY.md` rule 2 — only IDs, ranges, defaults and choice orderings
  freeze).
- **Both Hard-Stop items**, each of which carried a `DSP_POLICY.md` amendment enacted by its ADR
  (`ADR_POLICY.md` rule 5): §1.2 Post-EQ sits *before* the ceiling clamp → **ADR-0002** amended
  invariant 1; §3.3 constant reported latency → **ADR-0004** amended invariant 2. **ADR-0003**
  additionally closed the open point in invariants 2 and 5.
- The named decisions: §3.2 measurement tap, §3.4 no-zero-lookahead, §5.2/§5.3 macro architecture
  and coexistence, §8 copy-and-adapt.
- The §5.5 macro curves **as the P4 tuning starting point**, not as final values.
- The **eleven-ADR set** in §10, all authored `Accepted` and dated 2026-07-31, registered in
  `docs/architecture/design-decisions/ADR_INDEX.md`.

> **This document is now superseded section by section as P1–P6 land**
> (`SOURCE_OF_TRUTH.md` §"Where `DESIGN.md` sits"). It ranks with descriptive Architecture
> (level 5) and **loses to any Accepted ADR it disagrees with**. It is not maintained as a living
> spec: drift between it and shipped behaviour is expected and resolved in favour of the higher
> source. The ADRs above, plus the descriptive architecture set that lands at P1–P2, are what
> bind.

Evidence discipline: facts about Anamorph cite its source as `Anamorph:<path>:<lines>`
`[Verified]` — read during the P0 research pass (evidence trail:
`worklogs/2026-07-30-p0-anamorph-research.md`, against Anamorph commit `b6a3db8`). Facts about
Anabasis cannot be `Verified` yet (no `src/`): design statements here are the *contract the code
must satisfy*. **No number below is a measurement** (constraint C2): ranges/defaults are contract
proposals; macro curves are draft control mappings to be tuned by ear at P4; budget figures are
target allocations, not benchmarks.

---

## 1. Architecture overview

### 1.1 Two-layer decomposition (inherits Anamorph ADR-0001 pattern)

```
┌─────────────────────────────────────────────────────────────────────┐
│ Wrapper (global namespace, src/)                                    │
│  PluginProcessor ── APVTS + InternalState + A/B + presets + undo    │
│  PluginEditor ───── Simple/Advanced views, macro UI, Settings/About │
│  MacroEngine ────── message-thread macro→parameter mapper (§5)      │
├──────────────── POD snapshot: anabasis::EngineParameters ───────────┤
│ DSP core (namespace anabasis, src/dsp/ — AnabasisDSP INTERFACE lib) │
│  AnabasisEngine ─── chain: InGain→EQ→Comp→Clip→Limiter→Ceil→Dither  │
│  + LoudnessMeter (LUFS/dBTP/PLR) + GRHistory + AdaptiveEngine       │
│  depends ONLY on juce_dsp / juce_audio_basics (DSP_POLICY inv 13)   │
└─────────────────────────────────────────────────────────────────────┘
```

The wrapper rebuilds a full POD `EngineParameters` snapshot every block from cached
`getRawParameterValue` atomics and hands it to the engine; the engine owns all smoothing and
never sees JUCE parameter types. Precedent: `Anamorph:src/PluginParameters.cpp:286-389`,
`Anamorph:src/dsp/EngineParameters.h:28-103` [Verified] — including the bitwise
`sameParameters` no-change gate and the maintenance rule that every new snapshot field must be
added to the compare/copy/discrete-diff functions (`Anamorph:src/dsp/AnamorphEngine.cpp:160-209`).

### 1.2 Signal flow with monitoring taps

```
                  ┌───────────────────────[ delay-aligned dry ring ]────────────────────┐
                  │                                                                     │
in ─ InputGain ─ EQ(pre) ─ Comp ─┤OS region: Clip/Sat ─ Limiter├─ EQ(post) ─ Ceiling ─ Dither ─┬─ out
          │                  │      [ADAA]│        ▲                             ▲             │
          │                  │            │   [TP detector tap]           [TP estimate tap]    │
     [features]         [GR meter]   [curve view]  │ [GR meter]          [LUFS/TP/PLR meter]   │
                                                                                               │
   monitoring layer (never in the render path): loudness-compensated monitor gain ── bypass ───┤
                                                delta monitor (dry − wet) ─────────────────────┘
```

- The **EQ position switch** moves the EQ block between exactly two defined points: pre-comp
  (default) or post-limiter — and in the Post position the EQ sits **before the ceiling clamp**,
  which is *always* the last stage before dither. This is what makes DSP_POLICY invariant 4
  satisfiable: a post-limiter EQ boost (up to +12 dB shelf) re-introduces overshoot, and the
  clamp must sit downstream of it or the ceiling guarantee is void in the Post position. Brief
  §4.4's "after the limiter" is satisfied; nothing else reorders. The switch is a discrete change
  routed through the click-free transition layer (§2.8).

  > **⚠ Hard-Stop item — `DSP_POLICY.md` invariant 1 needs the same wording, and this document
  > cannot make that edit.** The policy prints the chain as `… Limiter → Ceiling → Dither` and
  > says the EQ switch moves the block "before the compressor, or after the limiter" — which does
  > *not* say where Post-EQ sits **relative to the Ceiling clamp**. This design reads it as
  > Limiter → **EQ(post)** → Ceiling, because the literal-appended alternative
  > (… → Ceiling → EQ(post)) makes invariant 4 unsatisfiable, and an invariant that cannot hold
  > is the worse reading. But a reader taking the policy's diagram literally would place it after
  > the clamp, so this is **ambiguity being resolved, not a reorder being asserted** — and
  > resolving it still touches DSP signal order, an `ARCHITECTURE_REVIEW_GATE` item and an
  > AI-agent **Hard Stop** (`CLAUDE.md`). `ADR_POLICY.md` allows a policy to change only through
  > an ADR, so `DSP_POLICY.md` was deliberately left untouched by *this document*. **ADR-0002
  > carried the amendment and it landed at sign-off**: invariant 1 now prints
  > `… Limiter → [EQ (post)] → Ceiling → Dither` and states the clamp is last before dither in
  > both EQ positions.
- The **dry ring** (input captured post-nothing, read at current reported latency) serves three
  consumers: loudness-matched bypass, delta monitoring, and the loudness-compensation reference.
  Precedent for the always-running chain + output-crossfade bypass:
  `Anamorph:src/dsp/AnamorphEngine.cpp:777-846,1302-1327` [Verified].
- Oversampling region scope and the true-peak detector placement are §3's decisions.

### 1.3 Planned module inventory (`src/`)

| Layer | File | Role |
|---|---|---|
| wrapper | `PluginProcessor.{h,cpp}` | APVTS, state, A/B, undo, latency reporting, transport |
| wrapper | `PluginParameters.{h,cpp}` | `pid::` IDs, layout, `toEngine()` snapshot builder |
| wrapper | `InternalState.h` | host-hidden session state (`ANABASIS_INTERNAL`) |
| wrapper | `MacroEngine.{h,cpp}` | macro→managed-parameter mapper (message thread, §5) |
| wrapper | `PresetManager.{h,cpp}` | factory/user presets, `.anabasis`, lock-aware apply |
| wrapper | `AbSlotIndex.h` | dependency-free slot clamp (copy of the Anamorph idiom) |
| gui | `PluginEditor.{h,cpp}` | views, top bar, Settings/About backdrops |
| gui | `LookAndFeel.{h,cpp}` | brand palette + glass language + control drawing |
| gui | `FrameClock.h` | vblank-paced visualizer driver (copy) |
| gui | `GrHistoryView`, `LoudnessMeterView`, `SpectrumView`, `CurveView` | visualizers |
| dsp | `AnabasisEngine.{h,cpp}` | chain owner, transitions, snapshot adoption |
| dsp | `EngineParameters.h` | POD snapshot |
| dsp | `TiltEq.h`, `MasteringComp.{h,cpp}`, `ClipSat.{h,cpp}`, `LookaheadLimiter.{h,cpp}` | stages |
| dsp | `TruePeak.h` | BS.1770-4 ≥4× estimator (detector + meter share it) |
| dsp | `CeilingClamp.h` | final safety clamp (inv 4) |
| dsp | `Dither.h` | TPDF + optional shaping |
| dsp | `LoudnessMeter.{h,cpp}` | K-weighting, M/S/I gated LUFS, PLR |
| dsp | `LoudnessComp.{h,cpp}` | monitoring compensation (Measure+Predict, §2.7) |
| dsp | `AdaptiveEngine.{h,cpp}` | feature extraction + slow trims + Freeze/Learn (§5.4) |
| dsp | `GrHistoryBuffer.h` | SPSC ring for GR/waveform history (ScopeBuffer idiom) |

### 1.4 Threading model

Two threads, as in Anamorph (`Anamorph:docs/architecture/THREAD_MODEL.md` — zero worker
threads [Verified]):

- **Audio thread**: engine, feature extraction, meter ballistics, adaptive trim slewing.
  Publishes via relaxed atomics + SPSC rings; consumes command atomics (duck request, learn
  start/stop, hold reset). `ScopedNoDenormals` at `processBlock` top is the single FTZ/DAZ
  mechanism (precedent `Anamorph:src/PluginProcessor.cpp:109`).
- **Message thread**: UI, macro mapper (§5.2), Learn control, PDC updates via a const
  race-free `predictLatency(snapshot)` (single `setLatencySamples` call site — precedent
  `Anamorph:src/PluginProcessor.cpp:88-105` [Verified]).
- **No worker threads in v1.** Linear-phase FIR kernels and all oversampler instances for every
  factor are precomputed/`initProcessing`'d at `prepare()` so factor switches never allocate
  (precedent `Anamorph:src/dsp/AnamorphEngine.cpp:44-56`). 16× offline quality fits the same
  rule: buffers are sized for the maximum factor up front.

---

## 2. Per-stage DSP design

### 2.1 Input gain
Plain smoothed gain (20 ms — the Anamorph standard smoothing constant,
`Anamorph:src/dsp/AnamorphEngine.cpp:58-81`). Range **⊕ −12…+24 dB**, default 0 (identity).

### 2.2 EQ
Static IIR (minimum-phase, zero latency): Tilt ±3 dB around a ≈700 Hz pivot (brief §4.4) implemented
as complementary shelving pair; low shelf + high shelf (Freq/Gain, fixed Q ⊕ 0.707); two bells
(Freq/Gain/Q). All-flat = bit-transparent (skip when all gains are 0, so DSP_POLICY invariant 7
holds structurally). Pre/Post position per §1.2. The **dynamic HF tame** used by the macro and
the adaptive engine (§5.4) is *not* this module: it is a program-dependent one-band high shelf
**inside the Clipper/Sat colour stage** (a sub-block of that chain stage, so DSP_POLICY
invariant 1's stage list is unchanged), and its amount is a real Advanced parameter —
`dynTilt`, §4.2 row 48 — because everything the macro drives must be expressible as
Advanced-parameter values (`MODE_AND_ADAPTATION_POLICY.md` inv 1) and reachable by host
automation (inv 6). Adaptation trims *around* the parameter value within §5.4's bounds, like
release and stereo link.

### 2.3 Compressor (mastering glue)
Feed-forward, log-domain envelope, RMS/Peak detector switch, soft knee, ratio 1.1–4:1,
attack 5–100 ms, release 50–1000 ms + Auto (two-pole program-dependent release), sidechain HPF
(shared 20–300 Hz detector HPF with the limiter, §3 of the brief), Mix for parallel
compression. Runs at base rate: its gain signal is band-limited by the 5 ms minimum attack, so
oversampling it buys nothing audible at glue ratios ≤ 4:1. That is a perceptual expectation, not a
measurement (C2) — the P2 aliasing measurement settles it, and if it comes out otherwise the
compressor moves inside the oversampled region, which is an ADR-0003 amendment.

### 2.4 Clipper / Saturation
Continuously variable hard↔soft knee morph with **ADAA** (first-order antiderivative
antialiasing) on the soft-clip curve, inside the oversampled region (§3). Drive with automatic
level compensation (peak-preserving makeup — precedent for the identity-preserving formulation:
Anamorph's `driveTanh` makeup + clean blend so 0 dB drive is bit-identity,
`Anamorph:src/dsp/AnamorphEngine.cpp:597-643` [Verified]). Colour models Clean/Tape/Tube/
Transistor as harmonic-shaping variants (odd/even balance + colouration tilt), with
**`colourDepth`** as the continuous *how much of the model's character is applied* control —
0% = the model contributes nothing, so `colourDepth = 0` is exact identity regardless of which
model is selected, and the Character macro has a managed target whose default is 0 (§5.5's
fixed-point rule). This is deliberately distinct from `clipMix`, which is the clipper stage's
parallel dry/wet blend (default 100% wet) and stays a manual control. Live transfer-curve
visualisation feeds from the same coefficient set the DSP uses (one source of truth for the
curve).

**`Clean` is the null model, and that makes the factory default a real decision.** Clean means
*no harmonic shaping at all* — the brief's `Clean ↔ Colour` axis (§5.1) — so with
`colourModel = Clean`, raising `colourDepth` applies more of nothing and the **Character macro
is inert**. That is correct behaviour (Clean is the deliberate "no colour whatever the knob
says" escape) but it is fatal as a *default*: the one-knob product's second control would look
dead until the user opened Advanced and changed a discrete parameter the macro never touches.
The factory default model is therefore **⊕ `Tape`** (row 23) — the gentlest of the three
coloured variants — so Character is audible in the factory patch, while `colourDepth = 0` keeps
the default patch bit-identical (inv 7) regardless. Which flavour is the default is a taste call
and is on the §11 checklist.

*The general rule this exposes*, alongside §5.5's fixed-point rule: **a managed target's
audibility must not be gated by an unmanaged discrete parameter at the factory default.** The
arithmetic fixed-point rule is necessary but not sufficient — the `clipMix` defect was a value
mismatch, this one was a value match whose *audible effect* was switched off elsewhere. Both
belong to the P4 macro review.

### 2.5 Limiter
Lookahead 0.5–10 ms (default 2 ms), dual-stage release (fast transient stage + slow program
stage, 1–1000 ms + Auto), styles Transparent/Punchy/Loud as attack-shaping/envelope presets,
stereo link 0–100%, transient preservation, detector HPF shared with §2.3. The gain computer
reads the **true-peak envelope** (§3.2) so the ceiling is dBTP-aware in true-peak mode.

**The ceiling *is* the limiter threshold — that is why there is no separate threshold**
**parameter.** Brief §4.3 lists "Gain/Threshold, Ceiling"; this design takes the conventional
maximizer reading, where `limGain` drives signal into a fixed threshold that equals `ceiling`,
rather than exposing an independent limiter threshold that could sit below the ceiling and make
"how loud can it get" a two-knob question. Stated explicitly because the table freezes at
v0.1.0 and *adding* a parameter later is a kVersion bump — the omission is a decision, not a
gap.

### 2.6 Ceiling clamp (the product promise)
A structurally separate final clamp, **always the last stage before dither** — downstream of the
Post-position EQ, so no parameter combination (including a +12 dB post-limiter shelf) can push
audio past it. It runs at base rate and carries its **own** true-peak estimator tap on its input
(the limiter's detector saw a different signal point once Post-EQ is between them): in TP mode
the clamp's gain acts on the TP estimate with a sample-level hard clip as the backstop, sample
peak otherwise; tolerance ≤ 0.1 dBTP (DSP_POLICY inv 4). It is not a quality stage — driving it
hard (e.g. boosting EQ after the limiter) trades fidelity for the guarantee, which is the correct
priority for a safety clamp. Anamorph deliberately has **no** output clipper (its ADR-0009);
Anabasis inverts that deliberately: a maximizer's contract is the ceiling. Guarded by
`testOutputNeverExceedsCeiling` (hostile-input sweep, across both EQ positions).

### 2.7 Loudness-compensated monitoring, loudness-matched bypass, delta
Adaptation of Anamorph's Measure+Predict Level Match (its ADR-0007,
`Anamorph:src/dsp/LoudnessMatch.cpp:126-185` [Verified]) with the polarity inverted — the wet is
essentially always louder, so compensation is essentially always attenuation:

- **Measure**: K-weighted short-term loudness of delay-aligned dry vs wet;
  `compGainDb = LUFS(dry) − LUFS(wet)`; freezes on silence (silence gate chosen against the
  BS.1770 −70 LUFS absolute gate, *not* Anamorph's −60 dBFS mean-square — a mastering plugin
  meets quiet classical passages).
- **Predict**: absolute, stateless feed-forward estimate from the deterministic gain lift
  (input gain + limiter gain − expected GR), floor-only (only ever lowers monitor gain), so
  cranking the macro pre-ducks the monitor instantly without ratchet.
- Applied **only** to the monitoring path (DSP_POLICY inv 10); the render is untouched.
  Loudness-matched bypass = the bypass crossfade target is the dry ring scaled by the same
  compensation. Delta = (delay-aligned dry − wet) on the monitor path, own crossfade.

### 2.8 Click-free transition layer
Inherit Anamorph's three-mechanism taxonomy (its ADR-0004 [Verified]): asymmetric raised-cosine
duck (~6 ms out / ~28 ms in) for genuine discrete rewires (OS factor/phase change, EQ position,
colour model, preset/A-B/undo bulk swaps — forced-duck atomics requested *before* the swap);
always-running output crossfades for bypass / loudness-comp / delta toggles (~10 ms, bit-exact
at the endpoints); smoothed parameters for everything continuous. Only an OS factor/phase change
latches at the duck bottom.

**Lookahead left the discrete set when §3.3 made reported latency constant in it — so it needs
its own click-free story, not just the absence of a duck.** Moving a read tap through a live
delay line is not inherently click-free: done naively it is a jump discontinuity. The mechanism
is that the **audio** delay stays fixed at the full 10 ms and the lookahead value moves only the
*detector/gain-computer* alignment, which is a smooth, band-limited control signal — so no audio
sample is ever skipped or repeated. This is a switchable path under DSP_POLICY invariant 8 and
needs its own per-path click test; it is listed with the P1 test obligations in §11.

### 2.9 Metering engine
- **LUFS** M/S/I per BS.1770-4 / EBU R128: K-weighting (the exact published coefficients are in
  Anamorph's LoudnessMatch, `Anamorph:src/dsp/LoudnessMatch.cpp:16-46` [Verified] — reuse), 400 ms
  momentary / 3 s short-term, integrated with −70 LUFS absolute + −10 LU relative gating over
  400 ms blocks at 75% overlap — implemented as a **fixed-size accumulator** (loudness
  histogram over quantised block-loudness bins), never a growing per-block container
  (`REALTIME_AUDIO_POLICY.md`'s named consequence for exactly this spot). Accuracy contract ≤ 0.1 LU against the EBU vectors.
- **True peak** from the shared ≥4× estimator (§3.2), ≤ 0.1 dB contract; **PLR** derived.
- **GR history**: per-block GR + waveform minima/maxima into a `GrHistoryBuffer` SPSC ring
  (ScopeBuffer idiom: power-of-two ring, one release-store per block,
  `Anamorph:src/dsp/ScopeBuffer.h:21-91` [Verified]), 10–30 s window.
- **Spectrum**: the brief (§6) requires an **overlaid input/output display** — two capture
  points (post-InputGain input and post-chain output), each an audio SPSC ring in the
  ScopeBuffer idiom, FFT'd on the GUI side and drawn as a dual-trace overlay; dismissible
  (`int_spectrumOn`).
- **Streaming target lines + loudness-penalty estimate**: pure display arithmetic
  (`penalty = platformTarget − integratedLUFS`, shown only when I-LUFS is valid). The reference
  values ship as a single compiled table with per-value source citation and an "as of" date
  surfaced in the tooltip; not user-editable in v1; refreshed each release (OQ-008 mechanism —
  the *numbers* are gathered with citations at P5, never invented here, C2).

---

## 3. Oversampling, true peak, and the latency contract

This section resolves the open point flagged in `DSP_POLICY.md` invariants 2 & 5.

### 3.1 Oversampling region
User oversampling (Off/2×/4×/8×/16×) wraps **Clipper/Sat → Limiter** — the stages that generate
harmonics. EQ, compressor, the ceiling clamp (which must sit after the Post-position EQ, §2.6),
dither and all metering taps stay at base rate.
Filters: JUCE minimum-phase polyphase IIR half-band with `useIntegerLatency=true` by default
(exact PDC — precedent `Anamorph:src/dsp/AnamorphEngine.cpp:44-56`, its ADR-0003 [Verified]);
**linear-phase mode** swaps to FIR half-band stages (precomputed kernels, §1.4) at the cost of
FIR group delay and pre-ringing. **Offline-render quality Follow/Force-Max**: at Force-Max an
offline bounce renders at 16× regardless of the live setting — the factor change rides the
existing latched-switch path at render start, and reported latency during `isNonRealtime()` uses
the forced factor.

### 3.2 True-peak detection is a MEASUREMENT TAP (decision)
The BS.1770-4 true-peak estimator (≥4× polyphase FIR interpolator) feeds **only** the limiter's
gain computer, the ceiling clamp's decision, and the TP meter. The audio that reaches the output
is never resampled by it. Consequences, and why this reading wins:

- With user oversampling **off**, reported latency is **exactly the lookahead allowance** (the
  constant 10 ms of §3.3) and the detector adds **nothing** to it — the detector's own group
  delay is absorbed *inside* the lookahead window (design constraint: the
  estimator's base-rate group delay must be ≤ the 0.5 ms minimum lookahead; the BS.1770-4
  Annex 2 48-coefficient, 4-phase interpolator has group delay (48−1)/2 = 23.5 taps at 4× ≈ 5.9
  base samples ≈ 0.12 ms at 48 kHz — design arithmetic, to be verified by the P2 impulse test,
  not asserted).
- The detector's *total* rate is always ≥ 4× (DSP_POLICY inv 3), at every user OS setting:
  at **Off** it runs its own 4× interpolator on the base-rate signal; at **2×** it interpolates
  the 2×-region signal by a further ≥2×; at **≥ 4×** it reads the oversampled signal directly
  (no double resampling). The ≤ 0.1 dB accuracy test (inv 11) runs across the whole OS matrix,
  because the estimator's input path differs per setting.
- The in-path alternative (output actually limited at ≥4× always) was rejected: it forces
  nonzero OS latency even at "Off", contradicts the brief §7's Off position, and buys nothing
  the ceiling clamp + estimator do not already guarantee within the 0.1 dBTP tolerance.

**Done:** ADR-0003 was accepted at sign-off and `DSP_POLICY.md` invariants 2 and 5 no longer
carry an "open point" — both now assert the measurement-tap reading.

### 3.3 Latency model (the contract `testReportedLatencyMatchesImpulse` guards)

```
reportedLatency = maxLookaheadSamples(10 ms, sr)           (CONSTANT — not the engaged value)
                + osLatency(factor, phaseMode)             (0 when factor = Off)
```

**Lookahead contributes its MAXIMUM, always — a decision, not an oversight.** The obvious model
(`engagedLookahead + OS`) makes reported latency a function of an ordinary sound parameter, and
`lookahead` is carried by every preset, A/B slot and undo step (it is in neither exclusion tier,
§4.2). So under that model **browsing presets or A/B-comparing during playback would change host
PDC on nearly every step** — a re-sync or dropout in the middle of the one workflow a mastering
plugin exists to support, and the forced duck could not dry-fill across it (Anamorph's dry-fill
engages only when `predictLatency == latched latency`,
`Anamorph:src/dsp/AnamorphEngine.cpp:290-307` [Verified]; the P0 research pass flagged exactly
this as a decision Anabasis owes, `worklogs/2026-07-30-p0-anamorph-research.md`). Instead the
limiter reads at a **variable offset inside a fixed 10 ms delay line**, and the engine pads the
difference, so the *engaged* lookahead changes freely while the *reported* figure never moves.

Consequences, all of them intended:

- **Bulk swaps can never cross reported latency.** The remaining latency sources are OS
  factor/phase **and offline-render quality** (`Force Max` renders at 16×, so the reported figure
  under `isNonRealtime()` uses the forced factor) — all three are host-hidden
  `ANABASIS_INTERNAL` settings (§4.3), never carried by presets, A/B or undo. PDC therefore
  recomputes on those three `onChanged` callbacks **plus `setNonRealtime()`**, which is the only
  callback guaranteed to fire on the realtime→offline transition (ADR-0011). Counted the way
  `DSP_POLICY.md` invariant 2 counts them: **three latency inputs** (`int_oversample`,
  `int_osPhase`, `int_offlineQuality`) *plus the transition itself* — the transition is a trigger,
  not an input — and `prepare()` is a further call site again, because it is the host
  re-establishing the whole model rather than an input changing (ADR-0004 item 5 and its
  Related-code note). So a preset step, A/B switch or undo is *always* dry-fillable and never
  touches PDC. This is what makes §7's "copy the Anamorph state machinery wholesale" safe here.
- **Lookahead needs no latch and no duck.** It is a continuous, smoothed read-offset change.
  Only an OS factor/phase change still latches at a reset or the silent duck bottom (§2.8).
- **The cost is real and is the trade:** at the 2 ms default the plugin reports 10 ms rather
  than 2 ms — ~8 ms of PDC the user does not "need". For a mastering processor that is cheap
  (this is not a tracking tool, and §3.4 already declines a zero-latency mode); for anyone who
  disagrees, the alternative is PDC churn while browsing presets. **On the §11 checklist.**
- `testReportedLatencyMatchesImpulse` becomes *stronger*: the impulse must land at exactly
  `maxLookahead + OS` for **every** lookahead value, so a padding bug is a test failure rather
  than a subtle host-sync complaint.

> **⚠ Hard-Stop item — this changes the reported-latency contract, and two binding sentences of
> `DSP_POLICY.md` invariant 2 must move with it.** (a) The invariant's body says "an
> oversampling-factor **or lookahead** change is **latched** and applied at a reset or a
> crossfaded boundary" — under this decision a lookahead change alters no reported figure, so it
> is a smoothed read-offset move and only the OS half stays latched. (b) The invariant's open
> point is phrased against "the engaged lookahead"; it must read *lookahead allowance*.
> A **reported-latency change is an AI-agent Hard Stop and an `ARCHITECTURE_REVIEW_GATE` item**
> (`AI_AGENT_POLICY.md`, `ARCHITECTURE_REVIEW_GATE.md`) — which is the point of surfacing it on
> paper at P0 rather than discovering it in code at P2. As with §1.2, `DSP_POLICY.md` was left
> untouched by *this document*. **ADR-0004 carried both amendments and they landed at sign-off**:
> invariant 2 now states the constant-allowance formula, its latch sentence **drops the lookahead
> and names the oversampling factor and phase mode** (both are inputs to
> `osLatency(factor, phaseMode)`), and its open point is closed.

No other stage contributes (EQ is IIR; detector is a tap, §3.2; dither is sample-wise). Values
are computed at `prepare()` from `getLatencyInSamples()` — recorded as measured numbers in
`LATENCY_MODEL.md` at P2, not predicted here (C2). Remaining rules:

- Latency never changes mid-block: OS factor / phase-mode changes are **latched** at reset or
  the silent duck bottom (§2.8).
- **No host can drive a latency input.** The three that remain — `int_oversample`,
  `int_osPhase`, `int_offlineQuality` — are **host-hidden** (§4.3), so they are not in the
  parameter tree at all. `lookahead` *is* a parameter and is **non-automatable for a different
  reason**, which this decision changed: it can no longer spray PDC (the reported figure is
  constant in it), but its engaged value is a **read offset into a live delay line**, so sweeping
  it at automation rate drags the tap through the buffer and produces pitch/comb artefacts. It is
  a set-and-leave control (footnote ³; ADR-0004). The adaptive engine is separately barred from
  all four (`MODE_AND_ADAPTATION_POLICY.md` inv 4). The delay line is sized for 10 ms at prepare,
  so no lookahead change ever allocates.
- The dry ring is sized `maxLookahead(10 ms) + maxOsLatency(16×, linear) + maxBlock + 1`.

### 3.4 OQ-010 — lookahead has NO zero/off position (recommendation)
Keep the brief's 0.5–10 ms range exactly. Reasons: (a) a 0 ms limiter cannot act ahead of a
transient — it degenerates to a clipper, which the chain already has as a *better* tool for that
job; (b) this is a mastering processor — the zero-latency tracking use case belongs to a
different product class, and several mastering limiters omit it deliberately (brief's own
OQ-010 trade-off note); (c) the range is a compatibility contract from v0.1.0 — widening later
re-scales saved sessions (`PARAMETER_COMPATIBILITY_POLICY.md` rule 3), whereas *narrowing* need
never happen. Row 27's footnote ⁶ states this in the table so the range cannot read as an
oversight. Consequence: **the plugin always reports non-zero latency** — and under §3.3 that
figure is the constant 10 ms allowance, not the engaged value. **Done:** ADR-0004 re-phrased
`DSP_POLICY.md` invariant 2 against the *allowance* at sign-off.

---

## 4. Parameter surface

### 4.1 Conventions
IDs are permanent from v0.1.0 (`PARAMETER_COMPATIBILITY_POLICY.md`). Following Anamorph
[Verified `Anamorph:src/PluginParameters.h:14-88`, `src/PluginParameters.cpp:94-95` (kVersion), `:97-103,153-194` (formatters/parsers)]:
short camelCase strings in a single `pid::` namespace header; `ParameterID{id, kVersion=1}`;
units via string-from-value lambdas (`db`/`ms`/`hz`/`pct` formatters + suffix-tolerant parsers);
discrete params use the Raw* exact-normalised classes (pluginval state-restoration contract,
`Anamorph:src/PluginParameters.cpp:11-89`); host-hidden state uses `int_`-prefixed identifiers
in `ANABASIS_INTERNAL`.

### 4.2 APVTS parameters (49)

Type: F float · C choice · B bool. Auto: host-automatable. Values, ranges and tapers marked
**⊕** are proposals (the brief does not specify them); unmarked values are the brief's. The
**Name column is proposed product wording in its entirety** (C8) — sign-off ratifies it as the
launch wording, but **display names stay revisable afterwards**:
`PARAMETER_COMPATIBILITY_POLICY.md` rule 2 permits a rename at any time while the ID is fixed
(registry + a `Changed` CHANGELOG entry), and that policy outranks this document
(`SOURCE_OF_TRUTH.md`). What freezes at v0.1.0 is the **ID**, and — per rule 3 — the range,
default and choice ordering. The P1 registry snapshot does capture names, so a later rename
re-freezes the snapshot deliberately; that is rule 2's normal workflow, not a violation.
Defaults satisfy DSP_POLICY inv 7 *as
that invariant is conditioned*: with all defaults **and no processing engaged** (material below
the comp threshold−knee region and below the ceiling) output is bit-exact identity — near-full-
scale material engages the default ceiling (⊕ −1 dBTP) and the comp knee by design, which is
what `testNullWithDefaults`'s stimulus level must respect.

| # | ID | Name | Type | Range | Default | Auto | Group |
|---|---|---|---|---|---|---|---|
| 1 | `bypass` | Bypass | B | — | off | yes | view |
| 2 | `advancedMode` | Advanced | B | — | off | **no**¹ | view |
| 3 | `loudness` | Loudness | F | 0…100 | ⊕ 0 | **no**² | macro |
| 4 | `character` | Character | F | ⊕ 0…1 (Clean↔Colour) | ⊕ 0 | **no**² | macro |
| 5 | `tone` | Tone | F | ⊕ −1…+1 (dark↔bright) | ⊕ 0 | **no**² | macro |
| 6 | `ceiling` | Ceiling | F | −20…0 dBTP | ⊕ −1.0 | yes | shared |
| 7 | `freeze` | Freeze | B | — | ⊕ off | ⊕ no | adaptive |
| 8 | `loudnessComp` | Loudness Comp | B | — | ⊕ off | yes | monitor |
| 9 | `deltaMonitor` | Delta | B | — | ⊕ off | yes | monitor |
| 10 | `inputGain` | Input Gain | F | ⊕ −12…+24 dB | 0 | yes | input |
| 11 | `scHpfFreq` | SC HPF | F | 20…300 Hz ⊕(log) | ⊕ 20 | yes | detectors |
| 12 | `compRatio` | Comp Ratio | F | 1.1…4 | 1.5 | yes | comp |
| 13 | `compThreshold` | Comp Threshold | F | ⊕ −40…0 dB | ⊕ 0 | yes | comp |
| 14 | `compAttack` | Comp Attack | F | 5…100 ms ⊕(log) | ⊕ 30 | yes | comp |
| 15 | `compRelease` | Comp Release | F | 50…1000 ms ⊕(log) | ⊕ 200 | yes | comp |
| 16 | `compAutoRelease` | Comp Auto Rel | B | — | ⊕ on | yes | comp |
| 17 | `compKnee` | Comp Knee | F | ⊕ 0…12 dB | ⊕ 6 | yes | comp |
| 18 | `compDetector` | Comp Detector | C | RMS/Peak | ⊕ RMS | yes | comp |
| 19 | `compMix` | Comp Mix | F | ⊕ 0…100 % | ⊕ 100 | yes | comp |
| 20 | `clipShape` | Clip Shape | F | ⊕ 0…1 (hard↔soft) | ⊕ 0.5 | yes | clip |
| 21 | `clipDrive` | Clip Drive | F | ⊕ 0…24 dB | 0 | yes | clip |
| 22 | `clipMix` | Clip Mix | F | ⊕ 0…100 % | ⊕ 100 | yes | clip |
| 23 | `colourModel` | Colour | C | Clean/Tape/Tube/Transistor | ⊕ Tape⁵ | yes | clip |
| 24 | `colourBalance` | Odd/Even | F | ⊕ −1…+1 | 0 | yes | clip |
| 25 | `colourTone` | Colour Tone | F | ⊕ −1…+1 | 0 | yes | clip |
| 26 | `limGain` | Limiter Gain | F | ⊕ 0…+18 dB | 0 | yes | limiter |
| 27 | `lookahead` | Lookahead | F | 0.5…10 ms ⊕(log)⁶ | 2 | **no**³ | limiter |
| 28 | `limRelease` | Lim Release | F | 1…1000 ms ⊕(log) | ⊕ 100 | yes | limiter |
| 29 | `limAutoRelease` | Lim Auto Rel | B | — | ⊕ on | yes | limiter |
| 30 | `limStyle` | Style | C | Transparent/Punchy/Loud | ⊕ Transparent | yes | limiter |
| 31 | `stereoLink` | Stereo Link | F | 0…100 % | ⊕ 100 | yes | limiter |
| 32 | `transientPreserve` | Transients | F | ⊕ 0…100 % | ⊕ 50 | yes | limiter |
| 33 | `truePeakMode` | True Peak | B | — | ⊕ on | **no**⁴ | limiter |
| 34 | `eqTilt` | Tilt | F | −3…+3 dB | 0 | yes | eq |
| 35 | `eqLowShelfFreq` | LS Freq | F | ⊕ 20…500 Hz (log) | ⊕ 100 | yes | eq |
| 36 | `eqLowShelfGain` | LS Gain | F | ⊕ −12…+12 dB | 0 | yes | eq |
| 37 | `eqHighShelfFreq` | HS Freq | F | ⊕ 1…20 kHz (log) | ⊕ 8k | yes | eq |
| 38 | `eqHighShelfGain` | HS Gain | F | ⊕ −12…+12 dB | 0 | yes | eq |
| 39 | `eqBell1Freq` | Bell 1 Freq | F | ⊕ 20…20k Hz (log) | ⊕ 300 | yes | eq |
| 40 | `eqBell1Gain` | Bell 1 Gain | F | ⊕ −12…+12 dB | 0 | yes | eq |
| 41 | `eqBell1Q` | Bell 1 Q | F | ⊕ 0.3…8 (log) | ⊕ 1.0 | yes | eq |
| 42 | `eqBell2Freq` | Bell 2 Freq | F | ⊕ 20…20k Hz (log) | ⊕ 3k | yes | eq |
| 43 | `eqBell2Gain` | Bell 2 Gain | F | ⊕ −12…+12 dB | 0 | yes | eq |
| 44 | `eqBell2Q` | Bell 2 Q | F | ⊕ 0.3…8 (log) | ⊕ 1.0 | yes | eq |
| 45 | `eqPosition` | EQ Position | C | Pre/Post | Pre | yes | eq |
| 46 | `dither` | Dither | C | Off/16-bit/24-bit | Off | ⊕ no | output |
| 47 | `ditherShaping` | Noise Shaping | B | — | ⊕ off | ⊕ no | output |
| 48 | `dynTilt` | Dynamic Tame | F | ⊕ 0…2 dB | ⊕ 0 | yes | clip |
| 49 | `colourDepth` | Colour Depth | F | ⊕ 0…100 % | ⊕ 0 | yes | clip |

¹ Anamorph precedent: host-automating a view toggle drives editor resizes that crash X11 hosts
(`Anamorph:src/PluginParameters.cpp:274-281`, its KI-003 [Verified]).
² Macro automatability is a §5.2 decision — non-automatable in v1; the managed Advanced
parameters are the automation surface. Changing this later is a kVersion-bump + ADR
(precedent: Anamorph ADR-0014 exposed params later). Note `withAutomatable(false)` is
*advisory* — some hosts expose the parameter anyway (`PARAMETER_COMPATIBILITY_POLICY.md`
rule 5); §5.2 states the consequence and the fallback.
³ **Not** latency-affecting under §3.3 (the reported figure is constant in it) — non-automatable
for a different, surviving reason: the engaged value *is* a read offset into a live delay line, so
sweeping it at automation rate drags the tap through the buffer and produces pitch/comb artefacts.
It is a set-and-leave control. Also never touched by adaptation
(`MODE_AND_ADAPTATION_POLICY.md` inv 4). OS factor/phase remain latency-affecting *and*
host-hidden, so neither can spray PDC changes.
⁴ *Not* latency-affecting (the detector is a tap, §3.2) — frozen non-automatable as a
conservative v1 choice; it flips the detector mode, and loosening later is a kVersion bump
(§11 risk 4).
⁵ **Not `Clean`** — see §2.4: `Clean` is the null model, so defaulting to it would make the
Character macro inert in the factory patch. `colourDepth`'s default of 0 keeps the default patch
bit-identical either way.
⁶ **No zero/off position — a decision, not a gap** (§3.4): a 0 ms limiter degenerates into a
clipper, which the chain already carries as a better tool, and widening the range later re-scales
every saved session (`PARAMETER_COMPATIBILITY_POLICY.md` rule 3).

**Exclusion tiers** (single shared predicate, Anamorph pattern
`Anamorph:src/PluginParameters.h:66-88` [Verified]): *view tier*
`{bypass, loudnessComp, deltaMonitor, advancedMode}` — serialized with the session, excluded
from A/B, undo **and** presets. *Preset-excluded* adds `{freeze}`.

Two deliberate departures from the Anamorph precedent, both because the exclusion list is
contract from v0.1.0 (`PARAMETER_COMPATIBILITY_POLICY.md` rule 6):

- **`advancedMode` is in the view tier, not merely preset-excluded** — Anamorph lets its
  `advancedMode` travel with A/B and undo. Here that would mean an **A/B compare or an undo step
  can resize the editor**, which is precisely the editor-resize path footnote ¹ cites as
  crashing X11 hosts. A/B exists to compare *sound*; switching the view while doing it is at
  best startling and at worst a host crash. It stays session-serialized, so a reopened session
  still shows the view you left.
- **`freeze` stays in A/B and undo** (preset-excluded only) — unlike the others it genuinely
  affects the rendered sound, and reproducing a slot's sound requires reproducing whether
  adaptation was latched. The obligation that makes this safe: the **frozen trim vector travels
  per-slot with `freeze`**, using the per-slot-memory inject pattern of §5.4, so switching to a
  frozen slot restores *that slot's* latched trims rather than re-latching whatever the engine
  happens to hold. It is preset-excluded because a preset is a settings document, not a capture
  of a moment's adaptation.

**Ceiling lock** (§9 of the brief): when
`int_ceilingLock` is engaged, preset apply captures and re-asserts `ceiling` exactly like a view
param — lock state itself is host-hidden (below), so browsing presets never moves a locked
ceiling. The lockable set is `{ceiling}` in v1; the mechanism is generic.

### 4.3 Host-hidden state (`ANABASIS_INTERNAL` — InternalState pattern, Anamorph ADR-0010)

| Identifier | Meaning | Values | Default |
|---|---|---|---|
| `int_oversample` | OS factor | Off/2×/4×/8×/16× | ⊕ Off |
| `int_osPhase` | OS phase mode | min-phase / linear-phase | ⊕ min |
| `int_offlineQuality` | offline render | Follow / Force Max | ⊕ Follow |
| `int_ceilingLock` | Ceiling preset-lock | bool | ⊕ off |
| `int_uiScale` | UI scaling | ⊕ 80/90/100/125/150/175/200 % | ⊕ 100 |
| `int_tooltipsOn` | tooltips | bool | ⊕ off |
| `int_uiAnimations` | UI animation | bool | ⊕ on |
| `int_spectrumOn` | spectrum overlay | bool | ⊕ on (brief §6 says *dismissible* — visible until dismissed) |
| `int_meterTargets` | target-line set | per-platform bitmask | ⊕ all on |
| `int_tpMeterOn` | TP meter toggle | bool | ⊕ on |

Rationale: `withAutomatable(false)` does not hide a VST3 parameter in every host — REAPER lists
them all (`Anamorph:src/InternalState.h:10-29` [Verified]); anything non-musical stays out of
the tree. OS factor/phase **and offline-render quality** drive the DSP through an atomic mirror +
`onChanged` → PDC recompute
callback, exactly the Anamorph mechanism. These fields persist with the session (so offline
renders reproduce) but never participate in A/B, undo, or presets.

### 4.4 Serialization schema v1
Root `AnabasisRoot` with an **explicit `schemaVersion` integer property = 1** — a deliberate
deviation from Anamorph, which has no version field and detects format generations structurally
(`Anamorph:src/PluginProcessor.cpp:535-613` [Verified]); greenfield Anabasis writes the version
from day one *and* keeps the structural-tolerance read rules (unknown fields ignored, missing
fields default, indices clamped at the boundary). Children: APVTS tree `ANABASIS` (with the
additive exact-`raw` attribute per PARAM — Anamorph ADR-0013), `ANABASIS_INTERNAL`, `AB` and `ADAPTIVE`.

**`AB` — active index (clamped) plus, per slot: the parameter tree, preset name, baseline, the
frozen trim vector, and the macro detach mask.** The last two are *per-slot state, not global*,
and the schema must give them a home or the obligations that make `freeze`-in-A/B and the detach
rule safe are unimplementable. A single global trim vector means slot A frozen with trims Tₐ and
slot B frozen with T_b share one vector, so an A/B switch restores the wrong latched trims and a
frozen slot stops being bit-repeatable — the property `MODE_AND_ADAPTATION_POLICY.md` invariant 3
requires. A single global mask means slot A's detached `clipDrive` describes slot B, so the next
macro gesture re-engages parameters the user never detached *here* and leaves detached ones they
did. The trim vector restores through the engine-side inject-at-the-duck-bottom path (it is audio
state); the **mask restores on the message thread** with the rest of the slot, since its only
consumer is the MacroEngine (§5.2) and nothing on the audio thread reads it.

**`ADAPTIVE` — global, and only what is genuinely global:** the learned reference targets (§5.4).
Absent = never learned. It holds neither the trim vector nor the mask.

Presets: `.anabasis` XML in `<userAppData>/RollyTech/Anabasis/Presets`, snapped-value contract,
**plus the detach mask** (§5.3), factory presets as compiled-in override tables (Anamorph
patterns [Verified]). The full state-test harness
(registry snapshot with `--write-snapshot` gate, byte-identical round-trip, corrupt/foreign
robustness, preset round-trip) is reproduced from day one per `DEVELOPMENT_BRIEF.md` §20.3.

---

## 5. Simple/Advanced macro layer and the adaptive engine

### 5.1 The invariant frame
One parameter model; a mode switch is a pure **view** change and never a value change
(`MODE_AND_ADAPTATION_POLICY.md` inv 1–2). Anamorph offers **no** usable precedent here — its
Advanced toggle *gates parameters to neutral at snapshot time*, i.e. toggling can change the
sound (`Anamorph:src/PluginParameters.cpp:326-389`, its PARAMETER_REFERENCE "Advanced-mode
gating" [Verified]) — the exact opposite of Anabasis's contract. This design is new, hence
ADR-0005.

### 5.2 Macro mapping architecture (decision)
The macro controls (`loudness`, `character`, `tone`) are real, host-visible, **non-automatable**
APVTS parameters. A message-thread **MacroEngine** maps macro moves onto the managed Advanced
parameters by writing them through the normal parameter interface
(`setValueNotifyingHost`, gesture-bracketed per knob drag):

- Because macros are non-automatable, macro changes are *expected* to originate on the message
  thread — no audio-thread parameter-write hazard, no offline-render timing dependency.
  `withAutomatable(false)` is **advisory** — some hosts expose the parameter regardless
  (`PARAMETER_COMPATIBILITY_POLICY.md` rule 5) — so the MacroEngine consumes macro changes
  exclusively through an async message-thread listener: a host that writes the macro anyway
  gets the mapping applied at message-thread rate, and offline-render determinism for that
  (unsupported) usage is explicitly not promised. Host automation
  rides the managed Advanced parameters themselves (the automation surface, policy inv 6).
- **How a macro-originated write is told apart from a manual edit** (this is the mechanism
  §5.3's detach rule depends on; without it the MacroEngine's own writes would trip the rule and
  every managed parameter would detach on the first macro gesture — the exact opposite of the
  re-engage contract). Two conditions, both required for a change to count as a *manual edit*:
  1. **Not macro-originated.** The MacroEngine raises a re-entrancy flag around its whole write
     burst; the listener that sets detach bits ignores every change seen while it is raised.
     Message-thread-only by construction (previous bullet), so the flag needs no atomics and
     cannot interleave with a user gesture.
  2. **Gesture-bracketed.** The change arrived inside a `beginChangeGesture`/`endChangeGesture`
     pair — i.e. a real UI drag or a host-side automation *write* gesture. Ungesture-d writes
     (automation playback, preset apply, A/B, undo) never detach, matching Anamorph's rule that
     host automation folds into the baseline rather than becoming an edit
     (`Anamorph:src/PluginProcessor.cpp:338-421` [Verified]).

  Comparing the incoming value against the expected curve value was **rejected** as the
  discriminator: it is a float comparison against a value the engine is mid-glide toward, so it
  misfires on both sides (a manual edit that lands on the curve would not detach; a rounding
  difference would detach spuriously). The flag is exact and the gesture bracket is the same
  signal the undo coalescer already needs.

  This also settles the "host writes the macro anyway" case from the previous bullet: such a
  write is macro-originated, so the flag suppresses detaching, and the managed parameters simply
  follow — the mapping stays a function of the macro position.
- The mapping is a **pure function** `managedTargets = M(loudness, character, tone)` (§5.3).
  Writes are rate-limited to control rate; the engine's 20 ms parameter smoothing makes the
  glide click-free.
- **Rejected — internal derived values** (engine computes managed values from the macro at
  snapshot time, APVTS holds only manual values): it creates two places a value lives, makes
  the Advanced view show numbers the DSP is not using, muddies automation and undo semantics,
  and weakens policy inv 1's "a host automating an Advanced parameter and the Simple knob write
  to the same place".
- **Rejected — automatable macro**: replaying a macro lane means the plugin writes other
  parameters during host playback; hosts differ on recording/feedback semantics, and offline
  bounces would depend on message-thread timing. Revisitable later via kVersion bump + ADR.

### 5.3 OQ-004 — manual-edit coexistence: macro-latch with re-engage on touch (decision)
When the user edits a managed parameter in Advanced and returns to Simple:

1. **Nothing moves** (inv 2 — the switch is sound-neutral by construction; there is nothing to
   test *around the switch itself* because no value path exists there, but
   `testModeSwitchIsSoundNeutral` still guards the implementation).
2. The edited parameters are **detached** from the macro (per-param detach bit). The mask is
   **per-A/B-slot state** (§4.4), so it travels with the slot it describes; the Simple view shows
   an *edited* indicator on the macro knob (exact wording/visual is owner-specified product text
   — C8, TODO).

   **Presets carry the mask ⊕** — a reversal of the earlier "preset-excluded, cleared on load".
   Be precise about *why*, because the obvious argument does not survive its own rules: carrying
   the mask does **not** change any value trajectory. Rule 3 re-engages every detached parameter
   on the next macro gesture regardless, and preset apply lands the stored values exactly as saved
   either way (below). What the mask changes is **what the UI tells the user and what "reset to
   macro" does**: without it, a preset saved after manual edits reloads with off-curve values
   silently marked *engaged*, so the Simple view claims the macro describes a sound it does not,
   and step 4's reset affordance has nothing to reset. Carrying it makes the preset an honest
   description of the state it captured. That is a smaller claim than "prevents a value jump", and
   it is the true one.

   **Accepted residue:** an off-curve-but-engaged state is still reachable — host automation
   writes managed parameters ungesture-d, so it never sets a detach bit (§5.2). A preset saved
   during such a session stores off-curve values with a clear mask. This is accepted rather than
   patched: making ungesture-d writes detach would mean automation playback silently detaching
   parameters, which is worse. The consequence is bounded — the next macro gesture re-engages
   them, exactly as rule 3 says.

   **Factory presets ⊕ ship an all-clear mask *wherever the patch is reachable from a single*
   `(loudness, character, tone)` *triple*; where it is not, the mask records the off-curve
   parameters.** That makes curve-consistency an *authoring constraint*, not a property: §5.5's
   nine curves are **jointly coupled to the triple** — six to `l`, `colourDepth` to `character`
   *and* `l`, `eqTilt`/`colourTone` to `tone` — so some plausible patches are simply unreachable.
   "Tape Glue" (heavy colour, gentle limiting) needs `colourDepth` high with `limGain` low, but
   `colourDepth = 100·character·(0.4 + 0.6·l)` caps at 40 % as `l → 0`, so heavy colour *requires*
   a high `l`, which forces `limGain` up. Such a preset ships a non-clear mask and is correct, not
   defective. What is not acceptable is assuming curve-consistency without checking: P6 preset
   authoring checks each patch against the frozen curves and records which ones needed a mask.

   **A preset apply must not be seen as a macro move.** Preset/A-B/undo writes are ungesture-d
   and are applied inside the MacroEngine's re-entrancy flag (§5.2), so the listener neither
   detaches anything nor re-maps: the stored managed values land exactly as saved. Without both
   conditions a preset containing off-curve managed values could not be recalled faithfully at
   all — the mapper would immediately overwrite them from the restored macro position.
3. The **next macro-knob gesture re-engages** every detached parameter: targets come from the
   macro curve at the new position, reached through the normal rate-limited glide. Turning the
   one knob *is* the "clear notice" moment — the user is explicitly choosing the macro.
4. A "reset to macro" affordance re-engages without changing the macro position (wording TODO).

**Rejected — carry-over offsets** (preserve manual edits as deltas riding the macro curve):
the knob's sound at a given position becomes history-dependent and untestable (the mapping is no
longer a pure function), offsets compound silently with adaptation, and A/B between two slots
with different offset sets is unexplainable. Macro-takes-precedence-on-touch keeps one source of
truth and implements the brief's first suggested direction — macro takes precedence, with the
re-engage gesture as the "clear notice" moment; the carry-over direction is the rejected
alternative above. ADR-0005 records this before P4 (OQ-004's deadline).

### 5.4 Adaptive engine
- **Features** (audio thread, block-rate): short-term LUFS, crest factor, spectral tilt/
  centroid, transient density — published as atomics for the UI, consumed by the trim logic.
- **Trims**: bounded deltas applied *inside the engine* around the current parameter values —
  release trim, stereo-link trim, sidechain-HPF trim, and the dynamic-tilt amount (§2.2) —
  slewed with second-scale time constants + hysteresis (policy inv 3: rate-limited, never
  stepped). Trims are **not** parameter writes: the host never sees them, undo never records
  them, and the Advanced view shows them as a delta overlay on the affected controls (display
  only). Bounds are hard: trims cannot exceed a parameter's declared range, cannot touch
  lookahead/OS (policy inv 4), cannot move a locked parameter.

  **Reconciliation with MODE inv 1/6 — stated so ADR-0005 does not paper over it.** Inv 1
  governs the *macro knob*: everything the knob does routes through real Advanced parameters
  (§5.2, including `dynTilt`), and that stays true. The trims are the *adaptive engine*
  (governed by inv 3/4, which explicitly let adaptation "move parameters within their declared
  ranges"), and they are deliberately not APVTS writes because parameter-spraying the host with
  second-scale modulation would pollute automation lanes and undo. The §5.2 rejection of
  "internal derived values" does not transfer: there the *user's own edit* would live in two
  places; here the delta is machine-generated, bounded, displayed as an overlay, deterministic
  in the input programme (same audio + same parameters ⇒ same trims), zeroed when adaptation is
  idle, and latched-and-serialized under Freeze. The inv-6 caveat is real and is stated rather
  than hidden: while adaptation runs, identical Advanced values do not alone determine the
  processing — the programme does too. That is the *definition* of §5.2-of-the-brief's adaptive
  engine, and Freeze is the escape hatch that restores strict value-determinism. ADR-0005
  carries this paragraph as its inv-1/6 compliance argument.
- **Freeze** latches the current trim vector (bit-repeatable output thereafter — the policy's
  Freeze test); the frozen vector serializes **per A/B slot** (§4.4), so both a session reload
  and a switch back to a frozen slot reproduce that slot's latched trims exactly.
- **Learn** (§5.2 of the brief): explicit start → analyse the playing passage (integrated-LUFS
  style accumulation of the feature set) → explicit end fixes the internal reference targets.
  Interaction grammar copies Anamorph's Match/Apply: engage is duck-routed, running feedback is
  an atomic-published readout, the commit is gesture/undo-bracketed as **one** step
  (`Anamorph:src/PluginProcessor.cpp:178-202` [Verified]; multi-target commit bracketing via
  the preset-load undo hook pattern, `Anamorph:src/PluginProcessor.cpp:33-38,402-421`).
  Learned targets serialize in the **global** `ADAPTIVE` child (they are a property of the user's
  material, not of a slot); the **per-slot** frozen trims use the `abMatchGain` sentinel-atomic
  inject pattern (`Anamorph:src/PluginProcessor.cpp:485-491` [Verified]) — **adapted, not copied:
  that precedent moves one float and the trim vector is four, so the transport is `OQ-013` and a
  Hard Stop until an ADR settles it**. Unlike Anamorph's Level
  Match, Learn's output feeds the *adaptive reference targets*, never the output stage — a
  maximizer must not auto-match its output level to its input.

### 5.5 Macro curves (draft — tuned by ear at P4, frozen before v0.1.0)
All values **⊕ draft**. `L` = loudness 0…100, normalised `l = L/100`. Managed set and shapes:

| Managed param | Curve (draft) | Intent |
|---|---|---|
| `limGain` | `18 · l^1.2` dB | the primary push; slightly soft start |
| `compThreshold` | `0 → −12 · min(1, l/0.6)` dB | glue engages over the low range, saturates by L≈60 |
| `compRatio` | `1.5 + 0.5·l` | stays gentle |
| `clipDrive` | `0` for l<0.3, then `9·(l−0.3)/0.7` dB | clipper absorbs transients from the mid range |
| `clipShape` | `0.5 → 0.35` over l=0.3…1 | harder knee as drive rises |
| `colourDepth` | `100 · character · (0.4 + 0.6·l)` % | Character scales colour, loudness deepens it (row 49) |
| `dynTilt` | `0 → 1.5 dB` over l=0.5…1 | tames HF harshness at high push (row 48) |
| `eqTilt` | `tone · 2` dB | one dark↔bright gesture… |
| `colourTone` | `tone · 0.5` | …split across the two tone controls |

**Binding rule — the default patch is a fixed point of the mapping.** For every managed
parameter, `M(loudness=0, character=0, tone=0)` **must equal that parameter's declared default**
in §4.2. Otherwise the first macro gesture would jump the value instead of gliding from where it
already was — an unexplained state change in the factory patch. Check by inspection above:
`limGain` 0, `compThreshold` 0, `compRatio` 1.5, `clipDrive` 0, `clipShape` 0.5,
`colourDepth` 0, `dynTilt` 0, `eqTilt` 0, `colourTone` 0 — each matching its row in §4.2. This
rule is why the colour amount is `colourDepth` (default 0) and **not** `clipMix`: `clipMix` is a
manual parallel-blend control that defaults to 100% wet, so it can never be a fixed point of a
curve that starts at zero colour. `clipMix` and `compMix` are therefore **not** in the managed
set. A P4 curve revision that breaks the fixed-point rule is a defect, not a taste choice, and
`testMacroDefaultIsFixedPoint` guards it.

Adaptive trims ride *around* these curves within their §5.4 bounds. The curve table is the
tuning artifact for the P4 listening sessions against Master Plan (§5.4 of the brief); every
revision before the freeze is recorded in the worklog, and the frozen curves become part of
ADR-0005's acceptance evidence.

---

## 6. UI design

### 6.1 Family-consistent frame (inherited structurally from Anamorph [Verified])
Top bar (46 px logical): wordmark + sub-brand left (ghost button → About); preset browser
`‹ name ›`; right-anchored cluster A/B · Copy · Undo · Redo · Settings · ADV · **Bypass
rightmost** (`Anamorph:src/PluginEditor.cpp:1657-1680` layout; red pill `LookAndFeel.cpp:332-334`; dim
overlay sync `PluginEditor.cpp:1039-1043` — Bypass is the first
`removeFromRight(84)`, red pill when engaged, dim overlay below the top bar). About and
Settings are Backdrop overlays with the same structure (About: wordmark, version+build,
company, one-line description, hyperlink; Settings: labelled rows bound to `ANABASIS_INTERNAL`).
Palette: reuse the neutral roles verbatim (`bg 0xff0e1014`, `bgPanel 0xff161a21`,
`bgRaised 0xff1d222b`, `outline 0xff2a313d`, `text 0xffd7dde6`, `textDim 0xff8b94a3`,
`Anamorph:src/gui/LookAndFeel.h:8-26` [Verified]); glass surface language and componentID-keyed
LookAndFeel variants copied. **Accent**: brief §1.2 encourages a distinct accent within the shared
language — proposal: a warm amber/gold family replacing Anamorph's teal/blue pair, exact values
chosen at P5 together with the colour-blind-safe metering palette check (§8 of the brief);
owner ratifies the swatch (C8-adjacent product identity).

Typography: Anamorph uses the platform default sans-serif with tracked ALL-CAPS labels and a
size ladder — **but the brief (§2, §8) explicitly asks Anabasis to use JUCE 9 variable-font
support**. Proposal: embed one variable font (licence cleared through brief-§13 approval *before*
adoption) with the Anamorph tracking/size grammar; fallback to the platform-default path if no
licence-clean font is approved by P5. This is a deliberate delta, recorded in brief-§23-style delta terms in
the brand checklist.

Resizing: brief §7 requires 80–200% UI scaling — implemented as the Anamorph whole-window
transform compose (`setScaleFactor` override, `Anamorph:src/PluginEditor.cpp:1312-1342`
[Verified]) with the ⊕ step list in §4.3, plus **free host resize is still off** (discrete
steps, size persists via `int_uiScale`).

Accessibility (**deliberate delta**): Anamorph has none [Verified — zero handlers in src/].
Brief §8 requires complete parameter/automation names, keyboard operability and a
colour-blind-safe metering palette — adopt JUCE accessibility (titles/descriptions on every
control, focus order, keyboard value entry) from P5, and it goes on the brand checklist as a
per-item gate.

OpenGL: follow Anamorph ADR-0011 — attach on macOS/Windows, never on Linux/X11 (host
use-after-free), CPU path visually identical [Verified].

### 6.2 Simple view wireframe (940×720 logical ⊕)

**⊕ The two frame sizes are Anamorph's, unexamined.** 940×720 / 940×900 are the sibling
product's hard-coded constants (`Anamorph:src/PluginEditor.h:295-302` [Verified]), reused here so
the wireframes have a concrete grid — but a maximizer's content is not a widener's, and the P0
research pass concluded Anabasis must pick its own base geometry. Ratifying them at sign-off
ratifies the sibling's exact frame; the honest expectation is that P5 re-derives them from the
real control inventory. Carry over the *pattern*, not the numbers: Anamorph keeps these constants
in one place because `paint()` and `resized()` both depend on them, and splitting them is how
they drift.

All strings in both wireframes — the sub-brand line ("MASTERING MAXIMIZER"), control labels,
readout formats — are **placeholders** (C8: product wording is owner-supplied; meter values are
illustrative).

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ ANABASIS  MASTERING MAXIMIZER      ‹ Preset ›   A/B Copy ↶ ↷  ⚙ ADV [BYPASS] │ 46
├──────────────────────────────────────────────────────────────────────────────┤
│                                                          ┌─────────────────┐ │
│                 ╭────────────╮                           │  LUFS  M  S  I  │ │
│                ╱              ╲                          │   -9.5  ▐▌      │ │
│               │    LOUDNESS    │                         │  target lines   │ │
│               │      ◉ 42      │                         │  Sp Ap YT ─ ─ ─ │ │
│                ╲              ╱                          │  TP  -1.02 dBTP │ │
│                 ╰────────────╯                           │  PLR    8.5     │ │
│        the one knob — visual focus                       │  penalty  -4.5  │ │
│                                                          └─────────────────┘ │
│   CHARACTER ◯──────   TONE ◯──────   CEILING ◯ -1.0 dBTP 🔒                  │
│   [Loudness Comp]  [Delta]  [Freeze]  [Learn]            out LUFS   -9.5     │
├──────────────────────────────────────────────────────────────────────────────┤
│  GR history (10–30 s scrolling waveform + GR trace)                ▁▂▄▂▁▂▆▁  │
└──────────────────────────────────────────────────────────────────────────────┘
```

### 6.3 Advanced view wireframe (940×900 ⊕ — zones + shared bottom metering strip, brief §8)

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ top bar (identical)                                                          │ 46
├──────────────┬───────────────────┬───────────────────┬───────────────────────┤
│ COMP         │ CLIP / COLOUR     │ LIMITER           │ EQ         [Pre|Post] │
│ ratio thr    │ shape ▁curve▁     │ gain ceiling 🔒   │ tilt  LS HS           │
│ atk rel auto │ drive mix         │ lookahead rel auto│ bell1 bell2           │
│ knee det mix │ model bal tone    │ style link trans  │ (f/g/Q each)          │
│              │ depth  dyn-tame   │                   │                       │
│ [GR meter]   │ [live curve]      │ [GR meter] TP[on] │ [response curve]      │
├──────────────┴───────────────────┴───────────────────┴───────────────────────┤
│ SC HPF ◯  input gain ◯  dither [Off|16|24] shaping[ ]  [Comp][Delta][Freeze] │
│ macro (read-only, with detach badges): L ◯  C ◯  T ◯      adaptive Δ overlay  │
├──────────────────────────────────────────────────────────────────────────────┤
│ metering strip: GR history ▂▄▆ · LUFS M/S/I · TP · PLR · spectrum (dismiss)  │
└──────────────────────────────────────────────────────────────────────────────┘
```

### 6.4 Settings page (Backdrop, Anamorph row grammar)
Oversampling `Off/2×/4×/8×/16×` · Phase `Minimum/Linear` (latency note in tooltip) · Offline
render `Follow/Force Max` · UI scaling `80…200%` · UI Animation toggle · Tooltips toggle ·
Metering: target-line selection + true-peak meter toggle. All bound to `ANABASIS_INTERNAL`.

### 6.5 Animation & rendering
FrameClock (vblank-paced, ~125 Hz cap, dt-correct) + static-layer caching + atomic meter
sources + snapshot repaint gates, copied as-is [Verified `Anamorph:src/gui/FrameClock.h:10-167`,
`src/gui/LevelMeter.cpp:12-73`]. The UI Animation toggle gates micro-anims/sweeps/reveals but
never meter ballistics (Anamorph's split) — and per brief §8, disabling it must not affect function.
brief §8's 60 fps target is met by vblank pacing (FrameClock caps at ~125 Hz and follows the
display's cadence — 60 Hz displays run at 60 fps).

---

## 7. Presets, A/B, undo

Copy the Anamorph state machinery wholesale (patterns [Verified], §4.4), with the slot unit
**widened by two fields**: StateSet `{params, presetName, baseline, frozenTrims, detachMask}` is
the A/B-slot **and undo** unit. The widening is not cosmetic — with the narrow unit, undoing a
manual edit would restore the value and strand its detach bit (the parameter returns to its curve
value yet stays badged *edited* and out of the macro), and undoing a `freeze` toggle would
restore the parameter without the trim vector it latched. Anything that is per-slot must also be
per-undo-step, because undo is a per-slot stack; per-slot undo stacks (cap 128,
never serialized); gesture-gated undo coalescing with host automation folded silently;
preset-load undo bracketing (parse before the bracket opens); `requestDuck()` before every bulk
swap; per-slot adaptive/Learn memory injected at the duck's silent bottom (transport = **OQ-013**,
a Hard Stop — §5.4). Factory presets:
≥12, compiled-in override tables; the brief names five (Transparent Master, Loud Pop, EDM Club,
Vocal Forward, Tape Glue) — the full list + wording is owner-supplied at P6 (C8, TODO).
Ceiling lock semantics per §4.2. A/B loudness-matched comparison works out of the box because
loudness compensation is monitoring-layer (§2.7) and per-slot compensation memory restores at
the duck bottom.

---

## 8. OQ-005 — shared `rollytech-ui` module: recommendation = copy-and-adapt now
Extraction couples two release cycles, requires coordinated changes in a shipped product (an
Architecture Review Gate item *there*), and Anabasis cannot modify Anamorph in any case
(CLAUDE.md cross-repo rule). The brief prioritises shipping on schedule (brief §1.2). Recommendation:
**copy-and-adapt** LookAndFeel/glass/FrameClock/Backdrop/InternalState idioms now, keep the
copied files' provenance headers pointing at the Anamorph originals, and revisit extraction as a
product-family ADR after Anabasis v0.1.0 ships — when both products' UI layers are stable enough
to see what is actually common.

**This decision is recorded by ADR-0009** (§10), not by this section and not by
`OPEN_QUESTIONS.md`: `CLAUDE.md` §3 requires code reuse across the two products to be recorded in
an ADR rather than made as an ad-hoc copy, and per `SOURCE_OF_TRUTH.md` a DESIGN decision binds
only through the ADR that names it. ADR-0009's scope is deliberately wider than this section —
it covers the DSP-source adaptations too (§2.4, §2.7–2.9), which are the copies most likely to be
questioned at P2.

---

## 9. Performance budget and benchmark plan

Target (brief §10): ≈5% of one modern desktop core at 48 kHz stereo 4× OS. Draft **allocation**
(⊕ targets, not measurements — C2): OS resampling ≤1.5% · limiter + TP detection ≤1.5% ·
clipper/ADAA ≤0.8% · compressor ≤0.3% · EQ ≤0.3% · metering + features ≤0.5% · headroom ≥0.1%.
Anabasis commits at P2 to the benchmark procedure Anamorph only prescribes
(`Anamorph:docs/architecture/PERFORMANCE_BUDGET.md:207-266` [Verified]): an OFF-by-default
bench target compiling the engine sources, shipped-Release flags, SR × block × OS × mode
matrix, ns/sample + worst-block, median of ≥5 runs, machine recorded — results land in
`TEST_REPORT.md` and `PERFORMANCE_BUDGET.md`.

---

## 10. The ADR set this document spawned (per brief §16) — all Accepted 2026-07-31

| ADR | Title | Settles |
|---|---|---|
| 0001 | Format-agnostic DSP core via POD `EngineParameters` | §1.1 (inherits Anamorph ADR-0001 pattern) |
| 0002 | Fixed serial signal chain; EQ Pre/Post as the only mobility; **ceiling clamp always last before dither**. Carried a **Hard-Stop amendment to `DSP_POLICY.md` invariant 1** (Limiter → EQ(post) → Ceiling) — ratified and **landed at sign-off** | §1.2, §2.6 / DSP_POLICY inv 1+4 |
| 0003 | Oversampling scope + **true-peak as measurement tap, ≥4× total at every OS setting** + linear-phase & Force-Max modes | §3.1–3.2; closes DSP_POLICY inv 2/5 open point |
| 0004 | Latency contract: **reported = CONSTANT max-lookahead allowance (10 ms) + OS**, engaged lookahead free inside a fixed line; only OS latches; latency inputs host-hidden and `lookahead` non-automatable; **no zero-lookahead position**. Carried a **Hard-Stop amendment to `DSP_POLICY.md` invariant 2** (latch sentence + allowance phrasing) — ratified and **landed at sign-off** | §3.3–3.4; resolves OQ-010 |
| 0005 | Macro-layer architecture: message-thread mapper, non-automatable macros, detach/re-engage coexistence, adaptive trims engine-internal | §5; resolves OQ-004 |
| 0006 | Ceiling guarantee: separate final clamp, ≤0.1 dBTP, monitoring never in render path | §2.6–2.7 |
| 0007 | State schema v1: explicit `schemaVersion`, raw-exact sessions, snapped presets, global `ADAPTIVE` child for learned targets, and **per-A/B-slot detach mask + frozen trim vector inside each `AB` slot**; **presets carry the detach mask** | §4.4, §5.3 |
| 0008 | **Build architecture and plugin identity**: CMake structure (INTERFACE `AnabasisDSP`, hardening target, one shared source list), **JUCE 9.0.0 pinned by commit `f8f8864…`**, **C++20** baseline + the C++23 feature-test-macro strategy, formats VST3 / AU / Standalone, and the frozen identity `RTec` / `Anbs` / `com.rollytech.anabasis` / categories Fx-Dynamics-Mastering | §11 P1, §1.1; **mandatory** — `ADR_POLICY.md` requires an ADR for build architecture *and* format support, and OQ-001's standing obligation names "the P0 build-decision ADR"; closes OQ-001 + OQ-003 |
| 0009 | **Code reuse from Anamorph — all of it, not just UI**: the GUI idioms (LookAndFeel, glass, FrameClock, Backdrop), the wrapper/state machinery (InternalState, StateSet A/B + undo, PresetManager, raw-exact serialization) **and the DSP-source adaptations** (K-weighting coefficients and the Measure+Predict structure from `LoudnessMatch`, the `ScopeBuffer` SPSC ring, the duck/crossfade transition taxonomy, the `driveTanh` peak-preserving makeup formulation) — copy-and-adapt with provenance headers, **no shared module for v1**; revisit extraction after v0.1.0 | §8, §2.4, §2.7–2.9, §7; **mandatory** — `CLAUDE.md` §3 requires code reuse across the two products to be recorded in an ADR, not an ad-hoc copy; resolves OQ-005 |
| 0010 | **Parameter surface**: the 49 IDs, types, ranges, defaults and choice orderings; the two **exclusion tiers**; the **lockable set** `{ceiling}`; macro automatability | §4.2–4.3; **mandatory** — `ADR_POLICY.md` requires an ADR for parameter semantics, and `PARAMETER_COMPATIBILITY_POLICY.md` rule 6 makes exclusion lists and the lockable set contract |
| 0011 | **Threading model**: two threads, no workers; POD snapshot per block; relaxed-atomic + SPSC publication; message-thread-only MacroEngine and PDC updates | §1.4, §5.2; **mandatory** — `ADR_POLICY.md` lists the threading model |

**All eleven were authored directly as `Accepted`, dated 2026-07-31, at the sign-off** — each
citing this document and the P0 worklog as its evidence (C1; code evidence accrues from P1
onward). They were deliberately *not* written speculatively as `Proposed` beforehand: an ADR whose
decision has not been ratified is exactly the "predefined quota" C1 forbids, and this document
already served the reviewable-proposal role.

**`docs/architecture/design-decisions/ADR_INDEX.md` is now the registry of record, not this
table** (`ADR_POLICY.md` rule 1). This table is retained as the map of *what each ADR was asked to
settle*; where it and an ADR disagree, the ADR wins (`SOURCE_OF_TRUTH.md`). Numbering is
Anabasis-local, no reserved blocks.

---

## 11. Phase mapping, risks, and what sign-off unblocks

- **One genuinely new test name needs a home**: `testMacroDefaultIsFixedPoint` (§5.5). The other
  two guards this document leans on are already registered —
  `testReportedLatencyMatchesImpulse` is row 2 of `DSP_POLICY.md`'s invariant→test map and
  `testModeSwitchIsSoundNeutral` is the named guard for `MODE_AND_ADAPTATION_POLICY.md`
  invariant 2. The new one guards a **macro-layer** rule, so `DSP_POLICY.md`'s map is the wrong
  destination: it belongs in `MODE_AND_ADAPTATION_POLICY.md` (with invariant 1) and in
  `procedures/TESTING.md`, added in the same unit of work as the test itself
  (`DOCUMENTATION_LIFECYCLE_POLICY.md`'s new/changed-test trigger).
- **P1 skeleton** (after sign-off): CMake per brief §18 (identity `RTec`/`Anbs`/
  `com.rollytech.anabasis` frozen — OQ-003; JUCE 9.0.0 @ `f8f8864…` — OQ-001; C++20), the §4
  parameter surface + registry snapshot, pass-through chain + basic limiter, state harness,
  pluginval L5. OQ-011 (macOS deployment target) is checked and set at P1 as planned.
- **P2–P6** follow brief §11 unchanged; the §10 ADRs gate their areas.
- **Top risks** — the two that are open dependencies rather than execution quality are
  **registered in `FUTURE_RISKS.md` now**, not conditionally later, because a risk recorded only
  inside a document that is superseded section by section (`SOURCE_OF_TRUTH.md`) is a risk that
  gets lost: (1) macro-curve quality is the product — the §5.5 curves are drafts and the
  Master Plan benchmark (§5.4 of the brief) is the arbiter; budget P4 listening time accordingly
  (execution quality, covered by RISK-004's adaptation-audibility framing). (2) The
  measurement-tap latency argument rests on the detector-delay-≤-min-lookahead constraint —
  **RISK-008**; verify with the first impulse test at P2. §3.3's constant-allowance decision
  makes the fallback cheap: a detector delay exceeding the 0.5 ms *minimum engaged* lookahead is
  absorbed by raising the minimum read offset inside the fixed 10 ms line, so the **reported**
  figure does not move and no ADR amendment or Architecture Review is triggered. Verify anyway —
  the accuracy contract (inv 11) is separate from the latency one. (3) Variable-font licensing (§6.1) needs owner
  approval lead time before P5 — **RISK-009**. (4) `dither`/`truePeakMode` non-automatability choices are conservative-frozen — loosening
  later is a kVersion bump.

**Sign-off checklist for the owner**: **the §1.2 Hard-Stop item — Post-EQ sits *before* the
ceiling clamp, which requires ADR-0002 to amend `DSP_POLICY.md` invariant 1's chain wording** ·
**§3.3 constant reported latency — the plugin reports 10 ms of lookahead allowance even at the
2 ms default, buying immunity from PDC changes while browsing presets** ·
**every ⊕ value in the document**, not only the tables — the §4.2/§4.3 surface (frozen at
v0.1.0), the ⊕ window geometry in §6.2/§6.3 (Anamorph's frame sizes, §6.2), and the ⊕ shelf Q in
§2.2 — **including the ⊕ `Tape` default colour model — a taste call that decides whether the
Character macro is audible in the factory patch (§2.4)** · display names as launch wording
(revisable later, rule 2) · §3.2 measurement-tap · §3.4
no-zero-lookahead · §5.2/§5.3 macro architecture & coexistence · §5.5 draft curves as the P4
starting point · §6.1 accent-family + variable-font direction · §8 copy-and-adapt · §10 ADR set.
