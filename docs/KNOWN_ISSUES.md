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

*(KI-001 — unducked discrete transitions — and KI-002 — inert Loudness Comp/Delta — are FIXED
and recorded as `POSTMORTEMS.md` INC-001/INC-002; their numbers are never reused.)*

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
| M0 → M1 | `AnabasisAudioProcessor::audioProcessorParameterChangeGestureBegin` (`src/PluginProcessor.cpp:122`) takes the §7 pre-state with `saveSlotFromLive()` → `copyStateWithRaw()` → `apvts.copyState()`, from **inside** the listener callback that already holds M0. |
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
decision. That is the interleaving KI-003 is about, and the §5.3 machinery exists *because*
gestures and parameter writes on the same managed parameter do overlap across threads.

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
- Source: `src/PluginProcessor.cpp:122` (the M0 → M1 edge); JUCE
  `juce_AudioProcessorValueTreeState.cpp:176` (the M1 → M0 edge)
- Test: `AnabasisStateTests` `testTheFrozenLatchNeedsNoThreadCrossing` provides the two-thread
  stimulus; the finding is the **ThreadSanitizer** `lock-order-inversion` report, not a suite
  failure — the suite passes. Reproduce with a `-fsanitize=thread` build of the state suite.
- Commit: P6 §7 gesture bracketing (pre-existing); observed 2026-08-03 (round 41)

**For the post-v0.1.0 fine review — the highest-severity open item in this family.**

### KI-009 — Field report: left channel silent — macOS-only, NOT REPRODUCIBLE on Linux (2026-08-05)

**The report.** The owner reports the LEFT channel carrying no audio at all, right channel
normal, "in both Simple and Advanced modes" (i.e. always). Host, OS and build not yet recorded.

**What was probed, and found healthy.** `testBothChannelsCarryAudioThroughTheWrapper`
(`tests/state_tests.cpp`) drives the REAL `AnabasisAudioProcessor::processBlock` — not the bare
engine — with per-channel-distinct sines (220/330 Hz, so a swap or a sum-into-one-channel cannot
masquerade as health) and measures settled per-channel RMS in **six configurations**: defaults ·
loudness 50 · the editor alive across the whole processing loop · 16× linear-phase oversampling ·
a factory preset applied · after a session save/load round-trip. (This paragraph said "seven"
against its own six-item list for three commits — the list is the count, and the list is what the
suite runs.) Both channels carry audio within
6 dB of each other in every one. The DSP suite separately covers the engine's stereo path, and
pluginval (both modes ×3, editor open) passes on the built VST3.

**What that excludes, and what it cannot.** Every channel loop in the engine and wrapper is
symmetric (`processChunk`'s five stages all iterate `[0, nCh)`; the wrapper adds no per-channel
path), and the parameter surface contains **no per-channel gain, pan or balance** — there is no
parameter whose value could mute one side. What the headless battery cannot reach: JUCE's
VST3/AU/Standalone format glue as driven by a *particular* host, the standalone's audio-device
channel mapping, and the possibility that the observation was a host meter reading rather than
the plugin's output. Those are exactly this file's standing-note categories.

**Status: OPEN — needs the environment.** To act on this, record: host + version, OS, which
build (CI zip? local?), whether the standalone or a plugin format, and whether the left silence
is heard or read off a meter. The six-configuration battery stays in the suite either way — it
is now the permanent guard on every headless-reachable stereo path, and whichever configuration
eventually reproduces the report becomes its seventh case.

**0.1.1 addendum — the input-side mechanism, closed.** The report persisted past the round-2
probe, so 0.1.1 re-ran the investigation as a line-by-line wrapper/bus/CMake diff against the
sibling plus a full re-audit of every channel-asymmetric expression in the engine and
`LookaheadLimiter`. Finding: the audible path is provably channel-symmetric (every `ch == 0`
asymmetry is an analysis tap), and no Anabasis source line can silence one channel — but the
**bus contract** could. Anabasis refused the mono→stereo layout the sibling accepts
(`isBusesLayoutSupported` demanded stereo→stereo exactly), so a host with a mono source — or a
mono input device under the standalone — was forced to negotiate stereo→stereo and feed whatever
its convention puts on the two input pins. Several conventions put the programme on ONE pin and
silence on the other, and this chain is strictly dual-mono (the comp/limiter stereo "link"
shares only the detector level; nothing cross-feeds), so a silent input pin propagated to a
silent output channel in both modes — exactly the report, and unreachable by the battery, which
always fed both channels. Fix: `isBusesLayoutSupported` now accepts mono→stereo (the sibling's
contract), and `processBlock` duplicates the mono input into channel 1 before the engine runs.
The battery gained its predicted seventh case (`mono in`, programme on channel 0 only, both
outputs asserted live). If the field setup was instead a silent left channel *delivered by the
routing upstream* (the other mechanism the audit ranks plausible), the plugin now also survives
the mono half of that: the host can drop to the mono layout instead of feeding a dead pin.

