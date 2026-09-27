# Anabasis — owner decisions left open by Phase 0 (after the PR #42 review)

**Date:** 2026-09-27 · **Branch:** the PR #42 branch, at the commit that
adds this file · **Version:** 0.2.13 · **Class:** dated record in `docs/reports/` — decision
**material**, not a decision. The owner's rulings supersede it; it is not edited to record them
(`docs/SOURCE_OF_TRUTH.md`).

It supersedes the first round's follow-up record
([`2026-09-27-phase0-follow-up.md`](2026-09-27-phase0-follow-up.md), produced at `3e9b343`) where
the two differ — chiefly DSP-001's verification, which had not covered switching true-peak mode on
while audio plays (§2).

Nothing in this file is decided. Each section states what the code does now, what it did before,
the options, and what each option costs, with figures measured on the real engine or in a real host.
Where the round was asked for a technical recommendation (ADR-0042) one is given and labelled as
such; **§1 carries none on purpose** — which meter defines "dBTP" is a product decision. The evidence
is in `worklogs/2026-09-27-pr42-review-closure.md`
and, for the first round, [`worklogs/2026-09-27-phase0-product-correctness.md`](../../worklogs/2026-09-27-phase0-product-correctness.md).

| # | Decision | Record | Blocks |
|---|---|---|---|
| 1 | Which meter defines "dBTP" in DSP_POLICY invariant 4 | KI-020; audit DSP-001 sub-item (a) | the wording of every TP promise; VIS-002 |
| 2 | Accept, amend or reject ADR-0041 (the clamp's true-peak path, revised in the review) | ADR-0041, Proposed | merging PR #42 |
| 3 | Accept, amend or reject ADR-0042 (a frozen latch survives a re-prepare) | ADR-0042, Proposed | merging PR #42 |
| 4 | STATE-002 (preset browse vs TP / Dither / LOCK), UX-003 (silent Save overwrite) | KI-021, KI-022 | their fixes |
| 5 | Phase 1 entry | the audit's roadmap | Phase 1 |

---

## 1. The delivery-meter definition — which meter the ≤ 0.1 dBTP promise is measured on

**The question.** `DSP_POLICY.md` invariant 4 promises that in true-peak mode the output does not
exceed the ceiling by more than 0.1 dBTP. A true peak is a property of the continuous waveform
between samples; every meter estimates it with a finite interpolator, and on programme with energy
in the top few percent below Nyquist the estimates disagree by up to about a decibel. A clamp holds
the ceiling on the meters whose readings it limits on, and not necessarily on any other. So the
promise is only complete once it names its meter.

**Measured on the current branch** (real engine; 2736 true-peak-mode configurations — every
oversampling cell, Force Max, 44.1–192 kHz, Post shelf 0/+6/+12 dB, block sizes, lookahead,
Loudness 0–100 %, three styles; the "hot subset" is 56 configurations at the hottest points):

| Meter | Over the ceiling by > 0.1 dB | Worst | Note |
|---|---|---|---|
| Product meter (`TruePeakEstimator`, 4×, 12 taps, Blackman) | **0 / 2736** | +0.005 dB | the dBTP display; held by the clamp |
| ITU-R BS.1770-5 Annex 2 example filter (order 48, 4 phases) | **0 / 2736** | +0.004 dB | independent implementation; held by the clamp |
| libebur128 1.2.6 (49-tap Hann interpolator; 4× < 96 kHz, 2× < 192 kHz, none above) | 130 / 2736 | +0.18 dB | not held; HF-heavy / transient synthetic programme, +6/+12 dB shelf, linear-phase cells |
| 32×/128-tap Kaiser reference (hot subset) | — | +0.98 dB | not a meter; low-passing to 20 kHz first reads **higher** (+2.15 dB) |
| Real host: Ardour 8.4 offline export of a hot programme | product −0.10 / Annex 2 −0.10 / libebur128 −0.01 dBTP vs a −0.10 ceiling | | first round's render |

`main` over the same matrix: 1674–1759 of 2496 over on every meter, worst +4.80 to +7.80 dB.

### Option 1 — the product meter and the Annex 2 example filter (what the code does now)

- **Method.** The clamp limits on the largest of three readings of one 32-sample window: an accurate
  32-tap Kaiser interpolator (quarter points plus a parabolic refinement), the product meter's own
  4× phases, and the Annex 2 example filter (ADR-0041 decisions 2–3). The promise is stated on the
  two meters it names; verification reads both (`testTruePeakModeHoldsTheCeiling`, 123 runs).
- **Standards.** BS.1770-5 Annex 2 defines the true-peak measurement by ≥ 4× oversampling and prints
  one order-48 FIR "that would satisfy the requirements"; holding that filter holds the
  Recommendation's own example. It does not make every conforming meter agree near Nyquist.
- **Discrepancies that remain.** libebur128 up to +0.18 dB (130 of 2736); a long-kernel reference up
  to +0.98 dB on the hot subset. Real-programme exposure is smaller than the synthetic worst case (the
  Ardour render: libebur128 −0.01 dBTP against a −0.10 ceiling) but not measured on a music corpus.
- **Affected content.** Strong content in the top octave: clipped or hard-limited HF-heavy programme,
  near-Nyquist tones, clicks, the +6/+12 dB Post shelf; mostly at the linear-phase oversampling cells.
- **Consequences.** A delivery checked on another meter can read up to ~0.2 dB (libebur128) or ~1 dB
  (a long-kernel meter) over on such content; the manual's workaround (a ceiling ~1 dB below the
  spec for clipped or HF-heavy programme, or oversampling) stands. VIS-002 stays as it is (below).
