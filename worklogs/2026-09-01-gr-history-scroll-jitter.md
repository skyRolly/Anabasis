# Worklog — 0.2.8: the GR history's scroll jitter (2026-09-01)

Session-local evidence trail for version 0.2.8. Raw investigation material, NOT architecture
documentation — `docs/SOURCE_OF_TRUTH.md`: worklogs are never cited as policy. What is binding is
`CHANGELOG.md`, the code and the tests; this file carries the measurements and the alternatives
that were rejected, because a decision without its rejected alternatives is not reviewable.

Owner report, verbatim:

> *"The newly drawn portion of the GR history to the right of the yellow line is jittery."*

---

## 1 — What the yellow line is

There is no vertical marker anywhere in `GrHistoryView`. The only yellow the well draws is
`colours::accent` (`0xfff0b432`, `src/gui/LookAndFeel.h`), and inside the view it is used for two
things: the GR trace's stroke (`GrHistoryView.cpp`, the `strokePath (gr, PathStrokeType (1.4f))`
at the end of `paintHistory`) and the GR|SPEC pill's lit segment. The "yellow line" is therefore
the trace itself where it is FLAT: zero reduction maps to `grY (0)` = the top of the plot area,
so a run of no reduction is a horizontal gold line — and while the ring is still filling, the
unmeasured region left of the oldest bucket is drawn as exactly that line, ending in a vertical
drop to bucket 0's value (the `drawsZeroRegion` branch, the "honest boundary" the 0.1.2 comments
describe). Under either reading the part to its RIGHT is the sloped, measured, scrolling trace.

That localisation is itself the diagnosis. A horizontal segment translated horizontally by any
amount rasterises to the same pixels, so the flat part cannot exhibit horizontal jitter; only the
sloped part can. The report is not describing a region of the panel, it is describing the one
class of segment on which the renderer's motion is visible.

The independent readers of the code (§7) arrived at the same referent without seeing this note.

## 2 — The mechanism, read off the code

`GrHistoryView::bucketX` at 0.2.7:

```cpp
return x0 + (width - 1.0f) - (float) (b.kHead - k) * ((width - 1.0f) / (float) (b.kFull - 1));
```

with `kHead = (head − 1) / stride` (integer division). `head` — the only time-varying input —
enters through the integer division alone, so between two repaints every vertex of both paths
moves by `−(ΔkHead) · pitch`: **nothing** for `stride − 1` consecutive blocks, then **one whole
pitch**. The pitch is `(width − 1) / (kFull − 1)` and is not an integer:

| Configuration (`cols` = plot width) | `want` | `stride` | `kFull` | pitch | steps/s |
|---|---|---|---|---|---|
| 48 kHz / 512, Simple well (904) | 1875 | 3 | 625 | **1.447 px** | 31.2 |
| 48 kHz / 512, Advanced well (604) | 1875 | 4 | 469 | 1.288 px | 23.4 |
| 48 kHz / 256, Simple | 3750 | 5 | 750 | 1.206 px | 37.5 |
| 48 kHz / 1024, Simple | 938 | 2 | 469 | 1.929 px | 23.4 |
| 44.1 kHz / 1024, Simple | 862 | **1** | 862 | 1.049 px | 43.1 |
| 48 kHz / 2048, Simple | 469 | **1** | 469 | 1.929 px | 23.4 |
| 192 kHz / 32, Advanced (ring-saturated) | 4095 | 7 | 585 | 1.033 px | 857 |

Two visible consequences, both confined to the sloped part of the trace:

1. **Stop–go motion.** At a 60 Hz display and 31 steps/s the trace stands still for one or two
   frames and then jumps 1.45 px — a judder, not a scroll.
2. **Re-rasterisation at every step.** A 1.447 px step cycles each vertex's fractional x through
   0 → .45 → .89 → .34 …, so the anti-aliased 1.4 px stroke renders every sloped segment with a
   different coverage pattern at every step: a shimmer. This is the same *class* of artefact the
   0.1.1 round fixed (there it was value re-bucketing; here it is geometry), and it entered the
   tree with 0.1.2's fixed-pitch rewrite — the 0.1.1 form used one bucket per column, an integer
   pitch, which is why 0.1.1's fix looked complete at the time.

A secondary mechanism at the very tip: the newest vertex aggregated `[kHead·stride, head)`, the
newest bucket's PARTIAL range, so the frame a bucket began it held one block's min/max and then
deepened as the bucket filled — a bucket-rate pop of the last ≈1.4 px of the trace.

## 3 — Measurement: a frame-by-frame model

