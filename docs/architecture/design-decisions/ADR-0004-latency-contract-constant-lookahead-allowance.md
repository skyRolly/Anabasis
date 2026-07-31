# ADR-0004 — Latency contract: reported = constant max-lookahead allowance + oversampling

**Status:** Accepted (2026-07-31 — owner sign-off on `docs/DESIGN.md`)

## Context

`DEVELOPMENT_BRIEF.md` §4.3 specifies the limiter lookahead as **0.5–10 ms** and leaves the question
of a zero/off position open as OQ-010. `DSP_POLICY.md` invariant 2 requires that reported latency be
*exact* — "every latency source is reported to the host so PDC compensates correctly, not a subset
chosen for convenience" — and left an open point on how the lookahead term is expressed. Both
`ADR_POLICY.md` (latency behaviour is ADR-mandatory) and `CLAUDE.md` / `ARCHITECTURE_REVIEW_GATE.md`
(a reported-latency change is an AI-agent Hard Stop) make this a decision that cannot be settled in
code or in a diagram.

Anabasis inverts the sibling product's premise. Anamorph's **only** latency source is oversampling,
which is a host-hidden setting, so its reported latency is 0 in the common case and changes only on a
settings action the user takes deliberately
(`Anamorph:src/dsp/AnamorphEngine.cpp:44-56`, its ADR-0003 [Verified]). Anabasis has a lookahead
limiter that is always engaged, so latency is always non-zero — and the P0 research pass flagged
precisely this as a decision Anabasis owes, together with its consequence for the forced duck: the
Anamorph dry-fill engages only when `predictLatency == latched latency`
(`Anamorph:src/dsp/AnamorphEngine.cpp:290-307` [Verified]), so any swap that crosses reported latency
ducks to **silence** instead of filling with the delay-aligned dry signal.

`DESIGN.md` §3.3 deliberately left `DSP_POLICY.md` untouched and deferred the amendment here, carrying
the decision on the §11 sign-off checklist as a Hard-Stop line rather than a preference.

**This ADR carries a policy amendment.** See *Decision*, item 8.

## Problem

`lookahead` (row 27 of the §4.2 surface) is an ordinary APVTS float in **neither exclusion tier** — the
view tier is `{bypass, loudnessComp, deltaMonitor, advancedMode}` and the preset-excluded tier adds
only `{freeze}` (§4.2). It is therefore carried by **every preset, every A/B slot and every undo
step**, and each of those is applied as a bulk swap through the forced duck (§7).

So the obvious latency model, `engagedLookahead + osLatency`, has a consequence that is not visible
from the formula: **browsing presets or A/B-comparing during playback would change host PDC on nearly
every step.** Each such step is a host re-sync or dropout in the middle of the one workflow a
mastering plugin exists to support, and the dry-fill gate above means the duck cannot even mask it —
a latency-crossing swap dips to silence by construction. The question is not obvious because the
naive model is the *more* correct-looking one: it reports what is actually delaying the signal at
that instant, which is exactly what invariant 2 demands.

Two sub-questions ride with it: whether the range needs a zero/off position (OQ-010), and — if the
reported figure stops depending on lookahead — whether the reasons for keeping `lookahead`
non-automatable survive.

## Options

- **A. `engagedLookahead + osLatency`, latched at the silent duck bottom like the OS factor; bulk
  swaps that cross latency duck to silence.** The honest, minimal model, and it reuses the Anamorph
  latch machinery verbatim. **Lost:** it makes reported latency a function of an ordinary sound
  parameter that presets, A/B and undo all carry, so the *normal* mastering workflow — audition
  preset, A/B against the last one, undo — sprays PDC changes at the host. The dry-fill gate
  (`predictLatency == latched latency`) then fails on those same steps, so the masking degrades from
  "crossfade against the delay-aligned dry input" to "dip to silence" exactly when the user is
  comparing two candidate masters. It also forces a latch and a forced duck onto a control that would
  otherwise be a smooth continuous move, and it makes `testReportedLatencyMatchesImpulse` a matrix of
  moving targets rather than a fixed one.
- **B. Report the CONSTANT maximum lookahead allowance (10 ms) + `osLatency`; the limiter reads at a
  variable offset inside a fixed 10 ms delay line and the engine pads the difference.** Reported
  latency becomes independent of every host-visible parameter. **Chosen.**
