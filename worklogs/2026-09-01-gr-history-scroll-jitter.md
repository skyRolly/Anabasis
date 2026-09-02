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

## 9 — The second review round: three reader/publisher defects in the fix (2026-09-02)

The fix of §4 was reviewed again after it was pushed, and the round found three defects in it. All
three are correctness rather than appearance, two of them races, and all three are repaired here.
The geometry of §4 is unchanged: nothing in this section moves a vertex.

### 9.1 — The frame's read window could reach the slot the audio thread was filling

**The finding.** `paintHead` (§4 C) deliberately draws the head the TICK observed, which the audio
thread may already have moved past. The read window was then derived from that stale head — and so
was the ring's safety margin. At a saturated window that margin is exactly one slot, so spending it
on the staleness let a single block arriving between the two reads put the oldest peek on the slot
the producer was writing.

**The producer/consumer sequence, in order.** `GrHistoryBuffer::push` (audio thread) writes
`entries[i & kMask]` and THEN release-stores `i + 1`. So a reader that acquire-loads
`available() == L` knows entries `0 … L−1` are complete, and knows the producer is now inside the
write of slot `L & kMask`. A peek of index `n` touches that slot when `n ≡ L (mod kSize)`, and since
the reader only ever reads below the head, the first such `n` is `L − kSize`. **The invariant is
therefore `L − n ≤ kSize − 1` for every peeked `n`, measured against `L`.**

Pre-0.2.8 that held by construction: `head` WAS `L`, `first = L − want`, and `windowEntries` clamps
`want` to `kSize − 1`, so `L − first ≤ kSize − 1` — the ring header's "the clamp is `kSize − 1`, not
`kSize`" argument, exactly. At 0.2.8 `head = shownHead = L − Δ` and `first = head − window`, so
`L − first = Δ + window`; with `window == kSize − 1` **any Δ ≥ 1 breaks it.** The window saturates
at 192 kHz/32 (`want == kSize − 1 == 4095`), where ~100 blocks land per 60 Hz frame, so Δ ≥ 1
between the tick's read and the paint's read is the normal case there, not a corner.

What the reader would then get is a non-atomic read of a two-float `Entry` the producer is writing:
a data race by the letter of the model, and in practice a torn or half-new value at the oldest end
of the trace.

**The fix.** `readFloor (live) = live − (kSize − 1)`, and `paintHistory` clamps its oldest readable
index up to it: `first = max (nb.first, readFloor (live))`, with `live` read ONCE so both the
staleness question (`paintHead`) and the safety question are answered from one observation. The
floor binds only when the window is saturated AND the drawn head is stale — everywhere else it is
below `nb.first` — and it moves NO geometry: `bucketX` and `kFirst` never see it, only which entries
the oldest bucket aggregates. The reads themselves went into a pure `bucketReads`, so the read set
is testable rather than reachable only from `paint`.

**The batch-duration half, fixed with it.** The clamp is sound *at the instant `live` was read*; a
long batch can be overtaken. That is the same failure mode, so it gets the same treatment the epoch
already has: `available()` is re-read after the batch and the frame discarded if the producer has
advanced past this batch's floor. One dropped frame, which is the seqlock bargain the ring's own
header states. And if the producer has lapped everything the frame would draw (`first >= head`), the
paint blanks rather than reading overwritten slots.

**Rejected:** shrinking `windowEntries` by a staleness margin (changes the window semantics for
every configuration to fix one); dropping `paintHead` and drawing the live head (re-opens the
tick→paint step the ramp exists to remove, §4 C); making the batch atomic against the producer
(that is a lock on the audio thread, which `REALTIME_AUDIO_POLICY` forbids outright).

### 9.2 — `shownHead` and `smoothHead` crossed the painting boundary as plain scalars

**The finding.** `tick` runs on the message thread; `paintHistory` runs on whichever thread paints,
which `THREAD_MODEL.md` §"Which context paints" settles for this tree: the GL render thread on
macOS and Windows, the message thread on Linux. Two plain scalars written by one and read by the
other are a data race and therefore undefined behaviour — the same defect class ADR-0027 recorded
for `presetMenusOpen`, in the same editor, found the same way.

