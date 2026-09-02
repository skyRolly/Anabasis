# Worklog — round 6: two concurrency findings and KI-017 (2026-09-02)

Session-local evidence trail. Raw investigation material, NOT architecture documentation —
`docs/SOURCE_OF_TRUTH.md`: worklogs are never cited as policy. What is binding is `CHANGELOG.md`,
the code and the tests; this file carries the measurements and the alternatives that were rejected,
because a decision without its rejected alternatives is not reviewable.

The round was given two review findings to fix, one owner gate to record, and one filed issue to
audit. It landed on **fix / proven-bounded / fix**, and the middle answer is the one that needed the
most work to reach honestly.

---

## 1 — Finding 1: overtaken GR history frames. REAL, FIXED.

**The claim.** *"When audio wraps during painting, `available()` can remain stale while newer payload
stores are already visible. The accepted frame can therefore contain overwritten history."*

**Confirmed, and the mechanism is exact.** `push` stored its payload relaxed and then released the
INDEX:

```
const auto i = writeIndex.load (relaxed);
slot.grDb.store (grDb, relaxed);          // (no fence here — the defect)
slot.peak.store (peak, relaxed);
writeIndex.store (i + 1, release);
```

A release store orders what precedes it, never what follows. So push #P's payload stores could become
visible to the reader BEFORE push #(P−1)'s release store of the index. A reader whose relaxed `peek`
returned push #P's value therefore had **no edge** forcing its closing `available()` re-read to
observe P — the model constrains that acquire load only against the reader's own earlier loads. So
`first < readFloor (available())` could legally be false and the frame was **accepted and drawn with
entries the producer had already overwritten**.

**Classification, kept separate because the round's report distinguishes three registers:** defined
but WRONG DATA. Every conflicting access has been atomic since the first 2026-09-02 amendment, so
this was never undefined behaviour; and it was never "one frame late", because the frame was drawn
rather than dropped.

**This was a documented residual, not a new discovery** — and that is the uncomfortable part.
ADR-0011's second amendment says in as many words that `push` has no release fence "so the model
gives that lap check no formal guarantee", and calls the result "a defined-behaviour one-frame
artefact rather than a race". The residual was stated accurately and its consequence was understated:
an accepted frame carrying overwritten data is not the same as a dropped frame.

**The fix: one release fence in `push`, before the payload stores.** Plus the reader's post-check
rewritten from one `||` into two sequenced statements — `||`'s left-to-right sequencing is guaranteed
and the old form was correct, but the ordering is now load-bearing for the LAP as well as the epoch,
and a future edit that reordered the operands would delete the guarantee silently.

**The proof, in BOTH cases, because the naive version is wrong.** By [atomics.fences]: the fence (A)
is sequenced before the payload stores (X); the reader's peek (Y) reads X's value and is sequenced
before `batchIntact`'s acquire fence (B); so A synchronises with B, and everything sequenced before A
— including push #(P−1)'s `writeIndex.store (P, release)` — happens-before everything sequenced after
B, which includes the `available()` re-read.

- **Case 1, no clear intervened.** `writeIndex`'s modification order is increasing, so write-read
  coherence forces the re-read ≥ P ≥ first + kSize, hence `readFloor` > `first`: discarded.
