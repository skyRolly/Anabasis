# Normal validation restoration — 2026-10-25

The owner authorized normal project validation on disposable runners. The temporary
publication-only router and deliberate failure marker are removed. Existing event
and deduplication conditions, normal build target coverage and security analysis
permissions are restored. CodeQL runs the existing complete Linux dependency setup.
The routing regression reads production files and covers direct Build and Release
manual invocation on both review branches, including their event/ref collision.
It also checks tags, main, unrelated branches, forks and the absence of invocation
inputs/nested calls; real Release metadata runs only in inert private fixtures.

The previous published workflow fails the review-branch Release regression; the
restored workflow selects the normal matrix. Manual rehearsals must never select
release creation, including from tag refs. No real rehearsal, release, tag, merge
or deployment is required by these fixtures. Normal CI outcomes are separate
runtime evidence and must be read at the actual final candidate.

No dependency pin, product code, artwork, version or identity changes. Accepted
ADR records remain Accepted. No separate code review or native/manual check is
claimed by this source change. Kernel/security settings are never changed: any
missing host capability fails visibly. Earlier publication evidence remains history.


Optional native macOS integration review consumes independently identified finished
package/staged artifacts through the direct Build manual input `integration-review`.
The JSON manifest names each product's repository, run ID, exact artifact head and
independently measured PKG SHA-256. Its default is empty; reusable Build calls define
no input and Release passes none. Normal validation always retains its full gates.
Seven selection jobs per product use separate fresh hosted Macs, measured choice
IDs and a selection-aware expected receipt/bundle set. The production default-full
exact-three assertion is unchanged. The Anamorph manual review also runs both real
two-product installation orders on separate clean Macs, requiring all six receipts
and preservation of the first product's metadata, listings and installed hashes.
These optional evidence consumers introduce no sibling build dependency, no live
host guard spoof and no receipt forgetting or pre-existing product deletion.
`python3 scripts/test-macos-integration-review.py --self-test` exercises pure fixture
validation only; native installs require the real disposable host guard and actual
identified artifacts. Archive digests are checked against GitHub metadata, and PKG
bytes against the independent manifest digest, before native installation.


### Review regression closure (2026-10-25)

`python3 scripts/test-workflow-routing.py --report <report.json>` now runs its
bounded self-test before production selection on every invocation. The same
production assertions accept a valid control and must specifically reject the
named-review-branch Release dispatch collision. Expression and needs controls
cover enabled/disabled jobs and failed/skipped prerequisites. A disabled verifier
cannot satisfy the negative control. No Release dispatch or tag is created.

Anabasis additionally runs the actual preflight presence/output shell body in
private inert directories with and without CMakeLists.txt, then verifies withheld
downstream jobs for ready=false, absent output and failed/skipped preflight.
This closes a coverage limitation; no production preflight defect was established.

The optional integration consumer authenticates run/head, artifact ZIP and PKG
bytes as before. Before installation, native `pkgutil --expand` supplies the
PackageInfo component identities/versions and Distribution references, checked
against staged Info.plist metadata. Receipt expectations come from that verified
component mapping, never installed receipts or a fixed current product version.
CFBundleShortVersionString identifies the marketing release used by build-pkg.sh;
CFBundleVersion identifies its build and can differ. Both fields must agree with
their corresponding native package declarations; they are not required to equal
each other. See Apple's [bundle version keys](https://developer.apple.com/library/archive/documentation/General/Reference/InfoPlistKeyReference/Articles/CoreFoundationKeys.html).
The supported `--self-test` covers current/alternate versions, metadata/receipt
mismatches, missing/duplicate/foreign components, Distribution reference failures
and distinct marketing/build strings, using inert files with no native/network call.
ZIP extraction requires a fresh empty private root, checks resolved containment,
and tests alternate spellings and archive link behavior. The reported spelling
escape remains unconfirmed; these fixtures are bounded evidence, not universal
safety proof. Existing ownership/content/location/selection/co-installation checks
and the production default-full exact-three gate remain unchanged.

This dated reconciliation preserves earlier exact-head evidence and Accepted ADR
provenance. It supersedes earlier recommendations only for these review defects;
final recommendations require published-head checks and affected native runs.
The script inventory drift was confirmed against docs/REPOSITORY_MAP.md and is
corrected here. No product version, identity, asset, DSP/state or approval change.

## 2026-10-25 staged-merge reconciliation

The owner merged PR #53 as `6dba5871b42d0d4c7323e21a1d226f5bf246164a`. Its complete
tree matches the reviewed first-PR head; both parent commits and relevant feature
bytes were checked against actual main. ADR-0049 remains Accepted and its
implementation is now effective on main. ADR-0050 remains Accepted, but its
implementation on main still awaits PR #54; importing actual main into this
feature branch does not perform that second merge. Owner confirmation remains
2026-10-25 and is distinct from formal GitHub reviews. Earlier Proposed/pending
entries and exact-head runtime results remain historical evidence.

The combined workflow preserves normal push/PR/manual/reusable caller semantics,
same-run routing liveness, dependency/security provisioning and both native
installer gates. Optional artifact-derived package consumers retain their own
selection/co-installation contracts and independent package inputs. Current normal
CI and affected installer/archive gates must complete on this combined candidate;
previous outer-workflow results do not certify its changed workflow.

The inherited routing module description incorrectly named TSan environment
fixtures. It now states the actual Release metadata and preflight presence/output
coverage. This is a nonblocking coverage-description correction. No Anabasis TSan
job, fixture or sanitizer evidence is added or claimed. Windows installer behavior
and the optional consumer/receipt/extraction validation code are unchanged.

Only the relevant conflict hunks were resolved; independent platform sections and
Accepted ADR rows were combined. No whole workflow/document was selected with
ours/theirs. Shared workflow/documentation overlap is now resolved against actual
main. Product versions, JUCE pins, identities, approved art and DSP/state/GUI are
unchanged. No GitHub PR merge, tag/release or deployment is performed here.
