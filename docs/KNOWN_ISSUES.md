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

### KI-001 — A/B slot switch is not click-safe in the P1 skeleton

**Severity:** Low
**Status:** Confirmed
**Affects:** all platforms, all formats (P1 skeleton builds only; no UI exposes A/B yet)

Switching A/B slots performs a plain bulk parameter swap with no transition
handling, so a switch during playback can produce an audible step. The §2.8
forced-duck transition layer (asymmetric raised-cosine duck requested *before*
every bulk swap) lands at P2; until then the swap is exactly the click-free-
invariant hole `DSP_POLICY.md` invariant 8's per-path test will catch.

**Workaround:** none needed in practice — no UI or host path triggers the swap
in the P1 build; the API exists for the state tests.
**Cause:** `switchToSlot` applies `applySlotToLive` directly (TODO(P2) marks
the duck call site).

Evidence [Verified]:
- Source: `src/PluginProcessor.cpp` (`switchToSlot`)
- Test:   `AnabasisStateTests` `testAbSlotsAndTiers` (exercises the swap, not its audibility)
- Commit: P1 skeleton commit (this change)

---

### KI-002 — Loudness Comp and Delta monitoring do nothing in the P1 skeleton

**Severity:** Low
**Status:** Confirmed
**Affects:** all platforms, all formats (P1 skeleton builds only)

The **Loudness Comp** and **Delta** toggles are carried through the parameter
surface and the engine boundary but the engine ignores both, so clicking either
has no audible effect. They are monitoring features that arrive with the
metering engine at P3 (`DESIGN.md` §2.7 loudness compensation, §2.6 delta
monitoring); the parameters exist now because the surface freezes at v0.1.0
(`PARAMETER_COMPATIBILITY_POLICY.md` rule 1) and adding them later would be a
`kVersion` bump.

**Workaround:** none — the features are not implemented yet, not broken.
**Cause:** `EngineParameters::loudnessComp` / `deltaMonitor` are populated by
`CachedParams::toEngine` but never read by `AnabasisEngine::process`.

Evidence [Verified]:
- Source: `src/dsp/EngineParameters.h` (fields), `src/dsp/AnabasisEngine.cpp` (no reader)
- Test:   none — there is no behaviour to assert until P3
- Commit: P1 skeleton

---

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
