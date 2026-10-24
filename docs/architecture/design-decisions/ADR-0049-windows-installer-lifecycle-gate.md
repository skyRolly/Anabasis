# ADR-0049 — Optional-component Windows uninstall icon and lifecycle gate

**Status:** Proposed. Implementation/testing is expressly requested by the owner in
this round. This record enacts the accompanying TESTING_POLICY enforcement addition;
formal human review/acceptance and merge authorization remain pending. No CMake,
dependency/pin/format/audio/state/identity architecture gate item is changed or an
Accepted decision contradicted. The optional-component contract in ADR-0021 remains.

## Context

Main `d5f227dd05ab26b44432ace590fceda0a3b5ac22` includes merged #51/#52 and Accepted
namespace ADR-0048. Its Inno script registers `{app}\Anabasis.exe` as DisplayIcon,
but Standalone is optional. Native compilation already exists; runtime evidence did
not. The testing policy requires a real regression and same-job checker liveness.

## Problem

A valid fresh VST3-only installation registers a missing icon target. Correct that
without new branding, payload, destination, identity or component-removal semantics,
and prevent a compiled installer from passing while registering that missing file.

## Options

- Retain the optional EXE: demonstrably broken.
- Invent/import a branded icon or fake EXE: unauthorized artwork/payload.
- Require Standalone: changes the supported component contract.
- Omit DisplayIcon: leaves fallback selection implicit and less directly testable.
- **Reference `{uninstallexe}` and validate the installed native resource:** proposed.
- Source-only string check: does not establish actual registry/file/icon behavior.

## Decision

Use Inno's always-installed uninstaller and its existing generic executable icon.
An approved branded Anabasis installer icon remains a separate design decision.
Preserve AppId `{19AAA915-4527-4BA2-B6A7-06068E527C54}`, optional components, existing
destinations and all version/host/DSP/state/GUI/parameter/latency contracts.

Record the TESTING_POLICY gate: self-test first in the same Windows job, then real
ISCC/runtime checks on the disposable GitHub-hosted VM; installer upload requires
success. Compile the old optional-EXE target as a fault seed and require rejection
specifically for a real missing icon. Test three fresh choices, repeated installs,
selection changes and uninstall; inspect actual registry paths, extracted native
icons, payload/shortcuts, retained components and safe cleanup. Inno retains earlier
components on deselection; do not invent a component-removal operation.
Real registry mutations and portable good/bad snapshots prove failure liveness.
Preserve sibling files, unmanaged files, presets/settings; no global delete rules.

## Consequences

The uninstall target is valid for all supported choices without additional product
art. Runtime tests require a disposable Windows VM and refuse existing product
installs/developer or self-hosted machines. This does not certify Explorer/DPI,
custom destination GUI, historical upgrades or actual DAWs. No shared module or
Anamorph artwork/write is introduced; copied test structure carries ADR-0009 provenance.

## Related code

`packaging/windows/Anabasis.iss`, `scripts/test-windows-installer.ps1`,
`.github/workflows/build.yml`, `docs/policies/TESTING_POLICY.md`.

## Evidence and confidence

Native Windows job 120704412396/run 40038702266 at
`c14b98bb7477906a7cdb429194293d2204060a6c` passed the compiled old-target reproduction,
18 fixed install/reinstall/change snapshots, three real registry mutants and four
uninstalls. Three portable choices/30 bad snapshots and three disabled-check mutants
pass their liveness requirements. Verified within those exact scopes; human visual
certification is Unverified. See
[the worklog](../../../worklogs/2026-10-24-windows-uninstall-icon.md).
