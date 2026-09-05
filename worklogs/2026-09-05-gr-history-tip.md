# Worklog — 0.2.11 and 0.2.12: the GR history's newest vertex, and its right edge (2026-09-04/05)

Session-local evidence trail for version 0.2.11 (§1–§6) and, appended the same day, 0.2.12 (§7).
Raw investigation material, NOT architecture
documentation — `docs/SOURCE_OF_TRUTH.md`: worklogs are never cited as policy. What is binding is
`CHANGELOG.md`, the code and the tests; this file carries the measurements, the alternatives that
were rejected, and the parts of the previous round's account that this round found to be wrong.

Owner report, verbatim:

> *"The GR History yellow line that was previously specified to be fixed still has the jitter
> issue in the newly generated portion. In fact, it was not fixed at all. Rather than describing
> it simply as 'jitter' in the newly generated portion, it would be more accurate to say that the
> newly generated line can have instantaneous changes, and it also changes while it is moving."*

The owner asked for an investigation first — root cause, reproduction, and a description of the
symptom to compare against theirs — and confirmed the description before any fix was made:

> *"A distinctive spike continues to move left at a steady speed with a stable shape once it has
> moved away from the right edge. The visible instability is confined to the newly generated
> portion at the right edge: its height changes instantaneously and its slope/shape changes while
> it is moving. So we are describing the same problem."*

---

## 1 — How it was reproduced

`worklogs/` carries no rendered frames and 0.2.8's model (`2026-09-01-gr-history-scroll-jitter.md`
§8) tracked a COMPLETED bucket on an ideal host, which is exactly the part of the display that was
fine. This round built a frame-by-frame harness in the session scratch directory (not in the tree)
that drives the REAL `AnabasisAudioProcessor` and the REAL `GrHistoryView`: a simulated host pushes
blocks through `processBlock` on a chosen delivery pattern, a simulated 60 Hz frame clock calls
`tick (dt)`, and every frame is rendered through `paintHistory` into an image with JUCE's software
renderer (the GL path rasterises through the same `EdgeTable`, argued in the review below). The
0.2.7 painter and, for the fix round, the pre-fix 0.2.8 painter were ported beside it — statics and
paint loop verbatim — and drawn from the same ring, head and phase on every frame, so the variants
compare vertex for vertex and pixel for pixel. Programme: a kick/snare/bass synthesis at 2 Hz with
+6 dB of limiter drive ("music"), and a denser 8-transients-per-second variant at +9 dB
("transient"). Configurations: Simple well (904 plot columns) and Advanced (604); 44.1 and 48 kHz;
512, 1024 and 2048-sample blocks; 1× and 2× render scale; ideal, jittered (σ 1–3 ms), bursty
(2/4/8 blocks per callback) and mis-sized (1024 prepared, 512/256/128 delivered) hosts.

**What the completed trace did (pre-fix 0.2.8, ideal host, every configuration):** a mid-trace
vertex moved −0.754 px on every frame with σ 0.000 and no stalls; a frame compared with the frame
four earlier shifted by three whole pixels (the true shift is 3.016 px) differed by 0.3 % on the
trace's pixels. The 0.2.8 claim for the completed trace is true, and it is a rigid translation.

**What the newest vertex did (pre-fix 0.2.8):**

| Simple well, 48 kHz / 512, 60 Hz | music | transient |
|---|---|---|
| newest vertex height change per frame, mean / max | 2.0 / 22 px | 2.6 / 16 px |
| height jumps > 3 px per second | 9 | 19 |
| already-drawn vertex revised, per second | 34 | 25 |
| segment re-sloped (same two vertices), per second | 40 | 40 |
| flat ledge at the tip collapsing into a spike, per second | 0–1 | 0.2 |
| last plot column dark (no accent pixel), frames | 74 % | 57 % |

