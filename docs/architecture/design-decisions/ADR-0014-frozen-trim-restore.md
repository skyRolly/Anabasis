# ADR-0014 — The frozen trim vector is restored: staged on ADR-0012's row, applied at the duck's silent bottom

**Status:** Accepted (2026-08-02 — owner decision on `OPEN_QUESTIONS.md` OQ-013, under the
v0.1.0 blanket approval of 2026-08-02; flagged ⊕ for the post-v0.1.0 fine review like every
decision taken under that approval)

## Context

ADR-0007's state schema carries a per-slot `FROZEN_TRIMS` child — the §5.4 trim vector latched
by Freeze — and OQ-013 blocked the **inject** half: the vector was serialized and carried
per-slot, but nothing ever handed it back to a running engine. ADR-0012 settled the transport
question (a four-scalar record fits the bounded staged-record row) and explicitly did **not**
settle the product question: may a restored vector be injected into a running engine at all, and
what does that do to the adaptation state machine? Until this ADR, `MODE_AND_ADAPTATION_POLICY.md`
invariant 3's Freeze clause had a gap: a frozen A/B slot restored with its parameters but not its
latched vector renders differently from the slot that was saved.

## Problem

1. Is injection permitted at all, and under what condition?
2. Where in the block does an injected vector land, given that it is a discontinuity in four
   values that feed audible stages?
3. What happens on a load→save with no audio between (the engine's published trims are one
   session stale), and on a slot whose Freeze is OFF?

## Options

- **A. Inject only for a freeze-ON slot, applied where every other restore-driven discontinuity
  lands** — the §2.8 duck's silent bottom (or the offline/unprimed direct-adopt). Freeze then
  holds the vector exactly (the latch skips the slew), which is precisely "the per-slot Freeze
  memory restoring". **Chosen.**
- **B. Inject unconditionally (freeze-OFF slots too).** The next audible block would slew away
  from the injected vector — a transient the user never asked for, restoring a memory the slot
  does not claim to hold. Rejected: with Freeze off there is nothing to restore *to*; the
  adapting vector is the truth.
- **C. Keep the restore serialization-only (status quo).** No injection risk; but invariant 3's
  bit-repeatability claim stays false across save/load and A/B for frozen slots, which is the gap
  OQ-013 existed to close. Rejected by the owner call.

## Decision

**Capture (message thread, `saveSlotFromLive`).** With Freeze ON the latched vector IS the
published one, so the save reads the four published-trim atomics into `FROZEN_TRIMS` — unless a
staged restore has not been APPLIED yet (`frozenRestorePending()`), where the engine's published
trims are stale and the previously loaded `liveFrozenTrims` copy is serialized instead: the same
mirror rule the ADAPTIVE child follows. The capture result goes into a local, never back into
`liveFrozenTrims`; the mirror is written by the restore paths only, because this same function is
the dirty-marker compare the editor polls at ~3 Hz and a display query must not rewrite
serialisable state.