**0.1.2 addendum — the report persists, and the Delta observation narrows it decisively.** The
owner reports the left channel still silent (stereo input, both editor modes), with a new datum:
**engaging Delta makes the left channel audible while the right carries a GR-flavoured
residue.** That observation is algebraically decisive, because the delta leg is computed
per channel *inside the engine* — `wetLeg = dryForDelta − processed`
(`src/dsp/AnabasisEngine.cpp`, stage E), with `dryForDelta` read from the dry ring the engine
fills from its own input. Delta-L being audible therefore proves `dry_L ≠ 0` — **the host is
delivering programme on the left input pin** — and normal-mode silence then requires
`processed_L ≈ 0`: the kill is inside the processed leg, between the stage-A dry-ring write and
the stage-E output. This *rules out* the 0.1.1 dead-pin mechanism for the current report (a dead
pin gives Delta-L = 0 − 0 = 0, inaudible).

Two independent 0.1.2 audits (the wrapper/engine trace and the monitor-path map) then failed to
find any expression on that leg able to hold exactly one channel at zero while the other plays:
every modulator is channel-shared (duck, bypass/delta mixes, monitor gain, smoothers, ring
positions), every per-channel gain is strictly positive (comp `10^(grDb/20)`, limiter
`ceiling/peak`), and every recursive state self-heals within the block (invariant 9; the one
uncovered per-channel state — the dither error-feedback pair — is now swept too, though a fault
there was unreachable). The fingerprint is **not expressible in the current source with intact
state**, which narrows the field cause to: a session holding limiter link < 100 % with divergent
per-channel envelope state, a stale or mismatched installed binary, host-side channel handling,
or in-process memory corruption (the KI-003/KI-008 windows remain the recorded vectors).

What 0.1.2 ships for it, mechanism by mechanism: **(a)** the GR-at-defaults confound is gone
(ADR-0023 — knee above threshold, unfiltered limiter detector, comp detector clamp), so on
sub-ceiling material Delta at defaults is now *exact digital silence* and the "GR-flavoured
residue on the right" observation should disappear with it — if it does not, what remains in
Delta **is** the anomaly, isolated; **(b)** the panel GR meters are per-channel
(`meterLimGrDbCh`/`meterCompGrDbCh`), so one field glance now disambiguates "L silent with its
GR lane pinned deep" (per-channel gain collapse — record the session's link values) from "L
silent at zero GR" (not the dynamics stages); **(c)** the battery gained the six diagnostic
configurations the report was observed under and never covered headlessly — Delta engaged
(channels asserted within 6 dB), loudness-comp on, limiter link 0 %, dither 16-bit shaped,
true peak on, 44.1 kHz — all green; **(d)** mono→mono closes the last layout-negotiation
surface (ADR-0023 item 5). **Still needed from the field, unchanged since round 2:** host +
version, OS, build provenance, format, the session file, and now the two GR lanes' readings
while the silence is audible.