- **Tests.** Unchanged: the product meter and the Annex 2 meter are in the suite; libebur128 and the
  reference stay in the worklog matrix (external to the build).
- **CPU.** Unchanged: the clamp's true-peak path 0.51 % of one core at 48 kHz (107 ns/sample,
  `AnabasisBench`); the budget case 3.00 → 3.60 % with TP on.
- **ADR changes.** Accepting ADR-0041 as written accepts this definition. DSP_POLICY invariant 4
  already names the two meters as the guard and marks the yardstick "recorded, not assumed" pending
  this decision; choosing Option 1 turns that into the stated definition (one sentence).
- **Behaviour change.** None.
- **Wording.** Every TP promise names its meter, e.g. "holds the ceiling within 0.1 dB on the
  plug-in's own dBTP meter and on the BS.1770 Annex 2 reference filter". Maintainer copy (C8) for
  the manual and tooltips.

### Option 2 — a different definition

Three shapes were measured or bounded; they can be combined.

**2a — also hold libebur128's reading.** Measured this round on a prototype (not in the product):
libebur128 1.2.6's interpolator (its own coefficients, unnormalised; 4× below 96 kHz, 2× below
192 kHz) added as a fourth reading. Checked against the library itself (three signals × five rates
× two channels) it agrees to within 8·10⁻⁷ dB.

| | Current (Option 1) | Prototype 2a |
|---|---|---|
| libebur128 over by > 0.1 dB | 130 / 2736, worst +0.18 dB | **0 / 2736**, worst +0.004 dB |
| Product meter / Annex 2 | 0, worst +0.005 / +0.004 dB | 0, worst +0.005 / +0.004 dB (unchanged) |
| 32×/128-tap reference (hot subset) | worst +0.98 dB | worst +0.98 dB (unchanged) |
| Level (RMS, TP-on renders) | — | median 0.000 dB, largest −0.017 dB |
| Latency | 10 ms + OS | unchanged — the filter fits the existing window (segment j needs x[j−5..j+6] at 4×, x[j−11..j+12] at 2×) |
| TP off | bit-identical to `main` | bit-identical (222/222) |
| Detector cost (stereo frame, −O2) | ~121–127 ns | ~157–161 ns (+~30 %; ≈ +0.17 % of a core at 48 kHz) |
| 192 kHz | — | no change (the library reads sample peaks there) |

- **Method / standards.** Holds the reading of one widely used open-source BS.1770 implementation.
  It ties the product to one library's filter design; any other meter with a different interpolator
  still reads its own residual (the reference is unchanged).
