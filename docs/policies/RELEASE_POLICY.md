# RELEASE_POLICY.md

Repository Governance Policy. Preconditions that must hold before a version ships.

## Preconditions (all required)

1. **Tests green** — the headless self-tests pass (`scripts/run-tests.sh`); pluginval passes at
   **strictness 10 in both modes ×3 on all three platforms** (`TESTING_POLICY.md` Levels 2–4).
2. **The §10 acceptance criteria are met** — the measurement gates in `TESTING_POLICY.md`
   (LUFS ≤ 0.1 LU, true peak ≤ 0.1 dB, ceiling never exceeded, latency exact, null test, aliasing
   and performance figures **measured and recorded**).
3. **Compatibility checklist passed** — every item in
   `procedures/RELEASE_COMPATIBILITY_CHECKLIST.md` is checked (parameter IDs unchanged or
   migrated, serialization verified, presets migrated, host matrix, latency, automation, session
   reload).
4. **Version bumped** — `CMakeLists.txt` `project(... VERSION x.y.z)` updated.
5. **CHANGELOG updated** — a dated entry per `CHANGELOG_POLICY.md`, evidence-cited.
6. **Architecture Review cleared** — if the release contains any `ARCHITECTURE_REVIEW_GATE.md`
   change, it has human sign-off and an ADR.
7. **Docs synced** — `DOCUMENTATION_LIFECYCLE_POLICY.md` triggers applied; `HANDOVER.md` status
   fields refreshed; `TEST_REPORT.md` current.
8. **Manual audition acknowledged (Level 5)** — the human sign-off. For a maximizer this is not a
   formality: the loudness-matched blind comparison across ≥ 5 genres and the DAW smoke tests
   (Reaper/Windows, Logic Pro/macOS AU) are release criteria. A green build is "ready to
   audition," not final.
9. **Licensing in place** — for any distribution beyond internal testing: the commercial JUCE tier
   (`OPEN_QUESTIONS.md` OQ-002), the product's own licence terms, and the third-party attribution
   files. An engineering-green build does not clear this; it is an owner action.

## Third-party attribution (required with every binary distribution)

`NOTICE` and `THIRD_PARTY_LICENSES.md` must **accompany every binary distribution**, not merely
exist in the repository: several licences JUCE vendors require their notice to travel with a
binary. They are produced at P6 against the actually-pinned dependency tree — an inventory copied
from another project is not evidence (constraint C7) — and must be **re-verified after any JUCE
version bump**, since the inventory is derived from the pinned tree.

## Versioning

`MAJOR.MINOR.PATCH`, starting at **0.1.0**, pre-1.0 (< 1.0.0 = pre-release line), plus a CI
build/dev number passed as `-DANABASIS_BUILD_NUMBER=${run_number}` and shown in the About box.

## Release artifacts

Per-push CI produces `Anabasis-<OS>` (loose files — a downloaded artifact extracts straight to the
payload) plus `Anabasis-<OS>-debug` symbol artifacts. From P6, pushing an annotated `vX.Y.Z` tag
additionally produces a **draft** GitHub Release carrying the exact staging trees CI built and
validated, with SHA-256 sums and a traceability manifest.

**Publishing the draft is a manual maintainer action** after precondition 8 — the pipeline cannot
ship a release on its own.
