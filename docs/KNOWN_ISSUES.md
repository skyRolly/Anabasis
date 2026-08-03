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
exposure (the restore itself writing `replaceState` / `liveDetachMask` / `livePresetName` while the
editor reads them) is untouched and still needs the thread-model decision below.

A **third** member, added 2026-08-03 (review round 28): the §7 undo/redo stacks.
`setStateInformation` clears `undoStacks`/`redoStacks`/`gesturePreState`, which
are plain `juce::Array<juce::ValueTree>` members, while the editor reads
`canUndo()`/`canRedo()` on its 24 Hz tick and `undo()`/`redo()` pop from them on
the message thread — so an off-thread load with the window open can race a
`clear()` against a `removeAndReturn()`. It is the same exposure as
`replaceState`, `liveDetachMask.clear()` and `livePresetName` on this path, and
it closes with the same thread-model decision rather than separately; recorded
because the stacks are NEW state added at P6, and this round's standard
elsewhere was that both halves of a premise should agree.

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

**The state half is CLOSED as of round 40 (2026-08-03), and the audio half is
what remains.** The fix is an ownership statement rather than a Freeze decision:
the wrapper's mirror is the DURABLE owner of the frozen vector — it is what
serialises — and the engine's published trims are a faster-moving copy that does
not survive the engine's own re-initialisation. `prepareToPlay` therefore copies
that latch into the mirror **before** calling `engine.prepare`, guarded by the
same two conditions the save already used (no restore staged-but-unapplied; the
engine has published at least one meaningful vector), which now live in one
function, `engineFrozenTrimsIfLive()`, read by both sites.
Guarded by `testPreparedStateAndSlotOwnership` case 4 (`liveLatch:`), whose
premise check asserts the engine's own copy is still gone — this closes the
serialization path without touching what a re-prepare does to the audio.

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
above. The alternative (re-stage the vector to the ENGINE from the wrapper at
`prepareToPlay`) used to be blocked by the same asymmetry — `liveFrozenTrims`
held one only after a load — but round 40's capture removes that objection: the
mirror is now populated for a live latch too, so re-staging has something to
re-stage in every case. It is still not done here, because re-staging is exactly
the Freeze-semantics change this section defers; the capture deliberately stops
at the serialization boundary.

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
LIVE, whose only record was the atomics the re-prepare cleared — by capturing that latch into the
mirror at `prepareToPlay`, and moved the two-clause "is the engine's copy the truth?" test into
`engineFrozenTrimsIfLive()` so the save and the capture cannot drift apart. The AUDIO half above
is untouched and still needs the owner call.

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

2. **Preset-ring navigation identifies the current entry by NAME.** `stepPreset`
   (`src/gui/PluginEditor.cpp`) matches `currentPresetName()` against the factory table first and
   the user files second, so a user preset saved as "EDM Club" resolves to the factory index and
   the arrows walk from the wrong place. Robust fix is to track the last applied SOURCE (factory
   index vs file) instead of re-deriving it from the display string — which is state the editor
   does not currently keep, hence not a one-line change.

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

5. **The dirty marker keys on the whole slot tree, so preset-EXCLUDED parameters mark a preset as
   edited.** `presetDirty()` compares `presetBaseline` against a fresh `saveSlotFromLive()`, which
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

6. **The spectrum view freezes rather than decaying when audio stops.**
   `SpectrumView::tick` returns early when neither capture ring's write count moved, so the
   per-bin EMA stops and the last analysed trace stays on screen indefinitely after a transport
   stop or a plugin suspend. It is cheap and reads as deliberate ("idle: nothing new"), but most
   analysers decay to the floor, and a frozen trace can be mistaken for live signal. Which
   behaviour this product wants is a listening-pass call, not a repair — the fix (run the EMA
   toward the floor on an idle tick) is three lines once the answer is known.

7. **RESOLVED 2026-08-03 (round 37) — Copy A→B and the destination's undo history.**
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

## Standing note for P1 onward

Two categories are known in advance to need entries in this project, from the sibling product's
experience. They are named here as **expectations**, not as claims that they already occur:

- **Anything that cannot be validated headlessly** — audio quality, GUI appearance, real-DAW host
  behaviour — is a documented coverage limitation, not an unknown. It belongs here the moment it
  blocks something concrete.
- **Host-specific behaviour** (parameter display, automation recording, editor resize, plugin
  rescan) is where most confirmed issues in a JUCE plugin end up. Record the exact host and
  version; "some DAWs" is not an entry.
