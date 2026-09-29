# ADR-0041 — The ceiling clamp's true-peak path, inside the constant latency allowance

> **✅ RATIFIED — THE ARCHITECTURE REVIEW GATE IS CLEARED (2026-09-27).** The owner accepted this
> record as revised in the PR #42 review, and settled on the same day the question its decision 7
> left open — which meter "dBTP" is defined on — as its own record,
> [ADR-0043](ADR-0043-dbtp-is-defined-on-the-product-meter-and-the-annex-2-filter.md) (the product
> meter and the BS.1770 Annex 2 filter). How it arrived stays in the record, because
> that is the half worth keeping. This record implements a decision that was already Accepted —
> ADR-0006 items 2 and 3, the clamp's own true-peak estimate driving its gain — and that the code
> never carried out (audit finding **DSP-001**,
> `docs/reports/2026-09-26-anabasis-product-ux-audit/findings-dsp-tech.md`). Carrying it out was not
> free of gate items, and they were named here rather than left for a reviewer to find:
>
> 1. **A conflict with Accepted ADR-0004.** The true-peak path needs lookahead of its own; to keep
>    the reported latency constant it takes that lookahead out of the 10 ms allowance, which amends
>    ADR-0004 items 1, 2 and 7 **for true-peak mode only** (item 1: "no other stage contributes";
>    item 2: the audio delay through the limiter stage is the full 10 ms; item 7: the lookahead
>    range is exactly 0.5–10 ms). The **reported** latency does not move by a sample — only its
>    composition.
> 2. **A wording departure from Accepted ADR-0006 item 2** ("both instances come from the shared
>    BS.1770-4 estimator"): the clamp's detector evaluates the shared estimator's phases *and* two
>    more readings (decision 3). ADR-0006's purpose for item 2 — an estimate of the clamp's own
>    input, a measurement tap that never resamples the audio — is kept exactly.
> 3. **A DSP signal-flow change** (`DSP_POLICY.md` Enforcement: "any change to … the ceiling
>    clamp, or the latency contract"), and a **policy amendment**: invariant 2's body and invariant
>    8's enumeration, carried in decision 8 as prescribed text.
>
> It was filed `Proposed`, flagged in the pull request as a gate item a green build does not clear,
> revised once in the PR #42 review, and held there until the owner answered.

**Status:** **Accepted — 2026-09-27**, on the owner's explicit approval of this record (the
instruction of record: "Accept Phase 0 Decisions, Close PR #42"). It was NOT covered by the standing
blanket approval for the post-v0.1.0 rounds — a conflict with an Accepted ADR and a signal-flow change
are gate items that approval never reached. The approval is of the design recorded below — the four
items under "What the owner was asked to decide", and decision 8's prescribed policy text as
completed at acceptance — and explicitly *not* of the voicing constants (attack 0.25 ms, release
10 ms), which stay ⊕ listening material, nor of the TP-mode cost at ≥ 4× over DESIGN §9's
limiter + TP-detection row (`PERFORMANCE_BUDGET.md`), which stays recorded.

> **Amended 2026-09-28 by [ADR-0046](ADR-0046-the-true-peak-clamp-eases-in-and-engages-from-12-khz.md)
> (decisions 3 and 4, and the "rail" sentence of decision 4; on the owner's direction, ⊕ for
> review).** The gain law of decision 3 — "a linear attack ramp" over A = max(8, round(0.25 ms · fs))
> — bounded a segment's reading only while the gains its interpolation read were equal, and at the
> 8-sample floor a worst-case burst read up to +0.157 dB over the ceiling below 44.1 kHz (KI-025).
> The ramp is now a geometrically weighted mean that eases in, the release rises at most 1 % per
> sample, and the attack floor is 16 samples, so **D = 46 at every rate below 66 kHz** (41 / 42 at
> 44.1 / 48 kHz in the figures below; unchanged at 88.2 kHz and up) and the longest engaged window in
> TP mode is 9.04 ms at 48 kHz (9.125 below). The composition rule, the reported latency, the three
> readings and the backstop are unchanged. Decision 4's rail — "no conforming rate reaches it" — is
> replaced by a stated rate contract: the path engages from 12 kHz (`truePeakPathEngages`), and
> below it the sample clip runs and the Ceiling reads dB. The text below keeps the figures it was
> accepted with. *(Ratified 2026-09-29: ADR-0046 cleared the Architecture Review Gate on the owner's
> explicit approval, so this amendment, filed ⊕ for review on 2026-09-28, is in effect. Its
> figures read more precisely in ADR-0046's ratification note: the D of the text below was up to
> 45 below 62 kHz, so the TP-mode window is shorter from 12 kHz to below 62 kHz and unchanged from
> 62 kHz.)*
>
> **Amended 2026-09-28 by [ADR-0045](ADR-0045-true-peak-mode-answers-to-the-ceiling-in-force-at-emission.md)
> (decision 3; on the owner's direction, ⊕ for review).** "Each frame carries the ceiling the limiter
> used for it" made every true-peak frame answer to the ceiling in force when it ENTERED the clamp,
> `truePeakDelay` samples before it left — so while the Ceiling descended the output read up to
> +2.7 dB (−1 → −20 dB) over the ceiling the smoother held at that instant, where the TP-off clip,
> which reads the live value, had none. Each frame now answers to the ceiling in force at its
> EMISSION (the smoother's deterministic glide, run ahead; frames in flight revised at a retarget),
> and the limiter plays to the same value in true-peak mode. Decision 3 is otherwise unchanged; the
> latency composition, the three readings, the gain law and every static-ceiling sample are.
> *(Added 2026-09-29: ADR-0045 itself stays ⊕ pending the owner's review. The stamp it set is in
> turn amended by ADR-0046, ratified 2026-09-29: a true-peak frame answers to the LOWER of the
> ceiling at its entry and the one predicted at its emission — the same on a static or falling
> ceiling; on a rising one, the entry value — and the limiter plays to that same value.)*
>
> **Implementation note 2026-09-28 (decision 5, not an amendment).** "Adopted directly … on entering
> offline" did not hold for an engagement decay in flight: the offline-entry branch forced the duck
> to unity and left `EngagementTail` running, so the last realtime frame's decay played into the head
> of the render (up to +1.46 dB over the ceiling with the Post EQ's ring-out). The branch now resets the
> decay with the duck, and restarts the output dBTP tap when it cut one
> (`testOfflineEntryDropsTheEngagementTail`; `KNOWN_ISSUES.md` KI-004).

> **Revised 2026-09-27, before acceptance (review of PR #42).** A review found that ENGAGING true-peak
> mode while audio plays leaked the requested ceiling: the latch waited for the §2.8 duck's out-leg,
> and that out-leg is emitted by the composition being replaced, whose clamp is the sample clip —
> measured up to +4.7 dB (product meter) / +5.5 dB (Annex 2) over the ceiling after the toggle.
> Decision 5 is revised (engaging no longer rides the out-leg), decision 8's prescribed text with it,
> the detector moved into its own JUCE-free header (decision 2; the first cut had put a JUCE include
> under the ceiling stage and failed the `realtime` CI gate), and the Consequences, Related code,
> Evidence and a new "What the owner is asked to decide" section followed. Nothing else moved: the
> latency composition, the three readings, the gain law and every steady-state figure are unchanged
> and re-measured bit-identical (the PR #42 review worklog).

## Context

`DSP_POLICY.md` invariant 4 promises that the output never exceeds the ceiling, **tolerance
≤ 0.1 dBTP in true-peak mode**, and ADR-0006 decided how: a structurally separate final clamp whose
gain, with `truePeakMode` on, "acts on its own TP estimate, with a sample-level hard clip as the
backstop" (item 3). `CeilingClamp.h` never grew past its P1 comment — *"sample-level hard clamp …
The true-peak-driven gain half of ADR-0006 item 3 arrives with the TruePeak tap at P2"* — so in
true-peak mode the clamp still clipped samples, and nothing measured what that left between them.

Measured on `main` at ed06ad0 with a probe that drives the real `AnabasisEngine` (worklog
2026-09-27 §Investigation), over 2496 true-peak-mode configurations — six programme shapes × five
Loudness positions × eight of the nine oversampling cells × four limiter voicings × two ceilings (the
ninth, 8× linear phase, ran as a 240-configuration addendum with the same result), plus the Force
Max bounce, a +6/+12 dB Post shelf, 44.1/96/192 kHz, three block sizes, lookahead up to 10 ms and
Character 0.5 — **1674 exceed the ceiling by more than 0.1 dB on the product's own dBTP meter,
worst +4.80 dB**, and 1759 on the BS.1770 Annex 2 example meter (worst +6.12 dB). Only the LF-heavy
programme passes. The mechanisms that put the overs there are all by design upstream of the clamp,
which is why ADR-0006 made the clamp — not the limiter — the guarantee:

- **the decimation filter regrows peaks** after the region limiter at every oversampling factor,
  and the limiter's own true-peak detection is off at ≥ 4× (ADR-0003 item 6 — correct for its own
  detector, blind to what the down-filter adds);
- **the limiter's attack is deliberately slewed** by Transients and the Punchy style, letting
  fronts through to the clamp (`LookaheadLimiter.h`);
- **a Post-position EQ boost** sits between the limiter and the clamp (ADR-0002);
- at base rate, **a sample clip itself manufactures inter-sample overs**.

## Problem

**(a) A true-peak-driven gain needs lookahead, and ADR-0004 has none to give.** An estimate of the
waveform between two samples is available only after the interpolator has seen the samples on
either side, and a gain that must not step (invariant 8) needs a ramp before the peak. Both are
delay. ADR-0004 fixed the reported latency at `maxLookahead(10 ms) + osLatency` and said the whole
10 ms is the limiter's line — so either the reported latency grows, or the limiter's line shrinks.

**(b) Which true peak?** "dBTP" is defined by BS.1770 as the maximum of the continuous waveform,
and every meter is a finite approximation of it. The product's own 4× estimator (`TruePeak.h`,
12 taps, Blackman) was sized for the limiter's detector and the meter; on the engine's TP-mode output
it reads up to ~1.4 dB **below** the Recommendation's own order-48 example filter, and on dense
broadband material up to ~0.24 dB **above** an accurate interpolator. A clamp driven by any one meter
holds the ceiling on that meter only — which is the audit's merged sub-item (c): "the new clamp TP
tap either uses a more accurate estimator or carries a stated margin".

## Options

**Where the lookahead comes from.**

- **A. Report more latency** — `maxLookahead + D + osLatency`, always or only in TP mode.
  **Lost:** a reported-latency change on every session (always), or PDC that moves whenever a
  preset, A/B slot or undo step carries a different `truePeakMode` (TP mode only) — the exact PDC
  spray ADR-0004 exists to prevent.
- **B. Keep a constant clamp delay in both modes** (the region line is 10 ms − D always). **Lost:**
  the true-peak-OFF path stops being bit-identical — the limiter's longest window shrinks for every
  user and the Post EQ's glide timing moves against the audio — to fix a defect that exists only in
  TP mode.
- **C. Carve D out of the allowance in TP mode only, and latch the composition like an oversampling
  change.** The reported figure never moves; TP-off is bit-identical; the longest engaged lookahead
  in TP mode is 10 ms − D. **Chosen.**
- **D. No lookahead — a reactive gain plus the hard clip.** **Lost:** the gain arrives after the
  peak it is reacting to; physically cannot meet the tolerance.
- **E. Oversample the clamp itself.** **Lost:** ADR-0006 option D — the clamp must be downstream of
  the base-rate Post EQ, and a clamp that resamples the audio it passes is what item 2 forbids.

**What the gain acts on.**

- **F. The shared 4× estimator alone** (ADR-0006 item 2 read literally). Built and measured first:
  the product meter reads the output at the ceiling (worst +0.008 dB) while the Annex 2 example
  meter reads it up to +1.44 dB over and a 32×/128-tap reference up to +2.67 dB. **Lost:** it holds
  the ceiling on the one meter that shares its blind spot.
- **G. An accurate interpolator alone** (16 phases × 32 taps). **Lost:** the product meter then
  reads the output up to +0.63 dB over — the 12-tap Blackman estimator over-reads dense broadband
  material — so the product's own display would show overs the clamp let through.
- **H. The largest of three readings on one window — an accurate interpolator, the product meter's
  phases, the Annex 2 example filter.** **Chosen**; each reading closes a failure the other two
  leave, all three measured.
- **I. The shared estimator plus a fixed margin.** **Lost:** the under-read it would have to cover
  is programme-dependent (0 to ~1.4 dB measured), so a margin is either a loudness tax on every
  master or not a bound.

## Decision

1. **With `truePeakMode` applied, the clamp's gain acts on its own true-peak reading of its own
   input** (ADR-0006 items 2–3, now implemented) with the sample-level hard clip kept as the
   backstop; with it off the clamp is the P1 sample compare, unchanged and bit-identical.

2. **The reading (`ClampTruePeakDetector`, `ClampTruePeakDetector.h`)** describes, per step, x[j] and the
   continuous waveform between x[j] and x[j+1], j = n − 16, from one 32-sample window, as the
   largest of: (i) a 32-tap Kaiser (β = 8) windowed-sinc interpolator designed at `prepare()`,
   evaluated at the quarter points and refined by a parabola through the largest point and its
   neighbours; (ii) `TruePeakEstimator`'s own 4× phases, so the dBTP display cannot show an over the
   clamp let through; (iii) the order-48, 4-phase example filter of **ITU-R BS.1770 Annex 2**
   (BS.1770-5, identical in -4), transcribed and pinned by test. Reporting lag 16. The detector and
   the meter's phase design live in `ClampTruePeakDetector.h`, which includes no JUCE module, so the
   whole ceiling stage stays inside the compile-time `-Wfunction-effects` gate (ADR-0029).

3. **The gain** (`CeilingClamp::processFrameTruePeak`): one LINKED gain for all channels; a
   requirement per segment against the lower of its two samples' ceilings (each frame carries the
   ceiling the limiter used for it); the minimum over every segment whose interpolation reads a
   sample; a forward minimum over an **attack of 0.25 ms** (at least 8 samples) and its moving mean
   — a linear ramp that is never above the requirement; a one-pole **release of 10 ms** on the
   reduction, snapping to exactly unity at −120 dB. Below the ceiling the path is an exact delay.
   The two voicing constants are ⊕ listening material, like the limiter's; neither affects the
   ceiling.

4. **The latency composition in true-peak mode (amends ADR-0004 items 1, 2 and 7 for TP mode):**

   ```
   reportedLatency = maxLookaheadSamples(10 ms, sr) + osLatency(factor, phase)   ← unchanged, both modes

   true-peak mode:  region lookahead line = maxLookaheadSamples − D   (base samples; × osN in the region)
                    clamp true-peak path  = D
                    D = attack + 30   (41 samples at 44.1 kHz, 42 at 48 kHz, 54 at 96 kHz, 78 at 192 kHz)
   ```

   The clamp's delay D comes out of the allowance; the region's lookahead line shrinks by D; the
   limiter's engaged window is capped at `maxLookahead − D`. Every lookahead setting up to
   10 ms − D engages exactly as before; a setting above it engages 10 ms − D (9.125 ms at 48 kHz).
   The dry/bypass alignment, `groupDelaySamples()` and `predictLatencySamples()` do not see the mode
   at all. If a host-supplied rate leaves no room for D above the 0.5 ms minimum window, the path is
   not used (a rail; no conforming rate reaches it).

5. **A true-peak toggle is a latched rewire.** It moves the region line's length, so it is latched
   at the §2.8 duck's silent bottom (the lookahead ring, the oversampler and the clamp's own ring
   restart), held for the refill, then faded in. Adopted directly on the first block after
   prepare/reset and on entering offline, as the factor is. The limiter's own detector mode still
   follows the snapshot per block, as before. The two directions reach the bottom differently
   *(revised in the PR #42 review)*:
   - **Disengaging (on → off)** rides the duck exactly like an oversampling change — out, latch,
     hold, in. Its out-leg is emitted by the true-peak path, which holds the new (sample-peak)
     ceiling as well as the old one.
   - **Engaging (off → on) while audio plays** latches at the toggle block itself: the silent bottom
     is entered at once, and the duck's out-leg is replaced by a **decay of the last emitted frame**
     to zero over ~6 ms (`EngagementTail`, `CeilingClamp.h`). Before its first sample the decay is
     checked with the clamp's own detector — every reading of a segment at or after the toggle, the
     ones whose windows still reach back into the emitted audio included — and scaled down by
     bisection only if it would exceed the ceiling. So **no reading of the output from the toggle on
     exceeds the requested ceiling**, and low-frequency material crosses the toggle without a step.
     A ceiling cut in the same block (a preset or A/B swap that also turns TP on) is checked against
     the new value: cuts of 3–10 dB held it from the toggle on (measured, hostile programme). Only
     silence failing the check — a cut deeper than ~4.6 dB under adversarial near-Nyquist history —
     would leave the old audio's own ringing, under the smoothed ceiling the stage enforces during
     any ceiling move.
     *Why not a fade of the audio:* the audio the out-leg would fade is already in the pipeline and
     the replaced composition has no lookahead at the clamp; holding a true-peak ceiling on it from
     the first post-toggle sample needs either a gain step or more latency (decision 4 rules that
     out). *Why not an instant mute:* same guarantee, but a step of up to full scale at every
     engagement — measured ~56 dB more transition splatter than the decay on a 100 Hz tone.

6. **Realtime.** Everything is allocated at `prepare()`; the per-frame path is allocation-free,
   lock-free and bounded (fixed windows; the requirement windows are scanned only while they hold a
   value below unity). The only recursive value (the reduction) is a max of values in [0, 1] ordered
   so a NaN operand loses; no finite input produces a non-finite output.

7. **Verification is on independent meters, at the policy's own 0.1 dB.** The durable guard
   (`testTruePeakModeHoldsTheCeiling`) reads every run with the product meter AND an independently
   implemented Annex 2 meter; the matrix in the worklog adds libebur128 and a 32×/128-tap reference.
   *(Settled at acceptance, 2026-09-27, by ADR-0043: the two meters the durable guard reads are the
   ones "dBTP" is DEFINED on — `DSP_POLICY.md` invariant 4 — and libebur128 and the long-kernel
   reference are reference and compatibility measurements, recorded and not asserted.)*

8. **Policy amendment (prescribed text, `ADR_POLICY.md` rule 5).**
   - `DSP_POLICY.md` invariant 2 gains, after "the engine pads the difference": *"In true-peak mode
     the ceiling clamp's true-peak path takes a short delay of its own (ADR-0041) out of that same
     allowance — the lookahead line shrinks by it and the longest engaged window becomes 10 ms minus
     it — so the reported figure is unchanged in both modes."*
   - `DSP_POLICY.md` invariant 8's enumeration gains **"the true-peak mode"** beside the
     oversampling phase mode, with its reason: it moves the clamp's share of the allowance, so it is
     latched at the §2.8 duck's silent bottom like an oversampling change — disengaging through the
     out-leg, engaging (revised in the PR #42 review) through a checked decay of the last emitted frame, so
     the ceiling holds from the toggle.
   - *(Completed at acceptance, 2026-09-27, so the approval covers the policy text as it stands.)*
     Invariant 8's parenthetical also carries the one place it yields to invariant 4: *"The decay is
     value-continuous; where the audio just before the toggle would ring above the ceiling it
     starts lower, the one place this invariant yields to invariant 4 — no lower than −1.7 dB over
     a 248-configuration hostile sweep, −3.2 dB for a synthetic full-scale Nyquist-rate history."*
     The sentence had been in the policy since the PR #42 review without being prescribed here;
     the behaviour it states is decision 5's and the Consequences' below. Invariant 4's own
     definition of "dBTP" is ADR-0043's prescribed text, not this record's.

## Consequences

- **The promise holds at every oversampling cell and in the Force Max bounce**, on the product meter
  and on the Annex 2 example meter — measured, not argued (worklog §Verification; the numbers are
  quoted in `TEST_REPORT.md`).
- **What is not claimed, stated:** a 32×/128-tap reference still reads residual overs on synthetic
  programme with strong content in the last few percent below Nyquist (clicks, near-Nyquist tones,
  a +12 dB Post shelf), and libebur128 reads a smaller residual. Filtering that content to 20 kHz
  before measuring makes the reference read HIGHER, not lower — the "true" peak of near-Nyquist
  content depends on the reconstruction filter. Which yardstick defines the promise was the owner's
  call (audit DSP-001 sub-item (a)), taken at acceptance: ADR-0043 defines "dBTP" on the product
  meter and the Annex 2 filter, so those two residuals are reference/compatibility measurements
  outside the definition; `KNOWN_ISSUES.md` KI-020 carries the figures and the options not taken (a
  longer accurate kernel measured to halve the residual, at twice the lookahead and CPU).
- **The limiter's longest window in true-peak mode is 10 ms − D.** Invisible below that setting.
- **A true-peak toggle dips the output**, like any latched rewire; a preset, A/B or undo step that
  changes the mode already sits inside one. Disengaging: the §2.8 duck (~6 ms out, a refill hold,
  ~28 ms in). **Engaging while audio plays** *(revised in the PR #42 review)*: the programme stops at the
  toggle and the last emitted value decays to silence over ~6 ms, then the refill hold (the
  allowance plus the oversampler's delay) and the ~28 ms fade-in in TP mode. The decay is
  value-continuous, but the audio's high-frequency detail stops at once — the detail that would carry
  an inter-sample peak — so a bright or tonal programme hears a more abrupt end than the old fade,
  though far from a mute's (measured
  on clean tones: at 100 Hz the transition splatter is ~20 dB above the old fade and ~56 dB below an
  instant mute; at 1–6 kHz ~4–17 dB above the fade). Where the emitted audio just before the
  toggle would ring above the ceiling the decay starts lower — a step at the junction; over a
  248-configuration hostile sweep (+12 dB Post shelf, hot limiter) its start was scaled to no less
  than 0.82 (−1.7 dB), unscaled in 107 runs; a synthetic full-scale Nyquist-rate history needs 0.69
  (−3.2 dB). The check costs ~7 µs per engagement unscaled, ~80 µs when it bisects.
- **Level:** true-peak mode now removes the overs it used to emit. Median RMS change over the main
  matrix −0.06 dB; the largest drops (−5 to −6.6 dB) are the +12 dB Post-shelf stress cases that
  rendered ~+4.7 dB true-peak overs before.
- **Cost:** the clamp's true-peak path measures 0.51 % of one core at 48 kHz standalone
  (`AnabasisBench`, 107 ns/sample; the detector's arithmetic is folded over its filters' mirror
  symmetry to get there). The budget case (48 kHz · 512 · 4× · working) goes from 3.00 % to 3.60 %
  with TP on — inside the ≈5 % target, but over DESIGN §9's limiter + TP-detection row at ≥ 4×,
  which this path is charged against (`PERFORMANCE_BUDGET.md`). The cost is per base sample, so it
  doubles at 96 kHz. TP mode is off by default (ADR-0015).
- **The GR meter still shows the limiter's reduction only** (unchanged semantics); the clamp's own
  reduction is not displayed.
- **TP-off is bit-identical to `main`** — 222 of 222 true-peak-off configurations hash-identical.
- **Forecloses:** reporting the clamp's delay as extra latency; a clamp detector that reads fewer
  than the three readings without a new record.

## What the owner was asked to decide *(added in the PR #42 review; decided 2026-09-27)*

Accepting this record accepted four things together, each of which the gate needed to see:

1. **The TP-mode latency composition** (decision 4). Verified against the code on 2026-09-27:
   `CeilingClamp::truePeakDelayFor` = attack + 30 with attack = max(8, round(0.25 ms · fs)) —
   **41** samples at 44.1 kHz, **42** at 48 kHz, 52 at 88.2 kHz, **54** at 96 kHz, 74 at 176.4 kHz,
   **78** at 192 kHz; the region line is the 10 ms allowance minus that (the longest engaged window
   9.07 ms at 44.1 kHz, 9.125 ms at 48 kHz, 9.44 ms at 96 kHz, 9.59 ms at 192 kHz); the **reported**
   latency is `maxLookahead(10 ms) + osLatency` in both modes, pinned by
   `testReportedLatencyMatchesImpulse` and `testOsLatencyMatrix` running in both modes, and observed
   unchanged across TP toggles in a host (Carla, 480 samples at 48 kHz). The engagement fix of
   decision 5 adds no latency: it changes only when the latch happens, not what it latches.
2. **The detector** (decisions 2–3): three readings, a stated departure from ADR-0006 item 2's
   wording, and the yardstick question it left open — which meter defines "dBTP" was a separate owner
   decision, laid out with measurements in `docs/reports/2026-09-27-phase0-owner-decisions.md` and
   taken the same day (ADR-0043: Option 1).
3. **The engagement transition** (decision 5, revised): the ceiling holds from the toggle, at the
   cost of the programme stopping at the toggle instead of fading (and, only where the audio just
   before the toggle would ring above the ceiling, a small step — −1.7 dB at worst over the hostile
   sweep, −3.2 dB for a synthetic full-scale Nyquist-rate history).
   Reproduced in a real host and closed there too (Ardour 8.4: +1.82 / +2.12 dB over after the toggle
   on the PR head, −0.05 / +0.00 dB on this revision).
   The alternatives, each measured on the same engine (the PR #42 review worklog): keep the old out-leg —
   click-free, but up to +4.7 dB (product meter) / +5.5 dB (Annex 2) over the requested ceiling for
   ~2 ms after the toggle, samples that a latency-compensated host places *before* the toggle on
   its timeline; or an instant mute — the same guarantee as the decay with a step of up to full
   scale at every engagement.
4. **The policy text** (decision 8): invariant 2's sentence and invariant 8's enumeration, with
   invariant 8's decay sentence added to the prescription at acceptance.

**The alternatives on record at the gate, not taken:** one of the options recorded above — A (report more
latency — a PDC change every session, or one that moves with the switch), B (a constant clamp delay
in both modes — TP-off stops being bit-identical and every user's longest lookahead becomes 9.125 ms
at 48 kHz), or rejecting the record, which returns TP mode to `main`'s behaviour (a sample-peak clamp
under a dBTP readout; up to +4.8 dB over on the product meter). As decided, the voicing
constants (attack 0.25 ms, release 10 ms) are still ⊕ listening material, and the TP-mode cost at ≥ 4×
is over DESIGN §9's limiter + TP-detection row (`PERFORMANCE_BUDGET.md`).

## Related code

`src/dsp/CeilingClamp.h` (the true-peak path; `EngagementTail`), `src/dsp/ClampTruePeakDetector.h`
(`ClampTruePeakDetector`, `truepeak::designMeterPhases`), `src/dsp/TruePeak.h`
(`TruePeakEstimator::designPhases`, now forwarding), `src/dsp/AnabasisEngine.{h,cpp}`
(`latchOsConfig`'s TP composition, `latchWanted`, the engagement block, the window cap, stage E's
frame-wise clamp and the decay), `tests/dsp_tests.cpp` (`testClampTruePeakDetector`,
`testCeilingClampTruePeakPath`, `testTruePeakModeHoldsTheCeiling`,
`testTruePeakEngagementHoldsTheCeiling`, `testTruePeakModeCapsTheWindowNotTheLatency`,
`testDuckWrapsTruePeakLatch`, `testTruePeakModeIsExactBelowTheCeiling`, and the TP-mode loops added
to `testReportedLatencyMatchesImpulse`, `testOsLatencyMatrix`, `testBypassNullUnderOs` and
`testTheAudioPathAllocatesNothing`), `tests/realtime_effects.cpp` (the TP path, the detector and the
decay under `-Wfunction-effects`), `tests/bench.cpp` (the clamp row and the `working+TP` mode).

## Evidence

Confidence: **Verified** for the mechanism and the measured figures (tests mutation-checked: the
latency composition, the TP path, the window cap and the toggle latch each fail their own guard when
reverted); **Unverified** for the voicing constants (not listened to).

- Before/after engine matrix, 2718 configurations, four meters, and the negative control (the
  regression guard fails on `main`: 102 of 123 runs over on either meter, worst +6.04 dB) —
  `worklogs/2026-09-27-phase0-product-correctness.md`.
- The engagement leak and its fix (PR #42 review): a 248-configuration transition sweep — before, 186
  runs over the requested ceiling after a mid-stream TP-on toggle (worst +4.66 dB product meter,
  +5.46 dB Annex 2); after, none (worst +0.003 dB); `testTruePeakEngagementHoldsTheCeiling` fails on
  the pre-fix engine in all 13 of its configurations; the decay's continuity and its junction check
  each fail their own assertion when removed — `worklogs/2026-09-27-pr42-review-closure.md`.
- The engagement in a real host (PR #42 review): Ardour 8.4, offline export with TP turned on
  mid-export through Ardour's plug-in parameter API — the PR head read +1.82 dB (product meter) /
  +2.12 dB (Annex 2) over the ceiling after the toggle, this revision −0.05 / +0.00 dB, and the TP-off
  exports of the two builds are sample-identical; Carla 2.5.8 reports 480 samples at 48 kHz before and
  after TP toggles. The steady-state matrix re-rendered on the revision is hash-identical to the PR
  head (2718 + 240 + 56 renders) — same worklog.
- The owner's options, with the delivery-meter question measured out (including a prototype that
  also holds libebur128's reading) — `docs/reports/2026-09-27-phase0-owner-decisions.md` (decision
  material; decided 2026-09-27 — this record accepted, the definition as ADR-0043).
- Acceptance re-verification, 2026-09-27: the steady-state matrix, the 248-configuration transition
  sweep, TP-off identity, the latency composition and the Ardour engagement export re-run on the
  accepted tree — `docs/reports/2026-09-27-phase0-closure.md`.
- Split-review re-verification, 2026-09-29, on PR #43's head `2a5f8a8` (the closure's own evidence
  pointer leads into the stacked PR #42): the transition sweep reconstructed with a negative control,
  TP-off identity with `main`, a steady-state TP-on matrix and the latency composition —
  `worklogs/2026-09-29-pr43-phase0-verification.md`.
- Estimator comparison (why three readings): same worklog, §Investigation.
- ITU-R BS.1770-5 (11/2023), Annex 2 — the example filter table; cross-checked value by value
  against the Recommendation's text.
- Depends on: ADR-0002 (clamp placement), ADR-0003 (TP as a measurement tap), ADR-0004 (amended
  here for TP mode), ADR-0006 (implemented here), ADR-0015 (TP off by default).
