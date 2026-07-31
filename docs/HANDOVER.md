# HANDOVER.md

Operational status snapshot for technical handover. Update on every release and at every phase
boundary (`docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md`). Facts are Verified from the
repository; fields with no repository evidence are marked `TODO` rather than invented
(constraint C7).

Snapshot **refreshed for the P0 design delivery (2026-07-30)** — `docs/DESIGN.md` now exists as a
`Proposed` document. **P0 itself remains open**: its exit criterion is owner sign-off, so this is
a mid-phase refresh, not a phase-boundary update (the phase-completion trigger and its three
required updates are described in Pending Tasks below).

The repository's starting point was the **bootstrap** — the migration of Anamorph's governance
system, documentation library, build/CI scaffolding and working conventions into a new, otherwise
empty Anabasis repository, together with the product brief (`docs/DEVELOPMENT_BRIEF.md`).

## Operational status

| Field | Value |
|---|---|
| **Current Version** | 0.1.0 (planned; not yet in a `CMakeLists.txt` — none exists). `CHANGELOG.md` has no released entry. |
| **Current Phase** | **P0 — research & design** (`DEVELOPMENT_BRIEF.md` §11). Exit criterion: `DESIGN.md` + `OPEN_QUESTIONS.md` signed off by the owner. |
| **Branch Strategy** | Feature branch → PR into `main`. CI builds every branch; `main` carries shipped versions. Release tagging convention: annotated `vX.Y.Z`, wired to a `release.yml` at P6. |
| **Build Status** | **No build exists.** There is no `CMakeLists.txt` and no `src/`. The three build/analysis workflows are guarded by a `preflight` job and self-skip until `CMakeLists.txt` appears. |
| **Test Status** | **No tests exist.** The gate activates at P1 (`docs/policies/TESTING_POLICY.md`). |
| **Release Status** | Pre-0.1.0. Nothing has ever left this repository, which is why the compatibility contract can still be shaped at zero cost (`COMPATIBILITY_POLICY.md` §"When the contract starts"). |
| **Known Blockers** | **Three items block P1**, all decisions rather than code: (1) owner sign-off on `DESIGN.md` — **drafted 2026-07-30, awaiting sign-off**; (2) **OQ-010** — lookahead 0/off position: `DESIGN.md` §3.4 recommends **no off position** (keep 0.5–10 ms exactly); the recommendation is ratified or overturned by the same sign-off; (3) **OQ-011** — the macOS deployment target, inherited unexamined and carrying a `TODO(P1)` in `.github/workflows/build.yml`; it is a user-visible support claim, so it is decided rather than guessed (C7), checked at P1 by plan. The JUCE pin (OQ-001) and the plugin identity codes (OQ-003) are **decided** and recorded; both must be written into `CMakeLists.txt` when it is created at P1. The JUCE licence tier (OQ-002) blocks commercial distribution but not development. This row must agree with every `Blocking P1` entry in `docs/OPEN_QUESTIONS.md` — check it there, not here, when adding one. |
| **Pending Tasks** | **P0 is NOT complete** — its exit criterion is owner sign-off (`DEVELOPMENT_BRIEF.md` §11), and `docs/DESIGN.md` is still `Proposed`. All P0 *work items* are delivered (2026-07-30): the Anamorph repository was read in full (evidence: `worklogs/2026-07-30-p0-anamorph-research.md`), every `OPEN_QUESTIONS.md` entry is resolved, recommended-in-DESIGN, or explicitly deferred with its phase, and `docs/DESIGN.md` exists (architecture, full 49-parameter table, draft macro curves, UI wireframes, 7-ADR proposal set). **The one remaining P0 item is the sign-off** — `DESIGN.md` §11 lists exactly what approval ratifies, including one **Hard-Stop** item (Post-EQ before the ceiling clamp, requiring ADR-0002 to amend `DSP_POLICY.md` invariant 1). Note for the next agent: the *phase-completion* trigger in `DOCUMENTATION_LIFECYCLE_POLICY.md` (→ this file + `DOCUMENTATION_COVERAGE.md` + the §13 phase summary) fires **at sign-off, not now** — delivering the work is not completing the phase. |
| **Roadmap** | P0 research & design → P1 skeleton (pluginval L5) → P2 DSP core → P3 metering engine → P4 Simple adaptive engine → P5 UI → P6 polish & release (pluginval L10, DAW matrix, docs). `DEVELOPMENT_BRIEF.md` §11. v2 candidates (codec preview, reference matching, dynamic EQ, multiband limiting) are out of scope — leave architectural room only. |
| **Ownership** | `TODO: no owner/team metadata in the repository. Requires project-owner input (OQ-009).` Company of record: RollyTech. |

