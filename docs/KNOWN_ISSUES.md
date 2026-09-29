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

*(KI-001 — unducked discrete transitions — KI-002 — inert Loudness Comp/Delta — KI-009 — the
silent left channel — and KI-006 — a re-prepare dropping a frozen slot's trims from the audio — are
FIXED and recorded as `POSTMORTEMS.md` INC-001/INC-002/INC-004/INC-007; their numbers are never
reused. KI-009's entry ran to five months of round-by-round investigation, and
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
and the latch may then clear the lookahead ring at full gain. The STEP is
bounded to the first sample of an offline render, and the alternative
(carrying a monitor fade into a bounce) is worse; recorded so it is not
rediscovered as a defect. *(Corrected 2026-09-28, the PR #42 review of 0.2.14:
this paragraph used to read as if nothing else reached the render.)* What the
unity gain then plays is whatever the pipeline holds — on this no-re-prepare
route the engine does not reset: the lookahead ring's realtime content, and,
when the previous realtime block latched a composition, the Post EQ's ring-out
from before the latch (the silent bottom would have hidden it). A host that
re-prepares before a bounce starts from an empty pipeline instead. Until
0.2.15 a true-peak ENGAGEMENT decay (ADR-0041 decision 5) also survived the
entry and played the last realtime frame's decay into the head of the render,
up to +1.46 dB over the ceiling with that ring-out; the entry now drops it,
since it belongs to the duck (`testOfflineEntryDropsTheEngagementTail`).

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
Loudness-macro move that carries it: the macro's drive curve is 0 dB below Loudness 30 % and
rises from there — `MacroEngine.h` `clipDriveDb`) across the 0 dB boundary during playback.
*(Corrected 2026-09-27: this line said "Character-macro"; the Character macro sets the colour
depth, not the drive — audit finding DSP-004.)*

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

**The steady-state half — the droop stays for as long as the drive is non-zero, whatever its
amount (audit finding DSP-004, measured 2026-09-27).** The engage step above is the transition;
the `(1 + z⁻¹)/2` response it switches to is also what the stage applies to the whole programme,
in the clipper's linear region, the entire time the drive is off zero — and at the default
oversampling (Off, Offline Follow, so the bounce too) that is the base rate. Measured on the real
engine with a −30 dBFS sine (the clipper linear), response with a drive of 0.07 dB — the value the
Loudness macro reaches at 30.5 % — or 3 dB (identical: the amount does not matter), relative to
drive exactly 0:

| Rate | Oversampling | 5 kHz | 10 kHz | 15 kHz | 16 kHz | 20 kHz |
|---|---|---|---|---|---|---|
| 48 kHz | Off | −0.47 dB | −2.01 dB | −5.11 dB | −6.02 dB | −11.74 dB |
| 48 kHz | 2× | −0.12 dB | −0.47 dB | −1.09 dB | −1.25 dB | −2.01 dB |
| 48 kHz | 4× | −0.03 dB | −0.12 dB | −0.26 dB | −0.30 dB | −0.47 dB |
| 44.1 kHz | Off | −0.56 dB | −2.42 dB | −6.35 dB | −7.58 dB | −16.74 dB |
| 44.1 kHz | 2× | −0.14 dB | −0.56 dB | −1.30 dB | −1.49 dB | −2.42 dB |
| 44.1 kHz | 4× | −0.03 dB | −0.14 dB | −0.31 dB | −0.36 dB | −0.56 dB |

It is the first-order ADAA kernel's own response, `cos(πf / (N·fs))` at N× oversampling, and it
is by design (the ADAA trade recorded in `ClipSat.h`); what was missing is that nothing outside the
code said so. **Workaround:** oversampling 4× (Settings) cuts it to ≤ 0.6 dB at 20 kHz; 2× leaves
~2 dB there. A DSP remedy — a droop-compensating pre-emphasis on the driven branch, built together
with this entry's engage ramp — or a non-Off oversampling default is an owner decision, not made
here (the worklog of 2026-09-27 records the measurement and the options).

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
   asked about a re-prepare. Settle them together or the two answers will disagree. *(2026-09-27:
   KI-006 is closed — ADR-0042, accepted, answers it for a re-prepare: with Freeze ON the applied
   vector carries. This item stays open, and its answer should be consistent with that one.)*

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
   `SpectrumView::tick` returns early when nothing new has arrived — since 0.2.12 that means the
   COMMITTED HEAD (`min` of the two rings' write counts, the newest frame both taps have published)
   has not moved and neither generation has; before it, that neither ring's write count had moved.
   The behaviour this entry is about is unchanged either way: the per-bin EMA stops and the last
   analysed trace stays on screen indefinitely after a transport stop or a plugin suspend. It is cheap and reads as deliberate ("idle: nothing new"), but most
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

10. **An A/B switch into a freeze-ON slot that holds no `FROZEN_TRIMS` keeps the OUTGOING
   slot's latch in the audio** (added 2026-09-27 from the audit's merged note on STATE-004;
   code-inferred, not run). `applySlotToLive` stages a restore only when the incoming slot has a
   vector, so nothing replaces the engine's applied vector, and Freeze holds it — while round 42's
   ownership rule correctly makes the incoming slot save none. The audio and the saved record then
   disagree, and a reload renders a different sound. The option consistent with round 42's
   no-borrowing rule is to stage a never-latched (zero) vector on that ownership change; it is the
   same Freeze-semantics owner call as item 1, and ADR-0042 (the re-prepare carry) deliberately
   preserves this pre-existing shape rather than resolving it — `testAFrozenLatchDoesNotFollowTheSlotSwitch`
   pins that the carry does not make it worse.

**For the post-v0.1.0 fine review, alongside ADR-0042 option A** (Freeze OFF across a re-prepare
— KI-006, closed 2026-09-27 as `POSTMORTEMS.md` INC-007, settled only the Freeze ON half).

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
| M0 → M1 | `AnabasisAudioProcessor::audioProcessorParameterChangeGestureBegin` (`src/PluginProcessor.cpp:223`) takes the §7 pre-state with `saveSlotFromLive()` → `copyStateWithRaw()` → `apvts.copyState()`, from **inside** the listener callback that already holds M0. |
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
- Source: `src/PluginProcessor.cpp:223` (the M0 → M1 edge); JUCE
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

> **CORRECTED AND EXTENDED 2026-09-06 (round 11).** The audit above is right about the window it
> examined — the one INSIDE `engine.prepare`, where the rings are rewound and the pair has not yet
> republished — and its conclusion for `SpectrumView` ("reads an empty ring and floors its trace")
> holds there. It did not examine the window on the OTHER side: **after** the pair republishes and
> **before** the view's next tick publishes a frame. There `paint` read the NEW rate against the
> trace the LAST tick left, which is the previous configuration's — the mismatch this round's review
> found, and which round 10's published frame made wider rather than narrower, by giving the trace
> its own publication schedule while leaving the rate on the processor's. Measured at 6 kHz, bin 512
> at 48 kHz and bin 256 at 96 kHz: −0.00 dB paired, −116.80 dB and −120.00 dB crossed. Repaired by
> carrying the rate INSIDE the published frame and taking it under the GR ring's epoch (ADR-0039,
> Accepted 2026-09-06 — this read `Proposed` until the 2026-09-27 acceptance sweep found it);
> `SpectrumView` therefore moves from the banner's unbracketed discipline to its
> bracketed one, and `CurveView` is the only unbracketed reader left — legitimately, since its curve
> comes from the parameter set and not from a ring, and its "bounded correct-but-late frame" reading
> above is unchanged.

Evidence [Verified]:
- Source: `src/gui/SpectrumView.cpp` (`paint`, the axis mapping) and `src/gui/CurveView.cpp`
  (`readInputs`, reached from `paint` AND the editor's timer); `src/gui/PluginEditor.cpp` (the
  channel-count read that remains); `juce_audio_processors`' plain `currentSampleRate`/`blockSize`
  members
