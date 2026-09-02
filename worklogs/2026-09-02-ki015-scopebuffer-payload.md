# Worklog — KI-015: the spectrum rings' payload (2026-09-02)

Session-local evidence trail for the KI-015 follow-up to the 0.2.8 review rounds. Raw investigation
material, NOT architecture documentation — `docs/SOURCE_OF_TRUTH.md`: worklogs are never cited as
policy. What is binding is `CHANGELOG.md`, the code and the tests; this file carries the
measurements and the alternatives that were rejected, because a decision without its rejected
alternatives is not reviewable.

The brief was a focused adversarial review with an explicit instruction not to treat the existence
of the KI-015 note as proof that the code was still wrong, and to land on exactly one of: (1) a real
bug, fix now; (2) race-free under a defensible memory-model argument, close it; (3) real, but the
correct fix needs a larger architectural change, so defer.

**Answer: (1).** What follows is why, and what the review corrected on the way.

---

## 1 — The race, established from the code

`ScopeBuffer` is the §2.9 spectrum capture ring: two instances on the engine, one per tap.

- **Producer**, audio thread: `AnabasisEngine::processChunk` calls `pushBlock` once per ring per
  CHUNK. `pushBlock` wrote the payload with `std::memcpy` into `left.data()`/`right.data()` (at most
  two contiguous segments) and published with one release store on the write index.
- **Reader**, message thread: `SpectrumView::tick` → `analyse` → `readLatest(scratchL, scratchR,
  4096)`, which acquire-loads the index and copies with a **plain per-element loop**.
- **Storage**: `std::vector<float>`.

The release/acquire pair on the index is sound and does exactly one job: every frame strictly below
the acquired index is complete, which is why `readLatest` clamps to `[w − count, w)`. It is a
**backward edge only**. Nothing in `pushBlock` reads anything the reader writes, so no
happens-before relation exists in either direction between the reader's copy loop and any push
issued after the publication it acquired. Acquire/release bounds staleness downward; it cannot bound
the future upward.

So when the producer laps the reader mid-copy, the two touch the same `float` objects, one side
modifying, neither ordered. That is a data race under [intro.races] — undefined the moment it
happens, which is the holding ADR-0011's first 2026-09-02 amendment already made for the sibling
ring, and the reason detection is the wrong tool.

**The `resetGen` bracket is orthogonal, not a mitigation.** `SpectrumView::tick` samples
`resetGeneration()` on both sides of its batch, but `pushBlock` never touches `resetGen`, and
`readLatest` has no `readFloor`-style window re-check. The bracket answers "did a re-prepare
happen", never "was I lapped" — so this ring had strictly LESS lap detection than `GrHistoryBuffer`
had before its repair, and detection would not have cured it anyway.

## 2 — Why option 2 fails: the headroom defence is not an invariant

This is the part that outlives the fix, because KI-015's entire severity argument rested on it.
The entry said the reader takes 4096 of 16384 frames, so the producer must advance ~12288 frames —
"~0.26 s at 48 kHz" — mid-copy. Three independent failures:

1. **It is a frame count, not a time.** 12288 frames is 0.256 s at 48 kHz and **0.064 s at
   192 kHz**, a rate this tree exercises.
2. **`reset()` rewinds the head.** A reader holding a pre-reset index has a margin anywhere in
   [0, capacity) — the header derived its bound from monotonic advance and never said so, while
   `SpectrumView` documents that a reader straddling a reset is expected.
3. **`n` is bounded only by the host.** `maxBlock` is `samplesPerBlock` with no upper clamp
   anywhere between `prepareToPlay` and the engine. A push reaches the reader's **oldest** slot at
   n ≥ capacity − count + 1 = **12289**, and covers its **whole window** at n ≥ capacity = **16384**.
   Two different thresholds; the review's first draft conflated them and an adversarial pass caught
   it. Neither needs a reader stall at all.

Stated the other way for honesty, because the replacement must not repeat the pattern it corrects:
that third path is **possible by construction and unexercised here** — no in-tree stimulus prepares
more than 512 frames, and what would settle it is a survey of host offline-bounce maxima, which this
tree cannot answer. It is enough to sink the defence: a probability is not a proof, and the margin's
own arithmetic was never written down.

Also worth recording, because it is the one genuine improvement on the sibling: in the NON-lapped
case there is no conflicting pair at all. The reader takes `[w − 4096, w)` and the producer writes
from `w`; those are disjoint objects. `GrHistoryBuffer::peek` aliased the slot being filled by
construction. That is why this was a follow-up and that one was the blocker.

