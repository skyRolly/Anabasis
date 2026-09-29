# 2026-09-28 — PR #42 review, third round: the true-peak contract's two open paths, GR max, and the copy record

The fourth round on PR #42, after the Phase 1 round
([`2026-09-27-phase1-match-statistics-observability.md`](2026-09-27-phase1-match-statistics-observability.md))
had been pushed as `dd983ec`. Asked for: a correctness and review-closure round, no Phase 1 expansion —
fix the two DSP defects a new review confirmed (a TP engagement decay surviving into an offline render;
TP mode leaking a descending ceiling), fix the GR max readout's contract, settle the STATISTICS
tooltip's copy through the repository's own process, verify CI, and only then reassess Phase 1. The
closure record is [`docs/reports/2026-09-28-pr42-review-closure.md`](../docs/reports/2026-09-28-pr42-review-closure.md);
this file is the evidence behind it.

Same container and toolchain as the earlier rounds (Linux x86-64, Xeon @ 2.10 GHz, 4 cores; GCC 13.3
Release + LTO for the suites and the plug-in; clang 22.1.8 for the realtime tiers and the warning
gate). Engine figures come from the real `AnabasisEngine`. The investigations ran as independent
scratch probes (nothing under the repository was touched until a fix was chosen); their method is
written down here and their probes, logs and patches stay in the session scratch directory, not in the
tree.

## State at the start (inspected, not taken from the last report)

- PR #42 branch head `dd983ec`, base `main` `ed06ad0`.
- **GitHub CI on `dd983ec`:** every check green except **`sanitizers`, failed** — one state-suite check
  under valgrind memcheck: `grReadout: (premise) the producer pushed while the readout scanned`
  (0 memcheck errors; the DSP suite passed under valgrind). `realtime`, `linux`, `linux-lto-tests`,
  `linux-lto-clang`, `windows`, `macos`, `macos-intel` (pluginval on each platform lane), `docs`,
  `source-lint`, `preflight`, `merge-check`, CodeQL, PREfast: green.
- **Code scanning:** 36 open PREfast threads, all under `tests/` — C6262 (function stack size) on the
  engine and editor fixtures, and two new C6011 at `tests/state_tests.cpp` 11820 / 11832 in the
  TEST-001 helpers (`findFirstChildOfType`, `collectCombos`). The C6011 pair is the pattern the
  2026-09-03 scanner audit classified as group G3 (PREfast models `dynamic_cast` as returning its
  operand; `getChildren()` holds no nulls) with a recorded "DO NOT FIX" — kept as recorded, not
  rewritten (an alternative rewrite exists and is not verifiable without MSVC here).
- **The review findings are not on the PR as comments.** They arrived in the instruction; each was
  verified against the code before anything changed.
- ADRs: 0041–0044 Accepted; ADR-0020 amendment 4 Accepted on direction, ⊕.

## 1. Offline renders inherited a TP engagement's decay (`src/dsp/AnabasisEngine.cpp`)

**Confirmed.** The state transition, block by block (a host that flips `nonRealtime` without
re-preparing, within ~6 ms of a mid-playback TP engagement):

- *Toggle block (realtime).* The engagement branch starts `EngagementTail` from the last emitted
  frame, checked against the ceiling, and sets the duck to the bottom (gain 0); the bottom branch
  latches the TP composition (`latchOsConfig`: the lookahead ring, limiter, clip, clamp and
  oversampler restart) and sets the refill hold. The block's output is the decay alone.
- *Next block (offline entry).* The engagement branch is skipped (`! enteringOffline`); the
  direct-adopt branch runs — no latch (nothing new wanted), duck **idle at unity**, hold cleared,
  duck requests dropped. **`engageTail` untouched**: `left = length − N`, `from`, `scale`, history.
- *Stage E.* The unducked clamp output plus `engageTail.value()` until the decay ends.

So the decay of the last REALTIME frame played into the head of the bounce: 256 samples at 48 kHz for
a 32-sample toggle block (peak −1.35 dBFS), 224 for 64, 32 for 256; at 96 kHz the decay is 576 samples
long and survives a 512-sample block. Summed onto the now-unducked processed path it met the Post EQ's
ring-out from before the latch (in realtime the silent bottom hides it), and the render read as a
file — fresh meters from its first sample — measured up to **+1.455 dB (product meter) / +1.459 dB
(Annex 2)** over the ceiling (18 of 180 configurations over 0.1 dB per input variant); the tail alone
+0.64 dB. The engine's own dBTP tap, which feeds the dBTP hold, read the same +1.455 dB. Measured over
720 runs (4 input variants × 180 configurations: 44.1/48/96 kHz, OS off / 2× / 16×, hot and mid
points, programme kinds 0/1/4, toggle blocks 32/64/256/512); unfixed minus fixed equals the recorded
tail term bit-exactly in all 576 runs where it was active.

