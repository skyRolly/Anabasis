# KNOWN_ISSUES.md

Confirmed limitations of the current build, with their workarounds. This file is
**tester-surfaced**: it is developer-authored but deliberately routed to testers, so entries are
written to be useful to someone holding a build, not only to a maintainer.

An issue is listed here only when it is **confirmed** — reproduced, or established from the code.
An unconfirmed report is not an entry (constraint C7); a *future* risk that has not yet
materialised belongs in `FUTURE_RISKS.md`; a resolved incident moves to `POSTMORTEMS.md`.

## Entry format

```
### KI-0NN — <one-line summary>

**Severity:** Low | Medium | High
**Status:** Confirmed | Mitigated | Reported upstream (external) | Fix pending re-test
**Affects:** <platform / host / format / configuration>

<What happens, and what the user sees.>

**Workaround:** <what a user can do today, or "none">
**Cause:** <the technical reason, if established>

Evidence [Verified | Partially Verified | Unverified]:
- Source: <file:lines>
- Test:   <test name>, or "none — not reproducible headlessly"
- Commit: <sha>
```

Numbering is sequential and permanent: a fixed issue is **removed from the open list and recorded
in `POSTMORTEMS.md`**, and its number is never reused. Entries are `###` (nested under
**Open issues**, not siblings of it) and listed in **ascending** KI order — the same order the
numbering reads in. `scripts/check-docs.py` checks table integrity, links, blockquotes and fences;
it does not check heading nesting, so this convention is held by hand.

## Open issues