Advanced well (238 px plot, 9.9 px per dB): mean 5.0 px per frame, maximum 57 px, 36 revisions
per second, 9 collapses in 8 s. 44.1 kHz / 1024 on the Simple well (stride 1): the tip was
bit-identical to 0.2.7's.

**0.2.7 on the same frames:** the tip's height moved 2.9 px per frame against 0.2.8's 2.6 (2.02
vs 1.99 on the music programme; identical at stride 1), with jumps over 3 px at 21 vs 19 per
second and MORE upward pops in 0.2.8 on the Advanced well (the sliding minimum rises whenever the
block that set it leaves the window; 0.2.7's partial-bucket minimum could only rise at a bucket
start). The 0.2.8 CHANGELOG's "no longer pops … 0.99 → 0.55 dB [Verified]" was a model figure and
is not reproduced on the real limiter. "Not fixed at all" is, for the tip, measured.

## 2 — The mechanism, read off the code (pre-fix)

Three decisions combined at the right edge, all in `GrHistoryView.h`/`.cpp` at `f8ad47a`:

1. **A sliding-window value.** `tipFirst` made the newest vertex aggregate the trailing `stride`
   entries `[head − stride, head)`, re-evaluated on every push — so it changed in both directions,
   dropping when a deep block entered the window and rising when it left. Every other vertex read
   a fixed-identity bucket.
2. **A pinned position.** `bucketX`'s `k >= kHead` branch held the vertex at `right` while its
   bucket filled, so the segment feeding it had a left end that scrolled and a right end that did
   not: its width grew from one entry-pitch to one pitch and its slope changed every frame.
3. **A staged hand-over.** Once complete the vertex drifted by `phase`, a flat lead-out filled the
   vacated strip, and one block later a new vertex sprouted at the edge. A deep block was first
   shown as a flat ledge up to a pitch wide and then collapsed into the one-pitch spike it would
   keep.

On top: the lead-out ended at `x0 + width − 1`, the LEFT boundary of the last plot column, with a
butt cap, so that column was lit only by the 0.7 px spill of a steep segment ending there — a
one-column sliver blinking at bucket rate, which the lead-out's own comment claimed to prevent.
Present in 0.2.7 too (37 % dark).

Everything else was examined and excluded: the completed-vertex geometry is continuous across
every bucket boundary and never moves rightward; completed buckets read the same entries on every
frame; the ring, its epoch guard, the read floor and the tick-to-paint publication add no motion;
the GL renderer rasterises through the same edge table as the software path; sub-pixel
anti-aliasing does not pulse the stroke (the brightest pixel of every steep segment is constant
through the translation); the per-frame pixel activity of the older trace is content moving through
the grid at 45 px/s, present in both versions and proportional to speed; vblank timestamps are
sum-preserving on every platform.

The review that reached these conclusions ran seven independent read-only lenses over the code
(timing, geometry, data/producer, rasterisation, platform paint pipeline, records vs code, host
behaviour), then two adversarial refuters and a completeness critic over the draft conclusion and
the harness numbers. All three closers returned "stands with corrections", and the corrections are
in §1 (the 0.2.7 tip comparison, which the draft had asserted from code reading and the refuter
then measured; the 44.1 kHz stride-1 case; the last-column defect the draft had demoted).

## 3 — The fix

**Every drawn vertex is a complete bucket, and every one obeys the same law.** `Buckets` gains
`kLast`, the newest COMPLETE bucket (`kHead` when `fill == stride`, else `kHead − 1`; `count` is
now `kLast − kFirst + 1` and is 0 for the first `stride − 1` blocks after a reset). `paintHistory`
draws `kFirst … kLast`. `tipFirst` is gone; `bucketReads` reads a bucket's own span for every `k`,
which for a drawn bucket is a constant of `k`. `bucketX` loses its `k >= kHead` branch: one
expression, `right − (kHead − k)·pitch + ((stride − fill) − phase)·perEntry`, which is
`right − perEntry · ((head + phase) − (k + 1)·stride)` for every `k` — each vertex sits where its
bucket's last entry falls in time, and the newest drawn vertex lies between `right − pitch` and
`right`, landing ON the anchor the frame its bucket completes. The strip beyond it is the lead-out,
which now runs to `area.getRight()` — the clip edge — rather than to the anchor, so the last column
is part of the lead-out on every frame.

