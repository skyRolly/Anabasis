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

## INC-001 — KI-001 closed: discrete transitions ran unducked through the P1/P2 skeleton

**Symptom:** Switching A/B slots — and later the `eqPosition` rewire, the `colourModel` rewire
and the oversampling factor/phase latch — performed a plain swap with no transition handling, so
a switch during playback could produce an audible step. Recorded as KI-001 (opened at P1,
extended twice during P2) rather than observed in a host: no UI path triggered it yet.
**Root cause:** The §2.8 transition layer did not exist; each rewire landed at whatever output
gain the moment had.
**Fix:** The §2.8 asymmetric raised-cosine duck (~6 ms out / ~28 ms in) in `AnabasisEngine`:
engine-side discrete rewires (eqPosition, colourModel, OS factor/phase) are held in applied-state
fields and execute ONLY at the silent bottom, at a block boundary; wrapper bulk swaps (A/B,
preset apply, session load) call `requestForcedDuck()` BEFORE the swap — the
THREADING_POLICY momentary-request atomic — so the duck's envelope covers the smoothed glide.
The first block after prepare/reset adopts directly (a duck there would dip the head of every
render for no transition at all).
**Prevention:** `testDuckWrapsDiscreteRewires`, `testDuckWrapsOsLatch`, `testDuckOnWrapperRequest`
(DSP suite) and `testAbSwitchRequestsDuck` (state suite, wrapper wiring) — each mutation-verified:
unducked rewires, an ignored request atomic, and a removed wrapper call all fail named checks.
One defect was caught during the build: a spurious `1−p` in the duck-out phase inversion sent a
fresh duck to the bottom in one sample — the smoothness test caught it before commit.

Evidence [Verified]:
- Source: `src/dsp/AnabasisEngine.{h,cpp}` (DuckState machine, applied-config flow),
  `src/PluginProcessor.cpp` (the three requestForcedDuck call sites)
- Test:   the four tests above
- Commit: PR #5, §2.8 transition-layer commit

## Relationship to the other status files

| File | Holds |
|---|---|
| `FUTURE_RISKS.md` | hasn't happened; might |
| `KNOWN_ISSUES.md` | is happening; confirmed and open |
| `POSTMORTEMS.md` | happened; fixed, with the mechanism recorded |

Every fixed bug ships a regression test that fails on the old code
(`TESTING_POLICY.md` rule 1) — that test is what an entry here cites in its **Prevention** field.