## 3 — Why option 3 fails: there is no architectural change to defer to

Ownership is clean: both rings are value members of the engine, itself a value member of the
processor; heap storage is allocated once at construction and never resized; `SpectrumView` caches
no reference across ticks; `readLatest` is message-thread-only, so ADR-0038's Message → Painting
boundary is not engaged by the ring at all. No signature, latency, serialization or parameter
contract moves.

KI-015's stated reason for deferring — "its bulk `memcpy` needs its own design pass rather than a
transliteration of the GR fix" — does not survive reading the reader half, which was *already* a
per-element scalar loop. Only the producer's four `memcpy`s had to change.

## 4 — The fix

Confined to `src/dsp/ScopeBuffer.h`. `SpectrumView` and `AnabasisEngine` are untouched; no signature
changes, so no caller moves.

- The payload element is a **`Sample`** class wrapping one `std::atomic<float>` and exposing only
  relaxed `store`/`load`.
- `pushBlock`'s two-segment `memcpy` becomes two store loops over the **same segment arithmetic**;
  the relaxed self-load of the index, the `n > capacity` clamp, `idx`, `first` and the single
  release-store publication are unchanged.
- `readLatest`'s two subscripts gain `.load()`. The acquire load, the short-read clamp, `available`,
  `start`, the masked index and the return value are unchanged.
- Two `static_assert`s: lock-freedom (a locking atomic here would be a lock on the audio path), and
  `sizeof`/`alignof` equal to `float`'s (the heap footprint the ADR-0009 Windows-stack delta depends
  on).

**Why a wrapper and not a bare `std::atomic<float>`.** `slot = value` on a bare atomic selects the
SEQ_CST `operator=` — a fenced store per sample on the audio thread that neither
`-Wfunction-effects` nor RealtimeSanitizer classifies as a lock, and that no instrument in this tree
would report. The wrapper makes that spelling **ill-formed** (verified: `error: no viable
overloaded '='`). This came from the adversarial pass; the first design did not have it.

**No reader-side acquire fence, and this is where the ring differs from its sibling.**
`GrHistoryBuffer` needed `batchIntact` because its `clear` WRITES THE PAYLOAD inside the epoch
window, so a reader's payload loads are the only thing that can witness a clear and an acquire load
would leave them unordered against the epoch re-read. `ScopeBuffer::reset()` writes one atomic and
touches no sample: there is no clear-window store for a payload load to synchronise through, and
`[atomics.fences]` gives an acquire fence nothing to attach to, since the producer's payload stores
are relaxed and no release fence precedes them. That is the same holding ADR-0011's SECOND
2026-09-02 amendment already states for the sibling's `push` — ratified precedent, not a new claim.
Three of the six review lenses recommended adding the fence; the synthesis overruled them, and the
adversarial memory-model pass confirmed the overrule while correcting its reasoning (the first draft
argued from release SEQUENCES, which is the wrong instrument: a release sequence is headed by an
operation on the same object, and the payload objects are different objects).

**Named premise.** `reset()` runs from `prepare` with audio stopped. That is a plugin-API contract,
not a C++ guarantee, and it is load-bearing twice: without it the generation bump orders nothing,
and the rewind itself is not guaranteed to be observed by the next push. The same premise already
underwrites `prepare` reallocating every other ring the engine holds, so the code is unsound far
beyond this ring if it fails — but it is stated here rather than left in one comment.

## 5 — Measurements

**Codegen, `pushBlock` at `-O3`, x86-64** (isolated TU, both toolchains):

| | call sites | lock-prefixed instructions |
|---|---|---|
| before | 4 (the four `memcpy`s) | 0 |
| after | **0** | **0** |

Neither compiler vectorises the store loop — LLVM and GCC will not vectorise atomic accesses — so
the four vectorised `memcpy`s become inline scalar stores. **The sibling's "instruction-identical"
evidence claim is therefore NOT transferable, and the record says so rather than borrowing it.**

**Cost, on the real workload** (two rings × two channels per chunk, `memcpy` vs relaxed store loop,
clang-22 `-O3`):