The code contradicted its own comments — "the entry to offline … has nothing to continue", and the
direct adopt "makes the no-re-prepare path behave exactly like the re-prepare path" (whose `reset()`
does clear the tail). KNOWN_ISSUES KI-004 understated the route ("bounded to the first sample").

**Fix (`74c114e`).** The offline-entry branch resets the tail with the duck — history included, as
`reset()` leaves it (the render's pre-history is silence). When a decay was cut, the output dBTP tap
restarts too: with the tail reset alone, the tap read the cut as a step across the realtime/offline
splice, up to +0.650 dB over in 15 of 180 configurations where the unfixed code had read nothing. With
nothing in flight the tap keeps its history exactly as before. Both engine comments are corrected;
KI-004 is corrected (docs commit).

**What else previous-playback state reaches the render on this route — existing behaviour, not this
defect, recorded rather than changed:** the lookahead ring's realtime content (the toggle block's
samples land at `[L − N, L)`; on the TP-off route with no latch the whole of `[0, L)` is realtime
programme — the offline flip equals staying realtime), the Post-EQ ring-out, and past `L` the
compressor detectors, EQ biquads, the adaptive vector and the dither RNG (−9.4 dBFS difference from a
clean start in the first 10 ms after `L` on the hot chain, −138.5 dBFS on an inert frozen chain) —
identical before and after the fix. A host that trims the reported latency from an export never
writes the tail at all (it ends inside `[0, L)`).

**Identity.** 480 renders — TP off, realtime TP engagement without an offline entry, TP on from the
start with an offline entry, clean offline start, entry after the decay ended — × toggle blocks
32/64/256 × 48 kHz OS off / 44.1 / 96 kHz 2× / 48 kHz 16× × two chains × two programme kinds × dither
off / 16-bit: bit-identical to the unfixed engine, the engine's TP taps included.

**Test — `testOfflineEntryDropsTheEngagementTail` (63 checks).** (1) Exact silence after a silent
toggle block for 32/64/256 at 48 kHz and 512 at 96 kHz, with the premise that the decay was still
running at the entry, and a finished-decay control; (2) the ceiling of the render read as a file on
both meters and on the engine's own tap, four programme/shelf cases; (3) the render's latency window
bit-identical to a freshly prepared, offline-from-the-start engine (the one span this route can make
exact), both an inert frozen chain and the hot chain; (4) TP off: the offline flip changes no rendered
sample, and a clean TP bounce holds the ceiling. **Mutation:** the fix removed → 20 failures (4 exact
silence, 8 ceiling, 2 tap, 6 latency-window); the tail reset without the tap reset → 4 failures (the
tap).

## 2. True-peak mode under a descending ceiling (`CeilingClamp.h`, `AnabasisEngine.cpp`)

**The five questions, answered from the code at `dd983ec`.**

1. *Where the ceiling is smoothed.* One `juce::SmoothedValue<float> ceilingLinear`, a 20 ms LINEAR glide
   (`reset (sr, 0.020)`, now `kCeilingGlideSeconds`); adopted on the first block after prepare/reset,
   retargeted once per `process()` call, advanced once per base sample into `ceilArr[n]` in stage A.
2. *Where delayed true-peak samples are stored.* `CeilingClamp::audio[ch]`, a ring of `delay + 1` frames;
   the frame written at step n is emitted at step n + D, D = attack + 30.
3. *Whether threshold history is stored.* Yes: `CeilingClamp::ceilings`, written with every frame. Each
   segment's requirement is judged against the lower of its two samples' stored values, and the
   backstop clip of an emitted sample against its own.
4. *When the output sample is emitted.* D calls after it entered — one call per base sample in stage E;
   the engine takes D out of the 10 ms allowance, so the reported latency never moves.
5. *Which ceiling governed the emitted frame.* The one stored with it — `ceilArr` at ENTRY, D samples
   before emission. The value passed with the emitting call (the live one) was only stored for the
   future. The limiter plays each sample to the ceiling of the step at which it leaves the region's
   line — in TP mode also emission − D. TP-off clips at `ceilArr[n]` at emission: the live value.