What this gives up: the newest value reaches the panel up to `stride − 1` blocks later than
before — 21 ms at 48 kHz / 512 on the Simple well, 32 ms on the Advanced — which is the price of
never revising a drawn vertex. Nothing on the audio thread, in the ring, in the smoothed head, in
the left edge or in the zero region moved.

**Alternatives rejected.**

| Option | Verdict |
|---|---|
| Keep a live tip but smooth its value in time | Rejected: still a vertex revised after it is drawn, and the revision is what the owner sees. |
| Draw the newest bucket's entries individually at their time positions | Rejected: at completion `stride` per-entry vertices would collapse into one bucket minimum — a hand-over snap of the same class as the ledge-to-spike collapse. |
| End the line at the newest complete vertex with no lead-out | Rejected: the line's end would then breathe between `right − pitch` and `right` at bucket rate. The flat lead-out is the existing design's answer and is kept, extended to the clip edge. |
| Draw the incomplete bucket at its partial minimum, frozen at completion (0.2.7's tip) | Rejected: a value revised on every block until completion; measured at 2.9 px per frame. |

## 4 — Validation

Harness, real processor and real paint path, 8 s per configuration, the fixed tree against the
pre-fix 0.2.8 and 0.2.7 painters on identical frames. The five properties the owner asked for:

| Property | FIXED | pre-fix 0.2.8 | 0.2.7 |
|---|---|---|---|
| already-drawn vertex revised (48k/512 Simple, music) | **0** | 34/s, max 22 px | 5/s |
| same newest vertex changed height between frames | **0** | 201 frames of 480 | 32 |
| segment re-sloped between the same two vertices | **0** | 40/s | 5/s |
| ledge → spike collapses (Advanced, music) | **0** | 9 in 8 s | 0 |
| mid-trace vertex per frame, ideal host | −0.754 px, σ 0.000 | −0.754, σ 0.000 | −0.753, σ 0.723 |
| last plot column dark | **0 %** of frames, ≥ 1 accent pixel on every frame | 74 % | 64 % |

The same zeros hold on every configuration run: Simple and Advanced, 44.1 and 48 kHz, 512 / 1024 /
2048 blocks, 2× scale, 3 ms host jitter, bursts of four and a mis-sized host — the last two with the
host-delivery motion unchanged (§5). New vertices appear once per bucket (31 per second at 48 kHz /
512 on the Simple well, 23 on the Advanced, 43 at 44.1 kHz / 1024), and the line's end changes
height only at those appearances, by the difference between two consecutive frozen buckets. The
rendered frames are in the session's scratch directory (contact sheets and 2 s GIFs at 3×: fixed,
pre-fix, 0.2.7, ideal and bursty hosts) and were sent to the owner with the investigation report.

**Suites and mutants** — §6.

## 5 — What remains, and is not this round's