| block | rate | memcpy | atomic | extra | as % of the block period |
|---|---|---|---|---|---|
| 512 | 48 kHz | 180 ns | 737 ns | +557 ns | **+0.005 %** |
| 512 | 192 kHz | 172 ns | 741 ns | +569 ns | **+0.021 %** |
| 2048 | 48 kHz | 676 ns | 2878 ns | +2202 ns | +0.005 % |
| 16384 | 48 kHz | 5946 ns | 24219 ns | +18274 ns | +0.005 % |

Against `PERFORMANCE_BUDGET.md`'s recorded 625.4 ns/sample working cell that is a relative delta
under 0.2 %. **Deliberately not presented as an `AnabasisBench` delta:** at 0.005 % of realtime the
change is far below that harness's run-to-run resolution on this machine, so a bench table would
report scheduler noise and call it evidence. The honest instruments here are the codegen diff and
the isolated timing above.

**Gates that were run rather than reasoned about.** The `realtime` job's two-compile step, which
covers `pushBlock` directly (it is called from the annotated `leafAudioPath` in
`tests/realtime_effects.cpp`): the clean compile succeeds with no diagnostics under
`-Werror=unknown-warning-option -Werror=function-effects`, and the canary compile still fails with
`-Wfunction-effects` naming `canaryAllocatingHelper` — so the gate is live and the relaxed stores are
effect-clean.

## 6 — Regression coverage

A data race cannot be staged deterministically, so — exactly as `grSync` did for the sibling — the
TESTING_POLICY rule-1 discriminator is a TYPE-level assertion, not an attempt to observe the race.
`specSync`, in the DSP suite beside the existing spectrum-ring section:

| Assertion | What it pins |
|---|---|
| `Sample` is neither copy-constructible nor copy-assignable | the payload is not a plain float |
| `Sample&` is not assignable from `float` | the SEQ_CST `operator=` spelling cannot return |
| `std::atomic<float>::is_always_lock_free` | the audio-thread store never takes a lock (portability guard — passes on old and new alike, and is labelled as such) |
| `sizeof`/`alignof` equal `float`'s | the heap footprint is unchanged |
| a 300-frame push round-trips bit-exactly | the repair moved no audio |
| a push LONGER than the ring keeps the newest `capacity` frames and still advances the index by the whole block | the `n > capacity` branch |

That last one closes a hole that **predates** the fix: nothing in the tree had ever executed that
branch, and this round turns it into a scalar loop.