- **Tests.** A fourth meter in the durable guard (libebur128 is external to the build today — it would
  have to be vendored under `DEPENDENCY_POLICY.md`, or its interpolator transcribed and pinned as the
  Annex 2 table is).
- **ADR changes.** ADR-0041 decisions 2 and 7 (four readings; verification on three meters) — an
  amendment before acceptance. **Behaviour change:** 1858 of the 2736 TP-mode renders change, by at
  most 0.017 dB RMS; 878 are bit-identical.

**2b — a longer accurate kernel.** Measured in the first round (the 16-phase prototype, same hot
subset): a 64-tap accurate kernel took the reference from +0.93 to +0.36 dB, libebur128 +0.18 → +0.16
dB, product and Annex 2 unchanged — at about twice the clamp's share of the 10 ms allowance (its lag
grows from 16 to 32 samples, so the longest limiter window in TP mode shrinks further) and twice the
accurate kernel's CPU. Not a latency change (the share still comes out of the allowance), but a
limiter-window change in TP mode. A reference-grade kernel (the 32×/128-tap one) is not a practical
clamp detector: a 64-sample lag and ~4000 multiply-adds per sample per channel. ADR-0041 decisions 2
and 4 would be amended.

**2c — a margin.** Hold the Option 1 readings at the ceiling minus m dB (m ≈ 0.2 for libebur128, ≈ 1
for the long-kernel reference, from the table above). Not measured as a clamp variant; first-order
cost: up to m dB of level on programme that reaches the ceiling, on every TP-mode render, including
the ones no meter would have flagged. The number the user sets and the level the output holds would
differ by m — a change to what the Ceiling control means (UI copy, C8; DSP_POLICY invariant 4).

### VIS-002 is the same question on the display side

The STATISTICS TP row compares its held reading with the ceiling **exactly** (ADR-0020 Amendment 2).
Under Option 1 the held product-meter reading of a TP-mode render sits 0.001–0.005 dB above the
ceiling in 74 of 2736 configurations — inside the tolerance, printed equal to the ceiling, and red.
Under 2a/2b the same happens (the gain still moves inside the meter's window); under 2c it would not.
The audit's VIS-002 remedy (the SP row's half-print slack for the TP row) is an ADR-0020 amendment
that any of the options can take.

**What the owner is asked:** name the meter (or meters) that define dBTP for the promise — Option 1,
2a, 2b, 2c or a combination — and accept the matching cost. The round implements none of Option 2.

---

## 2. ADR-0041 — the clamp's true-peak path (Proposed, revised in the review)

**Previous (`main`).** True-peak mode switched the limiter's detector only; the final clamp was a
sample clip. TP-mode output was over the dBTP ceiling in 1674 of 2496 configurations on the product
meter (worst +4.80 dB).

**Current (this branch).** The clamp has its own true-peak path (three readings, a 0.25 ms attack
ramp, 10 ms release, the sample clip kept as the backstop); its delay D = attack + 30 samples comes
out of the constant 10 ms allowance **in TP mode only**, so the reported latency is unchanged in both
modes. Verified against the code this round: D = 41 / 42 / 52 / 54 / 74 / 78 samples at 44.1 / 48 /
88.2 / 96 / 176.4 / 192 kHz; the longest limiter window in TP mode is 10 ms − D (9.125 ms at 48 kHz);
Carla reports 480 samples at 48 kHz before and after TP toggles. A TP toggle is latched at the §2.8
duck like an oversampling change; TP off is bit-identical to `main` (222/222 renders; an Ardour
export sample-identical).

**What the review changed (decision 5).** Engaging TP while audio played leaked the new ceiling: the
latch waited for the duck's out-leg, which the replaced (sample-clip) composition emits. Measured:
186 of 248 hostile transition configurations over by > 0.1 dB, worst +4.66 dB (product) / +5.46 dB
(Annex 2), first violating reading 0–106 samples after the toggle; reproduced in Ardour 8.4 (+1.82 /
+2.12 dB). Engaging now latches at the toggle block and replaces the out-leg with a checked decay of
the last emitted frame. After: 0 of 248 (worst +0.001 / +0.003 dB); Ardour −0.05 / +0.00 dB. No
latency, parameter, state or TP-off change.

