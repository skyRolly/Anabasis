# SOURCE_OF_TRUTH.md

Defines documentation authority and conflict resolution for the Anabasis repository.
When two sources disagree, the **higher** source in the list below wins, and the lower
source must be corrected to match.

Inherited unchanged from Anamorph's governance model (`docs/DEVELOPMENT_BRIEF.md` §14).

## Authority order (highest → lowest)

1. **Source Code** (`src/**`) — the running behaviour. The ultimate ground truth.
2. **Verified Test Cases** (`tests/dsp_tests.cpp`, `tests/state_tests.cpp`) — executable
   assertions that pin behaviour the code must satisfy. A claim proven by code **and** a
   test is the strongest evidence available (`Verified`).
3. **ADR** (`docs/architecture/design-decisions/`) — the final, dated record of a
   design decision. An ADR records *why* a constraint exists and supersedes any
   descriptive document about the same topic.
4. **Policies** (`docs/policies/`) — invariant constraints (what may not change). A
   Policy outranks descriptive Architecture: where Architecture *describes* and Policy
   *forbids*, the Policy is binding.
5. **Architecture** (`docs/architecture/`) — descriptive system reference.
6. **Procedures** (`docs/procedures/`) — how to build, test, release, troubleshoot.
7. **README.md** — project façade / entry point. Lowest authority for technical detail.

> Note: ADR is the final *decision* record; Policy is the *enforcement* of decisions.
> An ADR may change a Policy, but only by an explicit new/updated ADR (see
> `docs/policies/ADR_POLICY.md`). Policy weight is higher than general descriptive
> Architecture because a Policy is a hard rule, not a description.

## Where the product brief sits

`docs/DEVELOPMENT_BRIEF.md` is the **owner-supplied product specification**, not a
generated document. It sits *outside* the numbered chain:

- It is the **input** the code and the ADRs are answerable to during P0–P6. Where the
  implementation deviates from it, that deviation needs an ADR and owner sign-off — the
  brief is not silently corrected to match the code.
- It is **not** evidence for a claim about current behaviour. Once code exists, a
  statement about what the plugin *does* cites source/tests, never the brief.
- Its Part II (the inherited engineering standard) is a **summary with pointers**. The
  binding versions of those rules are the files in `docs/policies/`; on any conflict the
  policy file wins and the brief's summary is corrected.

## Where `DESIGN.md` sits

`docs/DESIGN.md` is the **P0 design deliverable** (`DEVELOPMENT_BRIEF.md` §11, §24) — the
document that answers the brief and from which the first ADR batch is spawned. It sits *below*
the ADRs it produces, at the same rank as descriptive **Architecture** (level 5):

- **Before owner sign-off** it was a `Proposed` document with **no authority at all** — nothing
  could be implemented from it and no other document could cite it as settling a question. That
  phase ended on **2026-07-31**.
- **Since sign-off** it is a *ratified proposal*, not an enforcement document. The decisions in it
  become binding only through the ADRs it names (level 3) and the policies those ADRs amend
  (level 4). Where `DESIGN.md` and an `Accepted` ADR disagree, **the ADR wins** and `DESIGN.md`
  is corrected — the same relationship descriptive Architecture has with ADRs.
- It is **not evidence** for a claim about implemented behaviour. Once code exists, "what the
  plugin does" cites source/tests; `DESIGN.md` only ever states what the code was asked to do.
- Its own evidence base is the P0 research trail in `worklogs/`, which — per the rule below — is
  raw material, never authority.

The practical consequence: as P1–P6 land, `DESIGN.md` is **superseded section by section** by the
ADRs plus the descriptive architecture set (`ARCHITECTURE.md`, `LATENCY_MODEL.md`,
`PARAMETER_REGISTRY.md`, …). It is not deleted, and it is not maintained as a living spec; drift
between it and shipped behaviour is expected and is resolved in favour of the higher source.

## Scope: the other three documentation classes

The numbered order above governs the **developer documentation** chain. The other three
classes sit alongside it, not inside it:

- **User documentation** (`docs/user/`) is *derived*: it must describe what the code and
  developer chain establish, and on any conflict it is the user document that gets corrected
  (per `DOCUMENTATION_LIFECYCLE_POLICY.md`). It never serves as evidence.
- **Internal / testing documentation** (`SUPPORT.md`, `.github/ISSUE_TEMPLATE/`) is derived in
  the same way, with one addition: its statements about what a tester may and may not do
  restate the legal class and must not diverge from it.
- **Legal documents** rank in two groups:
  - `NOTICE` and `THIRD_PARTY_LICENSES.md` (added at P6) are **authoritative for third-party
    attribution and licence facts** — each claim cites the licence file it was read from — and
    change only through their own re-verification procedure. Where a developer doc disagrees
    with them on an attribution fact, the developer doc is corrected.
  - `EULA.md`, `PRIVACY.md`, `TRADEMARKS.md` concern **Anabasis's own terms**. `PRIVACY.md` is
    derived from the source and is corrected when the source disagrees. Any EULA draft is
    authoritative for nothing until approved by the owner.

No document in these three classes may be cited as evidence for a technical claim.

`worklogs/` sits **outside all four classes**: session-local investigation records (measurements,
rejected alternatives and why they were rejected). A worklog is raw evidence a *later* document
may cite; it is never itself a policy, an architecture doc or a decision record. Finalized
decisions graduate to ADRs.

## Conflict-resolution rule

If documentation and source code disagree:

1. **Report the drift** (do not silently overwrite — see `AI_AGENT_POLICY.md` / constraint C6).
2. The source code wins **unless** the disagreement is itself a code defect contradicting
   an `Accepted` ADR or Policy — in which case the code change requires Architecture Review
   (`docs/policies/ARCHITECTURE_REVIEW_GATE.md`) and may be the actual bug.
3. Apply the **smallest** correction that re-syncs the document, with an evidence citation.

## Confidence levels (used throughout `docs/`)

| Level | Meaning |
|---|---|
| **Verified** | Provable from current source code, or code + a test case. |
| **Partially Verified** | Supported by README / commit / PR / code comment, but not fully provable from current code alone. |
| **Unverified** | No sufficient factual evidence; could be true but unproven (e.g. real-DAW host behaviour, performance numbers). |
| **Not Supported** | A deliberate, evidence-backed exclusion (e.g. AAX format). Distinct from Unverified. |

## Evidence citation format

```
Evidence [Verified]:
- Source: src/dsp/LimiterEngine.cpp:472-899
- Test:   tests/dsp_tests.cpp :: testTruePeakNeverExceedsCeiling
- Commit: 6a24b82
```

At least one source is mandatory for any historical, design-decision, incident, risk, or
known-issue claim.

## Current state of the chain (P1)

Levels 1, 2 and 5 are **empty** — no `src/`, no `tests/`, no descriptive architecture set exists
yet — **except that `docs/DESIGN.md` now occupies level 5**, having been signed off on
2026-07-31 (see §"Where `DESIGN.md` sits"). Level 3 is populated: the Accepted set is whatever
`docs/architecture/design-decisions/ADR_INDEX.md` registers — this file names the LEVEL, never its
membership, so an accepted ADR cannot leave it stale (it read "ADR-0001…0011 are Accepted" through
three later acceptances).
Levels 1 and 2 stay empty until P1 lands `src/` and `tests/`, so every statement about Anabasis's
*runtime* behaviour remains `Unverified` by construction and must be written as such. The policies in `docs/policies/`
state the invariants the future code **must** satisfy; they carry no compliance evidence yet, and
every such section is marked `TODO (no code yet)` rather than claimed (constraint C7).
