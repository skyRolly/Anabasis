# ADR-0039 — The spectrum publishes its two traces, and the configuration that makes them readable, to the painting thread as ONE frame

> **✅ RATIFIED — THE ARCHITECTURE REVIEW GATE IS CLEARED (2026-09-06).** The owner approved this
> decision on review of the shipped publication, including the round-11 widening that put the sample
> rate inside the frame. How it arrived stays in the record, because that is the half worth keeping: `ARCHITECTURE_REVIEW_GATE.md` lists "**Thread Model change** — new
> thread, new cross-thread path, new atomic ordering (`THREAD_MODEL.md`)"; `CLAUDE.md` and
> `AI_AGENT_POLICY.md` repeat "threading-model change" in the Hard Stop list. This record adds a
> cross-thread path carrying a **payload** and a **new atomic ordering** (a sequence bracket with
> release/acquire fences), and both
> [ADR-0027](ADR-0027-painting-thread-reads-editor-bookkeeping.md) clause 4 and
> [ADR-0038](ADR-0038-gr-history-display-scalars-cross-the-painting-boundary.md) clause 8 send
> exactly this case back here in as many words: *"Anything the paint path WRITES, anything carrying
> a payload, and any pair whose cross-pairings are not legal frames is a new path again and returns
> to this gate."* Every cross-pairing here is illegal by construction — that is the whole defect —
> so the exemption ADR-0038 won did not extend to this site and was never claimed. It was filed
> `Proposed`, flagged in the pull request as a gate item a green build does not clear, and held there
> until the owner answered.
>
> Filed `Proposed` **with the code in the tree**, for ADR-0038's stated reason and no other: the path
> already existed as an UNSYNCHRONISED read of two `std::vector<float>` — plain floats written on the
> message thread and read from the GL render thread on macOS and Windows — which is undefined
> behaviour, not a style question. Shipping the synchronisation is strictly better than shipping the
> race while the gate is answered. What the round must not do is assert "no threading change" in a
> pull request, which is the failure ADR-0027's banner records; this record is the flag.

**Status:** **Accepted — 2026-09-06**, on the owner's explicit approval of this record. It was NOT
covered by the standing blanket approval for the post-v0.1.0 rounds (ADR-0027 established that a
gated thread-model item is outside it, and ADR-0038 was held at `Proposed` for the same reason). The
approval is of the design recorded below — a sequence-bracketed frame carrying both traces, the
window they were analysed over and the rate their bins are read through — and explicitly *not* an
instruction to revert the synchronisation, which is the one option that was never available.

> **IT WAS WIDENED BEFORE APPROVAL, DELIBERATELY.** Round 11's review found that the published frame carried
> the two traces but not the SAMPLE RATE that turns their bin indices into frequencies, so `paint`
> read that rate for itself and a rendered frame could pair one configuration's trace with another's
> mapping. The repair belongs to this record rather than to a new one, and the reason is the index's
> own rule about widening: *"that record was signed off naming three different changes, and widening
> a signed-off record after the fact is the failure mode, not the shortcut"* (`ADR_INDEX.md`). This
> record was **not signed off** at the time. Amending it then is what put ONE coherent design in
> front of the reviewer instead of a decision plus an erratum — the opposite of the failure mode that
> rule names — and it is the design below, as widened, that the approval covers. Nothing from the
> first filing has been deleted.

> **A ROUND-11 CORRECTION IS PART OF WHAT WAS APPROVED, and it is worth naming because no sanitizer
> could have found it.** The reader cannot preserve its output on failure — the 4096-bin copy has
> already happened by the time the bracket can be checked — and `paint` was reading straight into its
> drawing buffers and discarding the result, so a read it LOST left it drawing a mixture of two
> publications through the previous frame's rate: the very incoherence this record exists to prevent,
> arriving through the reader instead of the writer. There is no race in it and no memory error, so
> ASan, UBSan and memcheck are all silent on it by construction. `paint` now stages the read and
> commits it with a swap only on success (clause 5), which is what the rest of this record always
> claimed it did.
>
> **WHAT THE APPROVAL COVERS, stated precisely because the correction landed in the same round.** Two
> things changed after the widening the owner reviewed, and neither is a widening: `paint` now stages
> its read and commits on success (clause 5), which makes the code do what clause 4 already promised
> — a CONFORMANCE repair, adding no cross-thread path, no shared state and no ordering; and the
> lock-free `static_assert` now names all five published types instead of one (clause 6), which only
> TIGHTENS a build-time guarantee. Both are dated here and in `CHANGELOG.md` so a reviewer who
> considers either material can see it rather than having to find it. Nothing about the boundary this
> gate protects moved.

