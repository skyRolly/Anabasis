# ADR-0040 — The GR history ring is sized as a DURATION at every prepared pair, not as an entry count at one of them

**Status:** **Accepted — 2026-09-07**, under the standing blanket approval for the post-v0.1.0
rounds. Nothing here is an `ARCHITECTURE_REVIEW_GATE.md` item: no parameter ID moves, no
serialization changes, no thread or ordering rule changes (the SPSC contract, the reset epoch and
the reader's window clamp are untouched), no DSP signal order moves and the reported latency is
unchanged. What changes is one capacity constant, where its storage lives, and a duration contract
that was never written down.

## Context

An entry in `GrHistoryBuffer` is **one prepared block of processed audio** — ADR-0011's 2026-09-07
amendment, the resolved half of OQ-017. The reader maps entries through the prepared `(rate, block)`
pair the ring stores, so the SECONDS a full ring holds are `kSize · block / rate`, and the window a
frame may read is one slot less than that (`GrHistoryView::windowEntries`, whose clamp exists so a
frame never reads the slot the producer is filling).

`kSize` was 4096, and the argument for it was a single sentence in the ring's banner: *"At
512-sample blocks a 4096-entry ring holds ~43 s at 48 kHz — beyond the 10–30 s display window at
every rate the product supports."* That computes headroom at ONE block size and then generalises
over every rate. The generalisation is false, and the clamp turned the shortfall into a shorter
window with nothing in the tree saying so: five places record the entry-count saturation and none
records what it costs in time.

MEASURED before this change, on the real engine, the real ring and the real view
(`worklogs/2026-09-07-gr-history-duration.md`):

| prepared pair | entries a second | window | retained |
| --- | ---: | ---: | ---: |
| 48 kHz / 512 | 93.8 | 1875 | 20.00 s |
| 48 kHz / 256 | 187.5 | 3750 | 20.00 s |
| 48 kHz / 128 | 375 | 4095 (clamped) | **10.92 s** |
| 48 kHz / 64 | 750 | 4095 (clamped) | **5.46 s** |
| 48 kHz / 32 | 1500 | 4095 (clamped) | **2.73 s** |
| 96 kHz / 64 | 1500 | 4095 (clamped) | **2.73 s** |
| 192 kHz / 32 | 6000 | 4095 (clamped) | **0.6825 s** |

Against that: `DESIGN.md` §2.9 and `DEVELOPMENT_BRIEF.md` both say **10–30 s**;
`GrHistoryView::kWindowSeconds` is 20.0, "⊕ default in the middle of the band"; and
`USER_MANUAL.md` promises *"Twenty seconds means twenty seconds of audio whatever size of buffer
your host hands the plugin"* — a sentence STRENGTHENED by the OQ-017 round. The band has been
enforced in the other direction before: `HANDOVER.md` rejects a proposed fix specifically because
it would have displayed 38.6 s, "outside `kWindowSeconds` and DESIGN §2.9's 10–30 s band". Below
the band is the same defect as above it.

192 kHz with a 32-sample block is not a hypothetical: ADR-0020 §1 costs the waveform panel against
it, ADR-0011's cost note is written for "~6000 blocks/s at 192 kHz with 32-sample buffers", and the
state suite used it as the premise of a concurrency proof. There is no documented minimum prepared
block or maximum sample rate anywhere — `COMPATIBILITY_MATRIX.md` has no rate or block row — so "out
of scope" was not available as a defence.

## Problem

The window's duration is `min (kWindowSeconds, (kSize − 1) · block / rate)`. Make the promised
window a promise at every pair a host can prepare, without changing what an entry MEANS.

## Options

- **A. Leave 4096 and weaken the documents to match.** Rejected. The shortfall reaches ordinary
  configurations — 5.46 s at 48 kHz / 64, a block size hosts run every day — and lands below §2.9's
  floor, not merely below the 20 s point. It would also retract a user-facing promise that the
  previous round deliberately strengthened.
- **B. Decimate at the producer** — one entry per N prepared blocks when the block is small, so the
  entry RATE is bounded and 4096 slots always span 20 s. Rejected: it changes what an entry is,
  which is precisely what ADR-0011's 2026-09-07 amendment decided, and a conflict with an Accepted
  ADR is a hard stop. It is also the cheaper answer only on paper — the displayed picture is
  identical either way, because the view already reduces `stride` entries to one vertex.
- **C. Size the ring at `prepare` from the pair.** Rejected. A reader can be inside `peek` when the
  host re-prepares; the epoch bracket makes a torn READ safe and nothing makes a freed pointer safe,
  so this needs a reclamation protocol the ring does not have — a new cross-thread path, and an
  Architecture Review Gate item, bought for a memory saving the measurements below do not justify.
- **D. Re-encode the payload** (two 16-bit fields inside one 32-bit atomic) to halve the storage,
  which would also make the pair atomic rather than merely each field. Rejected here as a separate
  decision: it touches ADR-0026's slot payload rules, the precision of a linear peak and every
  reader. It stays available if the footprint below ever matters.
- **E. A fixed capacity chosen from the worst prepared pair, held on the heap.** Chosen.

## Decision

1. `GrHistoryBuffer::kSize` is **1 << 17 = 131072 entries**. It is derived, not picked: a 20 s
   window at 192 kHz / 32 needs `ceil (20 · 192000 / 32) = 120000` entries, and the next power of
   two is this.
2. The ring's slots live in **one heap block taken at construction**, not in a member array.
   Nothing on the audio path allocates. This is load-bearing rather than tidy: at this capacity the
   array is a megabyte, and the suites build both rings and whole `AnabasisAudioProcessor`s as
   locals — two of the latter live at once in places, eight in one function — against a Windows
   main thread whose default stack is a megabyte in total.
3. The capacity is **fixed for the life of the ring** (option C's reasoning).
4. The duration contract is stated in code as `GrHistoryView::windowSeconds (rate, block)` —
   `windowEntries · block / rate` — and it is this:
   - the **whole `kWindowSeconds`** at every pair up to `(kSize − 1) / kWindowSeconds` = 6553
     entries a second: 192 kHz / 32, 96 kHz / 16, 48 kHz / 8 and everything with a larger block;
   - **never below §2.9's ten-second floor** up to `(kSize − 1) / 10` = 13107 entries a second,
     i.e. down to 192 kHz / 16 — half the smallest buffer a host offers at that rate;
   - and below that, `(kSize − 1) · block / rate`, shortening in exact proportion and nowhere
     abruptly.
5. The banner's sentence is replaced by the measurements above, and every other place that
   quantified the clamp's cost from the 4096-entry ring is corrected with it.

## Consequences

Measured on this container (Release, x86-64):

- **Footprint: +0.87 MiB per instance.** A prepared processor at 192 kHz / 32 held 2584 KiB of RSS
  before and 3476 KiB after. `sizeof (GrHistoryBuffer)` falls from 32800 B to 40 B and
  `sizeof (AnabasisAudioProcessor)` from 74.0 KiB to 42.0 KiB, because the slots left the object.
- **Producer cost: unchanged.** 2.11 ns per published entry against 2.1–2.2 ns before; the extra
  load of the storage pointer is hot in L1 and does not show. At the worst pair (6000 entries a
  second) the ring's write traffic is 48 KB/s.
- **`prepare` that clears the ring: 85.7 µs mean, 216 µs worst** — a megabyte of relaxed stores on
  the host thread with the audio stopped, where the previous ring took a few microseconds.
- **The GUI read cost scales with the WINDOW, not the capacity**, so only the configurations that
  need a wide window pay for one: the decimation scan is 5.5 µs a frame at 4088 entries, 40.5 µs at
  30000, 80.7 µs at 60000 and 163.7 µs at 120000 — 2.05 % of one core at the frame clock's 125 Hz
  cap, 120 MB/s of sequential reads. A whole painted frame at 192 kHz / 32 measured 317 µs against
  a 8 ms budget.
- **Bucket geometry moves where the clamp used to bind.** `kFull`'s ring-safety cap now binds only
  at blocks of about 7 samples or fewer at 48 kHz and 29 at 192 kHz. Six state-suite assertions
  that pinned the OLD saturation point were re-aimed at pairs that still saturate rather than
  special-cased, and one (`grBuckets`' count bound) was corrected: `kFull` reaches `cols` exactly
  when the window divides it, and the lead buckets then put the drawn count above the panel width.

## Related code

- `src/dsp/GrHistoryBuffer.h:129-148` — the capacity, its derivation and why it is fixed
- `src/dsp/GrHistoryBuffer.h:416-426` — the heap storage and why it is not a member array
- `src/gui/GrHistoryView.h:128-166` — `kWindowSeconds`, `windowEntries`' clamp, `windowSeconds`

Evidence [Verified]:
- Source: `src/dsp/GrHistoryBuffer.h`, `src/gui/GrHistoryView.h`
- Test:   `testTheHistoryWindowKeepsItsSecondsAcrossThePreparedPairs` (state suite),
  `testTheRingKeepsASecondOfEntriesAtTheSmallestPreparedBlock` (dsp suite),
  `testGrHistoryWindowNeverAsksForTheHeadSlot`,
  `testGrHistoryReaderStaysInsideTheRingAndSeesEveryReset`
- Worklog: `worklogs/2026-09-07-gr-history-duration.md`
