# Worklog — round 7: the stale spectrum, the channel count, the prepare lag (2026-09-02)

Session-local evidence trail. Raw investigation material, NOT architecture documentation —
`docs/SOURCE_OF_TRUTH.md`: worklogs are never cited as policy. What is binding is `CHANGELOG.md`,
the code and the tests; this file carries the measurements and the alternatives that were rejected.

Three questions, three different answers: **fix**, **fix**, **accept and document**. The round's most
useful output is not the patches — they are eleven lines between them — but the corrections to what
the previous round wrote down.

---

## 1 — The stale spectrum: FIXED, and KI-018's stated bound was wrong in both directions

**The distinction the round was asked to make.** Invariant 1 — *no pre-reset audio may be committed
as post-reset data* — was established last round and is untouched: `reset()` stores the rewound index
`release` before bumping the generation `release`, so a reader that acquires the new generation is
forced by write-read coherence to read a post-reset index. Invariant 2 — *no pre-reset visual result
may remain displayed after the reset becomes observable* — is this round's subject, and it did NOT
hold.

**Why not, exactly.** A reset is TWO stores and the reader already loads BOTH facets every tick —
`resetGeneration()` and `writeCount()` — and then keyed its decision on the generation alone. So
"the reset is observable" is really `min(visibility(resetGen), visibility(write))`, and the code
acted on only one term of that minimum. A reader that had observed the rewind but not the bump held
the previous spectrum, and — having committed `shownInCount = 0` — satisfied the idle test on every
tick after that and stopped looking. **The residual was a choice, not an inherent floor.** That is
what kills option B: KI-018's case rested on "the repairs are larger than the defect", which was true
of the two repairs it considered and false of the one it never considered.

**Both halves of KI-018's stated bound were wrong.**

| KI-018 said | Actually |
|---|---|
| "the bound is one atomic's visibility latency, sub-microsecond in practice" | The dominant case is an ordinary INTERLEAVING — the host thread is preemptible between the two stores — so the worst case is a scheduling quantum, tens of ms under load. (The typical case does remain store visibility; it is the worst case that was mis-stated.) And `[atomics.order]`'s "reasonable amount of time" is a *should*, so even that is "finite, not normatively guaranteed" |
| "drawn again — verbatim, against the new rate's bin mapping" | True only AFTER the prepared pair republishes, which is later still and only when the rate changed. Throughout the interleaving window the pair the view reads is the OLD one, so the held trace is drawn at the rate it was captured at — self-consistent, and bit-for-bit the frozen-analyser behaviour KI-007 item 6 already ships deliberately |

So the window is far longer than the entry claimed and the artefact far smaller. Both corrections are
now in the entry.

**The fix, message-thread only.** No new atomic, no ordering change, `ScopeBuffer` untouched, nothing
on the audio path.

1. `SpectrumView::resetObserved (gen, shownGen, count, shownCount)` — a pure static, in the
   `GrHistoryView` idiom, returning `gen != shownGen || count < shownCount`, used for both rings.
2. `analyse` floors the trace when `readLatest` returns zero frames.

**Why the count term is sound.** `write`'s modification order is 0 at construction, then a
non-decreasing run of `w + n` from a single producer, punctuated by 0 from `reset()` — the only
writer of a smaller value. `shownCount` is a value the SAME thread obtained from an earlier acquire
load of that same object, so the two loads are sequenced-before and read-read coherence
([intro.races]) forbids the later one returning a value earlier in the modification order. A strictly
lower count is therefore proof that a 0-store intervened. It survives arbitrary staleness, because
coherence is stated over the modification order rather than over real time. Premises, stated because
the proof rests on them: single producer; `reset()` is the only writer of 0; and `pushBlock`'s
addend is non-negative.

**No false positives, on any interleaving.** Both operands are unsigned and `shownCount` starts at 0,
so the first tick and a view attached to an already-running processor both compare against 0. The
idle test needs no change: `count < shownCount` implies `count != shownCount`, so it can never
swallow the new term.

**Nothing 0.2.7 removed is reintroduced.** That round retired the count as the SOLE detector because
a fast refill passes the old value and the reset is missed outright — an INSUFFICIENCY argument,
never a soundness one. As an OR-disjunct the generation remains the complete detector and the count
is only earlier-firing evidence. The added term is monotone: it can only flip `resetIn` false→true,
so every existing behaviour is preserved. The comments in `ScopeBuffer.h` and `SpectrumView.cpp`
that read as "the count is useless" now say which half of that is true.

