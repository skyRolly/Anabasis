# ADR-0011 — Threading model: two threads, no workers, atomic and SPSC publication

**Status:** Accepted (2026-07-31 — owner sign-off on `docs/DESIGN.md`)

## Context

`ADR_POLICY.md` lists the **threading model** among the changes an ADR is mandatory for, and
`THREADING_POLICY.md` is binding from day one: it fixes the thread inventory (audio ·
message/GUI · an optional GPU render context · **no worker threads**), enumerates the *only*
permitted cross-thread paths, forbids allocation/locking/painting/IO on the audio thread, pins
the atomic orderings, and defers the concrete model to `docs/architecture/THREAD_MODEL.md`
written at P1. It also states the enforcement asymmetry that makes this a P0 decision rather
than a P2 one: adding a thread — "including a worker for analysis, preset scanning, or
metering" — is an Architecture Review Gate item and an AI-agent Hard Stop. A thread admitted
casually at P2 cannot be removed casually at P3, and every lock-free contract in the engine is
written against the number of threads that exist.

`DESIGN.md` §1.4 fixes the inventory and the ownership split; §5.2 places the MacroEngine on the
message thread and makes its re-entrancy flag a plain `bool` on the strength of that placement.
The sibling product runs the same shape in shipped code — zero worker threads, FFT on the GUI
thread (`Anamorph:docs/architecture/THREAD_MODEL.md` [Verified]) — but Anabasis carries three
loads Anamorph does not, and each is a plausible argument for a worker: linear-phase FIR
oversampling kernels (§3.1), a 16× offline-render quality mode (§3.1), and an adaptive engine
with feature extraction plus Learn (§5.4). The P0 research pass named the first of these
explicitly as an open P0 decision "with no Anamorph precedent" — background FIR-design thread
versus precomputed kernels (`worklogs/2026-07-30-p0-anamorph-research.md`, `anabasis_notes`).
This ADR answers it.

## Problem

Three questions, none self-evident:

1. **How many threads, and does anything in Anabasis force a third?** Designing a linear-phase
   half-band FIR kernel set is not free, and doing it inside `prepareToPlay` lengthens a call
   the host makes while the transport may already be rolling. 16× oversampling multiplies both
   the buffer footprint and the per-block work. Either could be read as the classic
   "do it on a worker" case.
2. **What crosses between the threads, and in which direction?** Parameters have to reach the
   engine; meters, features and time-series history have to reach the UI; momentary commands
   (duck, Learn start/stop, meter hold reset) have to reach the audio thread. Each of these has
   at least two standard implementations, and the wrong choice for the parameter path in
   particular (a FIFO) makes per-block cost a function of user activity.
3. **Which thread owns the pieces that are neither pure DSP nor pure UI?** The MacroEngine
   writes managed parameters (§5.2); PDC has to be recomputed when a latency source moves
   (§3.3). Both touch state the other thread also touches, and both have an obvious-but-wrong
   home on the audio thread, where they would be cheap to call and impossible to make safe.

## Options

- **A. Two threads. Audio thread owns the engine and all analysis; message thread owns UI, the
  MacroEngine, Learn control and PDC; publication is relaxed atomics + SPSC rings; commands are
  single atomics. Chosen.** It is exactly the shape `THREADING_POLICY.md` permits, with no path
  outside its table, and it is proven in the sibling product at the same scale of metering and
  transition machinery. Every claim the rest of the design makes about determinism —
  Freeze's bit-repeatability (§5.4), the no-change gate (§1.1), the duck's silent-bottom swaps
  (§2.8) — is stated against a single audio-thread owner of engine state, and stays true here by
  construction rather than by discipline.
- **B. Add a background FIR-design worker for linear-phase kernels (and/or for 16× preparation).**
  Rejected for v1. It adds a thread — a Hard Stop and a Review Gate item — to satisfy a case that
  precomputation already covers: **all** oversampler instances, for **every** factor and **both**
  phase modes, are constructed and `initProcessing`'d at `prepare()`, so a runtime factor or
  phase change designs nothing, allocates nothing, and waits on nothing
  (`Anamorph:src/dsp/AnamorphEngine.cpp:44-56` [Verified] does this for three factors already).
  A worker would additionally have to publish a kernel set to the audio thread mid-stream — a new
  cross-thread path carrying a *payload*, which none of the permitted paths do — and would need a
  lifetime/cancellation story for the case where the user switches phase mode twice while a
  design is in flight. It buys a shorter `prepare()`; it costs the one property the whole
  threading model is chosen for.
- **C. Lock-free FIFO of parameter-change events, message → audio, instead of the per-block POD
  snapshot.** Rejected. Per-block work becomes a function of how fast controls are moving;
  coalescing, ordering and overflow policy become engine concerns; and there is no single object
  to compare, so the bitwise `sameParameters` no-change gate and the discrete diff that routes
  changes to the duck have nothing to operate on (ADR-0001). The snapshot is a small POD struct
  and rebuilding it unconditionally is cheaper than administering a queue. Anamorph proves the
  point in shipped code: no message queue, no FIFO of parameter changes
  (`Anamorph:src/PluginParameters.cpp:286-389`, `Anamorph:src/dsp/EngineParameters.h:28-103`
  [Verified]).
- **D. Run the macro mapping on the audio thread** (engine derives managed values from the macro
  positions at snapshot-adoption time). Rejected on two independent grounds. Mechanically, the
  mapper's output is `setValueNotifyingHost` on real parameters — a message-thread API with
  listener, gesture and undo consequences — so an audio-thread mapper would need a queue back to
  the message thread and its re-entrancy flag would have to become an atomic that can interleave
  with a user gesture, defeating the discriminator §5.2 depends on. Semantically it is the
  "internal derived values" option ADR-0005 already rejected: the Advanced view would display
  numbers the DSP is not using.
