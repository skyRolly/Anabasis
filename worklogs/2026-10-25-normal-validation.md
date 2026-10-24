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
