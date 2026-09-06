# ADR-0039 — The spectrum publishes its two traces to the painting thread as ONE frame

> **⛔ PROPOSED — THE ARCHITECTURE REVIEW GATE IS OPEN. This is a merge prerequisite, and a green
> build does not clear it.** `ARCHITECTURE_REVIEW_GATE.md` lists "**Thread Model change** — new
> thread, new cross-thread path, new atomic ordering (`THREAD_MODEL.md`)"; `CLAUDE.md` and
> `AI_AGENT_POLICY.md` repeat "threading-model change" in the Hard Stop list. This record adds a
> cross-thread path carrying a **payload** and a **new atomic ordering** (a sequence bracket with
> release/acquire fences), and both
> [ADR-0027](ADR-0027-painting-thread-reads-editor-bookkeeping.md) clause 4 and
> [ADR-0038](ADR-0038-gr-history-display-scalars-cross-the-painting-boundary.md) clause 8 send
> exactly this case back here in as many words: *"Anything the paint path WRITES, anything carrying
> a payload, and any pair whose cross-pairings are not legal frames is a new path again and returns
> to this gate."* Every cross-pairing here is illegal by construction — that is the whole defect —
> so the exemption ADR-0038 won does not extend to this site, and it is not claimed.
>
> Filed `Proposed` **with the code in the tree**, for ADR-0038's stated reason and no other: the path
> already existed as an UNSYNCHRONISED read of two `std::vector<float>` — plain floats written on the
> message thread and read from the GL render thread on macOS and Windows — which is undefined
> behaviour, not a style question. Shipping the synchronisation is strictly better than shipping the
> race while the gate is answered. What the round must not do is assert "no threading change" in a
> pull request, which is the failure ADR-0027's banner records; this record is the flag.

**Status:** **Proposed — 2026-09-06.** Awaiting the owner's explicit approval. **It is NOT covered by
the standing blanket approval for the post-v0.1.0 rounds** — ADR-0027 established that a gated
thread-model item falls outside it, and ADR-0038 was held at `Proposed` until answered for the same
reason. Approval sought is of the design below; withholding it does not imply reverting to the
unsynchronised read, which is the one option that is not available.

## Context

`SpectrumView` (§2.9) draws two traces — the post-input-gain tap and the post-chain tap — from two
`std::vector<float>` of 2048 smoothed dB values each. `tick` computes them on the message thread (a
`juce::VBlankAttachment` callback through `abgui::FrameClock`); `paint` walks them.

`THREAD_MODEL.md` §"Which context paints" settles who runs `paint`: the OpenGL context attaches on
macOS and Windows and never on Linux/X11, and when attached JUCE paints components on the GL render
thread. So on two of the three shipped platforms these two vectors are written by one thread and
read by another, with nothing between them.

Two things follow, and they are separate findings that happen to have one fix:

1. **It is a data race by the letter of the memory model** — the third instance of the defect class
   ADR-0027 recorded for `presetMenusOpen` and ADR-0038 for the GR history's scroll scalars, found
   the same way, by review.
2. **A rendered frame could mix two ticks.** `tick` assigns `inDb` and then `outDb`; a paint landing
   between them draws the input spectrum of tick N beside the output spectrum of tick N + 1. That is
   the same split the 0.2.12 committed-head work removed from the ANALYSIS (ADR-0011's 0.2.12
   amendments), re-entering one layer further out, in the one display whose entire purpose is
   comparing the two traces.

Measured, with the reading thread standing in for the renderer and identical audio in both rings so
that a coherent frame's traces are bit-identical (`specFrame`, 4000 publications, a whole 4096-frame
window of one of two alternating tones per tick):

| what the reader reads | reads | frames that mixed two ticks |
|---|---|---|
| the tick's working vectors (the shape this record replaces) | 1 321 607 | **1 161 778 (87.9 %)** |
| each trace published as soon as it is computed | 416 230 | **277 334 (66.6 %)** |
| one bracketed publication (this record) | 306 485 | **0** |

## Problem