**Why BOTH changes ship — neither subsumes the other.** The floor misses the partial-refill tick
(`0 < w << shownCount`): `got > 0`, so post-reset magnitudes are folded into a pre-reset EMA through
the attack assignment — a MIXTURE of two configurations, arguably worse than a freeze — and only the
count term catches it. The count term misses the tick where the reset lands between the reader's own
`writeCount()` load and `readLatest`'s: `ci` is pre-reset and large, the OR is silent, and the read
comes back empty. **An adversarial pass argued for dropping the floor** on the ground that the next
tick's small `ci` fires the OR anyway, so the floor buys at most one FrameClock period. That is
correct arithmetic and it is recorded here rather than buried: the floor was kept because invariant 2
is stated in frames, and because `got == 0` is *equivalent* to "the index I acquired was 0" —
reachable only from construction (where the trace is already floored, so the fill is a no-op) or from
a reset, so the branch cannot mis-fire.

**Rejected, each checked rather than waved away.**

| Option | Why not |
|---|---|
| Swap `reset()`'s two stores so the generation is bumped first | **A trap, named so nobody tries it.** It inverts the skew: a reader acquiring the new generation could then read a PRE-reset index, destroying invariant 1 — the one thing that already held |
| A release fence between the two stores | Separate objects; either state is still observable. Buys nothing |
| An odd/even seqlock over the rewind | The wrong instrument, not merely costly: `reset()` writes one atomic and touches no sample, so there is no clear-window payload store for a reader's loads to synchronise through — the reasoning already recorded in the header — and a seqlock detects TORN reads while this is a consistent-but-unannounced one |
| The packed (generation, index) word — **KI-018's own recommendation, now withdrawn** | It forces the frame counter below 64 bits. At 32 it wraps in ~25 hours at 48 kHz and would then FABRICATE a reset, and `writeCount()`'s monotone `uint64_t` total is a published contract with tests on it. A 128-bit atomic is not reliably lock-free and would be stored by `pushBlock` on the audio thread |

**What remains.** One corner: a reset whose refill reaches EXACTLY the previously observed count
while the bump is still invisible — the count term is silent (equal, not lower), the generation term
is silent, the idle test matches. It is the equal-count case in a new place, the same shape 0.2.7 and
round 2 each met. KI-018 is **retained and narrowed** to it, and no test pins it, which the entry and
the test's own comment both say.

## 2 — Tests, and one honest ADR-0025 filing

The truth table for `resetObserved` is deterministic and mutation-verified: the revert mutant (the
pre-round-7 rule, `gen != shownGen`) answers false for `(7, 7, 40, 100)` and fails **that row and no
other**. Two rows are anti-regression rather than discriminators: `(8, 7, 900, 100)` re-pins 0.2.7's
finding, and `(0, 0, 5e6, 0)` fails a `count != shownCount` mis-transcription. A correction to the
first draft's own rationale: row 2 `(7, 7, 900, 100)` does **not** discriminate `<` from `<=` — those
differ only at equality — rows 1 and 5 do.

**`analyse`'s floor ships under the ADR-0025 exception**, and this is the disclosure
`TESTING_POLICY.md` rule 1 requires rather than a claim of compliance:
- **The mechanism that makes a test impossible.** The branch is reached only when `readLatest`
  acquires an index of 0 while the reader's generation load still returns the old value. Producing
  that state needs a rewind without a visible bump, and `ScopeBuffer` offers no way to rewind without
  bumping — deliberately, since that coupling is invariant 1. A single-threaded suite therefore
  cannot construct the precondition.
- **What was verified instead.** The equivalence `got == 0` ⟺ "acquired index is 0" is read directly
  off `readLatest`'s clamp; the construction arm is a verified no-op because the trace is already
  floored; and the full suite is green with the branch live, which pins that it does not fire in any
  exercised path.
- **What is consequently unprotected.** A future edit that deletes the fill, or adds a guard that
  suppresses it, would not fail any test.
- **The condition under which the exception lapses.** If `ScopeBuffer` ever gains a way to rewind
  without publishing — or a test harness that can drive the two facets independently — the case
  becomes stageable and this exception no longer applies.

## 3 — The channel count: FIXED

KI-017's third read. `PluginEditor`'s timer read `getTotalNumOutputChannels()`, i.e. JUCE's plain
`cachedTotalOuts`, whose only writer is `AudioProcessor::audioIOChanged` — reached from
`applyBusLayouts`, `createBus`, the bus add/remove pair and `setPlayConfigDetails`, on the host's own
threads in every wrapper (the VST3 wrapper's own comment records hosts calling `setBusArrangements`
inside the `prepareToPlay` call stack; the Standalone path arrives on the device callback thread).
No wrapper takes a `MessageManagerLock` on any of them, and this plugin overrides none of the layout
callbacks that might have added an edge. Option B had no mechanism to name: a genuine data race.

