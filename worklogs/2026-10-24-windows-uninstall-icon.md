# Windows uninstall icon correctness — 2026-10-24

Starting main: `d5f227dd05ab26b44432ace590fceda0a3b5ac22` (merged #51 and
owner governance #52). The predecessor namespace ADR-0048 is Accepted. This work
fixes an installation defect within ADR-0021's existing optional-component layout;
it changes no CMake/build architecture, dependency, identity or accepted decision.
No new Architecture Review Gate item was identified. Human PR review remains required.

## Before and decision

The original ISS registers `UninstallDisplayIcon={app}\Anabasis.exe`, but its EXE
file entry has `Components: standalone`. Thus a fresh VST3-only choice registers a
missing file. The native harness compiles a one-directive fault seed using that
exact old target, installs it, and requires the production snapshot gate to reject
the actual registry entry specifically for a missing icon. This reproduces OS
behavior rather than asserting only a source string.

Chosen: `UninstallDisplayIcon={uninstallexe}`. Inno installs the uninstaller for
every legal choice, and its default icon can be extracted natively. A branded
icon needs a separate approved Anabasis asset; inventing artwork, copying B0,
making Standalone mandatory or installing a fake EXE would violate this scope.
Omitting DisplayIcon leaves OS fallback selection implicit; an explicit real
uninstaller path is directly testable. No additional installed resource is needed.
The generic icon is intentional, not a branding completion claim.

AppId remains `{19AAA915-4527-4BA2-B6A7-06068E527C54}`. Existing destinations,
installer UI, component semantics and version 0.2.17 are unchanged. No uninstall
delete rule is added. Inno retains previously installed components on deselection;
the test measures accumulated payload rather than implementing new removal behavior.

Drift reported before editing: the ISS compiler comment claimed CI had no Inno
step. Current `build.yml` already compiles it; that comment is corrected narrowly.

## Validation and limits

Local commands:

```sh
pwsh -NoProfile -File scripts/test-windows-installer.ps1 -SelfTest
python3 scripts/check-product-identity.py --self-test
python3 scripts/check-product-identity.py
python3 scripts/check-docs.py --self-test
python3 scripts/check-docs.py
python3 scripts/check-citations.py --self-test
python3 scripts/check-citations.py --check --base d5f227dd05ab26b44432ace590fceda0a3b5ac22
```

The portable self-test has three valid choices and 30 seeded snapshot failures,
plus safe-uninstaller parser controls. Native CI must additionally compile the
fault seed and fixed installer, test three initial choices and six laps each
(same-choice repeat, both components, each single selection, return to initial),
then uninstall. Actual registry missing-file mutations must be rejected and
restored before continuing. Native icon extraction uses shell32 ExtractIconEx
and destroys returned handles. Setup/uninstaller use Inno's unchanged generic art.
The self-test runs before the native gate in that same job. Installer upload
requires the native gate; its JSON is retained even on failure.

The harness refuses non-disposable/non-Windows/self-hosted targets and existing
product payloads/registrations/menus. It preserves and checks exact unmanaged,
sibling-plugin and user preset/settings sentinels; cleanup names only its own
sentinels. It never broadly deletes plug-in folders or preset directories.
No native Windows result is claimed until the actual current-head job completes.
Chooser UI, Explorer/DPI/cache, broader historical upgrades and real DAW recall
remain manual. No DSP/state/GUI/parameter/latency source or existing test changes.

No PR merge, ADR acceptance, direct-main write, release/tag, version change or
installer deployment is part of this work. Windows installations occur only on
the guarded disposable CI VM.
