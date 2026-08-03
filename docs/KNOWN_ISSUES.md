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

**Workaround:** none required on the hosts tested so far — no case of an
off-message-thread restore has been observed against this plugin. The entry
exists because the assumption is load-bearing and undocumented elsewhere.
**Cause:** the state-restore thread is a host contract, not a plugin choice;
`THREADING_POLICY.md` names the audio and message threads and does not state
which one restores state.

Evidence [Partially Verified]:
- Source: `src/MacroEngine.h` (`ScopedRestore`), `src/PluginProcessor.cpp`
  (`setStateInformation`, `switchToSlot`, `applyPresetFile`)
- Test:   `AnabasisStateTests` `testDrainInsideRestoreIsSuppressed` — models the
  mid-restore drain single-threaded; the uncovered `replaceState` race is not
  reproducible headlessly
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

### KI-006 — A sample-rate change silently drops a frozen slot's adaptation from the AUDIO while the readout and the save still report it

**Severity:** Medium
**Status:** Confirmed (fix deferred — it is a Freeze-semantics decision, not a repair)
**Affects:** all platforms/formats. Trigger: Freeze ON with a latched trim
vector, then any `prepareToPlay` — a host sample-rate or block-size change.

`AnabasisEngine::prepare` calls `AdaptiveEngine::prepare` → `reset()`, which
zeroes the internal `trims` struct along with the features. The PUBLISHED trim
atomics are NOT zeroed, and cannot be: `finishBlock` only publishes inside
`if (! freeze && audible)`, which is exactly what "Freeze latches the vector"
means. So after the re-prepare the engine applies a zero trim vector to the
audio while `publishedTrim*()` — the Advanced overlay, and the ADR-0014 save
capture — still report the latched one. The slot's serialized `FROZEN_TRIMS`
therefore stays correct; the sound does not match it until the vector is
restored by some later path (an A/B switch back, a session reload).

**Found by** the adversarial verification pass over review round 24
(2026-08-03), not by the review itself; it PREDATES ADR-0014 (P4 shipped the
same reset), which is why it is recorded rather than folded into that round's
fixes.

**Why it is not simply "keep the trims across reset".** That is the likely
resolution — the trim vector is a bounded, rate-independent control value, not
signal state, and carrying it would also make an un-frozen re-prepare re-slew
from where it was instead of jumping to zero — but it changes what
`MODE_AND_ADAPTATION_POLICY` invariant 3's Freeze clause promises across a
discontinuity, which is an owner/ADR call, not a bug fix. The alternative
(re-stage the vector from the wrapper at `prepareToPlay`) only works when
`liveFrozenTrims` holds one, i.e. after a load — a vector latched live in this
session has no copy to re-stage from.

**The SAVE half of the same gap, added 2026-08-03 (review round 27).** The description above is
about the audio; the capture has the mirror-image problem. `saveSlotFromLive` reads
`publishedTrim*()` whenever Freeze is on and no restore is pending — and on an instance that was
prepared but has never PROCESSED a block, those atomics are all zero. Such a session serialises an
all-zero `FROZEN_TRIMS` for a slot the user believes holds a latched vector, and the next load
injects zeros. The two halves want one answer: if the resolution is "the trim vector survives a
re-prepare", the published atomics stay meaningful and the capture is right as written; if it is
"a re-prepare drops it", the capture needs a has-ever-published discriminator. Do not settle one
half alone.

**For the post-v0.1.0 fine review.**

### KI-007 — Three preset/Freeze bookkeeping edges the fine review must settle together

**Severity:** Low (each is display or recall bookkeeping; none changes a rendered sample on its own)
**Status:** Recorded — raised by review round 25 (2026-08-03) and deliberately NOT fixed in that
round, because each is a semantics question rather than a defect, and two of them are the same
question KI-006 asks.

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

3. **Undo/redo do not restore `presetBaseline`.** They restore the whole SLOT tree while
   `applyFactoryPreset` / `applyPresetFile` / `savePresetFile` all reset the dirty datum, so
   undoing a preset apply restores the state but keeps the applied preset's baseline: the top
   bar's dirty mark can read wrong until the next apply or save. Display-only. The clean fix is to
   decide whether the baseline belongs IN the StateSet (a schema change — ADR-0007, Hard Stop) or
   whether undo should recompute it; that is why it is recorded rather than patched.

4. **The preset menu holds a raw pointer to an editor-owned LookAndFeel.** `showPresetMenu` calls
   `m.setLookAndFeel (&lnf)`, and `lnf` is an editor member; the async callback was given a
   `SafePointer` in round 24 but the menu's look-and-feel was not covered by it. If a host tore the
   window down while the menu was open, the menu window could outlive `lnf`. JUCE dismisses menus
   in most teardown orders, so it is not clearly reachable — and both available repairs carry their
   own risk (`dismissAllActiveMenus()` in the editor destructor also closes another instance's
   menu; a shared static LookAndFeel trades this for static-destruction order at DLL unload), which
   is why it is recorded rather than patched under a "no new bugs" round.

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
