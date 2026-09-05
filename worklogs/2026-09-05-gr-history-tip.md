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

## 8 — 0.2.12 review: a large host block blanked the plot (2026-09-05)

### 8.1 The finding, and the geometry that produces it

Review finding against `src/gui/GrHistoryView.h`: when the host prepares blocks that are roughly
half the history duration or more, the forced two-bucket geometry makes one pitch span the whole
plot, `floor (right − pitch) − 1` falls at or left of the plot's left boundary, and the GR history
disappears. Verified from the code before anything was changed:

- `pitch = span / (kFull − 1)` with `span = right − x0`, and `buckets` sets
  `kFull = max (2, ceil (want / stride))` — the `max` is a floor for the division, stated in its
  own comment as reachable "(a panel around one pixel wide)".
- `kFull == 2` ⟺ `want ≤ 2` ⟺ `ceil (20 · sr / block) ≤ 2` ⟺ **`block ≥ 10 · sr`**: one host block
  carrying ten seconds of audio. At that point `pitch == span`, `right − pitch == x0`, and the
  boundary is `x0 − 1`.
- `paintHistory` builds the clip as `jmax (0, clipRight − x0)` wide, so the rectangle is not
  invalid — it is EMPTY, and every stroke and fill is clipped away. The GR|SPEC chip survives
  (it is painted outside the history's clip, deliberately), so the panel shows the chip on an
  empty ground.

Reproduced on the same real-processor harness (real `AnabasisAudioProcessor`, real
`GrHistoryView::tick`/`paintHistory`, lit-pixel count per rendered frame), against the tree as
committed in `de0be99`:

| configuration | want | kFull | pitch | visibleRight | clip width | frames blank |
|---|---|---|---|---|---|---|
| 48 kHz / 512, Simple | 1875 | 625 | 1.447 | 910 | 900 | 0 of 480 |
| 48 kHz / 512, Advanced | 1875 | 469 | 1.288 | 610 | 600 | 0 of 480 |
| 48 kHz / 48000 (1 s) | 20 | 20 | 47.53 | 864 | 854 | 0 of 720 |
| 48 kHz / 240000 (5 s) | 4 | 4 | 301.0 | 611 | 601 | 0 of 450 |
| 48 kHz / 320160 (6.67 s) | 3 | 3 | 451.5 | 460 | 450 | 0 of 510 |
| 48 kHz / 479999 (9.99998 s) | 3 | 3 | 451.5 | 460 | 450 | 0 of 480 |
| **48 kHz / 480000 (10 s), Simple** | 2 | 2 | 903.0 | **9** | **0** | **528 of 528 (100 %)** |
| **48 kHz / 960000 (20 s), Simple** | 1 | 2 | 903.0 | **9** | **0** | **525 of 525** |
| **48 kHz / 480000, Advanced** | 2 | 2 | 603.0 | **9** | **0** | **528 of 528** |
| **44.1 kHz / 441000 (10 s), Simple** | 2 | 2 | 903.0 | **9** | **0** | **528 of 528** |

The finding is real and the boundary is exactly `block ≥ 10 · sr`: a 479999-sample block at 48 kHz
still draws, a 480000-sample block draws nothing.

### 8.2 What the rule has to be

Two requirements, and the questions the review asked answered in order.

- **"What does the newest stable visible history mean when the pitch is comparable to the plot?"**
  Nothing, at `kFull == 2`. `bucketX` gives `right − pitch ≤ x(kLast) ≤ right` with `pitch == span`,
  so the newest drawn vertex sweeps the ENTIRE plot once per bucket and every column carries the
  lead-out on some frame. The stable content — the single segment from `x(kLast − 1)`, which is at
  or left of `x0` — cannot be separated from it by any frame-independent boundary.
- **"Is there always at least one complete stable bucket to show?"** There is always a complete
  bucket, but at `kFull == 2` its vertex is off the left edge and the only visible stable content
  is part of the segment leading to `x(kLast)` — whose right end is the unstable point itself.
- **"What should happen when `right − pitch − 1` cannot fit inside the plot?"** The two
  requirements are then genuinely exclusive, so one must be named as the loser. Showing the
  history wins: a blank panel reports nothing at all, and the artefact the boundary exists to hide
  is a FAST one — a 1–2 px stub replaced 23 to 43 times a second — which does not exist at a
  geometry where one bucket completes every ten seconds.
- **"Does the two-bucket geometry need a special case?"** No, and it should not have one: a
  branch on `kFull == 2` would step the boundary discontinuously at the transition. The formula
  expressed as a CAP covers it — and the cap is not a rescue clamp, because `pitch == span / 2`
  EXACTLY when `kFull == 3`, so capping the hidden strip at `span / 2` is the same bound one bucket
  further out. It engages only at `kFull == 2` and holds the boundary where a three-bucket window
  puts it.
- **A frame-dependent boundary** (clip at `x(kLast)` itself) would satisfy both requirements on
  paper and is rejected on sight: the plot's right edge would then move at bucket rate, which is
  the class of defect this whole round removes.

### 8.3 The rule, and the invariant it enforces

```
span    = width − 1                         // the anchor span, right − x0
hidden  = min (pitch, span / 2)             // pitch = span / (kFull − 1)
visibleRight = max (x0 + 1, floor (x0 + span − hidden) − 1)      // span ≥ 1
             = x0                            // span < 1: no plot, an empty clip as before
```

For every `width ≥ 2` and every `kFull ≥ 2`:

1. `x0 < visibleRight ≤ right − 1` — the clip is a valid, non-empty rectangle inside the plot.
2. If `kFull ≥ 3`: `visibleRight == floor (right − pitch) − 1`, bit-for-bit the rule as committed,
   so no shown column can carry the lead-out (`x(kLast) ≥ right − pitch` on every frame) and the
   join margin is intact.
3. If `kFull == 2`: `visibleRight == floor (right − span / 2) − 1`, the three-bucket boundary.
4. The boundary is non-increasing in the pitch, and the pitch is bounded by `span`, so the strip
   is never wider than `span / 2 + 2` columns and at least the plot's left half always survives.

(The boundary is NOT monotone in the block size, and never was: `stride` steps up at
`want == cols + 1` and `kFull` halves under it, so the pitch is a one-column sawtooth there —
48 kHz / 1062 samples gives 911 and 48 kHz / 1064 gives 910. That is 0.1.1 decimation geometry,
identical in both rules and untouched here.)

### 8.4 Validation

Harness built twice — once against `de0be99` (the rule as committed) and once against the capped
rule — same host schedule, same seeds, same frames.

**1. Large blocks no longer blank, and the clip is always valid.** All four `kFull == 2`
configurations: 0 blank frames of 528 / 525 / 528 / 528, boundary 460 (Simple) and 310 (Advanced),
clip width 450 and 300, lit pixels 1350 minimum against 0 before.

**2. At least the intended stable history remains visible.** The last visible column carries the
trace on 528 of 528, 525 of 525, 528 of 528 and 528 of 528 frames; the leftmost lit column is 10
(the plot's left edge) in every configuration.

**3. Normal configurations are untouched.** Every rendered column, over the full panel width, is
bit-identical before and after the cap on all six `kFull ≥ 3` configurations — 0 differing
column-frames of 480, 480, 720, 450, 510 and 480 frames respectively (48 kHz / 512 on both wells,
48 kHz / 48000, / 240000, / 320160 and / 479999). The 0.2.12 validation summary (§7.4's five
configurations, all metrics) re-run against the capped build is byte-identical to the committed
build's.

**4. The right-edge artefact stays fixed, everywhere the bound can hold it.** Measured directly as
"did the lead-out reach inside the visible range this frame": **0 frames** on every `kFull ≥ 3`
configuration, including the 9.99998-second block one sample below the transition. At `kFull == 2`
it reaches inside on 236 of 528 frames by 224 px mean / 442 px max (Simple) and 236 of 528 by
149 / 295 px (Advanced) — the stated cost of a two-point window, at one bucket per ten seconds.

**5. The transition is continuous.** Sweep of the real statics over 41 block sizes from 32 samples
to 120 seconds, three rates (44.1 / 48 / 96 kHz) and both wells: never blank anywhere; identical to
the committed rule at every `kFull ≥ 3`; and the boundary goes 910 → … → 611 (5 s) → 460 (6.67 s)
→ 460 (9.99 s) → 460 (10 s, cap engaged) → 460 (20 s) → 460 (120 s) — it saturates rather than
stepping.

**6. Mutants** (one site each, state suite rebuilt and run, sources restored and verified identical
after each):

| Mutant | Kills (of 1071) |
|---|---|
| **rule as first committed** — no cap and no non-empty floor (the finding itself) | **12**, including the RENDERED one: "a ten-second host block still draws the GR history"; plus every-window-non-empty, two-bucket-at-the-half-span, no-step-at-the-transition and never-wider-than-half on both wells, the narrow-plot guard and the `grBlank` boundary |
| **cap removed** — `hidden = pitch` (the non-empty floor left in, so the plot survives as one column) | 9: the two-bucket, no-step and half-span pins on both wells, and both `grBlank` boundary pins |
| **cap too tight** — `min (pitch, span / 4)` | 6: "a window of three or more buckets keeps the uncapped bound" on both wells, the two-bucket pins, the `grBlank` boundary |
| **non-empty floor removed** | 2: the narrow-plot pin |
| **narrow-plot guard removed** | 2: the "narrower than two columns clips to nothing" pin |
| **clip back on the plot edge** (`paintHistory`) | 3: both rendered "nothing beyond the boundary" pins — no arithmetic pin sees it |
| **join margin column removed** | 14: the walk's boundary pin in all six geometry cases, both `grPaint` arithmetic pins, the boundary sweep and `grBlank` |
| **two columns further in** | 16: the same, plus the half-span pins |

The kill sets separate the three parts of the rule: the cap is measured by the two-bucket pins, the
bound it caps by the "uncapped where it can" pins, and the clip that reads either by the rendered
ones.

**7. Suites and gates.** `AnabasisTests` 316 + `AnabasisStateTests` 1071 = 1387, 0 failures (1367
before this round: the boundary sweep adds six checks per well, the narrow-plot guards two, and
`testGrHistorySurvivesAHostBlockOfTenSeconds` six). Full `ninja` build clean; pluginval strictness
10, deterministic ×3 and randomise ×3, green under xvfb; `check-docs`, `check-portability`,
`check-realtime`, `check-citations` and `git diff --check` clean.

### 8.5 What is not changed, and what is left

The bucket values, `buckets`, `bucketX`, the anchor, the read window, the left edge, the frame
clock, the smoothed head, the host-delivery behaviour (OQ-017) and the spectrum view are all
untouched; `paintHistory` reads the boundary in one place and nothing else moved. The USER_MANUAL's
"a few pixels" is left as written: it is exact for every interactive configuration, and the
two-bucket geometry needs a host block of ten seconds, which is an offline-render buffer rather
than something a user watches. The two-bucket display remains coarse by nature — two points across
twenty seconds — and the honest presentation of it is a follow-up question for the owner, not a
defect of this boundary.

## 9 — 0.2.12 review, second finding: the oldest drawn bucket changed value (2026-09-05)

### 9.1 The finding, read off the code and then measured

Review finding against `src/gui/GrHistoryView.h`: when a full window scrolls, `bucketReads`
truncates the oldest displayed bucket as entries expire, so its value can change before it leaves
the visible history, re-shaping the segment that crosses the left edge.

Read off the code first. `buckets` computes `window` as a LENGTH — `kFull` whole buckets — and set
`first = head − window`. That index is a bucket boundary only when `head` is a multiple of
`stride`; on every other head it falls INSIDE the oldest drawn bucket, and `bucketReads` clamps
that bucket's range to it:

```
bucketReads (b, k, first, head) = { max (first, k · stride), min (head, (k + 1) · stride) }
```

So bucket `kFirst` lost its earliest entries one at a time as the head advanced — up to `stride − 1`
of them — and the value it yields, a MIN over its span, changed with them. Every other drawn bucket
was unaffected: `first ≤ k · stride` for `k > kFirst`, and the new-end clamp went idle at 0.2.11.
With `stride == 1` there is nothing to truncate, which is the control the measurements use.

Measured on the real-processor harness (real `tick`/`paintHistory`; every frame, every drawn
bucket's read span and the value it yields, compared with the same bucket's value in the previous
frame):

| configuration | stride | value changes | frames | max change | where the changing bucket sat |
|---|---|---|---|---|---|
| 48 kHz / 512, Simple | 3 | **340** | 18.9 % of 1800 | 1.53 dB = 5.9 px | x 8.6 … 9.5 (plot starts at 10) |
| 48 kHz / 512, Advanced | 4 | **370** | 20.6 % | 1.57 dB = 15.5 px | x 8.7 … 9.7 |
| 48 kHz / 1024, Simple | 2 | **197** | 10.9 % | 1.57 dB = 6.0 px | x 8.3 … 9.0 |
| 44.1 kHz / 512, Simple | 2 | **264** | 14.7 % | 0.86 dB = 3.3 px | x 9.0 … 9.5 |
| 48 kHz / 128, Simple (window at the ring clamp) | 5 | **278** | 29.0 % | 0.65 dB = 2.5 px | x 9.0 … 9.8 |
| 48 kHz / 2048, Simple — the control | 1 | **0** | — | — | — |

**Every one of the 1449 changes was on the oldest drawn bucket**, in 3.9 million drawn-bucket
readings across the six configurations — the answer to "does the same ownership issue affect any
other displayed point": it does not. The changing bucket's vertex sits just LEFT of the plot
(x 8.3…9.8 against a left edge of 10), so what a viewer sees is not the vertex but the segment from
it to its neighbour, re-sloping inside the sliver that crosses the edge. Rendered and measured
translation-compensated over the leftmost eight columns: fill top edge 0.188 px mean / 5.81 px max,
a column moving more than a pixel on 20 % of frames (Simple), 0.594 / 18.51 / 48 % (Advanced) —
against an interior control of 0.069 / 0.51 / 0 %.

### 9.2 The invariant, and the rule that enforces it

> A displayed bucket aggregates its COMPLETE span for its whole visible life. It may leave the
> display because it has scrolled out, or because the producer has lapped the ring out from under
> it; it may never be re-drawn from part of itself.

Two edits enforce it, one for each way the span could be cut:

- **The window's start is aligned to the oldest drawn bucket's own first entry** — `buckets` keeps
  `kFirst` exactly as it was (the same bucket, the same x, the same crossing segment) and sets
  `first = kFirst · stride` instead of `head − window`. The expiring-end clamp in `bucketReads` is
  then idle for every drawn bucket. `kFull` is capped at `(kSize − stride) / stride` so those extra
  `stride − 1` entries fit inside the ring's one safe lap; the cap binds only where `want` is at
  `windowEntries`' own clamp (blocks of about 234 samples or fewer at 48 kHz) and costs that window
  one bucket of its twenty seconds and 0.2 % of its pitch. The cap is on `kFull` rather than on the
  window length because the two must agree: a window shorter than `kFull` buckets would put the
  oldest drawn vertex a pitch inside the left edge with the flat lead-in behind it, which is the
  bucket-rate walk 0.2.8 removed (the 192 kHz / 32 case fails `oldestOffEdge` if the length alone
  is capped — measured while getting this wrong).
- **A bucket the producer has lapped into is dropped, not truncated** — `firstDrawn (b, floor)`
  answers which bucket a frame may start at once `readFloor` is taken into account, and
  `paintHistory` reads from that bucket's own first entry. Reachable only on a saturated window
  with a stale head, which is the case `readFloor` exists for.

What is NOT changed: bucket identity and values, `bucketX`, the anchor, `kFirst`, the pitch at
every ordinary window, `visibleRight` and the right-edge clip, the smoothed head, the frame clock,
host-delivery behaviour (OQ-017) and the spectrum view.

### 9.3 Validation — the fixed tree against `b678c2b` on identical frames

| | before | after |
|---|---|---|
| drawn-bucket value changes, all six configurations | 340 / 370 / 197 / 264 / 278 / 0 | **0 / 0 / 0 / 0 / 0 / 0** |
| left-8 columns, fill top edge (mean / max / frames with a column > 1 px), 48 kHz / 512 Simple | 0.188 / 5.81 / 20 % | **0.090 / 2.35 / 4 %** |
| …Advanced | 0.594 / 18.51 / 48 % | **0.266 / 12.42 / 15 %** |
| …48 kHz / 1024 | 0.156 / 7.72 / 14 % | **0.080 / 2.49 / 2 %** |
| …44.1 kHz / 512 | 0.142 / 4.12 / 17 % | **0.091 / 2.86 / 6 %** |
| stride-1 control, Simple (cannot have the defect) | 0.080 / 2.23 / 2 % | 0.080 / 2.23 / 2 % — unchanged |
| stride-1 control, Advanced (cannot have the defect) | 0.259 / 9.93 / 15 % | 0.259 / 9.93 / 15 % — unchanged |

The two controls are what says the remaining left-edge motion is not the defect: a geometry whose
buckets hold ONE entry each cannot truncate anything, and the fixed configurations land on its
numbers. What is left there is a bucket leaving the display and the stroke being cut by the clip —
the honest events, unchanged by this round and identical in both builds.

**The right edge and the large-block protection are untouched, measured rather than argued.** The
0.2.12 validation harness (§7.4's five configurations, every metric) re-runs **byte-identical** to
`b678c2b`. The 26-second settled run differs in exactly three lines, all of them the left-40-column
measurements and all of them improved (fill top edge 0.086 / 5.07 px on 22 % of frames → 0.073 /
2.35 on 4 %); the novelty, fractional-scale, right-edge and hidden-column measurements are
byte-identical. The large-host-block harness is identical row for row — every `want ≤ 4`
configuration keeps its boundary (460 Simple, 310 Advanced), its clip width and its 0 blank frames;
the only differences anywhere in that sweep are the saturated rows, where `kFull` moves 819 → 818
(Simple) and 585 → 584 (Advanced) and the pitch by 0.001 px, leaving `visibleRight` unchanged.

### 9.4 Tests and mutants

Six checks added, four changed (state suite 1071 → 1078):

- `testTheOldestDrawnBucketKeepsItsValueUntilItLeaves` — a REAL `GrHistoryBuffer` walked past 400
  heads with a pattern whose minimum sits on the FIRST entry of every bucket, so dropping one entry
  moves a bucket's value by 11 dB. Asserts that no bucket ever changes value while drawn, that the
  window starts on a bucket boundary at every head, and that every drawn bucket reads its complete
  span.
- The walk's frozen-values pin now runs from `kFirst` rather than `kFirst + 1` — the oldest drawn
  bucket is exactly what it used to exclude.
- The window law (`m3`) pins `window == kFull · stride`, `first == kFirst · stride`,
  `first ≤ head − window` and `head − first ≤ kSize − 1`.
- The race test drives the paint's own caller (`firstDrawn`), pins that every bucket a stale frame
  draws reads its complete span, and adds the lapped case: with the producer far enough ahead, the
  oldest bucket is DROPPED (`kFD == kFirst + 1`) and everything still drawn reads its whole span.

| Mutant | Kills (of 1078) |
|---|---|
| **window start unaligned** — `first = max (0, head − window)`, the rule as found | **13**: the walk's frozen-values pin in all five multi-entry geometry cases, `grFrozen`'s alignment pin, and seven race pins |
| **ring cap removed** — `kFull` uncapped | 4: the floor binds where it should not, and the lapped-bucket pins |
| **lapped bucket kept** — `firstDrawn` always returns `kFirst` | 5: the floor and write-slot pins, and the drop pin |
| **floor division** — `firstDrawn` rounds the floor DOWN to a bucket | 5: the same — a bucket that starts below the floor is drawn |
| **paint loop starts at `kFirst`** | **0 — equivalent.** With `first` at the drawn bucket's start, the lapped bucket's range comes out empty and the loop's existing `e0 >= e1` guard skips it. |
| **paint reads from `max (nb.first, readFloor)`** | **0 — equivalent.** With the window's start aligned and the loop starting at `firstDrawn`, both expressions clamp to the same complete spans. |

The two equivalent mutants are the two halves of the fix overlapping: either alone would hold the
invariant in the case it covers, and the pair states the intent at both ends. They are recorded as
equivalent rather than presented as kills.

### 9.5 Verification record

Suites, this container, Release, GCC 13.3: **`AnabasisTests` 316 + `AnabasisStateTests` 1078 =
1394**, 0 failures (1387 before this round). Full `ninja` build of every target clean; pluginval
strictness 10, deterministic ×3 and randomise ×3, green under xvfb; `check-docs`,
`check-portability`, `check-realtime`, `check-citations` and `git diff --check` clean.

The USER_MANUAL's "Each point of the trace is drawn once … and is never redrawn" was not true of the
oldest point before this round. It is now true without exception, so the sentence stands as written.

### 9.6 What is left

- The left edge still shows a bucket LEAVING the display (a different vertex anchors the crossing
  segment) and the stroke being cut by the clip. Both are inherent to a scrolling, clipped plot,
  both are identical in the stride-1 controls, and neither is a value changing after it was drawn.
- The saturated window (blocks of about 234 samples or fewer at 48 kHz) holds one bucket fewer than
  before. Nothing else moved there: same boundary, same pitch to a thousandth of a pixel.
- OQ-017 (host delivery) is untouched, as instructed.