**"Not applied yet" is a generation pair, not the record flag** (corrected 2026-08-03, review
round 24). The obvious reading — "pending == the ADR-0012 flag is still up" — is wrong by one
step: the block top clears `frozenPending` with its `exchange`, but the vector is only injected
(and therefore published) at the duck bottom up to ~34 ms later, and every `saveSlotFromLive()`
landing in that window read the PRE-restore trims. `restoreFrozenTrims` therefore also bumps
`frozenStageSeq`; the block-top consume records which generation its pending copy holds; and only
`injectTrims` stores that generation into `frozenAppliedSeq`. `frozenRestorePending()` is
`stageSeq != appliedSeq` — relaxed on both sides (THREADING_POLICY's generation-counter row: no
payload, and the payload's own ordering is still the release/acquire pair). A stage that lands
between the consume and the apply leaves `stageSeq` ahead, so it stays pending and is applied at
the next bottom; ADR-0012 §Known-limits-3's one-block payload tear is unchanged.

**Transport (ADR-0012's row, second instance).** `AnabasisEngine::restoreFrozenTrims` stores the
four scalars relaxed, then release-stores one `frozenPending` flag; the block top consumes it
with `exchange(acquire)` into a pending copy. Last-writer-wins; the writer may acquire-load the
flag (`frozenRestorePending`) — all six ADR-0012 conditions hold.

**Application (audio thread).** The pending copy is applied via `AdaptiveEngine::injectTrims`
(each field clamped to its declared bound, then published) at exactly two sites: the §2.8 duck's
**silent bottom**, and the **direct-adopt** branch (unprimed engine or offline entry) — the two
places DESIGN §7 already routes every restore-driven discontinuity. Freeze is ON in the slot that
staged it, so `finishBlock` holds the vector from the injection on: bit-exact restoration, not a
starting point for slew.

**Staging sites (message thread).** Both restore paths stage it, and only for a freeze-ON adopted
surface: `applySlotToLive` (A/B switch, undo/redo, preset bracket) and `setStateInformation`
(session load). The freeze value is read from the freshly adopted APVTS surface, never from the
raw incoming tree — the read rules have already run. A freeze-OFF slot stages nothing (option B's
rejection, enforced).

**Every stager must request the duck**, because the bottom is the vector's only landing site. That
was already true of `switchToSlot`, `applyFactoryPreset`, `applyPresetFile` and
`setStateInformation`; `undo`/`redo` did not duck and were corrected on 2026-08-03 (review round
24). An undo whose step moves no discrete stage never reaches a bottom on its own, so the staged
vector sat pending indefinitely and was injected at the next unrelated duck — an A/B switch, say —
into whatever slot was live by then. Ducking an undo is independently owed: DSP_POLICY invariant
8's click-free enumeration names the undo step as one of the three bulk swaps (ADR-0004).

**The duck request is DERIVED FROM THE RECORD, at the consume.** The block top that takes the
record sets the same `duckAsked` a `requestForcedDuck()` would have set, and `restoreFrozenTrims`
deliberately raises nothing itself — the comment there says so. That is what makes the paragraph
above structural instead of a rule four call sites must remember: the caller's request and the
stage are separate stores with a whole parameter restore between them (`adoptParamsTree` replaces
the state and re-asserts every raw value), so an audio block landing in that gap consumes the
request, runs the entire ~34 ms duck and returns to idle *before the record exists*. Adversarial
verification of the round-24 fix found that strand, and the first repair added a second store
beside the flag — which review round 26 then corrected to this derivation, because two stores are
two things to observe and the consumer reads them a dozen lines apart, so a block could take the
record and miss the request. Deriving it cannot go out of order with the record it comes from.
Every duck state reaches a bottom from there: `idle`/`in` start a fresh out-leg, `out` carries the
ask through `duckAskedWhileOut`, `bottom` applies the pending copy in its own branch, and the
unprimed/offline branch applies it directly.

*(This paragraph described the second-store version for one commit. The code always carried the
`NOTE the absence of a duck request here` comment explaining the derivation, so the drift was
visible in the tree rather than only in review — but an ADR outranks DESIGN.md, so a stale one is
the most expensive kind of stale document there is.)*

**Degradation, stated not hidden.** A vector staged for one slot lands wherever the engine is when
the bottom arrives — the stage is engine-wide, the slot is a wrapper concept. If Freeze is turned
off between stage and consumption, the injected vector publishes once and the next audible block
slews away from it. The same is true of anything else that changes the live slot inside the duck
window: an A/B switch, a preset apply or an undo landing there injects the OUTGOING slot's vector
into the incoming one, transiently, and the next audible block slews away unless the incoming slot
is itself frozen. Both are the documented last-writer-wins degradation of ADR-0012, not faults —
recorded with the full window (any live-slot change, not only a Freeze toggle) because the earlier
wording named only the toggle and read as if it were narrower.

**Known limit: the generation pair is engine-wide, the mirror is per-slot.**
`frozenRestorePending()` answers "has the last record staged *to the engine* been applied", with
no notion of which slot staged it, while `liveFrozenTrims` is the active slot's tree. So: slot A
(freeze ON) stages a restore; the user switches to slot B before the bottom; B is freeze-ON but
carries no `FROZEN_TRIMS`, so `applySlotToLive` stages nothing and the counters stay unequal from
A's record. A save of B inside that ~34 ms window takes the mirror branch and finds B's invalid
tree, so it serialises no vector even though B has a published latch. Bounded to that window and
to a slot with no previously loaded vector; the next save is correct. Closing it means keying the
generation pair by slot, which buys a per-slot counter to fix one window's worth of one field.

## Consequences

- `MODE_AND_ADAPTATION_POLICY.md` invariant 3's Freeze clause is now whole: a frozen slot's
  render is reproducible across save/load and A/B switches, including the latched adaptation.
- The OQ-013 Hard Stop banners (`PluginProcessor.h`, `THREADING_POLICY.md`, `THREAD_MODEL.md`)
  are lifted and replaced by references to this ADR.
- `injectTrims` is audio-thread API with clamping at the boundary: a hand-edited session cannot
  push a trim outside its declared bound (`MODE` invariant 4 holds against hostile state).
- ADR-0007's evidence row upgrades — the FROZEN_TRIMS inject half is no longer unwired.

## Related code

- `src/dsp/AdaptiveEngine.h:161-168` — `injectTrims` (clamp + publish)
- `src/dsp/AnabasisEngine.h:158-167` — `restoreFrozenTrims` / `frozenRestorePending` (the staged
  record); `src/dsp/AnabasisEngine.cpp:248-254` — block-top consume; `:300-304` (direct-adopt)
  and `:345-353` (duck bottom) — the two application sites
- `src/PluginProcessor.cpp` — the capture in `saveSlotFromLive`, the stage in `applySlotToLive`
  and in `setStateInformation`

Evidence [Verified]:
- Source: the files above
- Test: `AnabasisStateTests` `testFrozenTrimRestore` — capture premise, unprimed-load
  (direct-adopt), primed-load (duck bottom), A/B away-and-back, **undo restores the step's
  vector**, **the consume→bottom save window keeps the loaded vector**, no-audio load→save
  mirror, and freeze-off-stages-nothing; nine mutants (each application site, each staging site,
  the capture, the mirror guard, the freeze condition, the undo duck, the generation pair) each
  killed by a distinct check. `testFrozenSlotRoundTrip` still pins the byte-transport half.