- **C. Quantise `lookahead` to a small set of discrete positions and duck-switch the latency at each.**
  Bounds the churn and keeps the reported figure "true". **Lost:** it reduces the frequency of PDC
  changes without removing them — a preset step that lands on a different position still crosses
  latency and still cannot dry-fill — while freezing a coarse grid into a parameter range that is
  compatibility contract from v0.1.0 (`PARAMETER_COMPATIBILITY_POLICY.md` rule 3). It pays the full
  cost of option A's machinery for a partial version of option B's benefit.
- **D. Move `lookahead` into host-hidden `ANABASIS_INTERNAL` (§4.3) so presets, A/B and undo never
  carry it, then report the engaged value safely.** Structurally sound — it is how the OS factor
  already escapes this problem. **Lost:** `ANABASIS_INTERNAL` is for *non-musical* settings; lookahead
  is a sound parameter (it sets how far ahead the gain computer sees a transient, which is audible
  limiter character). A preset that cannot carry its own lookahead is not a reproducible sound
  document, and an A/B comparison of two limiter settings that shares one lookahead compares the wrong
  thing.
- **E. Keep `lookahead` host-visible but add it to an exclusion tier so presets/A-B/undo skip it.**
  Same effect as D without hiding the control. **Lost:** for the same reproducibility reason, and the
  exclusion list is itself frozen contract (`PARAMETER_COMPATIBILITY_POLICY.md` rule 6) — spending a
  permanent exclusion on a musical parameter to work around a latency-model choice inverts cause and
  effect.
- **F. Report a fixed figure that is *not* the maximum (e.g. the default) and absorb the rest.**
  Cheaper PDC. **Lost:** it under-reports for every lookahead above the reported value, which is a
  direct violation of invariant 2's exactness clause and desynchronises the host by the difference —
  a wrong answer, not a trade.
- **G. Add a zero/off lookahead position (the OQ-010 alternative).** Would give a "no added lookahead
  latency" state. **Lost:** a 0 ms limiter cannot act ahead of a transient — it degenerates into a
  clipper, which this chain already carries as a *better* tool for that job (§2.4); the zero-latency
  tracking use case belongs to a different product class; and widening the range later re-scales every
  saved session (`PARAMETER_COMPATIBILITY_POLICY.md` rule 3), whereas narrowing never has to happen.
  Under option B it would additionally re-introduce the one thing B removes: a reachable state in
  which the reported figure differs, hence PDC churn between "off" and "on".

## Decision

1. **The latency contract is:**

   ```
   reportedLatency = maxLookaheadSamples(10 ms, sr)      ← CONSTANT, not the engaged value
                   + osLatency(factor, phaseMode)        ← 0 when the factor is Off
   ```

   No other stage contributes: the EQ is IIR, the true-peak detector is a measurement tap that adds
   nothing (ADR-0003, §3.2), and dither is sample-wise. Values are computed at `prepare()` from
   `getLatencyInSamples()` and recorded as **measured** numbers in `LATENCY_MODEL.md` at P2 — not
   predicted here (constraint C2).

2. **The lookahead term is the maximum, always.** The delay line is sized for 10 ms at `prepare()`.
   The **audio** delay through the limiter stage stays fixed at the full 10 ms for every engaged
   lookahead value; the `lookahead` parameter moves only the *detector / gain-computer* read offset
   inside that line, and the engine pads the difference. The engaged value therefore changes freely
   while the reported figure never moves. No lookahead change ever allocates.

3. **Click-free story (§2.8).** Because the audio path length is invariant, no audio sample is ever
   skipped or repeated when `lookahead` moves; only a smooth, band-limited control alignment changes.
   It is still a switchable path under `DSP_POLICY.md` invariant 8 and gets its **own per-path
   click-free test** (P1 test obligations, §11).

4. **Latching applies to the oversampler only.** An OS factor or phase-mode change is latched and
   applied at a reset or the silent duck bottom (§2.8), so effective latency never changes mid-block.
   **A lookahead change is neither latched nor ducked** — it is an ordinary smoothed control move.

5. **PDC update path.** A single `setLatencySamples` call site fed by a const, race-free
   `predictLatency(snapshot)` callable from the message thread (§1.4; precedent
   `Anamorph:src/PluginProcessor.cpp:88-105` [Verified]). Recomputation is triggered only by
   `prepare()` and by the `int_oversample` / `int_osPhase` `onChanged` callbacks (§4.3). At
   `int_offlineQuality = Force Max` an offline bounce renders at 16× and the reported figure during
   `isNonRealtime()` uses the forced factor (§3.1) — the factor change rides the existing latched
   switch at render start.