**Reproduced** with an independent harness on the unmodified engine (the live smoothed ceiling rebuilt
with the engine's own smoother, driven as the engine drives it — 0 mismatches against the engine's
`ceilArr` through a hook, and 1733 of 1936 TP-off renders sit bit-exactly on it). Minimal reproducer —
48 kHz, 64-sample blocks, OS off, hot HF programme, instant 0 → −12 dB: **+1.041 dB (Annex 2) / +0.874 dB
(product meter)** over the live ceiling, 76 / 69 segments over 0.1 dB between 73 and 970 samples after
the move, the worst at the end of the glide; against the carried ceiling +0.000 dB. The matrix (1936
TP-on configurations: 48 kHz OS off–16×, 44.1/96/192 kHz OS off and 4×, blocks 64/512, Post shelf
0/+12 dB, four programmes × 11 moves): instant 0 → −12 dB +1.13 dB, −1 → −20 dB +2.72 dB, 0 → −20 dB
+3.04 dB, a 50 ms DAW ramp +0.32 dB (174 of 176 configurations over), a ±6 dB zig-zag +0.39 dB, 200 ms
ramps +0.06 dB, 1 s ramps +0.01 dB; rises and a static ceiling ≤ +0.002 dB. Sample peaks went over the
live ceiling by up to +2.55 dB. The closed form 20·log10(1 + (D/R)(c0/c1 − 1)) — R the glide in samples —
matches the engine within 0–0.26 dB; D/R is 0.0465 / 0.0438 / 0.0281 / 0.0203 at 44.1 / 48 / 96 /
192 kHz, so the smallest instant cut that breaks 0.1 dB is 1.9 / 2.0 / 3.0 / 3.9 dB. The engagement
decay, checked once at the toggle against the lower of the smoother's current and target values, can
read up to +0.071 dB over a cut made one host block after engaging (Annex 2, 1-sample blocks; +0.035 dB
at 16-sample blocks at 192 kHz; ≤ 0 at 64 and above) — inside the tolerance.

**Which ceiling is "the" ceiling — checked, not assumed.** Against the processing-time smoothed value
(the one the TP-off clip holds bit-exactly) the finding stands. Against a host-timeline-aligned value
(the smoothed ceiling shifted by the reported latency) TP descents are clean, but ascents overshoot in
BOTH modes (TP-on +7.7 dB, TP-off +8.0 dB on an instant −12 → 0 dB): the 20 ms glide is longer than the
~10 ms lead, so no path can meet that reading and it cannot be the product's definition. Re-labelling
"live" as the carried value would change no output sample — it would declare that TP mode applies
ceiling automation D samples later than TP-off — so it is not a fix.

**Candidates (each prototyped on a scratch copy, measured on the same harness).**

| Candidate | Worst over live (product / Annex 2) | Why not / why |
|---|---|---|
| Stamp frames with the predicted emission ceiling only | +0.29 / +0.32 dB (instant cuts; +0.65 on a ±20 dB zig-zag) | frames in flight at a retarget keep the old prediction |
| …and lower the stored ceilings, no re-derivation | +0.05 / +0.16 dB (+0.31 on the ±20 dB zig-zag) | requirements already derived stay lenient |
| Output gain of live / carried | +0.02 dB | dips material BELOW the ceiling by up to −3 dB |
| A release-free live guard beside the carried path | ≤ +0.003 / +0.009 dB (+0.032 at 22.05 kHz) | two thresholds for one sample; open segments bounded approximately; see the judge below |
| **Predict, revise in flight, re-derive (P)** | **≤ +0.003 / +0.004 dB** | exact within the clamp's own model |
| **P with the limiter on the same value (PL) — chosen** | same as P | P alone leaves the clamp the whole descent: its release then holds a dip after the ceiling lands |

**Why PL and not P.** Measured with a scratch probe on a 997 Hz sine held at the ceiling by the
limiter at its default voicing — the lowest 1 ms true-peak reading below the landed ceiling in the
50 ms after an instant cut lands: −1 → −20 dB at 48 kHz / 64: HEAD −1.05 dB, **PL −0.99 dB**, P −2.39 dB;
0 → −12 dB: HEAD −0.43, PL −0.42, P −1.10; at 4× / 512: PL −1.26, P −2.83; 96 / 192 kHz: PL −0.97 / −1.02,
P −1.94 / −1.85. (TP-off: −0.001 dB — its clip has no release.) Without the limiter half the clamp takes
every descent at its 0.25 ms attack and its 10 ms release then holds the reduction after the ceiling has
landed; with it the limiter follows the ceiling as in TP-off and the clamp's post-landing behaviour is
the unfixed engine's.

**The fix (`dce072b`, ADR-0045).** At every block top in TP mode the engine copies the smoother and runs
the copy `clampDelay` steps ahead (`ceilingAhead`); stage A stores its per-sample value (`ceilEmitArr`) —
the value the smoother will have when that sample is emitted, exact because the copy runs the same float
operations in the same order (`skip()` multiplies instead and came out ABOVE the iterated value in
9,364–9,802 of 20,000 random glide states per rate). The clamp stores it with the frame; the region's
limiter reads it in TP mode. At a retarget the new trajectory's values at the in-flight frames' emissions
are computed the same way, and `CeilingClamp::lowerInFlightCeilings` lowers each stored ceiling to it
(never raises one) and re-derives, from a stored per-segment reading, every requirement that still
constrains a gain not yet applied — r for the segments that read a lowered frame, then the 32-wide min,
the forward min and the mean ring over the steps the next emissions read (a van Herk / Gil-Werman sliding
minimum). A clamp revised mid-stream held rings and below-counts bit-identical to a clamp handed the
lowered ceilings from the start, in 1600 of 1600 trials (4 rates, every revision position). A smoother
that is not gliding has had no retarget for a full ramp, so nothing is revised then.

