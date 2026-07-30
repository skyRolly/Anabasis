# ADR Index

Mandatory registry of all Architecture Decision Records for **Anabasis**. An ADR not listed here
is invalid.

ADRs are created **only** when supported by repository evidence (constraint C1) — the table below
is the evidence-confirmed result, not a predefined quota. New decisions append the next number.

Numbering is **local to this repository**: Anabasis ADR-0001 is unrelated to Anamorph ADR-0001.

Status values: Proposed · Accepted · Deprecated · Superseded.

| ID | Title | Status | Evidence confidence |
|---|---|---|---|
| *(none yet)* | | | |

## Expected first batch (from P0 `DESIGN.md`)

Listed as an expectation to steer the design phase, **not** as reserved numbers — an ADR is
written when the decision is made and evidenced, and takes whatever number is next at that time.

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
