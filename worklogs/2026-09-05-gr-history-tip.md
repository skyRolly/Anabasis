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

## 10 — 0.2.12 layout: the graph moved right so the panel keeps its full width (2026-09-05)

### 10.1 The geometry, restated and checked against the code

The owner's call: the right-edge boundary was taking its columns out of the PANEL, so the GR
history showed four pixels less than the spectrum view of the same well. The graph is to be moved
right by that amount instead — the boundary staying where it is in DATA terms, the freed columns on
the left filled with earlier history, nothing stretched.

Verified against the code before changing anything:

| | before | after |
|---|---|---|
| plot area, both views (`reduced (10, 8)` on identical bounds) | x ∈ [10, 914) = 904 columns [604] | unchanged |
| GR anchor `right` / pitch | 913 / 1.4471 px [613 / 1.2885] | 917 / 1.4471 [617 / 1.2885] |
| GR boundary (`visibleRight`) | 910 [610] | 914 [614] — the plot's own right edge |
| GR trace, painted columns (measured) | 10 … 909 = **900** [10 … 609 = 600] | 10 … 913 = **904** [10 … 613 = 604] |
| spectrum curve, painted columns (measured) | 10 … 914 = 905 [10 … 614 = 605] | unchanged |

`PluginEditor` gives both views the SAME rectangle in both modes (`strip`, `well`) and both inset it
by `reduced (10, 8)`, so the plot area is shared; the spectrum maps its curve onto
`area.getX() + t · area.getWidth()`, t ∈ [0, 1].

### 10.2 The change

- `paintHistory` draws in a FRAME whose origin is the plot area's plus `hiddenColumns` — the columns
  the boundary hides — and whose WIDTH is the plot area's own. Same width ⇒ same pitch ⇒ the trace
  is translated, not scaled; `bucketX` is linear in its `x0`, so every vertex moves by exactly the
  shift and nothing else about it changes.
- The boundary is read in that frame (`visibleRight (nb, ox, width)`), which by the definition of
  the shift is `area.getRight()`: the strip it hides now lies outside the plot instead of inside it.
- `buckets` covers `leadBuckets = ceil (hidden / pitch)` more buckets — 3 on the Simple well, 4 on
  the Advanced — so the trace still crosses the panel's left edge and the freed columns carry
  EARLIER history at the same pitch. The window holds `lead · stride` entries more than the nominal
  twenty seconds: 9 entries (96 ms) Simple, 16 (168 ms) Advanced.
- `kFull` — the pitch divisor, and so the pitch itself — is untouched; its ring cap now reserves the
  four buckets `leadBuckets` can ask for (`kMaxLead`, which the arithmetic bounds: `hidden` is at
  most `pitch + 3` and `pitch ≥ 1` for every geometry `buckets` produces).

### 10.3 Validation, against `fea3740` on identical frames

**The render is a pure translation.** Per-column pixel hashes, settled 26-second runs, comparing
each column against the previous build's column four to its left:

| configuration | columns compared | identical | differing |
|---|---|---|---|
| 48 kHz / 512 Simple | [14, 914) × 1560 frames | 1 401 969 | 2 031 (0.14 %) |
| 48 kHz / 512 Advanced | [14, 614) × 1560 | 932 479 | 3 521 |
| 48 kHz / 1024 Simple | [14, 914) × 1560 | 1 402 033 | 1 967 |
| 44.1 kHz / 512 Simple | [14, 914) × 1560 | 1 401 795 | 2 205 |
| 48 kHz / 512, still filling | [14, 914) × 480 | 431 516 | 484 |
| 48 kHz / 480000 (two-bucket) | [464, 914) × 528, shift 454 px | 237 065 | 535 |
| …Advanced | [314, 614) × 528, shift 304 px | 157 864 | 536 |

Geometrically — the same comparison on the per-column fill height and stroke centroid rather than
on pixel identity — the difference from a pure four-pixel translation is **0.00004 px mean**
(Simple) and 0.00026 px (Advanced), and **every** difference above half a pixel is at column 14, on
32 and 73 frames of 1560: that column was the previous build's clipped left END of the trace and is
now an interior column with the trace continuing past it, which is exactly the point of the change.
The rest are sub-pixel anti-aliasing (233 and 547 column-samples between 0.01 and 0.5 px, of 1.4 M
and 0.94 M).

**Nothing previously hidden became visible.** The trace's painted columns end at 913 [613], the
plot's last column; nothing is drawn at or beyond `area.getRight()` on any frame, in any
configuration, and the content of every visible column is the previous build's content from four
columns to its left — all of which was already inside the previous build's own boundary.

**The other invariants hold.** Drawn-bucket value changes: **0** in all seven configurations of the
§9 harness. Right-edge stability (translation-compensated residual over the rightmost 24 visible
columns, five configurations): **byte-identical** to the pre-shift run. Large host blocks: 0 blank
frames, the trace now filling the panel there too (painted columns 10…913 at 48 kHz / 480000).
Left-edge residual, which the shift could only have worsened, is slightly BETTER in every
configuration — the leftmost visible columns are interior to the trace now rather than its clipped
end: fill top edge 0.090 → 0.073 px mean, max 2.35 → 1.51 (Simple); 0.266 → 0.178, max 12.42 → 3.88
(Advanced); the stride-1 controls 0.080 → 0.061 and 0.259 → 0.176.

### 10.4 Tests and mutants

Six checks added or restated (state suite 1078 → 1085): the settled-window and walk pins now read
the geometry in the DRAWING frame and assert the oldest drawn vertex is at or beyond the PANEL's
left edge with the trace covering the panel's whole width; a new pin asserts the frame is a
TRANSLATION (one pixel of origin moves a vertex exactly one pixel, at an unchanged pitch); the
window law pins `window == (kFull + lead) · stride`; and the two rendered pins now assert the trace
reaches the plot's LAST column at every fill and that nothing is drawn beyond the plot.

| Mutant | Kills (of 1085) |
|---|---|
| **no shift** — the frame back on the panel's origin | 2: the trace no longer reaches the plot's last column |
| **no lead buckets** — `cover = kFull` | 22: every settled-window, window-law and oldest-vertex pin, plus three race pins |
| **one lead bucket short** | 22: the same — the coverage is pinned exactly, not loosely |
| **shift one pixel too far** | 3: content beyond the plot's right edge, in both rendered pins |
| **clip read in the panel's frame** | 2: the trace no longer reaches the plot's last column |

### 10.5 What is left

The spectrum's curve is mapped onto `[x0, x0 + width]` and its 1.3 px stroke therefore lights one
column BEYOND the plot area's last (914 [614], the first margin column), where the GR history fills
the area exactly (10…913 [10…613]). The two graphs now cover the same 904 [604] plot columns; that
one column of spectrum stroke spilling onto the margin is the spectrum's own endpoint convention,
predates this change, and was left alone — the spectrum view was not to be touched.

## 11 — the owner's view-switch report: one stale frame on the way back (2026-09-05)

### 11.1 The lifecycle, read off the code

`GrHistoryView::visibilityChanged` starts the frame clock when the view is shown and STOPS it when
it is hidden, and `tick` is the only thing that publishes `shownHead` / `smoothHead` /
`publishedEpoch` — the pair (plus epoch) that `paintHead` and `frameFor` turn into the head and
phase a frame draws. `paintHead` draws the TICK's head deliberately (0.2.8: the frame's phase must
belong to the head it is the phase of), and it accepts that head whenever
`shownHead > 0 && shownHead <= live` — a stale head satisfies both. The ring does not stop while the
view is hidden: the audio thread fills it throughout, so the pair goes stale by one entry per block.

`bucketX` places every vertex by `head + phase`, so a head `n` entries behind the ring puts the
whole trace — the GR stroke and the level fill under it, one path — `n · perEntry` px to the RIGHT.

`FrameClock::start` only attaches a `VBlankAttachment`; it does not call the callback. JUCE repaints
a component the moment it becomes visible. So on the way back there are two message-thread events —
the repaint and the clock's first callback — and nothing orders them.

### 11.2 Reproduced, both orderings, on the real paint path

Harness: real processor, real `GrHistoryView`, driven through the same lifecycle calls the editor
makes (`setVisible`), with both orderings modelled explicitly. Per transition it records the head
and phase the frame actually draws, the live head, and the offset measured from the RENDER by
aligning the first visible frame against the next one.

| hidden for | first visible frame draws | live head | entries behind | offset | rendered alignment |
|---|---|---|---|---|---|
| 50 ms | head 749, phase 0.438 | 754 | 5 | 2.20 px right | −2 px |
| 100 ms | head 1161, phase 0.938 | 1171 | 10 | 4.37 px | −4 px |
| 250 ms | head 1615 | 1640 | 25 | 12.03 px | −12 px |
| 500 ms | head 2190 | 2238 | 48 | 23.12 px | −23 px |
| 1 s | head 2952, phase 0.500 | 3047 | 95 | 45.58 px | −46 px |
| 2 s | head 4090 | 4279 | 189 | 91.17 px | −91 px |

The rendered alignment matches the analytic offset to the pixel at every duration, and
`perEntry · entries` reproduces it (0.4824 × 189 = 91.2). With the clock's callback landing FIRST
instead, the same transitions draw the live head every time — which is exactly why the owner saw it
only sometimes.

**Rate over the full set** (248 transitions per configuration: 100 with no gap at all, 100 one frame
apart with no recovery time, then 8 each at 50 ms … 2 s):

| | repaint first | clock first |
|---|---|---|
| before | **148 / 248 (60 %)**, worst 91.7 px, mean 10.2 px | 0 / 248 |
| after | **0 / 248** | 0 / 248 |

The 100 zero-gap toggles cannot be stale — no entry arrives in a zero-length gap — which is why the
before-rate is 60 % and not 100 %: the defect needs the repaint to win AND at least one block to
have landed, both of which a real switch satisfies.

### 11.3 The root cause, and what it is not

The first visible frame was drawing the pre-switch state, exactly as the owner hypothesised. It is
not a geometry, anchor or clip problem: `visibleRight`, the drawing frame, the buckets, their values
and the read window are all identical between the stale frame and the correct one — the ONLY
difference is the head-and-phase pair the frame is placed by. The right-edge validation table
re-runs byte-identical after the fix, which is the same statement from the other side.

### 11.4 The fix

`visibilityChanged` re-derives before anything can paint: a tick with **no elapsed time**, which
`smoothedHead`'s `[head, head + 1]` clamp resolves to the live head at phase 0 — the same re-anchor
it performs for a rewound head — published before `clock.start` and therefore before any repaint can
read it.

It is the publication the view always makes, on the thread that always makes it; no delay,
no skipped frame, no drawing change. The parked and cleared cases are unaffected: a head that did
not move while hidden AND had reached the cap leaves `parked` true and publishes nothing (the pair
is already current), and a ring cleared while hidden takes the `cleared` branch and anchors at
phase 0 under the new epoch.

