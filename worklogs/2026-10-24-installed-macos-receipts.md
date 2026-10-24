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
failure as liveness proof. At initial implementation native results awaited exact-head CI;
Linux fixture success is not a native Mac pass. Installer upload requires native smoke.

No audio/GUI/state/parameter/latency source, existing tests, AppId, host IDs, namespace,
version or artwork changes. No merges, ADR acceptance, release/tag or deployments.

## Native result at ff0f422c901b0c554223240ecca89e415b779d0f

The full push matrix 40039466165, PR merge check 40039489894, CodeQL 40039489924
and dependency review 40039490283 pass. Mac job 120706890289 performed actual default
full installation; artifact 12539322009 was independently downloaded and digest-checked:
ZIP SHA-256 `b6ac77722be6a2aadd9a6a17db73d0c5164c0733871c7c060aff2b67efe1a1ae`.
It records failure=null, all three canonical receipts at version 0.2.17/volume /,
locations Library/Audio/Plug-Ins/VST3, Library/Audio/Plug-Ins/Components, Applications,
CFBundleIdentifier tech.rolly.anabasis, AU codes and installed/staged hash equality.
Existing receipts survive; sibling count is zero, so co-installation is not claimed.
The active release-procedure gap is narrowed accordingly. This final documentation
follow-up changes no script/workflow/product bytes; fresh combined-head CI still applies.


## Review closure — 2026-10-24 (Asia/Shanghai)

Confirmed omissions: resource/signature/symlink receipt ownership, explicit workflow
liveness dependency and binding TESTING_POLICY coverage. Refuted field-name advice:
retained raw native plists use install-location, not location. Raw fixture provenance
records the original artifact digest/head; those historical bytes are unchanged.

Old validate_component accepts omitted leaves while trees match. Enriched fixtures
make that old validator and a disabled ownership gate fail specifically on payload
ownership. Failed/skipped liveness and failed-installation artifact boundaries are
proved against actual workflow expressions with harmless temporary execution/upload
markers; this is scheduler-expression evidence, not an actual failed PKG publication.
The native step now captures measured choice XML, host/head and package digest.

Proposed ADR-0050 records policy enforcement; formal review/acceptance and all merges
remain pending. No product identity/version/audio/GUI/state or signing policy change.

## 2026-10-25 current approval reconciliation

The owner directly confirmed **“all ADRs have received human approval.”**
ADR-0050 is now **Accepted**; human architecture/ADR review is complete.
This supersedes earlier pending-acceptance statements in this historical
worklog. 2026-10-25 is the confirmation/record-update date, not an invented
earlier approval date. No reviewer identity, meeting, signed record or GitHub
review submission is inferred. Separately required code review, unperformed
native/manual checks and merge/publication remain separate; no test result is
upgraded by approval. The associated implementation/policy becomes effective
on main only after merge. Other existing Accepted decisions remain Accepted.

Command-history reconciliation: `scripts/test-macos-receipt-gates.py` supports
`--workflow` and `--report`, not a `--self-test` CLI switch; it invokes its self-test
internally. The production command is `python3 scripts/test-macos-receipt-gates.py
--report <task-report-path>`. The separate supported scanner command remains
`python3 scripts/test-macos-receipts.py --self-test`. A historical report's
unsupported flag is not treated as an executed command or repeated here. Native
job logs and final-head local reports supply the actual command evidence.
