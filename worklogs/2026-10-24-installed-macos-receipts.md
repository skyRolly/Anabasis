# Installed macOS receipt validation — 2026-10-24

Starting main `d5f227dd05ab26b44432ace590fceda0a3b5ac22`, including merged #51/#52,
Accepted ADR-0048 and effective namespace amendments. This focused PR is independent
of the Windows correction. It validates existing receipt/identity decisions, changing
no CMake/dependency/format/state/architecture contract. No new Architecture Review Gate
item identified; human PR review and merge permission remain separate.

First-party copy-and-adapt provenance: Anamorph script at
`fd1941a4dd4e9e31af2c31aaf8c3505f97d24360` (ADR-0009). Adapted product/base/code/sibling
constants independently. No shared module; Anamorph is read-only in this workstream.

## Method and limits

After building the real PKG from validated staging, the disposable GitHub-hosted Mac
runs the self-test then the actual native install. Guard requires Darwin, GitHub-hosted
execution and this repository; existing product receipts/system bundles cause refusal.
Use real `/usr/sbin/installer -pkg <pkg> -target /` with noninteractive sudo and native
pkgutil enumeration/info/plists/files. No mocks, receipt forgetting, plug-in deletion,
legacy namespace handling or cache clearing; VM disposal is cleanup.

Require exactly `tech.rolly.anabasis.{vst3,au,app}`, matching current CMake version,
root volume, correct install location and bundle-specific receipt ownership; compare
installed/staged file hashes, CFBundleIdentifier and AU `aufx/Anbs/RTec`. Every previous
receipt and any sibling receipts/bundles must remain unchanged. Empty sibling snapshots
are explicitly not a both-products co-installation test. Full default installation is
covered; choice permutations/upgrades/GUI/DAWs require separate validation.

## Local validation

```sh
python3 scripts/test-macos-receipts.py --self-test
python3 scripts/check-docs.py --self-test
python3 scripts/check-docs.py
python3 scripts/check-citations.py --check --base d5f227dd05ab26b44432ace590fceda0a3b5ac22
```

Three valid components and 45 metadata/ownership/payload/separation/guard negatives.
Three independently run mutants disable receipt identity, named bundle ownership or
payload equality; each is rejected by its targeted fixture. The wrong-owner probe adds
a foreign entry to an otherwise valid complete receipt, avoiding unrelated missing-file
failure as liveness proof. Native results remain pending until exact-head CI completes;
Linux fixture success is not a native Mac pass. Installer upload requires native smoke.

No audio/GUI/state/parameter/latency source, existing tests, AppId, host IDs, namespace,
version or artwork changes. No merges, ADR acceptance, release/tag or deployments.