- **E. Recompute PDC from audio-thread state, or call `setLatencySamples` from the audio thread.**
  Rejected. `THREADING_POLICY.md` forbids it outright ("PDC/latency must be recomputed on the
  message thread via a `const`, race-free predictor"), and the reason is structural: the audio
  thread's *effective* latency follows the latched OS state, which by design changes only at a
  reset or the silent duck bottom, whereas the host wants the figure the moment a setting is
  chosen. Two owners of one number produce exactly the mid-block latency change the design
  forbids.
- **F. An analysis/metering worker** (spectrum FFT, gated integrated-LUFS accumulation, feature
  extraction, preset scanning). Rejected. Feature extraction is block-rate arithmetic on
  pre-allocated state and belongs where the samples already are; integrated LUFS is a fixed-size
  histogram accumulator, not a growing container, precisely so it stays on the audio thread
  (`REALTIME_AUDIO_POLICY.md` consequence 3); and the spectrum FFT runs GUI-side off the capture
  rings (§2.9), which is where Anamorph runs its FFT too. Nothing here is unbounded, so nothing
  here needs to be moved off the real-time path.
- **G. Per-module denormal handling** (flush-to-zero code inside each DSP module) instead of one
  `ScopedNoDenormals`. Rejected as the *mechanism*: it is per-module code that can be forgotten
  silently, and it duplicates what one scoped guard does for the whole block
  (`Anamorph:src/PluginProcessor.cpp:109` [Verified]). Note this rejects the mechanism, not
  `REALTIME_AUDIO_POLICY.md` consequence 6's additional advice that envelope state be flushed
  below a threshold — that is a numerical-robustness measure inside a module, not a second
  FTZ/DAZ mechanism.

## Decision

**Inventory. Two threads carry Anabasis work, and there are no worker threads in v1.**

**Audio thread** — sole owner of engine state. It runs: the chain (§1.2), all smoothing, the
transition state machine (§2.8), feature extraction (§5.4), meter ballistics (§2.9), and the
adaptive trim slewing. It adopts one POD `EngineParameters` snapshot per block, rebuilt by the
wrapper from cached `getRawParameterValue` atomics plus the `ANABASIS_INTERNAL` atomic mirrors
(ADR-0001). It never allocates, locks, paints or does IO. `juce::ScopedNoDenormals` at the top of
`processBlock` is the **single** FTZ/DAZ mechanism for the entire block; no module carries its own.

**Message thread** — owner of everything else: the editor and all visualisers, the MacroEngine
(§5.2), Learn start/stop control, preset/A-B/undo bulk swaps, `InternalState`, and PDC. Because
the macro parameters are non-automatable and the MacroEngine consumes macro changes only through
an async message-thread listener, the MacroEngine is message-thread-only **by construction** —
which is what lets its re-entrancy flag (§5.2's manual-edit discriminator) be a plain `bool` with
no atomics and no possibility of interleaving with a user gesture.

**Publication, audio → message** (audio thread is the only producer on every one of these):

- **Meters** — LUFS M/S/I, dBTP, PLR, GR: pre-converted values stored into `std::atomic<float>`
  in a single per-block `publish()`, `memory_order_relaxed`. All ballistics state stays in plain
  floats on the audio thread; the GUI does no maths (`Anamorph:src/gui/LevelMeter.cpp:12-73`,
  `Anamorph:src/dsp/ScopeBuffer.h:21-91` idiom family [Verified]).
- **Adaptive features** — short-term LUFS, crest factor, spectral tilt/centroid, transient
  density: the same relaxed per-block atomic publish (§5.4).
- **Time series** — GR/waveform history (`GrHistoryBuffer`) and the two spectrum capture points
  (post-input-gain and post-chain, §2.9): three SPSC rings, one producer and one reader each,
  power-of-two storage, **one release-store per block** on a monotonic write index, acquire on
  the read side, reads are stateless `const` peeks that consume nothing
  (`Anamorph:src/dsp/ScopeBuffer.h:21-91` [Verified]).

  > **Amended 2026-09-02 (0.2.8 review) — the ring PAYLOAD is atomic too, in `GrHistoryBuffer`.**
  > The release/acquire index above settles what a reader SEES and is unchanged. It does not make
  > the other case legal: a reader whose batch the producer laps reads the slot being written, and
  > under the C++ memory model a plain read concurrent with a plain write is a data race — undefined
  > the moment it happens, which no after-the-fact detection can undo. The ring's two guards (the
  > reset epoch, and the reader's `readFloor` window clamp) are built to DETECT exactly that case
  > and discard the frame, so detection was never the defect; what it was detecting was. The stored
  > fields are `std::atomic<float>` written and read **relaxed**, so the racing read is defined —
  > each field yields one of the two values, the pair may be mismatched — and the guards keep their
  > job unchanged. **Measured, not assumed: the audio-thread `push` compiles to an
  > instruction-identical sequence** (two `movss`, one `movq` publish; clang-22 `-O3`, x86-64), so
  > the realtime contract is untouched, and a `static_assert` on `is_always_lock_free` keeps it that
  > way. On the READER side the codegen changes and that is the point — the old build fused both
  > floats into one 8-byte `movsd`, i.e. exactly the wide non-atomic load this amendment removes.
  > `ScopeBuffer` is the same idiom and is **not** changed here: its reader takes 4096 of 16384
  > frames, so the producer must advance ~12288 frames (~0.26 s at 48 kHz) mid-read to reach them —
  > recorded as drift rather than repaired, because it is a separate component and outside this
  > round's scope.

  > **Amended 2026-09-02 (0.2.8 final review, same day) — the prepared (rate, block) pair is RING
  > METADATA published inside the clear, and the reader closes a batch with a FENCE.**
  > **✅ ACCEPTED BY THE OWNER 2026-09-02 — THE ARCHITECTURE REVIEW GATE IS CLEARED.** It was raised
  > as a gate item under `ARCHITECTURE_REVIEW_GATE.md`'s "**Thread Model change** — new thread, new
  > cross-thread path, new atomic ordering" row (the pair crosses through the ring rather than
  > through JUCE's members; the reader's close gains an acquire fence), flagged in the pull request
  > as something a green build does not clear, and held there until the owner answered. The approval
  > is of the amendment exactly as written below — the pair as ring metadata inside the clear's
  > seqlock window, `prepare` owning the clear-on-change gate, and `batchIntact` as the batch's
  > close — and it is not an instruction to revert any of it. No further action is pending on this
  > record. Two repairs
  > to the contract above; neither changes the decision. **(1)** `GrHistoryView` maps its window and
  > scroll rate through the prepared pair, and until this amendment read it from `AudioProcessor`'s
  > plain `currentSampleRate`/`blockSize` — written by `setRateAndBufferSizeDetails` on whichever
  > thread the host reconfigures on (the VST3 wrapper's `preparePlugin`, reached from
  > `setupProcessing`, `setActive` and `initialize`, always BEFORE `prepareToPlay`; every other
  > wrapper does the same), read by the tick on the message thread and by the frame on the painting
  > thread: a data race by the letter of the model. No repo-owned snapshot covered it — the wrapper's
  > `grRingPreparedRate/Block` were plain host-thread members that only gated the clear. The pair now
  > lives in `GrHistoryBuffer` as two relaxed lock-free atomics (`static_assert`ed), stored by `clear`
  > INSIDE its seqlock window — after the odd increment and its release fence, before the even release
  > increment — so a reader that brackets a batch with the epoch reads the pair and the entries as ONE
  > unit: a clear that ran through the batch is announced by the epoch and the frame is discarded,
  > exactly as it already was for the entries. The clear-on-change gate moved with it
  > (`GrHistoryBuffer::prepare`), so the pair the gate compares is the pair readers get. The audio
  > thread is untouched: `push` is unchanged, and the two stores sit on the host thread inside a clear
  > that already wrote 4096 slots. **(2)** The reader's CLOSE of a batch is `batchIntact`:
  > `std::atomic_thread_fence (acquire)` and then a relaxed re-read of the epoch. The acquire LOAD it
  > replaces orders later accesses after itself and says nothing about the relaxed loads sequenced
  > BEFORE it, so on a weakly ordered target a batch's loads could be satisfied after the epoch
  > re-read and a torn batch would pass as intact — on x86-64 (TSO) the hardware hid the gap, and
  > this tree paints on Apple silicon. The fence is the reader Boehm shows correct (*"Can Seqlocks
  > Get Along with Programming Language Memory Models?"*, MSPC 2012): a batch load that read a value
  > stored after `clear`'s release fence makes that fence synchronise-with this one, so the odd
  > increment happens-before the re-read and write-read coherence forbids it returning the old even
  > value; a batch that read only pre-clear values is a consistent pre-clear snapshot. That is the
  > C++ memory model's guarantee, not TSO's. What the fence does NOT buy, stated so nobody claims it
  > later: the `readFloor` re-read of `available()` after a batch is ordered after the batch's loads
  > by the same fence, but `push` has no release fence before its entry stores (deliberately — the
  > audio thread pays nothing), so the model gives that lap check no formal guarantee; it stays what
  > §10.3 of the worklog says it is, a defined-behaviour one-frame artefact rather than a race.
  > Pinned by `grPrepared` (`testGrHistoryReaderStaysInsideTheRingAndSeesEveryReset` §3d) through
  > the ring and through the wrapper's `prepareToPlay`; the audio-thread `push` is instruction-
  > identical to the previous amendment's measurement because it did not change.

  > **Amended 2026-09-02 (KI-015 follow-up) — the atomic-payload rule extends to `ScopeBuffer`, and
  > this SUPERSEDES the first amendment's closing sentence.** That sentence read "`ScopeBuffer` is
  > the same idiom and is **not** changed here … recorded as drift rather than repaired, because it
  > is a separate component and outside this round's scope." The scope reason stands as history; the
  > drift is now repaired, and the sentence no longer describes the tree.
  >
  > ✅ **ACCEPTED BY THE OWNER 2026-09-02 — THE ARCHITECTURE REVIEW GATE IS CLEARED.** How it
  > arrived stays in the record, because that is the half worth keeping: this amendment supersedes an
  > accepted sentence in an Accepted ADR, and `AI_AGENT_POLICY.md` makes "an existing Accepted ADR
  > conflict **detected**" a Hard Stop that a green build does not clear — so it was raised, flagged
  > in the pull request, and **held** rather than self-ruled, with the agent explicitly declining to
  > decide whether it was a repair under the existing SPSC-ring contract or a decision in its own
  > right. The owner has now ruled: the amendment is accepted **as written below**, superseding the
  > first amendment's "recorded as drift rather than repaired" sentence, and the approval is not an
  > instruction to revert any part of the repair. Nothing further is pending on this record.
  >
  > **The defect.** `ScopeBuffer`'s producer wrote its payload with `std::memcpy` on the audio
  > thread and its reader read the same `float` objects with plain subscripts on the message thread.
  > The release/acquire pair on the write index is sound and does exactly one job — every frame
  > strictly below the acquired index is complete — but it is a BACKWARD edge only: nothing in
  > `pushBlock` reads anything the reader writes, so the producer's SUBSEQUENT writes are unordered
  > against every iteration of the reader's copy loop. When the producer laps the reader mid-copy the
  > accesses conflict, and a plain read concurrent with a plain write is undefined the moment it
  > happens. Same defect class as the first amendment's, in the ring that amendment named.
  >
  > **Why the headroom defence was not an invariant** — this is the part `KNOWN_ISSUES.md` had wrong,
  > and the correction matters more than the fix. "4096 of 16384, so ~12288 frames (~0.26 s at
  > 48 kHz)" fails on three independent paths. (a) It is a FRAME count, not a time: 0.064 s at
  > 192 kHz. (b) `reset()` rewinds the head, so a reader holding a pre-reset index has a margin
  > anywhere in [0, capacity), not `capacity − count`. (c) `n` is bounded only by the engine's
  > `maxBlock`, which is the HOST's `samplesPerBlock` with no upper clamp — a push reaches the
  > reader's oldest slot at n ≥ 12289 and covers its whole window at n ≥ 16384, with no reader stall
  > at all. Those two thresholds are different and both were previously unstated. Possible by
  > construction; no in-tree stimulus prepares more than 512, so it is unexercised here rather than
  > known to be reachable in a shipping host.
  >
  > **The repair, confined to `src/dsp/ScopeBuffer.h`.** The payload element becomes a `Sample`
  > class wrapping one `std::atomic<float>` and exposing only relaxed `store`/`load`; `pushBlock`'s
  > two-segment `memcpy` becomes two store loops over the SAME segment arithmetic; `readLatest`'s
  > two subscripts gain `.load()`. Nothing else moves — same slots, same bytes, same values, same
  > single release-store publication and cadence, same reset protocol, same short-read clamp, same
  > heap storage and `sizeof`. `SpectrumView` is not touched.
  >
  > **No reader-side acquire fence, and this is deliberate — it is where this ring differs from its
  > sibling.** `GrHistoryBuffer` needed `batchIntact` because its `clear` WRITES THE PAYLOAD inside
  > the epoch window, so a reader's payload loads are the only thing that can witness a clear.
  > `ScopeBuffer::reset()` writes one atomic and touches no sample, so there is no clear-window store
  > for a payload load to synchronise through, and `[atomics.fences]` gives an acquire fence nothing
  > to attach to: the producer's payload stores are relaxed and no release fence precedes them. That
  > is the same holding the SECOND amendment above already states for the sibling's `push`, applied
  > to the identical producer shape, not a new claim. **Named premise it rests on:** `reset()` runs
  > from `prepare` with audio stopped — a host-API contract, not a C++ guarantee, and the same
  > premise the engine's reallocation of every other ring in `prepare` already depends on.
  >
  > **Measured, not assumed — and the first amendment's "instruction-identical" claim is explicitly
  > NOT transferable here.** `pushBlock` compiled at `-O3` for x86-64: **4 call sites → 0**, and
  > **0 lock-prefixed instructions**, under clang-22.1.8 and g++ 13.3 alike — the four `memcpy`
  > calls become inline scalar stores that neither compiler vectorises, because LLVM and GCC will
  > not vectorise atomic accesses. Cost, measured on the real workload (two rings × two channels per
  > chunk): **+0.56 µs per 512-frame block at 48 kHz, which is +0.005 % of the block period**, and
  > +0.021 % at 192 kHz/512, the worst ordinary cell. Against `PERFORMANCE_BUDGET.md`'s recorded
  > 625.4 ns/sample working cell that is a relative delta under 0.2 %. It is deliberately NOT
  > presented as an `AnabasisBench` delta: at 0.005 % of realtime the change is far below that
  > harness's run-to-run resolution, so a bench table would report noise and call it evidence.
  >
  > **Two spellings are now forbidden in that header, and the record states which gate catches
  > which, because the answer is asymmetric.** A re-introduced `memcpy` over the payload is
  > diagnosed by GCC (`-Wclass-memaccess`, on the "no trivial copy-assignment" criterion) and the
  > zero-first-party-warning gate makes that a red job — but **clang-22 is silent even with
  > `-Wnontrivial-memaccess`, measured**, so the GCC lanes are the whole of the automated catch.
  > `slot = value` on a bare `std::atomic<float>` would select the SEQ_CST `operator=` — a fenced
  > store per sample on the audio thread that no instrument in this tree reports — and the `Sample`
  > wrapper makes that spelling ill-formed rather than merely discouraged. Note also that
  > `std::is_trivially_copyable` reports TRUE for such a wrapper on both libstdc++ and libc++ despite
  > every copy and move operation being deleted, so it is not the property to lean on; the test
  > asserts copy-constructibility and copy-assignability instead.
  >
  > **Anamorph keeps the unrepaired shape.** ADR-0009 item 8 makes divergence accepted and one-way,
  > and `CLAUDE.md` §3 makes that repository read-only from here, so no sibling change is owed. The
  > instance is recorded as **KI-016** rather than left as an undocumented difference — "accepted
  > drift" against a named instance, not against a wording.

  > **Amended 2026-09-02 (round 6) — `push` release-fences, so the GR ring's LAP CHECK now has the
  > formal guarantee the second amendment said it lacked. This SUPERSEDES that amendment's closing
  > paragraph.** That paragraph read: "`push` has no release fence before its entry stores
  > (deliberately — the audio thread pays nothing), so the model gives that lap check no formal
  > guarantee; it stays … a defined-behaviour one-frame artefact rather than a race." It was an
  > accurate statement of a residual and the round that wrote it declined to close it. Review found
  > the residual is not benign in the way "one-frame artefact" suggests: the frame is **accepted and
  > drawn carrying entries the producer already overwrote**, not dropped.
  >
  > ⚠️ **RAISED AT THE ARCHITECTURE REVIEW GATE AND HELD — NOT SELF-RULED.** It adds a new atomic
  > ordering on the audio path (`ARCHITECTURE_REVIEW_GATE.md`'s "Thread Model change" row) and it
  > supersedes a paragraph inside a block the owner has already accepted, which `AI_AGENT_POLICY.md`
  > makes a Hard Stop on DETECTION, whatever the agent thinks of the severity. The owner's ruling is
  > owed; the action is the same either way — flag, hold, do not merge on a green build.
  >
  > **The defect.** `push` stored its payload relaxed and then released the INDEX. A release store
  > orders what precedes it, never what follows, so push #P's payload stores could become visible to
  > a reader BEFORE push #(P−1)'s release store of the index. A reader whose relaxed `peek` returned
  > push #P's value therefore had no edge forcing its closing `available()` re-read to observe P, and
  > `GrHistoryView`'s `first < readFloor (available())` — the lap check, the whole reason `peek`'s
  > comment says "the caller re-checks its window afterwards and throws such a frame away" — could
  > legally return false. Classification, kept separate because the round's three registers matter:
  > **defined but wrong data** (every conflicting access is atomic since the first amendment, so
  > never UB), and NOT "one frame late" (the frame is drawn, not dropped).
  >
  > **The repair.** `push` now carries `std::atomic_thread_fence (std::memory_order_release)` before
  > its payload stores — the same fence `clear` has carried since 0.2.8, for the same reason with
  > `writeIndex` in place of `resetGuard`. `GrHistoryView`'s post-check became two sequenced
  > statements rather than one `||`, because the peek → fence → re-read ordering is now load-bearing
  > for the lap as well as the epoch and `||`'s (guaranteed) sequencing is too easy to edit away.
  >
  > **The proof, in both cases, because the naive one is wrong.** [atomics.fences]: the fence (A) is
  > sequenced before the payload stores (X); the reader's peek (Y) reads X's value and is sequenced
  > before `batchIntact`'s acquire fence (B); so A synchronises with B, and everything sequenced
  > before A — including push #(P−1)'s `writeIndex.store (P, release)` — happens-before everything
  > sequenced after B, which includes the `available()` re-read.
  > *Case 1, no clear intervened:* `writeIndex`'s modification order is then increasing, so write-read
  > coherence forces the re-read ≥ P ≥ first + kSize, hence `readFloor` > `first` and the frame is
  > discarded.
  > *Case 2, a clear intervened:* the naive coherence step does NOT apply — `clear` stores
  > `writeIndex.store (0, release)`, so that modification order is not monotone in value and the
  > re-read may legally be 0. The discard comes from the OTHER half of the same post-check: whichever
  > store the reader's peek read from is sequenced after a release fence (`push`'s or `clear`'s), so
  > the same pairing puts `clear`'s odd `resetGuard` increment — or its closing even one —
  > happens-before the relaxed epoch re-read inside `batchIntact`, which therefore differs from
  > `epoch0` and discards the frame. **Both halves of the conjunction are load-bearing and neither
  > alone suffices**; that is why the post-check keeps both and why they are sequenced.
  > *Either the batch read clean data, or the discard is guaranteed. There is no third case.*
  >
  > **A release FENCE, not release payload stores.** [atomics.fences] would also admit the latter (a
  > release operation synchronises with an acquire fence when an operation on the same object,
  > sequenced before that fence, reads its value — which the peek is). The fence is chosen because it
  > is ONE barrier covering both fields rather than two release stores, and because it makes `push`
  > and `clear` the same shape.
  >
  > **Cost, measured independently of the patch's own comment:** `push` compiled at `-O3` is
  > **instruction-for-instruction identical on x86-64** — the fence emits `#MEMBARRIER`, a directive,
  > not an instruction — and adds **exactly one `dmb ish` on AArch64**, once per HOST BLOCK, since
  > `push` runs once per `processBlock` and never per sample.
  >
  > **Pinned where it can be pinned.** No deterministic suite can distinguish the two builds: the
  > difference is a synchronises-with edge, orderings are not introspectable, and unlike the first
  > amendment's payload change no TYPE moved. `scripts/check-realtime.py` therefore gained a second
  > mode — `REQUIRED_ORDER` — which fails the tree when `GrHistoryBuffer::push` lacks the fence, when
  > the fence has drifted below the payload stores, when it is the wrong fence, when it is only in a
  > comment, and when the function has been renamed out from under the rule. Six self-test cases,
  > both directions.
- **Staleness hints** — relaxed monotonic generation counters carrying no payload.

**Commands, message → audio** — one `std::atomic` per request, consumed with `exchange` at the
top of the audio-thread consumer: the forced-duck request (set *before* the parameter swap,
§2.8), Learn start/stop, meter hold reset, and the sentinel-valued per-slot inject atomics that
deliver a frozen trim vector at the silent duck bottom (§4.4, §5.4 — the `abMatchGain` idiom,
`Anamorph:src/PluginProcessor.cpp:485-491` [Verified]). **The trim-vector case is under-specified
here and is deliberately left so:** the precedent carries one scalar, the vector is four, and the
transport is `OPEN_QUESTIONS.md` **OQ-013** — a Hard Stop until an ADR settles it. The other three
commands are single scalars and are fully covered. Host-hidden engine config
(`int_oversample`, `int_osPhase`, `int_offlineQuality`) crosses through the `InternalState`
atomic mirror, not through a command.

**PDC.** Latency is recomputed **never on the audio thread — never inside `processBlock`** — by a `const`, race-free
`predictLatency(snapshot)` taking the same POD type the engine renders from, feeding a **single**
`setLatencySamples` call site (`Anamorph:src/PluginProcessor.cpp:88-105` [Verified]). It is
invoked from `prepareToPlay`, from the `InternalState` change callbacks for **all three**
latency-bearing host-hidden fields — `int_oversample`, `int_osPhase` **and
`int_offlineQuality`** — and from **`setNonRealtime()`**. Under ADR-0004 those are the only
remaining latency sources, so no APVTS listener needs to drive PDC at all.

> **Correction, same day (2026-07-31), for consistency with ADR-0004 — not a change of decision.**
> An earlier revision of this paragraph listed only the OS factor and phase. That contradicted
> ADR-0004 decision item 5: at `int_offlineQuality = Force Max` an offline bounce renders at 16×
> and *"the reported figure during `isNonRealtime()` uses the forced factor"*, which makes
> `int_offlineQuality` a **third** input to reported latency and the realtime→offline transition a
> **fourth** recompute trigger. Left as written, a P1 author would wire no recompute for a
> Force-Max change, and — in hosts that do not re-`prepare` when entering offline render — the host
> would compensate for the live factor while the render ran at 16×, time-shifting the bounce
> against the rest of the project. `setNonRealtime()` is named explicitly because it is the only
> callback guaranteed to fire on that transition.

> **Second correction, same day (2026-07-31) — a wording fix that makes the rule satisfiable.** This
> paragraph opened "Latency is recomputed **only** on the message thread", and
> `THREADING_POLICY.md`'s forbidden-access list said the same. Neither `prepareToPlay` nor
> `setNonRealtime()` is a message-thread callback — hosts call them from their own setup/processing
> threads — so two of the call sites this very paragraph mandates could not be honoured without
> breaking the rule, leaving a P1 author no compliant way to satisfy ADR-0004 item 5. The **substance
> was never thread-identity**: it is that the predictor is `const` and race-free, that a single
> `setLatencySamples` call site exists, and that nothing recomputes PDC from `processBlock`. Restated
> as *never on the audio thread* in both records; the mechanism, the trigger list and the call-site
> rule are unchanged. Enacted as a policy amendment below (`ADR_POLICY.md` rule 5).

The audio thread's effective latency follows its own latched OS state and is never written from the
message thread. The engine's dry-fill gate compares the predicted figure against the latched one
before engaging (`Anamorph:src/dsp/AnamorphEngine.cpp:290-307` [Verified]).

**No worker threads, and the two reasons one might be wanted are closed at `prepare()`.** Every
oversampler instance — every factor in `Off/2×/4×/8×/16×`, both phase modes — is constructed and
`initProcessing`'d at `prepare()`, and the linear-phase FIR kernels are designed there, so a
runtime factor or phase change is a latched selection among objects that already exist: it
designs nothing, allocates nothing and blocks nothing. All buffers — the lookahead line, the dry
ring (`maxLookahead(10 ms) + maxOsLatency(16×, linear) + maxBlock + 1`, §3.3), and the
oversampled scratch — are sized at `prepare()` for the **maximum** factor, which is what makes
the 16× Force-Max offline path (§3.1) require no separate machinery: it is the same latched
switch on buffers that were already large enough.

**Adding any thread later** — worker, analysis, scanner or otherwise — is an Architecture Review
Gate item and an AI-agent Hard Stop, and requires an ADR superseding this one.

## Consequences

- `THREADING_POLICY.md`'s permitted-path table is satisfied with **no path outside it**: every
  cross-thread edge above is one of its seven rows — six as originally written, plus the
  sentinel-valued single-scalar command row this ADR adds (see *Policy amendments* below; an earlier
  revision of this sentence claimed compliance against the six original rows while listing an edge
  none of them described) — **with one edge excepted: the frozen trim vector's restore path, whose
  mechanism is not settled by any row and is deferred to an ADR under OQ-013.** That exception is
  stated rather than papered over: an intermediate revision of the new row claimed the trim vector as
  its use while simultaneously excluding anything wider than one scalar, which is self-refuting and
  would have left an implementer with either no permitted mechanism or an unordered four-atomic
  publish. The policy's "Adaptive engine — where it runs"
  clause, which explicitly defers the choice to "an ADR before implementation", is discharged
  here: feature extraction and trim slewing run on the audio thread within the real-time budget
  (§9's ≤0.5% metering-and-features allocation), not on a worker and not on the message thread.
- The audio thread is the single owner of engine state, so Freeze's bit-repeatability
  (`MODE_AND_ADAPTATION_POLICY.md` invariant 3), the bitwise no-change gate, and the duck's
  silent-bottom swap all reason about one writer. No engine field needs to be atomic; the atomics
  in the design are boundary objects only.
- **`prepare()` gets more expensive, and that is the accepted trade.** Designing linear-phase FIR
  kernels and `initProcessing`-ing every oversampler up front moves work into a call the host
  makes off the audio thread, in exchange for a runtime factor/phase switch that cannot allocate
  or stall. Memory is likewise sized for 16× linear-phase whether or not the user ever selects
  it. Both costs are bounded and paid once per `prepareToPlay`; a worker would have traded them
  for an unbounded new failure surface.
- **Forecloses**, without an ADR superseding this one: any background analysis (offline loudness
  scan, batch preset analysis, asynchronous preset-folder scanning), any deferred kernel design,
  any audio-thread parameter write, and any second producer on a scope/GR ring.
- Parameter changes land at block boundaries; sample-accurate intra-block automation is
  foreclosed (ADR-0001's consequence, restated because it is a threading property).
- Macro behaviour under a host that exposes the non-automatable macro parameters anyway
  (`PARAMETER_COMPATIBILITY_POLICY.md` rule 5) is message-thread-rate mapping, and
  offline-render determinism for that unsupported usage is explicitly not promised (§5.2). This
  is a consequence of the MacroEngine's thread placement, so it is recorded here as well as in
  ADR-0005.
- The UI reads published atomics and ring peeks only — never engine state — so the vblank-paced
  FrameClock and the snapshot repaint gates (§6.5, `Anamorph:src/gui/FrameClock.h:10-167`
  [Verified]) can be copied without any locking layer, and an idle or hidden visualiser costs
  nothing.
- **One inventory nuance to record precisely at P1, not resolved here.** `THREADING_POLICY.md`
  names an "(optional) GPU render context" in its own inventory, and §6.1 attaches OpenGL on
  macOS/Windows (never Linux/X11). That context is created and driven by JUCE, is not a worker
  thread, and holds no Anabasis state — but component painting runs on it when attached, while
  the policy's ring rule is phrased with the message thread as the single reader. The rule that
  keeps this safe either way is already mandated and already the design: ring reads are
  **stateless `const` peeks**, so multiple read sites are safe. `docs/architecture/THREAD_MODEL.md`
  (P1) records which context paints, per repository; no policy amendment is asserted here.
- `docs/architecture/THREAD_MODEL.md` becomes a required P1 deliverable — `THREADING_POLICY.md`
  points at it for the concrete model and it does not yet exist. It is written from this ADR with
  code citations, and `REALTIME_SAFETY_AUDIT.md` (end of P2) audits compliance; neither is
  claimed in advance (C7).

## Policy amendments enacted by this ADR

`ADR_POLICY.md` rule 5 makes an ADR the instrument that changes a Policy, and this change set treats
an ADR/policy divergence as a defect. The Consequences section above states that
`THREADING_POLICY.md`'s "Adaptive engine — where it runs" clause "is discharged here"; that
discharge is carried as a prescribed block, matching the enacted text verbatim.

- **"Adaptive engine — where it runs" stops deferring the placement.** The clause's closing sentence
  ("Feature extraction and macro mapping run on the audio thread within the real-time budget, or on
  the message thread reading published values — whichever the P4 design chooses, it must be one of
  the paths above, decided in an ADR before implementation") is replaced by:

  > This clause previously deferred the placement choice to "an ADR before
  > implementation"; that deferral is **discharged by ADR-0011** (Accepted 2026-07-31) and the
  > placement is now fixed, not open:
  >
  > - **Feature extraction and adaptive trim slewing run on the audio thread**, inside the real-time
  >   budget (`DESIGN.md` §9's ≤ 0.5 % metering-and-features allocation) — not on a worker, and not on
  >   the message thread.
  > - **Macro mapping (the MacroEngine) runs on the message thread only**, by construction: the macro
  >   parameters are non-automatable (ADR-0005) and the engine consumes macro changes solely through
  >   an async message-thread listener.
  >
  > Implementing either piece on a different thread contradicts an Accepted ADR — a Hard Stop, not a
  > design choice.

  The opening sentence ("The Simple-mode adaptive engine … is **not** a licence for a worker
  thread") is unchanged — this ADR strengthens it rather than replacing it.

- **The permitted-path table gains a seventh row, for a payload-carrying command atomic** *(added
  2026-07-31, same day)*. The Decision's "Commands, message → audio" paragraph routes the **frozen
  trim vector** (ADR-0007) through sentinel-valued per-slot inject atomics, which *carry a value*.
  The table's nearest row — "GUI → Audio (momentary / transient requests) | a single
  `std::atomic<int>` per request" — describes a payload-free integer request where the arrival *is*
  the message; the Anamorph precedent this copies is an `atomic<float>` carrying a gain
  (`Anamorph:src/PluginProcessor.cpp:485-491` [Verified]). Since the policy states "Any path not in
  this table is a new cross-thread path → Architecture Review Gate", the Consequences claim of full
  compliance was asserting conformance to a row that did not describe the edge. This ADR enacts the
  missing row for the case the `abMatchGain` precedent actually establishes — a **single scalar** —
  so the table describes it before `THREAD_MODEL.md` is generated from this ADR at P1. The
  multi-scalar trim-vector case is **not** covered by it and is excepted from the compliance claim;
  see the correction below and OQ-013. Appended after the momentary-request row:

  > | GUI → Audio (sentinel-valued command **carrying one scalar**) | one `std::atomic<float>` per slot, an out-of-range **sentinel** meaning "nothing pending" | The `abMatchGain` idiom (ADR-0011): the writer stores the value, the audio thread `exchange`s the sentinel back in, so arrival and payload are one indivisible operation and no second flag can tear against it. One writer, one consumer, **one scalar** per slot — a *bounded* set of slots fixed at compile time, never a queue. Anything unbounded, wider than one lock-free scalar, or needing ordering against other state is **not** this row and is a new cross-thread path. |

  The existing momentary-request row gains the clarifying tail "Payload-free: the *arrival* is the
  whole message." so the two rows cannot be confused. The boundary in the new row's last sentence is
  the load-bearing part: it keeps the row from becoming a licence for a general message queue, which
  **is** a thread-model change and an Architecture Review Gate item.

  > **Correction, same day (2026-07-31) — a scope narrowing, and the gap it exposes is left open
  > rather than closed here.** The row as first drafted said "carrying a value … one value per slot"
  > and named the **frozen trim vector** as its use, while its own closing sentence excluded anything
  > "multi-word". Those cannot both hold: the trim vector is four scalars (release, stereo-link,
  > sidechain-HPF, dynamic-tilt — `DESIGN.md` §5.4, ADR-0005 item 10), so read strictly the restore
  > path had no permitted mechanism, and read loosely an implementer would publish four independent
  > atomics with no ordering and consume them half-updated — a permanently half-restored slot, which
  > defeats the Freeze bit-repeatability `MODE_AND_ADAPTATION_POLICY.md` invariant 3 requires. The
  > row is therefore narrowed to **one scalar**, which is exactly what the `abMatchGain` precedent
  > establishes (`Anamorph:src/PluginProcessor.cpp:485-491` [Verified] carries a single gain), and
  > the trim-vector transport is **not decided here**. ADR-0007's phrase "a sentinel-valued atomic"
  > is singular and does not settle it either. Choosing between *N* parallel sentinel scalars with a
  > stated ordering guarantee and a single release/acquire-gated per-slot POD is a thread-model
  > decision: Architecture Review Gate + ADR + Hard Stop, raised as **OQ-013**. Deciding it inside
  > this correction would be exactly the invention the gate exists to prevent.

- **The PDC forbidden-access rule is restated so it can be obeyed** *(added 2026-07-31, same day;
  see the second correction under §Decision "PDC")*. It read "PDC/latency must be recomputed on the
  **message thread** via a `const`, race-free predictor — never by mutating audio-thread state from
  the message thread", which no implementation can satisfy: `prepareToPlay` and `setNonRealtime()`
  are host callbacks not delivered on the message thread, and ADR-0004 item 5 mandates both as
  recompute triggers. Prescribed replacement:

  > - PDC/latency must be recomputed **off the audio thread** — never inside `processBlock` — via a
  >   `const`, race-free predictor feeding a single `setLatencySamples` call site, and never by mutating
  >   audio-thread state from the message thread. *(Amended by ADR-0011, 2026-07-31: this rule said "on
  >   the **message thread**", which is unsatisfiable — `prepareToPlay` and `setNonRealtime()`, two of
  >   the recompute call sites ADR-0004 item 5 mandates, are host callbacks that JUCE does not deliver
  >   on the message thread. The substance is unchanged: the predictor is `const` and race-free, so the
  >   rule never depended on which non-audio thread ran it.)*

  This narrows nothing: every property the original protected — const predictor, no races, one call
  site, nothing driven from `processBlock` — is stated explicitly rather than implied by a thread
  name that was the wrong one.

**Not amended, deliberately.** The forbidden-access rule "**No second producer** on a scope/GR ring,
and no reads off the message thread" stays exactly as written. The Consequences section records the
OpenGL-context nuance and states that no policy amendment is asserted for it; the policy carries a
non-normative parenthetical pointing here, so a contributor meets the nuance at the rule without the
rule moving. `docs/architecture/THREAD_MODEL.md` (P1) settles it per repository.

## Related code

None yet — P1 onward. Planned: `src/PluginProcessor.{h,cpp}` (`processBlock` with
`ScopedNoDenormals` at the top, per-block snapshot build, command atomics, `predictLatency` and
the single `setLatencySamples` call site), `src/PluginParameters.{h,cpp}` (cached
`getRawParameterValue` atomics, `toEngine()`), `src/InternalState.h` (host-hidden atomic mirrors
and the PDC-recompute callback), `src/MacroEngine.{h,cpp}` (message-thread mapper and its
re-entrancy flag), `src/PresetManager.{h,cpp}`, `src/dsp/AnabasisEngine.{h,cpp}` (single-owner
engine state, transition state machine, duck/inject atomic consumers),
`src/dsp/EngineParameters.h`, `src/dsp/GrHistoryBuffer.h` (SPSC ring),
`src/dsp/LoudnessMeter.{h,cpp}`, `src/dsp/LoudnessComp.{h,cpp}`,
`src/dsp/AdaptiveEngine.{h,cpp}` (audio-thread features, trims, Learn accumulation),
`src/gui/FrameClock.h`, and the visualisers `GrHistoryView`, `LoudnessMeterView`, `SpectrumView`,
`CurveView`.

Evidence [Unverified]:
- Design: `docs/DESIGN.md` §1.4 (the thread inventory, the audio/message ownership split, relaxed
  atomics + SPSC publication, command atomics, `ScopedNoDenormals` as the single FTZ/DAZ
  mechanism, `predictLatency(snapshot)` with a single `setLatencySamples` call site, and "no
  worker threads in v1" with the precomputed-kernel/`initProcessing` rationale and the 16×
  buffer-sizing rule), §5.2 (MacroEngine on the message thread; the re-entrancy flag needs no
  atomics because it is message-thread-only by construction; the async-listener path for hosts
  that expose the macro anyway), §1.1 and §1.3 (POD snapshot boundary; planned modules), §2.8
  (forced-duck atomic set before the swap; only OS factor/phase latches), §2.9 (meter atomics,
  the GR SPSC ring, the two spectrum capture rings, GUI-side FFT, fixed-size LUFS accumulator),
  §3.1 and §3.3 (all oversamplers prepared up front; dry-ring sizing; latency sources), §5.4
  (audio-thread feature extraction, trim slewing, Learn, per-slot sentinel inject), §6.5 (UI
  reads published atomics only), §9 (metering + features budget allocation), §10 (ADR-0011 scope).
- Research: `worklogs/2026-07-30-p0-anamorph-research.md` — "Anamorph has zero worker threads
  (THREAD_MODEL.md:13, FFT on GUI thread); Anabasis's linear-phase mode and 16x offline
  oversampling may not fit that constraint — a P0 decision (background FIR design thread vs
  precomputed kernels) with no Anamorph precedent" (`anabasis_notes`); "Meter/scope data crossing
  to the GUI thread" (SPSC ring + relaxed-atomic publish, `exchange` command channel);
  "Parameter snapshot pattern (APVTS -> audio thread)" (no message queue, no FIFO of parameter
  changes); "Denormal / FTZ and non-finite handling"; `patterns_to_copy` entries for the SPSC
  ring, the per-block atomic publish, the single `setLatencySamples` call site, and the
  pre-built/`initProcessing`'d oversamplers.
- Precedent [Verified — cited via `DESIGN.md` §1.4]:
  `Anamorph:docs/architecture/THREAD_MODEL.md` (zero worker threads),
  `Anamorph:src/PluginProcessor.cpp:109` (`ScopedNoDenormals` as the single FTZ/DAZ mechanism),
  `Anamorph:src/PluginProcessor.cpp:88-105` (const race-free `predictLatency`, single
  `setLatencySamples` call site),
  `Anamorph:src/dsp/AnamorphEngine.cpp:44-56` (all oversamplers pre-built and
  `initProcessing`'d at prepare, so a factor switch never allocates).
- Precedent [Verified — cited via `DESIGN.md` §1.1, §2.9, §3.3, §5.4, §6.5]:
  `Anamorph:src/PluginParameters.cpp:286-389` and `Anamorph:src/dsp/EngineParameters.h:28-103`
  (per-block POD snapshot instead of a parameter FIFO),
  `Anamorph:src/dsp/ScopeBuffer.h:21-91` (SPSC ring, one release-store per block, stateless
  acquiring peeks), `Anamorph:src/dsp/AnamorphEngine.cpp:290-307` (dry-fill gated on
  predicted == latched latency), `Anamorph:src/PluginProcessor.cpp:485-491` (sentinel-atomic
  per-slot inject consumed at the duck bottom), `Anamorph:src/PluginProcessor.cpp:338-421`
  (gesture-bracketed edits vs automation folded into the baseline — the message-thread signal
  §5.2's discriminator reuses), `Anamorph:src/gui/FrameClock.h:10-167` and
  `Anamorph:src/gui/LevelMeter.cpp:12-73` (vblank pacing, atomic meter sources, repaint gates).
- Policy: `docs/policies/THREADING_POLICY.md` (thread inventory, permitted-path table, forbidden
  cross-thread access, atomic ordering rules, adaptive-engine clause deferred to this ADR,
  enforcement); `docs/policies/REALTIME_AUDIO_POLICY.md` (lock-free/allocation-free rule,
  `ScopedNoDenormals` required, consequences 1–6); `docs/policies/ADR_POLICY.md` (threading model
  is ADR-mandatory); `docs/policies/ARCHITECTURE_REVIEW_GATE.md` and
  `docs/policies/AI_AGENT_POLICY.md` (thread-model change is a Hard Stop).
- Related ADRs: ADR-0001 (POD snapshot boundary, block-boundary parameter adoption), ADR-0004
  (latency contract — why OS factor/phase are the only PDC inputs), ADR-0005 (macro-layer
  contract whose threading half is settled here), ADR-0007 (per-slot state whose audio-side
  restore happens at the duck's silent bottom — by a transport **OQ-013** has not yet fixed; see
  the exception in §Consequences).
- Anabasis runtime claims are **Unverified** by construction: no `src/` exists at sign-off. Every
  statement about Anabasis behaviour above is the contract the P1+ code must satisfy, not a
  measurement (C2). Code evidence accrues from P1.
