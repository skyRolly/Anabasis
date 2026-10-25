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

This dated reconciliation preserves earlier exact-head evidence and Accepted ADR
provenance. It supersedes earlier recommendations only for these review defects;
final recommendations require published-head checks and affected native runs.
The script inventory drift was confirmed against docs/REPOSITORY_MAP.md and is
corrected here. No product version, identity, asset, DSP/state or approval change.
