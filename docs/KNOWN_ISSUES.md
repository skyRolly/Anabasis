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

## Standing note for P1 onward

Two categories are known in advance to need entries in this project, from the sibling product's
experience. They are named here as **expectations**, not as claims that they already occur:

- **Anything that cannot be validated headlessly** — audio quality, GUI appearance, real-DAW host
  behaviour — is a documented coverage limitation, not an unknown. It belongs here the moment it
  blocks something concrete.
- **Host-specific behaviour** (parameter display, automation recording, editor resize, plugin
  rescan) is where most confirmed issues in a JUCE plugin end up. Record the exact host and
  version; "some DAWs" is not an entry.
