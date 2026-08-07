# ADR-0021 — The release pipeline and installer set land at 0.1.1; attribution moves to the release page

**Status:** Accepted (2026-08-07 — owner directive of 2026-08-06, 0.1.1 round item 16: bump the
version one patch level and release it; "apply the same installer-building workflow as Anamorph,
with the installers included in the artifacts, and the artifact layout/content matching
Anamorph's — e.g. NOT containing NOTICE / THIRD_PARTY_LICENSES.md")

> **ARCHITECTURE REVIEW GATE — CLEARED (2026-08-07).** Two gate-adjacent items: this record
> **amends `RELEASE_POLICY.md`** (an `ADR_POLICY.md` rule 5 act — a policy change is enacted by
> an ADR) and it **overturns the OQ-007 resolution** of 2026-08-02, which deferred the pipeline
> and installers to "the first commercial release". Both are the owner's own call, made in the
> directive quoted above, under the round's standing sign-off instruction. No serialization,
> parameter, threading, DSP-order or latency contract is touched.

## Context

`OQ-007` (Resolved 2026-08-02) decided v0.1.0 would ship plain zips and that the release
pipeline and installers would wait for the first commercial release, alongside `OQ-002` (the
JUCE licence tier) and `OQ-012` (validating stripped/signed bytes). `packaging/` existed as an
empty directory and three `build.yml` staging steps carried a comment saying so. There was no
`release.yml`.

The 0.1.1 directive supersedes that: the owner wants the release cut now, with the sibling
product's installer pipeline and with **artifact contents matching the sibling's** — naming the
attribution files as the specific difference to remove.

That last clause is the substantive one, because it collides with `RELEASE_POLICY.md`
§"Third-party attribution", which required `NOTICE` and `THIRD_PARTY_LICENSES.md` to "accompany
every binary distribution" and which `build.yml` implemented by copying both into all three
staging trees.

## Problem

1. What does "accompany every binary distribution" mean once there are five distribution
   carriers (three zips, an Inno installer, a `.pkg`) rather than three?
2. Does removing the files from the archives weaken the obligation?

## Options

- **A. Keep copying both files into every staging tree, and additionally attach them to the
  release page.** Rejected: it contradicts the directive's explicit parity requirement, and it
  is *still* incomplete — the `.pkg`'s components are the three bundles and the Inno payload is
  deliberately lean, so neither installer would carry them either way. The policy would remain
  satisfied on three carriers out of five while reading as though it were satisfied on all.
- **B. Version-named release-page assets as the SOLE carrier — the sibling's model, whole.**
  **Chosen.** One carrier that *every* distribution route passes through, and the files are
  unambiguously identified: `Anabasis-<version>-NOTICE.txt` cannot be mistaken for another
  build's once extracted, which an unversioned `NOTICE` loose in a zip can. This is a change of
  *where* the obligation is met, not *whether*.
- **C. Add a fourth `.pkg` component and an Inno `[Files]` entry so every carrier holds them.**
  Rejected: it makes the installers offer a "Licences" component to tick, which is not a thing
  a user installs, and it still leaves the ambiguity option B removes.

## Decision

1. **`packaging/` is populated**, ported from the sibling under ADR-0009 with identity
   substituted: `linux/{INSTALL.txt,install.sh,uninstall.sh}` (VST3 → `/usr/lib/vst3`,
   standalone → `/usr/local/bin`, root-checked), `windows/{INSTALL.txt,Anabasis.iss}` and
   `macos/{INSTALL.txt,build-pkg.sh}` (three `pkgbuild` components →
   `com.rollytech.anabasis.{vst3,au,app}`, an explicit `distribution.xml` with
   `customize="allow"` and all three `start_selected="true"`, plus the sibling's own
   post-build self-checks). **The Inno `AppId` is a NEW GUID**, never the sibling's: Inno keys
   upgrade and uninstall on it, so a shared AppId would make either product's installer
   uninstall the other.

2. **`build.yml` stages the installer inputs and builds the installers.** Each staging step
   copies its `packaging/<os>/INSTALL.txt` (Linux additionally installs the two scripts with
   `install -m 755`, because a zip round-trip does not preserve the mode bit). Two new steps run
   ISCC and `build-pkg.sh`, both gated on the SAME validation checkpoint the customer zip is
   gated on — repacking a tree that failed validation would launder a rejected build into an
   installer. Both read the version from `CMakeLists.txt`, never from a workflow literal.
   Uploads: `Anabasis-Windows-installer`, `Anabasis-macOS-installer`.

3. **`release.yml` lands**, ported whole: `validate` (annotated tag ⇄ `project(Anabasis VERSION
   …)` ⇄ a **dated** `## [x.y.z] — YYYY-MM-DD` CHANGELOG heading, all fail-closed) → `build`
   (`uses: ./.github/workflows/build.yml`) → `draft-release` (executable-bit restore, per-entry
   `zipinfo` permission check, version-skew check on the installers, SHA256SUMS, a build
   manifest, CHANGELOG excerpt as notes, `gh release create --draft --verify-tag`). Publishing
   stays a human action — the draft is the pipeline's last step, per `RELEASE_POLICY.md`'s
   Level-5 audition precondition. `workflow_dispatch` is a rehearsal that never creates a
   release.

4. **`RELEASE_POLICY.md` §"Third-party attribution" is amended**: "accompany every binary
   distribution" is satisfied by **version-named release-page assets**, and the files are
   deliberately not copied into the archives, the `.pkg` or the Inno payload. The section states
   both reasons. The `-debug` artifacts stay out of the release entirely — they are per-push CI
   outputs for symbolication, not customer deliverables.

5. **`SUPPORT.md` is NOT invented.** The sibling ships one as a release asset; Anabasis has no
   such file and a support contact is not a fact CI may fabricate (constraint C7). The asset is
   dropped, and the omission is stated in the workflow rather than left to be noticed.

6. **Version 0.1.1**, `CMakeLists.txt` line 20, with a dated CHANGELOG entry. v0.1.0 was
   declared code-complete on 2026-08-02 but never tagged, so **0.1.1 is the first build this
   repository releases** and its notes are the whole P1–P6 development plus this round. The
   entry template moved to the file's preamble for a mechanical reason discovered here: notes
   are extracted as everything between a version heading and the next `## [` one, so a template
   after the entries was published inside the notes.

## Consequences

- **OQ-007's resolution is superseded.** Its reasoning (defer with `OQ-002`/`OQ-012`) still
  holds for *signing and notarization*, which this record does not do — `build-pkg.sh` states
  the `.pkg` is unsigned, and macOS users take the same `xattr` step the manual already
  documents for the zip. Only the pipeline-and-installers half of the deferral is lifted.
- **A tag now fails closed on real conditions**: a missing staging tree, a missing installer, a
  lost executable bit, a version skew between the tag and any produced artifact, or an undated
  CHANGELOG heading. That is the intended behaviour and must not be relaxed into warnings.
- **The `workflow_call` hazard `build.yml` documents does not fire here**, and this is the
  re-check that comment asks for: inside a reusable workflow `github.event_name` is the
  *caller's*, and `release.yml` triggers only on `push` (tag) and `workflow_dispatch` — never
  `pull_request` — so `preflight` runs and the three build jobs run. It WOULD fire if a
  `pull_request:` trigger were ever added to `release.yml`, or if `build.yml` were called from
  any PR-triggered workflow on a same-repo PR: `preflight` would skip, all three build jobs
  would skip, and the run would report success with zero binaries. Recorded here rather than
  patched, because the guard belongs in `build.yml` and is a separate change.
- **Forecloses:** shipping attribution loose inside an archive. Adding it back to a staging tree
  now creates the version ambiguity option B removed, on top of the parity the directive
  requires.
- Still not done, and deliberately: Developer ID signing, notarization, and any update-check
  mechanism. None has a decision behind it, and inventing one here would be a C7 violation.

## Related code

- `packaging/linux/*`, `packaging/windows/*`, `packaging/macos/*` (new)
- `.github/workflows/build.yml` — the three staging steps, the ISCC step, the `.pkg` step, the
  two installer uploads
- `.github/workflows/release.yml` (new)
- `docs/policies/RELEASE_POLICY.md` — the amended attribution section
- `CMakeLists.txt` — `project(Anabasis VERSION 0.1.1 …)`; `CHANGELOG.md` — the dated entry

Evidence [Partially Verified]:
- **Verified**: both workflow files parse; `packaging/` contents are byte-reviewed ports whose
  names match what `release.yml` consumes (`Anabasis-<version>-Windows-Installer.exe` from the
  `.iss`'s `OutputBaseFilename`, `Anabasis-<version>-macOS.pkg` from `build-pkg.sh`'s third
  argument); the local suites and `check-docs.py` are green at 0.1.1.
- **Unverified, and named rather than implied**: no installer has been BUILT — ISCC and
  `pkgbuild` exist only on the Windows and macOS runners, so the first real evidence for those
  two steps is the first CI run on this branch. The Linux scripts have not been executed as
  root anywhere. This is a CI-evidence gap, not a human-audition one, and it closes on the
  first green run rather than at the fine review.
- Directive: the owner's 0.1.1 instruction of 2026-08-06, item 16
