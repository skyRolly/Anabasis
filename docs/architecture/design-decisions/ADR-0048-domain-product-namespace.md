# ADR-0048 — Domain product namespace for pre-release packages

**Status:** Proposed — owner namespace decision and pre-implementation Architecture Review
permission recorded on 2026-10-23: **“Approve B, C, and E for implementation and testing.”**
This clears the implementation stop for the concrete proposal, not ADR acceptance or merge.
Independent human architecture review remains required before merging this PR.

## Context

The owner controls `rolly.tech` and directs all future products to `tech.rolly.<product>`.
The historical starting tree `1a40afd190d6994e33cfa12ae3fca0f375d4687c` used
`com.rollytech.anabasis`. ADR-0008 froze that bundle ID and ADR-0021 specified receipts below it.
Both are partially amended by this proposal; their other decisions and dated evidence remain.
The product is pre-release; the owner explicitly accepts the development package-identity break.

## Problem

Current application/package identifiers must follow the owner's domain without accidentally
changing independent host plugin IDs or Windows uninstall registration. The old ADR-0008
consequence also incorrectly treated BUNDLE_ID as an input to the VST3 UID/AU component triple.
That claim is corrected against the pinned JUCE code, not repeated as a compatibility assumption.

## Options

- Keep the previous namespace: contradicts the domain decision.
- **Use `tech.rolly.anabasis` and unique package suffixes: chosen for implementation.**
- Add legacy aliases/receipt support: explicitly rejected for unpublished development builds.
- Change manufacturer/plugin codes, product name or Windows AppId: unnecessary independent
  identity breaks and outside the authorized namespace change.
- Add format suffixes to every CFBundleIdentifier: rejected; JUCE uses the base for these formats.

## Decision

- CMake BUNDLE_ID is `tech.rolly.anabasis`. With pinned JUCE 9.0.4, ordinary Standalone, VST3
  and AU use this same CFBundleIdentifier. Native generated plists are the verification gate.
- pkgbuild receipt IDs and productbuild references are `tech.rolly.anabasis.vst3`,
  `tech.rolly.anabasis.au`, `tech.rolly.anabasis.app`. The uninstalled packaging canary uses
  `.probe`. Receipt IDs are distinct from CFBundleIdentifier and from each other.
- Preserve `RollyTech`, `Anabasis`, manufacturer `RTec`, plugin code `Anbs`, and Windows
  AppId `{19AAA915-4527-4BA2-B6A7-06068E527C54}`. Preserve the version, formats, source/DSP,
  Simple/Advanced/adaptive behavior, parameter IDs, state, serialization and latency.
- No old-identifier aliases, fallback paths, receipt handling or package migration machinery.
- Generated unused AAX/ARA/AU-framework defaults inherit the base too; this enables no new format.
- Update only the namespace clauses of ADR-0008/ADR-0021 with dated notes. They stay in the
  record; the new ADR stays Proposed pending independent human review.

## Independent identities and consequences

Pinned `JUCEUtils.cmake` emits `JucePlugin_CFBundleIdentifier` from BUNDLE_ID but emits AU
manufacturer/subtype from the independent codes. `juce_PlistOptions.cpp` writes those separate
fields. `VST3/juce_VST3ModuleInfo.h` uses `VST3Interface::jucePluginId(manufacturer, plugin,
interfaceType)` because this product sets `JUCE_VST3_CAN_REPLACE_VST2=0`; the product name is
not an input on this path. The old general statement about name-derived UIDs is not the behavior
of this pinned configuration. BUNDLE_ID is not an input either.

The measured pre-change Linux component CID is `ABCDEF019182FAEB52546563416E6273`, controller
`ABCDEF011234ABCD52546563416E6273`. AU remains `aufx / Anbs / RTec`. Native CI checks both.
Windows VERSIONINFO product/vendor and VST3 metadata are checked without executing installer
payloads. Windows AppId is a separate unchanged GUID, not a reverse-DNS string.

macOS registration/cache or host session lookup can still react to CFBundleIdentifier. Actual
DAW recall/rescan is a human test; stable independent IDs alone are not a host compatibility
certification. New receipts intentionally do not replace old development receipt identities.
No old receipt is forgotten or deleted by these scripts. There is no state-format break or
state compatibility exemption. Linux ELF has no CFBundleIdentifier; the base appears in JUCE
build metadata, and no Linux desktop identifier is installed by the existing packaging.

## Related code

`CMakeLists.txt`, `packaging/macos/build-pkg.sh`, `packaging/windows/Anabasis.iss`,
`scripts/check-product-identity.py`, `.github/workflows/build.yml`.

## Evidence and confidence

Source/pinned-framework relationships and starting identity inventory are **Verified**.
See [the worklog](../../../worklogs/2026-10-23-product-namespace.md) for the independently
collected inventory, exact old/new outputs, commands and limitations. Generated/native claims
are scoped to the actual checks and CI head that produced them; synthetic self-tests do not
prove native builds. Native DAW/installation review remains **Unverified** until performed.