**It matters that it can actually change.** `isBusesLayoutSupported` accepts stereo→stereo,
mono→stereo and mono→mono, so the output count is 1 or 2 — and a comment in `AnabasisEngine.cpp`
still said "isBusesLayoutSupported pins 2×2", which stopped being true at 0.1.2. **Reported and
corrected** as drift in this round rather than left standing.

**It could not be redirected** the way the two rate reads were: the prepared pair carries rate and
block only, and inferring mono from a per-channel meter reading zero is unsound because a stereo
channel at rest reads zero too. So this one needed a publication — and the justification is not
"the field is plain". The GR lanes read the engine's PER-CHANNEL atomics and must draw the geometry
those atomics were filled under; JUCE's accessor answers a different question, namely the layout the
host may be moving TO.

`pubOutChannels` is one relaxed `std::atomic<int>`, stored at construction (so an editor opened on a
never-prepared processor gets the negotiated geometry rather than a fabricated default — a state an
adversarial pass caught in the first draft), from `numChannelsChanged` (**which JUCE calls from
`audioIOChanged`, so the store lands on the writer's own thread** and a layout change with no
re-prepare is covered), and from `prepareToPlay`. It sits on `THREADING_POLICY`'s existing
Meters → GUI row, whose writer set already includes `prepareToPlay` on the host thread: not a new
cross-thread path, not a gate item. `prepareToPlay` now takes JUCE's plain read ONCE into a local and
reuses it, the rule the sibling accessor already states.

Pinned by `ki017c`: seeded at construction, republished by a layout change with no prepare,
republished by a prepare, and lock-free. The assertions do not compile against the old code, which
has no such accessor — the same blunt kill `grSync` and `specSync` carry.

## 4 — The prepareToPlay publication lag: ACCEPTED as a bounded transitional state

`AnabasisEngine::prepare` rewinds both spectrum rings at the TOP of its body; the prepared pair is
republished only after it returns. So the window is nearly all of `engine.prepare` — the eight
oversampler constructions, milliseconds — **not** the gap between two adjacent statements, which is
how the previous round described it. Two further corrections to that description: the rewind happens
on EVERY prepare, because `engine.prepare` is called unconditionally, while only the pair's
republication is gated on the pair actually changing; and "provably invisible for the spectrum"
over-claimed slightly.

**The order is load-bearing and must not be tidied.** Publishing the pair first would put a full
16384-frame ring of old-rate audio opposite the NEW rate for that whole window — the wrong-frequency
artefact the rewind exists to remove, produced deterministically on every vblank rather than as a
skew. The current order puts an EMPTY ring opposite the OLD rate, and every state a reader can reach
in the window is self-consistent. Duration is not the discriminator; content is.

Per reader, and the first of these was mis-stated in the round's own first draft:
- **`GrHistoryView`** — no exposure. It *does* read the pair (in `tick` and in the paint path), and
  the earlier claim that it "touches nothing `engine.prepare` writes" is false. The correct reason is
  better: it reads the pair from INSIDE the same epoch bracket as the entries, so the two move
  together by construction.
- **`SpectrumView`** — reads an empty ring, and since this round floors its trace on that read.
- **`CurveView`** — can draw the EQ response at the previous rate for the duration of
  `engine.prepare`. A bounded correct-but-late frame.

**The invariant, stated for the next reader:** *a GR frame never maps one configuration's entries
through another's time base, and the price is that non-ring readers may lag by one reconfiguration.*
No code change; documentation only; no gate item.

## 5 — Gates

**ADR-0011's FOURTH amendment (round 6's `push` release fence) was ACCEPTED by the owner and its
gate is CLEARED**, recorded in the ADR, the index, HANDOVER, the coverage log and this round's report.

Nothing in round 7 is a gate item, and the reason is worth stating rather than asserting: Q1 is a
message-thread predicate over two values the reader already loads plus a `std::fill` in an existing
early-return arm — no new thread, no new cross-thread path, no new atomic ordering, which is all
three limbs of the Thread Model row. Q2 adds an atomic but on an EXISTING row whose writer set
already includes the host thread in `prepareToPlay`. Q3 changes no code.

## 6 — Remaining risk

- **KI-018's equal-count corner** (above), unpinned by any test and unstageable.
- **The `prepareToPlay` window** — accepted, documented, and the ordering constraint recorded so a
  future round does not "fix" it backwards.
- **The races remain argued, not observed.** What is pinned is the arithmetic, the types, the source
  ordering rule from round 6, and now the decision rule. No TSan lane; this round found no reason to
  add one.
- **`ScopeBuffer::write` is `std::atomic<uint64_t>`** with a lock-freedom `static_assert` only on
  `std::atomic<float>`; no supported target is 32-bit.
