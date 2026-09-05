# Worklog — 0.2.11: the GR history's newest vertex (2026-09-04/05)

Session-local evidence trail for version 0.2.11. Raw investigation material, NOT architecture
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