**Tests (`testTruePeakModeHoldsTheCeilingUnderAutomation`, 7 checks over 100 TP renders and a TP-off
control).** Every reading whose segment starts at or after the move against the lower live value of its
two samples, on both meters, and every emitted sample against the live value itself; moves: instant
0 → −12 and −1 → −20 dB, a 50 ms ramp, a ±6 dB zig-zag every 5 ms, −12 → 0 dB instant and over 200 ms,
static −0.1 dB; 48 kHz at blocks 64/512 and OS off/4×, hot HF programme with and without the +12 dB Post
shelf, a driven sine at the limiter's default voicing; 44.1 kHz on the descending moves; OS 2×/8×/16× on
the transient programme; 96 and 192 kHz. Premise: every descending render reaches within 1 dB of the live
ceiling during the move. The post-landing dip of the sine after the −1 → −20 dB cut is bounded at 1.6 dB
(PL 0.97–1.26, the limiter half removed 1.85–2.83). **Before the fix:** 64 of 100 renders over, worst
**+2.44 / +2.57 dB**. **Mutants, each failing:** frames stamped at entry (+2.32 dB), the revision removed
(+0.24 dB), stored ceilings lowered without re-deriving the requirements (+0.15 dB), the backstop not
lowered (+0.24 dB), the limiter left on the entry-time value (dip −2.83 dB). Equivalent and recorded: the
not-gliding early exit.

**Realtime.** No allocation after `prepare` (the repository's `AllocationGuard` armed around every
`process()` over 104 TP-on automated configurations, down to 1-sample blocks; the prototype round). The
effects tier: `tests/realtime_effects.cpp` now drives `lowerInFlightCeilings` and the tail reset; clean
compile with clang 22 and the CI flags, canary still failing, and an allocation seeded inside
`lowerInFlightCeilings` is reported at the driver's call. The RTSan DSP suite passes with 0 reports.
Cost: a revision with a full rebuild ~0.5 µs at 48 kHz, ~1.3 µs at 192 kHz, once per block at most;
whole-engine CPU within noise at blocks of 16 and up, +19 % / +29 % (the head-to-head, on a machine at load 5–8: +25 % / +33–40 %) at 1-sample host blocks with the
target changing every block (48 / 192 kHz) — the pathological case.

**The adversarial review of P / PL** (an independent agent, told to find the reason it must not ship;
3532 runs over 22.05–384 kHz, OS off to 16×, 1-sample, irregular and oversize host blocks, prepare /
reset, TP, oversampling and offline toggles in mid-glide, mono; a standalone clamp-invariant test over
19.5 million frames; ASan/UBSan builds): the ceiling logic held — worst **+0.012 dB (product) /
+0.023 dB (Annex 2)** over the live ceiling, no sample above it, no revision that raised a requirement or
a gain, TP-off and static-ceiling identity, 0 allocations over ~162 million armed `process()` calls. What
it found, and what was done:

| Finding | Severity (reviewer) | Outcome |
|---|---|---|
| A conflict with Accepted ADR-0041 decision 3 | blocker (process) | ADR-0045, taken on the owner's direction and ⊕ for review — the gate item is named, not cleared by a green build |
| A one-sample level step at a downward retarget with the output at the ceiling: 0.0185 at 48 kHz (0.0197 / 0.0117 / 0.0083 / 0.0367 at 44.1 / 96 / 192 / 22.05 kHz) against a 0.0008–0.0019 TP-off glide; `testCeilingIsSmoothed` never ran in TP mode | major | **Recorded as invariant 8 yielding to invariant 4** (ADR-0045, prescribed invariant 8 text) and pinned: `testTruePeakModeBoundsTheStepAtACeilingCut` (the step ≤ (16 + A/2 + 2) glide steps: 0.0197 against 0.0211 at 44.1 kHz, 0.0185 against 0.0198 at 48 kHz; TP off as the control). The alternative — spend part of the 0.1 dB tolerance on the frames in flight — is laid out in the ADR for the owner, not prototyped |
| P alone moves 2–4 dB of fast limiting into the clamp, invisible on every GR display | major (P only) | PL shipped |
| The effects tier did not reach the new function | minor | driven (a seeded allocation is now reported) |
| No regression test | minor | the automation test and the step test |
| The guarantee depends on the glide's slope; the not-gliding exit's justification named the wrong property | minor | stated in the clamp header and at the smoother's reset; the comment corrected |
| PL's limiter threshold re-predicts at a retarget: at Transients 0 a 0.35 dB limiter step at 48 kHz (0.07 before); at the default setting identical to before | minor | recorded in ADR-0045 |
| Stale comments | minor | rewritten for PL |
| **Pre-existing, found on the way:** a mid-stream latch without a duck (entering offline with a factor change, e.g. Force Max, without a re-prepare) or a host `reset()` cuts the pipeline to zero at full gain, and the stream readings straddling the cut read up to +0.96 / +0.98 dB — identical before and after this round | minor (pre-existing) | **Recorded, not changed** (`KNOWN_ISSUES.md` KI-024): the cut sits at a boundary the host itself draws, and the render read as a file starts from the emptied pipeline's zeros (by construction; not separately measured). Closing it needs a checked decay at the cut, a design item of its own; it is the next true-peak item before Phase 1 resumes (closure record §4) |

