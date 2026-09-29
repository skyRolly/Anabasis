# ADR-0042 — A frozen trim latch survives a host re-prepare in the AUDIO, not only in the save

> **✅ RATIFIED — THE ARCHITECTURE REVIEW GATE IS CLEARED (2026-09-27).** The owner accepted this
> record — option C as written below. How it arrived stays in the record: it changes what Freeze does
> across a discontinuity, which `MODE_AND_ADAPTATION_POLICY.md` Enforcement puts behind the gate and
> `KNOWN_ISSUES.md` KI-006 deferred to the owner for exactly that reason. The round that carried it
> was asked, in the owner's own words, to make "the visible FREEZE state and the actual processing
> state agree" (audit finding **STATE-004**); this record is how, and the choice among the ways of
> doing it was the owner's to confirm. It was filed `Proposed` with the code in the tree, flagged in
> the pull request as a gate item a green build does not clear, re-verified path by path in the PR
> #42 review with no change to the code, and held there until the owner answered.

**Status:** **Accepted — 2026-09-27**, on the owner's explicit approval of this record (the
instruction of record: "Accept Phase 0 Decisions, Close PR #42"). It was NOT covered by the standing
blanket approval for the post-v0.1.0 rounds — a Freeze-semantics change is a gate item that approval
never reached. The approval is of option C as recorded below — the applied vector carried across
`AdaptiveEngine::reset()` and re-adopted by the first block whose snapshot has Freeze ON, with only
the applied/published set restored — and explicitly *not* of option A (Freeze OFF restarting from
rest) or of `KNOWN_ISSUES.md` KI-007 item 10 (the A/B trigger), which stay separate open decisions.
It extends ADR-0014 (the frozen vector *restored*) with what happens to a vector that is already
*applied* when the host re-prepares; ADR-0014 itself is untouched.

## Context

Freeze is the repeatability control: "while frozen, the adaptive layer contributes a constant, and
the plugin behaves as a static processor" (`MODE_AND_ADAPTATION_POLICY.md` invariant 3). Every host
`prepareToPlay` — a sample-rate or buffer-size change, and in some hosts a transport start or the
entry to an offline bounce — runs `AdaptiveEngine::reset()`, which zeroes the applied trim vector.
With Freeze ON, `finishBlock` never re-slews, so the zeros are then *held*: the audio runs on no
adaptive trims while FREEZE stays lit and the session save still writes the latched vector (the
retained set, rounds 40–42). Three answers to one question, none visible to the user; the audit
measured ~0.3–0.4 dB RMS and ~0.5 dB of per-channel GR between the frozen audition and a render after
a re-prepare. `KNOWN_ISSUES.md` KI-006 has carried this as its open "audio half" since 2026-08-03.

## Problem

Where does the applied vector live across a reset, and who decides whether it comes back — without
a thread crossing (the round-40 defect: a `juce::ValueTree` written from a host callback) and without
changing which A/B slot owns a latch (the round-42 defect: an engine-wide vector serialised by the
wrong slot)?

## Options

- **A. Carry the applied vector across every reset, Freeze on or off.** Simplest, and it also stops
  an unfrozen re-prepare from stepping the trims to zero. **Not chosen here:** it changes adaptation
  behaviour on every re-prepare, not only Freeze's promise — the audit recorded it as a separate
  owner call. Available later without undoing this record.
- **B. Carry it only if Freeze was ON for the last processed block.** **Lost:** the engine learns the
  Freeze state only from snapshots, so a user who engages Freeze while stopped and then re-prepares
  (buffer change, or a host that re-prepares on transport start) still gets the zeros.
- **C. Stash the applied vector at `reset()`, and let the FIRST BLOCK AFTER IT decide from its own
  snapshot: Freeze ON → the stashed vector comes back; OFF → adaptation restarts from rest, exactly
  as before.** **Chosen.**
