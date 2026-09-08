# 2026-09-07 — The GR history's duration at every prepared pair (0.2.12 round 17)

Measurement trail for [ADR-0040](../docs/architecture/design-decisions/ADR-0040-gr-history-ring-capacity-is-a-duration.md).
Everything below was run in this container, Release, x86-64, against the real
`AnabasisEngine`, the real `GrHistoryBuffer` and the real `GrHistoryView` — never a model of them.

## 1. The invariant

An entry is one PREPARED block of processed audio (ADR-0011's 2026-09-07 amendment), and the reader
maps entries through the prepared pair the ring stores. So

    retained entries = min (available, windowEntries (rate, block))
    windowEntries    = min (kSize − 1, ceil (kWindowSeconds · rate / block))
    duration         = retained entries · block / rate

and the clamp binds — the window stops being `kWindowSeconds` and starts being the ring's capacity
— exactly when `rate / block > (kSize − 1) / kWindowSeconds`.

## 2. Before: the sweep, reporting SECONDS rather than entries

Each row drives the engine at the pair until the window is saturated, then reads `available()`,
`GrHistoryView::windowEntries` and `GrHistoryView::buckets` off the real objects. `kSize = 4096`,
clamp 4095, `kWindowSeconds = 20`.

| rate | block | entries/s | want | retained | **seconds** | entries a 20 s window needs |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 192000 | 32 | 6000 | 4095 | 4095 | **0.6825** | 120000 |
| 192000 | 64 | 3000 | 4095 | 4095 | **1.3650** | 60000 |
| 192000 | 128 | 1500 | 4095 | 4095 | **2.7300** | 30000 |
| 192000 | 256 | 750 | 4095 | 4095 | **5.4600** | 15000 |
| 192000 | 512 | 375 | 4095 | 4095 | **10.9200** | 7500 |
| 192000 | 1024 | 187.5 | 3750 | 3750 | 20.0000 | 3750 |
| 176400 | 32 | 5512.5 | 4095 | 4095 | **0.7429** | 110250 |
| 96000 | 32 | 3000 | 4095 | 4095 | **1.3650** | 60000 |
| 96000 | 64 | 1500 | 4095 | 4095 | **2.7300** | 30000 |
| 96000 | 128 | 750 | 4095 | 4095 | **5.4600** | 15000 |
| 96000 | 256 | 375 | 4095 | 4095 | **10.9200** | 7500 |
| 96000 | 512 | 187.5 | 3750 | 3750 | 20.0000 | 3750 |
| 88200 | 32 | 2756.25 | 4095 | 4095 | **1.4857** | 55125 |
| 48000 | 16 | 3000 | 4095 | 4095 | **1.3650** | 60000 |
| 48000 | 32 | 1500 | 4095 | 4095 | **2.7300** | 30000 |
| 48000 | 64 | 750 | 4095 | 4095 | **5.4600** | 15000 |
| 48000 | 128 | 375 | 4095 | 4095 | **10.9200** | 7500 |
| 48000 | 256 | 187.5 | 3750 | 3750 | 20.0000 | 3750 |
| 48000 | 512 | 93.75 | 1875 | 1875 | 20.0000 | 1875 |
| 44100 | 32 | 1378.125 | 4095 | 4095 | **2.9714** | 27563 |
| 44100 | 64 | 689.06 | 4095 | 4095 | **5.9429** | 13782 |
| 44100 | 512 | 86.13 | 1723 | 1723 | 20.0040 | 1723 |

Where the duration falls below each documented bound:

- **below `kWindowSeconds` = 20 s**: every pair above 204.75 entries a second — 48 kHz / 128 and
  smaller blocks, 96 kHz / 256 and smaller, 192 kHz / 512 and smaller;
- **below DESIGN §2.9's 10 s floor**: every pair above 409.5 entries a second — 48 kHz / 64 and
  smaller, 96 kHz / 128 and smaller, 192 kHz / 256 and smaller.

The duration depends only on the RATIO `rate / block`, which is why the table repeats: 48 kHz / 16,
96 kHz / 32 and 192 kHz / 64 are one row.

## 3. Why 4096

Nothing in the tree derives it. The only argument is the ring banner's *"At 512-sample blocks a
4096-entry ring holds ~43 s at 48 kHz — beyond the 10–30 s display window at every rate the product
supports"*, which computes headroom at one block size and generalises over rates. The power-of-two
requirement is real (`kMask`) but says nothing about which power. No test pinned a duration; five
places pinned the entry-count saturation and read it as a rounding detail — "one bucket of the
twenty seconds and 0.2 % of the pitch" (`GrHistoryView.h`, ADR-0023's 2026-09-05 amendment), "the
window holds one point fewer" (`CHANGELOG.md`) — figures that are exact AT the saturation threshold
and understate the loss by ~29× at 192 kHz / 32.

## 4. What it costs to fix, measured

| quantity | 4096 entries | 131072 entries |
| --- | ---: | ---: |
| slot storage | 32 KiB | 1024 KiB |
| `sizeof (GrHistoryBuffer)` | 32800 B | 40 B (slots on the heap) |
| `sizeof (AnabasisAudioProcessor)` | 75728 B | 42968 B |
| RSS, one processor prepared at 192 kHz / 32 and run | 2584 KiB | 3476 KiB |
| `push` | 2.1–2.2 ns/entry | 2.11 ns/entry |
| `prepare` that clears | a few µs | 85.7 µs mean, 216 µs worst |
| GUI decimation scan, 4088-entry window | 5.5 µs/frame | 5.5 µs/frame |
| GUI decimation scan, 120000-entry window | n/a | 163.7 µs/frame (2.05 % of one core at 125 Hz, 120 MB/s) |
| whole painted frame at the widest window | n/a | 317 µs (software rasteriser, 640×160) |

The scan cost is a function of the WINDOW, not of the capacity, so an ordinary session pays exactly
what it paid before; only a session that asks for a 120000-entry window pays for one.

## 5. After

The same sweep at `kSize = 131072` returns **20.00 s at all twenty-six pairs**, 192 kHz / 32
included (120000 entries retained, 120884 covered by the drawn buckets — 20.15 s, the bucket
rounding the read window has always carried). The first pair that still saturates the clamp is
192 kHz / 16, which retains 10.92 s — inside §2.9's band — and the floor is only left below
192 kHz / 8.

## 6. Alternatives, and why they lost

- **Producer-side decimation** (one entry per N prepared blocks) would have held 20 s in 4096 slots
  and produced an IDENTICAL picture, because the view already reduces `stride` entries to one
  vertex. It changes what an entry is, which is the content of ADR-0011's 2026-09-07 amendment, and
  a conflict with an Accepted ADR is a hard stop.
- **Sizing the ring at `prepare`** would have cost 32 KiB in ordinary sessions. A reader can be
  inside `peek` when the host re-prepares; the epoch bracket makes a torn READ safe and nothing
  makes a freed pointer safe, so this needs a reclamation protocol — a new cross-thread path, and a
  gate item.
- **Re-encoding the payload** into one 32-bit atomic would halve the storage and make the pair
  atomic rather than each field. It touches ADR-0026's payload rules, the precision of a linear
  peak and every reader; recorded as available, not taken here.
- **Documenting a shorter window** was the cheapest and is what the record would have had to say:
  5.46 s at 48 kHz / 64. That is a block size hosts run daily, and the previous round had just
  strengthened the manual's promise in the other direction.

## 7. Blast radius

`GrHistoryBuffer::kSize` had two literal readers in `src` (its own banner and `kMask`). The rest of
the tree reads it symbolically, which is what made the change tractable — but six state-suite
assertions were pinned to the SATURATION POINT rather than to the clamp, and they moved:

- `grWindow` — 48 kHz / 64 and 192 kHz / 32 no longer saturate; the clamp is asserted at
  192 kHz / 16 and 96 kHz / 8, and the two former cases are asserted to get the whole window now;
- `grBuckets` — the case table gains 192 kHz / 16 as the saturating case, and its heads are rounded
  DOWN to a whole number of buckets, which `want · 4` happened to be for every pre-round-17 pair;
- `grBuckets` m1 — `count <= cols` was never an invariant: `kFull` reaches `cols` exactly when the
  window divides it and the lead buckets then put the count above it. Replaced by the exact bound,
  `count == kFull + leadBuckets`;
- `grBuckets` m3 — the window may now be more than one bucket short of `want` at the clamp, and the
  reason is stated instead: one more bucket would not fit inside the ring's safe lap;
- `grRace` 1a/1b/1c — the premise pair moved to 192 kHz / 16, the staleness sweep runs THROUGH the
  margin the bucket cap leaves (which was zero at the old clamp — "one stale block is enough"), and
  the number of entries needed to lap the oldest drawn bucket is read from the geometry rather than
  hard-coded at three.

## 8. Regressions

Both new tests fail on the 4096-entry ring and pass at 131072:

- state suite, seven checks: `grWindow`'s "the pairs the ring used to clamp now get the whole
  window", and six `grSeconds` checks including the review's own 192 kHz / 32 case;
- dsp suite, two checks: `grHold`'s "every one of them is still inside the lap a reader may peek"
  and the marker's position.

The sweep's bounds are derived from `kSize`, so they hold at any capacity and pin the SHAPE of the
contract; the named cases quote the seconds and pin its VALUE.
