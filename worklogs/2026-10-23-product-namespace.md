# Product namespace migration — 2026-10-23

Starting main: `1a40afd190d6994e33cfa12ae3fca0f375d4687c`.
Branch: `codex/namespace-tech-rolly-anabasis`, independent base main.
Owner chose the domain namespace and then explicitly approved the concrete implementation/testing
architecture proposal. ADR-0048 is Proposed; independent human review is required before merge.
No Anamorph files are modified by this workstream; no shared module is introduced.

## Inventory before editing

A complete local `git ls-files -z` content and filename audit covered all 259 tracked files,
case-insensitively for `com.rollytech.`, `rollytech`, BUNDLE_ID, CFBundleIdentifier, CFBundle,
identifier, pkg-ref, AppId, UninstallDisplayIcon, plugin identity, manufacturer code and plugin
code. It found 42 files containing at least one term; no matching tracked filename. The full raw
scan is retained in the session evidence directory. Per-file counts below include documentation
and parameter-identifier discussions, not just package identities.

| Tracked file | Matching lines |
|---|---|
| `.github/workflows/build.yml` | 1 |
| `CHANGELOG.md` | 2 |
| `CMakeLists.txt` | 4 |
| `NOTICE` | 1 |
| `README.md` | 5 |
| `SUPPORT.md` | 1 |
| `docs/DESIGN.md` | 6 |
| `docs/DEVELOPMENT_BRIEF.md` | 9 |
| `docs/DOCUMENTATION_COVERAGE.md` | 11 |
| `docs/HANDOVER.md` | 6 |
| `docs/OPEN_QUESTIONS.md` | 10 |
| `docs/POSTMORTEMS.md` | 1 |
| `docs/architecture/SERIALIZATION_REGISTRY.md` | 1 |
| `docs/architecture/design-decisions/ADR-0007-state-schema-v1.md` | 1 |
| `docs/architecture/design-decisions/ADR-0008-build-architecture-and-plugin-identity.md` | 14 |
| `docs/architecture/design-decisions/ADR-0009-code-reuse-from-anamorph.md` | 6 |
| `docs/architecture/design-decisions/ADR-0010-parameter-surface.md` | 1 |
| `docs/architecture/design-decisions/ADR-0021-release-pipeline-and-artifact-parity.md` | 4 |
| `docs/architecture/design-decisions/ADR-0028-juce-901-pin.md` | 1 |
| `docs/architecture/design-decisions/ADR_INDEX.md` | 2 |
| `docs/policies/AI_AGENT_POLICY.md` | 1 |
| `docs/procedures/BUILD.md` | 6 |
| `docs/reports/2026-09-26-anabasis-product-ux-audit/findings-doc-test.md` | 1 |
| `docs/reports/2026-09-26-anabasis-product-ux-audit/findings-ui.md` | 1 |
| `docs/reports/2026-09-26-anabasis-product-ux-audit/findings-ux.md` | 3 |
| `docs/user/INSTALLATION.md` | 1 |
| `docs/user/USER_MANUAL.md` | 5 |
| `packaging/linux/INSTALL.txt` | 2 |
| `packaging/macos/INSTALL.txt` | 2 |
| `packaging/macos/build-pkg.sh` | 11 |
| `packaging/windows/Anabasis.iss` | 7 |
| `packaging/windows/INSTALL.txt` | 2 |
| `scripts/check-portability.py` | 3 |
| `scripts/check-realtime.py` | 16 |
| `src/InternalState.h` | 12 |
| `src/PresetManager.h` | 1 |
| `src/dsp/LoudnessMeter.h` | 1 |
| `src/gui/PluginEditor.cpp` | 3 |
| `src/gui/SpectrumView.cpp` | 1 |
| `worklogs/2026-07-30-p0-anamorph-research.md` | 13 |
| `worklogs/2026-08-22-migration-roadmap-execution.md` | 1 |
| `worklogs/2026-09-26-product-ux-audit.md` | 3 |

Historical starting identity: base `com.rollytech.anabasis`, receipts under `.vst3/.au/.app`
and uninstalled `.probe`; Windows AppId `{19AAA915-4527-4BA2-B6A7-06068E527C54}`. The accepted
freeze is ADR-0008 and packaging namespace is ADR-0021. CMake-generated Info.txt/Defs.txt also
contain derived AAX/ARA/internal-framework identifiers although those formats are disabled.

## Implementation and reproduction