- **Host delivery.** The smoothed head's `[head, head + 1]` band has no slack, so bursty delivery
  (REAPER anticipative FX on prefill, seek, loop and catch-up) makes the whole trace lurch 1.45 px,
  creep 0.48 px and stall at the burst cadence, and a delivered block shorter than the prepared one
  (Logic's live-track I/O buffer against a `MaxFramesPerSlice` of 1024) runs the trace 2–8× too fast
  over a 2.5–10 s window. Measured this round on the harness, unchanged by the fix, left by the
  owner's instruction, and filed as **OQ-017** with the two field observations that decide it.
- **The completed trace's pixel activity.** A 1.45 px-pitch zigzag drawn with a 1.4 px stroke and
  moved 0.75 px per frame rewrites about a third of its pixels every frame — content moving through
  the grid, present in both versions, proportional to speed, and not a sub-pixel artefact (the
  stroke's peak brightness is constant through the translation). Only a different drawing — a
  filled envelope, a wider stroke, fewer vertices — changes that; not this round's question.
- **Presentation-pipeline hitches** on the two GL platforms (a vblank coalesced under message-thread
  load, a render that misses its vblank) are whole-trace, load-dependent, unmeasured in the field
  and unchanged here.

## 6 — Verification record

Suites, this container, Release, GCC 13.3: **`AnabasisTests` 316 + `AnabasisStateTests` 1042 =
1358**, 0 failures (1345 before this round). Full `ninja` build of every target clean. The state
suite's changes: the walk in `testGrHistoryWindowNeverAsksForTheHeadSlot` drops the two 0.2.8 tip
pins and gains five (only complete buckets drawn; newest drawn vertex within a pitch of the edge and
on it at completion; every drawn bucket reads its own span at every head; a completing bucket
appears at the edge as a new vertex one pitch past the previous; a bucket once drawn stays drawn),
the `tipFirst` block becomes `grComplete` (the bucket-boundary arithmetic at stride 3), the
just-reset case now asserts NO bucket until the first completes, and `grPaint` gains a RENDERED
snapshot at every fill of the newest bucket asserting the last plot column carries the trace.

**Mutants** (one-site edits of the fixed tree, state suite rebuilt and run, the fixed sources
restored after each):

| Mutant | Kills (of 1042) |
|---|---|
| **drawincomplete** — `kLast = kHead` (the half-collected bucket is drawn) | 24: only-complete-drawn, newest-within-pitch, values-frozen and appears-at-edge in all six geometry cases, the quarter-full pin at 192 kHz / 32, `grComplete`'s "drawn on the entry that completes it", and the just-reset case |
| **trailingwindow** — the newest drawn bucket reads `[head − stride, head)` again | 5: exactly the values-frozen walk in every geometry case, nothing else |
| **anchorleadout** — the lead-out ends on `right` instead of the clip edge | 1: exactly the rendered last-column check — the one property no static can carry, which is why that pin is a snapshot |
| **pinnedtip** — `bucketX`'s pre-fix `k >= kHead` branch restored | **0, and that is the intended result**: under `kLast` the branch is dead code — for a DRAWN newest bucket `fill == stride` and it returns the general expression's value — so the pin sits on which buckets are drawn, not on the branch. |

Each kill set is disjoint from the others, which is what says the three live parts of the fix are
measured separately.

Gates: `check-docs`, `check-citations`, `check-portability`, `check-realtime`, `git diff --check`
— recorded in the commit message with their counts.

## 7 — 0.2.12: the owner's third report — the lead-out was on screen (2026-09-05)

### 7.1 The report, and what was verified before anything changed

The owner, on the 0.2.11 build: *"approximately 1–2 pixels of newly generated content extending
to the right as a short horizontal line … exposes the state while it is still being generated"*,
in the grey level history as well as the yellow trace; proposed remedy, not to display that
strip and to move the visible right edge only. Investigated on the same harness as §1 (real
processor, real `tick`/`paintHistory`, simulated host and frame clock, 60 Hz, 8 s per
configuration), with the outer clip the only difference between variants, and reported before
any change; the owner confirmed and directed the clip-only fix with the extra column.

**The strip is the lead-out.** `paintHistory` draws complete buckets only (`kLast`), then a flat
segment from the newest drawn vertex to `area.getRight()` — the placeholder for the bucket still
collecting — and clips at the plot area's right edge. With `fill ∈ [1, stride]` and
`phase ∈ [0, 1]`, `bucketX` puts the newest drawn vertex at `right − pitch ≤ x(kLast) ≤ right`, the
lower end only on a parked (phase 1) frame (measured 911.58–913.00 against the bound 911.55 on the
Simple well at 48 kHz / 512), so the
lead-out is 1.00–2.42 px long there (mean 1.71; 1.00–2.27, mean 1.63, on the Advanced well;
1.00–2.90 at 48 kHz / 1024; 1.00–2.05 at 44.1 kHz), the 1 px floor being the last column, which the anchor —
the column's left boundary — never lets a complete vertex cross. Once per bucket (218 appearances
in 419 frames, 31.2/s, on the Simple well; 23.5/s Advanced and 48 kHz / 1024; 43/s at
44.1 kHz / 1024) the frame after the completing block replaces the strip: the new vertex lands at
`right − phase · perEntry`, the strip's height jumps to the new value and the segment left of it —
up to a pitch wide — turns from flat to sloped. The GR stroke and the level fill share the loop,
the `lastX/lastWy/lastGy` lead-out and the clip, so both show it.

**Measured, per column, translation-compensated** (frame m against frame m − 4 shifted the whole
3 px the trace moves in four frames; the fill's top edge from the ungated column coverage sum, the
stroke from its coverage-weighted row centroid, so no colour gate is involved — the earlier gated
grey metric was flipping on anti-aliased fill-edge pixels in the interior as much as at the edge,
25–58 flips per frame in every variant, and was set aside for that reason), Simple well,
48 kHz / 512, the 0.2.11 build, columns given as their position in the older frame:

| column | 913 | 912 | 911 | 910 | 909 … |
|---|---|---|---|---|---|
| fill top edge, mean change px (max) | 6.01 (25.2) | 3.03 (25.1) | 0.53 (11.1) | 0.07 (0.5) | 0.07 |
| stroke centroid, mean change px (max) | 3.08 (17.8) | 1.14 (10.4) | 0.20 (3.9) | 0.04 (0.1) | 0.04 |

Everything left of 911 sits at the floor every content column shows (0.07 / 0.04 px: anti-aliased
content moving through the grid, the §5 item, present in all variants alike). The instability is
exactly `[right − pitch, area.getRight())` — three columns here.

### 7.2 The bound, and the extra column

Clipping at `B = floor (right − pitch)` — the first column the lead-out can reach — hides it on
every frame by the inequality above, and every segment crossing `B` joins two complete buckets. A
sweep of `B − 0 … B − 4` on the real
`paintHistory` (outer clip only, per-column profile of the columns the older frame can be compared
against) put the rightmost comparable columns at the interior floor at `B − 0` on the Simple and
Advanced wells and at 44.1 kHz / 1024 — but at 48 kHz / 1024 the boundary-adjacent column read
0.11 px mean, 3.0 px max, 5 % of frames over 0.25 px against a floor of 0.06 / 0.4 / 2 %, gone at
`B − 1`. The cause is the stroke JOIN at the vertex that was newest until the new one appeared: its
join re-shapes from "segment → horizontal lead-out" to "segment → sloped segment" in that frame,
it sits between `right − 2·pitch` and `right − pitch` — at 60 Hz up to a frame's travel inside `B`
(0.65 px there, 0.17 on the Simple well at 512), further on a slower clock — and a JUCE mitred join
reaches up to four half-widths (2.8 px) from it. `visibleRight = floor (right − pitch) − 1` is the
owner's choice and the fix: a measured margin rather than a bound (a re-shape that did reach a shown
column would be confined to the stroke's width around one vertex, never a jump in height), and
`ceil (pitch) + 2` columns hidden — four for every block up to 1024 samples at every rate from
44.1 kHz and 2048 from 48 kHz up, on either well; five at 44.1 kHz / 2048 on the Simple well.

### 7.3 The fix

`GrHistoryView::visibleRight (const Buckets&, x0, width)` in the header, beside `bucketX` and
derived from the same `right` and `pitch`, and one changed rectangle in `paintHistory`: the clip's
right edge is `visibleRight` instead of `area.getRight()`. The anchor, `bucketX`, `buckets`, the
read window, the values, the smoothed head, the tick, the host-delivery behaviour (OQ-017) and the
left edge are untouched; the lead-out stays in the path, wholly behind the clip, because removing
it is not this fix. The plot's right margin is wider by the hidden columns (14 px against 10 on the
left at the shipped rates).

