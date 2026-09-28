# ADR-0045 — True-peak mode answers to the ceiling in force when a sample is emitted

**Status:** **Accepted — 2026-09-28, on the owner's direction** (the instruction of record for the
third review round on PR #42: "If the user enables true-peak protection, the output must obey the
declared ceiling … At every output sample the emitted signal must satisfy the current active
true-peak ceiling contract"; "Do not increase latency unless an explicit ADR decision is created"),
with the defect reproduced and the design chosen on measured evidence before the code moved. ⊕
Flagged for the owner's review of the pull request that carries it: the direction settled *that*
every emitted sample answers to the live ceiling; the mechanism below — prediction of the glide,
revision of the frames in flight, and the limiter moved onto the same value — is what that review
confirms. It is an Architecture Review Gate item on two counts, named here rather than left to be
found: a **conflict with Accepted [ADR-0041](ADR-0041-clamp-true-peak-path-inside-the-latency-allowance.md)
decision 3**, whose "each frame carries the ceiling the limiter used for it" it amends (dated banner
there), and a **change to the ceiling stage and the limiter's threshold timing in true-peak mode**
(`DSP_POLICY.md` Enforcement: "any change to … the ceiling clamp"). No latency, parameter, saved
state, signal order, threading or TP-off sample changes. Review finding "Descending ceiling leaks
delayed peaks" (PR #42, 2026-09-28).

