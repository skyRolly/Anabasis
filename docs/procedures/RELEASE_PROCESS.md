# RELEASE_PROCESS.md

How a version ships. Preconditions are defined in `docs/policies/RELEASE_POLICY.md`; the hard
compatibility gate is `RELEASE_COMPATIBILITY_CHECKLIST.md`.

> **Status:** no release has happened, and `release.yml` lands at P6. This document is the
> procedure the P6 work implements and the maintainer follows.

## Pre-release checklist

1. **All `RELEASE_POLICY.md` preconditions satisfied** — tests green, the §10 acceptance criteria
   met *with measured numbers*, the compatibility checklist completed, version bumped, CHANGELOG
   dated and evidence-cited, Architecture Review cleared for any gated change, docs synced,
   Level-5 audition performed, licensing in place for anything beyond internal testing.
2. **Raise pluginval strictness to 10** in `.github/workflows/build.yml`
   (`ANABASIS_PLUGINVAL_STRICTNESS`) and confirm both modes ×3 pass on all three platforms.
3. **`TEST_REPORT.md` is current** — including the aliasing measurement, the performance figure
   with its benchmark environment, and the loudness-matched listening-test report across ≥ 5
   genres with its conclusions and identified gaps.
4. **`HANDOVER.md` refreshed.**

## Versioning

`MAJOR.MINOR.PATCH`, starting at **0.1.0**, pre-1.0 line. The CI run number is passed as
`-DANABASIS_BUILD_NUMBER=${{ github.run_number }}` and shown in the About box, so a tester's
"Version 0.1.0 build 137" identifies an exact CI run.

The version lives in **one** place — `project(Anabasis VERSION x.y.z ...)` in `CMakeLists.txt` —
and everything else (compile definitions, installer names, release metadata validation) derives
from it. A second hand-maintained copy is a future mismatch.

## Tagging + release pipeline (P6)

1. Bump the version in `CMakeLists.txt`; add the dated `CHANGELOG.md` section.
2. Merge to `main`.
3. Cut an **annotated** tag: `git tag -a v0.1.0 -m "Anabasis 0.1.0"` and push it.
4. `release.yml` fires on the `v[0-9]+.[0-9]+.[0-9]+` tag and:
   - **validates metadata fail-closed** — the tag, the `CMakeLists.txt` version and the
     `CHANGELOG.md` section must agree, and the tag must be annotated;
   - reuses `build.yml` via `workflow_call` — one build, the identical gates and artifacts;
   - creates a **draft** GitHub Release from the exact staging trees CI built and validated,
     with SHA-256 sums over all assets and a traceability manifest.
5. **Publishing the draft is a manual maintainer action** after the Level-5 audition. The pipeline
   cannot ship a release on its own.

`workflow_dispatch` on `release.yml` is a **rehearsal**: validate + full build, no release.

## Why the metadata validation is fail-closed

A tag that disagrees with the built version produces artifacts nobody can trace back to a source
state. Catching it at tag time costs one failed workflow; catching it after publication costs a
recalled release.

## After release

- Confirm the published assets carry the expected names, contents and (on Linux/macOS) their
  executable bits — the GitHub Actions artifact transport does not preserve Unix file modes, so a
  release pipeline that archives artifact trees must restore them and verify fail-closed.
- Update `HANDOVER.md` (release status, version, test counts) and `README.md` (status/version).
- Record the Level-5 audition result — it is a human sign-off and is not reproducible from CI.
- Freeze the compatibility baseline for the shipped format: the registry snapshot and a session
  fixture (`RELEASE_COMPATIBILITY_CHECKLIST.md` §Notes).