> **Superseded by the 0.1.3 FOLLOW-UP addendum below (same day).** The oversampler hypothesis
> this addendum ends on was tested by the owner and came back NEGATIVE — the behaviour is
> independent of the oversampling factor. The localisation it establishes (the kill is between
> the compressor's output and the wet ring) is unchanged and is what the follow-up builds on;
> only its closing "decisive field experiment" is retired. Left as written, per the same rule
> the DESIGN banners follow.

**0.1.3 addendum — the owner used instrument (b), and the reading localises the kill.** With
BOTH stereo links at 0 (per-channel detectors and envelopes), the comp threshold pulled low and
the limiter gain pushed high, the owner observes: **comp GR on both lanes; limiter GR on the
RIGHT lane only; left output silent.** The two lanes are independent taps of the engine's own
state (`compGrDbCh[ch]` is the comp's per-channel envelope, `limGrDbCh[ch]` the per-channel
minimum of the limiter's applied gains), so together they bracket the kill:

- Comp GR on L, at link 0, requires the LEFT channel's detector level over the threshold —
  the left channel is **alive into the compressor** (stage A). This retires the 0.1.1
  dead-input-pin mechanism for the current report a second way (the 0.1.2 Delta observation
  already had).
- Limiter GR at zero on L, at link 0 and a push that reduces R by several dB, requires the
  LEFT channel's tapped level under the ceiling — the left channel is **dead at the wet
  ring**, which also carries the audible path (`delayedWet · gain`), consistent with the
  silence. Had the left lane instead been *pinned deep*, the diagnosis would have been
  per-channel envelope collapse; zero points the other way.

The kill zone is therefore the span **compressor output → staging → [oversampler] → ClipSat →
push → wet ring**. Audit of that span against the new constraint: the staging write, the push
(one shared scalar per sample), the ring indices (shared across channels) and every ClipSat
sub-block are channel-symmetric, and ClipSat's per-channel state is value-repaired every chunk
(invariant 9) — **at OS Off (the shipped default) the span contains no per-channel recursive
state at all**. With a factor engaged, JUCE's per-channel polyphase oversampler is the span's
one stateful occupant (its poisoning is detected per sample and reset per chunk, but it is the
single component whose internals this audit cannot inspect value by value). The exact field
configuration is now a permanent battery case at both extremes
(`testBothChannelsCarryAudioThroughTheWrapper`, "field config" pair: per-channel comp GR,
per-channel limiter GR and both outputs asserted alive at OS Off AND 4×) — both green, so the
fingerprint remains headlessly unreproducible in the very configuration that shows it in the
field. **The decisive field experiment this hands back:** with the silence audible, switch
oversampling Off (Settings). If the left channel returns, the mechanism lives in the engaged
oversampler path and the session's OS factor is the missing datum; if the silence survives OS
Off, no in-plugin site remains on the localised span, and the standing suspects narrow to a
stale/mismatched installed binary or host-side channel handling — record host + version, OS,
build provenance and format (unchanged asks since round 2), which that outcome would make
conclusive.

**0.1.3 FOLLOW-UP addendum — the Clip Mix evidence, and the mechanism it exposes (FIXED).** The
owner ran the experiment above and reported two things, both taken as confirmed and signed off:

- **With `Clip Mix` at 0 the left-channel silence disappears immediately; at any non-zero value
  it returns.**
- **The behaviour is independent of the oversampling factor**, which retires the 0.1.3
  addendum's hypothesis outright.

`clipMix` reaches exactly one place in the tree (`ClipSat::setPerBlock`, via `toEngine`; it is
not macro-managed and nothing else reads it), and its only effect is the stage's parallel blend:
at 0 the exact-skip branch leaves the sample array **untouched**, so ClipSat's computed value
never enters the signal at all. The gate is therefore not a mix VALUE — it is *whether this
stage's output reaches the wet ring*.

**What the code says about the stage itself.** `ClipSat` is **provably channel-symmetric**: the
same sequence on both channels leaves bit-identical, for every parameter combination and every
stimulus, and it never emits a non-finite value from bounded input. That is now pinned by
`testClipSatCannotLoseAChannel` (`tests/dsp_tests.cpp`) over a swept fuzz, plus an asymmetric
case in which a channel 30 dB below its neighbour survives the neighbour being clipped and
coloured. So nothing *inside* the stage can single out a channel — the shared terms
(`activityEnv`, the tame gain, the seven smoothers, `g`/`invG`, the push scalar) would silence
BOTH, and the per-channel states are identically driven.

**The one mechanism that fits, and it is real.** On the bracketed span there is exactly one
expression that can drive ONE channel to exact 0.0f while leaving the other intact: the
finiteness boundary at the wet-ring write (`AnabasisEngine::processChunk`), which substitutes
`0.0f` **per channel**. Arming it needs ClipSat to emit a non-finite value — and it can, from a
perfectly *finite* input: the colour sub-block raises to the **fifth power** (Transistor), and
at `clipDrive == 0` the clipper's own bound (`|c| ≤ 1/g ≤ 1`) is exact-skipped away, so the
polynomial runs on the raw magnitude and overflows at |c| ≈ 5.1e7. Invariant 9's repair for the
stage is `clip.sanitiseState()` — a **state** repair, inert against an **input-magnitude**
fault — so while such an input persists the boundary re-substitutes zero every sample:
**permanent, single-channel, digital silence.**

Measured end to end through the real wrapper, before the fix: a sustained finite input at 1e20
on channel 0 (ordinary programme on channel 1) produced **exactly 0.000000000 RMS on channel 0
and normal output on channel 1**, with `limGrDbCh(0)` reading 0 dB — the field fingerprint,
complete. The same input at `clipMix == 0` was merely limited to the ceiling and audible. The
existing `testExtremeLevelDoesNotSilencePermanently` already names "the clipper's colour
polynomial overflows" as a case; it drives ONE block into BOTH channels, so the **sustained**
and **per-channel** form of the same fault was never exercised.

**Fixed (0.1.3 follow-up):** the colour polynomial's argument is bounded (`ClipSat::kArithmeticLimit`,
`src/dsp/ClipSat.h`) so the stage cannot produce a non-finite value from any finite input,
restoring the premise the sub-block was written under. The bound is +120 dBFS — some 120 dB
above anything the chain can carry into this stage and eight orders inside the float ceiling —
so `jlimit` returns its argument unchanged for every reachable signal and no audible sample
changes, bit for bit; the residue is added to the UNBOUNDED through-signal, so Clip Mix
semantics and stereo independence are untouched. Pinned by the sustained one-channel case added
to `testExtremeLevelDoesNotSilencePermanently` (three mix values × four magnitudes); removing
the bound fails exactly the eight assertions of the two NON-ZERO mix rows (three mix values × four
magnitudes, two assertions each; the `clipMix == 0` rows correctly survive, which is the field
gating reproduced inside the suite) and no others. Round 3 extended the same constant to the
stage's OTHER arithmetic site — the drive product `dry · g`, which overflows the same way once
`g` leaves unity — so the constant's name no longer says "colour"; each of the two bounds is
mutation-verified on its own.

**Status: still OPEN, and deliberately.** What is fixed is a *demonstrated* defect that produces
this exact fingerprint with this exact gating. What is **not** established is that it is the
owner's trigger: reaching it needs ≳ +154 dBFS at the stage input, and no legal chain state
produces that (the EQ is RBJ-form with clamped Q and frequency and cannot go unstable; the
compressor's gain is ≤ 1; input gain tops out at +24 dB).

**0.1.3 FOLLOW-UP ROUND 2 — the owner re-tested and the field issue PERSISTS.** With the colour
bound in the build, the left channel still goes silent under the same workflow and the Clip Mix
correlation is unchanged. **The overflow mechanism above is therefore NOT the field root cause.**
It stays in the tree — it is a real defect with its own reproduction and its own regression, and
removing it would restore a way to lose a channel — but this record must not be read as though
it closed anything. **Two distinct failure classes, kept apart deliberately:**

| | Class A — the overflow (FIXED) | Class B — the field report (OPEN) |
|---|---|---|
| Trigger | a sustained finite input ≳ +154 dBFS at the colour stage (or ≳ +750 dBFS at the drive product — both bounded since round 3) | unknown |
| Reproducible headlessly | yes, exactly | **no** — every parameter/preset/macro combination swept |
| Clip Mix gated | yes | yes (owner-confirmed) |
| Status | fixed and pinned (`testExtremeLevelDoesNotSilencePermanently`, sustained one-channel case) | unresolved |

**What the code can no longer explain.** Every in-plugin path has now been either excluded or
pinned by test:
- `ClipSat` is **channel-symmetric and non-finite-free from bounded input**, swept
  (`testClipSatCannotLoseAChannel`) — so no state inside the one stage `clipMix` gates can
  single out a channel.
- `clipMix` reaches **only** `ClipSat::setPerBlock`; it is not macro-managed and nothing else
  reads it, so it cannot reach the compressor, the limiter, the EQ or the monitor legs.
- The compressor is upstream of ClipSat and its gain is strictly positive; the limiter, the
  push, the ring indices, the duck, the bypass/delta mixes and the monitor gain are all
  channel-shared or strictly positive.
- After the Class-A fix, **no expression on the span can produce a non-finite value from a
  reachable input**, so the per-channel zero substitution is unreachable in normal use.
- A **realistic soak** — 40 trials of ~12 s each: broadband, correlated, bass-heavy stereo near
  full scale, the Loudness macro driven through its whole range while audio runs, factory
  presets / Copy / undo landing mid-stream, editor alive, across 44.1/48 kHz, 128/512 blocks and
  three oversampling factors — produced **no channel loss in any block**.

**0.1.3 FOLLOW-UP ROUND 3 — the bypass answer, and the contradiction it leaves.** The owner ran
the bypass test and reported the environment. **All owner-confirmed, recorded as evidence:**

| Datum | Value |
|---|---|
| Build | latest, freshly installed |
| OS / formats | macOS; **AU and VST3 both, behaviour identical** |
| Oversampling | no effect at any factor |
| Every other parameter / preset / setting | no effect |
| Clip Mix | **0 removes it immediately; any non-zero value reproduces it** |
| **Global BYPASS** | **restores the left channel** |
| Comp GR lanes (both links 0) | L non-zero, R non-zero |
| Limiter GR lanes | R non-zero, **L zero** |

**Bypass is the load-bearing new datum, and it points inward.** The bypass leg carries
`delayedDry` — the plugin's own input, delay-aligned and bit-exact (invariant 7), with no stage
on it. Left returning under bypass proves the left pin is live and the host takes the left
output. **The loss is inside the processed leg.** Host routing and output handling are
excluded; so is the format layer, since AU and VST3 behave identically.

**And that leaves the evidence set jointly inconsistent with the code.** The implication chain,
each step forced by a line in the tree:

1. Left audible on bypass ⇒ `dryRing[0]` alive ⇒ the input is alive.
2. Comp GR on lane 0 **with `compStereoLink = 0`** ⇒ `linked = det[0]` ⇒ `|chans[0]|` is over
   the threshold at the compressor's input ⇒ alive INTO the compressor.
3. The compressor's applied gain and its published lane are the **same expression**
   (`grDb[ch]`, `MasteringComp.h`), and that gain is strictly positive ⇒ alive OUT of it, at
   whatever depth the lane shows.
4. `staging` → the region → `ClipSat` → the push → `wetRing` contains nothing channel-selective:
   ClipSat is proven symmetric and (since round 2) cannot emit a non-finite value from ANY
   finite input, the push is one shared scalar, the ring indices are channel-independent.
5. ⇒ `wetRing[0]` is alive ⇒ the limiter's tap sees it ⇒ its lane should read like the right's,
   and the output should be audible.

Steps 1–5 contradict the observation. One of them is false in the field, and **the code says
none of them can be** — so the next move is to break the chain empirically rather than to
propose a sixth mechanism.

**Two field experiments, both free, that split it:**

1. **Sweep Clip Mix rather than toggling it.** Try 1 %, 10 %, 50 %. The blend is
   `dry + (wet − dry)·mix`, so a stage that *kills* the channel produces a **cliff** (silent at
   1 %, because `wet[0]` would have to be ~−99× the dry to cancel it — which a bounded stage
   cannot produce), while a **ramp** (1 % barely audible change, 50 % half gone) says the left
   channel is not being killed at all but *cancelled* — `wet[0] ≈ −dry[0]`, a polarity result,
   which is a completely different search. Record the shape, not just the endpoints.
2. **Read the Statistics panel with the silence audible.** `SP` and `RMS` there read the
   **render tap** — the same signal the output carries, both channels summed. If SP tracks the
   right channel alone at roughly −6 dB of the two-channel figure, the left really is absent
   from the render; if SP is absurdly high, Class A is live after all despite the bound.

Also worth one glance: the two GR lanes' **depths**, not just their presence — L pinned at the
bottom of its 24 dB lane is per-channel gain collapse and reads as "has GR" at a glance, which
is a different diagnosis from L sitting at a normal depth.

**Still wanted, and now the blocking artefact: the session file.** Every parameter combination,
preset, macro position and A/B state reachable from the UI has been swept headlessly without
reproducing this; a saved session is the only object that can carry a state the sweep cannot
construct.

**0.1.3 FOLLOW-UP ROUND 4 — step 4 of the chain is now PROVEN, not argued.** The chain above
turns on one claim: that nothing between the compressor's output and the wet ring can single
out a channel. Rounds 2–3 supported it by symmetry, and symmetry is the weaker statement —
a stage can be perfectly symmetric and still hold a shared term that one channel drives into
the ground, which is precisely the shape "one channel silent, the other normal" has. Round 4
proves the stronger property and pins it (`testClipSatCannotLoseAChannel`, cross-channel
section):

- **`ClipSat` has exactly ONE cross-channel term** — `activityRaw`, the clip-depth maximum
  that drives the shared dynamic-tame gain. Everything else in the stage is per channel and
  identically driven, which the enumeration in the source and the swept symmetry fuzz both
  say.
- **With the tame idle the channels are BIT-EXACTLY independent**: the same channel-0 input
  rendered beside silence and beside a hostile 8.0-amplitude neighbour produces identical
  output, sample for sample, over 24 000 samples.
- **With the tame at its 2 dB maximum the neighbour moves channel 0's LEVEL by ~1 dB** and
  cannot approach silence.
- **And the reason is structural, not the 2 dB range.** The tame is a SHELF —
  `gLin·wet + (1−gLin)·tameLp` — so however far the shared gain falls, the lowpass leg
  survives, and a first-order lowpass still passes ~0.37 of its input at Nyquist. Mutation
  testing demonstrated this directly: widening the shared gain 40× does **not** break the
  assertion, while turning the same shelf into a broadband gain (`wet *= gLin`) breaks it at
  −48.5 dB. **A cross-channel kill is impossible in this stage by construction**, at any gain.

So step 4 is settled: **the span the field evidence indicts cannot produce the field symptom.**
Combined with step 3 (the compressor's applied gain and its published lane are the same
expression, so a live lane means a live output) and steps 1–2 (bypass and the comp lane prove
the input alive), the contradiction is now between the observation and a proof rather than
between the observation and an argument. What that leaves, in order of what a session file
would settle:

1. **A state the parameter sweep cannot construct** — the sweep drives the UI surface; a
   session carries serialized state, including the ADR-0014 frozen trim vector and the §5.3
   detach mask, which no knob sequence reproduces exactly.
2. **A stage OUTSIDE the indicted span** whose contribution merely correlates with Clip Mix —
   which the "sweep Clip Mix for a cliff-vs-ramp shape" experiment above discriminates
   directly, and which nothing in the record has yet ruled out because every observation so
   far has used the endpoints 0 and 100 only.
3. **A platform difference this Linux tree cannot exercise** — macOS arm64 codegen under LTO
   being the only untested axis, though AU and VST3 behaving identically argues against a
   wrapper-level cause.

None of these is a hypothesis about a mechanism; they are the three places a mechanism could
still hide once the span is proven clean.

**0.1.3 FOLLOW-UP ROUND 5 — "the Default preset reproduces it" retires Clip Mix as the cause.**
The owner adds that the failure reproduces at the **Default preset, and at every preset**. That
single datum settles the question the previous four rounds circled, because at the Default
preset **Clip Mix cannot change one sample of audio**:

- `clipDrive == 0` ⇒ the clip sub-block is exact-skipped (the bit-identity contract);
- `colourDepth == 0` ⇒ the colour residue is not evaluated at all;
- nothing clips ⇒ `activityEnv` is bit-zero ⇒ the dynamic tame takes its idle branch, and it
  does so even with the §5.4 dynTilt trim at its +0.5 dB bound (the shelf engages only below
  −0.01 dB, and `−0.5 × activityEnv` cannot reach it from a zero envelope).

So ClipSat's wet value **is** its input, and every branch of the mix lands on the same float:
`mix == 1` assigns the same number, `mix == 0` writes nothing, and any value between computes
`chans + (wet − chans)·mix` where `wet − chans` is exactly `0`. `clipMix` reaches nothing else
in the tree — it is not macro-managed, and `ClipSat::setPerBlock` is its only consumer.

**Pinned, not argued:** `testClipMixCannotChangeTheDefaultPresetsSound` (`tests/state_tests.cpp`)
runs the field scenario through the real wrapper — factory preset applied, correlated programme
near full scale, ~1 s of settled audio — and asserts the render is **bit-identical** at Clip Mix
0 / 1 / 25 / 50 / 99 / 100 %, with both channels alive at each. It then sweeps **every factory
preset** at both endpoints and asserts both channels stay alive and within 6 dB of each other.
Mutation-verified: giving the stage any mix-dependent contribution at the null settings
(`chans[ch] = wet[ch] * 0.9995f`) fails it at 23 027 differing samples.

**What this means for the investigation, stated plainly.** The Clip Mix correlation — the datum
every round since the first has been steered by — **cannot be causal at the preset where the
owner also observes the failure**. It is a correlate. Either the control being *moved* matters
rather than its value (it is an Advanced-view knob, so reaching it requires the ADV toggle and
a gesture — both of which touch state the audio path does not read), or the two observations
come from different moments in the session. Continuing to search the Clip Mix path is now
provably searching the wrong place, which is why this addendum exists rather than a sixth
mechanism.

**The field experiments that follow from this, in order:**

1. **Does Clip Mix still remove it at the Default preset with the ADV view already open?** If
   the fault clears only when the knob is *touched* rather than at any particular value, the
   trigger is a gesture/state event, not the parameter — and the session file becomes the
   whole investigation.
2. **Does it clear when ANY Advanced knob is touched, not just Clip Mix?** Same question,
   sharper: if yes, `clipMix` was never special.
3. **Does it recur after Clip Mix is returned to 100 %?** If it does not, the fault is a
   one-shot state that a parameter write clears — which points at the serialized/restored
   state paths (the ADR-0014 frozen-trim vector, the §5.3 detach mask) rather than at DSP.

**0.1.3 FOLLOW-UP ROUND 6 — runtime instrumentation, and the coverage hole it exposed.** Static
reasoning was taken as far as it goes, so the chain was instrumented instead: thirteen
per-channel energy taps from the raw input to the final write (raw in · post input gain · post
EQ-pre · comp out · ClipSat in · ClipSat out · wet-ring write · limiter detector tap · limiter
gains · delayed wet · stage-E in · post clamp · final out), run under the owner's exact
reproduction configuration at both Clip Mix endpoints.

**The trace found no divergence** — every tap balanced at L/R ≈ 1.04, both GR lanes reducing on
both channels, and the two endpoints bit-identical. But reading the engaged values back
exposed something the five previous rounds had been standing on without knowing it:

> **At "loudness 85" the clipper reported `clipDrive == 0`.**

`MacroEngine` maps the macro knobs onto the nine managed parameters on a **30 ms
`juce::Timer`**, and a headless console app runs no message loop, so that tick never fires.
Setting the Loudness parameter in a test moves the knob and **nothing downstream of it**. Every
"loudness N" configuration in `testBothChannelsCarryAudioThroughTheWrapper` — and every sweep
run against this report since round 1 — was therefore re-running the DEFAULTS case, with the
clipper exact-skipped and the colour stage contributing nothing.

**Why that matters here specifically.** The field report is observed with the chain PUSHED, and
a pushed chain is the one configuration in which `clipMix` is not inert — it is what makes the
clip/colour/tame contribution real, and so what makes the mix blend do anything at all. The
headless coverage of exactly that state was vacuous. This does not make the Clip Mix
correlation causal (at the *Default preset* it still provably cannot be — round 5), but it does
mean "swept headlessly without reproducing" was a weaker statement than it read as, for the
configuration that matters most.

**Fixed in the battery:** the loudness cases now engage `clipDrive`, `colourDepth`, `dynTilt`
and `limGain` directly alongside the knob, with the timer reason recorded at the call site. Run
that way, the owner's configuration with the clipper genuinely working still shows both
channels alive and matched (L/R within 1 %) at every mix value — so the hole is closed and the
report still does not reproduce, but the two facts are now independent rather than one being an
artefact of the other.

**Status: OPEN.** Not fixed. The two live directions are unchanged in substance and sharper in
priority: a serialized/restored state the parameter surface cannot construct, and the
gesture-versus-value question round 5's experiments separate. What round 6 removes is the
possibility that the headless "cannot reproduce" was itself the artefact.

**0.1.3 FOLLOW-UP ROUND 7 — the owner's platform negative, and what it does to the scope
(2026-08-10).** The owner re-tested on **Linux** and reports the fault does **not** occur there.
The heading's "NOT REPRODUCIBLE headlessly" therefore understates what is now known: the report
is **not reproducible on Linux at all**, headless or with the editor and a real host, by the
person who filed it. macOS remains the only platform on which it has ever been observed;
Windows is untested by either side.

**What that changes.** Six rounds of headless work all ran on Linux, so "cannot reproduce" and
"the owner cannot reproduce it here either" now agree — and agreement on a platform where the
fault is absent carries no information about the platform where it is present. Every
Linux-executed argument in the rounds above stays valid as a statement about the *source*
(channel symmetry, the absence of a per-channel gain, ClipSat's inability to emit a non-finite
from a finite input, Clip Mix's provable inertness at the Default preset). None of them is
evidence about the shipped macOS binary, because none of them ran on it.

**The hypothesis ranking is reordered accordingly.** Round 6 left two live directions plus a
third listed as an also-ran — a macOS-only divergence, whether in the arm64 codegen of the
clip/colour path or in the AU/VST3 glue as macOS drives it. The Linux negative promotes that
third to the leading hypothesis, because it is the only one of the three that predicts a
platform split. The state-restore and gesture-versus-value directions predict the fault on any
platform and are demoted, not dropped: a *state* that only a macOS session ever contains would
still fit.

**Status: OPEN, and now explicitly macOS-scoped.** The next useful datum can only come from a
macOS build: the same thirteen-tap trace of round 6 compiled and run on macOS arm64, which is
not reachable from this CI-less Linux tree. Until then no Linux-side experiment can move this
entry, and none should be run in its name — that is the concrete conclusion of round 7, and it
is why this round adds no test.

**0.1.3 FOLLOW-UP ROUND 8 — macOS validation restored, and what it says (2026-08-10).**
Round 7 concluded that only a macOS build could move this entry. It then turned out that no
macOS build had run: the macOS CI job had failed to COMPILE `tests/state_tests.cpp` on every
push since round 5's commit `f50d6c2` (`INC-003` — `juce::jmax<size_t>` instantiates JUCE's
`SIMDRegister` overload, which is well-formed on Linux and ill-formed on macOS because
`uint64_t` is `unsigned long` there and is not).

**The consequence for this entry is exact and uncomfortable.** The broken line sits INSIDE
`testClipMixCannotChangeTheDefaultPresetsSound`. So every KI-009 regression written in rounds 5,
6 and 7 — written specifically for a fault that reproduces only on macOS — had never once
executed on macOS. Three rounds of "the suites are green" meant "green on the platforms that
still ran", and that is the platform this report is not about.

**With the build restored, the whole macOS job is green** (run 31435190370, commit `6f63573`,
`macos-14`, universal arm64 + x86_64): build → both suites → pluginval ×3 in BOTH modes at the
`build.yml` strictness → packaging. So the first arm64 execution of every previously-dark test
passes, and the answer it gives is a negative one: **the suites do not reproduce KI-009 on
macOS either.** That is a materially stronger negative than round 7's, because it is measured on
the platform that reproduces rather than inferred from the one that does not — but it is still a
negative, and it means the reproduction lives outside what the suites drive.

**What the round DID find, and it is a real defect class rather than a restatement.** A
five-lens audit of the compressor→ClipSat→limiter span produced one finding that survived
verification: **at Clip Mix 0 this stage can poison its own state invisibly.** The mix loop's
exact endpoints leave the dry sample untouched at `M == 0` (the bit-exact identity path, which is
correct and must stay), so the stage's OUTPUT is finite no matter what its internals did —
while `tameLp[ch]` is updated on BOTH tame branches, deliberately, to keep the 6 kHz split warm.
Warm state plus an invisible output is a latch: invariant 9's repair is keyed on
`stageGeneratedNonFinite`, which is raised by the BOUNDARY, and at mix 0 the boundary has
nothing to see. The state would then be paid for later, on ordinary audio, the moment the mix
opened with the tame engaged.

That shape is the reported fingerprint exactly — channel-local, gated on a non-zero mix, absent
at mix 0, upstream of the limiter's detector tap (so the compressor still shows GR on both
channels while the limiter shows it on one), cured by bypass. **It is NOT reachable in the
shipped build**: 0.1.3's `kArithmeticLimit` bound on the colour argument holds the whole stage,
and `testClipSatCannotHideANonFiniteFromTheBoundary` now proves it over 30 poisoning attempts up
to FLT_MAX with the colour stage swept on and off. Remove that bound and the same test fails
with 32 000 of 120 000 non-finite output samples **on ordinary audio, after the poisoning
stopped** — which is what makes it worth pinning: it shows the 0.1.3 fix closed not just an
immediate overflow but a deferred, state-latched form of the same fault that no existing test
would have caught, because at mix 0 there is nothing at the boundary to catch.

**Also newly excluded, from evidence rather than argument:**

| Hypothesis | How it died |
|---|---|
| Uninitialised / stale per-channel state | Both suites are VALGRIND-CLEAN under memcheck with `--track-origins` (0 errors from 0 contexts) and clean under ASan+UBSan. An uninitialised read is platform-independent even when its symptom is not; the tools see it whatever bytes the allocator supplied |
| Wrapper, bus layout, channel pointers | Global bypass feeds ONLY the engine's output selector (`AnabasisEngine.cpp:1046-1049`) — no DSP is skipped, no state reset. "Bypass restores the left channel" therefore PROVES the host delivers channel 0, that both channel-0 pointers are right, and that the dry ring is intact |
| The two macOS `-Wshorten-64-to-32` warnings in the region access | `AudioBlock::getSample/setSample` take `int`; the casts were an int→size_t→int round trip on values confined to ch ∈ {0,1} and i < num<<osShift. Value-preserving, incapable of silencing a channel. Removed as noise |

**Status: OPEN, macOS-scoped, and now with a working validation path.** The kill zone is
narrowed to the oversampled region (`AnabasisEngine.cpp:726-846`) with the wrapper, the bus
layer and the memory-safety classes excluded. What no headless suite has yet reproduced on
either platform is the field configuration itself, so the next datum has to come from a real
macOS host: the plugin under a DAW with the reported settings, not another sweep. The macOS
gate now runs on every push — including, since this round, pluginval against the **AU** as well
as the VST3, the format the report names and the one that had never been validated at all.

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
KI-009 does, and under the same discipline: it carries the *experiments*, so the next round
starts from what has already been ruled out rather than repeating it.

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

Evidence [Partially Verified]:
- Source: `src/gui/PluginEditor.cpp` (GL gate, overlays, `applyUiScale`), `CMakeLists.txt`
- Test:   none — the report does not reproduce; the runtime harness above is recorded in
  `worklogs/`, not in the suites, because a passing probe of a fault that never appears would
  pin nothing
- Commit: this one

## Standing note for P1 onward

Two categories are known in advance to need entries in this project, from the sibling product's
experience. They are named here as **expectations**, not as claims that they already occur:

- **Anything that cannot be validated headlessly** — audio quality, GUI appearance, real-DAW host
  behaviour — is a documented coverage limitation, not an unknown. It belongs here the moment it
  blocks something concrete.
- **Host-specific behaviour** (parameter display, automation recording, editor resize, plugin
  rescan) is where most confirmed issues in a JUCE plugin end up. Record the exact host and
  version; "some DAWs" is not an entry.