`worklogs/` carries no rendered frames, so the motion was modelled rather than eyeballed:
`bucketX` and `buckets` re-implemented in Python (the script is reproduced in §8), entries pushed
at the host block rate with a synthetic per-block GR (slow envelope ± 2 dB block-rate variance,
roughly what a limiter on dense programme publishes), sampled at a 60 Hz vblank for 6 s, tracking
one completed bucket's x from frame to frame and the newest vertex's value. Machine: this Linux
container; the model is deterministic (seeded) and needs no hardware.

**Per-frame horizontal displacement of one completed bucket** (px; negative = leftward; "0 %" =
fraction of frames in which the vertex did not move at all):

| Configuration | 0.2.7 stepped: mean / σ / motionless | per-entry (A) alone: σ / motionless | A + since-arrival phase (first draft): σ / motionless | A + rate-smoothed head (C, shipped): σ / motionless |
|---|---|---|---|---|
| 48k/512 Simple | −0.75 / **0.72** / **48 %** | 0.24 / 0 % | 0.24 / 0 % | **0.000** / 0 % |
| 48k/512 Advanced | −0.50 / 0.63 / 61 % | 0.16 / 0 % | 0.16 / 0 % | 0.000 / 0 % |
| 48k/256 Simple | −0.75 / 0.58 / 38 % | 0.08 / 0 % | 0.08 / 0 % | 0.000 / 0 % |
| 48k/1024 Simple | −0.75 / 0.94 / 61 % | 0.40 / 22 % | 0.30 / 0 % | 0.000 / 0 % |
| 44.1k/1024 Simple (stride 1) | −0.75 / 0.47 / 28 % | 0.47 / 28 % (unchanged) | 0.31 / 0 % | 0.000 / 0 % |
| 48k/2048 Simple (stride 1) | −0.75 / 0.94 / 61 % | 0.94 / 61 % (unchanged) | 0.23 / 0 % | 0.000 / 0 % |
| 192k/32 Advanced (saturated) † | −14.75 / 0.47 / 0 % | 0.02 / 0 % | 0.02 / 0 % | 0.000 / 0 % |

† 100 entries per 60 Hz frame: the smoothed head is at its cap on every frame and the trace moves
14.75 px per frame whatever the geometry does. The row is kept because the case pins the ring
clamp, not because any of its columns is visible; the review (§7) is right that it measures nothing
the eye can see.

