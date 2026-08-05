# COMPATIBILITY_POLICY.md

**Highest compatibility authority.** The unified compatibility contract for the plugin. This
policy governs `SESSION_COMPATIBILITY_POLICY.md` and `PARAMETER_COMPATIBILITY_POLICY.md` (its
subsets) and the latency contract.

## The contract

A user's saved session — in any host, from any prior shipped version — must reload to the same
sound, with automation and presets intact. The following are **absolutely prohibited** unless an
exception (below) is satisfied:

| Prohibited change | Why it breaks the field |
|---|---|
| **Parameter ID rename or removal** | Sessions/automation key by ID. |
| **Serialization field removal** | Old sessions lose state silently. |
| **Preset schema break** | Saved/factory presets stop loading correctly. |
| **Host-visible parameter semantic change** | Automation lanes now mean something different. |
| **Reported-latency behaviour change** | Host PDC desyncs; timing shifts. |
| **Automation behaviour change** | Recorded automation plays back differently. |
| **Macro-layer semantic change** | A recorded Simple-knob automation lane now produces different Advanced values (`MODE_AND_ADAPTATION_POLICY.md`). |

## The only exception

A prohibited change may proceed **only if all** of the following are satisfied:

1. an **ADR** records the decision (`ADR_POLICY.md`), and
2. a **migration plan** preserves old sessions (a read path / default for the old form), and
3. the **Release Compatibility Checklist** passes (`procedures/RELEASE_COMPATIBILITY_CHECKLIST.md`), and
4. the change clears the **Architecture Review Gate** (human review).

## When the contract starts

**The contract binds from the first build that leaves this repository** — the first tester build,
not the first public release. Anabasis has shipped nothing yet, which makes the pre-0.1.0 window
the *only* opportunity to get the parameter surface and the serialization schema right at zero
cost.

Practical consequences for P0–P1:

- The **full parameter table is settled in `DESIGN.md` before any parameter is created in code**
  (`DEVELOPMENT_BRIEF.md` §24). Adding a parameter later is cheap; renaming or removing one is not.
- The state schema is designed with **room to grow**: a versioned root, additive fields that
  tolerate absence, and an explicit place for host-hidden session state, so the first *necessary*
  extension is not also the first *breaking* one.
- The moment the first build reaches a tester, freeze a
  `tests/fixtures/parameter_registry.snapshot` and a session fixture for that format
  (`SESSION_COMPATIBILITY_POLICY.md` rule 3).

## Backward-compatibility paths that must be preserved

**None yet** — no state format has shipped. This section is a ledger: every time a state format
ships, its read path is added here and a frozen fixture is added to `tests/fixtures/`. A read
path, once listed, is never removed without the exception above.

## Subset policies

- **Parameters:** `PARAMETER_COMPATIBILITY_POLICY.md` + ledger `docs/architecture/PARAMETER_REGISTRY.md`.
- **Session/serialization:** `SESSION_COMPATIBILITY_POLICY.md` + ledger
  `docs/architecture/SERIALIZATION_REGISTRY.md`.
- **Latency:** `docs/architecture/LATENCY_MODEL.md` (latency changes require an ADR).

## Status taxonomy (for `COMPATIBILITY_MATRIX.md`)

Verified · Partially Verified · Unverified · **Not Supported** (a deliberate exclusion — for
Anabasis, **AAX** is Not Supported by decision: the brief's §2 format list carries no AAX entry
and its §14.3 names AAX as the canonical Not Supported example, and `README.md` states the
exclusion outright. It is not "unverified". The citation used to read "`DEVELOPMENT_BRIEF.md`
§2" bare, as if §2 stated it in words — §2 excludes AAX by omission, which is a different kind
of evidence; corrected 2026-08-05 when the same phrasing, copied into `COMPATIBILITY_MATRIX.md`
with invented quotation marks around it, failed adversarial verification).
