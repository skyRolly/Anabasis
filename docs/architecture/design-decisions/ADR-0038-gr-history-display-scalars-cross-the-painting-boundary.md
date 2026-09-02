# ADR-0038 — The GR history publishes two display scalars to the painting thread, and they are safe by value rather than by snapshot

> **✅ RATIFIED — THE ARCHITECTURE REVIEW GATE IS CLEARED (2026-09-02).** The owner approved this
> decision on review of the shipped synchronisation. How it arrived stays in the record, because
> that is the half worth keeping: `ARCHITECTURE_REVIEW_GATE.md` lists "**Thread Model change** — new
> thread, new cross-thread path, new atomic ordering (`THREAD_MODEL.md`)"; `CLAUDE.md` repeats
> "threading-model change" in its hard-stop list; and
> [ADR-0027](ADR-0027-painting-thread-reads-editor-bookkeeping.md) decision clause 4 stated that the
> Message → Painting row "permits ONE scalar" and that anything "requiring the painting thread to
> see two values consistently is a new path again and returns to this gate". This record widens that
> permission to a second site carrying two scalars — so it was filed `Proposed`, flagged in the pull
> request as a gate item a green build does not clear, and held there until the owner answered.
>
> It was filed `Proposed` **with the code already in the tree**, deliberately: the path existed at
> 0.2.8 as an UNSYNCHRONISED read — plain `int64_t` and `double` written on the message thread and
> read from the GL render thread — which is undefined behaviour on macOS and Windows. Shipping the
> synchronisation was strictly better than shipping the race while the gate was answered. ADR-0027's
> own banner is the precedent and the warning — *"A rule can be quoted accurately and still not be
> applied"* — and the difference this time is that the round flagged it instead of asserting "no
> threading change" in a pull request.

**Status:** **Accepted — 2026-09-02**, on the owner's explicit approval of this record. It was NOT
covered by the standing blanket approval for the post-v0.1.0 rounds (ADR-0027 established that a
gated thread-model item is outside it). The approval is of the design recorded below — two relaxed
atomics, no consistency mechanism, safe by value — and explicitly *not* an instruction to revert the
synchronisation in order to preserve the older wording of the gate.

## Context

