# ADR Index

Mandatory registry of all Architecture Decision Records for **Anabasis**. An ADR not listed here
is invalid.

ADRs are created **only** when supported by repository evidence (constraint C1) — the table below
is the evidence-confirmed result, not a predefined quota. New decisions append the next number.

Numbering is **local to this repository**: Anabasis ADR-0001 is unrelated to Anamorph ADR-0001.

Status values: Proposed · Accepted · Deprecated · Superseded.

| ID | Title | Status | Evidence confidence |
|---|---|---|---|
| [ADR-0001](ADR-0001-format-agnostic-dsp-core.md) | Format-agnostic DSP core via a POD `EngineParameters` snapshot | Accepted | Verified — `AnabasisDSP` links only `juce_dsp`/`juce_audio_basics` (CMake) and the whole `AnabasisTests` target builds without a wrapper header |
| [ADR-0002](ADR-0002-serial-signal-chain-and-clamp-placement.md) | Fixed serial signal chain; EQ Pre/Post the only mobility; **ceiling clamp always last before dither** | Accepted | Verified — `testLimiterPushDoesNotDriveTheClipper`, `testEqPositionsAreDistinct`, `testOutputNeverExceedsCeiling` in both EQ positions |
| [ADR-0003](ADR-0003-oversampling-scope-and-true-peak-measurement-tap.md) | Oversampling wraps Clip/Sat + Limiter; **true peak is a measurement tap** at >= 4x total | Accepted | Verified — `testTruePeakAccuracy`, `testLimiterTruePeakMode`, and `testOsLatencyMatrix` (the tap adds no delay) |
| [ADR-0004](ADR-0004-latency-contract-constant-lookahead-allowance.md) | Latency contract: reported = **constant max-lookahead allowance** + OS; no zero-lookahead position | Accepted | Verified — `testReportedLatencyMatchesImpulse` (every lookahead value), `testOsLatencyMatrix` (whole factor × phase matrix incl. Force-Max) |
| [ADR-0005](ADR-0005-macro-layer-architecture.md) | Macro layer: message-thread mapper, non-automatable macros, detach/re-engage coexistence | Accepted | **Verified** (P5, 2026-08-02) — `testMacroDefaultIsFixedPoint`, `testModeSwitchIsSoundNeutral`, `testDrainInsideRestoreIsSuppressed` pin the mapper and the restore; `testDetachAndReengageGrammar` pins the detach/re-engage gesture grammar (all three discriminator conditions, the mapping skip, re-engage-on-gesture, reset-to-macro, and the two mid-gesture overlap cases — each condition killed by its own mutant) |
| [ADR-0006](ADR-0006-ceiling-guarantee.md) | Ceiling guarantee: separate final clamp; monitoring never in the render path | Accepted | Verified — `testOutputNeverExceedsCeiling`, `testLoudnessCompensationDoesNotAlterRender`, `testDeltaMonitor` |
| [ADR-0007](ADR-0007-state-schema-v1.md) | State schema v1: explicit `schemaVersion`, raw-exact sessions, **per-slot adaptive state** | Accepted | **Verified** (2026-08-02) — byte-identical round trip, the §4.4 read rules and the ADAPTIVE child are pinned by `AnabasisStateTests`; the FROZEN_TRIMS inject half is wired by ADR-0014 and pinned by `testFrozenTrimRestore` |
| [ADR-0008](ADR-0008-build-architecture-and-plugin-identity.md) | Build architecture + plugin identity: CMake graph, JUCE 9.0.0 SHA pin, C++20, `RTec`/`Anbs` | Accepted | Verified — the five-target graph configures and builds green at the pinned SHA on Linux, pluginval L5 ×3 both modes |
| [ADR-0009](ADR-0009-code-reuse-from-anamorph.md) | Code reuse from Anamorph: copy-and-adapt with provenance, no shared module for v1 | Accepted | Verified — the one adapted unit (K-weighting, provenance in `LoudnessMeter.h`) reproduces the standard's −3.01 LKFS vector in `testLufsCalibration`; no shared module exists |
| [ADR-0010](ADR-0010-parameter-surface.md) | Parameter surface: 49 APVTS parameters, exclusion tiers, lockable set | Accepted | Verified — the frozen registry snapshot plus the 49/9 count checks in `AnabasisStateTests` |
| [ADR-0011](ADR-0011-threading-model.md) | Threading model: two threads, no workers, atomic + SPSC publication | Accepted | **Partially Verified** — every implemented edge is mapped in `THREAD_MODEL.md` and `REALTIME_SAFETY_AUDIT.md` audits the audio thread; the two questions it left open are OQ-014 and the off-thread `replaceState` race (KI-003), and ADR-0012 amends its permitted-path table |
| [ADR-0012](ADR-0012-staged-record-cross-thread-path.md) | GUI→Audio **bounded staged record** behind a release/acquire flag (ratifies the learned-target restore, OQ-015) | Accepted | Verified |
| [ADR-0013](ADR-0013-release-trim-reaches-auto-poles.md) | The §5.4 release trim scales the limiter's **AUTO release poles** by `2^octaves` (resolves OQ-016) | Accepted | Verified — `testAutoReleaseFollowsTheTrimScale`, mutation-verified against the fixed-constant alphas |
| [ADR-0014](ADR-0014-frozen-trim-restore.md) | Frozen trim vector **restored**: staged on ADR-0012's row, applied at the §2.8 duck's silent bottom (resolves OQ-013) | Accepted | Verified — `testFrozenTrimRestore`, every element killed by its own mutant (the ADR enumerates them) |
| [ADR-0015](ADR-0015-pre-ship-contract-refreeze.md) | **Pre-ship contract re-freeze**: the round-2 `ceiling`/`truePeakMode` defaults, the `int_meterTargets` removal, and the Ceiling's mode-aware unit | Accepted | Verified — `testTheCeilingAdvertisesTheUnitItEnforces`, the re-frozen `testRegistrySnapshot`, `testFactoryPresets` and the §4.4 read-rule checks |