- **D. Re-inject the RETAINED vector at the end of `reset()`** (KI-006's "one-line re-injection").
  **Lost:** the retained set is the last *latch*, scoped to a slot (round 42), not what was playing;
  and doing it inside `reset()` has the same stale-Freeze problem as B.
- **E. Have the wrapper re-stage a vector from `prepareToPlay`.** **Lost:** the round-40 design — a
  ValueTree access on a host callback, a TSAN-reported race and a threading-model change.
- **F. Make the indicator follow the audio (turn FREEZE off when a re-prepare drops the latch).**
  **Lost:** the audio thread may not write a parameter (MODE invariant 4's spirit, the threading
  model's letter), and it would defeat Freeze's purpose instead of keeping it.

## Decision

1. `AdaptiveEngine::reset()` **stashes** the applied vector and its "is this a real vector" flag
   before zeroing them — once per reset sequence (the engine's `prepare` resets the object twice, and
   the second must not stash the first's zeros). Features, envelopes, an in-flight Learn pass, and
   everything else `reset()` clears are cleared exactly as before.
2. At the top of the **first block after a reset**, before any pending ADR-0014 restore is injected,
   the engine calls `resumeAfterReset (p.freeze)`: with Freeze ON the stashed vector becomes the
   applied vector again; with it OFF nothing changes (the pre-existing restart from rest). A staged
   restore therefore still has the last word.
3. The carry republishes **only the applied set** — the four published atomics and the
   `hasPublishedTrims` flag, back to what they held. The **retained set and its generation are not
   touched**: nothing new was latched, and bumping the generation would hand an engine-wide latch to
   whichever A/B slot is live at the re-prepare (round 42).
4. No thread crossing: `reset()` runs where it always has (processing stopped, on the thread that
   owns the engine), and the decision runs on the audio thread at a block top. No allocation.

## Consequences

- **FREEZE lit ⇒ the latched vector keeps playing across any host re-prepare** — same rate, new rate,
  new block size. Pinned by `testAFrozenLatchSurvivesARePrepare` (the re-prepared render is
  bit-identical to the same vector restored and frozen, at 48 and 96 kHz; a negative control proves
  the vector is audible there) and by `testPreparedStateAndSlotOwnership` case 4.
- **Round 42's slot isolation is kept**: a re-prepare in a vectorless freeze-ON slot claims no other
  slot's vector (`testAFrozenLatchDoesNotFollowTheSlotSwitch`; a carry written with the retained
  bump fails it).
- **Unchanged, and still open (not this record's scope):** the A/B trigger the audit merged into
  STATE-004 — a switch into a freeze-ON slot with no `FROZEN_TRIMS` keeps applying the outgoing
  slot's latch while that slot saves none. This record preserves that pre-existing disagreement
  across a re-prepare rather than resolving it; `KNOWN_ISSUES.md` KI-007 keeps it with the owner.
- **With Freeze OFF a re-prepare still restarts adaptation from rest** (a step, which invariant 3's
  "rate-limited, not stepped" arguably also forbids) — option A, deliberately left to the owner.
- `KNOWN_ISSUES.md` KI-006's audio half is closed for the re-prepare trigger *(accepted 2026-09-27;
  KI-006 is closed and moved to `docs/POSTMORTEMS.md` INC-007)*.

## Related code

`src/dsp/AdaptiveEngine.h` (`reset`'s stash, `resumeAfterReset`, `publishApplied`),
`src/dsp/AnabasisEngine.cpp` (the block-top call), `tests/dsp_tests.cpp`
(`testAFrozenLatchSurvivesARePrepare`), `tests/state_tests.cpp` (`testPreparedStateAndSlotOwnership`
case 4, `testAFrozenLatchDoesNotFollowTheSlotSwitch`).

## Evidence

Confidence: **Verified** at engine and processor level — every assertion above fails when the carry
is removed, and the two slot-isolation assertions fail when it is written with a retained-generation
bump (worklog 2026-09-27 §Verification). Re-checked in the PR #42 review: moving the carry AFTER the
staged-restore injection (decision 2's ordering reversed) fails four DSP checks and the state suite's
"an unprimed session load restores the vector on the first block", so "a staged restore still has the
last word" is pinned; the lifecycle paths (re-prepare at the same and a new rate, Freeze engaged while
stopped, session load before `prepareToPlay`, save after a re-prepare, A/B, preset load, entry to an
offline bounce, host activate cycles) are tabulated with their evidence in
`docs/reports/2026-09-27-phase0-owner-decisions.md` §3, with a technical recommendation (decision
material; accepted 2026-09-27). Re-checked at acceptance on the accepted tree: code unchanged since
the review, the ordering mutation still pinned, preset and A/B coherence as tabulated there
(`docs/reports/2026-09-27-phase0-closure.md`). *Pointer (2026-09-29, PR split review):* the
closure carries no ordering or mutation content; the mutation was re-run on PR #43's head `2a5f8a8` —
with the carry moved after the ADR-0014 injection, 4 DSP and 1 state check fail
(`worklogs/2026-09-29-pr43-phase0-verification.md` §6(a)). **Not verified
in a DAW:** whether a given host re-prepares
on transport start or before a bounce (the audit's TEST-002); a scripted Carla host re-prepared the
plugin through deactivate/activate with Freeze and every parameter preserved, but its engine carried
no audio, so the audio half of the carry was not observable there.

- Audit finding STATE-004 and its challenge (the retained-generation constraint), KI-006.
- Depends on: ADR-0012 (staged record), ADR-0014 (restore at the silent bottom), ADR-0011
  (threading), `MODE_AND_ADAPTATION_POLICY.md` invariant 3.