- **Case 2, a clear intervened.** The naive coherence step does **not** apply — `clear` stores
  `writeIndex.store (0, release)`, so that modification order is not monotone in value and the
  re-read may legally be 0, which passes the floor test. The discard comes from the OTHER half of the
  conjunction: whichever store the peek read from is sequenced after a release fence (`push`'s or
  `clear`'s), so the same pairing puts `clear`'s odd `resetGuard` increment — or its closing even one
  — happens-before the relaxed epoch re-read inside `batchIntact`, which then differs from `epoch0`.

**Both halves of the post-check are load-bearing and neither alone suffices.** That is why it keeps
both, and why they are sequenced rather than merged. *Either the batch read clean data, or the
discard is guaranteed. There is no third case.*

**A release FENCE, not release payload stores.** [atomics.fences] would admit the latter too — a
release operation synchronises with an acquire fence when an operation on the same object, sequenced
before that fence, reads its value, which the peek is. The fence is chosen because it is ONE barrier
covering both fields instead of two release stores, and because it makes `push` and `clear` the same
shape. This was checked rather than assumed: an early draft of the argument had it backwards.

**Cost, measured independently of the patch's own comment** (`-O3`, both compilers):

| | x86-64 | AArch64 |
|---|---|---|
| before | `movss`, `movss`, `movq` | `str`, `str`, `stlr` |
| after | **identical** — the fence emits `#MEMBARRIER`, a directive, not an instruction | **+1 `dmb ish`** |

Once per HOST BLOCK: `push` runs from `processBlock`, never per sample.

**Rejected alternatives.** A per-slot sequence stamp (Disruptor shape): correct, but a second store
per entry and a second invariant, for a ring that already has an epoch. Acquire payload loads:
correct by the same clause, but pays on every one of the batch's ~900 peeks instead of once per push.
Publishing the index before the payload: swaps which half is unordered. A wider seqlock over the
batch: the epoch already is one. Nothing beat one fence.

## 2 — Finding 1's regression test: a source-level rule, because nothing else can see it

No deterministic suite can distinguish the two builds. The difference is a synchronises-with edge;
memory orderings are not introspectable in standard C++; and unlike the payload repair no TYPE moved,
so there is no `static_assert` to write. The repository has said this twice already in its own voice
("the races themselves are argued, never observed").

So the regression test went where it can actually fail: `scripts/check-realtime.py` gained a second
mode, **`REQUIRED_ORDER`** — the file previously answered only "is a forbidden operation present?",
and now also answers "is a required one present, and in the right place?". The rule fires when
`GrHistoryBuffer::push` lacks the release fence, when the fence has drifted BELOW the payload stores,
when it is an acquire fence instead, when it appears only in a comment, and when the function has
been renamed out from under the rule (a silent rule being the failure mode that whole file exists to
prevent). Six self-test cases in both directions; the checker's self-test goes 134 → 141.

Verified by hand as well as by self-test: with the fence deleted the tree fails; with the fence moved
after `grDb.store` the tree fails; with the shipped shape it passes.

## 3 — Finding 2: ScopeBuffer reset / stale peaks. PROVEN BOUNDED, NOT REPAIRED.

**The claim.** *"When reset overlaps `readLatest`, both generation loads can remain old around the
rewound index. The previous spectrum can therefore survive until a later tick notices."*

**The demanded invariant already holds, and here is why.** `reset()` stores the rewound index
`release` and THEN bumps the generation `release`. A reader whose acquire load of the generation
returns the new value therefore has happens-before to the rewind, and write-read coherence forces
every subsequently sequenced load of the index — including the one inside `readLatest` — to return a
post-reset value. **"New generation, stale index" is impossible.** Combined with `SpectrumView::tick`
advancing `shownInGen` only in a tick that floored the EMA before `analyse` (`resetIn`) or after it
(`gi1 != gi0`), a pre-reset spectrum can never be committed as a post-reset one. That is exactly the
invariant the review asked for.

**What IS real is the reverse skew, and the round states it rather than dismissing it.** A reader can
observe the rewound INDEX while its generation load still returns the old value. Then `resetIn` is
false, `readLatest` yields nothing, `analyse` returns early **without touching the EMA**, and the
previous spectrum is drawn again verbatim against the new rate's bin mapping. Nothing decays it: the
EMA's decay only runs when frames arrive, and a re-prepare normally happens with the transport
stopped. It ends when the generation bump becomes visible — and there is no second correction path,
because the frame COUNT was deliberately retired as a reset detector in 0.2.7.

Three corrections to the round's own first analysis, kept because they change the description:
1. A pre-reset read is not a clean pre-reset snapshot — the producer refills slots `0..n` post-reset
   and the reader's window wraps across the origin, so the frame is a MIXTURE. Defined, mislabelled.
2. The dominant case is not mixed peaks; it is the **entire previous spectrum, frozen**, because of
   `analyse`'s early return on a zero-length read.
3. The bound is not "one tick" — a tick can satisfy the idle test and do nothing at all, so it is
   "until the acquire load returns the bump".

**Why no code change.** The bound is one atomic's visibility latency, which the standard requires to
be finite; the artefact is a stale display, not wrong audio and not UB. The repairs that would remove
the skew entirely are larger than the defect: fold the generation and the index into ONE atomic word
(the strongest — it makes the skew unrepresentable rather than detectable, at the cost of the ring's
published word layout, `readLatest`'s signature and every caller), or bracket the rewind in an
odd/even seqlock like `GrHistoryBuffer::clear`'s. Both are design changes to a ring that is not
wrong. Filed as **KI-018**, with the packed word named as the recommended option if it is ever taken.

**What DID change: two comments that promised more than the code delivers.** `ScopeBuffer::reset()`
said "there is nothing here for a reader to observe half-done" — false, and it is what misled the
review: the rewind IS observable before its announcement. `SpectrumView::tick` claimed its post-batch
sample "catches a reset that landed DURING this tick's reads" — best-effort, not guaranteed. Both now
say what is guaranteed and what is not.

## 4 — KI-017: FIXED, and the entry was wrong in three places

`SpectrumView::paint` and `CurveView::readInputs` read `AudioProcessor::getSampleRate()` — a PLAIN
member that `setRateAndBufferSizeDetails` writes from whichever thread the host reconfigures on.
That is a data race with the host thread: undefined behaviour, not stale data.

Corrections the audit forced on the entry itself:
1. **Three plain reads, not two.** The third is `PluginEditor`'s `getTotalNumOutputChannels()` in its
   timer callback. There is no published channel count in the tree, so repairing it needs a new
   publication rather than a redirect. Deliberately not bundled; recorded as remaining.
2. **Not "macOS and Windows only".** `CurveView::readInputs` is reached from the editor's 24 Hz timer
   as well as from `paint`, and the writer is the HOST thread — which takes no MessageManager lock in
   any wrapper. The defect was live on every platform, Linux included.
3. **The MessageManager-lock note is confirmed and irrelevant here.** It excludes `paint` from racing
   MESSAGE-thread work; it says nothing about the host thread. Kept because it still narrows
   ADR-0027's and ADR-0038's stated premise — but it was never a reason to leave KI-017 open.

**The fix is a redirect, not a new mechanism.** Both views now read
`AnabasisAudioProcessor::preparedSampleRate()`, which forwards the pair `GrHistoryBuffer` already
publishes — ONE atomic, ONE publication discipline, no second home for the same fact, and no new
cross-thread path. Each call site reads it ONCE into a local: the old `x() > 0.0 ? x() : 48000.0`
spelling read the accessor twice and could have straddled a reconfiguration inside one ternary.

**What it trades, stated rather than glossed.** `prepareToPlay` prepares the engine before the GR
ring, so inside that window the published pair still carries the previous rate. For `SpectrumView`
that is provably invisible (the spectrum ring is rewound, `analyse` returns early, the old rate maps
an already-floored trace). For `CurveView` it means the EQ response can be drawn at the previous rate
for the duration of `prepareToPlay`. **A bounded correct-but-late frame traded for undefined
behaviour** — the right trade, and not a claim of identical behaviour.

**`GrHistoryBuffer::prepared()`'s contract comment was amended in the same diff**, because the fix
adds callers that read it OUTSIDE the epoch bracket the comment states as the rule. It now names two
disciplines: entries-through-the-pair needs the bracket; what-rate-are-we-at does not, and taking one
would mean nothing since there are no entries in that question.

**Pinned by `ki017`**, and the discriminator is the suite's own quirk: a bare `prepareToPlay` sets
nothing in JUCE's base class, so the old source mapped every headless frame through its 48 kHz
fallback — two `CurveView` snapshots taken at 96 kHz and 48 kHz were IDENTICAL, fingerprint and
cached path included. They differ only when the view reads the published pair. The revert mutant
fails exactly that assertion and no other.

## 5 — Gates

- **ADR-0011's THIRD amendment (KI-015's) was ACCEPTED by the owner and its gate is CLEARED**,
  recorded in the ADR, the index, HANDOVER, the coverage log and the KI-015 worklog.
- **ADR-0011 gains a FOURTH dated amendment for Finding 1, RAISED AND HELD.** It adds a new atomic
  ordering on the audio path and supersedes a paragraph inside an already-accepted block, which
  `AI_AGENT_POLICY.md` makes a Hard Stop on DETECTION whatever the agent thinks of the severity. The
  owner's ruling is owed; the action is the same either way — flag, hold, do not merge on green.
- KI-017's repair is **not** a gate item: no new thread, no new cross-thread path, no new ordering —
  two readers redirected onto an atomic already published and already read from the paint path, which
  ADR-0027 clause 4 as amended by ADR-0038 positively licenses for a single unpaired scalar.

## 6 — Remaining risk

- **KI-018** — the bounded stale-spectrum window above.
- **KI-017's third read** — the channel count, needing a publication rather than a redirect.
- **The `prepareToPlay` ordering window** — `engine.prepare` runs before `grHistoryRing.prepare`, so
  the published pair lags inside that call. Invisible for the spectrum, one late frame for the curve.
- **The races remain argued, not observed.** No test stages a concurrent producer; what is pinned is
  the arithmetic, the types and — new this round — the source-level ordering rule. No TSan lane was
  added, and the round did not find a reason to add one.
- **`ScopeBuffer::write` is `std::atomic<uint64_t>` with a lock-freedom `static_assert` only on
  `std::atomic<float>`.** On a hypothetical 32-bit target that store sits on the audio path without
  the guard its neighbour has. Noted, not repaired: no supported target is 32-bit.
