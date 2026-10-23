# ADR-0048 — Domain product namespace for pre-release packages

**Status:** Accepted — explicit final owner approval of ADR-0048 and the completed
namespace implementation. **Architecture Review Gate: CLEARED; owner decision: APPROVED.**
Approval provenance and verified merge ancestry are recorded in [ADR_INDEX.md](ADR_INDEX.md#owner-approval-reconciliation--record-updated-2026-10-24).
The original final-approval date is not recorded; this status update is dated 2026-10-24
(Asia/Shanghai). The earlier 2026-10-23 permission, **“Approve B, C, and E for
implementation and testing,”** is retained as implementation-stage history.

## Context

The owner controls `rolly.tech` and directs all future products to `tech.rolly.<product>`.
The historical starting tree `1a40afd190d6994e33cfa12ae3fca0f375d4687c` used
`com.rollytech.anabasis`. ADR-0008 froze that bundle ID and ADR-0021 specified receipts below it.
Both partial amendments are now effective and registered in ADR_INDEX.md. ADR-0008 and
ADR-0021 remain Accepted; their other decisions and dated evidence remain.
The product is pre-release; the owner explicitly accepts the development package-identity break.

## Problem

Current application/package identifiers must follow the owner's domain without accidentally
changing independent host plugin IDs or Windows uninstall registration. The old ADR-0008
consequence also incorrectly treated BUNDLE_ID as an input to the VST3 UID/AU component triple.
That claim is corrected against the pinned JUCE code, not repeated as a compatibility assumption.

## Options

- Keep the previous namespace: contradicts the domain decision.
- **Use `tech.rolly.anabasis` and unique package suffixes: accepted.**
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
- Partially amend ADR-0008's bundle namespace/identity-derivation statements and ADR-0021's
  receipt namespace. These amendments are effective; earlier records remain Accepted with
  dated forward pointers and historical values, rather than pretending the namespace never changed.
- Enforce the existing source/generated/package identity gates through TESTING_POLICY.md,
  including same-job positive/negative checker fixtures and exact component receipt ownership.

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
