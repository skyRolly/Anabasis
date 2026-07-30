# AI_AGENT_POLICY.md

Repository Governance Policy — the collaboration codex for AI agents (and the expectations for
humans). Read this **before** modifying any code.

## Before changing code (mandatory reading)

1. `SOURCE_OF_TRUTH.md` (authority order, confidence levels).
2. The relevant **System Policies**: `REALTIME_AUDIO_POLICY.md`, `THREADING_POLICY.md`,
   `DSP_POLICY.md`, `MODE_AND_ADAPTATION_POLICY.md`, and the **COMPATIBILITY_POLICY** family.
3. The relevant Architecture doc + any ADR governing the area you touch.
4. `docs/DEVELOPMENT_BRIEF.md` — the product specification the implementation is answerable to,
   and `docs/OPEN_QUESTIONS.md` for what is deliberately undecided.

## During work

- **Report drift, never silently fix it (C6).** If documentation and code disagree, state the
  drift with an evidence reference before editing; prefer the minimal correction.
- **Update docs incrementally (C5).** Smallest change that re-syncs; preserve hand-written
  content; do not regenerate a doc wholesale unless explicitly asked. If a structural rewrite
  seems necessary, **stop and ask**.
- **Re-scan the workspace before each phase / on resume (C4).** The filesystem is the authoritative
  execution state, not chat history. Continue incrementally; never regenerate existing work.
- **Never invent numbers (C2).** A performance, aliasing, loudness or latency figure without a
  recorded machine, configuration and methodology is worse than an honest `TODO`. This applies to
  `PERFORMANCE_BUDGET.md`, `TEST_REPORT.md`, and anything shown in the UI.
- **Mark unknowns `TODO`, do not invent them (C7).** No invented owners, risks, status fields, or
  third-party facts.
- **User-visible text is specified, never invented (C8).** Tooltips, control labels, menu items,
  dialog strings and every other piece of UI copy are product wording owned by the maintainer.
  Do not add, extend, reword, or translate UI text unless the task explicitly requests that
  text. Implementing a behaviour change does not license announcing it in the UI — new
  behaviour is documented in `CHANGELOG.md`/docs, not in unrequested interface strings.
- **Record investigations in `worklogs/`.** Measurements, the alternatives you rejected, and *why*
  you rejected them. A decision without its rejected alternatives is not reviewable.
- **Do not guess at an open question.** Anything in `docs/OPEN_QUESTIONS.md` is escalated, not
  assumed — in particular the signal-chain order, parameter ranges, plugin identity codes, and any
  structural deviation from Anamorph's UI (`DEVELOPMENT_BRIEF.md` §13).

## After changing code

- Follow `DOCUMENTATION_LIFECYCLE_POLICY.md` and update the triggered docs **in the same change**.
- Keep the self-tests green (`TESTING_POLICY.md`); add a regression test for any bug fix.
- Update `CHANGELOG.md` per `CHANGELOG_POLICY.md` for user-visible changes.
- At the end of each phase, submit: changes made, plan for the next phase, current risks, and the
  C++23 canary status (`DEVELOPMENT_BRIEF.md` §2.1, §13).

## Hard Stop conditions (stop and request human review)

The agent must **immediately stop and request Human Review** — not proceed — when it detects any
of:

- **Parameter ID changes** (rename/removal) detected.
- **Serialization schema changes** detected.
- **Threading model changes** detected.
- **DSP signal-order changes** detected.
- **Reported-latency changes** detected.
- **Simple/Advanced macro-layer contract changes** detected
  (`MODE_AND_ADAPTATION_POLICY.md` — a mode switch that alters the sound).
- **An existing Accepted ADR conflict** detected (the change contradicts an ADR).

These map one-to-one to the `ARCHITECTURE_REVIEW_GATE.md` items. A passing build/test/pluginval
does **not** clear a Hard Stop — only human review does.

## Cross-repository constraint

**Anamorph is a read-only reference.** This project may copy and adapt first-party Anamorph code,
documentation and CI, but must **never modify the Anamorph repository**. Sharing code between the
two products (e.g. extracting a common UI module) is a product-family decision recorded in an ADR
with owner approval — see `docs/OPEN_QUESTIONS.md` OQ-005 — never an ad-hoc edit across repos.

## Third-party code and assets

Before introducing any third-party code or asset, state its licence and get owner approval
(`DEVELOPMENT_BRIEF.md` §13). Copyleft-licensed code (GPL/AGPL in particular) must not enter this
codebase. Competing products are behavioural and visual **benchmarks only** — no reverse
engineering of any kind.

## Inherited operational constraints (from `CLAUDE.md`)

The repository's `CLAUDE.md` operational constraints (no background tasks / no PR-webhook
monitoring; output discipline; strict domain focus) remain in force and are not overridden by this
policy.