**The fix, and why this shape.** Both are `std::atomic`, `memory_order_relaxed`, written only by
the tick and read only by the paint, with a `static_assert` on lock-freedom so a target where that
does not hold fails the build instead of putting a lock in the paint path. The substantive question
is not the atomicity but the PAIR: ADR-0027 clause 4 permits one scalar and sends "two values seen
consistently" back to the gate. These two do not need consistency, and that is an argument about
values: `smoothHead ≥ head` for every published pair (`smoothedHead`'s lower clamp) and both scalars
only increase between publications, so `frameFor` resolves ANY pairing to `min (smoothHead,
head + 1)` — a position between two frames the ramp itself produces. A torn read draws a frame the
display was about to draw; nothing jumps and nothing moves rightward. `grPair` pins that over every
cross pairing of a 64-frame publication sequence.

**Rejected:** a seqlock or `std::atomic<struct>` snapshot (16 bytes, not lock-free on the supported
targets — a lock in the paint path by another name, and it buys a guarantee this estimate does not
need); encoding both in one `double` (not recoverable — `smoothHead == head` and
`smoothHead == head + 1` are both exact integers meaning opposite things); a lock (ADR-0027's
reason, unchanged); single-thread ownership (there is no single painting thread — GL thread,
message thread, and `createComponentSnapshot` all paint).

**This is a gated change and it is NOT cleared.** It widens ADR-0027's Message → Painting row, so
it is filed as **[ADR-0038, `Proposed`]**, with `THREADING_POLICY.md` and `THREAD_MODEL.md` updated
to name the second site and its status. The code ships with the ADR unratified deliberately: the
path already existed unsynchronised, so shipping the synchronisation is strictly better than
shipping the race while the gate is answered. ADR-0027's banner is the precedent — *"a rule can be
quoted accurately and still not be applied"* — and the difference this time is that the round is
flagging it instead of asserting "no threading change".

**The rest of the state path, inspected rather than assumed.** `shownEpoch` is read and written only
by `tick` (the paint samples the ring's epoch itself) — plain, and stays plain, because a member
joins the atomic set when the painting thread reads it, not because a neighbour is atomic.
`FrameClock`'s state (`attachment`, `emaDelta`, `countdown`, …) is touched only in the vblank
callback and in `start`/`stop`, all message thread; the paint never reads it. **One item is
knowingly left:** `paintHistory` reads `processor.getSampleRate()` and `getBlockSize()`, which are
plain `AudioProcessor` members written on the host thread at `prepareToPlay` — the same class of
race, but JUCE-owned, present in every view in this tree since P5, and not this round's to fix; a
wrong read there costs a display-width approximation the header's time-base caveat already covers.
Recorded as known, not repaired, because repairing it means snapshotting the prepared pair for every
view — a broad refactor this review explicitly excludes.

### 9.3 — A clear that refilled to the same count was invisible to the idle gate

**The finding.** `parked` keyed on the entry COUNT. `GrHistoryBuffer::reset()` rewinds the write
index to 0, so a clear followed by a refill to exactly the previous count presents the tick with
`head == shownHead` over entirely different contents; the gate parked, and the old geometry stayed
on screen. One frame while audio keeps flowing (the next block makes the heads differ) — and
INDEFINITELY if the transport stops there, which is precisely the moment a host re-prepares.

**The fix.** The count is not the identity of the contents; the ring already publishes one. The
reset epoch — bumped twice per clear, and already read by the paint for its batch guard — joins
`parked`, and the tick remembers it in `shownEpoch`. Any change, odd or even, means new contents.
Ordinary frames are untouched: across them the epoch is constant, so the gate is exactly the count
rule it was. The same epoch also re-anchors the phase (`smoothedHead`'s `cleared` argument): a clear
restarts the timeline, and carrying the old sub-entry offset across it would draw the new history's
first frames one entry-pitch out. `smoothedHead`'s existing `previous > hi` rewind branch cannot see
this case — that is the whole point of the finding, the head has not visibly rewound — so the caller
passes the fact in rather than the function guessing from the index.

**Rejected:** special-casing "count unchanged but suspicious" (the review asked for an identity
mechanism rather than a count heuristic, and it is right — any count rule is a guess); comparing a
sampled entry (a heuristic with false negatives, and it reads ring data to answer a question the
ring already answers).

### 9.4 — Verification

Suites: **`AnabasisTests` 301 + `AnabasisStateTests` 970 = 1271**, 0 failures, clang-22.1.8 and the
local g++ 13.3, Release. The round adds one test,
`testGrHistoryReaderStaysInsideTheRingAndSeesEveryReset` (26 checks). It pins INVARIANTS rather than
an observed absence of a race, which is the only honest thing a deterministic single-threaded suite
can do about a race: which index a frame may read, which value pairs it may draw, which states it
may park on.

| Mutant (one site each, `GrHistoryView.h`) | Kills |
|---|---|
| **nofloor** — `readFloor` never binds (the pre-fix window) | 7, all `grRace`: the floor bound, every-read-inside-one-lap, the write-slot assertion, both real-ring assertions, the lapped-producer bound, and the "not vacuous" check |
| **noreadclamp** — `bucketReads` drops the `first` clamp on completed buckets | 4 `grRace` — the read set escapes below the floor even with the floor correct, which is why the clamp is asserted at the READ rather than only at `first` |
| **noepoch** — `parked` ignores the epoch (the pre-fix gate) | 2 `grReset`: the equal-count clear parks, and the real-ring exercise that reproduces it |
| **noreanchor** — `smoothedHead` ignores `cleared` | 1 `grReset`: the phase does not re-anchor on a clear the index cannot show |
| **nophaseclamp** — `phaseOf` loses its upper clamp | 2: the phase's own range pin and `grPair`'s cap — the pairing argument rests on that clamp, so removing it breaks the pairing claim exactly where the ADR says it would |

Each kill set is disjoint from the others, which is what says the three fixes are measured
separately rather than by one over-broad assertion.

Gates: zero first-party warnings under the full clang-22 gate; `check-docs` 103 files clean;
portability 48/0; realtime 40/0; citations against `origin/main`; `git diff --check` clean.

### 9.5 — Remaining risk, stated

- **The batch is not atomic against the producer, by design.** `readFloor` bounds the reads at the
  instant `live` is read and the post-batch re-check discards a frame the producer overtook, so a
  bad read is turned into a dropped frame — but the reader still peeks non-atomically while the
  producer runs. That is the ring's decided contract (`GrHistoryBuffer`'s header: the epoch
  "DISCARDS such a batch instead, which is the seqlock bargain"), not something this round changed,
  and closing it properly means either double-buffering the ring or locking the audio thread. Both
  are out of scope and the second is forbidden outright.
- **The prepared-pair reads named in §9.2** — `getSampleRate()` / `getBlockSize()` from the paint —
  are a known, unrepaired instance of the same class, repo-wide and JUCE-owned.
- **ADR-0038 is `Proposed`.** The synchronisation is in the tree and the DECISION it widens is not
  ratified. If the review rejects the two-scalar shape, the fallback is a consistent snapshot
  (option B in that record) — more mechanism, same visible behaviour.
- **The races themselves are argued, never observed.** No test here stages a concurrent producer,
  because none can do so reproducibly; what is pinned is the arithmetic that makes each race
  impossible. A TSan lane would test the claim directly and this repository has no TSan job
  (`sanitizers` is ASan + UBSan + valgrind, `TESTING_POLICY.md` Level 1b) — worth one, and out of
  scope here.

## 10 — The final review round: the post-check was validating a data race (2026-09-02)

One finding, and the fix is not where the finding points — which is the part worth recording.

**The finding**, against `GrHistoryView.cpp`'s post-check: *"When audio advances or reset clears
during painting, `resetEpoch()` and `available()` rechecks follow concurrent non-atomic reads.
Discarding the frame cannot undo undefined behavior."*

**Where the UB actually was.** The two validation reads were ALREADY race-free:
`resetEpoch()` is `resetGuard.load (acquire)` and `available()` is `writeIndex.load (acquire)`,
both on `std::atomic`. Nothing in the post-check expression needed changing, and changing it would
have been the "false fix" the review warned about — an atomic-looking check in front of data that
stays non-atomic. The race was in the payload:

```cpp
entries[(size_t) (i & (int64_t) kMask)] = { grDb, peak };   // push: plain store
return entries[(size_t) (n & (int64_t) kMask)];             // peek: plain read
```

`readFloor` (§9.1) keeps a batch inside one lap of the producer **as of the instant `live` is
read**; a batch the producer laps mid-flight still reaches those slots, and the post-check exists
precisely to notice that and drop the frame. So the guards were designed around a case that, in
plain C++, is undefined the moment it occurs. Detection was never the defect; what it was detecting
was.

**The fix.** `GrHistoryBuffer::Slot` holds `std::atomic<float> grDb, peak`, written and read
**relaxed**; `Entry` stays a plain value pair, so no caller changes. Relaxed is exactly right and
not a shortcut: the release-store of the write index still orders both payload stores before the
index a reader acquires, so *"a reader that sees index N sees entry N−1 complete"* holds unchanged —
what the atomics add is the OTHER case, where the read is now defined (each field yields one of the
two values; the pair may be mismatched) instead of undefined. The guards keep their job: such a
frame is still discarded. **Defined-then-discarded, rather than detected-after-the-fact.**

**The audio thread pays nothing, measured rather than asserted.** Compiling a minimal TU against
both headers (clang-22.1.8, `-O3`, x86-64, the state target's own include set):

```
audio_push:  movq 32768(%rdi),%rax · movl %eax,%ecx · andl $4095,%ecx
             movss %xmm0,(%rdi,%rcx,8) · movss %xmm1,4(%rdi,%rcx,8)
             incq %rax · movq %rax,32768(%rdi) · retq          ← IDENTICAL, plain vs atomic
```

The **reader's** codegen does change, and that is the evidence the fix is real rather than
cosmetic: the plain version fused both floats into a single 8-byte `movsd` — one wide non-atomic
load of the pair, exactly the access the finding names — where the atomic version emits two
separate 4-byte `movss` loads that the compiler may no longer merge. `static_assert
(std::atomic<float>::is_always_lock_free)` is what keeps the audio side honest: a target without
lock-free float atomics would put a LOCK in `push`, which `REALTIME_AUDIO_POLICY` forbids outright,
and it must fail the build rather than ship.

**Adjacent state, inspected rather than patched blindly.** `writeIndex` and `resetGuard` are atomic
and were already correct — no change. `reset()`'s bulk clear became relaxed atomic stores for the
same reason (a reader may be peeking those very slots while it runs, which is what the epoch
announces); its release fence still orders the odd epoch value before every one of them, so the
seqlock argument in that function is unchanged. Nothing else in the ring touches the payload.

**Rejected:** re-reading the epoch/index more often, or adding a pre-check (both leave the payload
plain — the false fix); a seqlock retry loop over the payload (the reader would spin on the audio
thread's progress and the ring already answers this with a dropped frame); double-buffering
(explicitly out of scope, and it would move a 32 KB copy onto some thread); anything blocking on
the audio path (`REALTIME_AUDIO_POLICY`, and the whole point of the SPSC design).

**Drift reported, not repaired.** `ScopeBuffer` (the two spectrum taps) shares the idiom —
`memcpy` into `std::vector<float>` under the same release/acquire index — and so shares the class
of defect. It is materially safer: its reader takes 4096 of 16384 frames, so the producer must
advance ~12288 frames (~0.26 s at 48 kHz) mid-read to reach them, and the header already argues
that headroom. It is a separate component, outside this review's scope, and this round's
instruction was explicitly not to widen into it. Recorded here so the next round starts from a fact
rather than a rediscovery.

### 10.1 — Verification

Suites: **`AnabasisTests` 301 + `AnabasisStateTests` 979 = 1280**, 0 failures, clang-22.1.8 and the
local g++ 13.3, Release. The round adds a `grSync` section to
`testGrHistoryReaderStaysInsideTheRingAndSeesEveryReset`: the payload's TYPE (both fields
`std::atomic<float>`, lock-free, at the layout of the pair they replaced), the round-trip through
that path, and the two guard inputs answering different questions — the epoch does NOT move when
the producer merely advances, and `readFloor (available())` is what sees the lap. It pins that the
validation is built on synchronised publication state rather than that a stale frame is discarded,
which is what the review asked for.

One mutant, `plainpayload` (the pre-fix non-atomic ring, `Slot` retained so the suite still
compiles): kills exactly one assertion — *"the ring's PAYLOAD is atomic"* — and nothing else, which
is the honest result. The remaining `grSync` checks are about the guards and the round trip, and
those behaved correctly before this fix too; they are there to keep the validation path honest, not
to detect this mutant.

Gates: zero first-party warnings under the full clang-22 gate; `check-docs` 103 files clean;
portability 48/0; realtime 40/0; citations 54 against `origin/main`; `git diff --check` clean.

### 10.2 — ADR-0038 accepted

The owner approved the Message → Painting widening (§9.2). ADR-0038 moves `Proposed` → **Accepted
2026-09-02**, its banner rewritten as a ratification record in ADR-0027's own idiom, and **ADR-0027
clause 4 is amended in place, dated**: its text stands and the boundary it draws moves from "ONE
scalar" to *"scalars whose every stale/fresh pairing is a frame the writer was itself about to
produce"*. That is a property to demonstrate rather than a count to check — strictly harder to
satisfy than "two are allowed now", and it leaves the clause's other exclusions (a payload, a
paint-path write, a pair that genuinely needs consistency) untouched and unreinterpreted. Nothing
about `presetMenusOpen` changes.

### 10.3 — Remaining risk after this round

- **The batch is still not atomic against the producer**, by design and unchanged: `readFloor`
  bounds the reads at the instant `live` is read, the post-check turns a lost race into a dropped
  frame, and the payload's atomicity is what makes the interval between them defined. This is the
  ring's decided seqlock bargain, now legal rather than merely intended.
- **A raced frame can still be MISMATCHED before it is discarded** — `grDb` from one entry, `peak`
  from the next. That is inherent in per-field atomics without a per-entry seqlock, it is bounded to
  frames the guard then throws away, and the alternative costs the audio thread a sequence counter
  per block for a display artefact nobody can see.
- **`ScopeBuffer` carries the same class of defect** with ~0.26 s of headroom (above).
- **The races remain argued, not observed.** No test stages a concurrent producer; what is pinned is
  the arithmetic and the types that make each race impossible or defined. A TSan lane would test the
  claims directly and this repository has none — noted, and deliberately not added here.

## 11 — The reset phase: a display estimate must carry the identity of the state it describes (2026-09-02)

The round's last finding, against `frameFor`: *"After a history reset, `frameFor` can combine the
new head with the pre-reset smoothed value because the atomics are independent. The first fresh
frame starts one entry ahead instead of at phase zero."*

**What actually caused it.** `smoothHead` is published as an ABSOLUTE ring index whose fractional
part is the phase. `GrHistoryBuffer::reset()` rewinds the write index to 0, so after a clear that
index describes entries that no longer exist — and nothing published with it said which timeline it
belonged to. Two windows produced the defect, and the first is the common one:

1. **A paint between the clear and the next tick.** The paint runs on its own schedule (the GL
   render thread on macOS/Windows; any `repaint` on Linux), so it can land after `reset()` and
   before the tick that re-anchors. It then reads the pre-clear `smoothHead` — say 251.0 — against a
   fresh head of 3, and `phaseOf` computes `clamp (251.0 − 3, 0, 1)` = **exactly 1.0**. Not a small
   error: the clamp saturates, so the first frame of the new history is drawn a full entry-pitch
   ahead of where it starts.
2. **The cross-pairing the finding names.** Even after the tick republishes, two independent relaxed
   atomics can be observed in either order on a weakly ordered target, so a frame could pair the new
   head with the pre-reset phase.

**Why no value-based rule can fix it — the case that decides the design.** The obvious cheap fixes
both work until the refill passes the old count, and then both fail:

| Candidate rule | Fails because |
|---|---|
| Anchor when `paintHead` fell back (`shownHead > live`) | Once the refill reaches the old count, `shownHead <= live` and no fallback occurs — the rule sees an ordinary frame |
| Anchor when `smoothHead − head ∉ [0, 1]` | The parked value is `head + 1` exactly, so a stale publication lands on `1.0`, which is a **legitimate** phase |

At the equal count both rules see a valid parked frame over entirely new data. That is the same
shape as §9.3's finding — the count is not the identity of the contents — and it is why the review's
instruction to prefer an epoch/identity mechanism over count inference is the correct one. Both wrong
answers are kept as mutants, and both fail the same three assertions.

**The fix, and its invariant.** A published display estimate carries the identity of the state it
describes. `publishedEpoch` (the ring's reset epoch) is stored by the tick **last, with `release`**,
and read by the paint **first, with `acquire`**; `frameFor` takes it and the epoch the frame is
drawing, and uses the phase only when they agree, anchoring at phase 0 otherwise.

The invariant in one line: **a frame that sees epoch E also sees the phase published under E.** The
release/acquire pair is what makes that true rather than hoped for — relaxed would have sufficed on
a TSO target and not on a weakly ordered one, where the epoch could become visible before the phase
it announces, which is window 2 reintroduced through the back door. It is the only ordered access on
this boundary; it orders those two publications and nothing else, no payload crosses, and the audio
thread is not involved.

**What was deliberately NOT changed.** `shownHead` and `smoothHead` keep their representation and
their relaxed accesses, so ADR-0038 clause 3's pairing argument survives verbatim. That mattered
more than it looks: an earlier draft of this fix published `(epoch, phase)` packed in one relaxed
word, which removes the ordering question entirely — and was **rejected on measurement**, because
`pos = head + phase` is not monotone in `phase` alone. With the phase published as a phase, a torn
read pairing head *i* with phase *i+1* can move the trace RIGHTWARD by up to one entry-pitch
(worked example: `h=100, φ=0.9 → 100.9` then `h=100, φ=0.4 → 100.4`), and no store order removes it
because the other order overshoots instead. The absolute form's `min (smoothHead, head + 1)` is
monotone in BOTH arguments independently, which is exactly why clause 3 holds — so the fix keeps the
absolute pair and pays for the epoch with one release/acquire rather than paying for the epoch with
a regression to the accepted no-rightward-motion claim.

**Also rejected:** a seqlock or snapshot over the three values (ADR-0038 option B, unchanged — it
buys consistency this estimate does not need); making `shownEpoch` atomic and relaxed (window 2 stays
open on ARM); encoding the epoch in `shownHead`'s spare bits (truncates the head, and 2^32 entries is
~8 days of continuous audio at 192 kHz/32 — a real if remote limit, taken for nothing).

### 11.1 — Verification

Suites: **`AnabasisTests` 301 + `AnabasisStateTests` 989 = 1290**, 0 failures, clang-22.1.8 and the
local g++ 13.3, Release. The round adds a `grResetPhase` block covering the review's six points in
order — an ordinary frame carries its phase; a clear occurs; the epoch moves; the same published
values anchor at phase 0 **at every refill state including the equal count**; the tick's first fresh
publication is itself at phase 0 (`smoothedHead`'s `cleared` re-anchor); and the very next frame
carries its phase again — plus repeated clears (every superseded epoch anchors, including the odd
value a clear in flight publishes, while the live one still carries its phase) and the same result
driven from a real `GrHistoryBuffer`'s own epoch arithmetic.

| Mutant | Kills |
|---|---|
| **noepochphase** — `frameFor` ignores both epochs (the pre-fix behaviour) | 3 `grResetPhase`: the anchor at every refill state, the repeated-clear sweep, and the real-ring case |
| **countinference** — anchor only when `paintHead` fell back (`h != shownHead`) | the same 3 — the plausible wrong fix fails exactly where the equal-count case predicted |

Gates: zero first-party warnings under the full clang-22 gate; `check-docs` 103 files clean;
portability 48/0; realtime 40/0; citations against `origin/main`; `git diff --check` clean.

### 11.2 — `ScopeBuffer` left alone, on instruction, and filed

§10 reported it as drift. This round's scope excluded it explicitly, so it is now **KI-015** — a
confirmed correctness entry with the headroom measured (4096 of 16384 frames, so ~12288 frames or
~0.26 s at 48 kHz must pass mid-copy), the reason it is not the blocker `GrHistoryBuffer` was (one
slot of margin there, by construction), and the note that its `memcpy` bulk copy needs its own
design pass rather than a transliteration of the GR fix.

### 11.3 — Remaining risk after this round

- **Everything in §10.3 still stands** and is unchanged by this fix.
- **`publishedEpoch` starts at 0 and the ring's epoch starts at 0**, so before the first tick they
  match and a frame would use the published phase. That phase is `phaseOf (0.0, live)` = 0 for any
  live head, so the frame is anchored anyway — harmless, and noted because it is a coincidence of
  the initial values rather than a property of the rule.
- **The epoch is 32 bits and moves by 2 per clear**, so it wraps after 2^31 sample-rate or
  buffer-size changes. A wrap could only collide if a publication survived exactly that many clears
  unread, which requires the tick never to run in between.

## 12 — The host reconfiguration race: the display's time base is ring metadata (2026-09-02)

The review after `6b1fb60` raised the last cross-thread read the scroll fix had left plain, and two
lane findings against the new test. All three are repaired here; the ScopeBuffer follow-up stays
KI-015 on instruction.

### 12.1 — Root cause

`GrHistoryView::tick` derived the entry period — the scroll rate — from
`processor.getSampleRate()` and `processor.getBlockSize()`, and `paintHistory` derived the window
(`windowEntries`) from the same two calls. Those are `juce::AudioProcessor`'s **plain**
`currentSampleRate` and `blockSize` (`juce_AudioProcessor.h`), written by
`setRateAndBufferSizeDetails`, and the wrapper calls that on **whichever thread the host
reconfigures on**, before `prepareToPlay`: in the VST3 wrapper `preparePlugin` does it, reached from
`initialize`, `setActive` and `setupProcessing`; the AU, AAX, LV2, VST2 and Standalone wrappers each
do the same. The tick reads them on the message thread and the frame on the painting thread. That is
a data race by the letter of the model on every platform, and it is on exactly the values the scroll
timing and the read window are computed from — a torn `(rate, block)` pair maps the window through
one configuration and the phase through another.

Who owned what, read off the tree before the fix:

- JUCE's pair: host-thread writer, plain, read by anyone. No repo-owned copy of it existed on the
  reading side.
- The wrapper's `grRingPreparedRate` / `grRingPreparedBlock` (`PluginProcessor.h`): plain, written
  and read only by `prepareToPlay`, i.e. host-thread only; they gated the clear (0.1.2 item 6: a
  re-prepare at the same pair keeps the history) and nothing else read them. Correct as far as they
  went, and no use to the display.
- The ring's `resetGuard` epoch and `writeIndex`: the only synchronised state on the path, and the
  bracket every other ring read already used.

So the view was reading the one copy of the pair that had no publication discipline while standing
next to the mechanism built for exactly that.

### 12.2 — The fix, and why it is correct under the C++ memory model rather than under TSO

**The pair is ring metadata.** `GrHistoryBuffer` gains `std::atomic<double> preparedRate` and
`std::atomic<int> preparedBlock` (both `static_assert`ed `is_always_lock_free`, matching the
payload's assertion), a `struct Prepared { double rate; int block; }`, and:

- `bool prepare (double rate, int block)` — host thread. The clear-on-change gate moved here from
  the wrapper, byte-for-byte in meaning: same pair → return `false`, nothing cleared, epoch unmoved;
  changed pair → `clear (rate, block)`, return `true`. The comparison reads its own previous stores
  on the single writer thread, so they are relaxed, and the `double` half compares with
  `juce::exactlyEqual` (a re-prepare at the same rate must not clear — an epsilon there would be a
  behaviour change).
- `Prepared prepared() const` — reader side, two relaxed loads. The caller brackets them with the
  epoch exactly as it brackets `peek`.
- `clear (rate, block)` stores the pair **inside the seqlock window**: after the odd increment and
  its release fence, after the 4096 payload stores, before the `writeIndex` release store and the
  even release increment. `reset()` is `clear` at the current pair, unchanged in effect.

The wrapper's two plain members are gone; `prepareToPlay` calls `grHistoryRing.prepare (sampleRate,
samplesPerBlock)`. The audio thread is untouched — `push` did not change, so the previous
amendment's instruction-identity measurement stands without re-measuring.

**Why a bracketed reader gets a coherent pair-plus-entries.** Reader: `e0 = resetEpoch()`
(acquire) → relaxed loads of the pair and the entries → `batchIntact (e0)`. Writer (`clear`):
`resetGuard.fetch_add (relaxed)` (odd) → `fence (release)` → relaxed stores (payload, pair) →
`writeIndex.store (release)` → `resetGuard.fetch_add (release)` (even).

- If `e0` is the even value AFTER a clear, the acquire load synchronises with the closing release
  increment and every store of that clear happens-before the batch: the batch reads the new pair
  and new entries, or later pushes.
- If `e0` is the even value BEFORE a clear that then runs concurrently, any batch load that reads a
  value stored **after the writer's release fence** (a cleared slot, either half of the new pair)
  makes that release fence synchronise-with the reader's acquire fence in `batchIntact`
  (fence–fence synchronisation, [atomics.fences]). The odd increment, sequenced before the writer's
  fence, then happens-before the reader's epoch re-read, and write–read coherence forbids that
  re-read returning the old even value: `batchIntact` is `false`, the frame is dropped. A batch none
  of whose loads read a post-fence value is a consistent pre-clear snapshot. This is the reader Boehm
  shows correct in *"Can Seqlocks Get Along with Programming Language Memory Models?"* (MSPC 2012),
  and it is the C++ model's guarantee — nothing in it depends on x86's store order.

**Why the CLOSE had to become a fence.** The previous close was `ring.resetEpoch() == epoch0`, an
acquire **load**. An acquire load orders accesses sequenced *after* it; it says nothing about the
relaxed loads sequenced *before* it. On a weakly ordered target those loads may be satisfied after
the epoch re-read — the hardware form of the same statement is that ARMv8 permits load–load
reordering without a barrier — so a batch could read a cleared slot after having read the old even
epoch and still pass as intact. TSO forbids that reordering, which is why x86-64 hid it and why the
proof obligation is explicitly "not merely x86". `batchIntact` = `atomic_thread_fence (acquire)` +
relaxed re-read is the reader form the paper gives, and it is now the close of both batches (the
tick's, which reads the pair and `available()`; the frame's, which reads everything). In the frame
it runs FIRST in the post-check, so the `available()` re-read for `readFloor` is sequenced after the
fence too.

**What the fence does not buy, stated so it is not claimed later.** The `readFloor` lap re-check
compares `first` against a fresh `available()`. `push` has no release fence before its entry stores
— deliberately, the audio thread pays nothing — so the model gives no synchronises-with edge from a
reader's entry load to the producer's index store, and the re-check remains what §10.3 called it: a
one-frame, defined-behaviour artefact on the oldest bucket, not a race. The fence removes the
hardware reordering hole in that re-check without turning it into a proof.

**Alternatives rejected.** Making JUCE's pair atomic is not ours to do (it is JUCE's class).
Turning the wrapper's `grRingPreparedRate/Block` into atomics would have been a second copy of the
pair with a second publication discipline, read under no epoch — a torn pair would still have been
possible between a `prepare` and a frame. A `std::atomic<Prepared>` (16 bytes) is not lock-free on
the supported targets. A mutex is out on both threads. The ring already had the exact mechanism
(one writer, a seqlock bracket, a reader that discards on tear) and the pair belongs to the timeline
the clear starts, so it went where the timeline lives.

**Architecture Review Gate — RAISED AND CLEARED.** The owner **accepted this amendment on
2026-09-02**, after the round was pushed and reported; the gate is cleared and nothing on this record
is pending. What follows is why it went to the gate at all, kept because that is the half worth
keeping. This is a Thread Model change in the letter of
`ARCHITECTURE_REVIEW_GATE.md` — a cross-thread path (the pair now crosses through the ring rather
than through JUCE's members) and a new ordering (the reader's fence). It is recorded as ADR-0011's
second dated amendment of 2026-09-02, the same treatment as round 3's atomic payload: a repair to a
decided contract, not a new decision, and flagged in the pull request as a gate item a green build
does not clear. The audio-thread half of ADR-0011 is not touched.

### 12.3 — A finding on the way: a bare `prepareToPlay` sets nothing in JUCE

The first draft of `grPrepared` asserted that after `proc.prepareToPlay (48000.0, 512)` the ring's
pair equalled `getSampleRate()`/`getBlockSize()`, and it failed: `getBlockSize()` was 0. A host
calls `setRateAndBufferSizeDetails` and THEN the callback; the callback alone sets nothing in the
base class. This suite has only ever called the override — so under the old source **every headless
GR test mapped its window through the 48 kHz / 1-sample fallback** (`windowEntries (0, 0)`), a
saturated window, while the ring's entries were recorded at 512. The test now asserts that premise
first (`proc.getBlockSize() == 0 && ring.prepared().block == 512`), then calls
`setRateAndBufferSizeDetails` as a host would and asserts equality, then a same-pair re-prepare
(no clear) and a changed pair (clear + publish), and finally that `windowEntries` and `entryPeriod`
are identical from either source — the "nothing moved under a stable configuration" claim,
measured rather than asserted.

### 12.4 — `linux-lto-tests`: six `-Wfloat-equal` sites

`-Wfloat-equal` is in JUCE's recommended warning set for GCC and not for Clang
(`JUCEHelperTargets.cmake`), which is why the clang gate passed and the `gcc:16` LTO lane failed.
The six sites were all in the round-4 `grResetPhase` block — `x.phase == 0.0` — three on one line
(6743) and one each at 6756, 6768 and 6782. Each is now `std::abs (x.phase) < 1.0e-12`, the
tolerance the surrounding checks already use. No gate, flag or exclusion changed. The lane's
container cannot be run on this machine (no docker daemon; the local GCC is 13.3), so the
reproduction is g++ 13.3 with the lane's flags (`-flto -Wduplicated-cond -Wduplicated-branches
-Wlogical-op`) and its checker invocation (`--compiler g++`): first-party warnings 6 → **0**, with
both suites green from the LTO'd binaries. `grep -c 'float-equal'` on that log: 0.

### 12.5 — PREfast C6262 at `state_tests.cpp:6435`

Line 6435 is `testGrHistoryReaderStaysInsideTheRingAndSeesEveryReset` in every revision of the pull
request, so the warning is this test's. MSVC's C6262 sums a function's block-scoped locals without
the slot sharing clang and GCC perform, and the function held four `GrHistoryBuffer` fixtures
(32,800 bytes each) and one `AnabasisAudioProcessor` (75,688) in sibling blocks. All five are
`std::make_unique`'d now, with a reference alias so every assertion reads as before; semantics and
coverage are unchanged. PREfast runs only in the Windows lane, so the measurement here is clang's
`-fstack-usage -fno-inline` (per-function frames, no inlining into `main`): this function
**76,632 → 936 bytes**. The remaining locals are the `unique_ptr`s, a `juce::AudioBuffer` (heap
data), a `GrHistoryView` (360 bytes) and scalars, so the summed figure any analyser reports is now
well under C6262's 16 KB threshold. One number is worth recording so it is not chased later: the
file's largest frame under clang is 379,048 bytes at `testLearnCommitAndAdaptiveRoundTrip` (five
processors on the stack), pre-existing and untouched — it is not what PREfast cited, which was this
function by line, and the near-coincidence with 379,228 is arithmetic (clang's frame for one
function, MSVC's summed locals for another).

### 12.6 — Verification

Suites: **`AnabasisTests` 301 + `AnabasisStateTests` 1003 = 1304**, 0 failures, from three builds —
clang-22.1.8 Release (full warning gate, zero first-party warnings, checker self-test 18 cases),
g++ 13.3 Release (zero first-party, four vendored), and g++ 13.3 `-flto` with the LTO lane's flags
(zero first-party). `grPrepared` adds 14 checks. Gates: `check-docs` 103 clean (self-test 67);
portability 48/0 (120); realtime 40/0 (134); citations re-anchored against `origin/main` (37);
`check-linux-abi` self-test 19; `git diff --check` clean.

| Mutant | Kills |
|---|---|
| **prepgate** — `prepare` never compares, always clears | 6: `grEpoch`'s same-pair-keeps-history and epoch-moves-by-2 (the pre-existing test, which is what says the moved gate is still covered) and 4 `grPrepared` |
| **pairunpublished** — `clear` stores no pair | 12: the 6 above (the gate then never matches) plus every `grPrepared` check that reads the pair, including "identical from either source" |
| **intactblind** — `batchIntact` returns `true` without the re-read | exactly 1: the torn-close check, and no other |

The fence has no mutant: dropping it is a hardware reordering no deterministic suite can stage, which
is why its correctness is argued in writing (§12.2, ADR-0011) rather than pinned.

### 12.7 — `ScopeBuffer`, again left alone

Excluded from this pull request on instruction across every round; KI-015 now says so in those
words, names the race class (producer overwrites reader, plain payload), the headroom (~0.26 s at
48 kHz), and why this round's ring change does not transfer — no reset epoch to bracket with, no pair
to publish, and a bulk `memcpy` that a per-element atomic payload cannot express without its own
design pass.

### 12.8 — Remaining risk after this round

- **§10.3 and §11.3 stand**, with one item narrowed: the `readFloor` re-check is now ordered after
  the batch's loads by the fence, but the model still gives it no synchronises-with edge from
  `push` (§12.2's last paragraph).
- **The prepared pair before the first `prepare` is (0, 0)**, which `windowEntries`/`entryPeriod`
  already map to 48 kHz / 1 sample — the same fallback the view used when JUCE's members were unset.
  Harmless and unchanged; noted because the headless suite used to live there without knowing it.
- **A host that reconfigures without calling `prepareToPlay`** (the VST3 wrapper's
  `CallPrepareToPlay::no` paths on `initialize` and `setupProcessing`) leaves the ring's pair at the
  last prepare. The ring's pair is the one its entries were recorded under, which is the value the
  display should map through; JUCE's members may disagree until the next `prepareToPlay`, and
  nothing reads them any more.