The mean is the same in every column — the trace covers the same distance in the same time; what
changes is how the distance is dealt out over frames. The first draft of part C read the phase as
"seconds since the tick that first saw the head move", and the rasterisation reviewer (§7) found
what the third column shows: at 48 kHz/512 the block rate (93.75 Hz) exceeds a 60 Hz display, so
every frame sees an entry, the phase is reset to 0 on every frame and never ramps, and the display
still steps by whichever of 1 or 2 entries the frame happened to catch — a 2:1 velocity beat at
33.75 Hz, σ 0.24 px. The shipped form advances a SMOOTHED head at the nominal entry rate and holds
it to `[head, head + 1]`; on a steady host that is one uniform step per frame at every block size
(σ 0.000 in every row, with the model's ideal timing), and the clamp is what keeps it honest when
the host is not steady (§4 C).

**Newest vertex, mean |Δ value| between consecutive frames** (dB): partial-bucket aggregate
**0.99** (max 3.9) → trailing-window aggregate **0.55** (max 3.4) at 48k/512 Simple; 0.85 → 0.37
Advanced; 1.01 → 0.48 at 256.

## 4 — The fix

Three parts, each with its own measurement above and its own assertions in
`testGrHistoryWindowNeverAsksForTheHeadSlot`:

**A. Scrolled by the entry.** `Buckets` gains `fill` (entries the newest bucket holds, 1…stride).
For a completed bucket `k < kHead`:

```
x(k) = right − (kHead − k)·pitch + (stride − fill − phase)·pitch/stride
```

The `(stride − fill)` term holds the trace one entry-pitch to the right per entry the newest
bucket has yet to collect and runs down to zero as it fills, so each completed vertex sits exactly
one entry-pitch further left for every entry pushed, continuously across bucket boundaries
(bucket `k`'s x is a function of `head` alone). Adjacent completed buckets stay exactly one pitch
apart; the newest bucket's own segment grows from one entry-pitch to one pitch; the newest vertex
holds the right edge. Bucket identity and every completed bucket's value are untouched — the
0.1.1 property.

**B. Trailing window for the tip.** `tipFirst (first, head, stride) = max (first, head − stride)`:
the newest vertex aggregates the last `stride` entries, the same filter length as every completed
bucket. The two ranges coincide at the instant the bucket completes, so the completed value takes
over without a step.

**C. Sub-entry phase from a rate-smoothed head.** With `stride == 1` A is a no-op — one block IS
one bucket — and that is the configuration of a 1024-sample buffer at 44.1 kHz or 2048 at 48 kHz
on the Simple well (the table in §2); and even at `stride ≥ 2` a frame catches 1 or 2 entries
depending on the beat between block rate and display rate, so per-entry motion alone is uniform
per ENTRY, not per FRAME. The view already maps the window through the prepared (rate, block)
pair (`windowEntries`); `entryPeriod` is that pair's block period, and `tick` keeps a smoothed
head advanced by `dt / period` every frame and held to `[head, head + 1]` (`smoothedHead`):

- **never behind the data** — an entry the estimate did not expect snaps it forward to `head`;
- **never more than one entry ahead** — a stopped transport, or a host delivering blocks larger
  than it prepared, parks it at `head + 1` instead of letting the trace run ahead of the data;
- **a rewound head** (`GrHistoryBuffer::reset()`, the only way the previous value can exceed the
  cap) re-anchors at `head`, phase 0.

`phaseOf` is its fraction past the real head. Every completed vertex sits a further `phase`
entry-pitches left; the newest vertex joins the drift only while its bucket is COMPLETE and waiting
(a filling bucket's vertex stays pinned to the edge and its segment stretches instead), and the
strip it vacates is drawn flat at its value — a lead-out, the mirror of 0.1.3's left-edge lead-in.

Monotonicity is by construction: every vertex's display position is `−(head + phase)` entry-pitches
from a fixed origin, `head + phase` IS the smoothed head, and the smoothed head never decreases (it
grows at the nominal rate, or is clamped UP to `head`; the cap only ever limits growth). The pure
form is also asserted: `x(k | head, phase 1) == x(k | head + 1, phase 0)`. The ring's recorded
time-base caveat is unchanged, and the review (§7 item 4) made its consequence precise: a host whose
cadence differs from the prepared pair degrades the MOTION to per-entry stepping at the host's own
cadence — longer-than-prepared blocks run the trace one period after each arrival and park it for
the excess, shorter ones keep the lower clamp biting — never behind the data and never more than
one entry ahead of it. Bursty delivery (several blocks per callback) is the same case at the burst
rate, measured in §7 item 5, where the lag-buffer design that would absorb it is recorded as the
owner's latency trade rather than taken.

The frame draws the head the tick computed the phase FOR (`paintHead`), not whatever `available()`
reads at paint time: where the deferred paint runs after the vblank tick an entry can land between
the two, and drawing the live head at the smoothed head's phase put that frame one sub-entry step
ahead of the ramp (§7 item 3). Entries below the tick's head are published by the ring's own
contract, so the older head is always safe to draw; the live head is used before the first tick and
on a rewind.

`tick` repaints whenever the smoothed head moved or a new entry arrived, and skips the frame once
the head is PARKED (no new entry and the estimate at the cap), so the pre-0.2.8 idle gate is back in
force after at most one entry period of extra frames following the last block.

`paintHistory` clips to the plot area's columns, so a vertex on or beyond the left edge draws its
crossing segment exactly. Cost: 0.7 px of stroke end-cap that used to spill into the 10 px margin.

**D. The left edge: a bucket-aligned read window.** The first draft kept the 0.1.2–0.2.7 rule
that the oldest drawn bucket lies WHOLLY inside the 20 s window, with 0.1.3's flat lead-in behind
it, and claimed the lead-in "now varies continuously". The review (§7 item 1) showed it does not:
the oldest vertex walks left by an entry-pitch per entry and then jumps a whole pitch RIGHT as its
bucket expires and `kFirst` steps on, the vacated strip flipping from a sloped segment to a flat
run — a bucket-rate pop at the left edge, the one discontinuity the fix had left on the panel. The
draft's reason for not drawing the partly expired bucket (that at the saturated window its oldest
entries alias the write slot) was unsound: the paint loop clamps every bucket's read range to
`first`, so no frame peeks below `head − (kSize − 1)` whatever `kFirst` is.

Shipped: `Buckets::window` is `want` rounded UP to `kFull` whole buckets, capped at `kSize − 1`
(at most `stride − 1` entries older than the 20 s, all off-panel or inside the crossing segment),
`first = max (0, head − window)`, and `kFirst = max (min (kHead, first / stride), kHead − kFull)`
— the bucket HOLDING `first` (partly expired; its in-window remainder is what it aggregates, a min
over a shrinking set, so its off-panel vertex drifts monotonically rather than popping) and no
earlier than the bucket just OFF the panel's left edge. With the window aligned the oldest drawn
vertex sits on or beyond the edge at every head and phase (`x0 − pitch ≤ x ≤ x0`, asserted at
phase 0 and 1), the next vertex is inside, and the clip renders the crossing segment exactly. The
lead-in code path is reached only when `kFull · stride` would exceed the ring cap — a saturated
window whose stride does not divide `kSize − 1` — which neither of this product's plot widths
produces (904 → stride 5, 604 → stride 7, both divide 4095); it is kept, commented as the
fallback, because a plot width is a layout constant and not a contract. The filling case's zero
region starts its paths whatever `xFirst` is, so the honest-boundary bar slides out under the clip
over the last frames of the fill instead of vanishing whole (§7 item 2).

## 5 — Alternatives rejected

| Option | Verdict |
|---|---|
| **Round every vertex to whole pixels** | Rejected. Removes the re-rasterisation shimmer but keeps — worsens — the stop-go: at 1.447 px per bucket the steps become 1, 1, 2, 1, 1, 2 px at bucket rate. Sub-pixel continuous motion is what a scrolling display needs; the eye tracks it as motion, not as sharpness. |
| **One bucket per pixel column** (the 0.1.1 form) | Rejected — that is what 0.1.2 removed, for the recorded reason: the window holds `want` entries, not `cols·stride`, so it left the left third of the panel blank, and it re-introduces the zoom-while-filling the owner's 0.1.2 directive forbids. |
| **A and B without C** | Measured (the second column of §3): complete for every configuration with `stride ≥ 2` as far as per-ENTRY motion goes, exactly as bad as 0.2.7 at `stride == 1` (the 44.1 kHz/1024 Simple-well case — a common setting), and still a 1-or-2-entries-per-frame velocity beat everywhere the block rate is not a multiple of the display rate. C is what makes the fix hold at every block size the product supports, and it costs one message-thread `double`, a bounded repaint ramp and three pure statics. |
| **C as "seconds since the tick that first saw the head move"** (the first draft, committed to this branch before review) | Rejected on the rasterisation reviewer's finding, then measured (third column of §3): wherever the block rate exceeds the display rate — 48 kHz/512 on any ≤ 93 Hz display — every frame sees an entry, the phase resets to 0 on every frame and never ramps, and the display steps by 1 or 2 entries as the beat dictates (σ 0.24 px at 512, 0.30 at 1024). A smoothed head advanced at the nominal rate makes the step uniform per frame regardless of how many entries the frame caught, and its `[head, head + 1]` clamp carries every guarantee the draft had (never ahead of the data, parks on a stall) plus one it lacked (never BEHIND the data). |
| **Interpolate the phase from wall-clock timestamps stored in the ring** | Rejected. Puts a clock read on the audio thread and a second field in the SPSC entry for a display refinement; the prepared pair the view already trusts for the window is the same time base at finer grain, and C's clamp bounds the error to one entry (never worse than A) where the host's cadence differs. |
| **Keep the "wholly inside the window" bound and the flat lead-in** (the first draft) | Rejected on the review's finding (§7 item 1): the lead-in's length was a sawtooth at bucket rate, not the continuous ramp the draft claimed, and the draft's aliasing argument against drawing the crossing bucket was wrong — the read clamp to `first` already makes any `kFirst` ring-safe. Replaced by the bucket-aligned read window (§4 D). |
| **Absorb bursty host delivery with a lagging smoothed head** (`[head − L, head + 1]`, §7 item 5) | Not taken. It is the right instrument for hosts that render ahead of real time, and it costs `L` entries of display latency plus a catch-up rule — a trade with no value that fits every host (REAPER renders hundreds of milliseconds ahead; Logic's process buffer is not bursty at all). That is the owner's call; the shipped form is never worse than per-entry stepping at the burst rate, and the design is recorded here and in the header banner for when it is made. |
| **Keep the tip aggregating its partial bucket** (B not taken) | Rejected — measured: 0.99 dB mean frame-to-frame movement at the tip, at bucket rate, is a pop on the pixel column the owner watches most. |
| **Pin the newest vertex to the edge even while it waits on the phase** | Rejected. The frame a new bucket begins, the old newest vertex would then jump from the edge to one entry-pitch left of it — sub-pixel at `stride ≥ 2`, but a full pitch (1.93 px) at `stride == 1`, exactly where C matters. Letting it drift with a flat lead-out keeps every vertex continuous. |
| **Let the newest vertex drift with the phase while its bucket is still filling** | Rejected: each entry inside a bucket would snap it back to the edge — a rightward motion of up to one entry-pitch at block rate. Pinning it while filling (its segment stretches) and drifting only once complete gives monotone motion for every vertex. |
| **Add an axis, a "now" marker, or any UI text explaining the change** | Rejected, `AI_AGENT_POLICY.md` C8: interface furniture is the maintainer's to specify. The manual gains one sentence. |

## 6 — Verification

Suites, this container, Release: **`AnabasisTests` 301 + `AnabasisStateTests` 944 = 1245**, 0
failures, under **clang-22.1.8** and **g++ 13.3** (the local GCC; CI's GCC arm is 16.2 in the
`gcc:16` container). (920 after the first draft; 925 with the smoothed head's `grPhase` block; 944
with the review round's sixth geometry case, the phase-1 left-edge sample, `parked` and
`paintHead`.) Zero first-party warnings under the full clang warning gate
(`check-clang-warnings.py`) — the two `-Wfloat-equal` diagnostics the first draft of the test
raised were removed before the gate ran, and are recorded here because the gate is the reason the
draft could not ship as written. Docs 101 files clean, portability 48/0, realtime 40/0, citations
54 anchors against `origin/main`, `git diff --check` clean.

**Which existing pins the geometry change touched, measured before the tests were edited:** the
0.2.7 test binary rebuilt against the new `bucketX` fails exactly ONE of its 873 checks — the
quarter-full case at 192 kHz/32, whose `head = want / 4 = 1023` is not a stride boundary (1023 =
146·7 + 1), so the old assertion that the newest and the next-newest bucket are one pitch apart
reads the newest bucket's one-entry segment. The other four cases put `want / 4` on a boundary and
passed by coincidence; the assertion now reads the pitch between two COMPLETED buckets, which holds
at every fill. (Measured against the first draft's geometry; the aligned window of §4 D changes
the left-edge pins deliberately, and those are re-stated rather than found broken.)

**What is NOT pinned, said outright** (the review's §7 item 7): the column clip, the lead-out and
the tick → `smoothHead` → paint composition are exercised only by a running frame clock, which the
headless suites cannot supply (`FrameClock` rides a `VBlankAttachment`); the rules they compose
are each pinned as statics and mutation-verified below, and the composition is three lines of
`tick` and `paintHistory` that read them. The zero-region exit under the clip (§4 D) is in the same
class.

**Mutants** (each a one-site edit of `GrHistoryView.h`, rebuilt with clang-22, the fixed header
restored and rebuilt after each; the logs are in this session's scratch directory):

| Mutant | Kills (of 944, six geometry cases) |
|---|---|
| **stepped** — `bucketX` returns the 0.2.7 expression (`right − (kHead − k)·pitch`) | 19: the per-entry walk's "ONE entry moves every completed bucket ONE entry-pitch" in the five `stride ≥ 2` cases, "the phase shifts every completed vertex …" and "… drifts with the phase once complete" in all six, and the re-pinned quarter-full case in the two whose `want / 4` is not a stride boundary. Not killed, and correctly so: the per-entry assertion in the 48 kHz/2048 case — at `stride == 1` the stepped form already moves one pitch per entry, which IS one entry-pitch; that case is exactly why part C exists. The left-edge assertions survive too: the stepped form's oldest vertex lands where the aligned window puts it, since the mutant leaves `buckets` alone. |
| **tip** — `tipFirst` returns the bucket's own start (`(head − 1) / stride · stride`) | 2, both `grTip`: "reads the trailing stride entries at every fill" and "is NOT the newest bucket's partial range at a bucket start". Survive, as they must: "coincides with the bucket the instant it completes" (the two ranges ARE equal there) and "never reaches before `first`" (the mutant keeps the `max (first, …)`). Nothing in the geometry walk moves — the tip's WINDOW and the tip's POSITION are separate questions. |
| **nophase** — the `phase` term dropped from both branches of `bucketX` | 12: "the phase shifts every completed vertex and is continuous across an arrival" and "… drifts with the phase once complete" in all six cases — and NOT "ONE entry moves every completed bucket ONE entry-pitch", which still passes because part A is intact. That survival is the evidence that A and C are measured separately: a fix that shipped A alone would pass the per-entry walk and fail exactly these twelve. |
| **nolowerclamp** — `smoothedHead` drops its lower bound (`jmin (hi, …)` instead of `jlimit (lo, hi, …)`) | 1, exactly the `grPhase` assertion that names it: "snaps FORWARD to a head the estimate did not expect, never behind the data". |
| **norewind** — `smoothedHead` loses the `previous > hi` re-anchor | 1, exactly "a rewound head (ring reset) re-anchors at phase 0, not at the cap". |
| **unaligned** — the first draft's window (`window = want`, `kFirst` from the "wholly inside" and `kHead − kFull + 1` bounds) | 7: the settled-window pin "spans the panel at the fixed pitch" in the two cases whose `want` is not a multiple of `stride` (44.1 kHz/256 and the Advanced well), and "the oldest vertex sits on or beyond the left edge, the next inside" in five of six — the walk catches the inside-the-edge vertex wherever it occurs, and only the 48 kHz/2048 case (stride 1, where the two forms coincide) is blind to it, as it should be. |
| **widthbound** — the aligned window with the OLD width bound (`kHead − kFull + 1`) | 5: the same five walk failures and nothing else — the window bound alone is not what puts the oldest vertex on the edge; the width bound must reach the bucket just off it. |
| **parkedgate** — `parked` ignores whether a new entry arrived | 1, exactly "a ramping estimate, a new entry or a rewound head each un-park it". |
| **painthead** — `paintHead` returns the live head | 1, exactly "a frame draws the head its phase was computed for, not a newer live one". |

Every assertion the fix rests on is killed by the mutant that undoes its part and by no other,
which is what says the parts are measured separately.

## 7 — Independent review

The diagnosis was put to independent readers of the code before the fix was written — three
lenses (geometry/timing, values, rasterisation), each denied this note and asked for a structured
finding with `file:line` evidence, followed by adversarial refuters per claimed mechanism and three
critics of the proposed design (edges, the phase, tests/policy). Recorded as it came in:

- **Geometry lens:** the yellow line is the trace's flat part (the only accent stroke in the
  well); primary mechanism "every vertex moves by 0 or exactly one full pitch per repaint, never a
  fraction" (high), with "frame-quantised, non-integer shift cadence — judder" and "sub-pixel
  anti-aliasing crawl of the near-vertical 1.4 px segments" as its two visible faces (both high);
  the partial newest bucket's reset (medium); a presentation-clock beat on top (medium,
  platform-dependent); and, as a negative result, nothing else in the drawn shape changes between
  frames. Independently the same two mechanisms §2 names, in the same order.
- **Values lens:** the newest-vertex sawtooth at the right edge (high) and its retroactive
  correction when the vertex hands over to the completed bucket (high); an entry-rate/frame-rate
  beat modulating how that sawtooth is sampled (medium); host block-length variance (low); a
  left-edge value pop at bucket expiry, explicitly flagged as *not* the reported region (high);
  the saturated-window oldest-slot hazard the `kSize − 1` clamp already covers (low); and the
  negative result that every other vertex is value-stable between frames. This is the case for
  part B, and it independently confirms that the left edge is a different report.
- **Rasterisation lens** (which read the working tree with the first draft of the fix already in
  it): confirmed from the pinned JUCE source that the software renderer has no snapping — EdgeTable
  rasterises the stroked outline at 1/256 px and converts span coverage to an 8-bit alpha, so a
  rigid horizontal translation by a non-integer amount changes every pixel's alpha along every
  steep segment (peak column alpha 255 → 178 for a 1.4 px stroke as the fraction cycles), while a
  horizontal stroke body emits no edges at all (`if (y1 != y2)` guards edge emission) and so is
  invariant — "exactly why the yellow zero line reads steady and the measured portion to its right
  reads jittery; the report is a description of this asymmetry". It reproduced the 48 % / 1.447 px
  figures independently, and then found the defect in the first draft of part C: with the phase
  read as seconds since the tick that first saw the head move, "the phase ramp never engages
  while the block rate exceeds the frame rate" — at 48 kHz/512 every 60 Hz frame sees an entry, so
  the display still alternated 0.482 / 0.965 px per frame, a 2:1 velocity beat at 33.75 Hz
  (σ 0.239 px), "a genuine residual jitter in the working-tree code; 3× smaller than 0.2.7". That
  finding is what replaced the draft with the rate-smoothed head (§3's last column, §5). Two
  further observations recorded as they stand: on Linux JUCE's "vblank" is a 16 ms `juce::Timer`
  (62.5 Hz against a 60 Hz panel, a 2.5 Hz beat) rather than a scanout-synchronised callback —
  platform behaviour, out of scope; and the alpha beat's depth scales with the editor's UI-scale
  transform (strongest at 100 %, gone once the stroke exceeds two device pixels).
- **The refuters** (two per claimed mechanism, told to default to "refuted" when a claim could not
  be supported from the code; 44 verdicts over 22 mechanism statements). Their reading tree was the
  working tree, which by then carried the fix, so most verdicts split two ways and both are
  recorded: **the primary mechanism stands** — every refuter of "the stepped `bucketX` moves each
  vertex by 0 or one whole non-integer pitch per repaint" conceded it on the arithmetic against
  `91a3707` (0.2.7), re-deriving the 48 % / 1.447 px figures and the panel widths independently,
  one adding that at 48 kHz/128 the per-frame hop is 1 or 2 pitches rather than 0 or 1. **The
  sub-pixel AA crawl was refuted as the explanation** while its rasteriser facts were confirmed:
  "a co-factor identical before and after; the mechanism that changed by an order of magnitude is
  the lurch" — which is how §2 already framed it, as consequence 2 of the stepping rather than a
  cause of its own. **The partial-bucket sawtooth was refuted as an account of a jittery
  *portion*** — "the pop changes ONE vertex, at the right edge" — and as present in the tree,
  where `tipFirst` had replaced it; §2 calls it secondary and part B is sized accordingly. Every
  other verdict ("not in the tree": the since-arrival ramp, the tip deformation, the lead-out) is
  a reading of code this round had already replaced or introduced, and is recorded as such.
- **The three critics** (edges; the phase; tests and policy), reviewing the draft design note
  against the tree, all agreed with the diagnosis and returned twenty findings. Those against the
  draft alone (its always-drifting tip, its since-arrival phase, a "preserved" right anchor) were
  already answered in the tree and are noted here only as confirmation that the draft was wrong
  where the tree differs from it. Those that reached the tree, each acted on:
  1. *Edges, major:* the lead-in did **not** vary continuously — the oldest drawn vertex walked
     left by an entry-pitch per entry and then jumped a whole pitch RIGHT at every bucket expiry,
     the strip it vacated flipping from a sloped segment to a flat run: the one bucket-rate
     discontinuity the fix had left on the panel, at the left edge. The stated range
     `[0, 2·pitch − pitch/stride)` was wrong at both ends. The critic also showed the aliasing
     rationale in §5's "partially-expired bucket" row to be unsound: the paint loop already clamps
     every bucket's read range to `first`, so no frame can peek below `head − (kSize − 1)`
     whatever `kFirst` is. **Adopted:** the read window is now bucket-aligned (`want` rounded up
     to `kFull` buckets, capped at `kSize − 1`) and `kFirst` reaches one bucket earlier on both
     bounds, so the oldest drawn vertex sits on or beyond the edge at every head and phase and the
     crossing segment is drawn exactly under the clip. The lead-in survives only as the fallback
     for an unaligned saturated window, which neither plot width produces (§4 D).
  2. *Edges, minor:* the zero region's `> x0 + 0.5` guard made the honest-boundary bar vanish
     between two frames instead of sliding under the clip, now that `xFirst` moves continuously.
     **Adopted:** the filling case always starts its paths (from the edge, or from `xFirst` once
     that has passed it), so the bar slides out.
  3. *Phase, nit:* `paintHistory` re-read `available()` while the phase had been computed for the
     head the tick saw — on platforms where the deferred paint runs after the vblank tick an entry
     landing between the two drew one sub-entry step ahead of the ramp (modelled σ 0.026 px at
     48k/512 with a 0–3 ms tick→paint skew). **Adopted:** `paintHead` — the frame draws the head
     its phase was computed for; entries below it are published by the ring's own contract.
  4. *Phase, minor / tests, minor:* the cadence-mismatch behaviour was mis-described as "a
     constant sub-pitch offset". With longer-than-prepared host blocks the trace runs one period
     after each arrival and parks for the excess (run/park at the host's block rate); with shorter
     ones the lower clamp bites every frame and motion degrades to per-entry stepping. Never behind
     the data, never more than one entry ahead. **Reworded** in the header banner, `smoothedHead`
     and §4 C.
  5. *Edges, major — bursty delivery:* hosts that render ahead of real time (REAPER's anticipative
     FX, Cubase's ASIO-Guard, Live's process-ahead) deliver several blocks per callback; the
     smoothed head, held to `[head, head + 1]`, then parks until the next burst and the trace
     lurches at burst rate — measured by the critic at 48k/512 with bursts of 4: σ 0.58 px, 22 %
     motionless (0.2.7: 0.72 / 48 %). The remedy is a LAG allowance (`[head − L, head + 1]`) so
     the estimate runs at the nominal rate through a burst, at the cost of `L` entries of display
     latency and a catch-up rule. **Not taken:** `L` is a latency-versus-smoothness trade with no
     value that fits every host (REAPER renders hundreds of milliseconds ahead), which makes it the
     owner's call rather than a constant this round may invent; recorded in the header banner and
     as an open item below. The shipped form is never worse than per-entry stepping at the burst
     rate.
  6. *Phase, minor:* the reader-contract banner said "nothing is cached across an epoch change";
     `smoothHead` now is. **Reworded** to say what is carried, and the bound on what a stale value
     can cost (one entry-pitch for one frame; the hidden-through-a-reset path is covered by the
     rewind re-anchor and the `[0, 1]` clamp).
  7. *Tests, minor:* the tick gate, the clip, the lead-out and the tick→phase→paint composition
     had no pin. **Partly adopted:** the gate (`parked`) and the paint head (`paintHead`) are pure
     statics, pinned and mutation-verified; the clip and the lead-out remain paint-only, and the
     composition needs a vblank the suites cannot supply — disclosed in §6 rather than covered by a
     test that would pass on any code.
  8. *Tests, major:* no tabled case had `want mod stride ≥ 2`, so the widened lead-in bound was
     never exercised where it was widened. **Adopted** as a new geometry case (44.1 kHz/256, 904
     cols: stride 4, remainder 2), which under the aligned window now exercises the crossing-bucket
     rule instead.
  9. *Tests, nit / edges, minor:* the 192 kHz/32 row of §3 measures nothing the eye can see —
     100 entries per frame, phase parked at 1 permanently — and is footnoted as such; the
     `oldestInLeadIn` walk sampled phase 0 only, and now samples phase 1 as well.
  10. *Phase, nit:* the `dt` clamp (50 ms) interaction is recorded in `smoothedHead`'s comment: a
     longer stall only under-advances the estimate, which the lower bound then snaps to `head`.
  11. *Tests, minor — ADR wording:* the amendment's "the right anchor unchanged" was imprecise
     once the newest vertex may sit under one entry-pitch inside the edge while its bucket waits on
     the phase. **Reworded** in ADR-0023's amendment.

## 8 — The model

The script that produced §3, reproduced so the numbers can be re-derived without this session:

```python
KSIZE, KWINDOW = 4096, 20.0
def window_entries(sr, bs):
    sr = sr if sr > 0 else 48000.0; bs = max(1, bs)
    return min(KSIZE - 1, math.ceil(KWINDOW * sr / bs))
def buckets(head, want, cols):
    c = max(1, cols); stride = max(1, (want + c - 1) // c)
    first = max(0, head - want); kHead = max(0, head - 1) // stride
    kFull = max(2, (want + stride - 1) // stride)
    kFirst = max(min(kHead, (first + stride - 1) // stride), kHead - kFull + 1)
    return dict(stride=stride, kFirst=kFirst, kHead=kHead, kFull=kFull,
                fill=head - kHead * stride, first=first)
def pitch_of(b, width): return (width - 1.0) / (b['kFull'] - 1)
def bucket_x_old(b, k, x0, width):
    return x0 + (width - 1.0) - (b['kHead'] - k) * pitch_of(b, width)
def bucket_x_new(b, k, x0, width, phase=0.0):
    p = pitch_of(b, width); ppe = p / b['stride']; right = x0 + (width - 1.0)
    if k == b['kHead']: return right - phase * ppe
    return right - (b['kHead'] - k) * p + (b['stride'] - b['fill'] - phase) * ppe
# Drive: entries at bs/sr seconds, frames at 1/60 s; per frame record bucket_x_*(track_k)
# and the tip value (old: min over [kHead*stride, head); new: min over [max(first,
# head-stride), head)); phase "observed" = seconds since the FRAME that first saw head
# change, clamped to one period. Report mean/σ/min/max of per-frame Δx, the fraction of
# frames with Δx == 0, and mean/max |Δtip|.
```

(The tip in the shipped `bucketX` drifts only while its bucket is complete — §4 C — a refinement
the model's `k == kHead` branch predates; it changes no number in §3, which tracks a completed
bucket, not the tip. The "smoothed head" column advances `smooth += frame_dt / block_s` each frame
and clamps it to `[head, head + 1]`, exactly `GrHistoryView::smoothedHead`. The model's `buckets`
is the first draft's — `first = head − want`, `kFirst` one bucket later on both bounds than the
shipped aligned-window form — which again changes no §3 number: the tracked bucket is mid-panel.)