Current base `tech.rolly.anabasis`; receipts `.vst3/.au/.app`; no compatibility aliases.
All three macOS CFBundleIdentifiers use the base on the pinned JUCE path, independently of
receipt suffixes. Current source/packaging checks:

```sh
python3 scripts/check-product-identity.py --self-test
python3 scripts/check-product-identity.py
python3 scripts/check-product-identity.py --build-dir build --platform linux --report build/product-identity-linux.json
scripts/run-tests.sh
scripts/run-pluginval.sh 10 deterministic
scripts/run-pluginval.sh 10 randomise
```

Native CI runs the identity self-test and generated-output gate on Linux, Windows, Apple Silicon
and Intel macOS. The existing pkgbuild/productbuild script parses expanded PackageInfo and
Distribution through the same checker after its self-test. Existing packaging relocation/version/
overwrite canaries, DSP/state/pluginval, sanitizers, static and format gates remain enabled.

Before reconfiguration, the retained Linux build at `74d61e0` has the same first-party tree as
starting main (only three workflow pins differ). It reports component CID
`ABCDEF019182FAEB52546563416E6273`, controller `ABCDEF011234ABCD52546563416E6273`, version
0.2.17, RollyTech/Anabasis, RTec/Anbs. Old Info.txt, Defs.txt and moduleinfo are retained in session
evidence for direct comparison. No name/GUID/code/format or C++ source change is proposed.

## Historical residuals and limits

Old namespace literals retained in ADR-0008 options/evidence, ADR-0048 context, the explicit
DESIGN amendment, earlier DOCUMENTATION_COVERAGE audit entries and the P0 research worklog
are historical evidence. They are not executable identifiers or current canonical declarations.
No obsolete literal remains in CMake, packaging, active test fixtures or CI configuration.
Native host recall/rescan, macOS Installer receipt installation and real Windows installer
registration remain human/native checks; no unavailable tooling result is claimed.

Validation results are scoped to the PR head and its CI checks. No release/tag, version bump,
installer deployment, merge or direct-main update is authorized.


## Local validation observed

Linux GCC 14.2 / pinned JUCE 9.0.4: generated Info/Defs now use the canonical base and
the VST3 moduleinfo remains byte-identical to the retained before-migration output. The
compiled plugin and Standalone exist. DSP 795 checks / state 1638 checks pass. Source identity,
matching-platform metadata/package fixtures, documentation/citation and shell syntax checks pass.
The full preflight exits 1 at the existing ABI floor: CXXABI_1.3.15 from this development
toolchain exceeds CXXABI_1.3.9. The shipping Clang matrix remains the release ABI gate. No
full green local preflight is claimed and no ABI policy is weakened. Pluginval/native CI
results are recorded by exact PR head separately.

The current brief's sibling comparison had still said `Anmf` despite current Anamorph using
RTec. Reported before editing; corrected the documentation only, preserving both actual codes.

Native resource premise correction: pinned `juce_ResourceRc.cpp` emits CompanyName,
ProductName, FileDescription, FileVersion and ProductVersion; it does not emit InternalName
or OriginalFilename. The new VERSIONINFO reader's API-boundary fixture failed on the
incorrect InternalName requirement, then passed after querying only the five real fields.
No production gate is relaxed for a resource that JUCE actually emits.

Native productbuild correction (2026-10-23): Anabasis job 120483320088 produced nine
pkg-ref elements for three unique component receipts; the input distribution has six.
The checker had incorrectly equated reference count with component count. It now requires
three unique PackageInfo IDs, one choice and payload URI per component, canonical IDs in
all references and canonical bundle metadata, while allowing generated metadata references.
Six-reference input and normalized nine-reference positive fixtures, missing/duplicate payload
and foreign bundle-ID negatives guard this correction. Existing package-generation behavior
and identifier values are unchanged; native validation is repeated on the corrective head.


### Clang LTO dependency transport — 2026-10-23

The starting-main run 39839766743 and namespace run 39978295519 timed out before compilation
while downloading packages from the hosted image's HTTP Azure Ubuntu mirror. The Clang LTO
lane now substitutes the official HTTPS archive in that disposable runner's apt mirror list.
This is restricted to GITHUB_ACTIONS=true, RUNNER_ENVIRONMENT=github-hosted and
GITHUB_JOB=linux-lto-clang; other jobs, containers and developer machines are unchanged.
The full dependency list, normal apt signature verification, Clang major, build flags,
assertions and 30-minute job limit are unchanged. The existing native compiler/version and
LTO test gates still run; no failed test is skipped or reclassified as successful.
Actual source/transport and test results are collected on the new PR head.