The painting thread needs **4098 values** — 2048 input bins, 2048 output bins, and the window
`{first, span}` they were analysed over — and it needs them to be **one tick's**. ADR-0038 admitted
a second site on the explicit ground that its two scalars did NOT need consistency: every
stale/fresh pairing was a frame the ramp itself produced. That argument is unavailable here and its
negation is the defect. So the question is not "may the paint read these?" but **which consistency
mechanism**, under the constraints the round is held to: no blocking lock on any of these threads,
no allocation per tick, no unbounded retry, no arbitrary delay.

## Options

- **A. An immutable snapshot per tick, published by pointer.** Correct and the easiest to state, and
  it allocates 16 KB every frame at 60 Hz on the message thread. Rejected on the round's explicit
  no-allocation-per-tick constraint; `std::atomic<std::shared_ptr>` is additionally not lock-free on
  the supported targets, which would put a lock in `paint` — ADR-0038 option D's objection.
- **B. Two slots and an atomic index.** No retry and no allocation, but the reader holds no claim on
  the slot it is reading: two ticks during one paint return the writer to that slot and tear it. The
  guarantee would rest on "a paint is faster than two ticks", which is a timing assumption, not a
  proof.
- **C. Triple buffer with an atomic exchange.** Genuinely wait-free on both sides and correct without
  any timing assumption. Rejected as the larger mechanism for the smaller gain: 48 KB of published
  storage instead of 16 KB, three slots to reason about, and an ownership protocol the tree does not
  otherwise have — for a reader whose fallback (option D's) costs one repeated frame at 16.7 ms.
  Recorded as the option to take first if the painter ever must never repeat a frame.
- **D. A sequence bracket over the published pair, with the painter keeping its own copy.** Chosen.
  The writer marks the bracket odd, stores the payload, marks it even; the reader copies inside the
  bracket and keeps the copy only if the counter did not move. Bounded at two attempts — a reader
  that loses twice keeps the pair it already has.
- **E. A mutex around the pair.** Rejected for ADR-0027's and ADR-0038's reason unchanged: a lock on
  the paint path, and a lock the message thread's tick can be made to wait on.
- **F. Give the painting thread its own state.** Rejected for ADR-0038 option E's reason, which is
  stronger here: "the painting thread" is not one thread — the GL thread during `renderOpenGL`, the
  message thread on Linux, and the message thread again for `createComponentSnapshot`, which the
  state suite uses.

## Decision

1. **The Message → Painting boundary admits a PAYLOAD at one named site: `SpectrumView`'s published
   frame.** Two arrays of `std::atomic<float>` (`pubIn`, `pubOut`, 2048 each), plus
   `std::atomic<uint64_t> pubFirst` and `std::atomic<int> pubSpan` — the window the pair was
   analysed over, which travels WITH the pair because it is part of what makes the frame one frame.
   Written only by `SpectrumView::publishFrame` on the message thread; read only by
   `SpectrumView::readPublishedFrame`.
2. **The payload is `memory_order_relaxed` on both sides, and the ordering is carried by the
   counter.** `std::atomic<float>` per bin for `ScopeBuffer::Sample`'s reason and no other: it makes
   the concurrent access DEFINED. It synchronises nothing on its own and is not asked to.
3. **The bracket is the ordinary sequence-counter form, and every step is load-bearing.** The counter
   goes ODD (relaxed) so a reader that starts mid-write knows before it copies; a **release fence**
   follows that store rather than the store carrying release, because release orders what came
   BEFORE it and what must be ordered here is what comes after; the payload stores are relaxed; the
   counter goes EVEN with **release**, which is what publishes them. The reader mirrors it: acquire
   load, relaxed copy, **acquire fence**, relaxed re-load, and the copy is kept only if the two
   agree. `frameSeq` is written on one thread only, so the plain load/store pair needs no
   read-modify-write.
4. **The reader gives up after TWO attempts**, and giving up means the caller keeps the frame it
   already has — an older coherent pair, never a mixed one. This is the round's no-unbounded-retry
   constraint met by construction rather than by tuning: the frame a third attempt would win is one
   the caller is about to redraw 16.7 ms later, and a spin on the render thread turns a stale frame
   into a dropped one.
5. **The painter owns its copy.** `paint` reads into `paintIn`/`paintOut`/`paintWindow`, which only
   `paint` touches, because each trace is walked four times over by the column interpolators and
   must not move between those reads. Both vectors are `kBins` long from construction; `paint`
   allocates nothing.
6. **Lock-freedom is a build-time requirement**, `static_assert`ed on
   `std::atomic<float>::is_always_lock_free`, for decision E's reason: a target where it does not
   hold would silently put a lock inside `paint`, and must fail the build instead.
7. **Publication is the LAST thing a tick does**, after both EMAs are settled and after the
   post-batch reset-generation checks. A tick that HOLDS (no coherent span — ADR-0011's 0.2.12
   amendments) publishes nothing and leaves the previous frame readable, which is the behaviour that
   hold means.
8. **…with one exception, and it is a correctness one: a RESET publishes the empty frame.** A held
   pair is an answer only while it still describes the audio the view is looking at. A re-prepare
   rewinds both rings and re-maps every bin, and where the host's block is at least a whole ring the
   reader's floor sits at or past the head on EVERY tick that follows — so before this record the
   floored EMA never reached the screen and the previous rate's spectrum stayed drawn against the
   new rate's axis indefinitely (6 kHz sits at bin 512 at 48 kHz and bin 256 at 96 kHz). On the reset
   edge the view therefore publishes the display floor in both traces and a zero-length window, and
   commits the reset accounting with it. Not a fabricated frame: it is exactly what this view shows
   before its first frame.
9. **This is not a licence to widen.** The permission is for ONE site, for a payload that is
   read-only on the painting side, published whole by a single writer, and validated by the bracket
   above. A second payload site, a paint-path WRITE, or a second writer of `frameSeq` is a new path
   again and returns to this gate.

## Consequences

- 16 KB of published storage and 16 KB of painter-owned copy, both allocated once at construction.
  A tick adds 4098 relaxed stores to work it already does; a paint adds 4098 relaxed loads. Neither
  allocates and neither can block.
- **Nothing on the audio thread changes.** The producer, its release/acquire index, its reset
  generation and the two rings' protocol are byte-identical. This record is entirely about the
  reader's own publication to its own renderer.
- ADR-0027 clause 4 and ADR-0038 clause 8 are **amended by exception, not reinterpreted**: their
  boundary — scalars whose cross-pairings are legal frames — stands for every site they cover, and
  this record adds the one site where a mechanism supplies the consistency instead of an argument.
- The painter may repeat one frame when it loses the bracket twice. That is a display consequence and
  it is bounded by the tick rate; option C is the recorded answer if it ever must not happen.
- **What the tests can and cannot see.** The coherence is pinned by a reading thread and by the
  painter's own window (`specFrame`); the `repaint()` that carries a published frame to the screen is
  not observable in a headless suite and is argued, not measured — the same limit ADR-0038 records
  for the race itself.

## Related code

- `src/gui/SpectrumView.h` (`readPublishedFrame`, `paintedWindow`, the published member block and its
  `static_assert`)
- `src/gui/SpectrumView.cpp` (`publishFrame`, `readPublishedFrame`, `tick`'s reset publication,
  `paint`)
- `docs/architecture/THREAD_MODEL.md` §"Which context paints"
- `docs/policies/THREADING_POLICY.md` (Message → Painting row)

Evidence [Verified]:
- Source: the files above at 0.2.12
- Test: `specFrame` — the published frame equals the frame the tick committed; an idle tick leaves it
  untouched; a paint takes the published window and follows it; and a reading thread over 4000
  publications with alternating whole-window tones never observes a mixed pair. `specReset` — the
  reconfiguration cases across the block-size boundary (8192, 13000, 16384, 32768), with and without
  new audio.
- Mutants: the renderer reading the tick's working vectors (1 161 778 mixed reads); each trace
  published as it is computed (277 334); the bracket removed (mixed reads); the painter reading the
  working vectors (the painted window never moves); the reset publishing nothing (`specReset` fails
  on every block at or above capacity).
- Worklog: `worklogs/2026-09-05-gr-history-tip.md` §16