- Precedent: ADR-0011's second dated 2026-09-02 amendment (the repaired instance in `GrHistoryView`)
- JUCE: `juce_opengl/opengl/juce_OpenGLContext.cpp` (the MessageManager lock around
  `paintComponent`), read at the pinned 9.0.1
- Worklog: `worklogs/2026-09-02-ki015-scopebuffer-payload.md`

### KI-018 — A spectrum reset can leave the previous trace on screen for one or more ticks (2026-09-02) — **REPAIRED except for one corner, round 7; the cross-ring variant removed in round 13 (2026-09-07)**

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
> **WHAT REMAINS — RE-SCOPED AND RE-BOUNDED IN ROUND 8, because two of this entry's own quantitative
> claims were wrong as applied to the corner.**
>
> **The interleaving.** The reader commits `shownInGen = G`, `shownInCount = C > 0` at tick T.
> `reset()` runs. At tick T+1 the reader loads `gi0 == G` (the bump not yet visible) and `ci == C` —
> the post-reset refill having landed on exactly C. `resetObserved (G, G, C, C)` is false, the idle
> test matches on all four equalities, and the tick does nothing.
>
> **The observable effect.** The previous trace is held. With both rings in the corner the tick
> early-returns and the trace is held verbatim. There was also a CROSS-RING variant, found in round 8
> and not previously written down: one `prepare` resets both rings in order, so the in-ring's rewind
> orders nothing about the out-ring's — a tick could floor `inDb` on a zero-length read while
> `analyse (out, …)` folded pre-reset frames into `outDb`, painting a floored input trace beside an
> intact pre-reset output one. **That variant is gone as of round 13** (see below): either ring's
> observed reset now floors both traces. The equal-count corner — this entry's subject — remains, and
> is **correct-but-one-frame-late**, not wrong data: nothing incorrect is committed, and the
> fall-through commit is idempotent.
>
> **NARROWED 2026-09-06 (0.2.12), by the committed head.** The reader now reads both rings at
> `min` of the two published counts and each read clamps that to its own ring's index
> (`ScopeBuffer::readEndingAt`), so a rewind that is VISIBLE when the tick takes its two count
> loads drags the committed head to 0 and both reads return nothing: both traces floor together and
> the cross-ring variant cannot occur. What survives is the narrower window in which the rewind
> lands AFTER those loads — there the clamp floors the rewound ring alone and the other still reads
> its pre-reset frames, for the same one tick. The equal-count corner this entry is about is
> untouched: the refilled head equals the shown one, so the idle test matches exactly as before.
>
> **AND WHAT THAT ONE TICK USED TO ALSO COST (round 11) — CLOSED IN ROUND 13.** Since ADR-0039 the
> published frame carries the sample rate its bins are read through, and the pairing that record
> proves is between the rate and the frames the tick's ACQUIRED INDICES describe — not between the
> rate and every sample the per-bin EMA remembers. In exactly the window above, the ring whose
> rewind was not yet visible could therefore fold pre-reset frames into an EMA published beside the
> NEW rate: one trace drawn through the wrong bin mapping, measured at **104.3 dB from its partner**
> at the previous configuration's marker bin (512-frame block; 69.0 dB at 4096).
>
> **THE CROSS-RING VARIANT IS REMOVED (2026-09-07, round 13), not narrowed.** `SpectrumView::tick`
> now floors BOTH traces whenever EITHER ring's reset is observed, at the reset edge and at the
> post-batch generation re-read alike. The detection stays per ring — a rewind is a property of one
> ring's index — but the consequence is the whole view's, because `AnabasisEngine::prepare` rewinds
> both rings back to back and unconditionally, so observing one is proof the other was rewound too
> and proof the other trace's EMA belongs to the configuration that ended. The published frame also
> carries the identity of the configuration generation it belongs to, so a frame that mixed two
> would say so in its own numbers. ADR-0039 clause 10, which had ratified the residual as *"bounded,
> not removed"*, is amended by exception with the same date. Pinned by `specGen` (the split each way
> round, both markers, both directions of the pair) and `specStraddle` (a rewind becoming visible
> inside a tick).
>
> **Why it is bounded — and the correction that matters most.** The "worst case is a scheduling
> quantum, tens of milliseconds" bound stated above **does not apply to this corner**, and leaving it
> to stand overstated the residual by orders of magnitude. That bound came from the host being
> preemptible BETWEEN `reset()`'s two stores. But `reset()` runs from `prepare` with audio stopped,
> so throughout any such preemption `write` stays at 0, the refill cannot run, and every reader tick
> sees a count below `shownInCount`, fires the count term and floors the trace. **The preemption
> window is exactly the case round 7 repaired; it cannot produce this corner**, whose refill by
> definition happens after `prepare` has returned. The corner's window is pure store-propagation
> latency. Four things must hold at once, and the last two pull against each other: `C > 0`; the
> refill lands on exactly C; no reader tick during the refill (any such tick floors); and the
> generation still stale on a load issued at least a block period after a release RMW that already
> retired. On the two shipped ISAs — x86-64, and AArch64, which is other-multi-copy-atomic — those
> last two are in practice contradictory. **That is a narrowing observation about real hardware, not
> the basis of the disposition**, and it must be re-derived if the target set ever widens to a
> non-multi-copy-atomic ISA.
>
> **Why it is not a stale-data correctness failure.** Invariant 1 is untouched: no payload is read on
> the early-return path, and on the fall-through path `readLatest`'s acquired index is post-reset.
> Invariant 2 — *no pre-reset visual result may remain displayed after the reset becomes
> OBSERVABLE* — has a false antecedent here, and the second correction is about that word. Saying
> "the count term is silent (equal, not lower)" reads as though the reader holds evidence and
> discards it. It does not: the value C it loads is BIT-IDENTICAL to what the ring published before
> the reset, so that load carries **zero bits of evidence**, as do the stale generation and a
> non-empty `readLatest`. All facets are silent. The reading in force is **per ring for DETECTION and
> per display for the CONSEQUENCE** (round 13): each trace is still drawn from its own EMA, but a
> reset detected on one ring floors both, so there is no longer a cross-ring variant to classify. The
> stronger per-display, real-time reading of the ANTECEDENT is still not the contract and cannot be,
> since under it every reset violates invariant 2 for one tick period and the invariant would
> describe no achievable design for a polled reader. What remains is a reset NEITHER ring has made
> observable — where the reader holds zero bits of evidence and no flooring rule can help.
>
> **Why it cannot persist and cannot lose a reset.** The idle early-return precedes the commit, so
> `shownInGen` stays at the pre-reset value and the reader is still comparing against it. Transport
> running: the count moves past equality and the generation term floors. Transport stopped: the
> count never moves, and the idle test fails on the generation alone — which is precisely why the
> generations are in that test. `shownInGen` advances only in a tick that floored before or after
> `analyse`; there is no third path. This is the structural difference from the pre-round-7 defect,
> where the reader committed a zero count and then satisfied the idle test for ever.
>
> **No test is owed**, and not because it is hard: rule 1 binds bug FIXES, and this is a documented
> non-defect. Its precondition is the one `ScopeBuffer` structurally refuses to offer, since there is
> no way to rewind without publishing — deliberately, because that coupling IS invariant 1.
>
> **TWO EDITS MUST REOPEN THIS ENTRY:** a `ScopeBuffer::reset()` call site outside `prepare` (which
> would make the preemption window refillable and restore the wider bound), and reordering,
> splitting or interleaving the two `reset()` calls — or adding a third spectrum ring — which changes
> which rewind carries the happens-before edge and so changes the cross-ring analysis.

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
(the index is 0), `analyse` returned early **without touching the EMA** — it FLOORS the trace there
since round 7, which is half of what closed this — and the previous spectrum was
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