## Context

`SpectrumView` (§2.9) draws two traces — the post-input-gain tap and the post-chain tap — from two
`std::vector<float>` of 2048 smoothed dB values each. `tick` computes them on the message thread (a
`juce::VBlankAttachment` callback through `abgui::FrameClock`); `paint` walks them.

`THREAD_MODEL.md` §"Which context paints" settles who runs `paint`: the OpenGL context attaches on
macOS and Windows and never on Linux/X11, and when attached JUCE paints components on the GL render
thread. So on two of the three shipped platforms these two vectors are written by one thread and
read by another, with nothing between them.

Three things follow. The first two are the original filing's; the third is round 11's and is what
widened it:

1. **It is a data race by the letter of the memory model** — the third instance of the defect class
   ADR-0027 recorded for `presetMenusOpen` and ADR-0038 for the GR history's scroll scalars, found
   the same way, by review.
2. **A rendered frame could mix two ticks.** `tick` assigns `inDb` and then `outDb`; a paint landing
   between them draws the input spectrum of tick N beside the output spectrum of tick N + 1. That is
   the same split the 0.2.12 committed-head work removed from the ANALYSIS (ADR-0011's 0.2.12
   amendments), re-entering one layer further out, in the one display whose entire purpose is
   comparing the two traces.
3. **A rendered frame could mix two CONFIGURATIONS.** A trace is a row of BIN indices; what turns a
   bin into a frequency is `binHz = rate / kSize` (`SpectrumView::paint`), and `paint` read that rate
   from `AnabasisAudioProcessor::preparedSampleRate()` on its own (same function, before this round)
   while the trace came from the published frame.
   Two independent reads of two objects, free to disagree. Note precisely what moves: the x axis is a
   FIXED 20 Hz–20 kHz log sweep with no rate term (`paint`'s `fLo`/`fHi` and its `freqAt` lambda), so
   a mismatch does not move the axis — it moves the DATA under it, through `dbForColumn`'s regime
   test, `dbCubic`'s sample position and the averaged bin range. *(Cited by SYMBOL, not by line: the
   round that first wrote this paragraph also inserted ~139 lines above these sites in the same file
   and shipped citations pointing at the wrong code. Line anchors into a file the same change is
   still editing are a trap; `SOURCE_OF_TRUTH.md`'s line format is kept everywhere the target is
   stable.)* MEASURED on
   the round-11 harness at the bin the tone actually occupies, 6 kHz being bin 512 at 48 kHz and bin
   256 at 96 kHz: **−0.00 dB** paired correctly, **−116.80 dB** as a 48 kHz trace under the 96 kHz
   rate, **−120.00 dB** as a 96 kHz trace under the 48 kHz rate. The tone leaves the display
   outright, either way.

   This is [ADR-0038](ADR-0038-gr-history-display-scalars-cross-the-painting-boundary.md) clause 7 —
   *"a published display estimate carries the identity of the state it describes"* — applied to a
   payload instead of a scroll phase, and the first filing did not cite it. `GrHistoryBuffer`'s own
   banner had already drawn the line this view fell on the wrong side of: a reader that maps ENTRIES
   through the prepared pair *"must bracket it with the epoch exactly as it brackets `peek`"*, while
   only a reader with *"no entries in its question"* may take it unbracketed
   (`src/dsp/GrHistoryBuffer.h:153-175`). `SpectrumView` has entries in its question.

Measured, with the reading thread standing in for the renderer and identical audio in both rings so
that a coherent frame's traces are bit-identical (`specFrame`, 4000 publications, a whole 4096-frame
window of one of two alternating tones per tick):

| what the reader reads | reads | frames that mixed two ticks |
|---|---|---|
| the tick's working vectors (the shape this record replaces) | 1 321 607 | **1 161 778 (87.9 %)** |
| each trace published as soon as it is computed | 416 230 | **277 334 (66.6 %)** |
| one bracketed publication (this record) | 306 485 | **0** |

## Problem

