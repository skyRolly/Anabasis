# POSTMORTEMS.md

Incident records: things that went wrong, why, how they were fixed, and what now prevents a
recurrence. An entry is written when a defect was **shipped, or nearly shipped, and cost real
time** — not for every bug.

The purpose is the last field. A postmortem whose "prevention" is "be more careful" has failed;
the prevention is a test, a policy, an ADR, or a CI gate.

## Entry format

```
## INC-0NN — <one-line summary>

**Symptom:** <what was observed, by whom>
**Root cause:** <the actual mechanism, not the proximate trigger>
**Fix:** <what changed>
**Prevention:** <the regression test / policy / gate that now blocks a recurrence>

Evidence [Verified | Partially Verified]:
- Source: <file:lines>
- Test:   <the regression test that would have caught it>
- Commit: <sha> / PR #NN
```

Numbering is sequential and permanent; entries are append-only and never deleted.

## Incidents

*(none — no build exists.)*

## Relationship to the other status files

| File | Holds |
|---|---|
| `FUTURE_RISKS.md` | hasn't happened; might |
| `KNOWN_ISSUES.md` | is happening; confirmed and open |
| `POSTMORTEMS.md` | happened; fixed, with the mechanism recorded |

Every fixed bug ships a regression test that fails on the old code
(`TESTING_POLICY.md` rule 1) — that test is what an entry here cites in its **Prevention** field.