## Critical dependencies (with version-lock reasons)

| Dependency | Pin | Version-lock reason |
|---|---|---|
| **JUCE** | **9.0.0** — immutable commit `f8f8864…` (OQ-001, decided 2026-07-30; same pin as Anamorph) | Framework for all DSP, parameters/state, GUI and plugin wrappers. An unpinned bump can silently change DSP/latency/state-ABI. Sharing the pin with the sibling product makes a JUCE-attributable difference between them impossible and makes a bump a product-family decision. A bump is a Build System change (ADR + Review). `docs/policies/DEPENDENCY_POLICY.md`. |
| **C++ standard** | C++20 (+ a non-blocking C++23 canary job) | Project baseline per `DEVELOPMENT_BRIEF.md` §2.1; raising it is a build-contract change. |
| **pluginval** | latest release (downloaded) | The conformance gate. Not vendored; fetched by `scripts/run-pluginval.sh`. Pinning it is a tracked improvement. |
| Linux system libs | distro (`scripts/setup-linux.sh`) | ALSA/JACK/X11/FreeType/GTK/mesa/**EGL**/xvfb for headless build + validation. |

## Relationship to Anamorph

Anamorph is a **read-only reference**. This repository inherits its governance system
(`SOURCE_OF_TRUTH`, the policy set, ADR discipline, the documentation-lifecycle trigger map), its
CI/CD shape, its testing conventions and its brand system, and may copy and adapt first-party code
from it. Anabasis **never modifies the Anamorph repository**. What is shared and what deliberately
differs is tabulated in `docs/DEVELOPMENT_BRIEF.md` §23.

## Documentation ownership (proposed RACI — confirm with project owner)

No team structure exists in the repository, so the following is a **proposed** mapping (governance
guidance, not asserted fact):

| Documentation area | Proposed owner |
|---|---|
| `docs/architecture/` (incl. ADRs, DSP, signal flow) | DSP / Audio engineer |
| `docs/architecture/PARAMETER_*`, `SERIALIZATION_*`, `STATE_*` | Whoever owns the parameter/state surface (compatibility-critical) |
| `docs/procedures/` (BUILD, CI_CD, PACKAGING, RELEASE) | Build / Release engineer |
| `docs/policies/` | Tech lead / maintainer (these are binding) |
| `POSTMORTEMS`, `KNOWN_ISSUES`, `FUTURE_RISKS`, `HANDOVER`, `OPEN_QUESTIONS` | Maintainer |

## First steps for a new maintainer (or a resuming agent)

1. **Re-scan the workspace** (constraint C4) — the filesystem is the authoritative execution
   state, not any chat history.
2. Read `CLAUDE.md` → `docs/SOURCE_OF_TRUTH.md` → `docs/policies/AI_AGENT_POLICY.md` (Hard Stop
   conditions).
3. Read `docs/DEVELOPMENT_BRIEF.md` — Part I is the product spec, Part II is the inherited
   engineering standard.
4. Read `docs/OPEN_QUESTIONS.md` before making any decision that looks like one of them.
5. From P1 onward: build + test locally per `docs/procedures/BUILD.md` / `TESTING.md`.