The painting thread needs **4099 values** — 2048 input bins, 2048 output bins, the window
`{first, span}` they were analysed over, and the sample rate they were captured at — and it needs
them to be **one tick's**. ADR-0038 admitted
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
   `std::atomic<uint64_t> pubFirst`, `std::atomic<int> pubSpan` and `std::atomic<double> pubRate` —
   the window the pair was analysed over and the rate its bins are read through, both of which travel
   WITH the pair because they are what make the frame one frame. Written only by
   `SpectrumView::publishFrame` on the message thread; read only by
   `SpectrumView::readPublishedFrame`. The reader receives them as one `Frame`
   (`src/gui/SpectrumView.h:77-105`), and `paint` reads **no processor state at all** — the one
   accessor call it used to make is gone.
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
5. **The painter owns its copy, AND STAGES THE READ INTO A SECOND ONE.** `paint` walks
   `paintIn`/`paintOut`, which only `paint` touches, because each trace is read four times over by
   the column interpolators and must not move between those reads. The read itself lands in
   `stageIn`/`stageOut` and is committed with a **swap** only when `readPublishedFrame` returned
   `true`. That is load-bearing rather than tidy: the reader CANNOT preserve its output on failure —
   the 4096-bin copy has already happened by the time the bracket can be checked — so a caller that
   reads into its drawing buffers and ignores the result draws a mixture of two publications through
   the previous frame's rate. Round 11 found exactly that in the tree and this clause is the repair;
   the contract is now stated at `readPublishedFrame` as well, so a future caller cannot make the
   same assumption. All four vectors are `kBins` long from construction; the commit is two pointer
   exchanges; `paint` allocates nothing.
6. **Lock-freedom is a build-time requirement, for EVERY published type** — `std::atomic<float>`,
   `<double>`, `<uint64_t>`, `<int>` and `<uint32_t>`, all `static_assert`ed together, matching
   `GrHistoryView`'s complete set. Decision E's reason: a target where any of them is not lock-free
   would silently put a lock inside `paint`, and must fail the build instead. The first filing named
   the float alone while the payload already carried five types — `std::atomic<uint64_t>` being the
   one with a real chance of failing on a 32-bit target — so the stated guarantee was narrower than
   the thing it claimed to cover. Corrected 2026-09-06.
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
9. **THE RATE IS TAKEN UNDER THE CONFIGURATION BRACKET, and that is what makes it the frame's own.**
   The rate lives in `GrHistoryBuffer` and the frames live in two `ScopeBuffer`s; a relaxed load of
   one is unordered against an acquire load of the other, so "publish the rate you happen to read" is
   not enough. `AnabasisAudioProcessor::prepareToPlay` writes both in one thread in one sequence —
   `engine.prepare` rewinds the two spectrum rings (`src/PluginProcessor.cpp:769` →
   `src/dsp/AnabasisEngine.cpp:68-69` → `src/dsp/ScopeBuffer.h:201-205`), then
   `grHistoryRing.prepare` republishes the pair inside its seqlock window
   (`src/PluginProcessor.cpp:785` → `src/dsp/GrHistoryBuffer.h:217-235`) — and `tick` uses that
   sequence by sampling `GrHistoryBuffer::resetEpoch()` and the rate together at its top and closing
   with `SpectrumView::configurationHeld` before it commits anything
   (`src/gui/SpectrumView.cpp`, `tick` and `configurationHeld`). It is `GrHistoryView`'s reader
   contract verbatim, including the evenness test (`src/gui/GrHistoryView.cpp:144`), and it is what
   moves this view from the banner's unbracketed case to its bracketed one.

   The cases are a split over where the sampled epoch fell in `resetGuard`'s modification order,
   which is what makes them exhaustive — **not** over the direction of the mismatch, which is not a
   partition: (i) ODD, rejected outright, because the odd increment is a RELAXED RMW and an acquire
   load that takes it synchronises with the PREVIOUS clear, carrying no ordering against the rewinds;
   (ii) EVEN and before the clear — either the tick read the new rate, which it can only have read
   from a store inside the clear window, and the seqlock reader rule (Boehm, MSPC 2012) forces the
   re-read past the acquire fence to observe that clear's odd increment; or the tick's acquired ring
   indices were post-rewind, and the clear's even release RMW happens-before the pushes that revealed
   them, so coherence forbids the re-read returning anything earlier — either way the epoch moved and
   the frame is held; (iii) EVEN and after the clear, where the acquire load of a release RMW puts
   the rate store AND both rings' `write.store (0, release)` before every later load in the tick.

   **A NAMED PREMISE**, in the words `ScopeBuffer::reset` already uses (`src/dsp/ScopeBuffer.h:197-200`):
   the host does not deliver audio across `prepareToPlay`. It is a plugin-API contract, not a C++
   guarantee, and case (ii)'s second limb rests on it. It also covers the window this bracket cannot
   see into — the milliseconds between the ring rewinds and the clear, spent constructing the eight
   oversamplers — where the epoch and the rate are both the old configuration's and the rings are
   already rewound: there is no new-rate audio yet to mis-map, and the rings report `committed == 0`.
   That window is the one `KNOWN_ISSUES.md` KI-017 already audited and accepted.