### KI-019 — Two spectrum concurrency tests asserted on interleavings they SEARCHED for rather than established (2026-09-07) — **CLOSED 2026-09-07 (0.2.12 round 18)**

Neither failure was ever a defect in the product. Both were assertions in `tests/state_tests.cpp`
that needed a particular interleaving to occur, and neither test made it occur: they ran the real
threads, hoped the scheduler would put one inside the other, counted the times it did, and asserted
the count was not zero. That is a test of the scheduler, and CI proved it twice in two different
environments before the design was corrected.

**What CI saw.** `specFrame`'s `mixed == 0` failed on the macOS x86_64 slice under Rosetta at
`abd209e3` and `ee32738d` and again at `f2babdc8`, passing at `f7fea2a7`, `20a9bd19` and twice at
`50cc099d`; the same universal binary's arm64 slice, native Intel (`macos-15-intel`) and Linux under
gcc, clang, LTO, ASan+UBSan and valgrind never failed it. `specStraddle`'s `guardFired > 0` failed
under valgrind memcheck on a GitHub `ubuntu-latest` runner in run 34134239185 attempt 2 —
**3 261 238 ticks, 6000 rewinds, 0 straddles** — while the same commit's attempt 1 had passed
memcheck an hour earlier and this container reaches the premise under memcheck in 99 rewinds with
exactly one straddle.

**Root cause, `specFrame`: the concurrent half was vacuous.** Its renderer thread read while four
thousand ticks published, but not one assertion in the function required the reader to have
overlapped a publication even once — every one of them holds for a reader that only ever reads
quiesced frames, so the test could take two hundred thousand reads without entering the state it
exists to check. `distinct > 1` measures publications BETWEEN reads, which is the opposite of the
overlap it was standing in for. What the sweep did do at that scale was expose the run to the
machine: ~4 x 10^8 float comparisons per run, against an environment that miscompares about one in
4 x 10^8 (below).

**Root cause, `specStraddle`: an open-loop search with a feedback signal that arrives too late.**
The window a rewind must land in is `tick`'s interior — after both ring windows are read and folded,
before the two generations are re-read. Land earlier and the reads come back short and
`onePairOneSpan` rejects the frame; land later and the tick has already committed. The producer
woke on "a tick has begun" and then swept a doubling spin and a yield count trying to land there,
with `guardFired` as its feedback — a signal that only becomes non-zero after the search has already
succeeded once. Natively the window is most of a tick and it converged on the first rounds; under a
cooperative scheduler it never converged at all.

**The deterministic architecture now used.** `SpectrumView` carries three rendezvous points, each
called at one place, each empty in every shipped build (ADR-0039 clause 12):
`whileHalfPublished` (counter odd, payload genuinely torn), `whileReadUncommitted` (a reader's copy
taken, closing check not yet run) and `whileBatchAnalysed` (both windows folded, generations not yet
re-read). The tests block the thread inside the bracket on a condition variable until the other
thread has done its half, so the interleaving is FORCED rather than raced — identically under a
preemptive scheduler, valgrind's cooperative one and binary translation.

`specFrame` now places a reader at all four states a publication has — before it, inside it with the
counter odd, across it, and after it — counts each placement separately in the branch that verified
that placement's own observable, and asserts all four are non-zero. It also asserts what the odd
marker actually buys: the refusal happens BEFORE the copy, so a reader's own buffers still hold the
last coherent frame and the torn pair is unreadable rather than read and discarded. `specStraddle`
forces exactly one straddle, on two real threads, and asserts the frame it produces has a full span
and both traces at the floor; its concurrent phase is now a fixed sixty rounds of stress with the
spin/yield sweep and the six-thousand-round hunt deleted, and `guardFired` demoted from a pass
condition to a diagnostic about the machine.

**What is NOT closed, and is not the same thing.**

