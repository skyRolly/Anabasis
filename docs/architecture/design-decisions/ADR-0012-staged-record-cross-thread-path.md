# ADR-0012 — GUI→Audio staged record behind a release/acquire flag (ratifies the learned-target restore)

**Status:** Accepted (2026-08-01 — owner decision on `OPEN_QUESTIONS.md` OQ-015, option 1)

> Numbered **0012**, not 0015: `ADR_POLICY.md` rule 6 makes the sequence sequential from the
> highest existing ADR, and ADR-0011 was the highest. It resolves **OQ-015**; the question number
> and the ADR number are deliberately independent sequences.

## Context

`THREADING_POLICY.md` enumerates the *only* permitted cross-thread paths and closes the table
with "any path not in this table is a new cross-thread path → Architecture Review Gate";
`CLAUDE.md` lists a threading-model change among the hard stops. ADR-0011 fixed the thread
inventory and the publication mechanisms, and explicitly deferred the concrete edge list to
`docs/architecture/THREAD_MODEL.md`.

The P4 §5.4 Learn work needed one edge the table does not carry. Restoring learned reference
targets from a session (`ADAPTIVE` child, ADR-0007 schema) means handing the audio thread **two
floats plus a learned/never-learned discriminator**, adopted as a unit at a block top. It was
implemented as a staged record — payload stored relaxed, a single `adaptivePending` flag
release-stored after, consumed `exchange(acquire)` — by analogy to `GrHistoryBuffer`'s
release/acquire discipline. External review (2026-08-01) established that the analogy does not
authorise it: `GrHistoryBuffer` instantiates an **Audio→GUI** row, and the closest GUI→Audio row
(the sentinel command) excludes verbatim "anything unbounded, wider than one lock-free scalar, or
needing ordering against other state". The path shipped inside PR #5 and was recorded as OQ-015
rather than being redesigned under review pressure; `THREAD_MODEL.md` marked both rows
"no row — see OQ-015". This ADR takes the decision that was owed.

## Problem

1. Is a bounded multi-scalar GUI→Audio record an admissible mechanism at all, or must every
   GUI→Audio hand-off decompose into independent single scalars?
2. If admissible, what exactly is the contract — who may write, who may read, what ordering is
   guaranteed, and what is *not* guaranteed?
3. What does the answer imply for OQ-013's frozen-trim vector, which is the same shape at a
   higher stake (four coherence-critical scalars, half-restored = permanently wrong)?

## Options

- **A. Ratify the staged record as a new permitted row.** One bounded record, one writer thread,
  one consumer, payload-then-release-flag / acquire-flag-then-payload. Costs a row in the policy
  table; makes the ordering contract explicit for every future user of the shape. **Chosen.**
