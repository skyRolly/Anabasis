# HANDOVER.md

Operational status snapshot for technical handover. Update on every release and at every phase
boundary (`docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md`). Facts are Verified from the
repository; fields with no repository evidence are marked `TODO` rather than invented
(constraint C7).

Snapshot taken at the **P0 → P1 phase boundary (2026-07-31)**. `docs/DESIGN.md` was signed off by
the owner on that date, closing P0's exit criterion (`DEVELOPMENT_BRIEF.md` §11), and the eleven
ADRs it authorised are Accepted and registered. This is a phase-boundary update, so all three
`DOCUMENTATION_LIFECYCLE_POLICY.md` phase-completion targets are covered: this file, the coverage
audit, and the §13 phase summary (below).

The repository's starting point was the **bootstrap** — the migration of Anamorph's governance
system, documentation library, build/CI scaffolding and working conventions into a new, otherwise
empty Anabasis repository, together with the product brief (`docs/DEVELOPMENT_BRIEF.md`).

## Operational status

| Field | Value |
|---|---|
| **Current Version** | 0.1.0 (pre-release; `project(Anabasis VERSION 0.1.0)` in `CMakeLists.txt`). `CHANGELOG.md` has no released entry; the P1 skeleton is under `[Unreleased]`. |
| **Current Phase** | **P1 — skeleton** (`DEVELOPMENT_BRIEF.md` §11). **P0 closed 2026-07-31** on owner sign-off of `docs/DESIGN.md`. P1 exit criterion: pluginval **L5** passes. |
| **Branch Strategy** | Feature branch → PR into `main`. CI builds every branch; `main` carries shipped versions. Release tagging convention: annotated `vX.Y.Z`, wired to a `release.yml` at P6. |
| **Build Status** | **Builds green on Linux** (P1 skeleton, 2026-07-31): `CMakeLists.txt` per ADR-0008 (five targets, JUCE 9.0.0 @ the pinned SHA fetched via FetchContent, C++20, warning-free under the recommended flags), `src/` + `src/dsp/` + `src/gui/` exist. The `preflight` guard now takes its ready=true path, so the full 3-OS matrix runs in CI; Windows/macOS results arrive with the first CI run of this commit. The `docs` job continues to run on every push and gates nothing. |
| **Test Status** | **149 checks green on Linux**: `AnabasisTests` (93 — null-with-defaults bit-exact, impulse-at-allowance for four lookahead values, ceiling clamp, control/gain priming, limiter window coverage and alignment, smoothing of ceiling and lookahead, hostile-input finiteness, self-heal recovery, bypass null, EQ frequency response/smoothing/positions, the ADR-0002 post-shelf ceiling stimulus, compressor static curve/detectors/mix/two-stage auto release/sidechain HPF, clipper curve/compensation/ADAA aliasing/colour models/dynamic tame, true-peak accuracy, limiter link/styles/preserve/two-stage auto/detector HPF/dBTP mode) and `AnabasisStateTests` (56 — registry snapshot vs the frozen fixture, 49/9 counts, raw-exact byte-identical round-trip and its fixed-point precondition, structural-tolerance read rules, batched latency notification, corrupt/foreign no-op, macro fixed point, restore-vs-macro-drain, A/B tier behaviour, preset contract, cache mapping). **pluginval L5 green ×3 in both modes on Linux** — the P1 exit criterion holds locally; 3-platform confirmation is the first CI run. Re-count from the suites' own output when editing this row; it has gone stale once already. |
| **Release Status** | Pre-0.1.0. Nothing has ever left this repository, which is why the compatibility contract can still be shaped at zero cost (`COMPATIBILITY_POLICY.md` §"When the contract starts"). |
| **Known Blockers** | **One item blocks one P1 path.** **OQ-011 is Resolved** (10.13 deliberate; JUCE 9's documented floor is 10.11, read from the pinned tree — see `OPEN_QUESTIONS.md` §Resolved). **OQ-013** — how the four-scalar frozen trim vector crosses message → audio; it blocks **that restore path only**, and is a thread-model decision (Architecture Review Gate + ADR + Hard Stop), so no P1 code may wire `frozenTrims` until it is taken. CMake, the parameter surface, the POD boundary, the pass-through chain and the latency contract are all independent of it. Everything that previously blocked P1 is closed: `DESIGN.md` is signed off, OQ-010 (lookahead 0/off) and OQ-004/OQ-005 are Resolved with their ADRs. **OQ-002** (JUCE licence tier) blocks commercial distribution, not development. This row must agree with every `Blocking P1` entry in `docs/OPEN_QUESTIONS.md` — check it there, not here, when adding one. |
| **Pending Tasks** | **P1 skeleton steps 1–7 are DONE** (2026-07-31): CMake/ADR-0008, the 49-param surface + frozen snapshot, POD boundary + threading shape, pass-through chain + basic lookahead limiter on the constant allowance, schema-v1 state harness (frozenTrims/detachMask serialized; inject path untouched per OQ-013), the three P1 tests plus latency/ceiling/bypass/fixed-point/robustness, OQ-011 resolved. **P1 closure items:** (a) pluginval L5 on Windows + macOS — confirmed by PR #4's CI run (merged 2026-08-01); (b) `THREAD_MODEL.md` + `PARAMETER_REGISTRY.md` — **written** (2026-08-01, from ADR-0011/ADR-0010 with code citations); (c) KI-001 — recorded. **P1 is closed except the OQ-014 owner call** (MacroEngine guard atomics vs the THREADING_POLICY table — documentation question, blocks nothing in code). **P2 is in progress** per the brief §11: EQ → compressor → clipper/ADAA → limiter upgrades → oversampling/dither, per-module unit tests as the exit criterion. |
| **Roadmap** | P0 research & design → P1 skeleton (pluginval L5) → P2 DSP core → P3 metering engine → P4 Simple adaptive engine → P5 UI → P6 polish & release (pluginval L10, DAW matrix, docs). `DEVELOPMENT_BRIEF.md` §11. v2 candidates (codec preview, reference matching, dynamic EQ, multiband limiting) are out of scope — leave architectural room only. |
| **Ownership** | `TODO: no owner/team metadata in the repository. Requires project-owner input (OQ-009).` Company of record: RollyTech. |

## P0 phase summary (`DEVELOPMENT_BRIEF.md` §13)

Required at the end of every phase: a summary of changes, the plan for the next phase, and the
current risks.

**Changes.** The repository went from empty to a governed P0 deliverable. Anamorph's governance
system, documentation library, CI/CD scaffolding and working conventions were migrated (never
modifying that repository, `CLAUDE.md` §3); the full Anamorph source was read across five domains
with `file:line` evidence (`worklogs/2026-07-30-p0-anamorph-research.md`); `docs/DESIGN.md` was
produced, survived four review rounds plus two adversarial verification passes, and was signed off
on 2026-07-31. Eleven ADRs are Accepted and registered. Four `DSP_POLICY.md` invariants were
amended by ADR (invariant 1 by ADR-0002; invariants 2 **and 8** by ADR-0004; invariants 2/5's open
point closed by ADR-0003) — two of those were **Hard-Stop** items ratified by the owner, not by a
green build. ADR-0005 and ADR-0011 amended `MODE_AND_ADAPTATION_POLICY.md` /
`PARAMETER_COMPATIBILITY_POLICY.md` and `THREADING_POLICY.md` respectively — `ADR_INDEX.md`
carries the five-ADR amendment table, which is the registry of record for "which rules were
rewritten, by what authority"; this row is a summary of it, not a second count to keep in step.
Five open questions closed: OQ-001, OQ-003, OQ-004, OQ-005, OQ-010.

**The four decisions that shape everything downstream.** (1) True-peak detection is a
**measurement tap**, so oversampling-off costs no detector latency. (2) Reported latency is a
**constant 10 ms lookahead allowance** plus oversampling — chosen so that browsing presets or
A/B-comparing during playback never moves host PDC, at the cost of ~8 ms nobody asked for.
(3) The **ceiling clamp is always last before dither**, downstream of the Post-position EQ, or a
post-limiter shelf would escape the product's core guarantee. (4) Simple is a **macro layer over
real Advanced parameters** written from the message thread, with per-parameter detach and
re-engage-on-touch.

**Plan for P1 (skeleton, exit criterion pluginval L5).** The eight-step order is in Pending Tasks
above. The shape of it: build system first (ADR-0008), then the parameter surface and its frozen
snapshot (ADR-0010) because IDs and ranges become permanent contract the moment a build leaves the
repository, then the POD boundary and threading shape (ADR-0001/0011), then a pass-through chain
with a basic limiter that honours the latency contract (ADR-0004), then the state harness
(ADR-0007). No DSP quality work at P1 — that is P2.

**Current risks.** RISK-008 (the measurement-tap latency contract rests on a detector group-delay
bound verified only by arithmetic — the first impulse test at P2 settles it; ADR-0004's constant
allowance makes the fallback cheap). RISK-002 (the parameter surface freezes at v0.1.0 before P2–P4
have taught us what it should be — mitigated by keeping pre-0.1.0 builds internal). RISK-003 (the
ceiling guarantee is asserted, not yet proven). RISK-009 (variable-font licence gates the P5
typography direction). RISK-006 / OQ-002 (JUCE licence tier blocks distribution, not development).

**C++23 canary status** (§2.1 requires this every phase): **not yet running** — there is no code to
compile at C++23. Scheduled for P2 per OQ-006's recommendation (DSP core + tests, weekly schedule
plus `workflow_dispatch`).

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