*(KI-001 — unducked discrete transitions — KI-002 — inert Loudness Comp/Delta — and KI-009 — the
silent left channel — are FIXED and recorded as `POSTMORTEMS.md` INC-001/INC-002/INC-004; their
numbers are never reused. KI-009's entry ran to five months of round-by-round investigation, and
what was durable in it — the hypotheses the rounds excluded, and the two ways the probe that
finally reproduced it was vacuous first — moved into INC-004 with the mechanism, because a fixed
issue's record lives there.)*

### KI-003 — A host that restores state off the message thread is only partly defended against

**Severity:** Low
**Status:** Mitigated (macro layer), Confirmed (the wider path)
**Affects:** all platforms, VST3/AU — hosts that call `setStateInformation` on
a thread other than the message thread

VST3 does not promise which thread `IComponent::setState` arrives on, and JUCE
passes it straight through to `setStateInformation`. On such a host the restore
runs concurrently with the message thread.

The **macro layer is now safe** on that path: `MacroEngine::ScopedRestore` is
held across the whole restore body, so the 30 ms drain timer cannot apply a
macro mapping mid-restore and rewrite the nine managed parameters from the
curves. Before that, only a trailing `abortPendingMapping()` guarded it, which
held solely while the restore out-raced the timer.

What is **not** covered: `apvts.replaceState()` itself mutates a `ValueTree`
that the editor may be reading on the message thread, and no atomic in the
accepted set orders those two. Closing that is a thread-model decision — the
synchronisers `THREADING_POLICY.md` admits are "the listed atomics + the SPSC
ring", so introducing a lock or a restore-marshalling path is an Architecture
Review Gate item and an AI-agent **Hard Stop**, not a patch. There is also a
residual check-then-act window of a few instructions in the guard itself (the
drain can read `restoreDepth == 0` immediately before the restore raises it);
it is nanoseconds against the microseconds the unguarded code exposed, and it
closes only with the same thread-model decision.

A second member of the same family, recorded rather than left implicit: the
wrapper mirrors the staged ADAPTIVE record (`stagedAdaptiveLearned`,
`stagedRefOnset`, `stagedRefTilt`) so a save that lands before the next audio
block serializes what was loaded. Writer and reader are both nominally the
message thread, so on a well-behaved host there is no race at all; they are
**atomics** anyway, because on a host that delivers `setStateInformation` off
the message thread a concurrent save would otherwise read a half-written
mirror. ADR-0012's contract covers the engine-side record, not this copy.

**Narrowed 2026-08-03 (review round 32), not closed:** the MacroEngine's 30 ms tick used to run
the wrapper's drain even inside a `ScopedRestore` — the restore guard sat one level down, in
`drainPendingMapping` — which made the tick a SECOND concurrent writer of `liveDetachMask` on this
path. The guard now covers the whole tick, so the restore is again the only writer; the underlying
exposure (the restore itself writing `replaceState` / `liveDetachMask` / `livePresetName` /
`liveSelection` while the editor reads them) is untouched and still needs the thread-model
decision below.

A **third** member, added 2026-08-03 (review round 28) and **CLOSED 2026-08-03
(round 42)**: the §7 undo/redo stacks. `setStateInformation` used to clear
`undoStacks`/`redoStacks`/`gesturePreState`, plain `juce::Array<juce::ValueTree>`
members, while the editor read `canUndo()`/`canRedo()` on its 24 Hz tick and
`undo()`/`redo()` popped from them on the message thread — so an off-thread load
with the window open could race a `clear()` against a `removeAndReturn()`.

It closed WITHOUT the thread-model decision the rest of this entry waits on,
because it did not need one: the containers have exactly one legal thread, and
the fix is to stop the other one touching them. The loader now only bumps
`historyEpoch`, a relaxed counter, and the message thread does the clearing
itself at the next `syncHistory()` — the single reconciliation point every read
and write of the history passes through (`canUndo`/`canRedo`, `undo`/`redo`,
`pushUndoStep`, both gesture callbacks' message-thread branches,
`copySlotToOther`). No lock, and nothing blocks in a host callback. What remains
unclosed here is the state the restore genuinely must write — `replaceState`,
`liveDetachMask`, `livePresetName`, `liveSelection` (ADR-0022 — the preset
identity beside the name, same exposure shape: written by the restore's slot
overlay and by `applySlotToLive`, read by the editor's menu and ‹ › ring
through `currentPresetSelection()`) and **`liveFrozenTrims`** — which has no
such option and still needs the decision below.

`liveFrozenTrims` is named explicitly because two rounds of work around it could
otherwise read as having closed it. Round 40 added a SECOND writer of that member
from `prepareToPlay`, which ThreadSanitizer reported as a data race against the
editor's `presetDirty()` poll; rounds 41–42 removed that second writer and routed
the remaining one through `adoptFrozenMirror()`. A single writer is not a
message-thread-only writer: `adoptFrozenMirror()` is still reached from
`setStateInformation`, so the exposure is back to the one this entry already owns
— reduced to its pre-round-40 shape, not eliminated. `MODE_AND_ADAPTATION_POLICY`
carries the same qualification beside its ownership statement.

**Round 51 re-examined that boundary and narrowed the OPPOSING side of it,
without touching the thread model.** The question asked was whether the transfer
between state loading, audio processing and state saving still exposes an
inconsistent snapshot. It does, and the answer is unchanged in kind: the writer
is `adoptFrozenMirror()` reached from `setStateInformation`, and the readers are
`saveSlotFromLive()` (the A/B swap, the §7 undo push, `getStateInformation`) and
the ADR-0014 restore branch. What changed is who else was reading. The editor's
~3 Hz dirty poll used to run `saveSlotFromLive()` continuously, which made it a
permanent concurrent reader of `liveFrozenTrims` and `liveBaseline` for as long
as the window was open; the marker now compares `presetShapeFromLive()`, which
reads only the fixed parameter list and their atomics and no ValueTree member at
all. The remaining readers are all deliberate, host-initiated operations rather
than a display timer, so the window is now as narrow as it can be made without
the decision below. It is not closed: an off-message-thread `setStateInformation`
concurrent with a `getStateInformation` or an A/B switch still races the tree's
refcounted pointer, and closing THAT needs a lock or a marshalling path on the
state route — an Architecture Review Gate item and an AI-agent Hard Stop,
deliberately not attempted.

**Round 63 removed the last editor-poll writer of a wrapper tree, on the other
poll.** Round 51 cleaned the ~3 Hz dirty-marker poll of `apvts`/wrapper
`ValueTree` access; the 24 Hz **settings** re-seed then acquired one of its own,
because `normalisedUiScale()` wrote `iid::uiScale` back to `InternalState` when
the persisted percent was not a legal ladder step. That made a display timer an
opposing writer to `InternalState::replaceFrom`, which `setStateInformation`
reaches on whatever thread the host chose — a narrow window (it converged after
one tick per illegal value) but a new pairing on exactly the surface this entry
covers. The correction moved to `replaceFrom` itself, where the §4.4 read rules
for every other field already live and where such a value actually enters, so
both editor polls are now read-only with respect to the wrapper's trees.

**The construction and destruction halves of the MacroEngine drain are not
equally strong**, recorded 2026-08-03 (review round 29) so the pair is not read
as fully closed, and NARROWED TWICE since. `startDraining()` closes its race
STRUCTURALLY — the timer does not exist until both callbacks are assigned, so
no tick can observe a half-written `std::function`. `stopDraining()` stops the
timer, drops any posted update, and sets the one-way `drainStopped` latch that
`drainTick` tests first, so the SEQUENTIAL half is structural too: no trigger —
timer, posted update, `flushPendingMapping`, `refreshMapping` — can reach the
owner after teardown begins (round 37; the latch replaced nulling the
`std::function`s, which raced a tick already about to invoke one — round 36).
What remains, and what the pair is still not symmetric about, is a
`timerCallback` that has ALREADY entered: `juce::Timer` offers no join, so a
tick executing while another thread destroys the processor is not waited for.
That needs a host that destroys a processor concurrently with its own message
thread — the same premise class as the rest of this entry, and it closes with
the same thread-model decision rather than separately.

**Stated more exactly, and made CHECKABLE, round 51.** `timerCallback` and
`handleAsyncUpdate` both run on the message thread, so destroying the object ON
that thread leaves no residual at all — one thread cannot be inside a drain and
inside `~MacroEngine` at once, and `stopTimer()`/`cancelPendingUpdate()`
returning therefore means no drain is executing and none can start. The residual
above exists only for a host that destroys the processor OFF the message thread,
which is precisely this entry's premise. `~MacroEngine` now asserts
`juce::MessageManager::existsAndIsCurrentThread()`, so that host is reported at
the point of violation instead of surfacing as a use-after-free downstream. An
assertion rather than a spin-join deliberately: joining would block a teardown
thread on the message thread, which deadlocks whenever that message thread is
itself waiting on the caller — a worse failure than the one it removes, and a
lock on a path `THREADING_POLICY` keeps lock-free. Debug-only, so the Release
build the pluginval gate gets is unchanged.

**Workaround:** none required on the hosts tested so far — no case of an
off-message-thread restore has been observed against this plugin. The entry
exists because the assumption is load-bearing and undocumented elsewhere.
**Cause:** the state-restore thread is a host contract, not a plugin choice;
`THREADING_POLICY.md` names the audio and message threads and does not state
which one restores state.

Evidence [Partially Verified]:
- Source: `src/MacroEngine.h` (`ScopedRestore`), `src/PluginProcessor.cpp`
  (`setStateInformation`, `switchToSlot`, `applyPresetFile`)
- Test:   `AnabasisStateTests` `testDrainInsideRestoreIsSuppressed` (the mapping
  half) and `testTheWholeTickIsSuppressedInsideARestore` (the wrapper half, the
  round-32 narrowing) — both model the mid-restore drain single-threaded; the
  uncovered `replaceState` race is not reproducible headlessly
- Commit: P1 skeleton, thread-safety pass

---

### KI-004 — During an OS-factor switch, reported and actual latency disagree for the duck window

**Severity:** Low
**Status:** Confirmed (accepted by design — ADR-0004's trade)
**Affects:** all platforms, all formats — only while an oversampling factor or
phase change is in flight, and only on the processed path

Changing the OS factor/phase does not rewire immediately: the §2.8 duck fades
the processed path to silence (~6 ms), executes the rewire at the silent
bottom on a block boundary, and recovers (~28 ms). Between the parameter
change and that bottom — at most one host block plus the ~6 ms out-leg — the
engine still runs the OLD oversampler group delay while the wrapper already
reports the NEW total to the host. The disagreement is bounded by the
integer-latency table's span (≤ 67 samples at the extremes, `Latency.h`), and
the audio inside the window is the duck's fade itself, so nothing audible
carries the wrong alignment. A related edge: an instance sitting in **full
bypass** adopts a factor change without the duck (the bypass leg is the
delay-aligned dry ring, kept bit-exact), so the dry leg's alignment steps by
the same bounded amount at the block boundary instead of fading through
silence. Un-bypassing afterwards is already click-free (the ~10 ms crossfade).

**The silent bottom is quantised to the host block grid.** The duck leaves the bottom only at a
block top (`process()` evaluates the state machine once per block), while the post-latch refill
counter `bottomHoldSamples` runs down per PROCESSED sample inside the block. A hold that expires
mid-block therefore waits for the next block top before the ~28 ms in-leg starts, so the audible
silence is the ~6 ms out-leg plus the refill plus **up to one host block** — ≈ 43 ms of that at a
2048-sample block, 48 kHz. The bound above ("at most one host block plus the ~6 ms out-leg")
describes the LATENCY disagreement; this is the separate cost in silence, on the same grid.
Accepted for the same reason: leaving the bottom mid-block means running the rewire off a block
boundary, which is what the duck exists to avoid.

**A request raised while the host is not processing is spent on the next playback.** The forced
duck is a sticky flag consumed at a block top, and the engine has no clock: a swap made while the
transport is stopped (in a host that suspends the plugin without re-preparing it) leaves the
request standing, and its ~6 ms out / ~28 ms in leg then plays over the head of the next take
instead of over the swap it was guarding. Bounded to ~34 ms and audible only as a fade-in — the
swap itself was never heard, which is why this is a surprise rather than an artefact. A reset
clears it (`prepareToPlay` reaches `reset()`), so the case that survives is specifically the
stopped transport. Ageing the request needs a time base the audio thread does not have; the
wrapper sees the transitions the engine cannot (`releaseResources`, `suspendProcessing`), so it
is recorded here as a P5 wrapper question rather than patched in the DSP.

**Entering offline abandons an in-flight duck.** When `nonRealtime` first goes
true the engine adopts the new configuration directly (so a bounce does not
open with a fade — see the note below), which forces the duck to idle at unity.
If a duck happened to be in flight at that instant — a factor/model rewire, or
a wrapper bulk swap requested moments earlier — the processed gain steps from
its current value (as low as 0.0 at the silent bottom) to 1.0 in one sample,
and the latch may then clear the lookahead ring at full gain. Bounded to the
first sample of an offline render, and the alternative (carrying a monitor
fade into a bounce) is worse; recorded so it is not rediscovered as a defect.

The same latch boundary also steps two internal CONSUMERS of the dry leg that
the duck does not cover: the §2.7 dry loudness measure and the §5.4 adaptive
feature extractor are fed the delay-aligned dry signal, whose read offset
moves by the same ≤ 67-sample difference when `osLatBase` re-latches. The
splice can register once as a spurious transient in the onset detector and
as a sub-millisecond hiccup in a 400 ms loudness window — both absorbed by
their own smoothing (the trims slew over seconds, the measure gates at
−70 LUFS), so this is measurement noise at the switch instant, not an
audible or persistent error.

Related and deliberate, so testers do not report it as a hang: the silent
bottom is **held until the pipeline refills** after a factor/phase latch —
the latch empties the 10 ms lookahead line and resets the oversampler, so
recovering immediately would splice real audio in partway up the fade. A
factor switch therefore mutes for roughly 45 ms end to end (≈6 ms out, ≈11 ms
refill rounded up to the block grid, 28 ms in) rather than the ~34 ms of the
two fade legs alone. Every other transition — A/B, preset, session load, EQ
position, colour model — does not clear the line and keeps the ~34 ms shape.

**Workaround:** none needed in normal use; for sample-surgical A/B of factor
settings offline, render each factor separately instead of automating the
switch mid-render.
**Cause:** ADR-0004 fixes the *reported* latency per factor and forbids
mid-block latency changes; the duck trades a ≤ 40 ms alignment window for
click-free, allocation-free switches on the audio thread.

Evidence [Verified]:
- Source: `src/dsp/AnabasisEngine.cpp` (block-top duck state machine,
  `latchOsConfig`), `src/dsp/Latency.h` (`kMaxOsLatencySamples`)
- Test:   `testDuckWrapsOsLatch` (the window is the duck envelope),
  `testOsLatencyMatrix` (the bound); the bypassed-instance step is
  established from the code path, not reproducible as a click headlessly
- Commit: PR #5, P2 transition layer

---

### KI-005 — Moving Clip Drive off exactly 0 dB steps the transfer by half a sample

**Severity:** Low
**Status:** Confirmed (fix deferred — needs a designed engage crossfade, see below)
**Affects:** all platforms, all formats — a direct `clipDrive` move (or a
Character-macro move that carries it) across the 0 dB boundary during playback

The clipper's sub-block is skipped **exactly** at 0 dB drive, which is the
bit-identity contract. One sample later, with the drive smoother barely off
zero, the ADAA-1 branch runs — and in the curve's linear region its divided
difference is `(u + u_prev)/2`, i.e. a `(1 + z⁻¹)/2` FIR. The stage therefore
swaps *identity* for *a half-sample delay plus a cos(πf/fs) droop* in one
sample. Both trajectories are individually smooth; the join between them is
not, and the step is proportional to the signal's **slew**, not to the drive
amount, so smoothing `driveDb` does not shrink it. An 8 kHz tone loses ~1.2 dB
and shifts ~12° at that instant; on broadband programme the artefact is one
sample at roughly half the local sample-to-sample difference. The same happens
in reverse when drive returns to exactly 0.

Not exposed on the bulk-swap paths — A/B, preset and session loads are covered
by the §2.8 duck. The reachable case is a knob or automation move.

**Workaround:** automate `clipDrive` from a small non-zero value rather than
from exactly 0, or make the move while the transport is stopped.
**Cause:** the exact-zero skip is a change of transfer, not of gain, so the
two branches cannot be joined by a gain crossfade keyed on drive. A correct
fix needs a time-based engage ramp (~20 ms) that keeps the ADAA branch running
while it fades out, primed like the other smoothers so a render does not open
mid-fade, and landing on exactly 0/1 so the bit-identity skip is preserved.
**A drive-keyed blend was tried and rejected** during the review round that
found this: it removes shaping the clipper legitimately owes at tiny drive
with a loud signal (the knee at unity gain), which `testClipCurveAndCompensation`
pins deliberately. The ramp belongs with the ⊕ tuning pass, where it can be
built with its own coverage rather than patched around an existing test.

Evidence [Verified]:
- Source: `src/dsp/ClipSat.h` (`clipOn` exact-zero test, the ADAA branch)
- Test:   none yet — the property needs the curvature-based measurement
  described in the header note; a max-delta test does NOT catch it (the engage
  sample's first difference is *smaller* than the signal's own)
- Commit: PR #5, recorded 2026-08-01

---

### KI-006 — A sample-rate change silently drops a frozen slot's adaptation from the AUDIO and the readout, while the SAVE keeps it

**Severity:** Medium
**Status:** Confirmed — **audio half only** (fix deferred: it is a Freeze-semantics decision, not a
repair). The save half is CLOSED (round 38, corrected in 39, completed in 40); the heading above
describes what is left, and it used to describe the reverse.
**Affects:** all platforms/formats. Trigger: Freeze ON with a latched trim
vector, then any `prepareToPlay` — a host sample-rate or block-size change.

`AnabasisEngine::prepare` calls `AdaptiveEngine::prepare` → `reset()`, which
zeroes the internal `trims` struct along with the features **and republishes
them**: `reset()`'s last step is a `publishTrims` call, so all four published
atomics go to zero too, whatever Freeze says. (This entry asserted the opposite
until 2026-08-03 — "the PUBLISHED trim atomics are NOT zeroed, and cannot be",
reasoning from `finishBlock`'s `if (! freeze && audible)` guard and missing the
`reset()` publish. Corrected against the code, which is the authority.)

The consequence was therefore SYMMETRIC, not one-sided: after a re-prepare the
engine applies a zero trim vector to the audio, the Advanced overlay reads zeros
with it, **and the state half went with them**. What never went to zero is the
wrapper's `liveFrozenTrims` mirror — the copy a load/A-B/undo placed — so a slot
whose vector arrived that way still serialised the right thing; a vector latched
LIVE in the session had no mirror, and after a re-prepare there was nothing left
to save (before round 39 the save wrote the post-reset zeros, an INVALID vector
that the next load re-injected; after it, no `FROZEN_TRIMS` child at all).

**The state half is CLOSED (round 40, re-implemented correctly at round 41,
slot-scoped at round 42) and the audio half is what remains.** The fix is an ownership statement rather than
a Freeze decision: the ENGINE owns the durable copy, in `AdaptiveEngine`'s
**retained** trim set — four lock-free scalars plus a release-stored flag that
`reset()` does not clear, so the latched vector outlives a re-prepare exactly as
`learned`/`refOnsetRate`/`refTiltDb` always have. The PUBLISHED set keeps its
current meaning (what the DSP is applying, zeroed by `reset()`), which is what
keeps the overlay honest. The save prefers the engine whenever
`! frozenRestorePending() && hasRetainedTrims()` — both clauses in
`engineFrozenTrimsIfLive()` — and falls back to the wrapper's mirror for the
staged-but-unapplied window, which is the only window the mirror covers.
Guarded by `testPreparedStateAndSlotOwnership` case 4 (`liveLatch:`), which
asserts the two sets part company at the re-prepare.

**Round 42 added the slot scope the retained set could not carry by itself.**
`FROZEN_TRIMS` is per-slot; the retained vector is engine-wide and knows nothing
about A/B. After a switch into a freeze-ON slot holding no vector of its own,
nothing stages a restore (the stage is gated on the mirror being valid), the
generation pair stays equal, and the incoming slot's next save serialised the
OUTGOING slot's latch as its own — after which the next A/B or undo restore
injected it. The retained set is a runtime CACHE of the last latch and may only
answer for the slot it was filled under, so the wrapper records the retained
GENERATION whenever the live surface's frozen ownership changes
(`adoptFrozenMirror`, the single writer of the mirror) and adopts the engine's
answer only when the generation has advanced past it. `testAFrozenLatchDoesNotFollowTheSlotSwitch`.

**Round 40's version of this fix was itself a defect, recorded because the shape
recurs:** it declared the wrapper's `juce::ValueTree` mirror the durable owner
and had `prepareToPlay` copy the latch into it. `prepareToPlay` is a host
callback JUCE does not deliver on the message thread, and the editor's
`presetDirty()` poll read and `createCopy()`d that same member continuously (it
went through `saveSlotFromLive()` until round 51 moved the marker onto
`presetShapeFromLive()`, which touches no ValueTree at all) — both sides gated
on Freeze being ON, so the windows coincided exactly rather than being disjoint. ThreadSanitizer reports it as a data race on
`ReferenceCountedObjectPtr<ValueTree::SharedObject>::get()` plus one on the
refcount increment; the current code is TSAN-clean on the same stimulus
(`testTheFrozenLatchNeedsNoThreadCrossing`). The lesson is general: state that
must survive a re-initialisation should be RETAINED where it already lives, not
copied across a thread boundary to somewhere more durable.

What is still open, unchanged: after a re-prepare the ENGINE applies a zero trim
vector and the Advanced overlay reads zeros until the next load, A/B or undo
re-injects the mirror. Closing that means "keep the trims across `reset()`",
covering the published atomics as well as the internal struct since both are
cleared together — which is a Freeze-semantics change, an Architecture Review
Gate item and an AI-agent Hard Stop (`MODE_AND_ADAPTATION_POLICY` Enforcement),
so it stays owner's business rather than a repair.

**Found by** the adversarial verification pass over review round 24
(2026-08-03), not by the review itself; it PREDATES ADR-0014 (P4 shipped the
same reset), which is why it is recorded rather than folded into that round's
fixes.

**Why it is not simply "keep the trims across reset".** That is the likely
resolution — the trim vector is a bounded, rate-independent control value, not
signal state, and carrying it would also make an un-frozen re-prepare re-slew
from where it was instead of jumping to zero — but it changes what
`MODE_AND_ADAPTATION_POLICY` invariant 3's Freeze clause promises across a
discontinuity, which is an owner/ADR call, not a bug fix. It would also have to
carry the PUBLISHED copy, not just the internal struct — see the correction
above. The alternative (re-stage the vector from the wrapper at
`prepareToPlay`) used to be blocked by the same asymmetry — `liveFrozenTrims`
held one only after a load — and round 41 removes that objection from the other
side: the engine's RETAINED set holds a live latch too, so the audio-side fix no
longer needs the wrapper at all. It would be a one-line re-injection from the
retained values at the end of `reset()`. It is still not done here, because that
IS the Freeze-semantics change this section defers; the retained set deliberately
stops at the serialization boundary and feeds no audio path.

**The SAVE half of the same gap, added 2026-08-03 (review round 27), CLOSED 2026-08-03 (round
38).** The description above is about the audio; the capture had the mirror-image problem.
`saveSlotFromLive` read `publishedTrim*()` whenever Freeze was on and no restore was pending — and
on an instance that was prepared but had never PROCESSED a block, those atomics are all zero, so
the session serialised an all-zero `FROZEN_TRIMS` for a slot the user believes holds a latched
vector and the next load injected zeros. It needed no answer to the audio half after all: the
capture now also requires `AdaptiveEngine::hasPublishedTrims()`, because "all four read 0" is
otherwise indistinguishable between *measured, and the answer is no trim* and *initialisation* —
and a value nothing measured cannot be more truthful than the one the slot already holds. The flag
tracks the CURRENT contents of the four atomics rather than "has one ever been published": it is
set by an audible `finishBlock` and by an ADR-0014 `injectTrims`, and CLEARED by `reset()` along
with the values. Round 38 shipped it as a one-way flag set inside `publishTrims()` — which
`reset()` also calls — so it read true for every prepared instance and the guard was inert; round
39 made it mean what its name says. Round 40 closed the remaining save case — a latch established
LIVE, whose only record was the atomics the re-prepare cleared — and round 41 re-implemented that
closure without a thread crossing, by retaining the vector in the engine instead of copying it into
the wrapper's mirror from a host callback (see above). The AUDIO half above is untouched and still
needs the owner call.

**For the post-v0.1.0 fine review.**

### KI-007 — Preset/Freeze bookkeeping edges the fine review must settle together

**Severity:** Low (each is display or recall bookkeeping; none changes a rendered sample on its own)
**Status:** Recorded — opened by review round 25 (2026-08-03) with three items and extended by
rounds 27, 28, 31 and 33; deliberately NOT fixed, because each is a semantics question rather than a
defect, and several are the same question KI-006 asks. **The count is deliberately not in the
heading**: it was "Three" for one round after the fourth item landed, which is exactly how a
fine-review checklist gets read as shorter than it is. Numbered items below are the list of
record.

1. **A factory-preset apply keeps the slot's frozen-trim vector.** `applyFactoryPreset` clears
   `liveBaseline` and the detach mask but not `liveFrozenTrims`, and `freeze` is
   preset-excluded — so a slot that was frozen carries the PREVIOUS programme's latched vector
   across a preset change, the next save serialises it, and the next A/B or undo restore
   re-injects it. Whether a preset should carry or clear the Freeze memory is a
   `MODE_AND_ADAPTATION_POLICY` invariant-3 question, and it is the same question **KI-006**
   asks about a re-prepare. Settle them together or the two answers will disagree.

2. **RESOLVED 2026-08-08 (ADR-0022) — preset-ring navigation identified the current entry by
   NAME.** `stepPreset` matched `currentPresetName()` against the factory table first and the
   user files second, so a user preset saved as "EDM Club" resolved to the factory index and the
   arrows walked from the wrong place. The fix is the SOURCE-tracking this item asked for, held
   where it survives: the wrapper records the identity (a factory id or the user file —
   `liveSelection`, carried on the SLOT tree through undo, A/B, Copy and the session), and
   `stepPreset` and the menu mark both resolve through
   `PresetManager::selectedPresetRow` — identity first, the name scan only for identity-less
   (pre-ADR-0022) state, where the factory-first answer remains the documented tie-break.
   An interim editor-local hint (round 44's `rememberPresetSource`) was replaced by that
   identity, not kept beside it.

3. **RESOLVED 2026-08-03 (round 38) — undo/redo restore `presetBaseline`.** They restored the whole
   SLOT tree, `presetName` included, while `applyFactoryPreset` / `applyPresetFile` /
   `savePresetFile` reset the dirty datum — so undoing a preset apply left the name and the datum
   describing different presets. Neither of the two routes this entry weighed was needed: the
   baseline did NOT have to go into the StateSet (that would be an ADR-0007 schema change and a
   Hard Stop) and undo did not have to recompute it. The stacks are session-local and never
   serialized, so a history entry is now the pair — `{ slot, baseline }` — taken and restored
   together at the one place entries are made.

4. **RESOLVED 2026-08-03 (round 37) — the preset menu's raw LookAndFeel pointer.** `showPresetMenu`
   handed the menu `&lnf`, an editor member the menu window could outlive if a host tore the window
   down while it was open — the one part of round 24's `SafePointer` hardening the look-and-feel did
   not cover. Both repairs considered here carried their own risk (`dismissAllActiveMenus()` in the
   destructor also closes another instance's menu; a shared static trades it for static-destruction
   order at DLL unload), and neither was needed: the menu is now given
   `Options::withParentComponent (this)`, so JUCE's MenuWindow is a CHILD of the editor and cannot
   outlive it, and `getLookAndFeel()` reaches `lnf` up the parent chain with no pointer to dangle.
   Kept numbered rather than removed so the references in `HANDOVER.md` and the coverage audit
   still resolve.

5. **RESOLVED 2026-08-04 (round 51) — the dirty marker keyed on the whole slot tree, so
   preset-EXCLUDED parameters marked a preset as edited.** The spec question this item held open
   is answered, and by the code that already knew the answer: `presetShapeFromLive()` projects the
   live state onto exactly what `PresetManager::savePreset` writes — the non-excluded parameters at
   their SNAPPED preset values (`PresetManager::presetValueOf`, now shared by the writer and the
   projection so the two cannot drift) plus the `DETACH_MASK`, which presets do carry. `BASELINE`,
   `FROZEN_TRIMS`, the exact-`raw` attribute and every preset-excluded id are dropped, which
   settles the "second input" below with them: trim CONTENT cannot reach the comparison because
   `FROZEN_TRIMS` cannot. Dropping `raw` also fixed the converse case nobody had recorded — a
   mid-step raw move on a discrete parameter marked a preset edited although the value a preset
   stores had not moved. `testTheDirtyMarkerMeasuresOnlyWhatAPresetCanCarry`. A second consequence
   is threading rather than display: the editor's ~3 Hz poll no longer reaches `saveSlotFromLive()`,
   so it no longer takes the APVTS tree lock (see KI-008) or reads the wrapper's ValueTree members
   (KI-003) — it walks the fixed parameter list and their atomics.
   **Round 52 made the "exactly `savePreset`'s content" claim structural rather than factual.**
   Round 51 shared the two RULES (`isPresetExcludedParam`, `presetValueOf`) but left the two WALKS
   distinct — the writer over `apvts.state`'s PARAM children, the projection over
   `getParameters()`. They agreed only because APVTS happens to create one tree child per
   parameter, which is a fact about JUCE rather than an invariant of this code: a parameter
   registered without a node (or a node without a parameter) would have put content in the file the
   marker could not see, or the reverse. `PresetManager::forEachPresetParameter` is now the single
   traversal both run, visiting in id order so the bytes of existing `.anabasis` files are
   unchanged (`getParameters()` is registration order; the tree's was id order, and registration
   order would also churn again on any future layout reshuffle).
   `testThePresetWriterAndTheDirtyMarkerCoverTheSameParameters` checks both directions — the two
   collections against each other, and every id the file carries against the marker.
   The original text is kept below because two other entries reference its reasoning.

   `presetDirty()` compared `presetBaseline` against a fresh `saveSlotFromLive()`, which
   carries the FULL parameter set plus the `FROZEN_TRIMS` child. `freeze` is preset-excluded
   (`isPresetExcludedParam`), so no preset can ever have carried it — yet toggling Freeze changes
   the slot tree twice over (the `freeze` PARAM node, and the appearance of `FROZEN_TRIMS` once
   the capture branch fires) and flips the name to edited. The view-tier exclusions behave the
   same way. Display-only. The fix is a comparison that drops what a preset cannot carry, which
   means deciding exactly that set (excluded params, `FROZEN_TRIMS`, `BASELINE` — but NOT
   `DETACH_MASK`, which presets do carry) — a small spec question, and the reason it is recorded
   here with items 1–4 rather than guessed at inside a no-new-bugs round.
   **One input to that decision is now settled** (round 33 recorded it, round 38 closed it): a
   freeze-OFF slot no longer serialises a `FROZEN_TRIMS` child at all. `frozen` used to start from
   the carried mirror unconditionally, so a slot that was frozen, loaded, then un-frozen wrote the
   old vector into every later save — a latch serialised by a slot that §5.4/MODE invariant 3 give
   nothing to latch. That was a state-consistency defect on its own, and it removes the
   `FROZEN_TRIMS` half of this item's noise; what REMAINS open is the preset-EXCLUDED parameter
   half (`freeze` itself, and the view-tier ids), which is still the spec question above.
   **A second input, added round 41: the CONTENT of `FROZEN_TRIMS` is a function of when Freeze was
   engaged, not of the parameter state.** With Freeze ON the latch holds, so the comparison is
   stable moment to moment — but the values latched are whatever the last audible block had
   produced, so engaging Freeze at two different instants over identical parameters yields two
   different slot trees, and therefore two different answers to "is this preset edited?". This is
   the same spec question one level down: if the comparison drops what a preset cannot carry, the
   trim content goes with it. Recorded because the item read as being only about the `freeze` PARAM
   node.

6. **The spectrum view freezes rather than decaying when audio stops.**
   `SpectrumView::tick` returns early when neither capture ring's write count moved, so the
   per-bin EMA stops and the last analysed trace stays on screen indefinitely after a transport
   stop or a plugin suspend. It is cheap and reads as deliberate ("idle: nothing new"), but most
   analysers decay to the floor, and a frozen trace can be mistaken for live signal. Which
   behaviour this product wants is a listening-pass call, not a repair — the fix (run the EMA
   toward the floor on an idle tick) is three lines once the answer is known.

7. **RESOLVED 2026-08-03 (round 37) — Copy A→B and the destination's undo history.**
   **[Superseded 2026-08-06 by ADR-0018 — the round-37 answer is reversed.]** The Copy is now an
   undo step ON the destination whose pre-copy history is KEPT (the sibling's semantics, per the
   owner's 0.1.1 directive); the clear-both-stacks resolution below is the historical record of
   the 0.1.0 answer.
   `copySlotToOther()` replaced `storedSlot` but not `undoStacks[1 - activeSlot]`, so switching to
   the copied-into slot and pressing undo restored a pre-copy state the user never edited from the
   copied values — silently discarding the copy AND that slot's last edit, because the copy itself
   is not an undo step. It needed no new semantics: `setStateInformation` already clears both
   slots' stacks because "a load starts a fresh history", and a Copy is that event for one slot, so
   `copySlotToOther()` clears the destination's stacks too. What REMAINS open is only the narrower
   question item 3 asks (whether undo should also restore the dirty baseline); the two were
   recorded together and only the history half is settled.

8. **RESOLVED 2026-08-03 (round 37) — a macro gesture that moves nothing now re-lands the curve.**
   `audioProcessorParameterChangeGestureBegin` cleared the detach mask for a macro-knob gesture but
   armed no mapping, so a click-and-release left the freshly re-engaged parameters holding the
   user's off-curve values. Recorded as a spec question, and settled by the specification rather
   than by a new choice: `MODE_AND_ADAPTATION_POLICY` invariant 3 already reads "the next macro
   gesture re-engages every detached parameter **through the normal rate-limited glide**", and
   round 30 had already fixed the identical "re-engaged but off-curve" shape on the tick path. The
   begin now calls `MacroEngine::armMapping()` alongside the re-engage — a relaxed store, so it is
   safe from whichever thread the gesture arrives on — and the two re-engagement routes (gesture
   and `resetToMacro()`) do the same two things. Inert when nothing was detached, because
   `setParam` skips writes that would not change the value.

9. **RESOLVED 2026-08-03 (round 38) — reset-to-macro is undoable.** It clears the detach mask and
   re-lands the curve on all nine managed parameters, but pushed nothing onto the §7 stack, and its
   writes are ungestured so no drag step appeared either — leaving the one Simple-view affordance
   that changes nine parameters at once as the only one the user could not take back. It needed no
   new grammar: the preset applies already push their pre-state before changing anything, so
   `resetToMacro()` does the same. No duck request was added — unlike a preset apply or an undo it
   rewires no discrete stage, and DSP invariant 8's click-free enumeration is about the bulk swaps
   that do. Item 7's Copy A→B, recorded here as the same shape, was settled in round 37.

**For the post-v0.1.0 fine review, alongside KI-006.**

---

### KI-008 — Two JUCE locks are taken in opposite orders on two reachable paths (potential deadlock)

**Severity:** Medium
**Status:** Confirmed by ThreadSanitizer (fix deferred — the change is to §7's snapshot point, an
Architecture Review Gate item)
**Affects:** all platforms/formats. Requires a host that delivers `setStateInformation` — or any
APVTS parameter write — on a thread other than the message thread, concurrently with a
gesture-begin on the SAME parameter. The KI-003 premise, one step worse than a torn read.

Two mutexes, both JUCE's own:

* **M0** — a `juce::AudioProcessorParameter`'s listener lock, held while it dispatches
  `parameterGestureChanged` / `parameterValueChanged` to its listeners.
* **M1** — the single `CriticalSection` inside `juce::AudioProcessorValueTreeState` that guards the
  parameter tree (`copyState()` and `ParameterAdapter::setDenormalisedValue` both take it).

They are acquired in **both** orders:

| Order | Path |
|---|---|
| M0 → M1 | `AnabasisAudioProcessor::audioProcessorParameterChangeGestureBegin` (`src/PluginProcessor.cpp:212`) takes the §7 pre-state with `saveSlotFromLive()` → `copyStateWithRaw()` → `apvts.copyState()`, from **inside** the listener callback that already holds M0. |
| M1 → M0 | `APVTS::ParameterAdapter::setDenormalisedValue` holds M1 and calls `setValueNotifyingHost` → `sendValueChangedMessageToListeners`, which takes M0. Reached by the macro mapping, by `reassertFromRaw`/`adoptParamsTree`, and so by every restore path. |

One thread cannot deadlock on this. Two can: the message thread starting a drag on parameter P
while a host thread restores state and writes P.

**The editor WAS a continuous acquirer of M1 — that half is closed** (recorded round 49, removed
round 51). `refreshPresetDisplay` polls `presetDirty()` on the 24 Hz tick, throttled to every 8th
tick, and that used to reach `saveSlotFromLive()` → `copyStateWithRaw()` → `apvts.copyState()`,
which flushes pending parameter values and takes M1 — so with the window open the message thread
acquired M1 at ~3 Hz all the time, plus immediately on every user action (the `recomputeNow` path).
It never added an EDGE to the inversion; it made the M0 → M1 side something the plugin did
continuously rather than only at a gesture-begin, which is what a probability estimate for this
entry rests on. The marker now compares `presetShapeFromLive()`, which reads the fixed parameter
list and each parameter's own atomic and takes no tree lock at all, so the only M1 acquisition left
on the message thread is the gesture-begin snapshot itself — back to "at a gesture-begin", which is
the rate this entry was originally scoped for. **The inversion is unchanged and the entry stays
open:** the two edges are exactly as tabulated above, and the fix is still the §7 snapshot-point
decision.

**0.1.4 moves the rate back in the OTHER direction, and this paragraph is the place that has to say
so.** A bracketed preset apply now takes `saveSlotFromLive()` TWICE — once in
`openPresetUndoBracket` for the pre-state, once in `closePresetUndoBracket` for the comparison that
decides whether the apply restored anything — where the pre-0.1.4 path took it once. Each reaches
`copyStateWithRaw()` → `apvts.copyState()` and therefore M1. No new EDGE: both run on the message
thread from the editor's own click handlers, never from inside a parameter listener holding M0, so
the inversion tabulated above is untouched. What changes is the frequency term the estimate above
rests on — and the path that drives it hardest is the preset ring, where `‹`/`›` walks
`applyPresetFile` once per keypress and a user can hold the key down. Round 51 halved the
message-thread M1 rate and this doubles what remains on that one path; neither is the defect, and
both belong in the same paragraph so the estimate is never read off a stale half of the story.

**The lock-order inversion tabulated above is the interleaving KI-003 is about**, and the §5.3
machinery exists *because* gestures and parameter writes on the same managed parameter do overlap
across threads. (That sentence closed the pre-0.1.4 paragraph, where "that" could only mean the
inversion; the rate paragraph was later inserted in front of it and left it appearing to point at
the doubled `saveSlotFromLive()` instead — which is a rate, not an interleaving. Named explicitly
here rather than pronouned, in a document whose whole point is that the estimate is never read off
a stale half of the story.)

**Why it is not fixed here.** The M0 → M1 edge is the §7 undo grammar's pre-state snapshot, and it
has to be taken *at* gesture begin — deferring it to the next drain tick would capture a state the
first edit had already changed, which is a different undo grammar rather than a repair. The other
edge is inside JUCE. Removing the inversion therefore means changing where §7 captures its
pre-state (for instance, keeping a continuously maintained snapshot that the callback only reads),
which is an undo-architecture change and an **Architecture Review Gate** item.

**PREDATES this review series** — it arrived with the P6 §7 bracketing and is not introduced by the
round-40/41 ownership work; it surfaced only because round 41 added the first two-threaded stimulus
to the suite, which is what let ThreadSanitizer's deadlock detector see both orders.

**Workaround:** none required on the hosts tested so far; no off-message-thread restore has been
observed against this plugin (the same standing caveat as KI-003).
**Cause:** taking a lock that guards the whole parameter tree from inside a parameter's own
listener callback.

Evidence [Verified]:
- Source: `src/PluginProcessor.cpp:212` (the M0 → M1 edge); JUCE
  `juce_AudioProcessorValueTreeState.cpp:176` (the M1 → M0 edge)
- Test: `AnabasisStateTests` `testTheFrozenLatchNeedsNoThreadCrossing` provides the two-thread
  stimulus; the finding is the **ThreadSanitizer** `lock-order-inversion` report, not a suite
  failure — the suite passes. Reproduce with a `-fsanitize=thread` build of the state suite.
- Commit: P6 §7 gesture bracketing (pre-existing); observed 2026-08-03 (round 41)

**For the post-v0.1.0 fine review — the highest-severity open item in this family.**

### KI-010 — The forced duck never dry-fills, so ADR-0004's "best masking mode" consequence is unimplemented (2026-08-07)

**Severity:** Low
**Status:** Confirmed
**Affects:** all platforms and formats — every preset load, A/B switch, undo step and discrete
rewire (the §2.8 duck's whole set)

**Workaround:** none needed — the transition is click-free either way; the dip is simply more
audible than a dry-filled one would be.
**Cause:** the dry-fill half of the sibling's duck was never ported; see below.

**What the record claims.** ADR-0004's §Consequences argues that Anabasis's constant-latency
contract makes every bulk swap *dry-fillable*: "A preset step, an A/B switch and an undo step are
therefore **always** dry-fillable and never touch PDC", and "**The forced duck keeps its best
masking mode in the workflow that matters** — the Anamorph gate `predictLatency == latched
latency` is satisfied by construction for every bulk swap". The sibling's duck, when that gate
passes, crossfades against the delay-aligned dry signal instead of dipping to silence.

**What the code does.** `AnabasisEngine::processChunk` applies the duck as a scalar on the
processed path only — `if (! exactlyEqual (duckGain, 1.0f)) processed *= duckGain;` — and no path
anywhere in `src/` substitutes or blends the delay-aligned dry ring during a duck. So every
preset load, A/B switch and undo step dips to **silence** for the duck's ~34 ms, which is the
weaker of the two masking modes the ADR discusses. Confirmed by the 0.1.1 migration audit and
independently re-verified against both trees.

**Why this is recorded and not fixed.** No invariant is violated: `DSP_POLICY` invariant 8
requires transitions to be **click-free**, and a raised-cosine dip to silence is click-free — the
duck tests pin exactly that and pass. What is wrong is that an Accepted ADR's Consequences
section describes a behaviour the tree does not have, which is a documentation-vs-code
contradiction rather than a defect in either alone. Implementing dry-fill is an **audible**
change to every bulk swap on the one path a listening pass has not yet covered, and it landed in
the audit on the day of the 0.1.1 release round. Changing how every preset load sounds, unheard,
to satisfy a sentence in a Consequences section is the wrong trade for a release.

**For the fine review — the decision is which side moves.** Either (a) implement the dry-fill
blend at the duck bottom and keep ADR-0004's text, which needs the delay-aligned dry ring routed
into the duck and its own crossfade shape, plus a listening pass; or (b) amend ADR-0004's
Consequences to say the latency contract makes dry-fill *possible* while the shipped duck dips to
silence, and record dry-fill as a deliberate later option. (b) is the smaller change and is
honest; (a) is what the ADR's author appears to have intended. This entry does not choose.

**Evidence [Verified]:**
- Source: `src/dsp/AnabasisEngine.cpp` (the duck application in `processChunk`); a repo-wide
  search for a dry-fill/blend path in `src/` returns nothing
- Record: `docs/architecture/design-decisions/ADR-0004-latency-contract-constant-lookahead-allowance.md`
  §Consequences
- Test: the duck tests (`testDuckWrapsDiscreteRewires`, `testDuckWrapsOsLatch`,
  `testDuckOnWrapperRequest`, `testAbSwitchRequestsDuck`) pass — they assert click-freeness, which
  is unaffected either way, so nothing in the suite discriminates between the two masking modes

### KI-011 — Ported helpers whose second half was left behind (2026-08-07)

**Severity:** Low
**Status:** Confirmed
**Affects:** (1) all hosts, on every session/preset/A-B restore; (2) all platforms, on a
Peak → RMS compressor detector switch

**Workaround:** none needed for (1) — no observed defect. For (2), leave the detector alone
during a take, or give it a second to settle after switching.
**Cause:** in both cases a sibling helper was ported without one of its rules; see below.

The 0.1.1 migration audit swept the tree for places where a sibling mechanism was copied in part.
Most findings were fixed in that round; these two were confirmed and deliberately left, both
because the fix is a behaviour change rather than a repair:

1. **`reassertFromRaw` is the degraded half of the sibling's `reassertParameters`.** The sibling
   carries two rules this copy dropped: an idempotence guard (`if (std::abs (norm -
   rp->getValue()) > 1.0e-6f)` — parameters already at the target are left untouched) and a
   `notifyHost=false` mode used by `setStateInformation` only. Without the guard, every restore
   notifies the host for all 50 parameters even when none moved; without the split, a session
   load announces parameter changes to a host that is mid-restore. Neither has produced an
   observed defect — pluginval's state-restoration tests pass at the gate strictness — but both
   are host-behaviour changes and belong with the DAW-matrix audition, where a real host can show
   whether the notification storm matters.
2. **`MasteringComp`'s RMS integrator is neither kept warm nor cleared across the Peak/RMS
   detector edge.** `meanSquare[ch]` advances only while `rmsDetector` is true, and the mode is
   assigned per block with no edge handling, so switching Peak → RMS resumes from a mean square
   that is however many blocks old. Every other mode-switched state in the tree is handled. The
   audible consequence is a brief wrong gain immediately after the switch; which of "clear" or
   "keep warm" is right is a listening-pass call, and the switch is duck-routed, so the artefact
   is partly masked already.

### KI-012 — Field report: the Linux editor accepts no mouse input — NOT REPRODUCED on this tree (2026-08-10)

**Severity:** High (if it holds, the plugin cannot be operated at all on the affected setup)
**Status:** Reported — not reproduced; the runtime harness below says the opposite
**Affects:** Linux, plugin format, host and desktop environment not yet recorded

The owner reports that on Linux **no control responds to a click and hovering produces no
visible reaction**, and asks whether the sibling does something here that this editor does not,
since Anamorph shows no such behaviour on Linux.

**Workaround:** none known — and none can be written until the setup is known.
**Cause:** not established. What follows is what the runtime evidence excludes.

This entry breaks constraint C7 (an unconfirmed report is not an entry) for the same reason
KI-009 did before it was closed, and under the same discipline: it carries the *experiments*, so
the next round starts from what has already been ruled out rather than repeating it. KI-009's
outcome is the argument for keeping this entry in that shape — the round that closed it began
from the excluded list rather than re-deriving it (`POSTMORTEMS.md` INC-004).

**The harness.** A real X server (`Xvfb :91`, 1600×1200×24), the built `Anabasis.vst3` loaded
into a purpose-built minimal JUCE 9.0.0 VST3 host (`AudioPluginFormatManager` +
`VST3PluginFormat`, editor in a `DocumentWindow`), synthetic pointer input injected through the
**XTEST** extension — real `ButtonPress`/`MotionNotify` from the server, not JUCE-internal
`handleMouseEvent` calls — and screen state read back with `XGetImage`. Every run was repeated
**without** a window manager and **under `twm`**, so the reparenting frame and the WM's focus
handling are both covered. The oracles are host-side and independent of the plugin's own
reporting: the X window's geometry, the host's view of the parameter values, and a pixel diff of
the plugin window.

**What the harness measured.**

| Probe | Result |
|---|---|
| Standalone: click the ADV toggle at editor (809, 23) | window content height 720 → 822 — the click landed |
| VST3 in the JUCE host, no WM: same click | same resize |
| VST3 in the JUCE host, no WM: rotary drag at editor (300, 120), Δy = −60 | `Loudness` 0.000 → 0.228, with `Comp Ratio`, `Comp Threshold` and `Limiter Gain` following it through the macro map |
| VST3 in the JUCE host **under `twm`**: same drag | identical |
| Pointer parked in the corner, two grabs 1 s apart | **0** pixels changed (correct — no audio is flowing, so the meters are still) |
| Pointer moved onto a knob, grab again | **26 861** pixels changed |

So on this tree, on Linux, through XEmbed, with and without a window manager: clicks land,
drags move parameters, and hover repaints. The reported symptom does not occur here.

**The sibling comparison the owner asked for, in full.** Every interaction-relevant construct is
the same in both editors:

- **OpenGL.** Both exclude the attach on Linux — Anabasis `#if JUCE_MAC || JUCE_WINDOWS`
  (`src/gui/PluginEditor.cpp`), the sibling `#if ! (JUCE_LINUX || JUCE_BSD)` with the
  ADR-0011/INC-006/KI-003 `XEmbedComponent` rationale. Confirmed at *runtime*, not just in the
  preprocessor: the X11 window tree under the host shows the plugin owning exactly **one**
  window and no GL child, so no GL child window can be swallowing the pointer.
- **`TooltipWindow`**, the `setOpaque (true)` on the editor, `applyUiScale()`
  (`setSize` then `setTransform (scale (hostScale × uiScale))`) and the `setScaleFactor`
  override: identical in both, line for line.
- **Build surface.** `EDITOR_WANTS_KEYBOARD_FOCUS FALSE`, `JUCE_WEB_BROWSER=0`,
  `juce_recommended_{config,lto,warning}_flags` and the same `--gc-sections`/`relro`/`now`
  hardening link options — identical in both `CMakeLists.txt`.
- **Overlays.** `dimOverlay` is `setInterceptsMouseClicks (false, false)`; the three `Backdrop`s
  are `addChildComponent` (invisible) and become visible only from an explicit click on the
  wordmark, the Settings button or the preset Save item. Nothing full-frame sits above the
  controls.

The **only** structural divergence found is that the sibling declares its `juce::OpenGLContext`
member on every platform and gates only `attachTo`, while this editor compiles the member out on
Linux entirely. That is not on the input path — and the runtime window tree above proves it is
not, since neither build creates a GL window on Linux.

**What is still open, and what would settle it.** The fault is real for the reporter and absent
in every harness here, so the difference is in the environment, not (on this evidence) in the
component tree, the hit-testing, the overlay z-order or the GL gate. To progress, record: the
**host and version**, the **desktop environment / window manager and whether a compositor is
running**, whether the build is a CI artifact or a local one, and — the single most
discriminating datum — **whether the meters move while audio plays**. Moving meters with dead
controls is an *input-routing* fault; frozen meters with dead controls is a *repaint/event-loop*
fault (on Linux JUCE drives `dispatchDeferredRepaints` from the same vblank timer that feeds
`VBlankAttachment`, so both symptoms share one carrier), and the two lead to opposite places.

**Addendum, 2026-08-16 (the JUCE 9.0.1 bump — ADR-0028). Three candidate mechanisms that were
live at 9.0.0 are closed at 9.0.1, one on each branch of the paragraph above. This entry stays
OPEN and its status is unchanged**: the report still does not reproduce here, this is not the
reporter's machine, and nothing below was *tested* against the reported configuration — it is
read from the upstream diff between the two pinned trees. It is recorded because the next round
should not re-derive it, and because if the report recurs at 9.0.1 these three are already
excluded.

*On the input-routing branch:*

- **JUCE dlopened `libXi.so`, not `libXi.so.6`** (`juce_XSymbols_linux.h:655` at `f8f8864…`).
  Every other X11 helper it loads is a SONAME — `libX11.so.6`, `libXext.so.6`,
  `libXcursor.so.1`, `libXinerama.so.1`, `libXrender.so.1`, `libXrandr.so.2` — and XInput alone
  asked for the *unversioned* name, which is the symlink `libxi-dev` installs. A developer machine
  has it (this container does, via the `scripts/setup-linux.sh` X11 dev packages); **an end user's
  machine has `libxi6` and no symlink.** That first step is MEASURED rather than assumed — with
  `/usr/lib/x86_64-linux-gnu/libXi.so` (a symlink to `libXi.so.6.1.0`) temporarily moved aside,
  `dlopen ("libXi.so", RTLD_LOCAL | RTLD_NOW)` returns null with *"cannot open shared object file"*
  while `dlopen ("libXi.so.6")` still succeeds; with the symlink in place both open. The failure is
  silent and total rather than loud:
  `X11Symbols`' stub for a missing symbol returns a default-constructed value, `Status` is `int`,
  and `Success` is `0` (`/usr/include/X11/X.h:350`) — so `setupXI2`'s
  `xiQueryVersion (…) != Success` test *passes* against the stub, `major`/`minor` keep the `2, 2`
  they were initialised with, `XQueryExtension` succeeds because the **server** has XInput2 even
  when the client library is missing, and JUCE proceeds as though XI2 were live. Then
  `registerForXI2Events` calls the `xiQueryDevice` stub, gets `nullptr` with `numDevices` still 0,
  iterates nothing, and **selects no XI2 event mask at all**. Dead pointer, live window, no
  diagnostic. 9.0.1 loads `libXi.so.6`, and makes the `XIQueryVersion` stub return `BadRequest` so
  the same absence would now fail closed; it also null-checks `xiQueryDevice`.

*On the repaint/event-loop branch:*

- **The Linux vblank timer compared milliseconds against hertz.**
  `juce_Windowing_linux.cpp` guarded its restart with
  `if (vBlankManager.getTimerInterval() != frequencyToUse) vBlankManager.startTimerHz (frequencyToUse)`
  — `getTimerInterval()` returns a **period in ms**, `frequencyToUse` is a **rate in Hz**, so for
  any display the two never agree (60 Hz → interval 16) and every call re-`start`s the timer,
  resetting its countdown. 9.0.1 converts to a period first and compares like with like. This is
  the same timer that drives `dispatchDeferredRepaints`, which is why it lands on this branch and
  not the other.
- **The message queue could starve the X event pump.** `juce_Messaging_linux.cpp`'s fd callback
  drained `popNextMessage` in an unbounded loop; 9.0.1 breaks out after 100 ms, with the comment
  "Avoid starving other LinuxEventLoop callbacks such as the XWindowSystem". Upstream's
  `CHANGE_LIST.md` files this under "Fixed unresponsive Linux GUIs".

*Also on the display-geometry path, and relevant because it feeds the timer above:*
`findDisplays` skipped the whole XRandR branch unless `_NET_WORKAREA` existed, so a bare X session
or a WM that sets no work area got no per-monitor refresh rate — hence the 100 Hz fallback, hence
the timer restart. 9.0.1 consults XRandR regardless and guards a division by a zero
`hTotal`/`vTotal`.

**What would now settle it, and it is unchanged in kind.** The same single observation — do the
meters move while audio plays — plus one new one that is cheap for the reporter and decisive
about the first mechanism: `ldconfig -p | grep libXi`. If that machine has `libXi.so.6` but no
bare `libXi.so`, the 9.0.0 build could not have received a pointer event and 0.1.5 should behave
differently for exactly that reason.

Evidence [Partially Verified]:
- Source: `src/gui/PluginEditor.cpp` (GL gate, overlays, `applyUiScale`), `CMakeLists.txt`
- Test:   none — the report does not reproduce; the runtime harness above is recorded in
  `worklogs/`, not in the suites, because a passing probe of a fault that never appears would
  pin nothing
- Commit: this one

### KI-013 — The click absorbed by the pop-up shield still counts toward the multi-click run (2026-08-13)

**Severity:** Low
**Status:** Confirmed
**Affects:** All platforms and formats; any control with a double-click action — in practice the
knobs, whose double-click resets to default.

Dismissing a pop-up by clicking outside it no longer operates the control underneath (the shield
absorbs that press). What the shield cannot do is *un-count* it. JUCE tracks the multi-click run on
the mouse source rather than on the component that received the press, so the absorbed click still
advances the run: click a knob to open something, click away to dismiss, then click that knob again
within the double-click interval and the second press can arrive as a double-click and reset the
knob to its default.

Two things keep this narrow. The absorbed press and the following one must land inside the system
double-click interval, and the reset is a normal undoable step, so the value comes straight back
with Undo.

**Workaround:** press Undo; or leave a moment between dismissing a pop-up and clicking a knob.
**Cause:** the multi-click counter lives on `juce::MouseInputSource` and is advanced when the event
is delivered, before any component decides what to do with it. A component that consumes an event
does not decrement it, and JUCE exposes no way to reset the run.

Evidence [Partially Verified]:
- Source: `src/gui/PluginEditor.h` (`PopupShield`), `src/gui/PluginEditor.cpp`
  (`refreshPopupShield`)
- Test:   none — not reproducible headlessly; it needs real double-click timing from a pointer
  device, which the suites do not synthesise
- Commit: this one

### KI-014 — macOS: a held letter or digit does not repeat in the Save Preset name field (2026-08-13)

**Severity:** Low
**Status:** Confirmed (platform behaviour, not a defect in this plug-in)
**Affects:** macOS only, all formats; the Save Preset name field. Punctuation and symbol keys
repeat normally, letters and digits do not.

Holding a letter or a digit in the Save Preset name field types one character and then stops.
Holding a punctuation key repeats as expected. Typing normally is unaffected, so the field is fully
usable — only auto-repeat is missing for those keys.

**Workaround:** none needed; type the character repeatedly rather than holding it.
**Cause:** macOS press-and-hold. For letter and digit keys the system suppresses key repeat in
favour of the accent/character picker, and delivers no repeat events for the framework to forward.
The plug-in never sees the repeats, so there is nothing here to fix. Fixes were considered and
rejected: suppressing the system behaviour requires a global preference this plug-in has no
business writing, and synthesising repeats from a timer would fire where the system deliberately
does not and would diverge from every other text field on the machine. Recorded so the next
investigation does not re-derive it.

Evidence [Partially Verified]:
- Source: `src/gui/PluginEditor.h` (`saveNameEditor`)
- Test:   none — platform input behaviour, not reachable from the suites
- Commit: this one

### KI-015 — `ScopeBuffer`'s payload is read and written non-atomically, as `GrHistoryBuffer`'s was until 0.2.8 (2026-09-02) — **CLOSED 2026-09-02**

> **✅ CLOSED — REPAIRED, not downgraded.** A focused adversarial review of this entry (six
> independent lenses, three adversarial stances and a completeness critic) confirmed a real data
> race and rejected both "close it as race-free" and "defer it to a larger redesign": the fix needs
> no architectural change, is confined to `src/dsp/ScopeBuffer.h`, and leaves `SpectrumView`
> untouched. The payload element is now a `Sample` wrapper over one relaxed `std::atomic<float>`
> (ADR-0011's third dated 2026-09-02 amendment, **raised at the Architecture Review Gate and held
> for the owner's ruling**, since it supersedes an accepted sentence). Pinned by `specSync` in the
> DSP suite.
>
> **THREE STATEMENTS IN THE ORIGINAL ENTRY BELOW ARE WRONG, and the corrections outlived the fix:**
>
> 1. *"`ScopeBuffer` has no reset epoch to bracket a batch with"* — **half false, and the true half
>    is load-bearing.** `resetGeneration()` exists with a documented before/after contract and
>    `SpectrumView::tick` brackets its batch with it on both sides. What `ScopeBuffer` does not have
>    is an odd/even epoch around a payload-writing clear — and that distinction is exactly why this
>    ring needs NO reader-side acquire fence where `GrHistoryBuffer` did: its `reset()` writes one
>    atomic and touches no sample.
> 2. *"its bulk `memcpy` needs its own design pass rather than a transliteration"* — **overstated.**
>    Only the PRODUCER half is a `memcpy`; the racing reader half was already a per-element scalar
>    loop. The repair turns four `memcpy`s into two store loops over the same segment arithmetic.
> 3. *"the headroom is 4096 of 16384, so ~12288 frames (~0.26 s at 48 kHz)"* — **not an invariant,
>    and this was the entry's whole severity argument.** It is a FRAME count, not a time (0.064 s at
>    192 kHz); `reset()` rewinds the head, leaving a reader that holds a pre-reset index a margin
>    anywhere in [0, capacity); and `maxBlock` is the host's `samplesPerBlock` with no upper clamp,
>    so a single push reaches the reader's oldest slot at n ≥ 12289 and covers its whole window at
>    n ≥ 16384 — two different thresholds, both previously unstated, neither needing a reader stall.
>    Stated the other way round for honesty: that is **possible by construction and unexercised
>    here** — no in-tree stimulus prepares more than 512 frames, and what would settle it is a survey
>    of host offline-bounce buffer maxima, which this tree cannot answer.
>
> The severity assessment was right — nothing user-visible ever depended on it, and the worst
> outcome after the repair is one FFT frame mixing old and new audio, decaying on the analyser's
> ~120 ms EMA. What the entry got wrong was treating an unquantified margin as a reason the defect
> was not a defect. The text below is left as written, per the append-only convention.

**Severity:** Low
**Status:** **CLOSED 2026-09-02 — repaired.** (Originally: Confirmed from the code; not reproduced, and not expected to be reproducible)
**Affects:** every platform and format; the two spectrum capture points only — the input/output
spectrum overlay. The GR history is **not** affected: the same defect was repaired there in 0.2.8.

Nothing is visible to a user. This is a correctness entry, filed so the next round starts from a
fact rather than rediscovering it: `ScopeBuffer` publishes its frames with the same
release/acquire index `GrHistoryBuffer` uses, but its payload is `memcpy`'d into
`std::vector<float>` by the producer and copied out by the reader with plain accesses. If the audio
thread laps a slow reader mid-copy, those accesses race, and a plain read concurrent with a plain
write is undefined behaviour under the C++ memory model however benign the machine code looks.

**Workaround:** none needed; nothing user-visible depends on it.
**Cause and why it is not fixed here.** It is materially safer than the GR-history case was, and
the headroom is the reason: the only caller asks for **4096 of 16384 frames**, so the producer must
advance ~12288 frames — about **0.26 s at 48 kHz** — between the reader's index acquire and the end
of its copy for the oldest frames to be overwritten. `GrHistoryBuffer` had **one slot** of margin by
construction, which is why it was the blocker and this is not. The repair is the same shape (atomic
payload, relaxed both ways, the audio-thread store unchanged — measured instruction-identical
there), but `ScopeBuffer` copies in bulk with `memcpy`, so it needs its own design pass rather than
a transliteration of the GR fix, and the 0.2.8 review scope explicitly excluded it. Recorded as a
separate follow-up.

**Intentionally excluded from the 0.2.8 review pull request (PR #27), on instruction, in every one
of its rounds** — including the final one, whose ring change (the prepared pair stored inside the
clear, the reader's acquire-fence close, `batchIntact`) is a `GrHistoryBuffer` repair that does not
transfer either: `ScopeBuffer` has no reset epoch to bracket a batch with and no pair to publish,
and its bulk `memcpy` is the thing a per-element atomic payload cannot express without the design
pass above. The race class, for the record: **producer overwrites reader, plain payload** — the same
class the GR ring had, with ~0.26 s of reader headroom where the GR ring had one slot. That headroom
is why the GR fix was a blocker and this is a follow-up, and it is not a proof: a suspended message
thread (debugger, a host batching redraws) spends it in one stop.

Evidence [Verified]:
- Source: `src/dsp/ScopeBuffer.h` (`pushBlock`'s `memcpy` pair; the reader's copy-out), against
  `src/dsp/GrHistoryBuffer.h`'s repaired `Slot`
- Test:   none — a race no deterministic suite can stage; the GR-history equivalent is pinned by
  the type-level `grSync` assertions, which have no `ScopeBuffer` counterpart yet
- Worklog: `worklogs/2026-09-01-gr-history-scroll-jitter.md` §10, §11

### KI-016 — Anamorph's `ScopeBuffer` carries the defect KI-015 repaired here, and that repository is read-only (2026-09-02)

**Severity:** Informational (about the SIBLING product, not this one)
**Status:** Confirmed from the sibling's source; deliberately not repaired
**Affects:** Anamorph only. Anabasis is unaffected — KI-015 repaired the same shape here.

`Anamorph:src/dsp/ScopeBuffer.h` is the file ADR-0009 records this ring as copied from, and it still
carries the producer `memcpy` / reader plain-subscript pair that KI-015 shows to be a data race when
the producer laps the reader. The sibling's `docs/KNOWN_ISSUES.md` has no ScopeBuffer entry, so the
defect ships there unrecorded.

**Why nothing is done about it from here, and why this entry exists anyway.** ADR-0009 item 8 makes
divergence between the two products **accepted and one-way** — no upstream-sync obligation, no
backport path, drift fixed per product — and `CLAUDE.md` §3 makes Anamorph read-only from this
repository. So no sibling change is owed and none is proposed. What ADR-0009's Consequences do
require is that an SPSC-ring improvement be made in both products *or accepted as drift*, and
"accepted drift" is only meaningful against a named instance. This is the name. Whether to schedule
the sibling repair is a product-family decision for the owner, taken in Anamorph's own tree.

Evidence [Verified]:
- Source: `Anamorph:src/dsp/ScopeBuffer.h` (the `memcpy` producer and the plain-subscript reader),
  against this repository's repaired `src/dsp/ScopeBuffer.h`
- Related: ADR-0011's third dated 2026-09-02 amendment; ADR-0009 item 8; KI-015

### KI-017 — The paint path reads JUCE's plain prepared sample rate, in two views (2026-09-02) — **CLOSED 2026-09-02 (round 6)**

> **✅ CLOSED — REPAIRED.** A focused audit confirmed a genuine data race with the HOST thread and
> fixed it: both views now read the pair `GrHistoryBuffer` already publishes, through
> `AnabasisAudioProcessor::preparedSampleRate()`. **Three statements in the entry below were wrong,
> and the corrections are the part worth keeping:**
>
> 1. **"in two views" — there are THREE plain reads, and the third is not fixed here.**
>    `SpectrumView::paint` and `CurveView::readInputs` are repaired; `PluginEditor`'s
>    `getTotalNumOutputChannels()` in its timer callback reads `cachedTotalOuts`, a different plain
>    member with no published equivalent anywhere in the tree (checked: there is no channel-count
>    publication). Repairing it needs a new publication rather than a redirect, so it is deliberately
>    NOT bundled — see the remaining-work note at the end of this entry.
> 2. **"macOS and Windows only, where the OpenGL context attaches" — wrong, and this is why the
>    MessageManager finding below does not rescue it.** `CurveView::readInputs` is reached from the
>    editor's 24 Hz `timerCallback` as well as from `paint`, so the message thread reads it too — and
>    the opposing writer is the HOST's reconfiguration thread, which takes no MessageManager lock in
>    any wrapper (VST3's `preparePlugin` is reached from `setupProcessing` and `setActive` with none;
>    VST2, AU, AUv3, AAX and LV2 are the same shape). The defect was live on **every** platform,
>    Linux included, where no GL context attaches at all.
> 3. **The MessageManager-lock note below is confirmed at the pinned source and is irrelevant to this
>    entry's question.** It excludes `paint` from racing MESSAGE-thread work. It says nothing about
>    the HOST thread, which is the writer here. It is kept because it remains true and because it
>    narrows ADR-0027's and ADR-0038's stated premise — but it was never a reason to leave this open.
>
> **What the repair trades, stated rather than claimed as behaviourally identical.**
> `prepareToPlay` prepares the engine before it prepares the GR ring, so inside that window the
> published pair still carries the previous rate. For `SpectrumView` this is provably invisible: the
> spectrum ring has been rewound, `readLatest` yields nothing and `analyse` returns before touching
> the trace, so the old rate maps an already-floored display. For `CurveView` there is no ring and no
> floor: it can draw the EQ response at the previous rate for the duration of `prepareToPlay`, where
> before it drew at the new one. That is a **bounded correct-but-late frame traded for undefined
> behaviour**, which is the right trade and is not a claim of identical behaviour.
>
> Pinned by `ki017` in the state suite: with a bare `prepareToPlay` JUCE's plain member stays 0, so
> the old source mapped every headless frame through its 48 kHz fallback and two snapshots taken at
> 96 kHz and 48 kHz were IDENTICAL; they differ only when the view reads the published pair. The
> revert mutant fails exactly that assertion.

**Severity:** Low
**Status:** **CLOSED 2026-09-02 — repaired for the two rate reads; the channel-count read remains
open (below).** (Originally: Confirmed from the code; not repaired.)
**Affects:** every platform — see correction 2. Display only.

Found while reviewing KI-015, in the same file, and deliberately left alone: `SpectrumView::paint`
takes `processor.getSampleRate()` to map its frequency axis, and `CurveView` does the same for its
own. That is `juce::AudioProcessor`'s plain `currentSampleRate` — written by
`setRateAndBufferSizeDetails` on whichever thread the host reconfigures on — read from the painting
thread. It is the **identical defect class** ADR-0011's second 2026-09-02 amendment repaired for
`GrHistoryView`, by making the prepared pair ring metadata published inside the ring's clear. Two
instances remain; neither is in KI-015's subject, and bundling them into a payload-atomicity repair
would have been scope creep. The visible cost if it ever bit would be one frame's axis mapped
through a half-updated configuration, on a re-prepare that already blanks the display.

**A finding that bears on how this is judged, recorded because it is not written anywhere in the
tree.** ADR-0027 and ADR-0038 both rest on the premise that a plain read from `paint` "is a data
race … on exactly the two platforms where the context attaches". In the pinned JUCE 9.0.1 the GL
render thread takes the **MessageManager lock** around component painting —
`juce_OpenGLContext.cpp` emplaces the scoped `mmLock` before `paintComponent` and releases it
after — so a component's `paint` cannot in fact run concurrently with message-thread work, and that
mutual exclusion is a happens-before edge neither ADR considers. Neither ADR's DECISION is disturbed:
their atomics are correct, cost nothing, and remain the right shape for state whose writer is not
the message thread. What is narrower than written is the stated JUSTIFICATION, for the
component-paint path specifically, and it is an implementation detail of one JUCE version rather
than an API guarantee — which is a reason to keep the atomics, not to remove them. Recorded here so
the next round starts from the source rather than from the premise. **No ADR text is changed on the
strength of this entry**; that is an owner call.

**THE THIRD READ IS ALSO CLOSED NOW (round 7).** `PluginEditor`'s timer callback read
`proc.getTotalNumOutputChannels()` — `juce::AudioProcessor::cachedTotalOuts`, written by
`AudioProcessor::audioIOChanged` on whichever thread the host reconfigures on, with no lock in any
wrapper. The audit could name no mechanism that serialises it against the editor's 24 Hz timer, so
option B was unavailable: a genuine data race, not a benign transient.

It could not be redirected the way the two rate reads were — the prepared pair carries rate and
block only, and inferring mono from a per-channel meter reading zero is unsound because a stereo
channel at rest reads zero too. So this one did need a publication, and the justification is not
merely "the field is plain": the editor's GR lanes read the engine's PER-CHANNEL atomics and must
draw the geometry those atomics were filled under, whereas JUCE's accessor answers a different
question — the layout the host may be moving TO. `pubOutChannels` is one relaxed `int`, stored at
construction, from `numChannelsChanged` (which JUCE calls from `audioIOChanged`, i.e. **on the
writer's own thread**, so a layout change with no re-prepare is covered), and from `prepareToPlay`.
It sits on `THREADING_POLICY`'s existing Meters → GUI row, whose writer set already includes
`prepareToPlay` on the host thread — not a new cross-thread path, and not a gate item. Pinned by
`ki017c`.

**THE prepareToPlay PUBLICATION LAG, audited in the same round and ACCEPTED as a bounded
transitional state.** `AnabasisEngine::prepare` rewinds both spectrum rings at the TOP of its body,
and the prepared pair is republished only after it returns — so the window is nearly all of
`engine.prepare` (milliseconds, the eight oversampler constructions), not the gap between two
adjacent statements. The rewind happens on EVERY prepare, because `engine.prepare` is called
unconditionally; only the pair's republication is gated on the pair actually changing.
**The order is load-bearing and must not be "tidied".** Publishing the pair first would put a full
ring of old-rate audio opposite the NEW rate for that whole window — the wrong-frequency artefact the
rewind exists to remove, produced on every vblank rather than as a skew. The current order puts an
EMPTY ring opposite the OLD rate, and every state a reader can reach in the window is
self-consistent. Per reader: `GrHistoryView` has no exposure at all — it reads the pair from inside
the same epoch bracket as the entries, so the two move together by construction; `SpectrumView`
reads an empty ring and (since round 7) floors its trace; `CurveView` can draw the EQ response at the
previous rate for the duration of `engine.prepare`, which is a bounded correct-but-late frame. The
invariant, stated for the next reader: **a GR frame never maps one configuration's entries through
another's time base, and the price is that non-ring readers may lag by one reconfiguration.**

Evidence [Verified]:
- Source: `src/gui/SpectrumView.cpp` (`paint`, the axis mapping) and `src/gui/CurveView.cpp`
  (`readInputs`, reached from `paint` AND the editor's timer); `src/gui/PluginEditor.cpp` (the
  channel-count read that remains); `juce_audio_processors`' plain `currentSampleRate`/`blockSize`
  members
- Precedent: ADR-0011's second dated 2026-09-02 amendment (the repaired instance in `GrHistoryView`)
- JUCE: `juce_opengl/opengl/juce_OpenGLContext.cpp` (the MessageManager lock around
  `paintComponent`), read at the pinned 9.0.1
- Worklog: `worklogs/2026-09-02-ki015-scopebuffer-payload.md`

### KI-018 — A spectrum reset can leave the previous trace on screen for one or more ticks (2026-09-02) — **REPAIRED except for one corner, round 7**

> **⟳ RETAINED AND NARROWED, not closed.** Round 7 repaired the case this entry describes and, in
> doing so, found that **both halves of the bound written below are wrong**. The corrections matter
> more than the patch:
>
> * **The window is not "one atomic's visibility latency, sub-microsecond in practice".** The
>   dominant case is an ordinary INTERLEAVING, not a visibility one: the host thread is preemptible
>   between `reset()`'s two stores, and every reader tick inside that window saw the rewind without
>   the announcement. Worst case is therefore a scheduling quantum — tens of milliseconds under load
>   — not a store drain. (The typical case does remain store visibility; it is the worst case that
>   was mis-stated.) And `[atomics.order]`'s "reasonable amount of time" is a *should*, so even the
>   typical bound is "finite, not normatively guaranteed".
> * **The artefact was SMALLER than stated, in the other direction.** "Drawn against the new rate's
>   bin mapping" is true only after the prepared pair republishes, which happens later still (see
>   the prepareToPlay note in KI-017) and only when the rate actually changed. Throughout the
>   interleaving window the pair the view reads is the OLD one, so the held trace is drawn at the
>   rate it was captured at — self-consistent, and bit-for-bit the frozen-analyser behaviour KI-007
>   item 6 already ships deliberately.
> * **"The repairs are larger than the defect" was true of the two repairs this entry considered and
>   false of the one it never considered.** No synchronisation change was needed. The reader already
>   loads BOTH facets every tick — `resetGeneration()` and `writeCount()` — and simply keyed its
>   decision on one of them. `SpectrumView::resetObserved` now takes both, and `analyse` floors the
>   trace when `readLatest` hands back zero frames (which is equivalent to "the index I acquired was
>   0", reachable only from construction or a reset). Message-thread only: no new atomic, no new
>   ordering, nothing on the audio path, `ScopeBuffer` untouched.
> * **The packed-word recommendation below is withdrawn.** Packing (generation, index) into one
>   64-bit atomic forces the frame counter below 64 bits; at 32 it wraps in ~25 hours at 48 kHz and
>   would then FABRICATE a reset, and `writeCount()`'s monotone `uint64_t` total is a published
>   contract with tests on it. A 128-bit atomic is not reliably lock-free and would be stored by
>   `pushBlock` on the audio thread. If the corner below is ever taken, it needs a fresh design pass,
>   not this entry's suggestion.
> * **A trap, named so nobody tries it:** swapping `reset()`'s two stores so the generation is
>   bumped first INVERTS the skew and destroys the invariant that already held — a reader acquiring
>   the new generation could then read a pre-reset index.
>
> **WHAT REMAINS, and it is why this entry is retained.** One corner survives: a reset whose refill
> reaches EXACTLY the previously observed count while the generation bump is still invisible. Then
> the count term is silent (equal, not lower), the generation term is silent, the idle test matches,
> and the tick does nothing. It is the equal-count case in a new place — the same shape 0.2.7 and
> round 2 each met — and no test pins it, because it cannot be staged deterministically.

**Severity:** Low
**Status:** **Repaired round 7 except the equal-count corner above; retained and narrowed.**
(Originally: investigated and proven bounded, deliberately not repaired.)
**Affects:** every platform; the spectrum overlay only.

Raised as a review finding that a reset overlapping `readLatest` lets "the previous spectrum survive
until a later tick notices". Audited against the C++ memory model, and the finding is real as a
DISPLAY residual and false as a correctness violation. Both halves matter, so both are recorded.

**What is guaranteed, by construction.** `ScopeBuffer::reset()` stores the rewound index with
`release` and THEN bumps the generation with `release`. A reader whose acquire load of the generation
returns the new value therefore has happens-before to the rewind, and write-read coherence forces
every subsequently sequenced load of the index — including the one inside `readLatest` — to return a
post-reset value. "New generation, stale index" is impossible. Combined with `SpectrumView::tick`
only ever advancing `shownInGen` in a tick that floored the EMA before `analyse` (`resetIn`) or after
it (`gi1 != gi0`), **a pre-reset spectrum can never be committed as a post-reset one**. That is the
invariant the review asked for, and it already held.

**What is not guaranteed, and this is the real residual.** The reverse skew is permitted: a reader
can observe the rewound INDEX while its generation load still returns the old value, because they are
two atomics and only one direction is ordered. Then `resetIn` is false, `readLatest` yields nothing
(the index is 0), `analyse` returns early **without touching the EMA**, and the previous spectrum is
drawn again — verbatim, against the new rate's bin mapping. The tick can even satisfy the idle test
outright and do nothing at all. Nothing decays it, because the EMA's decay only runs when frames
arrive and a re-prepare normally happens with the transport stopped. It ends when the generation bump
becomes visible to the reader's acquire load — and there is no second, independent correction path,
because the frame COUNT was deliberately retired as a reset detector in 0.2.7.

**Why it is not repaired here.** The bound is one atomic's visibility latency, which the standard
requires to be finite and which is sub-microsecond in practice; the artefact is a stale display, not
wrong audio and not undefined behaviour (the payload has been atomic since KI-015). The repairs that
would remove the skew entirely are real but larger than the defect: folding the generation and the
index into ONE atomic word (correct, and the strongest option — it makes the skew unrepresentable
rather than detectable, at the cost of changing the ring's published word layout, `readLatest`'s
signature and every caller), or bracketing the rewind in an odd/even seqlock like
`GrHistoryBuffer::clear`'s. Both are a design change to a ring that is not currently wrong, so they
belong to a round that takes them deliberately rather than to a defect report. **The packed-word
option is the recommended one if this is ever taken.**

**Workaround:** none needed; nothing audible and nothing persistent depends on it.

Evidence [Verified]:
- Source: `src/dsp/ScopeBuffer.h` (`reset`, `readLatest`), `src/gui/SpectrumView.cpp` (`tick`,
  `analyse`'s early return) — and the two comments in those files that claimed more than the code
  delivers were corrected in the same round rather than left standing
- Related: ADR-0011's first and third dated 2026-09-02 amendments; KI-015
- Worklog: `worklogs/2026-09-02-round6-concurrency.md`

## Standing note for P1 onward

Two categories are known in advance to need entries in this project, from the sibling product's
experience. They are named here as **expectations**, not as claims that they already occur:

- **Anything that cannot be validated headlessly** — audio quality, GUI appearance, real-DAW host
  behaviour — is a documented coverage limitation, not an unknown. It belongs here the moment it
  blocks something concrete.
- **Host-specific behaviour** (parameter display, automation recording, editor resize, plugin
  rescan) is where most confirmed issues in a JUCE plugin end up. Record the exact host and
  version; "some DAWs" is not an entry.
