# KNOWN_ISSUES.md

Confirmed limitations of the current build, with their workarounds. This file is
**tester-surfaced**: it is developer-authored but deliberately routed to testers, so entries are
written to be useful to someone holding a build, not only to a maintainer.

An issue is listed here only when it is **confirmed** — reproduced, or established from the code.
An unconfirmed report is not an entry (constraint C7); a *future* risk that has not yet
materialised belongs in `FUTURE_RISKS.md`; a resolved incident moves to `POSTMORTEMS.md`.

## Entry format

```
## KI-0NN — <one-line summary>

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
in `POSTMORTEMS.md`**, and its number is never reused.

## Open issues

## KI-001 — A/B slot switch is not click-safe in the P1 skeleton

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

## Standing note for P1 onward

Two categories are known in advance to need entries in this project, from the sibling product's
experience. They are named here as **expectations**, not as claims that they already occur:

- **Anything that cannot be validated headlessly** — audio quality, GUI appearance, real-DAW host
  behaviour — is a documented coverage limitation, not an unknown. It belongs here the moment it
  blocks something concrete.
- **Host-specific behaviour** (parameter display, automation recording, editor resize, plugin
  rescan) is where most confirmed issues in a JUCE plugin end up. Record the exact host and
  version; "some DAWs" is not an entry.