ADR-0015 was taken on **2026-08-06**, ratifying contract changes the owner's round-2 directive
(2026-08-05) had already landed in PR #8 — the same ratification shape as ADR-0012/0013/0014, and
carrying `Verified` confidence for the same reason. It is the first ADR whose Context records that
the code preceded its own authority; the ADR says why that is not a precedent. Two of its three
changes are `ARCHITECTURE_REVIEW_GATE.md` items (a Serialization Registry change and a Parameter
Registry change), and the **owner cleared that gate explicitly on 2026-08-06**, naming all three —
the `int_meterTargets` removal, the `ceiling` default and the `truePeakMode` default. The
sign-off is quoted in the ADR's own Status banner, which is the record of authority; this row
exists so the index answers "was the gate cleared?" without opening the file. **It is the only
gate clearance in this repository so far**, and the only ADR that needed one.

## ADRs amended by a later ADR

An ADR is a historical record and is **not** rewritten when a later decision moves one of its
numbers. Where the later ADR only supersedes *part* of an earlier one, the earlier record keeps its
`Accepted` status and its original text, and carries an amendment banner pointing forward. This is
the registry of those, so a reader auditing an ADR knows before reading it whether any of it has
moved:

| Amended | By | What moved |
|---|---|---|
| **0006** | **0015** | The `ceiling` (⊕ −1.0) and `truePeakMode` (⊕ on) defaults quoted in its Context and option E. The clamp mechanism, the stage placement and the monitoring decisions are untouched. |
| **0010** | **0015** | The ten-field host-hidden inventory in its Decision — `int_meterTargets` was removed, so the set is nine — and the same two defaults. The IDs, the exclusion tiers and the lockable set are untouched. |

ADR-0013 and ADR-0014 were taken on **2026-08-02** under the owner's v0.1.0 blanket approval
(every human-review/owner-decision gate pre-approved to unblock the complete first version, with
the item-by-item fine review owed afterward). Both are flagged ⊕ accordingly; both carry
`Verified` confidence at authoring for the same reason ADR-0012 did — they ratify mechanisms in
the tree with mutation-verified tests.

ADR-0012 was taken on **2026-08-01** (owner decision on OQ-015, during P4) and carries
`Verified` confidence at authoring — unusually for a fresh ADR, because it ratifies a mechanism
already in the tree with mutation-verified tests, which is precisely why ratification was the
cheap option. It is the first ADR authored after the sign-off batch.

The first eleven were authored on **2026-07-31**, the date of the owner's sign-off on `docs/DESIGN.md`
(the P0 exit criterion, `DEVELOPMENT_BRIEF.md` §11). They were authored `Unverified` by
construction — at that date Anabasis had no `src/` and no `tests/`, so every runtime claim was a
contract the P1+ code had to satisfy rather than an observation. **That is no longer the state:**
P1–P4 landed the full chain and its two suites, and the confidence column below is upgraded per
ADR **against a named test**, never wholesale. An ADR reads `Partially Verified` where part of its
decision is still unwired — the row says which part.

That includes ADR-0008: its JUCE pin and identity codes were read from the **sibling** repository,
which is evidence about how *Anamorph* builds, not about how Anabasis does. "This pin configures
and builds here" becomes `Verified` at the first green P1 build and not before
(`DOCUMENTATION_COVERAGE.md`: never upgrade a confidence level without the evidence that justifies
it).

**Five of them amended a Policy** (`ADR_POLICY.md` rule 5 — a policy change is enacted by an ADR).
Three carry the amendment inside a decision item, two in a dedicated
*Policy amendments enacted by this ADR* section; both forms are equally binding, and this list is
the registry of record for "which rules were rewritten, and by what authority":