6. **`lookahead` is non-automatable — for a different reason than before.** It no longer moves PDC, so
   the PDC-spray argument is retired. The surviving reason (§4.2 footnote ³) is that the engaged value
   **is a read offset into a live delay line**: sweeping it at automation rate drags the tap through
   the buffer and produces pitch/comb artefacts. It is a set-and-leave control. The adaptive engine is
   likewise barred from touching it (`MODE_AND_ADAPTATION_POLICY.md` invariant 4). The OS factor and
   phase mode remain latency-affecting *and* host-hidden, so neither surface can spray PDC changes.

7. **No zero/off lookahead position (OQ-010 resolved).** The range is exactly **0.5–10 ms**, default
   2 ms, logarithmic (row 27). Row 27's footnote ⁶ states this in the table so the range cannot read
   as an oversight. Consequence: the plugin **always** reports non-zero latency, and "reports 0" is
   not a reachable state to test for.

8. **Policy amendment (this is the Hard-Stop item ratified at sign-off).** `DSP_POLICY.md` invariant 2
   is amended in two places, and `ADR_POLICY.md` rule 5 makes this ADR the instrument that enacts it:

   - **(a) Lookahead leaves the latch sentence.** The invariant's body read "an oversampling-factor
     **or lookahead** change is **latched** and applied at a reset or a crossfaded boundary". Under
     this decision a lookahead change alters no reported figure, so only the OS half stays latched:

     > Latency must never change mid-block: an **oversampling-factor** change is **latched** and
     > applied at a reset or a crossfaded boundary. *(Pre-ADR-0004 this sentence also named lookahead;
     > a lookahead change no longer alters any reported figure, so it is an ordinary smoothed
     > read-offset move — but it is still a switchable path under invariant 8 and needs its own
     > click-free test.)*

   - **(b) The invariant's open point is re-phrased against the lookahead *allowance*.** It was
     written against "the engaged lookahead"; the reported figure is the constant allowance, and the
     invariant states the formula of item 1 with the `maxLookaheadSamples(10 ms, sr)` term marked
     CONSTANT, together with the compatibility reason (presets/A-B/undo all carry `lookahead`) so the
     constant is not read as sloppiness.

   Invariant 2's guard is strengthened in the same edit: `testReportedLatencyMatchesImpulse` measures
   across the **oversampling × lookahead matrix**, and the impulse must land at exactly
   `maxLookahead + OS` for *every* lookahead value — so a padding bug is a test failure rather than a
   subtle host-sync complaint. A reported-latency change is an AI-agent Hard Stop and an
   `ARCHITECTURE_REVIEW_GATE.md` item (`AI_AGENT_POLICY.md`); it was carried on the `DESIGN.md` §11
   sign-off checklist as a Hard-Stop line and ratified by the owner on 2026-07-31.

9. **Buffer sizing.** The dry / bypass ring is sized
   `maxLookahead(10 ms) + maxOsLatency(16×, linear) + maxBlock + 1`. All oversampler instances for
   every factor are constructed and `initProcessing`'d at `prepare()` so a factor switch never
   allocates (precedent `Anamorph:src/dsp/AnamorphEngine.cpp:44-56` [Verified]).

## Consequences

- **Bulk swaps can never cross reported latency.** OS factor and phase mode are the only remaining
  latency sources and both are host-hidden `ANABASIS_INTERNAL` settings that never travel with
  presets, A/B or undo (§4.3). A preset step, an A/B switch and an undo step are therefore *always*
  dry-fillable and never touch PDC — which is what makes §7's "copy the Anamorph state machinery
  wholesale" safe here rather than merely convenient.
- **The forced duck keeps its best masking mode in the workflow that matters.** The Anamorph gate
  `predictLatency == latched latency` is satisfied by construction for every bulk swap, so the duck
  crossfades against the delay-aligned raw input instead of dipping to silence.
- **Lookahead needs no latch and no duck**, only a smoothing and its own click-free test. Only OS
  factor/phase still latch at a reset or the silent duck bottom.
- **The cost is real and is the trade:** at the 2 ms default the plugin reports 10 ms rather than
  2 ms — roughly 8 ms of PDC the user does not "need", plus a delay line that is always full-size
  regardless of the engaged value. For a mastering processor that is cheap (this is not a tracking
  tool, and item 7 already declines a zero-latency mode); the alternative bought with it is immunity
  from PDC churn while browsing presets. Ratified on the §11 checklist, not assumed.
- **The plugin always reports non-zero latency.** Anamorph's premise — latency is 0 unless OS is
  engaged — does not carry across, and neither do tests built on it: true-bypass null tests must
  account for constant PDC, and the bypass/dry ring is always exercised rather than being a rarely
  hot path.
