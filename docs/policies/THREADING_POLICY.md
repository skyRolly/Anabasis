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
| GUI → Audio (momentary / transient requests) | a single `std::atomic<int>` per request | e.g. meter reset, `Learn` start/stop, `Freeze` — consumed with `exchange` on the audio thread. |
| Audio → GUI (scope / GR history) | SPSC ring buffer | Exactly **one producer thread and one reader thread**; release/acquire on the write index; the index is published **once per block**, so a reader that acquires it sees a whole committed block. |
| Audio → GUI (meters: LUFS M/S/I, dBTP, PLR, GR) | published `std::atomic<float>` | Audio writes in a single `publish()`; GUI reads via getters. `memory_order_relaxed` — monotonic display data, no ordering role. |
| Audio/param → GUI (staleness hints) | `std::atomic<uint32>` generation counters | A monotonic "something changed" hint that lets the GUI skip rebuilding caches. Carries **no payload**, so relaxed is sufficient. |

Any path not in this table is a new cross-thread path → Architecture Review Gate.

## Forbidden cross-thread access

- No painting, allocation, locking, or IO on the audio thread.
- No direct access to non-atomic shared state across threads (the only synchronisers are the
  listed atomics + the SPSC ring).
- **No second producer** on a scope/GR ring, and no reads off the message thread. Reads must be
  stateless `const` peeks so multiple message-thread read sites remain safe.
- PDC/latency must be recomputed on the **message thread** via a `const`, race-free predictor —
  never by mutating audio-thread state from the message thread.

## Atomic usage rules

- Published meter values: `memory_order_relaxed` (monotonic display data).
- Generation / staleness counters: `memory_order_relaxed` — they gate a message-thread cache
  rebuild and transfer no payload, so they are deliberately **not** ordering primitives.
- Scope/GR ring index: `release` on write, `acquire` on read — the one ordering-critical pair.

## Adaptive engine — where it runs

The Simple-mode adaptive engine (`MODE_AND_ADAPTATION_POLICY.md`) is **not** a licence for a
worker thread. Feature extraction and macro mapping run on the audio thread within the real-time
budget, or on the message thread reading published values — whichever the P4 design chooses, it
must be one of the paths above, decided in an ADR before implementation.

## Current model

**TODO (no code yet).** The concrete thread model, with evidence citations, is written at P1 into
`docs/architecture/THREAD_MODEL.md`. Until then this policy states the permitted shapes only.

## Enforcement

A change to the thread model, a new shared-state path, or a new atomic ordering triggers the
**Architecture Review Gate** and an **AI Agent Hard Stop**. Changing this policy requires an ADR.
