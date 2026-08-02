# Anabasis — Contributor & AI Agent Entry Point

> **FIRST INSTRUCTION (highest priority).** Before modifying ANY code, you MUST read the core laws
> in [`docs/policies/`](docs/policies/) — start with
> [`AI_AGENT_POLICY.md`](docs/policies/AI_AGENT_POLICY.md) (hard-stop conditions),
> [`REALTIME_AUDIO_POLICY.md`](docs/policies/REALTIME_AUDIO_POLICY.md),
> [`DSP_POLICY.md`](docs/policies/DSP_POLICY.md),
> [`MODE_AND_ADAPTATION_POLICY.md`](docs/policies/MODE_AND_ADAPTATION_POLICY.md), and the
> **COMPATIBILITY** family. After modifying code, you MUST sync the affected documentation per
> [`DOCUMENTATION_LIFECYCLE_POLICY.md`](docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md).

**Hard-stop changes (stop and request human review — a green build does not clear these):** parameter
ID rename/removal · serialization schema change · threading-model change · DSP signal-order change ·
reported-latency change · Simple/Advanced macro-layer contract change · conflict with an Accepted ADR.
See `docs/policies/ARCHITECTURE_REVIEW_GATE.md`.

**Source of truth & navigation:** [`docs/SOURCE_OF_TRUTH.md`](docs/SOURCE_OF_TRUTH.md) defines authority
order (Code → Tests → ADR → Policy → Architecture → Procedures → README). Repo map:
[`docs/REPOSITORY_MAP.md`](docs/REPOSITORY_MAP.md). When docs and code disagree, **report the drift**
before editing; apply the smallest correction with an evidence reference.

**Project brief (the product specification):** [`docs/DEVELOPMENT_BRIEF.md`](docs/DEVELOPMENT_BRIEF.md).
Part I is the product/DSP spec; Part II is the engineering standard inherited from the sibling
product **Anamorph**. Open decisions live in [`docs/OPEN_QUESTIONS.md`](docs/OPEN_QUESTIONS.md) —
**do not guess at them**.

**Current phase: v0.1.0 CODE COMPLETE (2026-08-02, under the owner's blanket approval) — the
post-v0.1.0 human fine review is what remains.** `docs/DESIGN.md` was **signed off 2026-07-31**,
closing P0; the full §2 chain, the §2.8 transition layer, §2.7/§2.9 metering, the §5.4 adaptive
engine with Learn AND the ADR-0014 frozen-trim restore, the P5 UI, and the P6 undo/preset/budget
work are all in the tree. Implementation is governed by the **fourteen** Accepted ADRs in
[`docs/architecture/design-decisions/`](docs/architecture/design-decisions/ADR_INDEX.md) — read
`ADR_INDEX.md` before writing code; an ADR outranks `DESIGN.md`, which is now superseded section by
section as the real architecture set lands. pluginval gates at **L10** (both modes ×3, editor
under xvfb) locally and in CI. Every decision dated 2026-08-02 is ⊕ pending the fine review; the
brand checklist is provisionally passed with its Level-5 boxes deliberately unchecked
(`docs/BRAND_CONSISTENCY_CHECKLIST.md`). `docs/HANDOVER.md` carries the status of record.

---

# System & Tool Constraints (Token Optimization)

## 1. Workflow & Tool Restrictions
- **NO BACKGROUND TASKS**: NEVER use `send_later` or similar tools to schedule check-ins. Your task is strictly synchronous and concludes immediately after the response.
- **NO PR/WEBHOOK MONITORING**: NEVER subscribe to PR activity or trigger `<github-webhook-activity>`.
- **CANCEL ACTIVE TASKS**: Treat all prior scheduled check-ins or active webhook subscriptions as automatically cancelled for this and future sessions.

## 2. Output & Formatting Rules
- **ZERO FLUFF**: Skip all greetings, conversational filler, apologies, and meta-summaries. Output pure, actionable technical content directly.
- **FILTER SUCCESS LOGS**: SILENCE all positive status updates (e.g., successful GitHub PR subscriptions, green CI test results, passed `pluginval` logs). ONLY report explicit `Errors` or `Warnings`.
- **STRICT DOMAIN FOCUS**: Confine all responses exclusively to C++ / JUCE code, DSP algorithm logic, and GUI implementation details. Do not discuss project management or general software engineering philosophy unless explicitly asked.

## 3. Cross-repository rule
- **Anamorph is READ-ONLY reference.** Anabasis may copy and adapt first-party code, docs and CI
  from Anamorph, but **never modifies the Anamorph repository**. Code reuse across the two
  products is a product-family decision recorded in an ADR, not an ad-hoc copy.