### 7.4 Validation — 0.2.11 against 0.2.12 on the same frames

The harness built twice, once linked against the 0.2.11 `GrHistoryView` (`7d34450`, taken from
git) and once against the fixed one, same host schedule, same seeds, same frames; each build dumps
a per-column pixel hash and the two geometric measures for every frame.

| 48 kHz / 512, Simple well | 0.2.11 | 0.2.12 |
|---|---|---|
| fill top edge, rightmost 24 visible columns: mean / max change, columns > 1 px per frame | 0.516 / 25.2 px, 1.47 | **0.070 / 0.44 px, 0.00** |
| stroke centroid, rightmost 24 visible columns: mean / max, columns > 1 px per frame | 0.242 / 17.9 px, 0.94 | **0.036 / 0.16 px, 0.00** |
| gated stroke residual, rightmost 24 visible columns: flips per frame | 5.78 (79 % of frames) | **0.00 (0 %)** |
| interior (fill mean / max; stroke mean / max) | 0.016 / 1.81; 0.008 / 3.28 | 0.016 / 1.81; 0.008 / 3.28 — unchanged |
| left 40 columns, both measures | 0.000 | 0.000 |
| last visible column carries the trace | 419 / 419 frames (column 913) | **419 / 419 frames (column 909)** |
| columns from the boundary to the plot edge | — | **background on 419 / 419 frames** |
| columns 10 … 909 pixel-identical to 0.2.11 | — | **480 / 480 frames** |
| per-column profile at the boundary (fill mean \| max \| % > 0.25 px), rightmost comparable column and the seven left of it | 6.01\|25\|93, 3.03\|25\|73, 0.53\|11\|23, then 0.07\|0.3–0.5\|3–5 … | 0.07\|0.3–0.5\|3–5 in every column — the interior floor |