1. **The execution-environment fault behind the Rosetta failures is real and remains.** At
   `f2babdc8` the instrumented test recorded `1 mixed, 0 working-pair splits; first mixed bin 202
   in=-112.685745 out=-112.685745 … re-read 0`: the writer's pair was bit-identical on every tick,
   the published pair read back equal immediately, and the two recorded values are themselves
   identical (`%.9g` round-trips a `float`). A comparison that disagrees with a reload of its own
   thread-local operands is not a state this program can be in. The redesign cuts `specFrame`'s own
   exposure from ~4 x 10^8 float comparisons a run to ~5 x 10^4 — about four orders of magnitude.
   That is an environment fact, not a test-design one, and it is why this entry is closed on the
   DESIGN and not on the observation.

   **Amended 2026-09-08 — that exposure figure is `specFrame`'s, and it does not bound the LANE.**
   The sentence above used to end "which makes the symptom vanishingly unlikely", which read as
   though the redesign had largely retired the fault. It had not, and the next occurrence proved it:
   at `4a5b71c` the Rosetta slice failed a DIFFERENT test — `specSpan`'s
   *"with neither tap advancing, both traces are held exactly"* — one check of 1423, in run
   34220696244 attempt 1.

   Everything about that failure says environment rather than defect, and one thing about it says
   MORE than the original observation did. The five commits since the last green run of this lane
   changed documentation only, so the bytes were the ones that had already passed it. The same run's
   native arm64 slice passed 1423/0 two minutes earlier, `macos-intel` (native x86_64) passed, and
   Linux, both LTO lanes, Windows, RealtimeSanitizer, ASan+UBSan and valgrind all passed. A re-run of
   the identical bytes (attempt 2) passed, and the suite passed 25/25 locally. And the assertion has
   no architecture-dependent path to fail on: the test pushes nothing to either ring between its
   snapshot and its two ticks, so both ticks hit `tick`'s idle gate — `committed == shownCommitted
   && gi0 == shownInGen && go0 == shownOutGen`, a `uint64_t` and two `uint32_t`s — and return before
   `analyse`. Neither trace is written, so the check is `exactlyEqual (x, copy_of_x)` over 4096
   float pairs with no store between them. **`specSpan` is single-threaded**, which is why this
   observation is stronger than the first: there is no concurrency left to blame.

   What follows for the record, and only this: the exposure reduction was a statement about ONE
   test's sweep, and the lane's risk is a property of the lane. Any test that compares floats
   bit-exactly carries some of it, and 117 `exactlyEqual` call sites in the state suite do. Nothing
   here changes what is appropriate: **no code workaround, no weakened assertion, no added retry,
   no skipped lane.** A test that reports a miscompare it cannot have produced is doing its job, and
   the response is to investigate the occurrence — as this one was — not to arrange for it to be
   unobservable.
2. **The forced placements prove the control-flow half of ADR-0039 and not the memory-model half.**
   Each rendezvous parks a thread on a mutex, and mutex release/acquire supplies happens-before
   edges strictly stronger than the seqlock's own annotations — so deleting the writer's release
   fence, the reader's acquire fence or the closing store's `release` is invisible to the forced
   phases. It is invisible to the stress too, and to every other test in the tree, and on x86-64 that
   is not an argument but a measurement: compiling `SpectrumView.cpp` with each fence removed and
   disassembling the two functions gives, for the writer's release fence and the reader's acquire
   fence, **textually identical machine code** — the mutant is the same program, so no test on this
   architecture can distinguish it. Demoting the closing store from `release` to `relaxed` changes
   the emitted code by exactly one instruction's POSITION (`movq %xmm0,%rax` moves one slot), which
   carries no ordering meaning under x86's TSO. On AArch64 the fences are real `dmb ish` instructions
   and the mutants are real; observing the reordering they prevent still needs the luck this round
   exists to stop depending on. Recorded as an unkillable mutant class rather than left to be
   discovered — see the table below.

**The round's mutation table.** Each mutant was compiled and the whole state suite run against it;
"killed by" counts only failures NAMED `specFrame` or `specStraddle`, since a mutant that merely
breaks some other test proves nothing about these two. `specPaint` — the round's other publication
test — is listed separately where it also fired, because it reads the same protocol through `paint`
and its agreement is corroboration rather than the claim.

| # | Mutation | Outcome |
| --- | --- | --- |
| M1 | `publishFrame` never stores the odd marker, so the bracket never opens | **Killed** — 3 `specFrame` failures (refusal, untouched buffers, four-placement coverage); `specPaint` also fires |
| M2 | `readPublishedFrame` copies the payload even with the bracket open | **Killed** — 4 `specFrame` failures, including the mixed-frame assertion itself; `specPaint` also fires |
| M3 | the reader's closing re-read always accepts | **Killed** — 7 `specFrame` failures across phases B and C (overtaken-once recovery, overtaken-twice refusal, both premises); `specPaint` also fires |
| M4 | the post-batch generation guard is deleted | **Killed** — 2 `specStraddle` failures (the floored straddle frame, and the premise that the run established it) |
| M5 | the generation guard floors only the input trace | **Killed** — 2 `specStraddle` failures |
| M6 | the straddle rendezvous no longer holds the tick — the MECHANISM that establishes the interleaving is removed, the rest of the test untouched | **Killed** — 2 `specStraddle` failures. This is the test failing because the ordering was not established, which is what makes the assertion non-vacuous |
| M7 | `specFrame`'s half-publication rendezvous no longer holds the writer inside the bracket — same removal, on the other test | **Killed** — 3 `specFrame` failures, the coverage assertion among them |
| M8 | the writer's release fence is deleted | **Survived** — and unkillable here: `objdump` of `publishFrame` is textually identical with and without it on x86-64 |
| M9 | the reader's acquire fence is deleted | **Survived** — same measurement on `readPublishedFrame` |
| M10 | the closing store is demoted from `release` to `relaxed` | **Survived** — the emitted code differs only by one instruction's position (`movq %xmm0,%rax`), which carries no ordering meaning under TSO |

M6 and M7 are the two that answer "does the test still pass if the thing that makes it deterministic
is taken away". They do not. M8-M10 are the class this entry declines to claim coverage of.

Evidence [Verified]:
- Source: `src/gui/SpectrumView.h` (the rendezvous banner and the assignment rule),
  `src/gui/SpectrumView.cpp` (the three call sites), `tests/state_tests.cpp`
- Test:   `testTheSpectrumsRendererNeverSeesHalfOfTwoFrames` (four placements, each counted),
  `testAResetThatLandsInsideATickNeverReachesTheScreen` (one forced straddle)
- Related: ADR-0039 clause 12 (2026-09-07)

### KI-020 — True-peak meters disagree near Nyquist, so "≤ 0.1 dBTP" is only as exact as the meter it is read on (2026-09-27)

**Severity:** Medium (delivery-spec exposure on programme with strong top-octave content)
**Status:** Confirmed, **documented limitation** — the yardstick is **decided (2026-09-27,
[ADR-0043](architecture/design-decisions/ADR-0043-dbtp-is-defined-on-the-product-meter-and-the-annex-2-filter.md))**:
the ≤ 0.1 dBTP promise is defined on the product meter and the BS.1770 Annex 2 filter
(`DSP_POLICY.md` invariant 4), so the libebur128 and long-kernel residuals below are
reference/compatibility measurements the product does not claim. The entry stays open for them and
for the STATISTICS TP row (VIS-002). *(Until 2026-09-27 the yardstick was an owner decision, audit
finding DSP-001 sub-item (a); the measurement below is what it was made on.)*
**Affects:** true-peak mode, all platforms/formats; worst on synthetic or heavily clipped programme
with energy in the last few percent below Nyquist, and after a large Post-EQ high shelf

"dBTP" is the maximum of the continuous waveform, and every meter approximates it. On the engine's
TP-mode output (ADR-0041, Accepted 2026-09-27) the ceiling holds on the product's own dBTP meter and on the
BS.1770 Annex 2 example filter — the two meters "dBTP" is defined on, and the two readings the clamp
is built to hold — worst **+0.005 dB**
over 2736 TP-mode configurations covering every oversampling cell. Two further meters still read a
residual:

- **libebur128** (a widely used BS.1770 implementation, 49-tap Hann interpolator): above the
  0.1 dB tolerance in **130 of 2736** configurations, worst **+0.18 dB** — HF-heavy and
  transient-heavy synthetic programme (most at the linear-phase oversampling cells) and the
  +6/+12 dB Post-shelf cases. An Ardour offline render of a hot test programme through the built
  plug-in read +0.09 dB over on it — inside the tolerance.
- **A 32×/128-tap Kaiser reference** (content up to ~0.47·fs): on a deliberately hot 56-configuration
  subset, worst **+0.98 dB**. Filtering the output to 20 kHz first makes it read HIGHER, not lower —
  the peak of near-Nyquist content depends on the reconstruction filter, which is why no meter is
  "the" truth there.

**The STATISTICS TP row can warn at the ceiling.** The row compares its hold with the ceiling
exactly (ADR-0020 Amendment 2), and the held product-meter reading of a TP-mode render sits 0.001 to
0.005 dB above the ceiling in 74 of the 2736 configurations (a gain that moves inside the
interpolation window) — inside the tolerance, printed equal to the ceiling at two decimals, and red.
Giving the row the SP row's half-print slack is the audit's VIS-002, an ADR-0020 amendment for the
owner.

Before ADR-0041 the same figures were +4.80 / +6.12 / +5.41 / +7.80 dB. The product meter itself
(`TruePeakEstimator`, 12 taps under a Blackman window) reads HF-rich programme up to ~1.4 dB below
the Annex 2 example filter — the same property, on the display side (DSP_POLICY invariant 11's
≤ 0.1 dB meter accuracy holds for the fs/4 test vectors only; `TruePeak.h`'s header records it).

**Workaround:** for a delivery checked on a long-kernel meter, set the ceiling ~1 dB below the spec
when the programme is clipped or HF-heavy; oversampling reduces the near-Nyquist content the clamp
has to catch.
**Cause:** finite interpolators, each accurate to a different frequency. The options weighed, with
their measured cost (worklog 2026-09-27; ADR-0043 took the first on 2026-09-27, and the others stay
available as an amendment of that record): define the promise on the product meter + Annex 2 (the
guard, now the definition); lengthen the clamp's accurate kernel to 64 taps (measured on the 16-phase prototype: reference
residual +0.93 → +0.36 dB on the same subset, at twice its lookahead share and CPU); also hold
libebur128's own interpolator (a prototype measured in the PR #42 review: libebur128 0 of 2736 over,
worst +0.004 dB, no latency change, the long-kernel reference unchanged at +0.98 dB, ~30 % more
detector CPU); or bring the meter's own estimator up to the accurate kernel so the display agrees
with the clamp.

Evidence [Verified]:
- Source: `src/dsp/TruePeak.h` (`TruePeakEstimator`), `src/dsp/ClampTruePeakDetector.h`
  (`ClampTruePeakDetector`, moved there in the PR #42 review with no change to its output)
- Test:   `testTruePeakModeHoldsTheCeiling` and `testTruePeakEngagementHoldsTheCeiling` (the two
  DEFINING meters, each checked on its own); the four-meter matrix is in the
  2026-09-27 worklog, not in the suite (libebur128 and the reference are external to the build)
- Decision: ADR-0043 (enacts `DSP_POLICY.md` invariant 4's definition); decision material:
  `docs/reports/2026-09-27-phase0-owner-decisions.md` §1, the definitions side by side with their
  measured consequences
- Commit: PR #42

### KI-021 — A factory preset turns TP, Dither and Noise Shaping off, and LOCK holds only the ceiling's NUMBER (2026-09-27)

**Severity:** Medium
**Status:** Confirmed — fix deferred to the owner (the core change widens ADR-0010's lockable set,
which is `{ceiling}` by an Accepted decision whose option I — a wider set — was rejected; an ADR and
the owner's sign-off are owed). Audit finding **STATE-002**.
**Affects:** all platforms/formats; every factory preset, on every load path (menu, ‹ ›, re-applying
Default), both views; a user preset saved with TP or Dither off does the same.

A factory preset is applied as "defaults + the preset's intents" over every non-excluded parameter,
and no factory table names `truePeakMode`, `dither` or `ditherShaping` — so every factory preset
sets TP **off** (the default since ADR-0015), Dither **off** and Noise Shaping **off**. With LOCK on,
the ceiling's VALUE is skipped and survives, but TP is not lockable: "−1.00 dBTP" becomes
"−1.00 dB", a sample-peak limit, and true peaks may pass it. A chosen 16-bit dither is switched off
without anything in the Simple view showing it. Undo restores all three.

**Workaround:** after browsing presets, re-engage TP (and Dither / SHAPE) before a delivery render —
or Undo back to the state you locked. The manual (§3.2, §7.3, the Presets FAQ) says so since this
round.
**Cause:** `PresetManager.cpp` factory apply (the defaults pass), `PluginParameters.cpp`'s exclusion
predicate, and ADR-0010's lockable set `{ceiling}`. The owner's options, from the audit: LOCK also
holds `truePeakMode` (a lockable-set change, ADR); a factory apply leaves the output rows untouched
(a preset-contract change, `PARAMETER_COMPATIBILITY_POLICY` rule 6); a visible cue when a preset
changes TP or dither (new UI copy, C8).

Evidence [Verified]:
- Source: `src/PresetManager.cpp` (factory apply), `src/PluginParameters.cpp` (exclusion predicate,
  the three defaults)
- Test:   none — no behaviour changed this round
- Commit: this round's PR (documentation only)

### KI-022 — Saving a preset over an existing name replaces that file without asking (2026-09-27)

**Severity:** Medium (permanent loss of a user preset not loaded in the current session)
**Status:** Confirmed, **documented behaviour** (USER_MANUAL §7.2: "Saving over an existing name
overwrites it") — a change is deferred to the owner. Audit finding **UX-003**.
**Affects:** all platforms/formats, the Save Preset panel

The Save panel writes `<user preset folder>/<name>.anabasis` with no existence check. The name
field opens prefilled with the current preset's name, all selected, so Return right after opening
replaces the loaded user preset (the intended one-keystroke update); a typed name that already
exists — or one that becomes an existing name once characters a file name cannot hold are stripped
— replaces THAT preset, with no prompt.

**Workaround:** keep copies of a preset library you care about (the folder is in §7.2); check the
name before pressing Save.
**Cause / why it is not changed here:** the silent overwrite is the inherited product-family
convention (Anamorph's manual documents the same), and `BRAND_CONSISTENCY_CHECKLIST.md` §A lists the
preset save flow as "must match" — a deviation needs an ADR and the owner's sign-off. There is no
platform overwrite prompt to reuse (the panel is the product's own overlay, not a file dialog), and
a confirm step needs new UI wording, which is the maintainer's (C8). The audit's recommended shape,
for that decision: prompt on every existing-target collision except the unedited prefill of the
currently selected user file, keyed on "the text was edited", guarded against Return auto-repeat.

Evidence [Verified]:
- Source: `src/gui/PluginEditor.cpp` (the Save panel's OK handler), `src/PresetManager.cpp`
  (`writeTo` replaces unconditionally)
- Test:   none — no behaviour changed this round
- Commit: this round's PR (documentation only)

### KI-023 — MATCH settles the processed signal slightly below the input's loudness (2026-09-27)

**Severity:** Low (a conservative listening-aid bias of a fraction of a LU at typical settings; no
rendered sample is affected)
**Status:** Confirmed, measured — the next MATCH item after ADR-0044. Audit finding **DSP-005**.
**Affects:** realtime monitoring with MATCH on, all platforms/formats; offline renders are unaffected

MATCH's gain is `min(measure, predict)`: the measure is the short-term dry − processed loudness, the
predict floor is the deterministic lift (input gain + limiter gain + the limiter's deepest recent
reduction). The floor counts only the LIMITER's reduction, so whenever another stage takes level out
it over-estimates the lift, and `min` keeps the too-deep floor once the measure converges: the
matched processed signal sits under the input. Since ADR-0044 put BYPASS at unity, this residual is
the whole of the BYPASS comparison gap. Measured 2026-09-27 on the real engine at the Loudness 70 %
point, pink noise (short-term / momentary, input − matched):

| Input level (pink, per channel) | Residual | With clip drive 0 | With the compressor idle |
|---|---|---|---|
| −17 dBFS RMS (dry −14.3 LUFS) | +0.63 LU S / +0.82 LU M | +0.30 LU M | +0.81 LU M |
| −12 dBFS RMS (dry −9.3 LUFS) | +1.73 LU M | +0.44 LU M | +1.39 LU M |

At these settings the **clipper's** level loss is the larger term — which contradicts the audit's
recommendation to add the compressor's reduction and drop the clipper term. The fix has to stay
inside ADR-0006 decision 7 (stateless, floor-only, attenuation-only) and must not turn MATCH into a
continuous AGC (DSP_POLICY invariant 10); both terms, and their per-block behaviour on transient
programme, need prototyping against the audit's acceptance criteria before a change is chosen.

**Workaround:** none needed for most judging — the bias is conservative (the processed signal is never
flattered). Where it matters, compare at moderate input levels, or read the input's and the output's
loudness directly.
**Cause:** the predict floor's "expected GR" term is the limiter's alone (`AnabasisEngine.cpp`, the
block-top predict; the GR-tap comment there stated the error direction backwards until 2026-09-27).

Evidence [Verified]:
- Source: `src/dsp/AnabasisEngine.cpp` (measure / predict at the block top; the GR tap comment)
- Test:   `testMatchedBypassIsLoudnessMatched` bounds the residual at 1 LU at the calibration point
  (measured +0.63 LU); the level sweep and the clip/compressor isolation are probe measurements in
  `worklogs/2026-09-27-phase1-match-statistics-observability.md`
- Commit: PR #42

### KI-024 — A reset or an unducked latch cuts the true-peak stream to zero at full gain (2026-09-28) — **route C FIXED 2026-09-28 (fourth round); the rest dispositioned**

> **Fourth-round disposition (2026-09-28, the PR #42 review of 0.2.15), on a reproduction of every
> route at engine level (44.1 / 48 kHz, 48 boundary positions per configuration, four programmes,
> EQ flat or a +12 dB shelf in either position, the Freeze states; the round's worklog §3).**
>
> **Two statements in the entry below are wrong, and are corrected here rather than erased:**
>
> 1. *"a host `reset()` mid-stream"* — **a host reset never reaches the engine.**
>    `juce::AudioProcessor::reset()` is empty and `AnabasisAudioProcessor` does not override it
>    (decided at P5: `THREAD_MODEL.md`, `MODE_AND_ADAPTATION_POLICY.md`, ADR-0011); the only caller
>    of `AnabasisEngine::reset()` is `prepare()`. The reviewer's harness called `engine.reset()`
>    directly, so the "reset" route is a host RE-PREPARE (route B below).
> 2. *"on the no-re-prepare route the render's first samples are the emptied pipeline's zeros"* —
>    **false whenever the EQ was in use**: the latch cleared the wet ring, limiter, clip, clamp and
>    oversampler but not the EQ, so the EQ's ring-out of the realtime audio reached the render —
>    Post position: ~13 samples at up to −1.0 dBFS inside the latency window; Pre position: up to
>    −1.2 dBFS in the head of the new audio, 637 samples after the cut at 44.1 kHz (676 at 48 kHz),
>    after the latency window, so in a host-trimmed file too. And the output dBTP tap read the step
>    into the emptied pipeline, so the session dBTP hold showed **+0.88 dB** over the ceiling for a
>    render whose file has no over. The reviewer's +0.96 / +0.98 dB was not reproduced; the worst
>    here is +0.884 / +0.918 dB (product meter / Annex 2), EQ flat.
>
> | Route | What happens | Disposition |
> |---|---|---|
> | **A** host `reset()` | nothing — a no-op by design; the pipeline continues | **Preserve** (overriding `reset()` would be a threading-model change, a hard stop) |
> | **B** host re-prepare | `prepare()` empties the pipeline and restarts every meter; the last 2–4 segments before the zeros read up to +0.884 / +0.918 dB — the same magnitude as any stream that simply ends on loud programme (+0.87 to +0.93 dB measured at the end of an uninterrupted run, up to +1.06 dB on Annex 2 in one). Nothing after the boundary is over, no pre-cut audio follows | **Preserve**. A checked decay at the cut was prototyped and **rejected**: it still reads +0.81 / +0.69 dB across the cut (the decay's check covers only the segments after the junction), and it puts ~6 ms of pre-cut audio into the render, whose file then reads +0.88 dB over at its first sample |
> | **C** offline entry that latches without a re-prepare (Force Max's factor; a TP / factor change on the entry block) | the EQ's ring-out of the realtime audio and the tap's step reading, above; and, found by the round's independent review, with **BYPASS** on the realtime input itself: the bypass leg's delay ring was not emptied, so the render carried the realtime input at −2.0 dBFS until sample 446 (44.1 kHz) / 485 (48 kHz) — the render latency − 1 (identical in 0.2.15) | **FIXED** — the latch now also restarts the EQ and the output dBTP tap (`a43094b`) and empties the bypass leg's ring (`f03d673`), as `prepare()` does (`AnabasisEngine.cpp`, the `! smoothersPrimed \|\| enteringOffline` branch). Measured: tap and hold ≤ +0.003 dB, no pre-cut audio in either EQ position or through the bypass leg, the file from the boundary unchanged (≤ +0.004 dB). **Not changed, and dispositioned with B:** the continuous stream's own step from the last realtime sample to the emptied pipeline's zeros, which reads what B's does (+0.884 / +0.918 dB on the last 2–4 segments before the zeros) — the render's file starts after it. Guard: `testAForceMaxEntryStartsTheRenderClean` (fails 3 checks unfixed — tap and hold +0.827 dB, old audio; the EQ reset alone or the tap reset alone each fails its half; its bypass case fails without the ring clear) |
> | **D** offline entry with no composition change; **Dd** the same with a forced duck in flight | the pipeline continues at unity (KI-004); read as a file the render's latency window carries pre-entry audio at the ceiling (+0.97 / +0.96 dB at its head, Annex 2); Dd's duck jumps to unity (+0.92 dB tap) | **Defer** to KI-004's owner decision: emptying the pipeline on every offline entry would change rendered samples on the no-re-prepare route and break `testOfflineEntryDropsTheEngagementTail` part (4), which pins the documented behaviour |
> | **E** offline → realtime with a composition change; **E0** without | E goes through the §2.8 duck; E0 is continuous | **Preserve** |
> | **Freeze** (ADR-0042) | acts only through `prepare()`'s stash and `resumeAfterReset`; none of the mechanisms above reads the trims — B and C measured with Freeze off, on for the whole run and engaged halfway: the same readings | no interaction |
>
> **Investigate further (outside this entry):** a clean, re-prepared Force Max render that starts on
> loud programme, trimmed by the reported latency, reads +0.40 / +0.50 dB over at its head in a
> fresh meter — the 16× oversampler's linear-phase response puts energy ahead of its nominal
> integer latency, so the trimmed file starts mid-waveform. The emitted stream is not over; the
> reading is the host's trim. Not investigated further in this round. **Open for ADR-0020:** whether
> an offline entry without a re-prepare should start a fresh statistics session (today the render's
> integrated reading, LRA and holds include the realtime playback before it).
>
> *The entry as recorded in the third round follows, unedited.*

**Severity:** Low (inter-sample readings straddling a host-drawn stream boundary; the render read as
a file starts from silence)
**Status:** Confirmed, measured, pre-existing (identical before and after 0.2.15), found by the
adversarial review of ADR-0045; not changed in the round that found it — the next true-peak item.
**Affects:** true-peak mode, a host `reset()` mid-stream, and entering offline with a composition
change (e.g. Force Max changing the factor) without a re-prepare

Two routes clear the pipeline without the §2.8 duck: `reset()`, and the offline-entry direct adopt
when the entry itself wants a latch (`latchOsConfig` at full gain — KI-004). The output steps from the
last emitted value to the emptied pipeline's zeros in one sample, and the true-peak readings whose
windows straddle that step read up to **+0.96 dB (product meter) / +0.98 dB (Annex 2)** over the
ceiling (44.1 kHz, a +12 dB Post shelf, hot programme; the reviewer's harness, the 2026-09-28
worklog) — the segments 2–4 samples before the zero run. The same step exists with TP off; true-peak
mode is where it is measured against a promise.

**Workaround:** none needed for a bounce — a host that re-prepares before rendering (most do) starts
from an empty pipeline, and on the no-re-prepare route the render's first samples are the emptied
pipeline's zeros, so the render read as a file carries no reading of the step (by construction; not
separately measured). The readings are in the stream the host itself ended.
**Cause:** neither route has a transition to fade — `reset()` is the host's, and the offline-entry
latch deliberately does not duck the head of a bounce (KI-004). Closing it means a checked decay at
the cut, as `EngagementTail` does for a TP engagement.

Evidence [Verified — the reviewer's harness]:
- Source: `src/dsp/AnabasisEngine.cpp` (`reset()`; the `! smoothersPrimed || enteringOffline` branch)
- Test:   none asserts it (a stream spanning a reset is not a render)
- Commit: PR #42 (recorded)

### KI-025 — Below 44.1 kHz a worst-case burst can read over the true-peak ceiling (2026-09-28) — **CLOSED 2026-09-28 (fourth round, ADR-0046)**

> **✅ CLOSED — fixed at every rate the true-peak path engages (12 kHz and up), the tolerance
> unchanged; below 12 kHz the path no longer engages.** Closed by
> [ADR-0046](architecture/design-decisions/ADR-0046-the-true-peak-clamp-eases-in-and-engages-from-12-khz.md)
> (on the owner's direction, ⊕ for review; ratified at the Architecture Review Gate on
> 2026-09-29), which replaces the clamp's boxcar attack ramp with one
> that eases in (geometric weights, a 16-sample floor), caps the release's rise at 1 % per sample,
> narrows what a revision reaches, stamps each frame with min(entry, predicted emission ceiling), and
> engages true-peak mode from 12 kHz through one predicate the Ceiling's unit shares.
>
> **The entry below under-stated the issue, and the correction is kept:** a longer search on 0.2.15
> (~350 hill-climbs, 656k engine evaluations, 64-sample bursts) found **every rate from 4 to 32 kHz
> over on a STATIC ceiling** — +0.214 dB at 8 kHz, +0.232 at 11.025, +0.197 at 16, +0.189 at 22.05,
> +0.188 at 24, +0.185 at 32 kHz (Annex 2) — where the table below lists +0.056 / +0.020 dB static
> at 22.05 / 32 kHz; 44.1 kHz read +0.095 dB, inside the tolerance with little to spare. And a
> Ceiling REVERSAL mid-ascent read +0.30 dB at 48 kHz at clamp level (ADR-0045's stamping), which
> the same record closes.
>
> **What holds now (measured and derived on the integrated tree; the fourth-round worklog §5):**
> over a 24 167-render engine matrix (12 kHz–768 kHz engaged, OS off–16× and Force Max, 13 Ceiling
> automation shapes, a lifecycle tier) the worst reading at every engaged rate is +0.0426 dB
> (product meter) / +0.0380 dB (Annex 2) — 0.2.15 read over 0.1 dB in 378 of the same renders, all at
> 32 kHz and below; engine climbs reach +0.0308 dB at 12 kHz; a derived bound on the retarget step is
> +0.0672 dB at 12 kHz (+0.1008 at 8 kHz, which is why the rail is at 12). The regression guard `testTruePeakModeHoldsTheCeilingBelow44k` fails 12 checks on
> 0.2.15 (the 22.05 / 32 kHz cut bursts +0.1566 / +0.1250 dB, the 16 kHz static burst +0.1157 dB, a
> 360-render matrix +0.1179 dB, the reversal premise, the rail) and passes.
>
> **The round's independent review found a defect in the fix, closed before this entry was
> pushed further (`f03d673`; ADR-0046's implementation note):** at attack lengths whose float
> weights summed to 1 + 1–3 ulp (88.2 / 384 / 768 kHz) an astronomical input (a forward minimum at
> 0, 1e30) drove the clamp gain to −1.19e-7 and the backstop clipped it to the ceiling — +1.85 dB
> over on the product meter where 0.2.15 was silent. The eased sum is now normalised by the float
> weights' own total and capped at 1 (`testTheClampSilencesAnAstronomicalInput`); the release cap
> and the entry-time stamp, which no check had caught reverted, are now pinned
> (`testTheClampReleaseRiseIsCapped`, `testTruePeakModeLagsAnAscentByTheEntryCeiling`). The engine
> matrix re-run on the fixed tree (14 200 renders, 12–192 kHz) is unchanged at its worst.
>
> **What remains, recorded:** (1) no all-input derived bound was obtained at any rate — the promise
> rests on a derived bound for inputs not already under reduction when a cut arrives, a search over
> requirement sequences with an exact inner maximiser for the rest, and clamp and engine searches;
> (2) at 3901–11999 Hz, 8 and 11.025 kHz included, true-peak mode is not available (the sample clip;
> the Ceiling reads dB) — a behaviour change from 0.2.15, where the path ran there with this
> entry's residual (`COMPATIBILITY_MATRIX.md` §Sample rates; OQ-020 for any wording beyond the unit
> (resolved 2026-09-29: the TP and Ceiling tooltips name the 12 kHz boundary instead of claiming
> dBTP; wording ⊕));
> (3) a finite input around +180 dBFS is outside what the clamp's float gain can resolve (KI-027,
> older than this entry).
>
> *The entry as recorded in the third round follows, unedited.*

**Severity:** Low (constructed bursts at sample rates below 44.1 kHz; every programme matrix holds)
**Status:** Confirmed, measured; found by an adversarial search against the 0.2.15 engine; the static
half pre-existing (bit-identical in 0.2.14), the automation half larger before 0.2.15. Not changed in
the round that found it — a true-peak item before Phase 1 resumes, with KI-024.
**Affects:** true-peak mode at host sample rates below 44.1 kHz — 22.05 and 32 kHz under a falling
Ceiling, and below 22.05 kHz with a static one

The clamp's gain law bounds each segment's interpolated peak on the assumption that the gain is the
same across the samples the interpolation reads. An attack ramp that starts a few samples after a
segment sitting just under the ceiling lowers some of those samples and, through the interpolation
kernel's negative lobes, RAISES that segment's peak. The effect scales with the ramp's slope — the
depth of the reduction over the attack length, which is 8 samples at 32 kHz and below — so it grows
as the sample rate falls. A search for worst-case bursts (a 32-sample pattern and its position,
hill-climbed on the real engine against both defining meters; the 2026-09-28 worklog) found, over the
live smoothed ceiling:

| Rate | Static ceiling | Under a 0 → −20 dB cut (the burst at the bottom of the glide) |
|---|---|---|
| 4 kHz | **+0.23 dB** (Annex 2) | the static figure |
| 8 kHz | **+0.11 dB** | the static figure |
| 16 kHz | **+0.12 dB** | the static figure |
| 22.05 kHz | +0.056 dB | **+0.157 dB** (Annex 2; product meter +0.033; 0.2.14 on the same burst +3.6 dB) |
| 32 kHz | +0.020 dB | **+0.125 dB** (Annex 2; product meter −0.004; 0.2.14 on the same burst +3.07 dB) |
| 44.1 / 48 kHz | +0.058 / +0.026 dB — inside the tolerance | nothing beyond the static figure |

The design ADR-0045 chose over its alternative (a clamp-only prediction without the limiter half) is
the smaller of the two here: that alternative reads +0.31 dB under its own search at 22.05 kHz, where
the shipped engine reads −0.76 dB on the same burst. At the bottom of a fast glide at 22.05 kHz the
ceiling falls ~1.5 % per sample, the limiter leaves the clamp overs of up to ~2.4 dB, and the clamp's
ramp is correspondingly steep.

**Workaround:** run the session at 44.1 kHz or above, or avoid fast full-range Ceiling cuts in
true-peak mode at 22.05 / 32 kHz.
**Cause:** the requirement `r[j]` and the windows built on it (`q`, `m`, the attack mean) keep every
gain a segment reads at or under that segment's requirement, which bounds the interpolated peak only
while those gains are equal. Closing it means either a longer attack at low rates (a clamp voicing
constant, ⊕, and a change to the true-peak path's delay composition — an ADR) or a requirement that
bounds the ramp's effect on its neighbours.

Evidence [Verified — adversarial search on the real engine; the reproducing bursts are kept with the worklog's scratch record]:
- Source: `src/dsp/CeilingClamp.h` (`processFrameTruePeak`: `r`, `q`, `m`, the attack mean)
- Test:   none asserts it (the regression tests run 44.1–192 kHz)
- Commit: PR #42 (recorded)

### KI-026 — At high rate × oversampling the limiter's release stops short of unity (2026-09-28)

**Severity:** Low (a level error under the ceiling, never over it; audible only as a small
permanent gain reduction after a limited passage)
**Status:** Confirmed, measured, pre-existing; found by the fourth PR #42 review round's sample-rate
audit. Not changed in that round — the limiter's numerics are a DSP change of their own.
**Affects:** the limiter at a high REGION rate (host rate × oversampling factor): 16× at 48 kHz and
up, any factor above 192 kHz, and every Force Max bounce (16× at any host rate)

After the limiter has reduced gain, its release should return the gain to unity once the programme
drops. At a high region rate it stops short and stays there: after a −6 dB hold followed by quiet
programme, measured on the whole engine (a −30 dBFS sine after a burst, trims frozen), the
residual reduction is **−0.012 dB at 48 kHz × 1, −0.20 dB at 48 kHz × 16, −0.92 dB at 192 kHz × 16
and −6.02 dB at 768 kHz × 16** with a 1000 ms manual release (−0.065 / −0.26 / −1.24 dB on AUTO).
The GR meters show the residual; the output is quieter than it should be, and the ceiling is
unaffected.

**Workaround:** a shorter release, or a lower oversampling factor for the realtime pass (Force Max
bounces always run 16×).
**Cause:** the release is a float one-pole on the ENVELOPE, `env += (needed − env) · aRel`
(`src/dsp/LookaheadLimiter.h`, `stepEnv`), and the per-sample step falls below half an ulp of `env`
once `(needed − env) · aRel` is small enough — so the envelope stops moving before it reaches
`needed`. The stall depends only on the region rate R = sr × OS (the coefficient shrinks as R
grows). Measured on the class alone, 20 s after a −6 dB hold: −0.011 dB at R = 44.1 kHz, −0.20 dB at
768 kHz, −0.92 dB at 3.072 MHz, −6.02 dB at 12.288 MHz (never releases) with a 1000 ms release. The
clamp's release had the same shape of problem and runs on the REDUCTION (1 − g) for that reason
(`CeilingClamp.h`, "The release runs on the REDUCTION"); doing the same here would change the
limiter's numerics everywhere, so it is its own decision.

Evidence [Verified — measured on the class and on the engine; the fourth-round worklog §4]:
- Source: `src/dsp/LookaheadLimiter.h` (`stepEnv`)
- Test:   none asserts it (no test runs 16× above 48 kHz with a long release)
- Commit: PR #42 (recorded)

### KI-027 — A finite input near +180 dBFS reads over the true-peak ceiling (2026-09-28)

**Severity:** Low (needs a finite sample around 1e9 — +180 dBFS — reaching the ceiling stage; no
converter, plug-in chain or file format a host passes produces that from programme)
**Status:** Confirmed, measured, pre-existing (0.2.15 reads the same class); found by the fourth PR
#42 review round's independent review of ADR-0046. Not changed in that round.
**Affects:** true-peak mode, a finite input so large that the clamp's required gain is below float's
resolution just under 1

The clamp's gain is `1 − reduction` in float. Just below 1 a float's step is 5.96e-8, so the smallest
gain above 0 the clamp can produce is 5.96e-8 (−144.5 dB); a required gain between 0 and that rounds
either to exactly 0 (silence) or to 5.96e-8 and above, which leaves an input of 1e9 at ~+35 dB over
the ceiling for the sample backstop to clip. The clipped waveform's inter-sample peaks then read over
the ceiling (derived from the float format; the readings below are measured). An input of 1e30 is
far enough past the resolution that the reduction rounds to exactly 1 — silence, at every rate
(`testTheClampSilencesAnAstronomicalInput` pins that, and pins the gain in [0, 1]).

Measured on the whole engine, TP on, −1 dBTP, default settings, a ±1e9 burst of 256 samples (the
product meter over the ceiling): **48 kHz +0.78 dB in 0.2.15, −5.25 dB since `f03d673`; 88.2 kHz
+0.89 dB in both; 384 kHz +1.81 dB in 0.2.15, +1.96 dB since; 768 kHz +1.85 dB in both.** With
Punchy and Transients 100 %, 0.2.15 read +1.64 dB at 48 kHz and was silent at 88.2 / 384 / 768 kHz;
the fixed tree is −8.53 dB at 48 kHz and silent at the others.

**Workaround:** none needed — keep programme in the ordinary range; a stage that emits +180 dBFS
upstream of the plug-in is itself the fault.
**Cause:** float gain resolution (above). Closing it means a gain domain with resolution at the
bottom — e.g. carrying the gain, not the reduction, near 0 — a clamp numerics change of its own.

Evidence [Verified — measured on the whole engine, 0.2.15 and the fixed tree; the fourth-round worklog §5.5]:
- Source: `src/dsp/CeilingClamp.h` (`processFrameTruePeak`: `gain = 1 − reduction`)
- Test:   `testTheClampSilencesAnAstronomicalInput` covers 1e30 (silenced) and the gain range at 1e9, not the 1e9 reading
- Commit: PR #42 (recorded)

### KI-028 — pluginval sometimes crashes after its `SUCCESS` line, at validator teardown (2026-09-28)

**Severity:** Medium (a release-gate failure on a platform without a crash-retry; whether a real
host is affected is unknown)
**Status:** Confirmed (observed in CI and locally), intermittent, not reproduced on demand, cause not
established. Recorded by the fourth PR #42 review round; not changed.
**Affects:** pluginval validation — observed on Linux (VST3, the `linux` job and local runs) and, once,
on macOS Intel (AU, the `macos-intel` job); every observation is after every test has passed

A pluginval pass prints `SUCCESS` for every test and then the validator process dies while it shuts
down:

- **Linux, VST3:** a segmentation fault at validator exit — seed `0xcb2cae` on `dd983ec` (the third
  review round's records), seed `0x37bc7e` on the fourth round's first build (1 of 29 replays with
  the editor tests, 0 of 3 without them, 0 of 8 under gdb). The Linux crash-retry in
  `scripts/run-pluginval.sh` (written for the X11 / XEmbed editor flake) passed each of these on
  retry, so CI stayed green.
- **macOS Intel, AU, randomise pass 3 / 3, seed `0x5161f59`** (push run 36495741278 on `fe29bda`):
  `libc++abi: terminating due to uncaught exception of type std::__1::bad_function_call` —
  an EMPTY `std::function` was invoked — and pluginval's own handler turned the abort into exit 9.
  macOS has no crash-retry by design, so the job failed. The plug-in's source at `fe29bda` is
  byte-identical to `f03d673`, whose `macos-intel` job passed all twelve passes (VST3 and AU, both
  modes ×3, other seeds); it is the first such failure on either macOS job in the last 100 push
  runs of `build.yml` (the others there: the `58107a4` build error, and three Rosetta self-test
  failures on 2026-09-07). A re-run of the failed job (attempt 2, job 109189497584) passed every lane —
  AU randomise ×3 on seeds `0x92afc7` / `0x3cc398d` / `0x7808ca6` — which shows the failure is
  intermittent, not that it is gone.
- **Local, Linux, VST3, the same seed** (`--randomise --random-seed 0x5161f59`, strictness 10, editor
  under Xvfb, the `fe29bda` build): 5 of 6 runs clean; 1 segfault INSIDE the Editor test (before
  `SUCCESS`) — the XEmbed flake the Linux retry exists for, not the exit crash.

What is known about the cause: every `std::function` the plug-in's own code invokes is either
null-checked at the call (`src/MacroEngine.cpp`, `src/InternalState.h`, the editor's controls,
`FrameClock`) or always initialised (`tickClockMs`); the processor's destructor stops the macro
drain before any member is destroyed; the two `SafePointer` lambdas are a menu's and a file
chooser's, which pluginval never opens. None of this rules the plug-in out: an empty `std::function`
invoked at teardown is also what a call through a destroyed object whose storage reads as zero looks
like, in the plug-in or in the validator's host code. Not investigated on macOS (no macOS here).

**Workaround:** none needed by a user as far as is known; for CI, a re-run.
**Cause:** not established — the validator's host code at shutdown, or the plug-in's teardown.
Establishing it needs a macOS reproduction with a symbolised crash report (the AU randomise lane, the
seed above) or the Linux exit crash under a debugger that catches it.

Evidence [Partially Verified — observed; not reproduced on demand]:
- Source: not identified (the candidates above were read and found guarded)
- Test:   none — pluginval's own teardown, not reproducible headlessly on demand
- Commit: PR #42 (recorded)

## Standing note for P1 onward

Two categories are known in advance to need entries in this project, from the sibling product's
experience. They are named here as **expectations**, not as claims that they already occur:

- **Anything that cannot be validated headlessly** — audio quality, GUI appearance, real-DAW host
  behaviour — is a documented coverage limitation, not an unknown. It belongs here the moment it
  blocks something concrete.
- **Host-specific behaviour** (parameter display, automation recording, editor resize, plugin
  rescan) is where most confirmed issues in a JUCE plugin end up. Record the exact host and
  version; "some DAWs" is not an entry.