**The head-to-head** (an independent agent: HEAD, P, PL and G built from one probe source and run on
the SAME 14440 configurations — 22.05 / 32 / 44.1 / 48 / 88.2 / 96 / 176.4 / 192 kHz, OS off to 16× at
48 kHz and off / 4× elsewhere, host blocks 1 / 16 / 64 / 512 and an irregular sequence against a
512-sample prepared maximum, Post shelf 0 / +12 dB, four programmes, fifteen moves and a TP engagement
with the cut 0–4 blocks later):

| | HEAD | P | **PL** | G |
|---|---|---|---|---|
| Worst over the live ceiling, product / Annex 2 (all moves) | +4.92 / +4.99 dB | +0.059 / +0.071 | **+0.059 / +0.071** | +0.059 / +0.071 |
| …of which outside the engagement decay | +4.92 / +4.99 | +0.003 / +0.011 | **+0.003 / +0.011** | +0.027 / +0.032 at 64/512 blocks |
| Configurations over 0.1 dB on either meter | 7156 | 0 | **0** | 0 |
| In-glide level of a sine at the ceiling, mean / worst 1 ms bin vs live (−1 → −20 dB) | +0.76 / +0.16 | −0.03 / −0.57 | **−0.02 / −0.40** | −0.001 / −0.013 |
| Post-landing dip, sine at the ceiling, worst extra vs HEAD (any move) | — | −3.07 dB | **−0.25 dB** (median −0.01) | −0.64 dB (median 0) |
| …all programmes, worst extra vs HEAD | — | −3.07 | **−1.57** (median 0) | −1.18 (median 0) |
| Largest one-sample clamp-gain change, sine, −1 → −20 dB | 0.037 dB | 0.308 | **0.308** | 0.184 |

