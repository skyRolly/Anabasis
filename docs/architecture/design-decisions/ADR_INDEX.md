# ADR Index

Mandatory registry of all Architecture Decision Records for **Anabasis**. An ADR not listed here
is invalid.

ADRs are created **only** when supported by repository evidence (constraint C1) — the table below
is the evidence-confirmed result, not a predefined quota. New decisions append the next number.

Numbering is **local to this repository**: Anabasis ADR-0001 is unrelated to Anamorph ADR-0001.

Status values: Proposed · Accepted · Deprecated · Superseded.

| ID | Title | Status | Evidence confidence |
|---|---|---|---|
| [ADR-0001](ADR-0001-format-agnostic-dsp-core.md) | Format-agnostic DSP core via a POD `EngineParameters` snapshot | Accepted | Unverified (no `src/` yet) |
| [ADR-0002](ADR-0002-serial-signal-chain-and-clamp-placement.md) | Fixed serial signal chain; EQ Pre/Post the only mobility; **ceiling clamp always last before dither** | Accepted | Unverified |
| [ADR-0003](ADR-0003-oversampling-scope-and-true-peak-measurement-tap.md) | Oversampling wraps Clip/Sat + Limiter; **true peak is a measurement tap** at >= 4x total | Accepted | Unverified |
| [ADR-0004](ADR-0004-latency-contract-constant-lookahead-allowance.md) | Latency contract: reported = **constant max-lookahead allowance** + OS; no zero-lookahead position | Accepted | Unverified |
| [ADR-0005](ADR-0005-macro-layer-architecture.md) | Macro layer: message-thread mapper, non-automatable macros, detach/re-engage coexistence | Accepted | Unverified |
| [ADR-0006](ADR-0006-ceiling-guarantee.md) | Ceiling guarantee: separate final clamp; monitoring never in the render path | Accepted | Unverified |
| [ADR-0007](ADR-0007-state-schema-v1.md) | State schema v1: explicit `schemaVersion`, raw-exact sessions, **per-slot adaptive state** | Accepted | Unverified |
| [ADR-0008](ADR-0008-build-architecture-and-plugin-identity.md) | Build architecture + plugin identity: CMake graph, JUCE 9.0.0 SHA pin, C++20, `RTec`/`Anbs` | Accepted | Unverified |
| [ADR-0009](ADR-0009-code-reuse-from-anamorph.md) | Code reuse from Anamorph: copy-and-adapt with provenance, no shared module for v1 | Accepted | Unverified |
| [ADR-0010](ADR-0010-parameter-surface.md) | Parameter surface: 49 APVTS parameters, exclusion tiers, lockable set | Accepted | Unverified |
| [ADR-0011](ADR-0011-threading-model.md) | Threading model: two threads, no workers, atomic + SPSC publication | Accepted | Unverified |

All eleven were authored on **2026-07-31**, the date of the owner's sign-off on `docs/DESIGN.md`
(the P0 exit criterion, `DEVELOPMENT_BRIEF.md` §11). They carry `Unverified` confidence by
construction: Anabasis has no `src/` and no `tests/`, so every runtime claim is a contract the P1+
code must satisfy, not an observation. Confidence is upgraded per ADR as its code and tests land.

That includes ADR-0008: its JUCE pin and identity codes were read from the **sibling** repository,
which is evidence about how *Anamorph* builds, not about how Anabasis does. "This pin configures
and builds here" becomes `Verified` at the first green P1 build and not before
(`DOCUMENTATION_COVERAGE.md`: never upgrade a confidence level without the evidence that justifies
it).

**Three of them amended a Policy** (`ADR_POLICY.md` rule 5 — a policy change is enacted by an ADR):
ADR-0002 and ADR-0004 each carried a **Hard-Stop** amendment to `DSP_POLICY.md` invariant 1 and
invariant 2 respectively, ratified by the same sign-off; ADR-0003 closed the open point in
invariants 2 and 5.

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