0.2.8 made the GR history scroll continuously (ADR-0023 item 6's dated amendment). The mechanism is
a sub-entry phase: `GrHistoryView::tick` advances a head smoothed at the nominal entry rate and
`paintHistory` reads it to place the trace between two ring entries. Two view-local scalars carry
that state from the tick to the paint:

- `shownHead` — the ring index the tick last observed, and therefore the head the frame draws
  (`paintHead`);
- `smoothHead` — that head advanced at the nominal entry rate, whose fractional part is the phase
  (`smoothedHead`, `phaseOf`).

`tick` runs on the message thread (it is a `juce::VBlankAttachment` callback through
`abgui::FrameClock`). `paintHistory` runs on **whichever thread paints**, and
`THREAD_MODEL.md` §"Which context paints" already settles what that is for this tree: the OpenGL
context attaches on macOS and Windows, never on Linux/X11, and when attached JUCE paints components
on the GL render thread.

As written at 0.2.8 both scalars were plain. That is a data race by the letter of the memory model
on exactly the two platforms where the context attaches — the same defect class ADR-0027 recorded
for `presetMenusOpen`, in the same editor, found the same way (review, not the field).

## Problem

The painting thread needs two numbers, not one. ADR-0027 permits one. The question this record has
to answer is not "may the paint read these?" but **"does the paint need to see them consistently?"**
— because that is the boundary clause 4 draws, and a mechanism that provides consistency (a seqlock,
a snapshot struct, a lock) is a materially larger change than two atomics.

## Options

- **A. Two independent relaxed atomics, no consistency guarantee.** Chosen. Requires an argument
  that every stale/fresh pairing is a legal frame — see the Decision.
- **B. A seqlock or a `std::atomic<struct>` snapshot.** Rejected. It buys a guarantee this display
  estimate does not need, and pays for it with a mechanism the tree does not otherwise have on this
  boundary (the ring's epoch seqlock exists for a bulk clear of 4096 entries, not for two scalars).
  `std::atomic<struct{int64_t,double}>` is 16 bytes and not lock-free on the supported targets, so
  it would put a lock in the paint path — which option D rejects for the same reason.
- **C. Encode both in ONE atomic `double`.** Rejected on correctness. The pair is not recoverable
  from `head + phase` alone: `smoothHead == head` (the lower clamp biting, which happens whenever
  more entries arrive than the estimate expected — routine at 48 kHz/512) and
  `smoothHead == head + 1` (parked) are both exact integers meaning opposite things, so `floor`
  cannot separate them. Capping the phase at `1 − ε` to make the encoding total was considered and
  rejected as a trick that trades a documented invariant for an undocumented one.
- **D. A lock around the pair.** Rejected for ADR-0027's reason: a lock on the paint path for two
  display scalars is worse than the problem.
- **E. Single-thread ownership — give the painting thread its own copy and its own clock.**
  Rejected: "the painting thread" is not one thread. It is the GL thread during
  `renderOpenGL`, the message thread on Linux, and the message thread again for
  `createComponentSnapshot` (which the state suite uses). State owned by "whoever painted last" is
  a worse race than the one being fixed.

## Decision

1. **The Message → Painting row admits a second site: `GrHistoryView`'s two display scalars**,
   `std::atomic<int64_t> shownHead` and `std::atomic<double> smoothHead`, written only by `tick` on
   the message thread and read only by `paintHistory` on the painting thread.
2. **`memory_order_relaxed` on every access**, for ADR-0027 clause 2's reason unchanged: neither
   value orders any other memory, publishes no payload, and is not a handshake. They are display
   estimates; the ring's own release/acquire index is what orders the DATA, and it is untouched.

   > **Amended 2026-09-02, same round, before any tag — see clause 7.** The two scalars above are
   > still relaxed and their representation is unchanged. What this clause did not anticipate is
   > that their VALIDITY is not self-evident: a phase means nothing outside the timeline it was
   > measured in, and a clear rewinds that timeline. The epoch that answers it
   > (`publishedEpoch`) is the one access on this boundary that carries ordering — `release` on the
   > store, `acquire` on the load — and it orders exactly those two publications and nothing else.
   > Relaxed would have been sufficient on a TSO target and NOT on a weakly ordered one, where the
   > epoch could become visible before the phase it announces; the ordering is what makes "the
   > epoch matches" a statement about a value the frame can actually read.

3. **The pair is safe by VALUE, not by consistency, and that is the load-bearing claim.** A frame
   may read either scalar's newer value with the other's older one. `GrHistoryView::frameFor`
   resolves any pairing to a position of `min (smoothHead, head + 1)` entry-pitches, because
   `smoothHead ≥ head` holds for every published pair (`smoothedHead`'s lower clamp) and both
   scalars only ever increase between publications. Every pairing therefore lies **between two
   frames the ramp itself produces**: the trace never jumps past where it was going, and no vertex
   ever moves rightward. This is clause 4's exclusion not applying rather than being waived — the
   painting thread does not need the two values to be consistent, and the property is pinned by
   test (`grPair`, four pairings per publication across a realistic 64-frame sequence) rather than
   asserted here.
4. **Lock-freedom is a build-time requirement, not an expectation.** `static_assert` on
   `std::atomic<int64_t>::is_always_lock_free && std::atomic<double>::is_always_lock_free`: a target
   where either is not lock-free would silently put a lock in the paint path, which is decision D,
   rejected. It must fail the build instead.
5. **`shownEpoch` stays a plain `uint32_t`** — it is read and written only by `tick`. The painting
   side samples the ring's epoch itself, for its own batch guard, and never reads this member. A
   member is added to the atomic set when the painting thread reads it, not because a neighbour is
   atomic.
7. **A published display estimate carries the identity of the state it describes** (added
   2026-09-02, the round's last finding). `shownHead` and `smoothHead` are indices into a ring that
   `GrHistoryBuffer::reset()` rewinds, so after a clear a published phase describes indices that no
   longer exist. A paint runs on its own schedule and can land in the window between the clear and
   the next tick; reading the stale `smoothHead` against the fresh head saturates
   `phaseOf`'s clamp to exactly 1 and draws the first frame of the new history one entry-pitch out.
   `frameFor` therefore takes the epoch the phase was published under and the epoch the frame is
   drawing, and uses the phase only when they agree — anchoring at phase 0 otherwise.

   **The count cannot stand in for the identity, and this is the case that proves it:** once the
   refill passes the old count, `shownHead <= live` (so `paintHead` no longer falls back) and
   `smoothHead - head == 1` (a legitimate parked value), and a stale publication is numerically
   indistinguishable from a current one. Both a head-fallback heuristic and an out-of-range test on
   `smoothHead - head` pass that case; only the epoch separates the timelines. Pinned by
   `grResetPhase`, and mutation-verified against **both** wrong answers — ignoring the epoch, and
   inferring the reset from the head — each of which fails the same three assertions.

8. **This row is still not a licence to widen.** ADR-0027 clause 4 stands with its boundary moved,
   not removed: the painting side may read scalars that are relaxed (bar the identity of clause 7,
   whose ordering is what makes the others checkable), read-only, payload-free, and
   whose every stale/fresh pairing is a frame the writer was itself about to produce. Anything the
   paint path WRITES, anything carrying a payload, and any pair whose cross-pairings are **not**
   legal frames is a new path again and returns to this gate.

## Consequences

- `THREADING_POLICY.md`'s Message → Painting row and `THREAD_MODEL.md` §"Which context paints" both
  name this second site; the policy's "one scalar" wording becomes "scalars whose cross-pairings are
  legal frames", which is the property, not the count.
- The claim in clause 3 is a claim about `smoothedHead`'s clamps. If a future change lets
  `smoothHead` fall below `head`, or lets either scalar decrease without an epoch change, the
  pairing argument fails and this record has to be re-taken. `grPair` fails first, which is the
  point of pinning it.
- **A rewind breaks the monotonicity by design** — `GrHistoryBuffer::reset()` rewinds the write
  index — and there the epoch guard discards the frame outright, so the pairing argument is never
  asked to cover it.
- Nothing on the audio thread changed. The ring's producer, its release/acquire index and its reset
  seqlock are byte-identical; this record concerns the reader's own bookkeeping only.
- **ADR-0027 clause 4 is amended, not reinterpreted.** Its text stands unchanged with a dated
  amendment naming this record: the boundary it draws moves from "ONE scalar" to "scalars whose
  every stale/fresh pairing is a frame the writer was itself about to produce", and everything else
  it excludes — a payload, a paint-path WRITE, a pair that genuinely needs consistency — is excluded
  exactly as before. Nothing about `presetMenusOpen` changes.
- **The permission is the property, not the count.** A future site that wants three scalars is
  admitted by the same test (are all cross-pairings legal frames?) and a future site that wants two
  scalars which must agree is not admitted at all. That is deliberately harder to satisfy than a
  numeric limit and deliberately easier to check.

## Related code

- `src/gui/GrHistoryView.h` (the member block and its `static_assert`; `frameFor`, `phaseOf`,
  `smoothedHead`, `parked`)
- `src/gui/GrHistoryView.cpp` (`tick` publishes; `paintHistory` reads)
- `docs/architecture/THREAD_MODEL.md` §"Which context paints"
- `docs/policies/THREADING_POLICY.md` (Message → Painting row)

Evidence [Verified]:
- Source: the files above at 0.2.8
- Test: `testGrHistoryReaderStaysInsideTheRingAndSeesEveryReset` §2 (`grPair`) — the pairing
  property across every cross pairing of a 64-frame publication sequence, plus the lock-free
  assertion mirroring the header's `static_assert`; §3c (`grResetPhase`) — clause 7, over every
  refill state including the equal count, repeated clears, and a clear-in-flight odd epoch
- Worklog: `worklogs/2026-09-01-gr-history-scroll-jitter.md` §9
