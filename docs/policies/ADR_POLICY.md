# ADR_POLICY.md

Repository Governance Policy. When a decision must be recorded as an ADR.

## Rule

Adding or updating an ADR (`docs/architecture/design-decisions/`) is **mandatory** for any change
that alters:

- DSP **signal flow** (stage order/placement),
- Parameter **semantics** (range/default/meaning of a host-visible param; ID changes),
- the **macro layer** (Simple → Advanced mapping, mode-switch semantics),
- **Threading** model,
- Plugin **format** support,
- **Build** architecture (CMake structure, JUCE pin, C++ standard baseline),
- **State serialization** (schema/fields),
- **Latency** behaviour,
- **Oversampling** strategy,
- a **DSP algorithm** replacement (swapping a module's core maths).

## ADR rules

1. Every ADR is registered in `ADR_INDEX.md`; an unregistered ADR is invalid.
2. ADRs are **evidence-driven** — created only when backed by code/test/commit/PR evidence
   (constraint C1). No predefined quota.
3. Required fields: **Status** (Proposed/Accepted/Deprecated/Superseded), Context, Problem, Options,
   Decision, Consequences, Related code, Evidence + confidence.
4. ADRs are **append-only**: never delete an ADR. A reversed decision adds a new ADR and marks the
   old one `Superseded` (cross-linked), or `Deprecated`.
5. ADRs are the **final decision record** and outrank descriptive Architecture (`SOURCE_OF_TRUTH.md`).
   A Policy change is enacted by an ADR.
6. Numbering is sequential (`ADR-NNNN`); the next number follows the highest existing. Anabasis
   numbers its own ADRs from **ADR-0001** — the sequence is **not** shared with Anamorph, and an
   Anabasis ADR number never refers to an Anamorph decision.

## Rejected options are part of the record

An ADR that lists only the chosen option is not reviewable. Record the alternatives that were
seriously considered and *why* they lost — with measurements where the decision was empirical. The
raw measurement trail lives in `worklogs/`; the ADR summarises it and cites it.

## Expected first ADRs (from P0)

The design phase is expected to produce, at minimum, decisions on: the fixed serial chain order,
the oversampling strategy and its latency contract, the true-peak ceiling guarantee, the
Simple-macro-layer architecture, the state-schema shape, and the JUCE pin. These are listed as an
expectation, not as a quota — an ADR is written when the decision is made and evidenced, not to
fill a slot (C1).

## Relationship to other policies

- An ADR is the entry condition for the **Architecture Review Gate** items and the
  **COMPATIBILITY_POLICY** exception.
- Creating/updating an ADR triggers the `DOCUMENTATION_LIFECYCLE_POLICY.md` doc-sync set.