| Engagement transition | Ceiling from the toggle | Click (HF splatter, clean 0.5 tones, 100 Hz / 1 / 3 / 6 kHz, dB) | Latency | CPU |
|---|---|---|---|---|
| Old out-leg (pre-review) | **no** — up to +4.7 / +5.5 dB | −83.3 / −32.2 / −25.5 / −44.7 | — | — |
| Instant mute ("silent until applied") | yes | −8.2 / −9.5 / −21.3 / −24.9 | — | — |
| **Checked decay (implemented)** | **yes** — 0/248 | −63.8 / −26.0 / −21.7 / −27.8 | — | ~7 µs per engagement (~80 µs when it scales) |
| A TP ceiling enforced on the fade itself | would need the TP path's lookahead, which the replaced composition does not have: a gain step or more latency (decision 4) | — | + D, or a step | — |

The decay's cost against the old fade: the programme stops at the toggle (value-continuous, but its
HF detail ends at once — ~20 dB more splatter than the fade at 100 Hz, ~4–17 dB more at 1–6 kHz, far below
a mute), and where the audio just before the toggle would ring above the ceiling the decay starts
lower (−1.7 dB at worst over the hostile sweep, −3.2 dB for a synthetic full-scale Nyquist-rate
history).

**What accepting it accepts** (the ADR's own section, *What the owner is asked to decide*): the TP-mode
latency composition, the three-reading detector, the engagement transition, and the policy text.
**Alternatives on record:** report more latency (A), a constant clamp delay in both modes (B — TP off
stops being bit-identical), or reject (TP mode returns to `main`'s sample clamp). The meter question
(§1) is separate and can be decided either way under any of them.

---

## 3. ADR-0042 — a frozen latch survives a host re-prepare (compact decision record)

**Current (this branch).** `AdaptiveEngine::reset()` stashes the applied trim vector; the first block
after the reset restores it when **that block's** snapshot has Freeze on, before any staged ADR-0014
restore is injected (so a restore still wins). Only the applied set is republished; the retained set
and its generation do not move.

**Previous (`main`).** Every `prepareToPlay` zeroed the applied vector and a frozen engine held the
zeros: FREEZE lit, the audio on no adaptive trims, the session save still writing the latched vector
(KI-006's audio half; ~0.3–0.4 dB RMS and ~0.5 dB of per-channel GR in the audit's measurement).

**Proposed.** Option C of the ADR, as implemented. **Alternatives:** A — carry the vector across every
reset, Freeze on or off (also removes the unfrozen restart-from-rest step; changes adaptation
behaviour on every re-prepare); B — carry only if Freeze was on in the last processed block (misses a
Freeze engaged while stopped); D — re-inject the retained vector in `reset()` (the retained set is a
slot-scoped latch, not what was playing; same stale-Freeze problem as B); E — have the wrapper
re-stage from `prepareToPlay` (a ValueTree write on a host callback — the round-40 race, a threading
change); F — turn FREEZE off when a re-prepare drops the latch (an audio-thread parameter write;
defeats Freeze).

**Behaviour, path by path** (engine/processor tests unless marked; a DAW has not been observed
re-preparing during playback):

| Path | Behaviour | Evidence |
|---|---|---|
| `prepareToPlay`, same rate and block (transport start in some hosts) | frozen vector keeps playing | `testAFrozenLatchSurvivesARePrepare` (bit-identical to the vector restored and frozen) |
| `prepareToPlay` at a new rate (48 → 96 kHz) or block size | same — the trims are rate-independent quantities | same test at 96 kHz; `testPreparedStateAndSlotOwnership` case 4 |
| Freeze engaged while stopped, then a re-prepare | the vector comes back (the first block's snapshot decides) | same test, case (c) |
| Freeze off at the re-prepare | adaptation restarts from rest, as before | same test, case (c) |
| Session load staged before `prepareToPlay` | the loaded vector wins over the carry | ordering mutation (carry after the restore) fails 4 DSP + 1 state checks, incl. "an unprimed session load restores the vector on the first block" |
| Session save after a re-prepare | writes the same latched vector; retained generation unchanged | `testPreparedStateAndSlotOwnership` case 4 |
| A/B into a freeze-ON slot with its own vector | the slot's vector is staged and lands at the duck bottom (ADR-0014), even across a re-prepare (a staged record survives `prepare`) | ADR-0014 tests; engine code path |
| A/B into a freeze-ON slot without a vector | unchanged pre-existing disagreement: the outgoing latch keeps playing and the slot saves none — the carry preserves it across a re-prepare, it does not resolve it (KI-007 item 10) | `testAFrozenLatchDoesNotFollowTheSlotSwitch` (slot isolation kept) |
| Factory / user preset load while frozen | Freeze is preset-excluded and presets carry no vector: the latch keeps playing, and across a later re-prepare | `testAPresetApplyKeepsTheFrozenLatchItDidNotChange` + the carry |
| Entering an offline bounce | if the host re-prepares for it: carried; if it only flips non-realtime: nothing is reset | engine code path; not observed in a DAW |
| Host activate/deactivate (Carla 2.5.8, ×3) | Freeze, TP and every parameter preserved | real host; its engine carries no audio, so the audio half is not observable there |

**Trade-offs.** C keeps Freeze's promise across every re-prepare with no thread crossing and no slot
leak; it leaves the unfrozen restart-from-rest step (A) and the A/B-without-vector disagreement
(KI-007 item 10) exactly as they were.

**Technical recommendation:** accept C as written. The evidence covers every lifecycle path the
engine can see and pins the two ordering constraints (restore wins; slot isolation) by mutation. Take
option A, and the KI-007 item 10 trigger, as separate decisions — neither is made harder by C.
**Still owed after acceptance:** DAW evidence of which hosts re-prepare on transport start or before a
bounce (the audit's TEST-002).

---

## 4. STATE-002 and UX-003 — re-checked, unchanged

No code on either path changed in this round (`src/PresetManager.*`, `src/PluginProcessor.cpp`'s
preset and save paths, the Save panel). Both remain blocked on the owner for the reasons recorded in
KNOWN_ISSUES KI-021 / KI-022, and both are disclosed in the manual.

- **STATE-002** (a factory preset turns TP, Dither and Noise Shaping off; LOCK holds the ceiling's
  number only). Options: widen the lockable set to `truePeakMode` (ADR-0010 rejected a wider v1 set —
  a superseding ADR); leave the output rows untouched on a factory apply (a preset-contract change,
  `PARAMETER_COMPATIBILITY_POLICY` rule 6); or a visible cue when a preset changes TP or dither (UI
  copy, C8). The dither-under-browsing rule is part of the same decision.
- **UX-003** (Save overwrites an existing preset silently). The overwrite is the family's documented
  convention and `BRAND_CONSISTENCY_CHECKLIST.md` §A lists the save flow as must-match; there is no
  platform prompt to reuse. Options: a confirm on every existing-target collision except the unedited
  prefill of the loaded user preset (the audit's shape), a family-wide proposal shared with Anamorph,
  or keep the convention. Needs an ADR or a family decision, and the words (C8).

---

## 5. Phase 1 readiness

**Not ready as a whole**, for the same reasons the first round recorded, none of which this round
could remove: UX-009 moves the MATCH gain onto the wet leg (a monitor-stage signal-order change — a
hard stop — and an ADR-0006 D8 amendment); VIS-001 conflicts with ADR-0020's session-cumulative
contract; UX-002 changes a family convention (click-to-reset); every new caption is maintainer copy
(C8); VIS-005 and UX-010 need the DAW evidence TEST-002 has not collected; TEST-004's statics are still
open. **Phase 0 itself closes only when §1–§3 are decided** (ADR-0041 and ADR-0042 at the gate).
**TEST-001** (feedback-layer test reach) changes no product behaviour and can start first.