10. **TWO THINGS THIS BRACKET IS NOT**, stated because the short version is wrong in both:
    * **It announces the RATE, not the ring reset.** `GrHistoryBuffer::prepare` clears only when the
      (rate, block) pair CHANGED (`src/dsp/GrHistoryBuffer.h:144-151`) while `AnabasisEngine::prepare`
      rewinds both rings UNCONDITIONALLY (`src/dsp/AnabasisEngine.cpp:68-69`), so the ordinary
      transport-start re-prepare at an unchanged pair rewinds with the epoch standing still. That case
      cannot move the rate, which is all this bracket is about, and the rings' own `resetGeneration`
      remains the SOLE detector for it. Nothing here subsumes `resetObserved`.
    * **It pairs the rate with the frames this tick's ACQUIRED INDICES describe, not with every sample
      the EMA remembers.** The ring payload is read through relaxed loads, and KI-018's cross-ring
      residual — one ring's rewind observed and the other's not, for one tick — is unchanged: that
      tick floors the ring it saw and can fold the other's pre-reset frames into an EMA published
      beside the new rate. One tick, ~16.7 ms, decaying on the 120 ms EMA. **The residual is bounded,
      not removed**, and the property claimed is the bounded one.

11. **This is not a licence to widen.** The permission is for ONE site, for a payload that is
   read-only on the painting side, published whole by a single writer, validated by the sequence
   bracket, and carrying the identity of the configuration it describes. A second payload site, a
   paint-path WRITE, or a second writer of `frameSeq` is a new path again and returns to this gate.

## Review package

Collected here so a reviewer does not have to assemble it from the prose above.

