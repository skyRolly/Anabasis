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
| GUI → Audio (sentinel-valued command **carrying one scalar**) | one `std::atomic<float>` per slot, an out-of-range **sentinel** meaning "nothing pending" | The `abMatchGain` idiom (ADR-0011): the writer stores the value, the audio thread `exchange`s the sentinel back in, so arrival and payload are one indivisible operation and no second flag can tear against it. One writer, one consumer, **one scalar** per slot — a *bounded* set of slots fixed at compile time, never a queue. Anything unbounded, wider than one lock-free scalar, or needing ordering against other state is **not** this row and is a new cross-thread path. |

> **The frozen trim vector does not yet have a row, and must not be forced into the one above.**
> ADR-0007 routes it through "a sentinel-valued atomic consumed at the forced duck's silent bottom",
> but the vector is **four** scalars — release, stereo-link, sidechain-HPF and dynamic-tilt trims
> (`DESIGN.md` §5.4, ADR-0005 decision item 10) — and the row above covers one. Four independent
> instances of it would restore a slot correctly only if the four `exchange`s are guaranteed to be
> observed together; nothing in the accepted set establishes that, and a half-consumed vector is a
> permanently half-restored slot, not a transient artefact — which would defeat the per-slot
> bit-repeatability `MODE_AND_ADAPTATION_POLICY.md` invariant 3 requires of Freeze. Choosing between
> *N* parallel sentinel scalars with a stated ordering guarantee and a single release/acquire-gated
> per-slot POD is a **thread-model decision**: Architecture Review Gate, ADR, and an AI-agent Hard
> Stop (`OPEN_QUESTIONS.md` OQ-013). Until it is taken, no P1 code may wire that restore path.
| Audio → GUI (scope / GR history) | SPSC ring buffer | Exactly **one producer thread and one reader thread**; release/acquire on the write index; the index is published **once per block**, so a reader that acquires it sees a whole committed block. |
| Audio → GUI (meters: LUFS M/S/I, dBTP, PLR, GR) | published `std::atomic<float>` | Audio writes in a single `publish()`; GUI reads via getters. `memory_order_relaxed` — monotonic display data, no ordering role. |
| Audio/param → GUI (staleness hints) | `std::atomic<uint32>` generation counters | A monotonic "something changed" hint that lets the GUI skip rebuilding caches. Carries **no payload**, so relaxed is sufficient. |

Any path not in this table is a new cross-thread path → Architecture Review Gate.

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
  the **message thread**", which is unsatisfiable — two of the four recompute triggers ADR-0004
  item 5 mandates, `prepareToPlay` and `setNonRealtime()`, are host callbacks that JUCE does not
  deliver on the message thread. The substance is unchanged: the predictor is `const` and
  race-free, so the rule never depended on which non-audio thread ran it.)*

## Atomic usage rules

- Published meter values: `memory_order_relaxed` (monotonic display data).
- Generation / staleness counters: `memory_order_relaxed` — they gate a message-thread cache
  rebuild and transfer no payload, so they are deliberately **not** ordering primitives.
- Scope/GR ring index: `release` on write, `acquire` on read — the one ordering-critical pair.

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

**TODO (no code yet).** The thread inventory, ownership split and cross-thread edges are decided in
**ADR-0011**; the concrete thread model, with code citations, is written at P1 into
`docs/architecture/THREAD_MODEL.md` from that ADR. Until that file exists, read ADR-0011 for the
model and this policy for the permitted shapes.

## Enforcement

A change to the thread model, a new shared-state path, or a new atomic ordering triggers the
**Architecture Review Gate** and an **AI Agent Hard Stop**. Changing this policy requires an ADR.
