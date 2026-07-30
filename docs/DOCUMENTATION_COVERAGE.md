# DOCUMENTATION_COVERAGE.md

Permanent documentation-coverage audit. **Future contributors/AI must update this on every
documentation-affecting change** (`docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md`).

Coverage = how well the module/topic is documented. Confidence = strength of the evidence behind
that documentation (Verified / Partially Verified / Unverified / Not Supported).

**Last updated:** for the **OQ-001 / OQ-003 resolutions** (2026-07-30). Two blocking decisions
moved to `Resolved` in `OPEN_QUESTIONS.md` (entries are never deleted): the JUCE pin is **9.0.0 at
commit `f8f8864…`**, the same revision Anamorph pins, so the product line shares one framework
baseline; and the plugin identity is **`RTec` / `Anbs` / `com.rollytech.anabasis`**, with the
vendor code spelling RollyTech rather than the first product. Synced: README (§Requirements),
`DEPENDENCY_POLICY` (pin row + the shared-pin rationale), `BUILD.md` (toolchain, options table,
new §Plugin identity), `HANDOVER` (dependency row, Known Blockers — now one, `DESIGN.md`
sign-off), `DEVELOPMENT_BRIEF` §23.2. Both values must be written into `CMakeLists.txt` at P1 and
are frozen from the first build that leaves this repository. Anabasis pays nothing for the
identity decision because it has never built; the sibling product absorbs the one-time break
(Anamorph 0.9.1 / its ADR-0023 / its KI-016). No `src/` change — there is still no `src/`.

Prior: repository bootstrap — the migration of Anamorph's governance system,
documentation library, build/CI scaffolding and working conventions into a previously empty
Anabasis repository, plus the product brief (`docs/DEVELOPMENT_BRIEF.md`, Part I unchanged from
the owner-supplied prompt + an additive Part II recording the inherited engineering standard).
No `src/`, no `tests/`, no `CMakeLists.txt` — so every claim about runtime behaviour in this
repository is `Unverified` **by construction**, and the policies state invariants the future code
must satisfy rather than compliance it already has (constraint C7).

## Code-module coverage

| Module | Documented in | Coverage | Confidence |
|---|---|---|---|
| *(none — `src/` does not exist)* | — | — | — |

Rows are added as modules land. The planned module set and its responsibilities are listed in
`docs/REPOSITORY_MAP.md` §`src/`; that is a **plan**, not coverage.

## Documentation-set self-coverage (deliverables present)

| Tier | Files | Status |
|---|---|---|
| docs root | DEVELOPMENT_BRIEF, SOURCE_OF_TRUTH, REPOSITORY_MAP, OPEN_QUESTIONS, HANDOVER, DOCUMENTATION_COVERAGE, KNOWN_ISSUES, FUTURE_RISKS, POSTMORTEMS, BRAND_CONSISTENCY_CHECKLIST | Present |
| policies | 15 docs (incl. the Anabasis-specific `MODE_AND_ADAPTATION_POLICY`) | Present |
| procedures | BUILD, DEVELOPMENT, CI_CD, TESTING, RELEASE_PROCESS, RELEASE_COMPATIBILITY_CHECKLIST, TROUBLESHOOTING | Present (PACKAGING deferred to P6) |
| architecture | `design-decisions/ADR_INDEX.md` only | Skeleton — the descriptive set lands with P1–P2 |
| user | — | Deferred to P6 |
| worklogs | — | Empty (no investigation has happened yet) |
| root — developer/status | README, CHANGELOG, CLAUDE | Present |
| root — legal | — | Deferred to P6 (produced against a real dependency tree; copying another project's inventory would be invented evidence) |
| root — internal/testing | — | Deferred to P6 (SUPPORT.md ships with the first tester build) |
| .github | workflows/{build,codeql,msvc,dependency-review}.yml, dependabot.yml, ISSUE_TEMPLATE/{bug_report,config}.yml | Present (release.yml deferred to P6) |
| scripts | setup-linux, build, run-tests, run-pluginval.{sh,ps1} | Present |

## Known coverage gaps / TODOs

These are **deliberate**, not oversights. Each names what would close it.

- **No architecture set** — `ARCHITECTURE.md`, `SIGNAL_FLOW.md`, `DSP_GRAPH_REFERENCE.md`,
  `THREAD_MODEL.md`, `PARAMETER_REGISTRY.md`, `SERIALIZATION_REGISTRY.md`, `LATENCY_MODEL.md`,
  `REALTIME_SAFETY_AUDIT.md`, `COMPATIBILITY_MATRIX.md`, `DSP_ALGORITHMS.md`,
  `PERFORMANCE_BUDGET.md` all describe code that does not exist. Closed by P1–P2.
- **No ADRs** — ADRs are evidence-driven (constraint C1); the first batch comes out of P0's
  `DESIGN.md`. `ADR_INDEX.md` exists as the registry.
- **Policy compliance sections are `TODO (no code yet)`** in `REALTIME_AUDIO_POLICY`,
  `THREADING_POLICY`, `DSP_POLICY` and `MODE_AND_ADAPTATION_POLICY`. Closed as each phase lands,
  with evidence citations.
- **No performance or aliasing numbers anywhere** — and none may be written until measured with a
  recorded machine and methodology (constraint C2). Closed by `TEST_REPORT.md` at P2/P6.
- **No host (DAW) matrix** — requires manual testing. Closed by the P6 DAW smoke tests.
- **Legal / attribution class absent** — closed at P6 against the actually-pinned JUCE tree.
- **`docs/user/` absent** — closed at P6.

## Update protocol

On any change, set this file's "Last updated" to the new HEAD (or the change description before
the first tag) and adjust the affected rows. A new module → add a row; a new doc → add to
self-coverage; new measured data → upgrade the confidence. Never upgrade a confidence level
without the evidence that justifies it.