| | |
|---|---|
| **Problem** | Two 2048-bin dB traces and the configuration that makes them readable cross from the message thread to the painting thread. Before this record they crossed as plain `std::vector<float>` plus an independent accessor call: a data race on two of three shipped platforms, a frame that could mix two ticks, and a frame that could mix two sample rates. |
| **Ownership** | ONE writer (`SpectrumView::publishFrame`, message thread, called only from `tick`), ONE reader (`SpectrumView::readPublishedFrame`, called from `paint` in the plugin and from tests). The published storage is written nowhere else and read nowhere else. |
| **Writer thread** | The message thread — `tick` is a `juce::VBlankAttachment` callback through `abgui::FrameClock` (`src/gui/FrameClock.h`). |
| **Reader thread** | Whichever thread paints: the OpenGL context's render thread on macOS and Windows, the message thread on Linux, and the message thread again for `createComponentSnapshot` (`docs/architecture/THREAD_MODEL.md` §"Which context paints"). |
| **Publication sequence** | counter odd (relaxed) → `atomic_thread_fence(release)` → 2048 + 2048 relaxed `float` stores → `first`, `span`, `rate` relaxed → counter even (`release`). |
| **Read sequence** | counter acquire → reject if odd → relaxed copy of both traces and the three scalars → `atomic_thread_fence(acquire)` → relaxed re-read → keep only if unchanged. |
| **Memory ordering, and why each part** | The release FENCE rather than a release store, because release orders what came BEFORE it and what must be ordered here is what comes after. The payload relaxed, because the counter carries the ordering and the atomics exist to make the access defined. The closing store `release`, which is what publishes the payload. The reader's acquire fence BEFORE the re-read, which is the Boehm (MSPC 2012) seqlock reader — an acquire load alone is not sufficient on a weakly ordered target. `frameSeq` has one writer, so no read-modify-write is needed. |
| **Coherent-frame invariant** | *Every rendered frame observes one tick's traces, the window they were analysed over, and the sample rate they were captured at — or it observes an earlier such frame in full. No rendered frame combines state from two ticks or two configurations.* |
| **Retry / fallback** | Bounded at two attempts. `readPublishedFrame` returns `false` and its two trace outputs are then INDETERMINATE; the caller stages, so the frame `paint` draws is the one it already held — an older coherent frame, never a mixed one. No spin, no lock, no delay. The bound is deliberate: a spin here is a spin on the render thread, where a stall is a dropped frame rather than a stale one. |
| **Allocation** | None on either side at run time. 16 KB of published storage, 16 KB of painter-owned drawing copy and 16 KB of painter-owned staging, all sized once in the constructor (`std::vector<std::atomic<float>>` cannot be resized, so it is built with a count). |
| **Cost** | MEASURED, 2000 iterations on the round-11 harness: a whole tick (two 4096-point FFTs and both ring reads) is **210.5 µs**; `publishFrame` is **2.37 µs** of it — **1.13 % of a tick, 0.014 % of a 60 Hz frame period**. `readPublishedFrame` is **1.50 µs** per paint, and committing it is two pointer swaps. Memory: 16 KB published + 16 KB drawn + 16 KB staged = **48 KB per view**, one view per editor, all allocated once at construction. Nothing on the audio path changed at all. |
| **Reset / reconfiguration** | A reset publishes the EMPTY frame — the display floor in both traces, a zero-length window, and the new rate — and commits its accounting, so a reconfiguration can never leave the previous configuration's pixels up. The configuration bracket closes BEFORE that accounting, so a tick that straddled a clear commits nothing and the reset is answered by the next tick rather than swallowed. |
| **Sample-rate coupling** | The rate is part of the frame (clause 1) and is taken under the GR ring's reset epoch (clause 9), which is what makes it the same configuration's as the frames. `paint` reads no processor state. |
| **Alternatives** | Immutable snapshot per tick (allocates 16 KB per frame; `atomic<shared_ptr>` is not lock-free here) · two-slot double buffer (tears when two ticks land in one paint — a timing assumption, not a proof) · triple buffer (correct and wait-free, but 48 KB and an ownership protocol the tree does not otherwise have, to avoid a fallback that costs one repeated frame at 16.7 ms) · a mutex (a lock on the paint path, and one the tick can be made to wait on) · painting-thread ownership ("the painting thread" is not one thread). For the rate specifically: a second atomic beside the frame (rejected — that is the mismatch, restated) and adding a rate to `ScopeBuffer` (rejected — it extends the producer/consumer protocol for a fact the plugin already publishes, the same reasoning that withdrew the `maxPush` draft this round). |
| **Why the design is necessary** | The path exists whether or not it is synchronised; the only question is whether it is defined. Removing it would mean not drawing the spectrum, or drawing it from the audio thread. Of the mechanisms that make it defined without a lock, an allocation or an unbounded retry, this is the smallest — and it makes the boundary NARROWER than before, because `paint` now reads exactly one object instead of the view's mutable vectors plus a processor accessor. |

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
- **RACE-FREEDOM IS ARGUED, NOT DETECTED, AND NOTHING IN CI CHANGES THAT.** The `sanitizers` job runs
  AddressSanitizer, UndefinedBehaviorSanitizer and valgrind **memcheck**; none of the three is a race
  detector, and the repository runs no ThreadSanitizer, helgrind or DRD lane. A green sanitizers job
  therefore says "no memory error and no UB on the paths executed" and says **nothing** about races,
  and this record does not claim otherwise. Measured while asking the question honestly: helgrind
  reports 16 "possible data race" hits on this publication — and a twenty-line control program
  containing nothing but a textbook lock-free seqlock over `std::atomic<float>` produces the same
  reports, because helgrind models pthread primitives and not the C++11 memory model. So helgrind is
  not an instrument for this code either way.
- **What the tests can and cannot see, stated rather than implied.** The coherence is pinned by a
  reading thread, by the painter's own frame, and — since round 11 — by a thread that actually PAINTS
  while the pair moves (`specFrame`, `specAxis`, `specPaint`). Two things are argued rather than
  measured, and each is a mutant that survives: the `repaint()` that carries a published frame to the
  screen (a headless suite has no repaint region to inspect); and the configuration bracket's three
  guards — the bracket itself,
  its evenness test and the placement of the reset commit behind it — which defend against a
  `prepareToPlay` landing INSIDE a tick. The suite reconfigures from the thread that ticks, so a clear
  cannot overlap a tick there, and making one overlap needs a host thread reconfiguring while audio
  is present, which the named plugin-API premise forbids. The evenness test is required by
  `GrHistoryBuffer`'s stated reader contract (`src/dsp/GrHistoryBuffer.h:122-127`) whether or not a
  test can see it, and `SOURCE_OF_TRUTH.md` puts that contract above a test's reach.