> **Amended 2026-09-28 by [ADR-0046](ADR-0046-the-true-peak-clamp-eases-in-and-engages-from-12-khz.md)
> (decisions 1, 2 and 5; the fourth PR #42 review round, on the owner's direction, ⊕ for review).
> The promise of this record is unchanged — every true-peak frame answers to the live ceiling, and
> a retarget never raises a stored ceiling or a gain; three of its mechanisms and one figure are
> not.**
>
> - **Decision 1 — the stamp is the LOWER of the entry and the predicted emission ceiling.** During
>   an upward glide the frames in flight carried the rising trajectory, and a reversal mid-ascent
>   (−20 → 0 → −20 dB) lowered those stamps by both slopes at once: the segment straddling the
>   emission point read **+0.30 dB (48 kHz), +0.49 dB (22.05 kHz), +0.60 dB (8 kHz)** over the live
>   ceiling at clamp level (Annex 2, stereo linked; not reproduced at engine level, where the limiter
>   absorbs it). `ceilEmitArr[n] = min(predicted emission, entry)`: identical on a static ceiling and
>   on a descent; on an ascent a frame reaches the output up to D samples later — under the ceiling.
> - **Decision 2 — a revision reaches only what a revised reading defines.** A revised r_j now
>   constrains the frames x[j−5 .. j+6] its two defining readings read, judges the taps before its
>   main lobe against the glide one frame ahead (`kRevisionLead`), and leaves the entry-time
>   requirements' full 32-sample reach alone. "Re-derives every requirement … the 32-wide, forward
>   and mean windows" below describes the 0.2.15 rebuild.
> - **Decision 5 and the Consequences' step — the figure.** The one-sample step at a downward
>   retarget with the output at the ceiling is ~4.9 glide steps at every rate below 66 kHz (0.0040
>   on 0.9 DC for −1 → −20 dB at 48 kHz), not (16 + A/2 + 1) (0.0185); `DSP_POLICY.md` invariant 8
>   carries the new figure, and `testTruePeakModeBoundsTheStepAtACeilingCut` pins 7 glide steps.
>   The tolerance-spending alternative the Consequences leave to the owner is moot at this size.
> - **The Consequences' "Where the promise still depends on the rate" (KI-025) is closed** by
>   ADR-0046's gain law, at every rate the path engages (12 kHz and up). Its figures, and the other
>   figures below, are kept as measured on 0.2.15.

## Context

The Ceiling reaches the DSP through one smoother, a 20 ms **linear** glide (`kCeilingGlideSeconds`)
retargeted once per `process()` call and advanced once per base sample; `DSP_POLICY.md` invariant 8
requires the glide, invariant 4 promises that the output never exceeds the ceiling "at any automation
rate … during and after every transition", with 0.1 dB tolerance in true-peak mode on each of the two
meters ADR-0043 names.

**TP-off** holds it to the letter: the clamp is the last stage and adds no delay, so its clip reads
the smoother's value at the very sample it emits, and the limiter's gain computer plays each sample
against that same value. **True-peak mode** (ADR-0041) adds a delay D at the clamp (41/42/54/78
samples at 44.1/48/96/192 kHz, taken out of the 10 ms allowance so the reported latency never moves)
— and decision 3 had each frame carry the ceiling in force when it ENTERED: the requirement of every
segment and the backstop clip of every sample were judged against a value D samples old. While the
ceiling holds still that is the same number. While it descends, every emitted sample answered to a
ceiling above the one in force:

    over (dB) ≤ 20·log10 (1 + (D / R) · (c0 / c1 − 1)),   R = the glide in samples

largest at the END of a glide (the linear-gain slope is steepest relative to the ceiling there) and
lasting D samples past it. Measured on the real engine at `dd983ec` against the live smoothed ceiling
rebuilt sample by sample (1936 true-peak configurations × 11 moves; the 2026-09-28 worklog): an
instant 0 → −12 dB cut **+1.13 dB** (Annex 2, 44.1 kHz), −1 → −20 dB **+2.72 dB**, 0 → −20 dB
**+3.04 dB**; a 50 ms DAW ramp +0.32 dB; a ±6 dB zig-zag +0.39 dB; 200 ms ramps +0.06 dB, 1 s ramps
+0.01 dB. The sample peaks went over too — up to +2.55 dB — which the TP-off clip never allows. Against
the carried value the clamp was exact (≤ +0.002 dB), so this is the timing of the threshold, not the
clamp's gain law.

Which ceiling is "the" ceiling was checked, not assumed. Against the processing-time smoothed value —
the one the TP-off clip holds bit-exactly — the finding stands. Against a host-timeline-aligned value
(the smoothed ceiling shifted by the reported latency) true-peak descents are clean, but BOTH modes
overshoot ascents by up to +8 dB, because the 20 ms glide is longer than the 10 ms lead; no path can
meet that reading, so it cannot be the product's.

## Problem

How the true-peak path, whose audio leaves D samples after it enters, can hold every emitted sample
and every reading to the ceiling in force when it is emitted — without a latency change, without
touching TP-off or the static-ceiling output, allocation-free and bounded.

## Options

Each prototyped on a scratch copy of the engine and measured on one common harness against the live
smoothed ceiling (the 2026-09-28 worklog carries the method and every figure):

- **A. Re-label "live" as the carried ceiling.** Changes no output sample; it declares that in TP
  mode ceiling automation takes effect D samples later than in TP-off, while the output stays up to
  +3 dB above the value the smoother and the TP-off clip hold at that instant. A relabel, not a fix.
  Rejected.
- **B. A smooth output gain of live / carried.** Dips material that is BELOW the ceiling (up to
  −3 dB measured) and still leaks at glide ends. Rejected.
- **C. A worst-case slope margin.** The smoother's descent is bounded, so a margin could cover it —
  but only by lowering every static-ceiling output. Violates the constraint. Rejected.
- **D. More latency** (a longer true-peak delay, reported). ADR-0004/ADR-0041's constant-latency
  contract; not needed. Rejected.
- **E. A release-free live guard beside the carried path.** Keeps decision 3's carried judgement and
  adds a guard that judges closed segments against the live values that occurred and open ones
  against the live value now and one peeked step; backstop at the live ceiling. Holds the tolerance
  on programme (≤ +0.003 / +0.009 dB over the live ceiling across 2960 renders, +0.032 dB at 22.05 kHz), with an undershoot of up to −0.37 dB for ~1 ms after a glide lands and per-sample gain changes up to +0.26 dB larger than before — but **not under an adversarial search**: it judges the open segments against a one-sample peek of the live ceiling rather than their own emission-time value, and a transient burst aligned with the bottom of a 0 → −20 dB cut reads +0.14 dB (Annex 2) over the live ceiling at 22.05 kHz, +0.11 dB at 32 kHz and +0.42 dB at 8 kHz, where F holds the same inputs. Its repair is F's prediction. Rejected in favour of F: it keeps two thresholds in play for
  the same sample, bounds the open segments approximately rather than exactly, and leaves the limiter on the entry-time ceiling, so the clamp — invisible on every GR display — does the ceiling-following (deepest clamp gain on a sine at the ceiling −6.2 dB against F's −2.1 dB and the unfixed engine's −2.1 dB).
- **F. Judge every frame against the ceiling in force at its emission.** Between block-rate retargets
  the smoother is a deterministic ramp, so the value it WILL have D samples on is known exactly; a
  retarget can only move the future of the frames already in flight, and those are revised. The
  limiter plays each sample to the same value, as it does in TP-off. **Chosen.** (Without the limiter
  half, "F without L", the ceiling result is identical and the clamp alone absorbs every descent:
  a 997 Hz sine held at the ceiling then dips 1.8–2.8 dB under the landed ceiling after an instant −1 → −20 dB cut, as the clamp's 10 ms release holds the descent's reduction, against 0.97–1.26 dB with the limiter half and 1.0–1.2 dB before this record.)

## Decision

1. **The ceiling a true-peak frame answers to is the one in force at its emission.** At every block
   top in true-peak mode the engine copies the ceiling smoother and runs the copy `clampDelay` steps
   ahead (`ceilingAhead`); per base sample the copy's value (`ceilEmitArr`) is the smoother's value at
   the step that sample leaves the clamp. It is exact — the same float operations in the same order
   (JUCE's `skip()` multiplies instead and overshoots the iterated value in about half of glide
   states, so it is not used). The clamp stores it with the frame, judges every segment against the
   lower of its two samples' stored values, and clips the emitted sample against its own — decision 3
   of ADR-0041 unchanged in every other word.
2. **A retarget revises the frames in flight, never upward.** A new target at a block top can move the
   trajectory below what in-flight frames were stamped with. The new trajectory's value at each
   in-flight frame's emission is computed the same way, and `CeilingClamp::lowerInFlightCeilings`
   lowers each stored ceiling to it (never raises one) and re-derives every requirement that still
   constrains a gain not yet applied — r for the segments that read a lowered frame from the stored
   per-segment readings, then the 32-wide, forward and mean windows — so every gain still to be
   emitted again satisfies the clamp's own bound for the revised ceilings. A smoother that is not
   gliding has had no effective retarget since the in-flight frames were stamped — every effective
   retarget leaves it gliding (its ramp is `kCeilingGlideSeconds`, so `stepsToTarget > 0` at every
   supported rate), and a glide that ends on its own is already in every prediction — so nothing is
   revised then and the static path does no extra work.
3. **The limiter plays to the same value.** In true-peak mode the region's gain computer reads
   `ceilEmitArr` for each base sample instead of `ceilArr` — the value the clamp stamps the same
   sample with, held across the oversampled region exactly as `ceilArr` was — so the limiter, not the
   clamp's 0.25 ms attack, does the ceiling-following, as in TP-off.
4. **Nothing else moves.** The reported latency, `clampDelay`, the region line, the TP-off path (every
   new line is behind `appliedTpClamp`), the output with a static ceiling (the emission-time value IS
   the entry-time value, bit for bit), the parameters, the saved state, the engagement decay
   (EngagementTail, checked once at the toggle against the lower of the smoother's current and target
   values — see Consequences), the dither and bypass legs.
5. **Policy amendment (prescribed text, `ADR_POLICY.md` rule 5).** `DSP_POLICY.md` invariant 4's
   guard list gains `testTruePeakModeHoldsTheCeilingUnderAutomation` for "any automation rate", with
   the reference stated: *every reading and every emitted sample checked against the LIVE smoothed
   ceiling at the output sample, the value the TP-off clip holds*. Invariant 8's note on where it
   yields to invariant 4 gains a second place: *in true-peak mode a DOWNWARD Ceiling retarget, with
   the output at the ceiling, may drop the clamp's gain within one sample by up to about (16 + A/2 + 1)
   glide steps — 0.0185 on 0.9 DC for a −1 → −20 dB cut at 48 kHz, where TP-off glides 0.0008 per
   sample — because frames already in flight get the new ceiling with less notice than the attack
   ramp needs (ADR-0045; `testTruePeakModeBoundsTheStepAtACeilingCut` bounds it).*

## Consequences

- **The output follows the ceiling as it moves, in both modes alike.** Measured after the change:
  over 1936 main-matrix renders (48 kHz OS off–16×, 44.1/96/192 kHz OS off and 4×, blocks 64/512, Post shelf 0/+12 dB, four programmes, eleven moves), 880 stress renders (blocks of 1, 16 and 64 samples, ±20 dB zig-zags, 1 ms zig-zags) and 135 renders whose host blocks differ from the prepared size, the worst reading is +0.003 dB (product meter) / +0.004 dB (Annex 2) over the live smoothed ceiling — the static-ceiling floor — with no segment over 0.1 dB and no emitted sample above the live ceiling; before, +2.72 / +3.04 dB and 23.9 million samples emitted above it.
- **Where the promise still depends on the rate (KNOWN_ISSUES KI-025, found after this record was
  taken, by a search for worst-case bursts hill-climbed on the real engine).** At 44.1 and 48 kHz the
  search found nothing above the static-ceiling floor (+0.058 / +0.026 dB, identical in 0.2.14). At
  32 and 22.05 kHz, a burst placed at the bottom of a fast full-range cut reads **+0.125 dB and
  +0.157 dB** (Annex 2) over the live ceiling — above the tolerance; 0.2.14 read +3.1 / +3.6 dB on the
  same bursts, and option F without the limiter half reads +0.31 dB under its own search at 22.05 kHz.
  The cause is the clamp's gain law, not this record's timing: the requirement bounds a segment's peak
  while the gains it reads are equal, and an attack ramp beside it — steep at these rates, where the
  attack sits at its 8-sample floor and the ceiling falls ~1.5 % per sample — breaks that. Below
  22.05 kHz a static ceiling already reaches +0.23 dB (4 kHz), unchanged by this record. Closing it is
  a clamp-law or attack-length decision of its own, taken before Phase 1 resumes.
- **A one-sample gain step at a downward retarget — invariant 8 yielding to invariant 4, bounded.**
  The frames already in flight have at most `clampDelay` samples of notice, and a segment reading the
  next frame to leave constrains samples up to 15 steps ahead, so when the output sits AT the ceiling
  the first frame emitted after the retarget takes its revised requirement at once: about
  (16 + A/2 + 1) glide steps instead of one. Measured on 0.9 DC held at the ceiling, −1 → −20 dB in one
  block: **0.0197 at 44.1 kHz, 0.0185 at 48 kHz, 0.0117 at 96 kHz, 0.0083 at 192 kHz** (0.0367 at
  22.05 kHz), against the TP-off glide of 0.0008–0.0019 per sample and the 0.01-per-sample bound
  `testCeilingIsSmoothed` holds TP-off to; on a sine at the ceiling it is 0.12–0.16 dB of clamp gain,
  inside the clamp's own per-sample steps on programme (up to 2.2 dB on transients, before and after
  this record). It never occurs with the output below the ceiling, on a rising or static ceiling, or
  on a glide the host does not retarget. Not heard (no listening was performed).
  `testTruePeakModeBoundsTheStepAtACeilingCut` pins it at (16 + A/2 + 2) glide steps. **The owner may
  prefer the alternative the adversarial review named:** spend part of invariant 4's 0.1 dB tolerance
  on the frames in flight (lower them to the new trajectory raised by a tolerance that fades to 0 across
  `clampDelay`), which would roughly halve the step at the cost of readings up to 0.1 dB over the live
  ceiling after a retarget — not prototyped.