The same holds on every configuration run — Advanced well (fill 1.58 / 69 → 0.175 / 0.98 px; stroke
0.80 / 48 → 0.11 / 0.29; boundary 610), 48 kHz / 1024 (0.57 / 26 → 0.062 / 0.40; the join column
now at the floor), 44.1 kHz / 1024 (0.47 / 30 → 0.059 / 0.47) and 44.1 kHz / 512 (0.47 / 28 →
0.058 / 0.42): visible columns
identical to 0.2.11 in 480 of 480 frames, the hidden columns background on every frame, the last
visible column lit on every frame, the newest drawn vertex never nearer than 1.10 px to the last
visible column (48 kHz / 1024; 1.58 px on the Simple well at 512, 1.73 on the Advanced). Contact
sheets (rightmost 60 px, ×6, eight consecutive frames) and 2 s GIFs at 3× of both builds are in the
session's scratch directory and were sent to the owner: the 0.2.11 frames show the stub growing
and snapping, the 0.2.12 frames end on a sloped segment cut cleanly by the clip.

Three checks the review asked for beyond the compensated residual, all on the same harness:

- **The boundary column itself, frame against frame, with no translation model.** A stroke pixel
  (coverage > 0.5) in frame m is NOVEL if frame m − 1 carried no stroke within ±2 rows in that
  column or the one to its right — content that did not scroll in from its neighbour — and
  VANISHED is the mirror. Over the three columns left of the last visible one (the last one's
  right-hand neighbour is hidden in m − 1, so scroll-in there is not a change), 419 frames each:
  0.2.12 stroke novel 0, vanished 0 on the Simple well at 48 kHz / 512 and / 1024, the Advanced
  well and 44.1 kHz / 1024; 0.2.11's last three columns on the same frames: 640 / 36, 2061 / 44,
  681 / 67, 576 / 13. The fill's top-edge version of the same test fires at the same rate per
  content column at the boundary as in the interior (7 events in 3 columns against 597 across the
  interior's filling content on the Simple well; 52 against 2218 on the Advanced) — a steep
  anti-aliased edge crossing the threshold, the events repeating column by column as the feature
  scrolls, not a boundary effect.
- **Fractional UI scales.** The editor paints its children through `setTransform (scale (…))`,
  and JUCE maps the integer clip to the smallest device-pixel container. Rendered through a scaled
  `Graphics` on the Simple well at 48 kHz / 512, the rightmost device column ever lit ends, in
  component units, at 910.67 (75 %), 910.59 (85 %), 910.00 (100 %), 910.40 (125 %), 910.00 (150 %
  and 200 %) — against `right − pitch` = 911.55, so the lead-out stays at least 0.89 px clear of
  the visible range at every scale, more than the stroke's half-width. The left edge expands by
  the same mechanism (9.33 at 75 %), and it is the same rectangle edge 0.2.11 clipped at.
- **The settled window.** 26 s on the Simple well at 48 kHz / 512 — the 20 s window full and
  buckets expiring at the left edge for the last six — 0.2.11 against 0.2.12: visible columns
  identical in 1560 of 1560 frames, the hidden columns background on every frame, the last visible
  column lit on all 299 settled frames, the rightmost 24 visible columns at the floor (fill
  0.071 / 0.47 px, stroke 0.036 / 0.14), the interior and the left 40 columns identical between
  the builds to the digit (left 40: fill 0.086 / 5.07, stroke 0.041 / 2.38 in both — the expiring
  oldest bucket's segment sliding under the clip, present and unchanged).

### 7.5 Verification record

Suites, this container, Release, GCC 13.3: **`AnabasisTests` 316 + `AnabasisStateTests` 1051 =
1367**, 0 failures (1358 before this round). The state suite's changes: the walk in
`testGrHistoryWindowNeverAsksForTheHeadSlot` gains `leadOutHidden` in every geometry case (the
newest drawn vertex at or beyond `visibleRight + 1` at every head and both ends of the phase; the
bound a constant of the window equal to `cols − 1 − ceil (pitch) − 1` and past the half-width), and
`grPaint` pins the bound's arithmetic on this view and on both shipped wells (910 of 10…913, 610 of
10…613) and re-pins the rendered snapshot at every fill of the newest bucket on both halves of the
contract: the last VISIBLE column lit on every fill, and every column from `visibleRight` to the
plot edge equal to the untouched margin on every fill.

**Mutants** (one-site edits of the fixed tree, state suite rebuilt and run, the fixed sources
restored and verified identical after each):

| Mutant | Kills (of 1051) |
|---|---|
| **clipatedge** — the clip's right edge back on `area.getRight()` (0.2.11's rectangle, the bound computed and ignored) | 1: exactly the rendered "no column from `visibleRight` to the plot edge carries a pixel" check — the property no static can carry, which is why that pin is a snapshot; the last-visible-column pin still passes, as it should |
| **noextracolumn** — `floor (right − pitch)` without the `− 1` | 8: the walk's boundary pin in all six geometry cases and both `grPaint` arithmetic pins (the formula, the shipped wells' 910 / 610) |
| **overclip** — `floor (right − pitch) − 3` | 8: the same eight — the bound is pinned to its value, not merely to a side of the vertex |

The rendered kill and the arithmetic kills are disjoint, which is what says the clip and the bound
are measured separately.

Gates: `check-docs`, `check-citations`, `check-portability`, `check-realtime`, `git diff --check`
— recorded in the commit message with their counts.