- **`CurveView` is NOT covered by this record.** It reads `preparedSampleRate()` unbracketed from
  both the painting thread and the editor's timer (`src/gui/CurveView.cpp:27,35`), and it is the
  banner's legitimate unbracketed case: its curve comes from the parameter set, not from a ring, so
  there are no entries whose timeline the rate has to match. KI-017 already records the consequence —
  a bounded correct-but-late frame. Left alone deliberately.

## Related code

- `src/gui/SpectrumView.h:77-105` (`Frame`, `lastFrame`, `paintedFrame`), `:107-131`
  (`readPublishedFrame`), `:275-300` (the published member block and its `static_assert`)
- `src/gui/SpectrumView.cpp` (`tick` — the configuration sample at its top, the reset-edge
  publication, the bracket close before each commit; `publishFrame`; `readPublishedFrame`;
  `configurationHeld`; `paint`)
- `src/dsp/GrHistoryBuffer.h:144-151` (the clear-on-change gate), `:153-175` (the two-discipline
  rule this view now sits on the other side of), `:189-193` (`batchIntact`), `:217-235` (`clear`)
- `src/PluginProcessor.cpp:769,785` (the order the bracket's proof rests on),
  `src/PluginProcessor.h:560-564`
- `src/dsp/AnabasisEngine.cpp:68-69`, `src/dsp/ScopeBuffer.h:197-205`
- `src/gui/GrHistoryView.cpp:144` (the same reader contract, already in the tree)
- `docs/architecture/THREAD_MODEL.md` §"Which context paints"
- `docs/policies/THREADING_POLICY.md` (Message → Painting row)

Evidence [Verified]:
- Source: the files and lines above at 0.2.12
- Test: `specFrame` — the published frame equals the frame the tick committed; an idle tick leaves it
  untouched; a paint takes the published window and follows it; and a reading thread over 4000
  publications with alternating whole-window tones never observes a mixed pair. `specReset` — the
  reconfiguration cases across the block-size boundary (8192, 13000, 16384, 32768), with and without
  new audio.
- Test (round 11): `specAxis` — a frame carries the rate its bins were captured at; a paint before
  the next tick draws the previous frame through the rate that produced it; across a reconfiguration
  no frame pairs a trace with a rate that did not produce it; the empty frame a reconfiguration
  publishes carries its configuration; a never-prepared view falls back to 48 kHz rather than
  dividing by zero; and a reading thread watching a 48 kHz ⇄ 96 kHz churn never sees the tone
  anywhere but where its own frame's rate puts it.
- Test (round 11): `specPaint` — a thread that PAINTS while the analyser publishes, with two tones at
  opposite ends of the spectrum and a `dt` that makes every frame its own analysis, so a coherent
  frame has exactly one of the two marker bins lit and a torn one has both or neither. Measured on the
  shipped build: 4274 paints, 95 reads the painter lost, 44 of which had already copied — the state
  the test looks for is reached tens of times per run.
- Mutants killed: the renderer reading the tick's working vectors (1 161 778 of 1 321 607 mixed
  reads); each trace published as it is computed (277 334 of 416 230); the sequence bracket removed;
  the painter reading the working vectors; the reset publishing nothing; the reader-side reserve
  dropped (30 checks); `paint` reading `preparedSampleRate()` for itself; the trace published without
  its rate; the rate published on the tick's schedule rather than the frame's; **`paint` reading into
  its drawing buffers and ignoring the result** (the round-11 defect); and **the staged read committed
  regardless of the result**.
- Mutants that SURVIVE, with the reason: the rate stored one instruction past the closing release
  (the reader reads it ~4096 loads after the sequence load, so the writer would have to be preempted
  inside a one-instruction window for the reader's whole copy); and the three configuration-bracket
  guards, for the reason given under Consequences.
- Worklog: `worklogs/2026-09-05-gr-history-tip.md` §16
