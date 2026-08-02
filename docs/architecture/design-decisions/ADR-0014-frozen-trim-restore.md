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
staged restore is still unconsumed (`frozenRestorePending()`, a load-then-save with no audio
between), where the engine's published trims are one session stale and the previously loaded
`liveFrozenTrims` copy is serialized instead: the same mirror rule the ADAPTIVE child follows.

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

**Degradation, stated not hidden.** If Freeze is turned off between stage and consumption, the
injected vector publishes once and the next audible block slews away from it — the documented
last-writer-wins degradation of ADR-0012, not a fault.

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
  (direct-adopt), primed-load (duck bottom), A/B away-and-back, no-audio load→save mirror, and
  freeze-off-stages-nothing; seven mutants (each application site, each staging site, the
  capture, the mirror guard, the freeze condition) each killed by a distinct check.
  `testFrozenSlotRoundTrip` still pins the byte-transport half.
