# DOCUMENTATION_LIFECYCLE_POLICY.md

Repository Governance Policy. Defines which documents must be updated when code changes — the
trigger map that keeps docs and code in sync (prevents documentation rot) — plus the
documentation-only triggers below.

## Core rule

Documentation is updated **incrementally** alongside the code change in the same unit of work.
Apply the **smallest** change that re-syncs the doc; preserve hand-written content; never
regenerate a file wholesale unless explicitly requested (constraint C5). Before editing an
existing doc, run a **drift check** and report any code/doc disagreement (constraint C6).

## Trigger map (change → docs to update)

| Code change | Update these |
|---|---|
| **DSP algorithm / module maths** | `DSP_ALGORITHMS.md`, `SIGNAL_FLOW.md` (if order/placement), `DSP_GRAPH_REFERENCE.md`, an **ADR**, `CHANGELOG.md` |
| **Signal-flow / stage order** | `SIGNAL_FLOW.md`, `DSP_GRAPH_REFERENCE.md`, `DSP_POLICY.md` (if an invariant), **ADR**, `CHANGELOG.md` |
| **Add/remove/rename a parameter** | `PARAMETER_REGISTRY.md`, `PARAMETER_REFERENCE.md`, `PARAMETER_COMPATIBILITY_POLICY.md` (if contract), the frozen registry snapshot (only for an INTENTIONAL change), **ADR**, `CHANGELOG.md` |
| **Macro mapping / mode behaviour** | `MODE_AND_ADAPTATION_POLICY.md` (if an invariant), `PARAMETER_REGISTRY.md` (macro is part of the surface), **ADR**, `CHANGELOG.md` |
| **Adaptive engine (features, time constants, Learn/Freeze)** | `DSP_ALGORITHMS.md`, `MODE_AND_ADAPTATION_POLICY.md`, **ADR**, `CHANGELOG.md` |
| **State serialization schema** | `STATE_SERIALIZATION.md`, `SERIALIZATION_REGISTRY.md`, `SESSION_COMPATIBILITY_POLICY.md`, **ADR**, `CHANGELOG.md` |
| **Threading / cross-thread path** | `THREAD_MODEL.md`, `THREADING_POLICY.md`, **ADR** |
| **Latency behaviour (lookahead or oversampling)** | `LATENCY_MODEL.md`, **ADR**, `CHANGELOG.md` |
| **Oversampling strategy** | `LATENCY_MODEL.md`, `DSP_ALGORITHMS.md`, the oversampling **ADR**, `CHANGELOG.md` |
| **Metering (LUFS / dBTP / PLR / GR history / targets)** | `DSP_ALGORITHMS.md`, `TEST_REPORT.md` (accuracy figures), `USER_MANUAL.md`, `CHANGELOG.md` |
| **Build / CMake / JUCE pin / C++ baseline** | `BUILD.md`, `CI_CD.md`, `DEPENDENCY_POLICY.md`, **ADR** |
| **CI workflow** | `CI_CD.md`, `TESTING.md`; **also** `TESTING_POLICY.md` when what a gate REQUIRES changes, and `REPOSITORY_MAP.md` when a script is added or removed. The two-file row let 0.1.4's citation gate land while four other files went on describing `source-lint` as one script. |
| **Packaging / signing** | `PACKAGING.md`, `RELEASE_PROCESS.md` |
| **New/changed test** | `TESTING.md`, `DOCUMENTATION_COVERAGE.md`, the invariant→test map in `DSP_POLICY.md` |
| **Plugin format** | `COMPATIBILITY_MATRIX.md`, `COMPATIBILITY_POLICY.md`, **ADR**, `CHANGELOG.md` |
| **Ship a version** | `CHANGELOG.md`, `HANDOVER.md`, `README.md` (status/version) |
| **Fix a notable incident** | `POSTMORTEMS.md` (new INC), `KNOWN_ISSUES.md` (remove if it was listed) |
| **New unresolved limitation** | `KNOWN_ISSUES.md` and/or `FUTURE_RISKS.md` (new RISK) |
| **Complete a phase (P0–P6)** | `HANDOVER.md`, `DOCUMENTATION_COVERAGE.md`, the phase summary (`DEVELOPMENT_BRIEF.md` §13) |

## Documentation-only triggers

Not every documentation obligation starts with a code change:

| Change | Update these |
|---|---|
| **Add / remove / reclassify a document** | `docs/REPOSITORY_MAP.md` (tree entry), `docs/SOURCE_OF_TRUTH.md` (class list), `README.md` §Documentation, `docs/DOCUMENTATION_COVERAGE.md` (self-coverage) |
| **Resolve or add an open question** | `docs/OPEN_QUESTIONS.md` (never delete an entry — move it to `Resolved` with the decision, date and ADR) |
| **Product-model / licensing / legal-status change** | the affected legal document, `docs/KNOWN_ISSUES.md`, `docs/FUTURE_RISKS.md`, `README.md` §Licensing |
| **Brand/UI change touching a shared element** | `docs/BRAND_CONSISTENCY_CHECKLIST.md` |

## Audit obligation

On every documentation-affecting change, update `DOCUMENTATION_COVERAGE.md` (the persistent
coverage audit). A future agent must keep it current.

## Enforcement

Documentation drift discovered during work must be **reported, not silently corrected** (C6).
This policy is invoked by `AI_AGENT_POLICY.md` after any code change.