> **Corrected in §12 (the review's finding under this fix).** Two sentences above need it. "Resolves
> to the live head at phase 0" holds only when the producer ADVANCED while the view was hidden,
> which is what makes the clamp's LOWER bound bite; with the head unmoved the clamp does nothing at
> all and a zero-second tick republishes the retained phase verbatim — `tick (0.0)` is a clamp of the
> old value into the new head's window, not a re-anchor. And "a head that did not move leaves
> `parked` true" is only half a case: `parked` also requires the smoothed head to have reached
> `head + 1`, so a switch that catches the ramp mid-flight is not parked and does republish the stale
> phase. §12 replaces the zero with the seconds the clock was actually stopped, which resolves both.

`GrHistoryView::tick` moved from private to public — the only other change — because the regression
test has to publish a pre-hide state to create the condition at all, which is the reason
`SpectrumView::tick` is public too ("a direction nothing can call is a direction nothing can
guard").

### 11.5 Validation

- 248 transitions × 3 configurations after the fix: **0 stale frames**, worst 0.0 px, in BOTH
  orderings and on both wells; including 200 per configuration with no recovery time between them.
- The first visible frame's head equals the live head at every hidden duration from 0 ms to 2 s, and
  the frame after it is 0.00–0.48 px away — one frame of ordinary scroll, no corrective jump.
- The right-edge / rigid-translation validation table (§7.4's five configurations, every metric)
  re-runs **byte-identical**: the drawing path is untouched.
- Suites 316 + 1089 = **1405**, 0 failures. `testTheGrHistoryIsCurrentTheFrameItBecomesVisible`
  renders three frames — before the spectrum takes the well, the instant it comes back, and one from
  a view that has never been stale — and holds the returning frame to the last rather than the
  first. Removing the re-derivation makes it fail with the defect verbatim: *"the first visible frame
  is NOT the one from before the spectrum took over"*.

### 11.6 What is left

> **Superseded by §12 (2026-09-06).** The follow-up below was taken up in the same review round:
> `SpectrumView` was investigated on its own terms — it is not the same defect — and fixed.

`SpectrumView` has the same lifecycle (`visibilityChanged` starts and stops its clock) and therefore
the same class of staleness on ITS first visible frame: it would draw the spectrum as it was when
the GR history took the well. It is far less visible — a spectrum's shape at 60 Hz differs little
frame to frame, and nothing about it is placed by a scrolling index, so there is no jump — and the
owner's instruction was to leave the spectrum alone unless the synchronisation strictly required it,
which it does not. Recorded here as the obvious follow-up rather than changed.

---

## 12 — the review's finding under §11, and the spectrum's half of the same lifecycle (2026-09-06)

Two things were asked for together: the finding that §11's fix is incomplete when the producer makes
no progress while the view is hidden, and an independent investigation of `SpectrumView`, which §11.6
had recorded as a follow-up. They turn out to be one missing measurement applied to two different
state models, so they are recorded together — but the models were read separately, and the second is
NOT the first by analogy.

### 12.1 The GR finding, read off the arithmetic

`tick (0.0)` reaches `smoothedHead (previous, head, 0.0, T, cleared)`, which is

```
smoothHead' = cleared ? head : (previous > head + 1 ? head : clamp (previous, head, head + 1))
```

so with `dt == 0` the advance term is exactly 0 and the call is a CLAMP of the retained value into
the new head's window, not a re-anchor. It re-anchors only where the clamp bites:

| what happened while hidden | `previous` vs the new `head` | what `tick (0.0)` published |
|---|---|---|
| many entries arrived | `previous ≤ head₀ + 1 < head` | the lower bound binds → the live head, phase 0 ✅ |
| exactly one entry arrived | `previous ≤ head₀ + 1 = head` | the lower bound still binds (equality) → phase 0 ✅ |
| nothing arrived, ramp mid-flight | `head ≤ previous < head + 1` | **neither bound binds → the pre-switch phase, verbatim** ❌ |
| nothing arrived, ramp already parked | `previous == head + 1` | `parked` returns first: nothing published (already correct) ✅ |
| the ring was cleared | — | `cleared` → phase 0 under the new epoch ✅ |

So the defect surface is exactly the zero-progress case, as the review says, and one entry of
producer progress cures it. What it costs: the phase is a fraction of an ENTRY, so the first visible
frame is out by up to one entry-pitch — 0.48 px on the Simple well and 0.32 px on the Advanced at
48 kHz / 512, 0.22 px at 96 kHz / 128, and 4.2 px at 44.1 kHz / 4096 — and the frame clock then
finishes the expired ramp over the following frames, which is the "obsolete motion" of the finding.

### 12.2 The spectrum, read independently

`SpectrumView` is **not** structurally analogous. It has no head, no phase and no position of any
kind; its retained display state is the two per-bin EMAs (`inDb`, `outDb`), which `paint` reads
directly, plus the four "have I seen this?" counters. Its only wall-clock coupling is
`decay = 1 − exp (−dt / 0.12)` inside `analyse`. What it shares with the GR view is the LIFECYCLE —
`visibilityChanged` starts and stops the clock — and the consequences of that:

* the two `ScopeBuffer` rings keep filling while the view is hidden (`AnabasisEngine::processChunk`
  pushes unconditionally; there is no visibility term anywhere on the audio path), so the retained
  analysis ages;
* `Component::setVisible (true)` marks the component dirty BEFORE it sends the visibility change, and
  the paint runs later on the message loop — so a reveal that publishes nothing hands the first
  visible frame whatever the view was holding;
* `FrameClock::start` resets its pacing on purpose, so the first callback after a restart carries a
  neutral 1/60 s, never the hidden interval. At that dt the decay is 0.13: **87 % of every bin whose
  true level had FALLEN survives into the first analysed frame**, and the trace reads high for a
  further ~0.12 s of watching however long the switch was. Bins that ROSE are corrected in the same
  frame by the instant-attack branch, so the staleness is one-sided.

Two further consequences, neither of them the GR defect:

* **no obsolete motion is possible here** — there is no ramp to replay;
* **a re-prepare during the switch** put one frame of the previous configuration's analysis on screen
  through the new rate's bin mapping, because the reset floors live in `tick` and `tick` could not
  run before the visibility repaint. That is the artefact the ring rewind exists to remove.

And one place where doing nothing is already right: with **no** new frames in either ring the idle
gate at the top of `tick` returns before `analyse`, so a hidden-then-shown view holds exactly the
trace it had — which is what a VISIBLE analyser does with an idle ring (`KNOWN_ISSUES` KI-007
item 6, deliberately unchanged). A reveal that floored or re-analysed unconditionally would answer
that open question by accident.

### 12.3 Reproduced on the real processor and the real paint paths

`life.cpp` runs BOTH views on one processor for every scenario: a CONTROL that stays visible and is
ticked throughout, and a TEST that is hidden for the interval and then shown. The hidden interval is
spent in REAL time, because the production code measures it from the wall clock, with the audio
pushed on the same real timeline — or not pushed at all, which is the review's case. The oracle is
therefore the invariant itself: **the first frame after a reveal should be the frame a view that was
never hidden would be showing at that instant.**

Sweep: hidden ∈ {0, 17, 50, 250, 1000} ms × audio {arriving, stopped} × pre-hide phase ∈ {0, 0.25,
0.5, 0.75, 0.99} × 3 repeats × 3 configurations — 450 GR transitions — and for the spectrum
hidden ∈ {0, 17, 100, 500, 2000} ms × audio {arriving, stopped} × 3 repeats × 2 configurations,
with the programme changed at the moment of hiding so stale state is distinguishable.

**GR, transport stopped while hidden** (the review's case; 60 transitions per configuration):

| configuration | first frame ≠ a never-hidden view's | worst | motion after the reveal that the control does not have |
|---|---|---|---|
| Simple 924×108, 48 kHz / 512 | **48 → 0** | 0.482 px → **0.000** | 48 → **0** |
| Advanced 624×254, 48 kHz / 512 | **48 → 0** | 0.322 px → **0.000** | 48 → **0** |
| Simple, 96 kHz / 128 | **48 → 0** | 0.222 px → **0.000** | 48 → **0** |

(The 12 per configuration that never differed are the φ = 0.99 rows — already at the cap, the case
`parked` was covering.)

**GR, audio arriving while hidden** (60 per configuration): the head was already correct after §11;
what remains is the sub-entry phase, and it improves but does not vanish — 60 → 45 differing on the
Simple well, worst 0.392 → 0.260 px; 60 → 45 Advanced, 0.260 → 0.177 px; 45 → 30 at 96 kHz / 128,
0.222 → 0.203 px. §12.6 says why that residue is irreducible.

**Spectrum** (mean per-bin distance over in and out, 12 transitions per configuration, hidden > 0):

| measured against | before | after |
|---|---|---|
| the PRE-HIDE analysis it must no longer be | **0.000 dB — it *was* that frame** | 3.880 dB |
| the CURRENT raw analysis of the rings | 5.110 dB | **1.231 dB** |
| a never-hidden control | 2.727 dB | 3.502 dB (the control's own distance from the current analysis is 4.72 — see §12.6) |
| audio stopped while hidden, all three of the above | 0.000 dB | **0.000 dB** (nothing invented) |

Per hidden interval, distance from the current analysis: 4.11 → 3.16 dB at 17 ms, 5.21 → **1.67** at
100 ms, 5.66 → **0.07** at 500 ms, 5.49 → **0.000** at two seconds (`decay` saturates to exactly
1.0f in float beyond ~2.1 s, so the reveal is a bit-exact re-anchor there).

**Re-prepare while hidden:** the first visible state moved **0.000 dB** from the pre-reset analysis
with **0 of 2048** bins floored before; **116.8 dB** and **2048 of 2048** after.

### 12.4 The fix: one measurement, two state models

`abgui::HiddenInterval` (new, `src/gui/HiddenInterval.h`) is the whole of the shared mechanism: a
wall-clock stamp taken beside `clock.stop()`, and the seconds since it, handed to the view's own tick
beside `clock.start()`. It is deliberately not in `FrameClock` — the clock is a PACING device whose
restart semantics ("a neutral 1/60 s rather than the whole hidden interval") are correct as they
stand, it is a verbatim Anamorph copy under ADR-0009, and it is shared with `LoudnessMeterView`,
which is never hidden.

What the seconds MEAN is each view's own business, and that is the part that is not shared:

* **GR** — they go into the ramp `smoothedHead` already runs, and its existing clamp resolves every
  case: the producer advanced ⇒ the lower bound gives the live head carrying the offset the ramp
  really has by now; nothing arrived and the gap outlasts the remaining ramp ⇒ the upper bound parks
  the trace one entry on, which is where a never-hidden view already sits; the gap is shorter than
  the remaining ramp ⇒ it lands part way along, exactly as an unhidden view's would; already parked
  ⇒ `parked` publishes nothing; cleared ⇒ the `cleared` re-anchor, gap or no gap. No new branch.
* **SPECTRUM** — they go into `decay`, so the reveal folds the current window in with the decay the
  elapsed time earns: a switch of half a second or more re-anchors the trace outright, a brief one
  keeps exactly the peaks a never-hidden view would still be holding. The idle gate keeps the
  no-audio case an exact no-op, and the reset floors now run before the first visible frame.

Rejected alternatives, for the record. **Re-anchoring at phase 0 on every reveal** moves the GR trace
RIGHT by the offset it discards, which is the one direction `bucketX` guarantees no vertex ever moves
(`testTogglingTheGraphWellNeverMovesTheGrHistoryBackwards` fails on that mutant). **Saturating to the
parked value** is right only because the flip is driven by a 24 Hz timer, so it would be wrong for
any caller whose gap is genuinely shorter than an entry. **Flooring the spectrum on every reveal**
would answer KI-007 item 6 by accident and would re-attack on every rapid toggle.

### 12.5 Tests, and the one place the suite sleeps

Eighteen checks, seven of them new pins:

* `testTheGrHistoryDoesNotResumeAnExpiredRamp` — mid-ramp before the hide, no producer progress, a
  40 ms gap: the reveal must publish the parked value, and nothing may move over the next thirty
  frames. **This is the one test in either suite that depends on real elapsed time**, and it is
  written so that only a LOWER bound matters: `sleep_for` blocks for at least the requested duration,
  40 ms is 3.75 entry periods at 48 kHz / 512, and a longer sleep only saturates the same clamp
  harder. The quantity under test is real elapsed seconds and neither the ring (which stops with the
  transport) nor the frame clock (whose pacing state is reset on restart) can report it.
* `testTogglingTheGraphWellNeverMovesTheGrHistoryBackwards` — fifty switches, no audio: `head + phase`
  may only grow, and the phase must stay inside the one-entry band.
* `testTheSpectrumIsCurrentTheFrameItBecomesVisible` — a falling bin must have decayed by the seconds
  the view was away, not by one frame's worth (the same 40 ms bound).
* `testTheSpectrumHoldsItsTraceWhenNothingArrivedWhileHidden` — and must be bit-identical when nothing
  arrived.
* `testARePrepareWhileHiddenDoesNotReachTheFirstVisibleSpectrumFrame` — a reconfiguration during the
  switch reaches the first visible frame as the floor, not as the previous rate's analysis.
* `testTheHiddenIntervalMeasuresWhatItClaims` — never-stopped reads 0, ms→s, the stamp is consumed,
  a backwards clock reads 0.
* `testTheGrHistoryIsCurrentTheFrameItBecomesVisible` (§11's) is **restated** rather than extended: it
  now asserts the head on the published pair (`GrHistoryView::drawnFrame`, new and public for the
  reason `tick` is) and drives its reference view to the same phase before comparing pixels, so it
  pins "nothing survives the hide" without depending on how long the hide happened to take. Left as
  it was, it would have become timing-dependent: the reference publishes phase 0, and a gap above
  1.29 s would have parked the view under test at phase 1.

Mutation testing, nine mutants, each rebuilt and run against the whole suite:

| mutant | killed by |
|---|---|
| the GR reveal publishes a zero-second tick again | `grRamp` (both checks) |
| the GR reveal publishes nothing (pre-§11) | `grSwitch` (all three) + `grRamp` |
| the hide never stamps, so every gap reads 0 | `grRamp` (both) |
| the GR reveal re-anchors the phase at 0 | `grToggle` |
| the spectrum reveal analyses with no elapsed time | `specSwitch` |
| the spectrum reveal publishes nothing | `specSwitch` + `specPrepare` |
| the gap is not clamped at zero | `hiddenGap` |
| the stamp is not consumed | `hiddenGap` |
| the reveal happens AFTER `clock.start` | **NOT killed** — see §12.6 |

### 12.6 What is left, honestly

1. **The sub-entry phase after a hide during which the producer advanced cannot be reconstructed
   exactly.** A never-hidden view integrates its ramp in ~60 steps, each clamped to `[head, head + 1]`,
   and the clamp is lossy: where the host's real cadence differs from the nominal one the estimate
   repeatedly loses the excess. A single-step reconstruction over the whole gap keeps it. The
   difference is bounded by one entry-pitch — measured ≤ 0.26 px on the Simple well and ≤ 0.18 px on
   the Advanced at 48 kHz / 512 — it is the tolerance `smoothedHead` already advertises, and it never
   moves a vertex rightward. The head is exact in every case.
2. **The gap under-counts by up to one frame period.** It is measured from `clock.stop()`, and the
   last tick that folded state ran up to ~16.7 ms before that. The error is bounded, one-sided
   (it can only keep MORE of the old state, never less) and disappears into the same clamp.
3. **Ordering is not testable headlessly.** The reveal must publish before `clock.start` so that the
   race §11 measured cannot re-open; with no message loop and no vblank in the suite, swapping the
   two lines changes nothing observable — hence the surviving mutant. The argument is stated at the
   call site and the ordering is a one-line invariant.
4. **The spectrum's resumed frame is not the control's frame, and cannot be.** A never-hidden view
   folded every intermediate window into its EMA; those windows are gone (the ring holds 4096
   samples). The resumed frame is the current analysis with the elapsed decay — closer to the current
   spectrum than the control is (1.23 dB vs 4.72 dB mean), and smoothing rather than staleness is
   what separates them.
5. **`LoudnessMeterView` has the same start/stop shape** and is not changed: it is never hidden in
   either layout, and its ballistics are published from the processor's atomics rather than
   accumulated in the view. Recorded, not fixed.
6. **OQ-017 is untouched.** The band `[head, head + 1]` and the time base are exactly as they were;
   this change is about WHEN the ramp is advanced, not about what it is.

---

## 13 — the review's split-publication finding: the pairing the analyser never had (2026-09-06)

The finding, at `src/gui/SpectrumView.cpp:64`: *"Split ring publication leaves stale spectrum"* —
`tick` can observe the input ring advancing while the output ring has not, and the reveal then
applies the full hidden-interval decay to both traces even though one still contains pre-switch
audio, so the first visible frame is stale and recovers over later frames.

**The causal chain as stated is false, and it is pointing at a bigger defect than it describes.**
Both halves are worth writing down, because the difference decided the fix.

### 13.1 Where the stated chain breaks

1. **The counts `tick` loads never selected either analysis window.** `analyse` called
   `readLatest`, which takes its OWN acquire load of its OWN ring's index; `ci`/`co` fed the idle
   gate, the reset count-term and the commit, and nothing else. So an `in > out` observation at the
   count loads had no effect at all on what was read or drawn.
2. **The reveal's decay cannot carry stale content.** `dt` enters `analyse` at exactly one place —
   `decay = 1 − exp (−dt / 0.12)` — acting on the EMA state, never on the window. A LONGER hide
   anchors a trace harder to the window just read (decay > 0.98 at half a second), which is the
   opposite of preserving pre-hide audio.
3. **The band the finding needs is a single point.** A window holds pre-hide audio only while fewer
   than 4096 frames have arrived since the hide, and publications are whole chunks, so "the output
   window still holds pre-hide audio while the input window does not" requires exactly
   `N_out = 3584, N_in = 4096` — one 512-frame chunk of a 4096-frame window, at the OLDEST end,
   where the Hann window weights it at 1.2 % of amplitude.
4. **The zero-progress case it claims is damaged is the one case that was already exactly right:**
   with neither count nor generation moved the idle gate returns before `analyse`, so both traces
   are held bit-for-bit and no decay is applied for audio nobody produced.

### 13.2 What is actually wrong — and it is ~100× more frequent

The skew that reaches the screen is not between the producer's two stores; it is between the
reader's two READS, and those are separated by a whole 4096-point FFT. Measured on the real
processor with a real audio thread (`spec.cpp`):

| what was measured | 48 kHz / 512 | 48 kHz / 128 |
|---|---|---|
| the two rings disagree at the COUNT loads (the review's window, ~1 µs) | 0.015 % of observations, **in ahead** | 0.029 %, in ahead |
| the two ANALYSED WINDOWS end at different indices | **1.28 % of ticks, out ahead** | **4.70 %, out ahead** |
| one FFT, measured | 132 µs | 133 µs |

The direction is the reverse of the finding's: `analyse (in, …)` runs first, so the output ring's
read is the later one and it is the OUTPUT trace that leads. At 60 Hz, 1.28 % is about once a
second. What such a frame draws, constructed deterministically (a marker chunk published to one
ring only, 6 kHz into a bin the settled programme leaves empty):

| first visible frame, 48 kHz / 512 | before | after |
|---|---|---|
| marker bin, input trace | **−32.97 dB** | −120.00 dB |
| marker bin, output trace | −120.00 dB | −120.00 dB |
| in/out mismatch | **87.03 dB** | **0.00 dB** |
| the same with the roles swapped (output published alone) | 87.03 dB | **0.00 dB** |
| a chunk BOTH taps published | 0.00 dB (both show it) | 0.00 dB (both show it) |
| neither tap moved | 0.00 dB, held | 0.00 dB, held |

The reveal is not special here: an ordinary tick produced the same 87.03 dB mismatch. The finding
attached the defect to the reveal because that is where it was looking; the defect is in every tick.

### 13.3 The invariant

> **One frame, one span.** A frame's two traces are analysed over the SAME committed span of audio:
> the window ending at `E = min (w_in, w_out)`, the newest frame index BOTH taps have published. A
> chunk one tap has published alone is not yet a state the PAIR can represent, and is drawn on the
> first frame where both have. The hidden-interval decay is then justified for both traces by
> construction, because they describe the same span and the same seconds.

Chosen over the alternatives the brief lists: a shared publication epoch would mean a new atomic on
the audio path for a display concern; per-trace elapsed time is the wrong quantity (time passed for
both, and the EMA's target is the analysis of what each ring holds); atomic publication of the pair
would mean changing the producer, which is the one thing a display must not cost.

### 13.4 The fix

* `ScopeBuffer::readEndingAt (dst, dst, count, end)` — `readLatest` with an upper bound on where the
  window ends, and `readLatest` is now that call with `kNewest`. The CLAMP is the correctness half:
  `e = min (end, acquired index)`, so an index from the other ring (which can be larger) or one this
  ring has since rewound reads exactly what `readLatest` would have. Fourth functional delta on a
  file copied from Anamorph; the provenance banner says so.
* `SpectrumView::tick` takes `E = min (ci, co)` once, passes it to both analyses, keys the idle gate
  on it, and remembers it (`shownCommitted`). The per-ring counts stay for `resetObserved`, whose
  coherence argument is about one ring's modification order and does not transfer to a minimum.
* `analysedOutDb()` joins `analysedInDb()` as a read-only accessor: the defect is a disagreement
  between the pair, and a test that can see one trace cannot pin it.

Cost: a frame that catches the split waits for the other tap — at most one chunk (10.7 ms at
48 kHz / 512), on the ~1–5 % of ticks that see it at all. Nothing else changes: same FFT, same
window function, same normalisation, same EMA, same attack-instant rule, same bin mapping, same
reset floors, same geometry.

### 13.5 Validation

* **0 of ~15 000 ticks** with differing spans at either block size, against 193 and 707 before.
* All four orderings, deterministic: input alone → neither trace moves; output alone → neither
  moves; both → both move together; neither → held bit-for-bit. Through an ordinary tick and
  through the reveal.
* **No recovery tail:** the marker reaches both traces on the first frame after the second tap
  publishes, and the 12 frames after that move it 0.00 dB further.
* The GR history's right-edge/rigid-translation table and its left-edge/oldest-bucket table both
  re-run **byte-identical**; §11/§12's reveal numbers re-run unchanged (stopped-transport GR first
  frames 0 of 180; spectrum distance from the current analysis 1.230 → 1.231 dB, run-to-run noise).
* Suites 320 + 1116 = **1436**, 0 failures.

### 13.6 Tests and mutants

`testTheSpectrumsTwoTracesAlwaysDescribeTheSameSpan` (state suite) constructs the split at the
narrowest boundary that reproduces it — a chunk pushed into one ring and not the other IS the state
the audio thread holds between its two publications — and separates the two halves of the repair:
the READ cases advance the pair first, so the tick RUNS and the read is what decides; the GATE case
leaves the EMA mid-fall, so "the gate held" is distinguishable from "the gate opened and the
analysis landed on the same numbers". Four ring-level checks in the DSP suite pin `readEndingAt`
itself, the clamp included.

| mutant | killed by |
|---|---|
| each ring read at its own head again (the pre-0.2.12 pairing) | 3 × `specSpan` |
| the committed head takes `max` instead of `min` | 6 × `specSpan` |
| `readEndingAt` no longer clamps to this ring's head | 4 × DSP, incl. the tap-content pin |
| the idle gate goes back to the per-ring counts | `specSpan` (the mid-fall gate case) |
| the committed head is never remembered | `specSpan` (the same) |

### 13.7 What is left

1. **Index-aligned is not time-aligned.** The output tap carries the chain's latency, so frame k of
   the output ring is the processed form of input audio ~10 ms earlier at 48 kHz. Aligning the taps
   would mean delaying the input tap by the reported latency — a display decision with its own cost.
   Recorded, not taken.
2. **KI-018's cross-ring variant is narrowed, not closed** — a rewind visible at the count loads now
   floors both traces together; one landing after them still floors the rewound ring alone for one
   tick. Its equal-count corner is untouched.
3. **KI-007 item 6 is untouched by design:** a tick that sees nothing new still holds the trace, and
   the entry's predicate wording was corrected rather than its behaviour.
4. **OQ-017 untouched.**

---

## 14 — the large-block finding: a shared endpoint is only half of a shared span (2026-09-06)

The finding, at `src/dsp/ScopeBuffer.h:325`: *"Large audio blocks desynchronize spectrum traces"* —
`readEndingAt` does not stay safe when one producer chunk is longer than the ring's safe historical
window, so the faster ring can overwrite slots the shared endpoint still makes the reader ask for,
and the two traces describe different audio again despite sharing `E`.

**This one is right, and it is right in the terms it states.** §13's own comment named the quantity
— "12288 frames become 12288 − skew" — and did not follow the subtraction to zero.

### 14.1 The arithmetic, and the boundary

A slot holds absolute index `i` until the producer writes `i + capacity`. The reader asks for
`[E − count, E)`, so the oldest frame it wants survives iff `w − E ≤ capacity − count` = **12288**
for the analyser's 4096 of 16384. Above that, `w − E − 12288` frames of the window have been taken
back, all of it at `w − E ≥ 16384`. In the split window `w − E` is one chunk, and `num` is the
host's prepared block with no upper clamp.

Reproduced directly — the window each ring returns at `E` is snapshotted, the split is constructed,
and the same window is re-read and compared frame by frame:

| chunk | frames of the 4096 requested that had been overwritten |
|---|---|
| 512, 4096, 12287, **12288** | **0** |
| **12289** | **1** |
| 13000 | 712 |
| 16383 | 4095 |
| 16384, 20000, 32768 | **4096 — the whole window** |

Which ring is unsafe: whichever one is AHEAD (it is the one that has lapped its own history); the
lagging ring can still serve the window in full, which is precisely how the two traces come to
describe different audio while sharing an endpoint. Both publication orders reach it — the producer
publishes the input tap first, but a reset publishes the rings independently and the reader's own
loads are independent too. The condition is per-CHUNK, not cumulative: the two rings advance by the
same `num` per chunk, so the skew is 0 or `num` and never compounds. The reader's own delay is the
OTHER term (`w_at_read − count_at_load`), which is the pre-existing `readLatest` display margin and
is unchanged by any of this.

Reachability needs both halves: the tick must RUN (the committed head is also the idle gate, so a
split alone leaves that head where the last frame was drawn) and the skew must exceed 12288. That
means a host preparing blocks above 12288 frames AND a reader that missed a chunk — an offline
render with the editor open, where the message thread is starved for far longer than one block.

### 14.2 What it drew

The display consequence, measured through the real analyser on the real rings, in a bin where the
two taps genuinely differ by 0.3 dB:

| chunk | before: overwritten / in vs out | after: span / in vs out |
|---|---|---|
| 512 … 12288 | 0 / **0.31 dB** | 4096 / **0.31 dB** (bit-identical) |
| 12289 | 1 / 0.31 | 4095 / 0.31 |
| 13000 | 712 / **15.6 dB** | 3384 / **0.43 dB** |
| 16383 | 4095 / 2.8–3.2 dB | 1 / 0.19 dB |
| 16384 | 4096 / **18.4 dB** (one trace at −120) | **held** / 0.34 dB |
| 20000, 32768 | 4096 / **21.4 dB** | **held** / 0.24 dB |

### 14.3 The invariant

> **One frame, one span — and a span both rings still hold.** A ring can serve `[w − capacity, w)`.
> The pair's floor is the HIGHER of the two rings' floors, because a span is common only if both
> still hold it; the window is `[E − N, E)` with `N = min (kSize, E − floor)`; and where `N` is
> zero there is no coherent pair to draw.

### 14.4 The fix

* `ScopeBuffer::oldestReadable()` — one acquire load, no payload touched: the oldest absolute index
  this ring can still serve.
* `readEndingAt` clamps its START to that floor, re-derived from its own acquired index, and returns
  the shorter count. A backstop for the caller that does not ask and for the producer that advances
  during the transform; the short-read contract already covers it (`analyse` zero-pads).
* `SpectrumView::tick` computes `floor = max (in.oldestReadable(), out.oldestReadable())` and
  `span = min (kSize, committed − floor)`, passes ONE span to both analyses — so neither read has to
  shorten itself and the two windows cannot end up different lengths — and where `span == 0` with a
  non-empty history, **holds the last coherent pair** rather than drawing half a frame.

Why holding only there: a shorter window is the honest analysis of what is still available, and it
is what this view already does at start-up and after a rewind; an EMPTY window is not an analysis at
all, and flooring would put silence on screen where there is audio. `committed == 0` stays on the
zero-length path (empty or rewound rings floor, as before).

The one visible consequence of a shortened window, recorded rather than hidden: fewer real samples
zero-padded into the same 4096-point transform leak more, so a shortened frame reads a little hotter
away from the programme (16 dB in the marker bin at a 13000-frame chunk) — **on both traces
equally**, which is the property under repair. It lasts one frame.

### 14.5 Validation

* **0 overwritten frames read at every chunk size from 512 to 32768**, both publication orders.
* Traces agree at every size: worst 0.43 dB, against 15.6–21.6 dB before.
* **Blocks up to 12288 are bit-identical to before** — same span, same numbers.
* §13's skew measurement re-runs: 0.80 % / 3.82 % of ticks with differing spans at each ring's own
  head, **0.000 % at the committed head**.
* §11/§12's reveal numbers re-run unchanged (GR zero-progress first frames 0 of 180; spectrum
  distance from the current analysis 1.231 dB); the GR right-edge and left-edge tables re-run
  **byte-identical**.
* Suites 324 + 1154 = **1478**, 0 failures.

### 14.6 Mutants

| mutant | killed by |
|---|---|
| `readEndingAt` no longer clamps its start | the ring's value-pinned lapping check |
| the view asks for the full window regardless | 34 × `specLap` |
| the pair's floor takes the LOWER of the two rings | 34 × `specLap` |
| no hold when no common span survives | 4 × `specLap` (the held case) |
| each ring read at its own head again | 16 × `specLap` + `specSpan` |

### 14.7 What this does and does not touch

* **KI-018 is unchanged.** Its remaining corner is a reset-and-refill identity question; this is a
  lapping one. Nothing here narrows or widens it.
* **The reveal-smoothing note stays informational.** A resumed view cannot replay the intermediate
  windows a continuously visible one folded into its EMA — the ring holds 4096 frames, and those
  windows are gone. This round changes nothing about it: the span rule decides WHICH samples a frame
  analyses, never how many past frames the EMA has seen. Still a design limitation, still recorded,
  still not expanded into a fix.
* **OQ-017 untouched**; the GR history is not in this diff.
* Still open: the taps are index-aligned, not audio-time aligned (the chain's latency sits between
  them); and the pre-existing display margin against a producer that laps the reader DURING a
  transform is unchanged — `readEndingAt`'s start clamp now bounds what that can return, but the
  frames it drops are frames no reader could have had.

---

## 15 — the concurrent-publication finding: a chosen span is not a held span (2026-09-06)

The finding, at `src/gui/SpectrumView.cpp:329`: the shared floor is sampled, a large block lands
after that snapshot, the two `readEndingAt` calls no longer observe the same ring state, one window
shortens, and the frame draws two spans again. **Correct, and the mechanism is exactly as described.**

### 15.1 The interleaving

`readEndingAt` clamps its start to ITS OWN ring's floor at ITS OWN acquire load — which is right for
one ring and is precisely what breaks a pair: whichever read observes the newer state shortens, and
the other does not. Four places the producer can land, all reachable:

| where the producer publishes | what happened before | what happens now |
|---|---|---|
| before the snapshot | the span is chosen from the new state (§14) | unchanged |
| between the snapshot and read 1 | read 1 shortens, read 2 does not | rejected, frame held |
| between read 1 and read 2 | read 2 shortens, read 1 does not | rejected, frame held |
| during either copy | neither `got` sees it; the copy is trampled | rejected by the floor re-read |

Measured with a producer publishing flat out beside the analyser, 3000 frames per configuration:

| | 512-frame chunk | 13000-frame chunk |
|---|---|---|
| frames with one read short (before) | 0 | **1802** |
| frames whose copy was lapped (before) | 4 | **3000** |
| **drawn frames holding two windows that were not the same audio (before)** | 0 | **1441 of 3000** |
| the same, after | **0 of 2913** | **0 of 873** |

### 15.2 Why §14's floor was not enough

§14 chose the span from a snapshot, which settles what is ASKED. It cannot settle what came back:
the snapshot is not a lock, and the two reads are two separate copies from a producer that never
stops. The clamp inside `readEndingAt` then makes the asymmetry rather than preventing it.

### 15.3 The invariant

> Every displayed frame analyses one span that was valid for BOTH rings when it was acquired and
> still valid for both reads when they finished. A frame that cannot show that is not drawn.

### 15.4 The proof, and the term no reader can see

`SpectrumView::onePairOneSpan (span, gotIn, gotOut, first, oldestIn, oldestOut)` — both reads served
the whole span, and neither ring's floor has passed the window's start. The first term rejects a read
the producer shortened. The second is the before-and-after discipline the reset generations already
use, applied to the lapping bound: the floor is monotone, so a floor still at or below `first` after
both copies means no slot in the window was overwritten at any point during either. The frame reads
both windows into their own scratch pair BEFORE transforming either — the old single pair was
overwritten by the second read — and a frame that cannot prove itself is **held whole**: nothing
folded into either EMA, nothing committed, and the next tick re-derives. No retry loop, no lock: the
invalidation needs the producer to publish `capacity − span` frames inside two 4096-frame copies, so
a retry is a second draw of the same lottery on a thread that has a frame to paint.

That proof still left a residual, and it is the one the review's framing cannot reach: **`pushBlock`
writes its payload BEFORE it publishes its index**, so a push that has not published yet is invisible
in `write` while its stores are already landing on slots a reader is copying — a reader checking the
index before and after sees a ring that never moved. Measured: **17 of 2269 drawn frames** still held
two different windows at a 13000-frame push with the pair proved against the published index alone.
The bound a reader needs is the SIZE of the largest push. **CORRECTED in §16.3, same round:** this
section first recorded a `ScopeBuffer::prepare (maxPushFrames)` and a `maxPush` atomic inside the
ring, set by the engine at `prepare`. That draft was withdrawn — the largest push is
`samplesPerBlock`, which the plugin ALREADY publishes — and the reserve is now
`SpectrumView::reservedFloor (writeCount, preparedBlockSize())`, applied where the two rings are
paired. The arithmetic and every measurement below are unchanged; what changed is which side owns
the bound, and that the ring's protocol gained nothing. **The audio thread pays nothing** either way.

Rejected, and recorded rather than hidden: a RESERVATION INDEX published before the payload writes
would be exact and would keep drawing where this reserve gives up, but it is a store on the audio
path and an `ARCHITECTURE_REVIEW_GATE` item. What the reserve gives up: a host preparing blocks of a
whole ring or more (16384 frames, 341 ms at 48 kHz) leaves no window a reader can vouch for at any
instant, so the analyser holds. Below that it costs nothing measurable — with the producer at its
real cadence and the analyser at 60 Hz, **0 of 360 frames** across 512-, 4096- and 13000-frame blocks
refused to draw audio that had arrived, and the per-frame cost is two atomic loads and a branch.

### 15.5 Tests and mutants

The decision is a pure static with a seven-row truth table; the stimulus is a thread — a producer
publishing 13000-frame chunks flat out beside 5000 drawn frames, with IDENTICAL audio in both rings,
so a frame whose two traces differ at all is a frame whose two windows were not the same span. Five
`specSync` checks pin the ring's reserve, including the case where one push can rewrite the whole
ring and the floor says so rather than pretending.

| mutant | killed by |
|---|---|
| the pair is never proved | `specRace` (threaded) |
| only the counts are checked (a lapped copy accepted) | 2 × `specRace` truth table |
| only the floors are checked (a short read accepted) | 2 × `specRace` truth table |
| the floor stops reserving room for an in-flight push | `specSync` / `specReserve` (§16.3) |
| each ring read with its own independently acquired state | `specRace` + 3 × `specSpan` |
| the engine stops telling the rings the largest push | `specRace` (mutant retired with the draft — see §16.3) |

### 15.6 What this changes about §14, and what it does not

The no-split behaviour of §14 is unchanged: the full 4096-frame window up to a 12288-frame block,
shorter above it. What changes is the SPLIT case at large blocks — where §14 drew a shortened window,
the frame is now held, because the reserve says no window is safe while a push of that size may be in
flight. Blocks up to 4096 are bit-identical to §14 in every case measured. KI-018 is untouched (its
corner is a reset-identity question, this is a publication-timing one), the reveal-smoothing note is
untouched (this decides which samples a frame analyses, never how many past frames the EMA has seen),
and OQ-017 is untouched.

### 15.7 Follow-ups

1. **The reservation index** — the audio-path change that would keep the analyser drawing at block
   sizes at or above the ring's capacity. An architecture-gate item; not taken here.
2. Unchanged from §14: the taps are index-aligned, not audio-time aligned.

---

## 16. The pair reaches the screen (2026-09-06, round 10)

Three items from one review pass: two SpectrumView correctness bugs and one repository
architecture-review requirement. §15 and everything before it concern how the analyser READS. This
section is about the hand-over from `tick` to `paint`, and about a protocol extension that turned out
not to be needed.

### 16.1 Concurrent painting splits the traces

**Who runs `paint`.** Not assumed — read off the tree. `PluginEditor` attaches a
`juce::OpenGLContext` on macOS and Windows and never on Linux/X11, and when a context is attached
JUCE paints its components on the context's render thread. `THREAD_MODEL.md` §"Which context paints"
already states it, and ADR-0027 and ADR-0038 are two earlier defects of exactly this shape in this
editor. `SpectrumView::tick` is a `juce::VBlankAttachment` callback through `abgui::FrameClock`, so
it is the message thread. Two threads, two `std::vector<float>` of 2048 floats each, nothing between
them.

**Two defects, one fix.** The accesses are a data race by the letter of the memory model — the
same class as `presetMenusOpen` (ADR-0027) and the GR scroll scalars (ADR-0038). Separately, and
this is the one that is visible: `tick` assigns `inDb` and then `outDb`, so a paint landing between
them draws the input spectrum of tick N beside the output spectrum of tick N + 1 — the split the
committed head removed from the ANALYSIS, re-entering at the display.

**Reproduced with a controlled harness and a generation marker made of the audio itself.** Both
rings get IDENTICAL blocks, so a coherent frame analyses the same samples twice and its two traces
come back bit-identical; the tone alternates every tick over a whole 4096-frame window, so a frame
assembled from two ticks disagrees across the spectrum. Any inequality is a mixed frame, and there is
no threshold:

| what the reading thread reads | reads | mixed |
|---|---|---|
| the tick's working vectors (pre-fix shape) | 1 321 607 | **1 161 778 (87.9 %)** |
| each trace published as soon as it is computed | 416 230 | **277 334 (66.6 %)** |
| one bracketed publication (shipped) | 306 485 | **0** |

**The mechanism, and why this one.** A sequence bracket over per-bin `std::atomic<float>` storage,
carrying both traces and the window they describe; the painter copies inside the bracket into buffers
only `paint` touches and keeps the copy only if the counter did not move; two attempts, then keep the
frame already held. Rejected: an immutable snapshot per tick (16 KB allocated every frame, and
`atomic<shared_ptr>` is not lock-free here), two slots with an atomic index (tears if two ticks land
in one paint — a timing assumption, not a proof), a triple buffer (correct and wait-free, but 48 KB
and an ownership protocol the tree does not otherwise have, for a fallback that costs one repeated
frame at 16.7 ms — recorded as the option to take first if that ever matters), a mutex (a lock on the
paint path and one the tick can wait on), and painting-thread ownership ("the painting thread" is not
one thread — GL during `renderOpenGL`, the message thread on Linux, and again for
`createComponentSnapshot`, which the suite uses).

**This is an `ARCHITECTURE_REVIEW_GATE` item and it is FILED, not claimed.** A new cross-thread path
carrying a payload, and a new atomic ordering. ADR-0027 clause 4 and ADR-0038 clause 8 both name this
case as returning to the gate in as many words. [ADR-0039](../docs/architecture/design-decisions/ADR-0039-spectrum-frame-publication.md)
is `Proposed`; a green build does not clear it.

### 16.2 A large-block reset leaves the previous mapping on screen

**Not the mechanism the finding names, and the difference matters.** The vectors ARE floored on the
reset edge — `resetIn`/`resetOut` fill them with −120 before the span is consulted. What the early
return at `span == 0 && committed > 0` skips is the **publication and the repaint**, so the floored
trace never reaches the screen. And at a host block of at least a whole ring the span is 0 not for
one tick but for every tick that follows (`reservedFloor` puts the floor at or past the head there),
so the hold never ends: the previous rate's spectrum stays up, under the new rate's bin mapping, for
as long as the plugin runs. 6 kHz sits at bin 512 at 48 kHz and at bin 256 at 96 kHz.

It needs the reset and the audio to arrive TOGETHER: a tick landing between the rewind and the first
block sees `committed == 0`, takes the ordinary zero-length path, floors and publishes. That case
always worked, and it is the control in the test.

**The behaviour chosen.** On the reset edge only, the view publishes the display floor in both traces
with a zero-length window and commits the reset accounting. Not a fabricated frame — it is exactly
what this view shows before its first frame. Scoped to the edge deliberately: without a reset, a span
of 0 is the lapping case of §15, where the held pair is still the current configuration's and
blanking it would put silence on screen where there is audio. §15's ring-safety behaviour is
unchanged at every block size.

### 16.3 The threading change that was withdrawn instead of reviewed

§15 added `ScopeBuffer::prepare (maxPushFrames)` and a `maxPush` atomic inside the ring so the floor
could reserve an in-flight push. The review is right that this is architecture-level: it adds a field
to the shared producer/consumer protocol, which is `ARCHITECTURE_REVIEW_GATE.md`'s "Thread Model
change — new cross-thread path" and `AI_AGENT_POLICY.md`'s hard stop, and no test result clears it.

Before preparing that review material the question the round is required to ask first — can the
design avoid extending the protocol? — has a plain answer: **the largest push is `samplesPerBlock`,
and the plugin already publishes it.** `GrHistoryBuffer::prepared()` returns `{rate, block}`,
`AnabasisAudioProcessor::preparedSampleRate()` forwards the rate under the stated rule "one atomic,
one publication discipline, no second home for the same fact", and `PluginProcessor.cpp` passes the
same `samplesPerBlock` to `grHistoryRing.prepare` and to `engine.prepare`, where it becomes
`maxBlock` and bounds every `processChunk` push. So the draft was a SECOND HOME for a published
fact, and it was removed rather than reviewed: `ScopeBuffer::prepare` and `maxPush` are gone,
`oldestReadable()` is back to promising exactly what the published index proves, and the reserve is
`SpectrumView::reservedFloor (writeCount, preparedBlockSize())`, applied both when the span is chosen
and in the post-read proof. The ring's protocol is byte-identical to §14's.

Verified rather than assumed: the `spec` harness re-run against the shipped view reports 0 frames
with disagreeing traces at 512-, 13000- and 16384-frame chunks with a producer flat out, the
large-block boundary table is identical to §14/§15 frame for frame, and the mutant that drops the
reserve fails 30 checks.

**What would still have needed clearance had it been kept**, recorded because the answer is the
material and not the outcome: `maxPush` is the largest number of frames one `pushBlock` can write; it
is written by the host thread inside `prepare` with audio stopped (the same named premise `reset`
rests on) and read by the GUI thread in the floor query; relaxed would have sufficed since it orders
nothing, and its lifetime is the ring's; it is needed because a reader cannot otherwise bound an
unpublished push; the invariant it serves is "a drawn frame rests only on frames no in-flight push
can be inside"; it interacts with overwrite safety by making the floor conservative and never
liberal; it adds no allocation and no blocking to the audio path, which never reads it; the
alternatives were the reader-side reserve that shipped and an audio-path reservation index (still
rejected, still a gate item); and without any of them 17 of 2269 drawn frames mismatched.

### 16.4 Tests and mutants

`specFrame` — deterministic (what a tick publishes is the frame it committed; an idle tick leaves it
untouched; a paint takes the published window and follows it) and threaded (4000 publications, a
reading thread standing in for the renderer, alternating whole-window tones, identical audio in both
rings). `specReset` — 8192/13000/16384/32768 × with and without new audio, 48 kHz → 96 kHz.
`specReserve` — the floor arithmetic and a drawn frame obeying it at three prepared block sizes.

| mutant | killed by |
|---|---|
| the reset publishes nothing (pre-fix early return) | 4 × `specReset` |
| the renderer reads the tick's working vectors | `specFrame` (1 161 778 mixed) |
| the sequence bracket removed from the reader | `specFrame` |
| the reader-side reserve dropped | 30 checks (`specLap`, `specReserve`) |
| the painter reads the working vectors | 2 × `specFrame` (the painted window never moves) |
| each trace published as soon as it is computed | `specFrame` (277 334 mixed) |
| *(survives)* the reader brackets each trace separately | the writer's gap between two brackets is zero-width; the mutant models no realistic defect |
| *(survives)* the reset publishes without committing its accounting | behaviourally identical — later ticks re-answer the same reset and produce the identical frame; it costs work, not correctness |

**What no headless suite can see, stated rather than implied.** `repaint()` is what carries a
published frame to the screen, and there is no repaint region to inspect: a mutant that deletes the
call while leaving the publication survives. The data race itself is likewise argued from the memory
model rather than measured — the same limit ADR-0038 records.

### 16.5 Follow-ups

1. **ADR-0039's clearance.** A merge prerequisite for the pull request, not work in the tree.
2. Unchanged from §15: the audio-path reservation index; and the taps are index-aligned, not
   audio-time aligned.

---

## 17. The frame carries its own configuration (2026-09-06, round 11)

Three items: one correctness finding, one architecture-review requirement, one CI failure that turned
out to be neither.

### 17.1 Mismatched frequency axes — reproduced, and not quite where the finding said

**The lifecycle, traced rather than assumed.** The rate the display uses lives in
`GrHistoryBuffer::preparedRate` (`src/dsp/GrHistoryBuffer.h:338`), written relaxed inside `clear`
(`:232`) between a release-fenced odd guard increment and a release even one (`:217-235`), read
relaxed through `prepared()` (`:176-180`) and forwarded as
`AnabasisAudioProcessor::preparedSampleRate` (`src/PluginProcessor.h:564`). One writer: the host's
reconfiguration thread, through `prepareToPlay` (`src/PluginProcessor.cpp:748,785`). Three readers:
`GrHistoryView` (bracketed), `CurveView` (unbracketed), `SpectrumView::paint` (unbracketed,
`src/gui/SpectrumView.cpp:575` before this round).

**What it affects, corrected.** Not the axis. The x axis is a fixed 20 Hz–20 kHz log sweep with no
rate term (`:577,651-654,661`); the rate enters only as `binHz = rate / kSize` (`:619`) and from
there into the regime test (`:636`), the Catmull-Rom sample position (`:638`) and the averaged bin
range (`:639-640`). A rate mismatch moves the DATA under a stationary axis. There are no frequency
labels and no Nyquist bound to get wrong; above Nyquist the fixed axis saturates onto the top bin
through `jlimit`, which is a pre-existing display property and is untouched.

**Reproduced.** 6 kHz is bin 512 at 48 kHz and bin 256 at 96 kHz. Read at the bin the tone actually
occupies:

| pairing | dB at the tone |
|---|---|
| 48 kHz trace, 48 kHz rate | **−0.00** |
| 48 kHz trace, 96 kHz rate | **−116.80** |
| 96 kHz trace, 96 kHz rate | **−0.00** |
| 96 kHz trace, 48 kHz rate | **−120.00** |

The tone leaves the display outright, either way.

**Where the window is, and it is not the one already audited.** `KNOWN_ISSUES` KI-017's
`prepareToPlay` publication-lag note audits the window INSIDE `engine.prepare` — rings rewound, pair
not yet republished — and concludes correctly that `SpectrumView` reads an empty ring there. The
defect is on the other side: **after** the pair republishes and **before** the view's next tick
publishes a frame. Round 10 widened that window rather than closing it, by giving the trace its own
publication schedule and leaving the rate on the processor's.

**The fix, and why it is a bracket rather than a comparison.** The rate travels inside the published
frame (`SpectrumView::Frame`), and `paint` reads no processor state at all. Which rate to publish is
the whole question: the rate and the frames live in different objects, and a relaxed load of one is
unordered against an acquire load of the other. `prepareToPlay` writes them in one sequence — rings
rewound at `:769`, pair republished at `:785` — so `tick` samples `GrHistoryBuffer::resetEpoch()` and
the rate together at its top and closes with `batchIntact` before it commits anything. That is
`GrHistoryView`'s reader contract verbatim, evenness test included
(`src/gui/GrHistoryView.cpp:144`), and it is what moves this view from the ring banner's unbracketed
discipline to its bracketed one. The three cases are a split over where the sampled epoch fell in
`resetGuard`'s modification order — odd; even-before-the-clear; even-after-the-clear — which is what
makes them exhaustive; splitting over the direction of the mismatch, as the first draft did, is not a
partition. Full statement in ADR-0039 clause 9.

**Two things it does not claim**, both found by adversarially refuting the first draft: the epoch
announces the RATE, not the ring reset (`GrHistoryBuffer::prepare` clears only on a changed pair
while `AnabasisEngine::prepare` rewinds unconditionally, so `resetObserved` remains the sole detector
for a same-pair re-prepare); and it pairs the rate with the frames this tick's ACQUIRED INDICES
describe, not with every sample the EMA remembers — KI-018's one-tick cross-ring residual is
unchanged and now carries a rate consequence, recorded there.

### 17.2 The architecture gate

Unchanged in substance and still OPEN. `ARCHITECTURE_REVIEW_GATE.md:13` gates a "new cross-thread
path, new atomic ordering"; `THREADING_POLICY.md:29` makes "any path not in this table" one;
ADR-0027 clause 4 and ADR-0038 clause 8 name a payload as returning to the gate.
`AI_AGENT_POLICY.md:62-63` says a passing build does not clear it and only human review does, and
there is no provision anywhere for an agent to mark such a record Accepted. The owner's blanket
approval for post-v0.1.0 rounds explicitly excludes the gated class (ADR-0026).

**Widened rather than re-filed.** ADR-0039 is `Proposed`, not signed off; the index's warning is
about widening a record that HAS been signed off. Amending it now is what puts one coherent design in
front of the reviewer. The record gained a review package (ownership, threads, publication sequence,
orderings, invariant, retry, allocation, measured cost, reset behaviour, rate coupling, alternatives,
necessity) and file:line citations in the format `SOURCE_OF_TRUTH.md` fixes.

**Simpler design, asked and answered.** The chosen mechanism makes the boundary NARROWER than before,
not wider: `paint` now reads exactly one object where it used to read the view's mutable vectors plus
a processor accessor. Adding the rate cost one `std::atomic<double>` inside an existing bracket and
one reuse of an existing reader contract — no new mechanism, no new shared state, nothing on the
audio path. The alternatives are recorded in the ADR and each is larger.

**A doc-sync gap round 10 left, found and closed:** `DOCUMENTATION_LIFECYCLE_POLICY.md` requires
`THREAD_MODEL.md` AND `THREADING_POLICY.md` AND an ADR; only the first two of the three had been
done. That is verbatim the omission ADR-0027 exists to record.

### 17.3 The CI failure was the test, not the detector

The failing job is named `sanitizers`, but the failing STEP is valgrind memcheck on the UNSANITIZED
Release build, and memcheck reported **0 errors from 0 contexts**. The failure was one assertion:
`specFrame: (premise) the reading thread really did read whole frames, and the pair really was moving
under it`. The ASan+UBSan leg passed.

**Root cause.** valgrind serialises threads, so while the reader holds the CPU the published frame
cannot move: every iteration of a reader quantum returns the same frame, and `distinct` counts reader
quanta that straddled a publication rather than reads. With `lastFirst` seeded to `~0` the first
successful read always makes it 1, so `distinct > 1` failing means the reader got exactly ONE
productive turn in 4000 ticks — near-total starvation, not a marginal miss.

**Not reproduced locally, and the honest form of that.** Three configurations, none failing: native;
memcheck on one CPU; memcheck on one CPU under contention. What DOES reproduce is the mechanism, as a
monotone degradation — distinct frames out of 500 ticks:

| competing spin loops on the same CPU | no yield | with a yield per tick |
|---|---|---|
| 0 | 481 | 499 |
| 8 | 221 | 454 |
| 24 | 219 | 486 |

The reader's share of frames is the scheduler's to decide, and CI decided 1.

**The fix is the test's, and the repo already prescribes it.**
`testTheFrozenLatchNeedsNoThreadCrossing` records the same class of failure from 2026-08-14 (run
31801408265, same job) and the rule: *"the fix is to remove the dependency rather than to tune it …
holds by construction … A stronger stimulus than the original, not a weaker one."* So the reader is
waited for — RUNNING and holding a whole frame before the measured section (guaranteed: nothing
publishes during that wait, so the counter is even and stable and its first attempt succeeds) — and
then published-and-yielded to until it has taken a second, different frame. The two halves of the
premise are also split into two checks, so a future failure names which one broke. No product change
is justified: widening the two-attempt retry bound would contradict its stated rationale and would
itself be a gated threading change.

**Evidence it now passes:** memcheck, `--track-origins=yes --error-exitcode=1`, pinned to ONE CPU
(the worst case for this starvation): **1261 checks, 0 failures, 0 errors from 0 contexts, exit 0.**

### 17.4 Tests and mutants

`specAxis` (deterministic: the frame carries its rate; a paint before the next tick draws the previous
frame through the rate that produced it; the reconfiguration cycle never splits the pair; the empty
frame carries its configuration; a never-prepared view falls back to 48 kHz rather than dividing by
zero) and a threaded variant in which a reading thread watches a 48 kHz ⇄ 96 kHz churn and asserts
that every whole frame with a real peak puts the tone where its own rate says it is. `specFrame`
gains a rate check in the reading thread.

| mutant | killed by |
|---|---|
| `paint` reads `preparedSampleRate()` for itself (the pre-fix pairing) | `specAxis` (the no-tick window) |
| the trace is published without its rate | 4 × `specAxis` |
| the rate is published on the tick's schedule, not the frame's | `specAxis` (threaded churn) |
| the zero-rate fallback removed | `specAxis` (unprepared view) |
| *(re-run)* renderer reads the working vectors · reset publishes nothing · sequence bracket removed · reserve dropped | as §16.4 |
| *(survives)* the rate stored one instruction past the closing release | the reader reads the rate ~4096 loads after the sequence load, so the writer must be preempted inside a one-instruction window for the reader's whole copy |
| *(survives)* the configuration bracket removed · its evenness test removed · the reset commit moved in front of it | all three guard against a `prepareToPlay` landing INSIDE a tick; the suite reconfigures from the thread that ticks, and staging an overlap needs a host thread reconfiguring while audio is present, which the named plugin-API premise forbids. The evenness test is required by `GrHistoryBuffer`'s stated reader contract whether or not a test can see it |

**Cost, measured** (2000 iterations): a whole tick 210.5 µs; `publishFrame` 2.37 µs of it — 1.13 % of
a tick, 0.014 % of a 60 Hz frame; `readPublishedFrame` 1.50 µs. 32 KB per view, allocated once.
Nothing on the audio path changed.

### 17.5 Follow-ups

1. **ADR-0039's clearance.** Still a merge prerequisite, not work in the tree.
2. Unchanged: the audio-path reservation index; the taps are index-aligned, not audio-time aligned;
   and the reveal-smoothing limitation, which this round did not touch — the ring still does not
   retain the intermediate windows a continuously visible EMA would have seen, and nothing in the
   frame publication changes that premise.

---

## 18. The sanitizers job, closed (2026-09-06, round 12)

### 18.1 What actually failed

The job is named `sanitizers` and runs two legs. Both were examined rather than one:

| leg | result |
|---|---|
| clang-22 ASan + UBSan, both suites | `PASS: 327` and `PASS: 1261`, **no sanitizer report of any kind** |
| valgrind memcheck, `AnabasisTests` | `PASS: 323`, `ERROR SUMMARY: 0 errors from 0 contexts` |
| valgrind memcheck, `AnabasisStateTests` | **`FAIL: specAxis: (premise) …` — 1261 checks, 1 failure**, `ERROR SUMMARY: 0 errors from 0 contexts` |

So: not a sanitizer, not a memory error, not UB. The failing command is the memcheck step
(`build.yml:1184-1189`) on the UNSANITIZED `build-vg` binary, and the failing thing is one premise
assertion in `testNoFrameARendererPicksUpEverMixesTwoConfigurations`.

**It is the same class as round 11's failure, in the test round 11 added** — the repair was applied
to the sibling test and not to the new one, which ran a bounded churn and then asserted that the
reading thread had seen a frame at both rates. Under valgrind's serialised scheduler the reader's
share of frames is not the test's to decide, and a bounded loop simply runs out.

**Reproduced?** Not locally, and that is stated rather than glossed. Three configurations were run
against the CI-failing form — native; memcheck on one CPU; memcheck on one CPU under eight competing
spin loops — and all three passed. This is exactly what
`testTheFrozenLatchNeedsNoThreadCrossing` recorded of its own instance in 2026-08-14 ("the same build
under the same command reproduces 8440 polls here across repeated runs and never 0 — so the mechanism
is unconfirmed and is deliberately not asserted"). The reproduction of record is CI itself, twice.

**One honest correction to the round-11 report.** It cited "the property the test exists to assert
did not fail" as evidence of product correctness. That is vacuous on that run: `split` is only
incremented for frames the reader actually observed, so if the reader observed none at the second
rate, `split == 0` says nothing. A premise failure invalidates the assertion it guards. Withdrawn.

### 18.2 The fix, and a second one the audit found

**Test side.** Every threaded premise in the file is now (a) established by waiting for the other
thread to have done the thing, (b) BOUNDED, so a liveness defect fails the suite instead of hanging
it to a CI timeout, and (c) split into one check per conjunct, so a failure names which half broke.

**Product side — and this one is real.** Auditing the whole publication path for anything a sanitizer
could legitimately report turned up something no sanitizer can: `readPublishedFrame` copies the
4096-bin payload into the CALLER's vectors before it validates the bracket — it has to, the
validation is what the copy is checked against — and `paint` was reading straight into its drawing
buffers and discarding the result. A read the painter LOST therefore left it drawing a mixture of two
publications through the previous frame's rate: the exact incoherence ADR-0039 exists to prevent,
arriving through the reader instead of the writer. Three places said the opposite, including the ADR.

There is no race in it and no memory error, so ASan, UBSan and memcheck are silent by construction.
MEASURED on the shipped build: 4274 paints, **95 reads the painter lost, 44 of which had already
copied**. `paint` now stages into its own pair and commits with a swap only on success — two pointer
exchanges, 16 KB more, nothing else changed — and the contract is stated at `readPublishedFrame` so a
future caller cannot make the same assumption.

### 18.3 What the sanitizer legs do and do not establish

They establish no memory error and no UB on the paths executed. They say **nothing** about data
races: ASan, UBSan and memcheck are not race detectors, and the repository runs no ThreadSanitizer,
helgrind or DRD lane. Round 11's report implied otherwise; corrected here.

Asked properly: helgrind reports 16 "possible data race" hits on this publication. A twenty-line
control program containing nothing but a textbook lock-free seqlock over `std::atomic<float>` —
no product code at all — produces the same reports, because helgrind models pthread primitives and
not the C++11 memory model. **Helgrind is not an instrument for this code either way**, and that was
established by experiment rather than by citing the manual.

### 18.4 And the premise the whole family rests on, measured

`JUCE 9.0.1`'s `OpenGLContext::CachedImage::renderFrame` takes a
`MessageManager::Lock::ScopedTryLockType` before it paints components and releases it after;
`paintComponent` opens with `JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED`. So on that path the GL render
thread paints **under the message-manager lock**, mutually exclusive with the message thread — which
is why ADR-0027, ADR-0038 and ADR-0039 all say the race is argued rather than measured. It changes
none of them: the lock is a property of a vendored renderer that the pin can move, it is absent from
the other callers of `paint`, and "safe because something else holds a lock" is the reasoning this
tree refuses elsewhere. Recorded in `THREAD_MODEL.md` so the next reader does not mistake the
synchronisation for the only thing standing between the display and a race.

### 18.5 Tests and mutants

`specPaint` — a thread that PAINTS while the analyser publishes. Two tones at opposite ends of the
spectrum, one whole window per tick and `dt = 1 s`, so a coherent frame has exactly one marker bin
lit and a torn one has both or neither; `lit(low) == lit(high)` catches both tear directions.

| mutant | killed by |
|---|---|
| `paint` reads into its drawing buffers and ignores the result (the round-12 defect) | `specPaint` |
| the staged read committed regardless of the result | `specPaint` |
| *(re-run, all still killed)* the nine of §16.4 and §17.4 | as recorded there |

### 18.6 ADR-0039

**Accepted 2026-09-06** on the owner's explicit approval, recorded in the four places this
repository's process puts it plus the two clause amendments it widens. §17.2's "still open" is
superseded.

### 18.6b Three more the final diff review turned up, all closed here

* **`testTheSpectrumsPairSurvivesAProducerRunningDuringTheFrame` (round 9) had the same unguaranteed
  premise.** Its handshake exits at `pushed >= 4` and it then asserts `pushed > 4`, i.e. that the
  producer pushed again during a measured section containing no yield, no sleep and no syscall. It
  has never failed, and it is the same class as the two that did; snapshotted and waited for
  explicitly, bounded.
* **`HANDOVER.md`'s status-of-record still described the GR reveal as a "zero-`dt` tick … resolved to
  phase 0".** False in both halves since round 9: the reveal ticks with the measured hidden interval,
  and the code explicitly rejects phase 0 because it would move the trace right. Corrected.
* **ADR-0039's citations into `SpectrumView.cpp` were stale by ~139 lines** — the round that wrote
  them inserted that many lines above them in the same file. Re-anchored by SYMBOL, with the reason
  stated in place: a line anchor into a file the same change is still editing is a trap, and the
  review artefact is the worst place for one.

### 18.7 Follow-ups

1. There is no repository mechanism for classifying or retrying an environmental CI failure, and none
   is proposed here: the two failures were test defects, not infrastructure, and both are fixed.
2. Unchanged: the audio-path reservation index; the taps are index-aligned, not audio-time aligned;
   the reveal-smoothing limitation, whose premise this round did not touch.

## 19. The cross-configuration pair: one frame is one span AND one configuration (2026-09-07, round 13)

The review's finding, at `src/gui/SpectrumView.cpp:492`: *"When one ring's reset/reconfiguration
state becomes visible before the other ring's reset state, `tick` can still call `analyse` for both
windows … one trace is based on old-configuration / old-sample-rate data while the other trace is
already based on the new configuration."* It is real, it is constructible, and this section records
what it is, what it is not, and what was measured.

### 19.1 What the tick could observe, ordering by ordering

Six orderings, taken against the real `tick` (`gi0`, `go0`, `ci`, `co` are the four loads at the top
of the tick; `committed = min(ci, co)`).

| # | State | What the tick does |
|---|---|---|
| 1 | Neither reset observed | `resetIn`/`resetOut` both false. Either the idle gate returns, or an ordinary frame is drawn and published. Coherent. |
| 2 | Input observed, output not, `ci == 0` | `committed = 0` → `span = 0`, the reset-edge branch is skipped (`committed > 0` fails), both reads return nothing, `analyse`'s zero-length branch floors BOTH. Safe, and it is `min` that made it so. |
| 3 | Input observed, output not, `ci = R > 0` | **THE DEFECT.** A non-zero post-rewind count is a value released by `pushBlock`, and both rewinds happen-before that push (the audio-stopped premise at `ScopeBuffer::reset`), so `co` is forced post-reset too: both windows hold the NEW configuration's audio, `onePairOneSpan` accepts, and the pre-repair tick folded them into an `outDb` still holding the PREVIOUS configuration's EMA. |
| 4 | Output observed, input not | The mirror of 3, exactly. |
| 5 | Reset concurrent with the tick | If the rewind becomes visible inside `readEndingAt`, that ring's `e = min(end, w)` collapses and `onePairOneSpan` REJECTS — nothing published, nothing committed. If both copies finished first, the post-batch generation re-read is the guard, and pre-repair it was per ring. |
| 6 | Reset concurrent with `paint` | No effect. `paint` reads no processor state at all: it takes the pair through `readPublishedFrame`'s bracket into `stageIn`/`stageOut` and commits on success only, and it maps bins through `paintFrame.rate`. A reset during a paint is answered by the next tick. |

Row 3 is the whole finding, and the mechanism is narrower than "the rings disagree": **the ring
CONTENT is the new configuration's in both traces.** What is stale is the reader's own EMA — the
only state this view carries across a tick — for the ring whose rewind it did not observe.
`resetObserved`'s count term does not save it: the two counts are committed as the raw `ci`/`co` of
the last successful tick and can differ by a chunk, and in any case the count term is silent for a
ring that has refilled past its shown count.

### 19.2 Measured, before and after

A harness driving the real `AnabasisAudioProcessor` and the real `SpectrumView`, with the reader
holding one ring's reset and not the other across a genuine `prepareToPlay` from 48 kHz to 96 kHz.
Marker 5 kHz — bin 427 at the old rate, a frequency the new rate's mapping calls 10 kHz and the
audio has nothing at.

| Host block | Before (per-ring floor) | After (joint floor) |
|---|---|---|
| 512 | IN −120.0 dB, OUT −15.7 dB — **104.3 dB apart in one frame** | 3.1 dB |
| 4096 | IN −79.9 dB, OUT −10.9 dB — **69.0 dB apart** | 0.1 dB |
| 16384 | span 0, both floored — 0.0 dB | 0.0 dB |

The 3.1 dB and 0.1 dB that remain are the ordinary difference between a pre-chain and a post-chain
tap near the floor: the control run, with no reconfiguration at all, shows the same residual.

**How often it arrives on its own: not once.** A host thread alternating 48/96 kHz against a ticking
analyser reached 2006 re-prepares over four eight-second runs and produced no cross-configuration
frame either before or after the repair. Reaching row 3 needs the reader to hold one of `prepare`'s
two back-to-back rewinds and not the other, which is KI-018's propagation corner rather than an
ordinary interleaving. **The defect is constructible, not frequent**, and it is fixed because a frame
that mixes two configurations is wrong whenever it lands. Two earlier detectors were discarded before
this one: a peak-bin comparison found nothing (the EMA's instant attack moves both peaks together),
and a "worst bin disagreement > 20 dB" rule found 28 % — until a control run with ZERO
reconfigurations found the same, proving it was measuring the chain rather than the reset. The
detector of record is per-configuration marker tones with non-colliding harmonics and a relative
threshold, and its control reads 0 %.

### 19.3 The invariant, and the repair

**Every published frame represents one coherent audio span AND one coherent configuration
generation.** The two traces, the window, the rate and the generation belong to one configuration or
the frame is not published.

Two changes, both inside ADR-0039's existing mechanism:

1. **A reset observed on EITHER ring floors BOTH traces**, at the reset edge and at the post-batch
   generation re-read alike. Detection stays per ring — a rewind is a property of one ring's index,
   and `resetObserved`'s coherence argument is about that ring's modification order — and the
   CONSEQUENCE becomes the whole view's, because `AnabasisEngine::prepare` rewinds both rings back to
   back and unconditionally. Observing one is therefore proof the configuration changed, and proof
   the other trace's EMA describes the configuration that ended.
2. **The frame carries a configuration identity** (`Frame::config`, `pubConfig`), stored inside the
   same sequence bracket as the traces, the window and the rate. Not the GR ring's epoch, which
   stands still on a re-prepare at an unchanged (rate, block) pair while the rings still rewind; not
   either `ScopeBuffer` generation, because there are two and a frame needs one identity.

**What it promises, exactly:** two frames carrying the same id were produced with no reset observed
between them, so they describe one configuration. The converse is not promised — one reconfiguration
answered on one ring and then the other advances it twice — and that direction is the safe one. The
blank branch now commits RE-SAMPLED generations rather than the pre-count samples, which removes the
common cause of that double advance: marking a reset answered against a generation the ring never
had made the next tick answer it again.

**Split-reset behaviour, chosen and deterministic.** A tick that observes either reset floors both
EMAs and then publishes what the new configuration actually looks like: the analysed window if the
pair can serve one, and the EMPTY frame — the floor in both traces, a zero-length window, the new
rate and a new identity — if it cannot. Where no reset is involved and the span is 0, the last
coherent pair is HELD, which is the lapping case and unchanged. Nothing is fabricated, nothing waits,
nothing depends on repaint timing.

### 19.4 ADR-0039 is amended, not reopened

The approved protocol is sufficient: the defect was in the ANALYSIS stage, before publication, and no
part of the bracket, its ordering, its writer or its bound changes. But clause 10's second bullet
ratified the removed behaviour in as many words — *"The residual is bounded, not removed, and the
property claimed is the bounded one"* — so the change conflicts with an Accepted ADR, which
`CLAUDE.md` lists as a hard stop a green build does not clear. Filed as a dated by-exception
amendment to clauses 1 and 10, with a self-row in `ADR_INDEX.md`'s amendment registry. Clause 11's
trigger list ("a second payload site, a paint-path WRITE, or a second writer of `frameSeq`") is not
tripped by adding one scalar to the existing bracket.

A code comment 130 lines above the change said the opposite of the code — *"PER RING, and the scope
is deliberate … KI-018 carries it"* — and is rewritten. That drift is reported here rather than
quietly overwritten, per the source-of-truth rule.

### 19.5 Tests, and what they can and cannot reach

`specGen` (`testNoSpectrumFrameEverPairsTwoConfigurationGenerations`) is single-threaded and exact. It
settles both traces on one configuration, rewinds ONE ring and refills BOTH — which is the
reader-visible state of the propagation skew, since a rewind sends the producer back to slot 0 and
the frames it writes next overwrite exactly the slots the shared window reads. It asserts at the
PREVIOUS configuration's marker bin that the two traces agree, and agree that it is gone. Four cases
(old/old, new/new, input-new + output-old, input-old + output-new), both split directions, the reset
edge at a whole-ring block, one and eight blocks after a reset, and a 48 → 96 → 48 → 44.1 → 88.2 →
44.1 sweep.

`specStraddle` (`testAResetThatLandsInsideATickNeverReachesTheScreen`) covers the half no single
thread can reach: a rewind that becomes visible after the tick sampled the generations and before it
re-sampled them. Its producer is paced by the reader's own publications with a bounded spin, and the
rewind's landing point is swept with a delay drawn from a plain LCG rather than a clock. The
interleaving is OBSERVED, not assumed: a published frame with a full span and both traces entirely at
the floor can only come from the post-batch re-read, and the test requires such frames to exist
before it believes its own negative results.

### 19.6 Mutants

| Mutant | Result |
|---|---|
| Floor per ring at the reset edge (the defect itself) | **Killed** — `specGen`, both split directions, on both detectors |
| Floor per ring at the post-batch re-read | **Survived** — see below |
| Remove the post-batch generation guard entirely | **Killed** — `specStraddle` (the guard-floored frames vanish, and a trace holding two markers appears) |
| Publish the traces without a configuration identity | **Killed** — `specGen` case 2, and `specStraddle`'s identity-consistency check |
| Require BOTH rings' resets before flooring | **Killed** — `specGen`, both split directions |
| Require BOTH rings' resets before publishing the reset-edge blank | **Killed** — `specGen`'s whole-ring-block leg |
| `paint` reads the sample rate from the processor instead of the frame | **Killed** — `specAxis` |
| Commit the pre-count generation samples on the blank branch | **Survived** — see below |

**The two survivors, stated rather than hidden.** Flooring per ring at the POST-BATCH re-read is
indistinguishable from flooring jointly unless the reader's two generation re-reads straddle the
producer's two `reset()` calls — two nanosecond-scale windows nested inside each other. It is kept
joint for consistency with the edge and because the per-ring form is the shape the finding named, not
because a test can tell the difference. Committing the pre-count generation samples on the blank
branch likewise has no single-threaded consequence: it can only advance the identity twice for one
reconfiguration, which is the safe direction, and it cannot produce a mixed frame.

### 19.7 Follow-ups, unchanged

OQ-017's bursty-host lurch; the input/output taps being index-aligned rather than audio-time aligned;
KI-018's remaining equal-count corner; the audio-path reservation index; `LoudnessMeterView`; and the
spectrum reveal-smoothing limitation, whose premise this round did not touch. The memcheck job's
wall-clock cost is a CI-performance question and not a reason to change product behaviour.

---

## 20. The history's cadence: one entry is one PREPARED block of processed audio (2026-09-07, round 14)

**The instruction.** Implement OQ-017 **fix 1 only** — the mis-sized/variable-delivery half. The
bursty-host half stays an open follow-up and is not implemented here; no lag allowance, no guessed
`L`, no change to the rendering geometry, and nothing in the already-merged GR-history rendering
work is touched. The decision to implement was taken by the owner after §19's investigation, which
re-derived the two defects rather than accepting the three standing hypotheses.

### 20.1 The defect, in one line

`GrHistoryBuffer` was pushed **once per `processBlock` CALL** (from the wrapper), and
`GrHistoryView` maps entry k to `k · block / rate` and sizes its window as `20 s · rate / block`,
reading the **PREPARED** pair the ring publishes. The two agree only where the host delivers exactly
its declared maximum. JUCE's own `prepareToPlay` contract says it will not — *"completely variable
block sizes can be expected from some hosts"* — and the AU and VST3 wrappers both prepare with the
maximum and render with whatever the host passes, with no re-prepare on a change. So the whole time
base ran out by `B / D`, unbounded, measured from 0.125× to 8×.

Restated as the two figures a user sees, at 48 kHz with 512 prepared (measured on the real
processor; the pixel column is the same statement at the Simple well's 0.482372 px per entry and
60 frames a second, where the design travel is 45.222 px/s):

| delivered | entry rate before | window held before | entry rate after | window after |
|---|---|---|---|---|
| 128 | 375.00 /s (4× fast, 180.9 px/s) | 5.0 s | 93.75 /s | 20.0 s |
| 512 | 93.75 /s | 20.0 s | 93.75 /s | 20.0 s |
| 4096 | 11.72 /s (8× slow, 5.65 px/s) | 160.0 s | 93.75 /s | 20.0 s |

### 20.2 Why the producer, and why the ENGINE

The display cannot fix it without being told the delivered size, and telling it would mean either a
new published scalar (a second time base to keep coherent with the entries, which is the coupling
ADR-0038 exists to avoid) or a display that learns a number the host is free to change every
callback. The wrapper cannot fix it either: a prepared-block boundary of the PROCESSED stream falls
wherever the running total puts it — inside a delivered block whenever the host leaves a remainder —
and the two statistics an entry carries are folded **per sample** inside the chain.

The engine already chunks on exactly that grid. `AnabasisEngine::process` now takes the ring through
`setGrHistorySink`, breaks its chunk loop on `maxBlock - histSamples` as well as on `maxBlock`, and
pushes when the accumulator holds `maxBlock` samples, carrying the remainder across calls. A chunk
therefore lies **wholly inside one entry**, so the per-sample folds already in `processChunk` are
exact for the entry's own span: nothing is approximated, nothing is assigned wholesale, and a
delivered block spanning several entries is split into as many, each with its own statistics. The
`if (histSamples >= maxBlock)` is an `if` and not a `while` — the loop bound guarantees the
accumulator never holds more than one entry, so there is no unbounded catch-up.

Cost on the audio path: the per-sample folds moved from the per-CALL accumulators to per-CHUNK ones
and are folded chunk-into-call afterwards, which is exactly equal (min and max are associative) and
adds nothing per sample. `push` runs `⌈D / B⌉` times a call instead of once — **fewer** times than
before for any host delivering under the prepared size. No allocation, no lock, no new cross-thread
path, no new atomic ordering.

### 20.3 What is bit-identical

At **D == B** the entry the engine pushes is bit-identical to the pair the wrapper pushed before
this round — `gainToDecibels (lastBlockMinGain(), -60)` and `lastRenderPeak()` — asserted against
that expression itself rather than a remembered number
(`testGrHistoryEntriesFollowThePreparedBlock`, pass 6). The GR METER is untouched: it is a per-call
reading and stays one; only the history moved.

### 20.4 The measurements

Cadence, on the real processor with a real ring: exactly `rate / B` at 44.1 / 48 / 96 kHz, for
B = 512 and B = 1156 (the AU default), at D/B = 0.25, 0.5, 1, 2 and 8 — thirty configurations, and
the entry count equals `samples / B` in every one of them.

Splitting, the property that says the statistics belong to their own samples:

- Six delivery schedules — 64, 128, 512, 1024, 4096, and a variable one whose seven sizes
  (1, 3, 17, 63, 512, 1024, 1964) average the prepared block and none of which is a multiple of it —
  produce **bit-identical entry sequences**, with the limiter engaged.
- Each entry's peak equals the max of exactly the B rendered samples it spans, derived in closed
  form from the input and `groupDelaySamples()`, over all 48 entries of a run, with a negative
  control proving neighbouring entries differ.
- A 64-sample transient inside a 4096-sample delivered block lands in **exactly one** entry
  (`entries with peak > 0.1` = 1), at the index the prepared grid and the group delay put it at.
  Same at B = 256 with an 8192-sample delivery.

### 20.5 The finding that changed a comment: §5.4 sets its cadence from the DELIVERED size

The schedule-invariance above holds **frozen**. With the §5.4 trims live the six schedules part —
worst **0.0032 dB** of GR and **0.00035** linear of peak, first difference at entry 6 — and the
cause is not the accumulation: `adaptiveEngine.finishBlock` runs once per `process()` CALL and the
trims it produces are adopted for that whole call, so a host running 64 adapts eight times as often
as one running 512. That was true before this round and is untouched by it; it is now asserted as a
bound (pass 3b) rather than left to be discovered.

The same experiment re-measured a claim the first draft of this round's code comment carried over
from the investigation: that chunking is transparent to the audio. It is — **including with the
limiter engaged**, which the original measurement had not exercised — and the comment now rests on
that run rather than on one that never engaged it.

### 20.6 Reset and re-prepare

> **SUPERSEDED BY §21 (round 15).** The paragraph below describes what round 14 shipped. The PR
> review found it wrong for the same-configuration case, and the partial now follows the ring's own
> clear-on-change gate. Kept as written because the reasoning it records is the reasoning that had
> to be corrected.

The accumulator is dropped by `prepare` and by `reset`, never carried across either: a re-prepare is
a discontinuity in the audio the entries describe, so at most `maxBlock - 1` samples of a partial
entry are discarded rather than being spliced onto audio from the other side of it — less than one
entry of a twenty-second window. The ring's own clear-on-change gate is unchanged, so a re-prepare
at the same pair still keeps the timeline; pass 7 asserts that the first entry after such a
re-prepare carries none of the audio from before it.

### 20.7 What is untouched

Limiter behaviour, gain and peak calculation, the chain's signal order, the FFT/spectrum path, the
GR bucket geometry, bucket values once in the ring, `bucketX`, `visibleRight`, the smoothed head and
its band, the reader contract, `push`, the epoch protocol, the prepared-pair metadata, and every
merged GR-history and Spectrum fix. `ScopeBuffer`'s two rings are untouched — they are pushed per
chunk already and their reader reads frames, not a time series.

### 20.7b Mutation testing, and the two mutants that survived the first suite

Nine mutants of the new code, each built and run against the DSP suite:

| # | mutant | outcome |
|---|---|---|
| 1 | chunk loop stops breaking on the history boundary | killed (cadence, split, §5.4 bound) |
| 2 | `histSamples >= maxBlock` → `>` (the entry never closes) | killed (eight checks) |
| 3 | the peak accumulator is not cleared after a push | killed |
| 4 | the gain accumulator is not cleared after a push | killed |
| 5 | `histPeak = renderPeakChunk` (last chunk wins) | killed |
| 6 | `histMinGain = grMinChunk` (last chunk wins) | killed |
| 7 | the per-chunk minima are not reset at the top of a chunk | **survived**, then killed |
| 8 | `prepare`/`reset` no longer clear the accumulator | **survived**, then killed |
| 9 | the push is unconditional (one entry per chunk) | killed |

**Mutant 7 is the interesting one.** Without the per-chunk reset, `grMinChunk` becomes a running
GLOBAL minimum — and that is still schedule-invariant, because the entry boundaries fall at the same
absolute samples in every schedule, and it still agrees with `lastBlockMinGain()`, because the
per-call fold reads the same running value. Cadence, split and identity all stayed green while the
GR trace would have latched at the session's deepest reduction and never recovered. The pass added
for it feeds a loud passage and then silence: with the reset deleted the tail reads −3.62 dB, the
same figure as the passage; with it, better than −0.02 dB.

**Mutant 8 exposed a test that passed for the wrong reason.** `process` works IN PLACE, so the
re-prepare pass — which reused one buffer across its calls — was feeding the engine its own delayed
output, and three calls of that is digital silence: a partial entry carrying loud audio across a
re-prepare would have been indistinguishable from one carrying nothing. The pass now refills before
every call and asserts the loud render IS in the accumulator (`lastRenderPeak() > 0.5`) before the
re-prepare is asked to drop it.

**One code change came out of the same exercise.** Mutant 2 did not fail the suite, it HUNG it: with
the accumulator never emptied, `maxBlock - histSamples` goes non-positive and `start += num` stops
advancing — an infinite loop on the audio thread. The shipped code cannot reach that state, but the
loop's termination rested on an invariant held elsewhere rather than on anything visible at the loop
itself, so `num` is now `jmin (jmax (1, maxBlock - histSamples), totalSamples - start)`: identical on
every reachable state, and terminating by inspection on all of them.

### 20.8 Follow-ups, unchanged

**OQ-017's bursty half stays Open** and this round is not a partial answer to it: the fix changes
WHICH SAMPLES an entry stands for, and a burst delivers the same entries at the wrong INSTANTS,
which no producer-side change can address. Also unchanged: the input/output taps being index-aligned
rather than audio-time aligned; KI-018's remaining equal-count corner; the audio-path reservation
index; `LoudnessMeterView`; and the spectrum reveal-smoothing limitation.

---

## 21. The partial entry follows the ring, not the call (2026-09-07, round 15)

**The PR review's blocking finding**, at the line round 14 had written: *"Pause-resume drops recent
history. On each same-configuration re-prepare, `prepare` clears a partial history entry while the
history ring preserves earlier completed entries."*

### 21.1 The root cause, and why it is not the one the comment claimed

Round 14 gave the engine an accumulator and dropped it on every `prepare`, arguing that *"a
re-prepare is a break in the audio an entry describes — a transport stop, a rate change, a
rescan"*. That sentence puts a transport stop and a rate change in the same class, and the product
does not: `GrHistoryBuffer::prepare` keeps the ring's entries when the `(rate, block)` pair is
unchanged, and it does so under an **Accepted ADR** — ADR-0023 item 6, *"a transport-start
re-prepare keeps the timeline"* — restated to users in `USER_MANUAL.md` as *"pausing and resuming
continues the timeline; it restarts only when the sample rate or block size changes."* So the ring
and the accumulator were following opposite rules across the one event hosts generate most often,
and the samples in the unpublished entry fell between them.

Nothing in the record reconciled the two. ADR-0011's 2026-09-07 amendment, the only Accepted-ADR
record of round 14, is silent on the accumulator's lifecycle across a prepare; ADR-0023 item 6 and
its amendments never mention the accumulator. The drop was documented in four code comments, §20.6
above, `TESTING.md` and one test pass — so it was deliberate rather than accidental — but it was
never checked against the contract it contradicts, and round 14's own mutation set hunted the
opposite behaviour (mutant 8) and killed it.

### 21.2 The state that was discarded

Three plain members of `AnabasisEngine`: `histMinGain`, `histPeak` (the two statistics an entry
carries, folded over the samples collected so far) and `histSamples` (how many that is, always
strictly less than `maxBlock`). They describe audio the plugin had **already rendered and already
emitted to the host**; only their publication was outstanding. Everything else `reset()` clears is
either audio memory or an edge detector — this was the only piece of *publication* state on that
list.

### 21.3 Measured, on the real engine and a real ring

Marker material: a 64-sample burst at 0.85 against a 0.20 base, fed early enough that its RENDER
falls ten samples inside the partial. 48 kHz, 512 prepared, four blocks primed, a 300-sample partial,
then the break.

| break | before | after |
|---|---|---|
| none (control) | marker in entry 4, peak 0.850 | same |
| same pair | **marker in no entry at all** | marker in entry 4, peak 0.850 |
| rate 48 → 96 kHz | marker dropped | marker dropped |
| block 512 → 256 | marker dropped | marker dropped |
| explicit `reset()` | marker dropped | marker dropped |

Cumulatively, forty cycles of twenty 512-sample blocks followed by a 300-sample one — 421 600
samples, **8.783 s** at 48 kHz / 512, with a same-configuration re-prepare at the end of each:
**800 entries before, 823 after, against the 823 the audio is worth** — a quarter of a second of
history that reached no entry, and it accumulates with every transport start.

The per-event loss is exactly `histSamples` at the moment of the call, so 0 … `maxBlock − 1`
samples: 511 at 48 kHz / 512 (10.6 ms), 1155 at Logic's 44.1 kHz / 1156 (26.2 ms). It is not a
per-pause constant — the drop re-phases the entry grid to the resume point, so the total after k
re-prepares is Σ (Sᵢ − Sᵢ₋₁) mod B, and pauses spaced an exact multiple of B apart cost nothing
after the first.

### 21.4 The invariant, and the mechanism

> For one `(rate, block)` timeline, every processed sample either reaches exactly one history entry
> or is discarded by the same event that clears the ring. There is no third case.

`AnabasisEngine::prepare` now asks one question first, before `sr` and `maxBlock` are overwritten:
does this prepare END the timeline or CONTINUE it? It answers with the same comparison, on the same
two RAW values, that `GrHistoryBuffer::prepare` makes sixteen lines later in `prepareToPlay` — for
which the engine keeps a second pair, `preparedRateRaw`/`preparedBlockRaw`, rather than its railed
`sr`/`maxBlock` copies, so the two predicates are provably the same function and not merely equal in
practice. The accumulator is captured before the body runs and restored after `reset()`, which is
where it has to go: `reset()` is `prepare`'s own last statement and zeroes the same three members, so
gating only the in-prepare clear would have been a no-op — round 14 cleared the accumulator twice per
prepare.

The engine's raw pair is committed as the **last** statement of `prepare`, not beside the comparison
at the top, and that is exception safety rather than style: this function allocates eight
oversamplers and half a dozen buffers, and the rail at the top of it exists because a `bad_alloc`
here crosses the wrapper's C ABI. Commit the pair first and a prepare that throws leaves the engine
holding the new pair while the ring — whose own `prepare`, sixteen lines later in `prepareToPlay`,
never ran — still holds the old one; the host's retry would then have the engine answer "same" and
carry while the ring answered "changed" and cleared. Committed last, a throw leaves the pair OLD and
the retry answers "changed" on both sides.

Nothing else changes. No entry is published early or short — the entry still completes at exactly
`maxBlock` samples, so the display's time base is untouched. Nothing is re-processed, so no sample is
counted twice. A same-pair `GrHistoryBuffer::prepare` is a **total no-op** — it returns before
touching `writeIndex`, `resetGuard`, the stored pair or any slot — so the carried entry is published
under the same epoch, at the next monotonic index, through the same `push`; the reader cannot
distinguish it from any other entry. On the common path the host thread now touches the accumulator
**zero** times, where round 14 wrote it twice per prepare.

**The realtime claim, as the bound it is rather than as "unchanged".** A call that carried samples in
can publish an entry where the prepare would previously have discarded them, so the count on an
individual call can differ. What is unchanged is what a realtime argument can use: the per-call bound
stays `ceil(delivered / prepared)` pushes — the carry is strictly less than one entry, so it cannot
add one to the ceiling — the long-run rate stays `rate / preparedBlock`, and chunks per call stay
`ceil((carried + delivered) / prepared)`, which is round 14's figure, since the remainder was already
carried across calls. No allocation, no lock, no atomic, nothing per sample.

### 21.5 The one residual, stated

The first entry after a resume needs only `maxBlock − carried` new samples, so it is published up to
`(maxBlock − 1) / rate` seconds early — 10.6 ms at 48 kHz / 512. `smoothedHead` holds its estimate to
`[head, head + 1]`, so an early head advances the trace by at most **one entry pitch in a single
frame** (0.482 px on the Simple well at that configuration), once per resume. That is the magnitude
of the two-block burst OQ-017 already records as accepted, it is bounded, and it replaces a loss of
content that was permanent and cumulative.

### 21.6 A changed configuration still drops it — and `reset()` stopped being a second door

A rate or block change is where the ring clears, so a carried partial would put audio recorded under
the old time base into the new timeline's first entry. It is dropped there, and the changed-pair half
of pass 7 pins that an entry of the new size is never part-filled with samples counted against the
old one.

**`reset()` no longer clears the accumulator, and the evidence for changing that is the same evidence
the rest of this round rests on.** The first draft of this repair left `reset()`'s clear alone and
restored the accumulator after it, on the principle that reset semantics should not move without
cause. Adversarial review found the cause: `reset()` touches **no ring state whatsoever** — no epoch,
no write index, no slot, no prepared pair — so a reset that dropped the partial would take up to
`maxBlock − 1` samples out of a timeline the ring is still keeping, with no guard able to fire
because nothing in the ring moved. That is this round's defect exactly, at a different door, and the
first draft's own test asserted it as correct. The accumulator is not audio memory and not an edge
detector — it is the only piece of PUBLICATION state on `reset()`'s list — so it does not belong
there. It now has ONE writer outside the chunk loop, `prepare`'s changed-pair branch, whose rule is
literally the ring's, instead of two writers sixty lines apart that contradicted each other. (Round
14 cleared it twice per prepare, inline and again inside `reset()`; that is also why the review
finding's implied one-line remedy — gate the inline clear — would have been a no-op.)

Nothing a host can observe changes: `AnabasisAudioProcessor` does not override
`AudioProcessor::reset()`, so `prepare`'s own tail is that function's only caller in the tree. The
rule is pinned for whoever gives it a second one.

### 21.7 Mutation testing

Nine mutants, built and run against the DSP suite:

| mutant | failures |
|---|---|
| always drop (round 14's behaviour) | 12 |
| never drop, even on a configuration change | 4 |
| the stored raw pair never advances | 12 |
| drop the count but not the statistics | 3 |
| drop the statistics but not the count | 1 |
| ignore the rate in the gate | 3 |
| ignore the block in the gate | 2 |
| put the clear back into `reset()` | 14 |
| compare the RAILED copies instead of the raw pair | **0 — survives** |

The "statistics but not the count" mutant is the one that needed a test written for it: dropping only
`histMinGain`/`histPeak` across a 512 → 256 change leaves `histSamples` at 300, which already exceeds
the new entry size, so the very first sample of the new timeline closes an entry standing for one
sample. The marker checks cannot see that — the statistics WERE cleared — so the pass now feeds 255
samples of the new configuration and requires the ring to be empty, then one more and requires it not
to be.

The survivor survives for a reason worth recording rather than papering over. Comparing the RAILED
copies (`sr`, `maxBlock`) instead of the raw arguments differs only where `jmax (1, maxBlockSize)`
collapses distinct arguments — block sizes of 0, 1 or negative — and at an effective block of 1 every
sample completes an entry, so the accumulator is always empty and there is nothing to carry either
way. The raw pair is kept because it makes the engine's predicate provably the ring's, not because a
test can tell them apart.

### 21.8 The stale cadence contract (the review's second, non-blocking finding)

`GrHistoryBuffer::push` still promised *"once per HOST BLOCK, since `push` runs once per
`processBlock` and never per sample"* — true until round 14 and not since. The corrected wording
states the unit (per published entry), what a call actually publishes
(`floor((carried + delivered) / prepared)` — none, one, or several), what is guaranteed (the
long-run rate `rate / block`, and the bound of `ceil(delivered / prepared)` calls per host block,
never per sample), and leaves the measured x86-64/AArch64 figures attached to the call rather than
to a unit they no longer describe. The same claim was corrected in the ring's file banner, in
`THREADING_POLICY.md`'s Audio → GUI row, and in `PluginProcessor.cpp`'s `prepareToPlay` comment,
which still described the ring's time base as entries-per-host-block.

### 21.9 Follow-ups, not taken here

`GrHistoryBuffer::reset()` has no production caller, yet several comments attribute the production
rewind to it rather than to `prepare` → `clear`; and `latchOsConfig` tears down the wet ring, the
limiter and the oversampler mid-stream without touching the accumulator, which is consistent with
this round's rule but was inconsistent with round 14's. Both are wording/consistency items, not
defects. OQ-017's bursty half is untouched.
