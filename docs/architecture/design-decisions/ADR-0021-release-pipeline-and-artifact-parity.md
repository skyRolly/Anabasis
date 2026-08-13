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

5. **`SUPPORT.md` is written, not invented.** The sibling ships one as a release asset and
   `DEVELOPMENT_BRIEF.md` §14.2 names it in the Internal/testing documentation class, but
   Anabasis had none. It is now written — and it is *shorter* than the sibling's on purpose:
   that class's rule is "restates the legal class, never diverges from it", and Anabasis has no
   approved licence, EULA or privacy document to restate (OQ-002 / OQ-009 open, and
   `HANDOVER.md` records that the owner-legal set was deliberately not produced). The file
   states that as its own §1 and confines itself to what the repository can evidence: the
   reporting channel, what a usable report contains, and that terms come from the owner rather
   than from it. No support contact is fabricated (constraint C7).

6. **Version 0.1.1**, `CMakeLists.txt` line 20, with a dated CHANGELOG entry. v0.1.0 was
   declared code-complete on 2026-08-02 but never tagged, so **0.1.1 is the first build this
   repository releases** and its notes are the whole P1–P6 development plus this round. The
   entry template moved to the file's preamble for a mechanical reason discovered here: notes
   are extracted as everything from a version heading to the next h2 heading — and the newest
   entry has none after it, so a template sitting after the entries was published inside the
   notes. See the amendment below for how that boundary is drawn.

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

Evidence [Verified — with two named gaps]:
- **Verified locally**: both workflow files parse; `packaging/` contents are byte-reviewed ports
  whose names match what `release.yml` consumes; the suites and `check-docs.py` are green at
  0.1.1.
- **Verified in CI** (run 31135082913 on this branch, all three platforms green — the run that
  closed this record's original Unverified claim). The **Windows installer built**: ISCC 6.7.1
  compiled `packaging/windows/Anabasis.iss` against the staged tree and produced
  `Anabasis-0.1.1-Windows-Installer.exe`, uploaded as `Anabasis-Windows-installer`. The
  **macOS `.pkg` built**: `package_macos` carries no `continue-on-error`, so a failure in
  `build-pkg.sh` — including any of its own post-build self-checks on the three component
  identifiers, `customize="allow"` and the three `start_selected="true"` — would have failed the
  job and the run; the run is green, so those checks passed and `Anabasis-macOS-installer`
  uploaded. Both filenames took their version from `CMakeLists.txt`, which is the property
  `release.yml`'s skew check depends on.
- **Still unverified, and named rather than implied**: (1) the Linux `install.sh` /
  `uninstall.sh` have not been EXECUTED as root anywhere — CI stages them and sets the mode bit,
  which is not the same as running them; (2) `release.yml` itself has never run. It is
  tag-triggered and no tag exists yet, so the validate → build → draft-release chain's first
  real exercise is the `v0.1.1` tag, or the `workflow_dispatch` rehearsal that trigger exists
  for.
- Directive: the owner's 0.1.1 instruction of 2026-08-06, item 16

## Amendment — the notes extractor bounds a section on ANY h2, and reads fences as data (2026-08-07)

No rule in §Decision changes; this corrects how item 6's extraction is implemented. The original
terminator was `^## \[` — the next *version* heading — which is sound for every entry except the
one that matters most. The **newest** entry has no version heading after it, so its notes run to
end of file. That is deliberate and stays (0.1.1's notes ARE the whole P1–P6 development beneath
them, carried in `###` sections), but it made every future h2 added below the newest entry a
silent addition to that release's published notes. The file demonstrates the second half of the
hazard itself: its preamble holds a `## [x.y.z] — YYYY-MM-DD` sample **inside a fence**, and an
extractor blind to fences would have started there for any release whose version that sample ever
named — publishing the template as the notes and stopping at the real heading.

The extractor now terminates on any `^## ` — which does not match `### `, the third character
being `#` rather than a space, so an entry's own sub-sections are untouched — and tracks fenced
blocks, closing one only on the character that opened it, so a `~~~` line inside a ``` block
stays data. Output for the release being cut is byte-identical to the earlier form; what changes
is the releases after it. `CHANGELOG.md`'s preamble now states the two editing rules that follow
from the boundary (an `## ` below the newest entry ends its notes; entry sub-sections stay at
`### ` or deeper).

Evidence [Verified]:
- Both extractions run against `CHANGELOG.md` at `VERSION=0.1.1` produce identical 425-line
  output (`cmp` clean), so the 0.1.1 notes are unchanged.
- Against a fixture carrying a fenced version sample inside a newer entry and a trailing
  `## Notes on this file` heading, the old form starts inside the fence and swallows the trailing
  heading into the release notes; the new form extracts exactly the intended section in both
  directions.
- `.github/workflows/release.yml` parses (`yaml.safe_load`).

## Amendment — the Linux installer is two-mode, and item 1's "root-checked" no longer describes it (2026-08-13)

> **ARCHITECTURE REVIEW GATE — CLEARED (2026-08-13).** `CLAUDE.md` lists "conflict with an
> Accepted ADR" as a hard stop, and §Decision item 1 is an Accepted decision, so this change was
> held and reported rather than assumed: the 0.1.4 review round carried it as the one item needing
> the owner, with the contradiction stated and the revert path named. **The owner approved it on
> 2026-08-13**, which is what turns this from a reported drift into a decision. Item 1's
> "root-checked" clause is superseded by the text below; the rest of item 1 stands unchanged.

This amendment records what the installer does and why. It changes no other rule in §Decision.

**What item 1 says.** `linux/{INSTALL.txt,install.sh,uninstall.sh}` install "VST3 →
`/usr/lib/vst3`, standalone → `/usr/local/bin`, **root-checked**".

**What the installer does since 0.1.4.** It is two-mode and chooses interactively. The DEFAULT is a
**per-user** install needing no root at all — VST3 → `~/.vst3`, standalone → `~/.local/bin` — and
the system-wide destinations above are the second option, reached by answering `2` or by running
the script as root. There is no root check to fail: a non-root run is the normal path, and the
system branch elevates individual operations with `sudo` rather than demanding the whole script be
privileged. `uninstall.sh` mirrors both modes.

**Why the behaviour changed.** `~/.vst3` is the standard per-user VST3 folder and is scanned by
most DAWs, so the common case never needed root; requiring it was the sibling's shape inherited
unexamined. The 0.1.4 migration replaced the delete-then-copy installer with a transaction, and
making the default unprivileged is the larger part of what removes risk from it — an installer that
does not need root cannot misuse it. That reasoning is the owner's to accept or reject; it is
recorded here rather than argued in a script comment.

**What else in this record the change touches.** The "Still unverified" note above says the Linux
scripts "have not been EXECUTED as root anywhere". That is now partly out of date in the useful
direction: the 0.1.4 rounds exercised fresh, upgrade, interrupted-recovery, uninstall, duplicate-
warning and staging-refusal paths against a redirected destination, in both modes. What remains
unexercised is a real root run against the real `/usr/lib/vst3` on a distribution system.

**The revert path, kept because a ratified decision is still reversible**: the gated part is the
mode prompt and the per-user branch, not the transaction — the staging/park-aside/reconcile
machinery is orthogonal to which destination is the default, and a later reversal of the default
must not take it out. See `POSTMORTEMS.md` INC-006 for why the staging location in that machinery is a trust
boundary rather than a matter of taste.