The +0.059 / +0.071 dB common to every candidate is the engagement decay (one block after engaging,
1-sample host blocks) — ADR-0045's recorded residual, bit-identical to HEAD there. On this matrix all
three hold the tolerance; they trade artefacts. G tracks a sine more tightly during a glide, steps
less at a retarget and costs less at 1-sample blocks with the target moving every block (+2–13 %
against PL's +25–40 %; at blocks of 64 and 512 PL is within noise and G +5–15 %). PL leaves less
behind after the ceiling lands, and it is the only candidate that returns the clamp's workload to
HEAD's (deepest clamp gain on the at-ceiling sine −2.11 dB, HEAD −2.14, G and P −6.24). PL also
changes every TP-on render with a RISING ceiling (1520 of 1520; the limiter follows the ceiling up
`clampDelay` samples earlier, 1 ms bins −0.015 to +1.20 dB against HEAD, never above the live ceiling);
G changes 17 of them by ≤ 4.8·10⁻⁷. TP off (1824 renders), TP on with a static ceiling (1520) and
sub-ceiling programme under automation (1235) are bit-identical for all three; latency and the impulse
position are identical in every configuration.

**The judge ranked G first, PL second, P last. PL is kept**, on two grounds:

1. **G does not hold the promise under an adversarial search.** The adversarial review of G, run
   alongside, found its open segments judged against a one-sample peek of the live ceiling rather
   than their own emission-time value: a transient burst aligned with the bottom of a 0 → −20 dB cut
   reads **+0.14 dB (Annex 2) over the live ceiling at 22.05 kHz, +0.11 dB at 32 kHz and +0.42 dB at
   8 kHz** (the TP path engages down to 4 kHz), where P reads −0.59 / −0.48 / −0.11 dB on the same
   inputs. Its repair is to run the smoother ahead — PL's prediction.
2. **The reason that made P unacceptable:** in G the limiter still plays to the entry-time ceiling, so
   the clamp's guard does the ceiling-following — up to the lag ratio, ~3 dB of fast reduction at the
   end of a full-range glide — where no GR display shows it. In PL the limiter does it, as in TP-off,
   and the clamp stays the backstop its header describes.

The bursts that broke G, replayed on the shipped engine over their alignment sweeps (4, 8, 16, 22.05,
32, 44.1 and 48 kHz): **≤ +0.000 dB** on both meters at every rate. A fresh engine-level search aimed at
the shipped engine: see *Adversarial search against the shipped engine* below.

**Adversarial search against the shipped engine** (after the judge; the engine at `dce072b` — the
code of this PR's head). A 32-sample burst and its position, hill-climbed on the real engine to
maximise the larger of the two defining meters over the live smoothed ceiling: a 0 → −20 dB cut at
1000 ms, the burst near the bottom of the glide, an idle chain (no drive, compressor or clipper; the
limiter at its default voicing), OS off, host blocks of 1, 16, 37 and 64 samples and four irregular
schedules against a 512-sample maximum, seeded from G's breaking bursts and random patterns; 42 runs
of 1500 steps over 4–48 kHz, then 24 runs of 3000–4000 steps at 22.05–48 kHz. Every worst burst was
replayed with a static ceiling and on the 0.2.14 engine and on P, to classify it:

| Rate | Worst under the cut (shipped) | Same burst, static ceiling | 0.2.14, same burst | Class |
|---|---|---|---|---|
| 4 kHz | +0.23 dB (A2) | +0.23 | +0.23 static / +1.76 under the cut | **static floor, over** |
| 8 kHz | +0.11 | +0.11 | +0.11 / +0.11 | **static floor, over** |
| 16 kHz | +0.12 | +0.12 | +0.12 / +0.12 | **static floor, over** |
| 22.05 kHz | **+0.157** (product meter +0.033) | −0.64 | +3.56 | **automation, over** |
| 32 kHz | **+0.125** (product meter −0.004) | −0.85 | +3.07 | **automation, over** |
| 44.1 kHz | +0.058 | +0.058 | +0.058 | static floor, inside |
| 48 kHz | +0.026 | +0.026 | +0.024 / +0.026 | static floor, inside |

A dedicated static search at 22.05 kHz found at most +0.056 dB. P under its OWN search at 22.05 kHz
reads **+0.309 dB** (the shipped engine −0.87 dB on that burst); on the shipped engine's worst burst P
reads −0.76 dB. So the automation residual is not PL's: both designs leave one, and PL's is the smaller.

**Mechanism** (an instrumented scratch copy recording the clamp's gain per frame, on the 22.05 kHz
burst): the emission-time stamps are exact there (17 ms after the retarget), and the clamp is idle
(gain 0.99927) through the segment that reads over; two samples later an attack ramp begins
(0.960, 0.921, … 0.763 over six samples) for a later segment whose input is ~2.4 dB over the falling
ceiling. Those reduced samples sit inside the over-reading segment's Annex 2 window, and lowering them
raises its interpolated peak through the kernel's negative lobes. The clamp's requirement keeps every
gain a segment reads at or under that segment's own `r`, which bounds the peak only while those gains
are equal. At 22.05 and 32 kHz the attack is at its 8-sample floor and, at the bottom of a 20 ms
0 → −20 dB glide, the ceiling falls ~1.5 % per sample, so the limiter leaves the clamp large overs and
the ramps are steep. Recorded as `KNOWN_ISSUES.md` KI-025 (with the static floor below 22.05 kHz,
pre-existing and bit-identical in 0.2.14); the clamp header's "≤ +0.023 dB, 22.05–384 kHz", ADR-0045's
Consequences and the CHANGELOG's "every tested case" are corrected (`b55831a`). Closing it — a longer
attack at low rates (a ⊕ voicing constant and the true-peak delay composition) or a requirement that
bounds a ramp's effect on its neighbours — is a design decision of its own and is not taken here.

## 3. GR max could omit history the graph still drew (`src/gui/GrHistoryView.h`)

**Confirmed, both ways the ranges differed.** The readout's max scanned `[head − min(windowEntries,
kSize − 1 − 4096), head)`; the graph draws the bucket-aligned range `[buckets(head, want,
cols).first, (kLast + 1)·stride)`, which begins OLDER than `head − want` by the lead buckets, the
`kFull` round-up and the alignment. At the shipped plot widths (904 columns Simple, 604 Advanced) GR
max missed 3–2027 drawn entries (0.07–0.21 s) at every non-saturated pair from 44.1 kHz / 16 to
384 kHz / 2048 — at 48 kHz / 512, 9–11 entries (96–117 ms) on Simple, 17–20 (181–213 ms) on Advanced —
and 3388–3822 entries (0.16–0.17 s) at the saturated pairs, where the lap margin also cut the range.
In the other direction it included up to `stride − 1` newest entries the graph had not drawn yet
(≤ 33 ms).

**Contract.** *The GR max readout represents the deepest limiter gain reduction among the entries the
GR history graph draws at the reading's head in the current layout — every entry of every drawn
bucket, from `buckets(head, windowEntries, plotColumns(GR view bounds)).first` — plus the newest,
still-collecting bucket the graph holds back and "now" already reports; so max ≥ now at every head.*
It depends on the plot width because the graph's oldest edge does; the GR view keeps its bounds in both
layouts while the spectrum owns the well, so the figure is what the GR graph shows the moment it is
revealed.

**Fix (`e380d72`).** `readingFrom (ring, cols)` scans that range in two lap-certified chunks: the
overhang beyond the guarded span (empty unless the drawn history outgrows `kSize − 1 − 4096`, i.e.
only at (near-)saturated pairs) is read first and certified on its own; the guarded span is read and
closed as before with `kReadoutLapMargin` pushes in hand; either failing discards the reading, so a
taken reading is always the exact minimum. `plotColumns` is `paintHistory`'s own expression, now
shared. No new thread, atomic, ordering or audio-thread change; +4–12 % per read
(`PERFORMANCE_BUDGET.md`).

**The valgrind failure.** The old test ran a real producer thread against 60 scans and required it to
have pushed during them. valgrind runs one thread at a time and, with its default unfair scheduling,
the compute-bound reader kept the lock: 0–4 producer pushes during the reads, a premise failure in 3 of
20 standalone runs (20 of 20 passing with `--fair-sched=yes`) — intermittent, which is why it failed in
CI and not in every local run. The premise was also weaker than its message (a moved head proves pushes
between readings, not during a scan). Replaced by a deterministic producer: `readingFrom` is a
template (a test seam only), and a ring adaptor pushes a chosen burst at a chosen peek. It pins that a
lapped scan is never published, every taken reading is exact, and each chunk absorbs exactly its slack
— `kReadoutLapMargin` for the guarded span at a saturated pair — and not one push more.

**Tests.** `testTheGrReadoutReadsTheRingItNames` rewritten (the contract at every head residue on both
wells at 48 kHz / 512 and 384 kHz / 16; a brute-force minimum at 904, 604, 97 and 1999 columns while
filling and scrolling; the deterministic lap block; stall rule and formatter unchanged);
`testTheTickMaxIsTheDeepestEntryTheGraphDraws` on the real processor and editor (one burst aged to be
the graph's oldest drawn entry, older than the nominal window; the label checked in Simple, with the
SPECTRUM showing, and in Advanced). **Mutation:** the old range → 7 failures; the overhang check removed
→ 2; the final lap check removed → 2. Under valgrind (the CI flags) on this build: see the gates section.

## 4. The STATISTICS tooltip's copy

**Finding valid for the tooltip's second clause only.** Commit `7166140` had to drop "Click to reset…"
(the body is inert; those words became RESET's tooltip, which ADR-0020 amendment 4 records). In the same
edit it added "a bypass you listen to is not measured into the session figures". The repository's
convention does not cover it: `AI_AGENT_POLICY.md` C8 makes UI text the maintainer's wording and says a
behaviour change "does not license announcing it in the UI"; the only earlier wording for the meaning is
the audit's different, on-panel proposal (routed to the owner); every text that now carries it (the
manual, the amendment, the DESIGN note, the CHANGELOG) was written in the same commit; and no
owner-decision list names it. The first clause, "Waveform statistics off the output", is the recorded
0.1.1 wording (`d1640bb`, ⊕ under HANDOVER's fine-review item (j)).

**Resolution (docs commit).** A documented decision item, **OQ-018** (`docs/OPEN_QUESTIONS.md`):
current placeholder, intended meaning (from the engine condition), owner decision required (C8), the
affected surface, three options. Interim, so nothing undecided ships: option 1 — the tooltip returns to
the recorded first clause verbatim. The same check found the "lim GR" / "GR max" captions and tips in the
same position (conventions cover their parts, not the words; the audit routes the GR label to the
maintainer), recorded as **OQ-019**, kept shipping as flagged placeholders because an unlabelled number
would not name its stage. RESET, "Reset statistics", RESET's tip and the session-time format are already
recorded decisions (ADR-0020 amendment 4) and are unchanged. One behavioural assertion guards the tooltip
without pinning words: it must not tell the user to click the inert body.

## 5. Gates

Local, on the final tree (the code of `e380d72`; the docs commits change no compiled file but the
tooltip and its truth check, both in it):

| Gate | Result |
|---|---|
| GCC 13.3 Release + LTO, both suites | 649 / 649 and 1584 / 1584 |
| clang 22.1.8 Release, all targets; the first-party warning gate | builds; **0 first-party warnings** (2 in vendored code, not gated); the gate's self-test 18 / 18 |
| clang-22 suites | 649 / 649 and 1584 / 1584 |
| RTSan canary | fires (exit 43, one report) — the tier is live |
| Effects tier (`-Werror=function-effects`) | clean compile; its canary fails to compile as it must |
| RTSan DSP suite | 644 / 644, 0 reports (the allocation guard compiles out under RTSan, as disclosed) |
| check-docs, check-realtime (1 / 1 ordering requirement), check-portability | clean |
| check-citations against `origin/main`, the merge base and `dd983ec` (this push's predecessor, the base CI uses) | clean after re-anchoring 5 citations the engine's moved lines had drifted (ADR-0013, ADR-0014, ADR-0039) |
| `scripts/preflight.sh` | passes on the final tree; on the intermediate head `e380d72` its push-predecessor citation check flagged the five anchors the code commits moved and commit 4 re-anchored — CI compares a push against its predecessor (`dd983ec`), where they pass |
| pluginval at the CI strictness, editor under Xvfb | deterministic ×3 and randomise ×3: every pass on its first attempt |
| valgrind memcheck, the state suite, CI flags | 1584 / 1584, 0 memcheck errors (the DSP suite under valgrind: CI, below) |

**GitHub CI on `f048328`** (commits 1–4; the code of this PR's head — `c43be87` after it changes one
comment): **every check passed** — Build & Validate on push (run 36388515614: docs, preflight,
source-lint with the citation gate against `dd983ec`, linux with pluginval ×3 both modes, linux-lto-tests,
linux-lto-clang, realtime, sanitizers with ASan/UBSan and valgrind on BOTH suites, windows, macos,
macos-intel, each with its pluginval lanes), the pull-request run's merge-check, CodeQL (actions and
c-cpp; "no new alerts in code changed by this pull request"), PREfast, and dependency review. The
`sanitizers` job that failed on `dd983ec` is green.

**Warnings, recorded:** the 2 vendored clang warnings (ungated, as before); the PREfast C6262 and C6011
threads (*State at the start*), plus ONE new C6262 from this round — 46 KB of stack in
`testTheTickMaxIsTheDeepestEntryTheGraphDraws` (`tests/state_tests.cpp`, a test fixture on the stack,
the class the earlier 36 are); 37 threads open, all under `tests/`.

## 6. Decisions this round

| Decision | Taken how | Where it is recorded |
|---|---|---|
| Each true-peak frame answers to the ceiling in force at its EMISSION, and the limiter plays to the same value (PL) | On the owner's direction ("the output must obey the declared ceiling … at every output sample"), the design chosen on the measurements in §2; **⊕ for the owner's review** — an Architecture Review Gate item on two counts: a conflict with Accepted ADR-0041 decision 3, and a change to the ceiling stage and the limiter's threshold timing in TP mode. A green build does not clear it | ADR-0045; ADR-0041's dated banner; `ADR_INDEX.md` row and registry |
| PL over G | Measured (the head-to-head above): both hold the promise; in G the clamp's guard does the ceiling-following where no GR display shows it | ADR-0045 Options F; this worklog |
| Invariant 8 yields to invariant 4 at a downward retarget, bounded at about (16 + A/2 + 1) glide steps in one sample | Recorded, not rate-limited; the prescribed policy text is in ADR-0045 decision 5 (`ADR_POLICY.md` rule 5). **The alternative — spend part of the 0.1 dB tolerance on the frames in flight — is the owner's choice**, laid out and not prototyped | ADR-0045 Consequences; `DSP_POLICY.md` invariants 4 and 8; `testTruePeakModeBoundsTheStepAtACeilingCut` |
| Offline entry drops the engagement decay, and restarts the output dBTP tap only when it cut one | The smallest change that removes the leak: the lookahead ring, Post-EQ ring-out, detectors and RNG are left as they were (not this defect; identical before and after) | ADR-0041 decision 5 implementation note; KI-004 corrected |
| GR max = the deepest entry the GR history graph draws in the current layout, plus the newest still-collecting bucket | The one-sentence contract of §3; no new realtime communication, atomic or ordering; `readingFrom` a template only as a test seam | `USER_MANUAL.md` §3.4; `PERFORMANCE_BUDGET.md`; `procedures/TESTING.md` |
| The STATISTICS tooltip returns to its recorded 0.1.1 wording; the bypass clause becomes an owner decision | `AI_AGENT_POLICY.md` C8 — no convention covers the clause, so it is recorded, not rewritten | OQ-018 (interim applied), OQ-019 (the GR readout's captions, kept as flagged placeholders) |
| A reset or an unducked latch cutting the TP stream at full gain is recorded, not fixed | Pre-existing (identical before and after this round) and needs a design of its own (a checked decay at the cut); the next true-peak item, ahead of DSP-005 | `KNOWN_ISSUES.md` KI-024; closure record §4 |
| The low-rate residual a worst-case search found (KI-025) is recorded, not fixed | It is the clamp's gain law, shared by every candidate (P's is larger), and closing it changes a ⊕ voicing constant or the clamp's requirement — a decision for the owner, not a patch inside this round; recorded with KI-024 as the true-peak items before Phase 1 resumes | `KNOWN_ISSUES.md` KI-025; ADR-0045 Consequences; closure record §2 and §4 |
| The two PREfast C6011 threads in the TEST-001 helpers stay as they are | The 2026-09-03 scanner audit's group G3 "DO NOT FIX" (a `dynamic_cast` modelling artefact) | this worklog, *State at the start* |
| Version 0.2.15 | The repository's per-round version rule; nothing tagged | `CHANGELOG.md`, `CMakeLists.txt` |

Not done, deliberately: no Phase 1 feature work; no latency, parameter-ID, saved-state, threading or
signal-order change; nothing in the Anamorph repository touched.