- **RISK-008's fallback becomes cheap.** If the P2 impulse test finds the true-peak estimator's group
  delay exceeds the 0.5 ms *minimum engaged* lookahead (the constraint ADR-0003 rests on), it is
  absorbed by raising the minimum read offset inside the fixed 10 ms line: the **reported** figure
  does not move, so no ADR amendment and no Architecture Review is triggered. The accuracy contract
  (invariant 11) is separate and must be verified regardless.
- **Forecloses:** a zero/off lookahead position; widening the 0.5–10 ms range (rule 3 — it re-scales
  saved sessions); automating `lookahead`; making the reported figure depend on any host-visible
  parameter again; and reporting less than the full allowance. Each of those re-enters the
  Architecture Review Gate as a reported-latency or parameter-range change.
- **Doc-sync obligation:** the invariant 2 edits above land in `DSP_POLICY.md` with this ADR, and the
  ADR is registered in `ADR_INDEX.md` (`ADR_POLICY.md` rule 1); `LATENCY_MODEL.md` is created at P2
  with **measured** sample counts and the source table (every non-lookahead, non-OS stage listed as
  0); `procedures/TESTING.md` gains the lookahead click-free path test and the widened
  `testReportedLatencyMatchesImpulse` matrix (`DOCUMENTATION_LIFECYCLE_POLICY.md`).

## Related code

None yet — P1 onward. Planned: `src/dsp/LookaheadLimiter.{h,cpp}` (fixed 10 ms line, variable
detector read offset), `src/dsp/AnabasisEngine.{h,cpp}` (latency padding, `predictLatency(snapshot)`,
latched OS switch, duck/dry-fill), `src/PluginProcessor.{h,cpp}` (single `setLatencySamples` call
site, PDC recompute on `int_oversample` / `int_osPhase`), `src/InternalState.h` (`int_oversample`,
`int_osPhase`, `int_offlineQuality` + `onChanged`), `src/dsp/EngineParameters.h` (POD snapshot
carrying `lookahead`), `src/PluginParameters.{h,cpp}` (`pid::lookahead`, row 27, non-automatable),
`src/dsp/TruePeak.h` (estimator whose group delay sits inside the minimum read offset).

Evidence [Unverified] — Anabasis has no `src/`, so every runtime claim above is the contract the code
must satisfy, not a measurement (constraint C2):

- Design: `docs/DESIGN.md` §3.3 (the latency formula, the constant-allowance decision and its
  Hard-Stop callout), §3.4 (OQ-010 — no zero/off position), §2.5 (lookahead 0.5–10 ms, default 2 ms),
  §2.8 (transition taxonomy; lookahead's own click-free story), §3.1 (OS region, integer-latency IIR,
  linear-phase FIR, Force-Max offline reporting), §3.2 (true-peak as a measurement tap, contributing
  no delay), §4.2 row 27 + footnotes ³ and ⁶ + the exclusion tiers, §4.3 (`int_oversample`,
  `int_osPhase`, `int_offlineQuality` host-hidden, never in A/B, undo or presets), §1.3 (planned
  module inventory), §1.4 (message-thread PDC, no allocation at switch time), §7 (bulk swaps through
  the forced duck), §10 (this ADR's scope), §11 (sign-off checklist Hard-Stop line; RISK-008)
- Research: `worklogs/2026-07-30-p0-anamorph-research.md`
- Policy amended by this ADR: `docs/policies/DSP_POLICY.md` invariant 2 (`ADR_POLICY.md` rule 5)
- Precedent [Verified]: `Anamorph:src/dsp/AnamorphEngine.cpp:290-307` — the forced-duck dry-fill
  engages only when `predictLatency == latched latency`; the gate this decision satisfies by
  construction
- Precedent [Verified]: `Anamorph:src/PluginProcessor.cpp:88-105` — single `setLatencySamples` call
  site fed by a const, race-free `predictLatency(snapshot)` from the message thread
- Precedent [Verified]: `Anamorph:src/dsp/AnamorphEngine.cpp:44-56` — oversamplers built at
  `prepare()` with `useIntegerLatency=true` (exact PDC, no allocation on a factor switch); its
  ADR-0003 also supplies the latched-OS-engagement rule kept here
- Precedent [Verified]: `Anamorph:src/InternalState.h:10-29` — host-hidden settings with an atomic
  mirror and a latency-recompute callback, the mechanism that keeps the OS term off presets/A-B/undo
- Precedent [Verified]: `Anamorph:src/PluginParameters.h:66-88` — the single shared exclusion
  predicate whose tiers place `lookahead` outside both, which is the fact that forces this decision
