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

*(none — no build exists. The first entries arrive with P1.)*

## Standing note for P1 onward

Two categories are known in advance to need entries in this project, from the sibling product's
experience. They are named here as **expectations**, not as claims that they already occur:

- **Anything that cannot be validated headlessly** — audio quality, GUI appearance, real-DAW host
  behaviour — is a documented coverage limitation, not an unknown. It belongs here the moment it
  blocks something concrete.
- **Host-specific behaviour** (parameter display, automation recording, editor resize, plugin
  rescan) is where most confirmed issues in a JUCE plugin end up. Record the exact host and
  version; "some DAWs" is not an entry.