| Mutant | Kills |
|---|---|
| **plainpayload** — `Sample` is a plain-float struct with plain accessors (today's code, renamed) | exactly 1: the copy-constructible/assignable assertion |
| **overclamp** — the `n > capacity` branch keeps the OLDEST frames | exactly 1: the long-push assertion |

Two mutants have no automated killer and the record says so rather than pretending otherwise:

- **A re-introduced `memcpy` over the payload.** GCC diagnoses it (`-Wclass-memaccess`, criterion
  "no trivial copy-assignment") and the zero-first-party-warning gate makes that a red job — but
  **clang-22 is silent even with `-Wnontrivial-memaccess`, measured**, so the GCC lanes are the whole
  of the automated catch. `<cstring>` is no longer included, so the spelling also needs a new include:
  a visible diff rather than a silent one.
- A **measurement worth recording on its own**: `std::is_trivially_copyable` reports **true** for
  `Sample` on both libstdc++ and libc++, despite every copy and move operation being deleted. It
  therefore cannot separate the wrapper from a plain float, which is why the test asserts
  copy-constructibility and copy-assignability instead. GCC's warning uses a different criterion,
  which is why it still fires.

**No ThreadSanitizer lane was added**, per the brief, and the review did not demonstrate one is
necessary: this repository's established instrument for this class is a one-off out-of-band run
recorded as evidence, and a lane would additionally owe a liveness canary under TESTING_POLICY
rule 5.

## 7 — Alternatives rejected

| Option | Why not |
|---|---|
| **Reader-side clamp** (`readFloor` analogue) | makes the race rarer or detectable, not defined — and the reader already reads strictly below the acquired index |
| **Seqlock over the read batch** | a seqlock still needs an atomic payload for its announcement to be about defined behaviour; it is the chosen fix plus two audio-thread atomics per chunk |
| **Larger capacity / smaller window** | pure probability, and it addresses neither the reset-rewind nor the unbounded-`maxBlock` path |
| **Double/triple buffer with pointer swap** | changes display behaviour — successive analyses currently overlap heavily; a swapped 4096-frame snapshot would refresh only every ~85 ms at 48 kHz — and opens a new cross-thread path |
| **Producer honouring a reader claim** | a reader-dependent branch on the audio path, and a suspended reader freezes the ring, so the idle test reports "nothing new" permanently |
| **`std::atomic_ref` over the existing `std::vector<float>`** | leaves `data()` a `float*`, so a re-introduced plain subscript compiles silently; and `__cpp_lib_atomic_ref` availability across all four toolchains is unverified |
| **An 8-byte interleaved frame payload** (one atomic per frame instead of two) | halves the store count, but changes the ring's layout — and the measured cost does not justify a layout change. Not held open as a conditional fallback: the recommendation is unconditional |
| **Fusing the tap with the ring write** — stage A and stage E storing straight into the ring, deleting the four scratch vectors | **cheaper than the code that ships today** (~4 stores/sample against today's ~4.5 and this fix's ~8), equally well-defined, and genuinely the "design pass" KI-015 was holding open. Rejected **for this change only**, on scope: it edits the engine's stage loops rather than one leaf header, deletes 4 × `maxBlock` floats of state, and belongs in its own round with its own measurement. Recorded here so the next round starts from it rather than rediscovering it |
| **Gating the tap on "a reader is attached"** | would make headless, offline and pluginval runs cheaper than today and shrink the race window, but it is not a fix for the UB and it has a liveness cost (`writeCount()` stops advancing under the reader's idle test) |

## 8 — Findings filed rather than fixed

- **KI-016** — Anamorph's `ScopeBuffer` carries the unrepaired shape and its Known Issues has no
  entry for it. ADR-0009 item 8 makes divergence accepted and one-way and `CLAUDE.md` §3 makes that
  repository read-only from here, so nothing is owed — but "accepted drift" is only meaningful
  against a named instance, and this is the name.
- **KI-017** — `SpectrumView::paint` and `CurveView` read JUCE's plain `currentSampleRate` from the
  painting thread: the identical defect class ADR-0011's second 2026-09-02 amendment repaired for
  `GrHistoryView`. Out of this round's subject, deliberately not bundled.
- **Inside KI-017, a records finding.** ADR-0027 and ADR-0038 both rest on "a plain read from
  `paint` is a data race on the two platforms where the context attaches". In the pinned JUCE 9.0.1
  the GL render thread takes the **MessageManager lock** around `paintComponent`, so a component's
  `paint` cannot run concurrently with message-thread work — a happens-before edge neither ADR
  considers, and which appears nowhere in this tree. Neither ADR's decision is disturbed: the
  atomics are correct, cost nothing, and are the right shape for state whose writer is not the
  message thread; and it is one JUCE version's implementation detail rather than an API guarantee,
  which is a reason to keep them. **No ADR text was changed on the strength of it** — that is the
  owner's call, and the finding is recorded rather than acted on.

## 9 — The gate

ADR-0011 gains a **third** dated 2026-09-02 amendment, and it **supersedes an accepted sentence** in
the first one ("`ScopeBuffer` … is **not** changed here … recorded as drift rather than repaired").
`AI_AGENT_POLICY.md` makes an existing Accepted ADR conflict **detected** a Hard Stop that a green
build does not clear, so the amendment is **raised at the Architecture Review Gate and held for the
owner's ruling**. Whether this is a repair under the existing SPSC-ring contract or a decision in its
own right is not the agent's call to make; an earlier draft of the decision ruled it a repair by
inference from another amendment's missing clearance line, and the adversarial architecture pass was
right that the inference is unsound and that the record, not the action, is what it would damage.
The action is the same either way: flag, hold, do not merge on a green build.

## 10 — Remaining risk

- **A batch may still mix pre- and post-reset frames without the generation bracket noticing.** The
  post-batch `resetGeneration()` load has no happens-before with the host thread's increment, so it
  is best-effort. This is **pre-existing** and out of scope to repair here, but both
  `SpectrumView`'s comment and `THREAD_MODEL.md` assert that the post-batch sample "catches a reset
  that landed DURING this tick's reads", which is stronger than the code guarantees. Left standing
  and named here rather than silently corrected in passing.
- **The `readFloor`-class residual does not apply**, because this ring has no such re-check; what
  remains is the display artefact above, bounded by the analyser's ~120 ms EMA.
- **The race remains argued, not observed.** No test stages a concurrent producer; what is pinned is
  the type that makes the read defined. That is the same standing limitation the 0.2.8 rounds
  recorded, and no TSan lane was added.
