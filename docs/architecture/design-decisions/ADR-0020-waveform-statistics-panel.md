# ADR-0020 — The Waveform Statistics panel: eight rows in both views, `int_tpMeterOn` removed, two standard selectors added

**Status:** Accepted (2026-08-06 — owner directive of 2026-08-06, 0.1.1 round item 14: the meter
panel must show, in BOTH modes, true peak · sample peak · RMS (50 ms Hann, AES-17/"scientific"
reference selectable in Settings) · momentary (400 ms) · short-term (3 s) · integrated (BS.1770,
with a Settings switch between the -1 and -2 revisions) · loudness range; and "delete the
Settings True-Peak Meter toggle — TP is always shown")

> **ARCHITECTURE REVIEW GATE — CLEARED (2026-08-06).** A **Serialization Registry change** in
> both directions: `int_tpMeterOn` is **removed**, and `int_integratedStd` / `int_rmsRef` are
> **added**. The removal is the gated half — the addition is inert under the §4.4 missing-field
> rule. The owner's directive names the removal explicitly ("把设置里面的 True Peak Meter 开关
> 删掉，TP 一直显示"), and the round's standing instruction signs anything needing human
> confirmation ("如果有需要人工确认的，你就直接帮我确认或者签字就可以。如果需要开 ADR 的，直接
> 就开 ADR"). This is the **fourth** field-level change to `ANABASIS_INTERNAL` in the pre-ship
> window (after ADR-0015's `int_meterTargets` removal, ADR-0016's semantic change and
> ADR-0017's domain change) and, like all three, is taken while no released build's sessions
> exist to owe a migration to.

## Context

The 0.1.0 meter panel showed five readings — LUFS M/S/I, dBTP and PLR — with the dBTP row hidden
behind a Settings toggle that shipped **off** (ADR-0015). The owner's 0.1.1 directive replaces
that panel with a full waveform-statistics readout and makes the true peak unconditional.

Three of the directive's **seven** required readings did not exist. (Seven is the count the
directive names; the panel shows **eight rows**, because PLR — not in the directive — was already
there in 0.1.0 and is kept. §Decision 6 states the layout that way round: three M/S/I bar rows
plus five numeric ones.)

| Required | 0.1.0 state |
|---|---|
| Momentary / Short-term / Integrated | Computed and published |
| True peak | Computed and published; **display gated** by `int_tpMeterOn` |
| **Sample peak** | Computed (`lastRenderPeak()`) but published only into the GR ring's waveform — no meter |
| **RMS, 50 ms Hann** | **Absent.** `LoudnessMeter` is K-weighted over 100 ms rectangular sub-blocks (loudness, not level); `MasteringComp`'s RMS is a 10 ms ballistics detector inside the sidechain |
| **LRA** | **Absent** |

## Problem

Four decisions, none mechanical:

1. Where does a 50 ms Hann RMS live, and what does it cost at 192 kHz with small blocks?
2. What does "BS.1770-1 vs -2" mean as an implementable switch?
3. Where is a Settings-selected standard resolved — audio thread or view?
4. What replaces `int_tpMeterOn`?

## Options

- **A. A new header-only `RmsMeter`, fed from the engine's render tap, recomputed on a fixed
  10 ms cadence.** **Chosen.** See §Decision 1 for the cost argument.
- **B. Recompute the windowed sum once per BLOCK.** Lost: the cost is then N per block, i.e.
  N/blockSize per sample — 4.7/sample at 48 kHz/512, but **300/sample** at 192 kHz with
  32-sample blocks. A meter whose cost depends on the host's buffer size is a meter that
  becomes a CPU problem on someone else's machine.
- **C. O(1)/sample via three running sums** (Hann = 0.5 − 0.5cos ⇒ windowed energy from Σx²,
  Σx²cos, Σx²sin). Lost: elegant and genuinely O(1), but the running sums drift as float error
  accumulates over a session with no bound and no way to notice. Option A is exact by
  construction — every reading is a fresh sum over the ring.
- **D. Resolve the Settings standards on the AUDIO thread** (read the InternalState mirrors in
  `processBlock` and publish one figure). Lost: it puts a display preference on the audio
  thread's read path for no benefit, and it makes a setting change wait for audio — invisible
  when the transport is stopped, which is exactly when someone opens Settings.
- **E. Keep `int_tpMeterOn` and default it on.** Lost: it gates a delivery-critical reading, and
  the panel that replaces the old one has no row it could sensibly hide.

## Decision

1. **`src/dsp/RmsMeter.h`** — a 50 ms Hann-windowed sliding RMS: a circular buffer of per-frame
   mean squares (stereo = the mean square across channels), a precomputed window, and
   `sqrt(Σ w·x² / Σ w)` — normalised by the window's **own sum**, so a stationary signal reads
   its true RMS rather than the window's average of it. The sum is recomputed at most once per
   **10 ms of audio** (the display refreshes at 24 Hz ≈ 42 ms, so finer is invisible). N is
   0.05·sr and the cadence is 0.01·sr, so the amortised cost is **5 multiply-adds per sample at
   every sample rate and every block size**. It reads the silent sentinel until the first full
   window: a partly filled window reads low by the fraction still empty, and a wrong number on a
   transport start is worse than an absent one. (That sentinel is a value no reading can take —
   see the amendment below, which separates it from the meter's floor.) Allocation-free after
   `prepare()`; joins the invariant-9 unconditional sanitise list beside `outMeter`.

2. **BS.1770-1 vs -2 IS gated vs ungated.** That is the substantive difference between the
   revisions: -1 defined integrated loudness as the plain mean energy over the measurement
   interval, and **-2 introduced the two-stage gate** (−70 LUFS absolute + −10 LU relative) that
   every later revision kept. `LoudnessMeter` gains `integratedUngatedLufs()`, fed by a separate
   two-scalar accumulator — *not* recoverable from the histogram, whose absolute gate is applied
   at insert.

3. **LRA (EBU Tech 3342)** — a second 751-bin histogram over SHORT-TERM values sampled at the
   100 ms sub-block cadence (10 Hz, far above the standard's ≥ 1 Hz), absolute-gated at −70
   LUFS, relative-gated at **mean − 20 LU** (LRA's threshold, *not* the integrated reading's
   −10), read as the 95th minus the 10th percentile. Counts only — ~3 KB against the integrated
   histogram's ~9 KB, because nothing needs per-bin energy here. Its reset watermark spans the
   **short-term window** (`subCount + 30 + straddler`), not the integrated one's four sub-blocks:
   one retained pre-reset short-term value would set the 95th percentile for the whole session.

4. **Both integrated readings and the mathematical RMS are published unconditionally**; the
   VIEW picks. `LoudnessMeterView::tick` reads `int_integratedStd` to choose the integrated
   figure and `int_rmsRef` to add AES-17's exact **+3.01 dB** (a full-scale sine reads 0 dBFS
   rather than −3.01). The audio thread therefore never reads a display preference, and flipping
   either setting is instant with no audio involvement. Both choices land inside the view's
   existing bitwise snapshot gate, so a flip repaints without a separate flag — the mechanism
   `shownTpOn` needed and which is gone with the field it mirrored.

5. **Schema.** `int_tpMeterOn` **removed**. `int_integratedStd` (0 = BS.1770-2+, gated —
   default; 1 = BS.1770-1, ungated) and `int_rmsRef` (0 = AES-17 — default; 1 = mathematical)
   **added**. Net: nine host-hidden fields → **ten**. Defaults are the readings a delivery spec
   means and the reference mastering uses; the alternatives exist for the engineer who needs
   them. An old session carrying `int_tpMeterOn` is ignored by the §4.4 unknown-field rule; a
   session missing the two new fields takes their defaults by the same rules.

6. **Panel.** Header "STATISTICS", the M/S/I bar rows unchanged, then five numeric rows written
   by ONE `statRow` lambda: TP · SP · RMS · LRA · PLR. Identical in both views. 202 px of the
   234 the Advanced strip allows, so neither view relayouts. The sample-peak row warns against
   the ceiling **exactly** (with TP off the ceiling *is* a sample-peak limit; with it on the
   clamp is stricter — see the second amendment for the dB tolerance that makes the comparison
   as exact as the reference), which is what finally lets a user read the inter-sample excess directly
   instead of inferring it from the TP row's deliberately mode-blind warn colour (ADR-0015's
   open fine-review question — unchanged, but no longer the only signal).

7. **Settings panel.** The True-Peak Meter toggle is deleted; the two standard selectors take
   its place. Panel height recomputed 310 → **350** (title + six combo rows + two toggles), not
   nudged.

## Consequences

- Three new DSP quantities on the audio thread, all on the existing render tap: +5 mult-adds per
  sample (RMS), one extra histogram walk and two scalar adds per gating block (LRA + ungated).
  The per-block `integratedLufs()` walk noted at `PluginProcessor.cpp` now has two siblings; the
  caching remark there ("compute only when a sub-block commits") becomes worth doing if the
  fine review's profiler pass puts metering near DESIGN §9's ≤ 0.5 %. **Done — see the third
  amendment**, which is what makes "per gating block" above describe the code.
- Five new published atomics (10 total on THREAD_MODEL's meter row) — same contract, same
  clear list, no new cross-thread path, so **no threading-model change**.
- `samplePeakMaxHold` joins `dbTpMaxHold` inside `requestMeterReset`'s session-cumulative
  contract; the meter-reset click now clears both peak holds, the integrated histogram and LRA.
- **Forecloses:** hiding any statistics row behind a Settings flag. The two new fields select a
  STANDARD, never visibility — the distinction `int_tpMeterOn` failed to draw, and the reason it
  is not simply renamed.
- The 0.1.0 wording "the six meter atomics" is gone from the code rather than re-counted: the
  LIST is the count, because that number was wrong within two commits of first being written.

## Related code

- `src/dsp/RmsMeter.h` (new) — the 50 ms Hann RMS
- `src/dsp/LoudnessMeter.h` — `integratedUngatedLufs()`, `lraLu()`, `kNoLra`,
  `clearSessionCumulative()`, the LRA watermark in `resetIntegrated()`
- `src/dsp/AnabasisEngine.{h,cpp}` — `outRms` member, `outputRms()`, prepare/reset/sanitise/tap
- `src/PluginProcessor.{h,cpp}` — five atomics + accessors, `samplePeakMaxHold`, the publish
  block, `publishSilentMeters`
- `src/InternalState.h` — `int_tpMeterOn` removed, `int_integratedStd` / `int_rmsRef` added
- `src/gui/LoudnessMeterView.{h,cpp}` — the eight-row panel, `statRow`, the standard choices
- `src/gui/PluginEditor.{h,cpp}` — Settings rows, the two combos, the deleted toggle, panel height

Evidence [Verified]:
- Test: `AnabasisTests` `testRmsMeterReadsTrueLevels` (sine −3.01, DC 0.00, linearity, the
  partly-filled sentinel, the stereo mean-square convention) and
  `testLoudnessRangeAndTheUngatedReading` (steady tone ≈ 0 LU, a 10 LU step ≈ 10 LU, the −20 LU
  gate keeping a 15 LU-down passage, the ungated reading dragged down by silence the gated one
  ignores, the short-term reset watermark) — the −20 LU gate and the 30-sub-block watermark are
  each **mutation-verified**, killed by their own assertion and no other;
  `AnabasisStateTests` `testTheWaveformStatisticsRowsReadTheirStandards` (the wrapper-level
  publication, both integrated standards separated by trailing silence, the sample-peak hold
  and its reset)
- Directive: the owner's 0.1.1 instruction of 2026-08-06, item 14

## Amendment — the silent sentinel and the reading floor are separate constants (2026-08-07)

Decision item 1's contract does not change: no reading until the first full window, and the
sentinel is what stands in until then. What was wrong is that the sentinel and the meter's
computed range **overlapped**. `kSilentDb` is −144 dB, but the guard against `log(0)` was applied
to the MEAN SQUARE at `1e-15` — so the computed range ran down to 10·log10(1e-15) = **−150 dB,
beneath the sentinel**, and a mean square of 3.98e-15 published exactly −144.0. A consumer could
not then tell a real reading of a near-silent passage from "nothing measured yet", and exact
digital silence — a full window the meter *did* measure — was reported as an absence.

`kFloorDb` (−140 dB) now names the lowest LEVEL the meter reports, with a `static_assert` holding
it strictly above `kSilentDb`. Every computed reading is clamped up to it, digital silence
included; only a non-finite accumulation reverts to the sentinel, which is the condition
`sanitiseState` already published it for. `rmsDb() < kFloorDb` is therefore the exact test for
"no reading", and `LoudnessMeterView`'s AES-17 guard now uses it in place of the
`> kSilentDb + 1.0f` tolerance that had been standing in for the missing separation — a tolerance
that silently denied the +3.01 dB offset to any genuine reading inside its band.

Nothing displayed moves: the statistics rows print `-` below −99 dB, above both constants, so
silence and the sentinel render as they always did. No audio path, parameter or serialized field
is touched, and the meter's published value on every path with a signal is bit-identical.

Evidence [Verified]:
- `AnabasisTests` `testRmsMeterReadsTrueLevels` gains a digital-silence case and a −163 dBFS
  case, both asserting `kFloorDb` exactly. **Mutation-verified**: removing the clamp fails only
  the −163 case, and publishing the sentinel for silence fails only the silence case.
- The five pre-existing assertions of that test (sine −3.01, DC 0.00, linearity, the
  partly-filled sentinel, the stereo mean-square convention) are unchanged and still pass.


## Amendment — the PLR row follows the selected standard, and the SP warn gets a dB tolerance (2026-08-07)

Two display corrections, neither of which moves a Decision. Both make §Decision 4's rule — the
view resolves the standard, the audio thread never reads a display preference — reach a row it
had not been applied to.

**1. PLR was still the GATED difference under BS.1770-1.** `pubPlr` is computed on the audio
thread as `TP − I_gated`, which is 0.1.0 code written before an ungated reading existed, and the
panel read it verbatim — while the I row directly above it switched to `integratedUngatedLufs()`
whenever `int_integratedStd == 1`. With that standard selected the panel printed
`PLR = TP − I_gated` beneath `I = I_ungated`, so the row was not the difference of the two rows
it sits under; after a passage of silence, which is precisely what separates the two standards,
they diverge by several LU. The row's own "no reading" test already keyed on the SHOWN integrated
value, so the two halves of the row did not agree with each other either.

`LoudnessMeterView::plrFromShown (tpDb, integratedLufs)` now derives it from what the panel
prints. It takes both operands rather than reading the processor, so the suite can pin the rule
without driving the panel's `FrameClock` tick. Under the gated standard it reproduces
`meterPlr()` exactly, so the shipped default is unchanged. **`pubPlr` deliberately keeps
publishing**: it is the canonical gated PLR, it is one of the ten scalars `publishSilentMeters`
clears (THREAD_MODEL's meter row), and removing a published atomic is a threading-model edit this
correction does not need.

**2. The SP row compared in dB a bound that holds in linear.** `CeilingClamp` guarantees
`|x| ≤ ceilingLinear` on every clamped sample, but the row tests
`gainToDecibels(|x|max) > ceiling_dB` while `ceilingLinear` is `dbToGain(ceiling_dB)` — a round
trip the parameter itself never makes, and float does not survive it exactly. A fully limited
master could read back −0.09999997 against a −0.1 dB ceiling and trip a warn that §Decision 6
promises means "the clamp was genuinely exceeded". The comparison gains a **0.005 dB** tolerance:
half this row's own print resolution, roughly four orders of magnitude above the round-trip
error, and below anything the row can display — so every exceedance a user can read still warns.
It also absorbs the LSB-scale noise the dither stage adds DOWNSTREAM of the clamp: near the
ceiling 0.005 dB is ≈ 18 LSB at 16-bit, so flat and shaped TPDF both fit inside it, and a
quantiser's noise floor is not a limiter failure. **The TP row keeps its exact comparison**: its
over-warning is deliberate and documented (ADR-0015's open fine-review question), and it measures
a quantity the clamp does not bound.

Neither change touches the DSP, the published atomics, a parameter or a serialized field.

Evidence [Verified]:
- `AnabasisStateTests` `testTheWaveformStatisticsRowsReadTheirStandards` gains four assertions:
  under BS.1770-2 the row reproduces the published gated PLR exactly; under BS.1770-1 it is TP
  minus the ungated figure; the two differ by more than 2 LU after the test's trailing silence,
  which is what makes the first two non-vacuous; and a sentinel integrated reading still yields
  0. **Mutation-verified**: dropping the sentinel guard fails the fourth assertion and no other.
- Stated rather than implied: no headless test drives the panel's tick, so the suite pins the
  RULE and not its single call site — the same limit every other `shown*` value already has. The
  SP tolerance is likewise a `paint()` threshold with no headless driver.

## Amendment — the two histogram readings are cached, so "per gating block" is now true (2026-08-07)

§Consequences prices the LRA and ungated additions at "one extra histogram walk and two scalar
adds **per gating block**", and the caching remark two bullets down treated the per-block walk as
a profiler-pass question. Read against the code, the first was a description of the intent and
not of the call site: `integratedLufs()` walks the 751-bin histogram twice (~1500 iterations) and
`lraLu()` walks it three times (~2250 — once for the relative-gate total, once per percentile),
and the wrapper reads BOTH once per `processBlock`.

The figure that decides it is the block RATE, not the block cost. At 48 kHz/512 that is 94
blocks/s and invisible. At 192 kHz with 32-sample buffers it is ~6000 blocks/s × ~3750 iterations
≈ **22 M iterations/s** — the same order as DESIGN §9's entire ≤ 0.5 % metering allocation, and
the shape ADR-0020 §Decision 1 already rejected once for `RmsMeter`: a meter whose cost scales
with the host's buffer size is a meter that becomes someone else's CPU problem.

Both figures are pure functions of the session-cumulative accumulators, and those change in
exactly two places — `finishSubBlock` (a gating block commits, at most once per 100 ms) and
`clearSessionCumulative` (every reset path routes through it). `LoudnessMeter` now holds each
reading behind a validity flag those two functions clear, so the walks run at **10 Hz** and the
returned value is **bit-identical** to the recomputed one: the inputs cannot have changed between
a hit and the value it replaces. Measurement, update timing and the public surface are unmoved —
which is why every existing loudness test passes unedited, and that is the correctness evidence.

The flags are `mutable` and NOT atomic, deliberately: every reader of the two cached figures and
every writer of the accumulators is on the audio thread (the publish at the end of `processBlock`,
the gating-block commit inside it, the meter-reset consume at its top). That invariant is
load-bearing and the type system does not carry it — `AnabasisEngine::outputLoudness()` hands out
a public `const LoudnessMeter&`, and a `const` method that mutates advertises nothing — so it is
recorded for maintainers in `THREAD_MODEL.md` §"Audio-thread-only state behind a `const`
accessor", together with what a GUI-side reader should use instead. `momentaryLufs()` and
`shortTermLufs()` — the readings the engine's §2.7 compensation calls from elsewhere — are NOT
cached and are untouched: they walk 4 and 30 sub-block means and move every sub-block anyway.
`integratedUngatedLufs()` stays uncached because it is already O(1).

Evidence [Verified]:
- `AnabasisTests` `testTheCachedLoudnessReadingsAreNeverStale` drives the two ways a cache goes
  wrong: a committed gating block must move both readings (an upward level step, chosen because a
  downward one is legitimately excluded by the −10 LU relative gate and would pass against a
  frozen cache), and `resetIntegrated()` must drop both to their sentinels on the very next read
  with no audio between — the failure a stale cache makes user-visible, the meter-reset button
  appearing to do nothing. **Mutation-verified**: dropping either `invalidateReadings()` call
  fails only its own assertions.
- Every pre-existing loudness assertion in `AnabasisTests` passes unedited, which is the
  bit-identity evidence: a cache returning anything different would fail there first.

## Amendment 4 — RESET is an explicit control, a realtime bypass audition is not measured, and the session shows its length (2026-09-27)

**On the owner's direction** (Phase 1 of the 2026-09-26 audit: the STATISTICS reset becomes an
explicit control in the repository's own terminology, and the statistics' scope is made visible,
bypass folding included), **⊕ flagged for the owner's review of the pull request that carries it.**
Filed as a dated amendment rather than an edit, because it moves three things this record described:
decision 6's panel (the header line now carries a control and a reading), the Consequences' "the
meter-reset click" (there is no longer a panel click), and the scope of the session figures, which
the Consequences place "all on the existing render tap" with a reset as the only clearing event.
Audit findings UX-002, VIS-001, VIS-009, and DOC-002 (documentation only).

1. **RESET (audit UX-002).** Until now the WHOLE panel was the reset: any mouse-down anywhere on it
   — a right-click, the first press of a double-click, the start of a drag, a click on the ~318 px
   of empty glass under the rows in Simple — discarded I, LRA, PLR and both peak holds with no
   confirmation and no undo. The body is now inert and one named control does it: a `TextButton`
   labelled **RESET** on the header line, right-aligned inside the 28 px above the first row, so no
   reading moves (decision 6's 202 px stack is unchanged). The wording follows the family's
   uppercase action-label convention (LEARN, LOCK, MATCH, BYPASS); the accessible title is
   **"Reset statistics"**, after the panel's own header, as LOCK's title "Ceiling lock" follows its
   control; the tooltip is **"Reset the integrated measurement, the loudness range and both peak
   holds"**, the existing panel tooltip's own words. It is focusable (Return presses it, as every
   editor `TextButton`) and has a `press` accessibility action. The panel's tooltip no longer says
   "Click to reset". What a reset clears is unchanged: the integrated histogram, the ungated mean,
   LRA and both peak holds (PLR follows) — plus the session length below.
   **A family deviation, recorded:** Anamorph resets its holds on a click on any readout, and its
   manual documents it. The cost is not symmetric — a stray click there clears a peak tick, here it
   discards an integrated measurement of a whole programme — which is why the deviation is taken
   and why `BRAND_CONSISTENCY_CHECKLIST.md` now lists it, so the Level-5 pass does not "restore"
   the sibling's behaviour.
2. **A realtime bypass audition is left out of the session figures (audit VIS-001).** The session
   figures — the integrated reading (gated and ungated), LRA, the TP and SP holds, and so PLR —
   describe the processed programme. A realtime BYPASS is an audition of the input (ADR-0044 makes
   it play at unity), and folding it in let a comparison raise the delivery peak holds above the
   ceiling and pull the integrated figure toward the input for the rest of the session. The engine
   now pauses the session half while any part of the bypass is audible (`bypassMix > 0`, the ramp
   included) and the session TP/SP maxima skip those frames; resuming applies
   `resetIntegrated`'s straddler watermark without clearing, so the first gating block and the
   first LRA window after a pause carry none of the paused audio. **Offline, the bypass is part
   of the rendered file and is measured like everything else** — the statistics then describe the
   file. Unchanged: the rolling readings (M, S, RMS) follow whatever plays, the GR history's
   waveform keeps reading the render peak, and every rendered sample. Stated costs: up to one
   100 ms sub-block of programme either side of a pause is not measured; LRA resumes ~3 s after it;
   the true-peak estimator's ~6-sample report delay lets at most that many samples of the closing
   ramp (under 2 % of the input's level) into the TP hold.
   **Host-dependent, separated:** a host that bypasses the plug-in through its bypass parameter
   (`getBypassParameter()` is `bypass`, so JUCE's VST3/AU wrappers route a host bypass here) gets
   the same pause. A host that instead stops calling the plug-in — a disabled or frozen track,
   some hosts' own bypass — processes nothing, so nothing is measured, the session length stops,
   and the rolling readings hold their last values (VIS-005, unchanged). Which hosts do which is
   real-DAW evidence (audit TEST-002).
3. **The session shows its length (audit VIS-009).** Beside the header: the seconds of programme
   the session figures cover, `m:ss` (h:mm:ss from an hour), whole seconds, never rounded up. It
   advances with measured audio, stops while a realtime bypass audition is audible and while no
   audio is processed, and returns to 0:00 after RESET, a state load and every prepare — exactly
   the session figures' own reset points; an A/B switch or a preset apply resets neither. One more
   relaxed scalar on the existing Meters → GUI row (`pubSessionSecs`, eleven in
   `publishSilentMeters`' list), same contract, same clear list — the precedent of this record's
   own Consequences for its five; no new path, no new ordering. `THREAD_MODEL.md` and
   `THREADING_POLICY.md`'s meter row name it.
4. **What a reset does NOT clear is documented, not changed (audit DOC-002).** The display clear
   that pairs with the request (round 33) blanks every published reading, the rolling ones
   included, until the next processed block republishes them — immediately while audio plays,
   and not until audio flows while the transport is stopped. The rolling windows themselves are
   not reset. `USER_MANUAL.md` §3.4 said "not reset" without that sentence; it says both now.

Nothing here changes a parameter, a serialized field, the DSP signal path or the latency.

Evidence [Verified]:
- `AnabasisTests` `testTheSessionFiguresPauseForABypassAudition` — an audition 12 dB over the
  programme moves I by > 3 LU when measured and by < 0.2 LU when paused; LRA likewise; the
  straddler and a reset issued during a pause pinned separately. **Mutation-verified**: the pause
  removed at either admission site, the resume watermark removed, and the straddler removed each
  fail their own checks.
- `AnabasisStateTests` `testABypassAuditionStaysOutOfTheSessionFigures` — a −0.45 dBFS input
  auditioned over a −6 dB-ceiling programme leaves SP and TP at −6.00 and I within 0.3 LU, the
  session length stops for the audition (to within its ramps), and an offline render measures it
  (SP −0.45); `testTheSessionDurationFollowsTheSessionFigures` (counting, the reset/load/prepare
  zeros with no audio, A/B and preset leaving it alone, the formatter);
  `testTheStatisticsPanelResetsOnlyFromItsResetControl` (left/right/double clicks and drags on
  the body reset nothing in both layouts; RESET's place, name, focus and hit-testing; a pointer
  press on it resets). Each fails with its mechanism reverted (the 2026-09-27 Phase 1 worklog).
  One mutant is equivalent in the pinned JUCE and recorded as such: the view's child-click flag,
  which `Component::hitTest` consults only when the parent ignores clicks.
- `dsp_tests.cpp`'s extreme-level route through the bypassed histogram now runs OFFLINE with a
  premise that the integrated reading is live — it would otherwise have passed vacuously.

**Implementation note — a reset starts the session at the reset's position (2026-09-28, the fourth
PR #42 review round).** Not an amendment: nothing this record decided moves — what a reset clears,
the rolling readings, the GR history, the pause — but amendment 4's session figures did not start
where the reset put them, and the fix states the semantics it enforces. Review finding "Old peaks
survive statistics reset" (`AnabasisEngine.h`, `resetMeterHolds`).
- **The session TP hold after a reset is the true peak of the output waveform at positions from
  the reset on.** The output estimator (`TruePeakEstimator`) reports `kReportLag` = 6 samples late,
  so its first six readings after a reset describe positions BEFORE it — and they reached the
  fresh hold: a reset straight after a loud passage, then silence, brought the old peak back into
  the new session, up to **+0.96 dB above the old session's own maximum** (those last positions
  had never been reported to it). The session TP now skips exactly those readings
  (`sessionTpSkip`); every later reading describes a post-reset position, so no post-reset sample
  or 4× point is skipped. **The estimator keeps its history**, deliberately: restarting it from
  zeros invents an onset that is not in the audio (a continuing programme read up to +0.97 dB
  high), and the rolling reading and the GR history stay continuous. What a reading of a
  post-reset position may still carry is the real waveform there — the 4× kernel's tail over the
  samples just before the reset, bounded analytically at 0.2504 × the pre-reset sample peak
  (−12 dB). The sample-peak hold was never affected (it has no lag).
- **The integrated reading's first admitted sub-block starts at least one whole sub-block
  (100 ms) after a reset or a bypass resume** (`LoudnessMeter::firstCleanSubBlock`): the straddler
  rule admitted a sub-block starting right at the reset, into which the K-weighting filters (never
  cleared — that would notch the rolling windows) ring the pre-reset programme; 5 s of digital
  silence after a reset read −33.7 LUFS integrated where the empty value belongs. The guard was
  first built at half a sub-block (50 ms, `0f162c8`); the round's independent review then measured
  DC at 0.99 cut at that minimum gap leaving the ungated mean at −115.5 LUFS after 2 s of silence
  (16 of 336 reset positions, 8–192 kHz; integrated empty in all 336), and at odd sub-block lengths
  the gap was 49.98 ms, not 50. At a whole sub-block (`f03d673`): 0 of 336, worst −120.69 LUFS —
  the floor. Amendment 4's stated cost changes from "up to one 100 ms sub-block either side of a
  pause" to up to 100 ms before a pause and up to 200 ms after a pause or a reset; LRA's ~3 s is
  unchanged.
- Guards: `AnabasisTests` `testStatisticsResetStartsTheSessionAtTheReset` (the review scenario with
  TP on and off, one and three presses, a reset with no history, a continuing programme — never
  above the continuous meter and every later reading covered — a click in the first post-reset
  samples, the loudness ring-out on and off a sub-block boundary, and the review's DC ring-out at
  the minimum admitted gap on the meter itself at 22.05 / 48 / 96 kHz); `AnabasisStateTests`
  `testResetRightAfterALoudPassageKeepsTheOldPeakOut` (the processor's own request and published
  holds). Unfixed: 6 DSP and 2 state failures; skipping the estimator's whole reach, or restarting
  it from zeros, each fails its own checks (the fourth-round worklog).

**Implementation note — what the session length counts (2026-09-29, review item "Session clock
includes unmeasured return audio", `src/dsp/LoudnessMeter.h:205` `setSessionPaused`).** Not an
amendment: item 3's length is kept as built and its relation to the figures is stated. It counts
the frames the session is OPEN for since the last reset or prepare — every frame with no part of
a realtime bypass audible (`bypassMix` at 0), every frame of an offline render — from the first
open frame after a resume or a reset, exactly; those are the frames the SP hold takes and at
which the TP hold takes its readings. It is not the audio admitted into I or LRA, which is no
duration (gated 400 ms blocks, 3 s windows): the watermark leaves up to 100 ms before a pause and
100–200 ms after a resume out of both (item 2's cost as the note above restates it), exactly as
the length already counted from a RESET while I skipped its first 100–200 ms. So the length
counts a fraction of a second per audition that I does not, and an open run under 0.5 s between
auditions reaches the length and the holds but no I or LRA block. Measured on the engine
(44.1 / 48 / 96 kHz, blocks 64 / 512; local Release, LTO off): length = open frames exactly in
every scenario, holds bit-identical to a model over the same frames, I's first block after each
of 720 resumes (a reset inside each audition, so the block is observable) exactly where the
watermark rule puts it (100.0–199.6 ms of length beyond its 400 ms), LRA's ~3.2 s later. Guards: `AnabasisTests`
`testTheSessionClockCountsTheOpenFramesNotTheAdmittedAudio`, `AnabasisStateTests`
`testThePublishedSessionLengthIsTheOpenFrames`; `USER_MANUAL.md` §3.4 states it. **⊕ For the
owner's review with amendment 4 itself:** this note settles how item 3's "the seconds of programme
the session figures cover" is read — as the frames the session is open, which the peak holds cover
and I / LRA measure with their stated guards — rather than as the audio I admits (which would stall
the clock in fast A/B comparison, move the SP/TP holds and the clock apart, and change the recorded
and pinned RESET behaviour). The behaviour is unchanged; the reading is recorded so the review can
overturn it.
