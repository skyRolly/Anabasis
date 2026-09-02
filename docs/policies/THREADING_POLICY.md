# THREADING_POLICY.md

**Priority: 2.** System Policy. The implemented model is described in
`docs/architecture/THREAD_MODEL.md` (written at P1).

## Threads

Audio · Message/GUI · (optional) GPU render context · **no worker threads**.

Adding any thread — including a worker for analysis, preset scanning, or metering — is a
**Thread Model change**: Architecture Review Gate + ADR + AI Agent Hard Stop.

## Allowed communication paths (only these categories)

| Direction | Mechanism | Rule |
|---|---|---|
| GUI → Audio (automatable params) | APVTS `std::atomic<float>*` | Read **once per block** into the `EngineParameters` POD; never read piecemeal mid-block. |
| GUI → Audio (host-hidden session state) | `InternalState` ValueTree + an atomic mirror | Only the values the audio thread actually needs cross (e.g. oversampling factor, phase mode, offline-render quality). |
| GUI → Audio (momentary / transient requests) | a single `std::atomic<int>` per request | e.g. meter reset, `Learn` start/stop, `Freeze` — consumed with `exchange` on the audio thread. Payload-free: the *arrival* is the whole message. |
| GUI → Audio (sentinel-valued command **carrying one scalar**) | one `std::atomic<float>` per slot, an out-of-range **sentinel** meaning "nothing pending" | The `abMatchGain` idiom (ADR-0011): the writer stores the value, the audio thread `exchange`s the sentinel back in, so arrival and payload are one indivisible operation and no second flag can tear against it. One writer, one consumer, **one scalar** per slot — a *bounded* set of slots fixed at compile time, never a queue. Anything unbounded, wider than one lock-free scalar, or needing ordering against other state is **not** this row — a bounded record of several scalars is the **staged-record** row below (ADR-0012), and anything wider than that is a new cross-thread path. |
| GUI → Audio (**bounded staged record**) | *N* lock-free scalars + **one** `std::atomic<bool>` flag, `release`-stored after the payload | **ADR-0012.** The payload is stored `relaxed` first, the flag `release`-stored after; the audio thread `exchange`es the flag with `acquire` **at a block top** and only then reads the payload, so the record adopts as a unit. Conditions, all mandatory: *N* fixed at compile time (no allocation, no container, no variable length) · one writer thread (off the audio thread), one consumer · **last-writer-wins only** — an unconsumed record is overwritten, never queued · the writer may `const`-acquire-load the flag to ask "taken yet?" · the consumer does nothing but adopt. Anything queued, multi-writer, unbounded, or consumed mid-block is **not** this row. |
| Audio → GUI (scope / GR history) | SPSC ring buffer | Exactly **one producer thread and one reader thread**; release/acquire on the write index, published **once per COMMITTED UNIT** — a whole host block for the GR history, a processing CHUNK for the spectrum taps, which publish inside `processChunk` and therefore several times when a host block exceeds the prepared size. The guarantee the wording exists for is unchanged either way: a reader that acquires the index sees every frame below it complete, because the payload is written before the index is released. Only the reader's "has anything new arrived?" cadence differs. |
| Audio → GUI (meters: LUFS M/S/I, dBTP, PLR, GR, and the ADR-0020 statistics rows — sample peak, RMS, ungated integrated, LRA) | published `std::atomic<float>` | Audio writes in a single `publish()`; GUI reads via getters. `memory_order_relaxed` — monotonic display data, no ordering role. A non-audio thread may write the CLEARED values (prepare, state load) through the same helper: the scalars are independent and carry no ordering role, so the row is about the values' meaning, not about a single writer. |
| Audio/param → GUI (staleness hints) | `std::atomic<uint32>` generation counters | A monotonic "something changed" hint that lets the GUI skip rebuilding caches. Carries **no payload**, so relaxed is sufficient. |
| Any thread → Message (listener → async drain guard) | one `std::atomic<bool>` pending flag + one `std::atomic<int>` suppression depth | The `juce::AsyncUpdater` shape ADR-0005/ADR-0011 already mandate ("the MacroEngine consumes macro changes solely through an async message-thread listener" — an AsyncUpdater is itself an atomic flag plus a message post), written down as a row per the OQ-014 owner call (2026-08-02, reading 1: documentation gap, not a new mechanism). The flag is set from whichever thread delivers the listener callback and drained **only** on the message thread; the depth guard (`ScopedRestore`) suppresses the drain across a restore. Payload-free — the parameters themselves travel through APVTS. Residual check-then-act window: `KNOWN_ISSUES.md` KI-003. |
| Message → **Painting** (display bookkeeping the paint path reads: one editor counter; the GR history's two scroll scalars) | `std::atomic` scalars, `memory_order_relaxed`, read-only from the painting side | **ADR-0027 (Accepted 2026-08-14).** `drawResizableFrame` must tell a parented pop-up from a `ResizableBorderComponent`, and asks the editor `isPopupMenuOnScreen`. That call comes from `paintOverChildren`, i.e. from the GL render thread wherever the context is attached. Permits ONE scalar carrying no payload and no ordering: it orders no other memory, is not a handshake, and a one-tick-stale read draws the border it would have drawn a frame earlier. A hook the paint path invokes is torn down only AFTER that thread is joined (`glContext.detach()` first), because assigning to a live `std::function` races on the callable regardless of what it reads. Anything with a payload, anything the paint path WRITES, or anything needing two values seen consistently is a new path again. **Widened by ADR-0038 (Proposed 2026-09-02, gate NOT cleared) to a second site carrying TWO scalars:** `GrHistoryView`'s `shownHead` and `smoothHead`, published by its frame-clock tick and read by `paintHistory` to place the 0.2.8 sub-entry scroll. They are read as a pair and are safe by VALUE rather than by consistency — every stale/fresh pairing resolves to `min (smoothHead, head + 1)`, a position between two frames the ramp itself produces, so a torn read draws a frame the display was about to draw and no vertex moves rightward (`GrHistoryView::frameFor`, pinned by `grPair`). The boundary moves rather than opening: a pair whose cross-pairings are NOT legal frames still needs consistency and is still a new path. Lock-freedom is `static_assert`ed, because a target where these are not lock-free would put a lock in the paint path. |


Any path not in this table is a new cross-thread path → Architecture Review Gate.

The **Message → Painting** row is the first entry here that is not audio↔GUI, and it was added
RETROACTIVELY: 0.1.4 introduced the path without flagging it, and the `THREAD_MODEL.md` paragraph
written in the same round states the trigger verbatim — "the first GUI-side cross-thread read that
is NOT a stateless `const` peek" — while the change went in ungated and the pull request asserted
"no threading change". The sentence immediately above is what should have stopped it. The row was
written pending ADR-0027 and is now settled: the owner ratified it on 2026-08-14.

> **The frozen trim vector — formerly this table's one knowingly missing row — is wired
> (ADR-0014, 2026-08-02, resolving OQ-013).** ADR-0012 settled the transport (a four-scalar
> record on the staged-record row above, so no half-consumed vector can leave a slot permanently
> half-restored); ADR-0014 took the product decision ADR-0012 deliberately left open: injection is
> permitted **only for a freeze-ON adopted surface**, and the vector is applied where every
> restore-driven discontinuity lands — the §2.8 duck's silent bottom or the unprimed
> direct-adopt — after which Freeze holds it exactly. The Hard Stop this banner carried is
> lifted; the history stays in `OPEN_QUESTIONS.md` OQ-013.

## Forbidden cross-thread access

- No painting, allocation, locking, or IO on the audio thread.
- No direct access to non-atomic shared state across threads (the only synchronisers are the
  listed atomics + the SPSC ring).
- **No second producer** on a scope/GR ring, and no reads off the message thread. Reads must be
  stateless `const` peeks so multiple message-thread read sites remain safe. (Nuance, **not** an
  amendment: when an OpenGL context is attached, JUCE paints components on that context rather than
  the message thread, so the reader is "the thread that paints". ADR-0011 §Consequences records this
  deliberately without amending the rule — the stateless-peek requirement is what keeps it safe
  either way — and `docs/architecture/THREAD_MODEL.md` states per repository which context paints.)
- PDC/latency must be recomputed **off the audio thread** — never inside `processBlock` — via a
  `const`, race-free predictor feeding a single `setLatencySamples` call site, and never by mutating
  audio-thread state from the message thread. *(Amended by ADR-0011, 2026-07-31: this rule said "on
  the **message thread**", which is unsatisfiable — `prepareToPlay` and `setNonRealtime()`, two of
  the recompute call sites ADR-0004 item 5 mandates, are host callbacks that JUCE does not deliver
  on the message thread. The substance is unchanged: the predictor is `const` and race-free, so the
  rule never depended on which non-audio thread ran it.)*

## Atomic usage rules

- Published meter values: `memory_order_relaxed` (monotonic display data).
- Generation / staleness counters: `memory_order_relaxed` — they gate a message-thread cache
  rebuild and transfer no payload, so they are deliberately **not** ordering primitives.
- Scope/GR ring index: `release` on write, `acquire` on read — the ordering-critical pair.
- **Publication flags: `release` on write, `acquire` on read.** A flag that ANNOUNCES other atomics
  is not a staleness counter, and the relaxed rule above does not reach it — its whole job is to
  tell a reader that the values beside it may now be used, which is precisely a payload. The
  distinction is easy to lose because the two look identical at the call site, so the test is what
  the reader does next: if observing the flag gates a read of other state, it is a publication
  flag. In this build: `AnabasisEngine::frozenAppliedSeq` (gates `publishedTrim*`),
  `AdaptiveEngine::pubTrimEver` and `retTrimSeq` (each gates its own four trim scalars — the second is a counter, and is on this row rather than the relaxed one because it announces a payload; the wrapper's `slotFrozenBase`, which is only ever COMPARED against it, is correctly relaxed), and
  ADR-0012's staged-record flags (each gates its payload). Unobservable on x86-TSO; real on a
  weakly ordered target, and the cost is a compiler barrier on a path that runs once per block.

## Adaptive engine — where it runs

The Simple-mode adaptive engine (`MODE_AND_ADAPTATION_POLICY.md`) is **not** a licence for a
worker thread. This clause previously deferred the placement choice to "an ADR before
implementation"; that deferral is **discharged by ADR-0011** (Accepted 2026-07-31) and the
placement is now fixed, not open:

- **Feature extraction and adaptive trim slewing run on the audio thread**, inside the real-time
  budget (`DESIGN.md` §9's ≤ 0.5 % metering-and-features allocation) — not on a worker, and not on
  the message thread.
- **Macro mapping (the MacroEngine) runs on the message thread only**, by construction: the macro
  parameters are non-automatable (ADR-0005) and the engine consumes macro changes solely through
  an async message-thread listener.

Implementing either piece on a different thread contradicts an Accepted ADR — a Hard Stop, not a
design choice.

## Current model

Decided in **ADR-0011**; the concrete implemented model, with code citations, is
[`docs/architecture/THREAD_MODEL.md`](../architecture/THREAD_MODEL.md) (written at P1 as this
policy required). Read the ADR for the decision, that file for what the tree actually does, and
this policy for the permitted shapes. The MacroEngine guard atomics, formerly pending the OQ-014
owner call, are now the listener-guard row above (reading 1, taken 2026-08-02).

## Enforcement

A change to the thread model, a new shared-state path, or a new atomic ordering triggers the
**Architecture Review Gate** and an **AI Agent Hard Stop**. Changing this policy requires an ADR.