| ADR | Policy amended | What changed |
|---|---|---|
| **0002** | `DSP_POLICY.md` invariant 1 | Chain order prints the ceiling clamp as always-last-before-dither, in both EQ positions. **Hard Stop**, ratified at sign-off. |
| **0003** | `DSP_POLICY.md` invariants 2 and 5 | Closes the open point: true-peak detection is a measurement tap, so "oversampling off ⇒ no oversampling latency" holds unconditionally. |
| **0004** | `DSP_POLICY.md` invariants 2 and 8 | Reported latency is the constant lookahead allowance + OS; the latch sentence names factor **and** phase mode; invariant 8's click-free enumeration gains the phase mode, the lookahead, the colour model and the three named bulk swaps (preset load / A/B switch / undo step, each owed its own click test). **Hard Stop**, ratified at sign-off. |
| **0005** | `MODE_AND_ADAPTATION_POLICY.md` invariants 1, 4, 5 and 6; `PARAMETER_COMPATIBILITY_POLICY.md` rule 7 | Invariant 1 gains a named guard with its binary; invariant 4's lookahead bar is re-grounded off the lapsed latency derivation; invariants 5 and 6 are rewritten from "open decision" wording to the settled coexistence strategy and macro non-automatability; rule 7's macro-curve freeze is re-grounded on **recall**, not automation. (Only 1 and 4 are carried as prescribed verbatim blocks — 5 and 6 reproduce this ADR's own decision text.) |
| **0011** | `THREADING_POLICY.md` | The adaptive-engine placement clause stops deferring to a future ADR; a seventh permitted-path row is added for single-scalar sentinel commands; the PDC rule is restated as *off the audio thread* rather than the unsatisfiable *on the message thread*. |

An earlier revision of this paragraph said "three", counting only the `DSP_POLICY.md` amendments and
missing ADR-0005's and ADR-0011's entirely — which would let a reader auditing the sign-off conclude
that two rewritten rule documents had been changed without authorisation.

## Expected first batch (from P0 `DESIGN.md`) — delivered 2026-07-31

Retained as the record of what P0 set out to decide. Every item below is now an Accepted ADR
above; the list is no longer forward-looking.

- The fixed serial signal-chain order and the EQ Pre/Post placement (`DEVELOPMENT_BRIEF.md` §3).
- Oversampling strategy: which stages are wrapped, filter type, and the exact latency contract
  (§2, §4.3, §7).
- The true-peak ceiling guarantee: detector oversampling, the final safety clamp, and its
  tolerance (§4.3, §10).
- The Simple macro-layer architecture and the Simple ⇄ Advanced coexistence strategy
  (§5.3 / OQ-004).
- The state-schema shape: versioned root, subtree split, host-hidden session state (§2).
- The JUCE pin (version + immutable commit SHA) and the C++20 baseline (§2, §2.1 / OQ-001).
- The plugin identity codes and format set (§2 / OQ-003).
- **Code reuse from Anamorph** — which first-party GUI, wrapper/state and DSP sources are copied
  and adapted, and whether a shared module is extracted (`CLAUDE.md` §3 makes this ADR-mandatory;
  OQ-005).
- **The parameter surface** — IDs, ranges, defaults, choice orderings, the exclusion tiers and the
  lockable set (`ADR_POLICY.md` parameter semantics; `PARAMETER_COMPATIBILITY_POLICY.md` rule 6).
- **The threading model** (`ADR_POLICY.md`).

`docs/DESIGN.md` §10 carries the same mapping. Since sign-off this index — not that table — is
the registry of record (`ADR_POLICY.md` rule 1).

## How to add an ADR

1. Confirm the decision is backed by code/test/commit/PR evidence.
2. Use the field structure: **Status**, Context, Problem, Options, Decision, Consequences,
   Related code, Evidence + confidence. **Record the rejected options and why they lost** —
   with measurements where the decision was empirical (`ADR_POLICY.md`).
3. Assign the next sequential number; add a row here.
4. If the ADR changes a Policy or another ADR, mark the superseded record `Superseded`/`Deprecated`
   and cross-link.

## Template

```markdown
# ADR-NNNN — <decision in one line>

**Status:** Proposed | Accepted | Deprecated | Superseded by ADR-MMMM

## Context
<the situation that forced a decision>

## Problem
<what specifically had to be decided>

## Options
- **A. <option>** — <consequence>
- **B. <option>** — <consequence>. Chosen.

## Decision
<what was decided, precisely enough to implement against>

## Consequences
<what this costs, what it forecloses, what now depends on it>

## Related code
- `src/...:NN-MM`

Evidence [Verified | Partially Verified | Unverified]:
- Source: <file:lines>
- Test:   <test name>
- Worklog: worklogs/<file>.md   # the measurement trail, if the decision was empirical
```