- **B. Re-express on the existing rows** — `restoreNeverLearned` as the payload-free momentary
  request (which it already fits), the two refs as sentinel-valued `atomic<float>` slots. Stays
  inside the current table, but the pair can tear across two session loads (one session's onset
  with another's tilt) — a permanent wrong state, which is precisely what the sentinel row's
  exclusion exists to prevent. Rejected: it trades a documented mechanism for an undocumented
  tearing window.
- **C. Defer the restore to P5** and ship Learn as session-local. No policy question and no
  mechanism; a user-visible regression (a learned session would not survive a save/load), and
  the question returns unchanged the moment the restore is written. Rejected.

## Decision

**A bounded staged record is a permitted GUI→Audio path**, added to `THREADING_POLICY.md`'s
permitted-path table under these conditions, all of which the learned-target restore already
satisfies:

1. **Bounded and fixed at compile time.** A record of `N` lock-free scalars, `N` fixed in the
   type, no allocation, no container, no variable-length payload.
2. **One writer, one consumer.** The writer is a non-audio thread (message thread in practice —
   ADR-0011's "off the audio thread" rule, not "on the message thread"); the consumer is the
   audio thread at a **block top**, never mid-block.
3. **Ordering:** payload fields stored `relaxed` **first**, then a single flag `release`-stored.
   The consumer `exchange`es the flag with `acquire` and only then reads the payload. This is the
   same primitive pair as the SPSC ring's index publication, in the opposite direction, and it is
   what makes the record adopt as a unit.
4. **Last-writer-wins is the only semantics offered.** A record staged and not yet consumed is
   overwritten by the next one. Anything needing *queueing* is a different mechanism and a new
   gate item — a staged record is a mailbox, not a queue.
5. **The writer may read back its own staged record** (same thread that wrote it) to answer
   "has the consumer taken it yet?" — the flag is readable via a `const` acquire load. This is
   what lets `getStateInformation` serialize correctly before the next audio block.
6. **No side effects in the consumer beyond adopting the record.** Adoption is a plain assignment
   into engine state; no allocation, no locking, no notification.

The learned-target restore (`AnabasisEngine::restoreLearnedTargets` / `restoreNeverLearned` /
`adaptiveRestorePending`, consumed at the `process()` block top) is **ratified unchanged** as the
first instance. `AdaptiveEngine::learned` — refs published before a release-stored flag, read
with an acquire load by `getStateInformation` — is ratified as the Audio→GUI mirror of the same
contract (published state read as a unit, staleness bounded by one block).

**OQ-013 is NOT resolved by this ADR.** The frozen-trim vector may now use this mechanism as its
transport, but *whether* a restored trim vector should be injected into a running engine at all —
and what it means for the adaptation state machine — is a separate, still-open product question.
This ADR removes the mechanism objection from OQ-013; it does not answer OQ-013.

## Known limits of the contract

Both are one-block/one-save effects on references that slew over seconds. They are stated because
a contract whose edges are unwritten gets read as having none — and because OQ-015 asked for the
coherence properties to be on the record rather than assumed.

1. **Consume-then-adopt window.** The consumer clears the flag with `exchange` and adopts a few
   instructions later. A writer-side read landing between the two (condition 5's "taken yet?")
   sees `false` while the engine still holds pre-adoption values, so a save in that window
   serializes the older learned state — one save's worth. Closing it means clearing the flag
   *after* adoption, which trades this for a **lost update**: a record staged between adopt and
   clear would be erased. The stale-read window is the better trade and is the one taken.
2. **The same window on a COMPOSING writer** (the §5.4 Learn commands, added 2026-08-01). Where
   the writer composes by reading the flag back (condition 5), limit 1 has a second consequence: a
   `requestLearnStart()` landing between the consumer's `exchange` and its payload read observes
   `pending == false`, composes a bare start instead of commit-then-start, and the outstanding
   commit is dropped. Same few-instruction window, same trade — closing it means reading the code
   before clearing the flag, which converts a stale read into a lost update. Recorded here because
   the Learn path relies on composition and limit 1's wording assumed a writer that only overwrites.
   **Amended 2026-08-02: the Learn path no longer uses this row.** The limit above is real, but
   the two-store publication it rests on has a second consequence in the OTHER direction, found by
   review: a consumer whose `exchange` lands *between* the writer's payload store and its flag
   store runs the already-visible new code and then has the flag re-raised behind it, so the same
   command is delivered twice. A repeated `commitThenStart` commits the pass its own first
   delivery started one block earlier — a reference measured from a single block, then serialized.
   The fix was not a wider mechanism but a narrower one: the Learn payload is two bits, so the code
   and the flag became one atomic word (`kLearnNone` = nothing pending) and the edge moved to the
   single-lock-free-scalar row. **This ADR's contract is unchanged** — it still governs the
   learned-target restore, which has a real payload — and condition 5 still describes how the Learn
   writer composes. Recorded here because the limit above was written as if it were the only
   consequence of publishing a record in two stores.

3. **A second stage during the payload reads.** The consumer exchanges the flag (acquire) and
   then reads the payload fields with relaxed loads. A `restoreLearnedTargets` landing between
   those reads can have the engine adopt one session's onset reference with another's tilt. It
   self-corrects at the next block top — the second call release-stored its flag after its
   payload, so the following block re-consumes and lands on that record in full — so the torn
   state lasts one block and affects only slow-slewing reference targets. Widening the payload to
   a single atomic struct would close it and would cost a lock on any target without a
   lock-free 12-byte CAS; not worth it at this stake.

## Consequences

- `THREADING_POLICY.md` gains a permitted-path row and a statement of the six conditions above;
  the sentinel row's exclusion sentence now reads "…is not this row" rather than "…is not
  permitted", since a wider record has a row of its own.
- Future multi-scalar GUI→Audio hand-offs are ordinary work, not gate items, **provided they meet
  all six conditions**. Anything unbounded, queued, multi-writer, or consumed mid-block remains a
  new cross-thread path and a Hard Stop.
- The process failure that produced this ADR is recorded rather than tidied away: the mechanism
  was built by analogy, documented under invented row names ("momentary request +
  flag-orders-payload") for two review rounds, and only an external reviewer's reading of the
  policy text caught it. `DOCUMENTATION_COVERAGE.md`'s fourth-round entry carries the lesson —
  "uses the same primitives as an approved mechanism" is not "is an approved mechanism".
- Confidence is `Verified` at authoring, unusually for a fresh ADR: the mechanism it ratifies is
  already in the tree with mutation-verified tests, which is the whole reason ratification was
  the cheap option.

## Related code

- `src/dsp/AnabasisEngine.h` — `restoreLearnedTargets`, `restoreNeverLearned`,
  `adaptiveRestorePending`
- `src/dsp/AnabasisEngine.cpp` — the block-top consume in `process()`
- `src/dsp/AdaptiveEngine.h` — `commitLearn`, `setLearnedTargets`, `hasLearned`
- `src/PluginProcessor.cpp` — `setStateInformation` (stages + mirrors), `getStateInformation`
  (prefers the staged record while it is unconsumed)

Evidence [Verified]:
- Source: the four files above
- Test: `AnabasisTests` `testAdaptiveRestoreLastStagedWins` (last-writer-wins, mutation-verified
  against a discriminator-ignoring mutant); `AnabasisStateTests`
  `testLearnCommitAndAdaptiveRoundTrip` (round trip, the `learn/noAudio` group for the
  read-back-your-own-staged-record condition, mutation-verified)