- **The limiter's own threshold re-predicts at a retarget.** The emission-time value is re-derived at
  every block top, so the ceiling the limiter plays to moves by up to (D + 1) glide steps at a retarget.
  At the default Transients setting this is smoothed away (the limiter's gain step equals the unfixed
  engine's, 0.067 dB at 44.1 kHz); at Transients 0 (instant attack) it is 0.35 dB at 48 kHz where the
  unfixed engine stepped 0.07 dB — smaller than the clamp's own revision step, recorded rather than
  rate-limited.
- **An upward retarget holds the frames in flight to the old, lower trajectory** for at most
  `clampDelay` samples (they are never raised) — under the ceiling, not over; before this record the
  whole ascent answered to a lagging, lower value.
- **Under automation in true-peak mode the limiter's threshold moves `clampDelay` samples earlier than
  before** — onto the same timing TP-off always had. With a static ceiling nothing changes.
- **Cost.** Per block while gliding: `clampDelay` smoother steps and compares; a revision with a
  rebuild once per block at most (~0.5 µs at 48 kHz, ~1.3 µs at 192 kHz). Per sample: one smoother
  step and a few operations. Whole-engine CPU is within measurement noise at host blocks of 16 samples and up, +1–2 % at 1-sample blocks with a static ceiling, and +19–25 % (48 kHz) / +29–40 % (192 kHz) at 1-sample host blocks with the target changing every block — the pathological case (two independent measurements on a loaded machine). Memory: a few rings of 2·attack + 32 floats.
- **What remains, recorded:** the engagement decay is checked once, at the toggle, against the lower of
  the smoother's current and target values; a further downward retarget during its ~6 ms can read up
  to +0.071 dB over the live ceiling (Annex 2, 1-sample host blocks, a −20 dB cut one block after
  engaging; +0.035 dB at 16-sample blocks; ≤ 0 at 64 and above) — inside the tolerance, unchanged by
  this record, measured and not asserted.
- **Forecloses** judging a true-peak frame against the ceiling in force when it entered, and a
  limiter/clamp threshold pair that differ in timing, without superseding this record.

## Related code

- `src/dsp/AnabasisEngine.cpp` (the block-top prediction and revision; `ceilEmitArr` in stage A; the
  region's `ceilingNow`; the clamp call in stage E), `src/dsp/AnabasisEngine.h` (`ceilingAhead`,
  `ceilEmitArr`, `ceilInFlight`, `kCeilingGlideSeconds`)
- `src/dsp/CeilingClamp.h` (`lowerInFlightCeilings`, `windowMin`, the per-segment history; the header's
  "THE CEILING A FRAME IS JUDGED AGAINST")
- `tests/dsp_tests.cpp`: `testTruePeakModeHoldsTheCeilingUnderAutomation`, `testTruePeakModeBoundsTheStepAtACeilingCut`
- `tests/realtime_effects.cpp` (the compile-time tier drives `lowerInFlightCeilings`)

## Evidence

Confidence: **Verified** at engine level on synthetic programme and constructed bursts, at 44.1 kHz
and above; **below 44.1 kHz a residual is recorded** (KI-025); **not heard**; not run in a host under
automation.

- The regression test fails on the unfixed engine (64 of 100 renders over, worst +2.44 / +2.57 dB,
  product / Annex 2) and passes on this one; stamping frames with the entry-time value fails it
  (+2.32 dB), dropping the revision fails it (+0.24 dB), lowering the stored ceilings without
  re-deriving the requirements fails it (+0.15 dB), leaving the backstop at the entry-time value fails
  it (+0.24 dB), and leaving the limiter on the entry-time value fails its post-landing check (a sine at
  the ceiling dips 2.83 dB under the landed ceiling where 1.6 dB is allowed). The not-gliding early exit
  is an optimisation and is equivalent under it.
- An adversarial review of the chosen design (3532 runs: 22.05–384 kHz, OS off to 16×, 1-sample,
  irregular and oversize host blocks, prepare/reset, TP, oversampling and offline toggles in mid-glide,
  mono): worst **+0.012 dB (product) / +0.023 dB (Annex 2)** over the live ceiling, no emitted sample
  above it — a sweep, not a search: the hill-climbed bursts above (KI-025) go further below 44.1 kHz; a standalone clamp test, 19.5 million frames at six rates, found no revision that raised a
  requirement or a gain and no gain above its requirement at emission; TP-off and static-ceiling output
  bit-identical; 0 allocations over ~162 million armed `process()` calls.
- TP off: 1936 of 1936 renders bit-identical to the unfixed engine (1584 of them under ceiling automation, all eleven moves) and 135 of 135 renders with irregular host blocks; TP on with a static ceiling: 352 of 352 bit-identical, the limiter half included; the reported latency and the measured impulse position identical in 144 cases (4 rates × OS 0–16× × phase × TP on/off × with and without a ceiling move); no allocation inside `process()` over 104 automated TP configurations down to 1-sample blocks; the repository's DSP suite unchanged otherwise.
- The review finding and its reproduction, the four candidate designs and their measurements, and an
  adversarial review of the chosen design: `worklogs/2026-09-28-pr42-review-tp-contract.md`.
- Depends on: ADR-0041 (decisions 1–5, amended here at 3), ADR-0043 (the two meters), ADR-0004 (the
  constant latency this record does not move).
