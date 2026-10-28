# Locked ordinary pluginval acquisition (2026-10-28)

## Scope and provenance

The owner authorized the same acquisition follow-up separately in Anabasis after Anamorph.
Base: `4a13d02ad11529cd5805007fa5436557b99db4f8`. First-party reference:
Anamorph `21b61fb91c7f8b151ee2f95747424d1512ac2895` (PR #181), including the
interrupted HTTP/decompression setup-error refinement and its regression.
`scripts/acquire-pluginval.py` is copied with a provenance header; the test copy adapts only
product naming and optional discovery-helper copying. `scripts/pluginval-release.json` copies
the inspected release inventory, independently rehashed again before this repository's commit.
No shared module, sibling edits or third-party source enters this repository (ADR-0009).

The pin selects existing official v1.0.4, annotated tag object
`c6dfeaeac8e729c468d2b43a739e19ef4be922e1`, source target
`ed19c2c16b57a6d94db391bea3ef4a80b769d5bf`. GitHub's upstream API digest fields were null.
These hashes are measured official release bytes, not signed upstream statements or proof of
binary/source correspondence. Archives were inspected as data and never executed locally.

- Linux: official asset 210999199, 5911681 bytes, archive SHA-256
  `c01c49d8063965c4c2dea8324468336768f5c9139e0b1caebde14c2400b55352`; executable
  `pluginval`, SHA-256
  `d3d342ea92293b0699a5c983937211e3faf2b0f38145b31cb8d78424a25e10ba`.
- macOS: official asset 210999197, 9202795 bytes, archive SHA-256
  `3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f`; executable
  `pluginval.app/Contents/MacOS/pluginval`, SHA-256
  `32ec60efb9f8161f0b840103f9bd98fa455cd86ec59eb2803133b4382fa4425c`.
- Windows: official asset 210999198, 2408590 bytes, archive SHA-256
  `c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab`; executable
  `pluginval.exe`, SHA-256
  `f4ac5c31c5544a434f73e686b816861d92c22ae849d69ad693681cbc9522fcd8`.

## Alternatives and refusal behavior

- Floating latest or a version marker/executable-only cache check cannot establish content.
  Rejected: these were the original failure modes, including modified macOS resources.
- Independent shell/PowerShell pin implementations would need a new drift contract.
  Chosen: one lock plus one Python standard-library preparer, already available in CI.
- A cache-local extracted manifest can be modified with the executable. Chosen: rehash the
  archive against the committed lock and derive the complete comparison inventory from it on
  every reuse; reject missing/extra/modified files, directories, links and reparse points.
- Automatic replacement could conceal a damaged cache. Chosen: fail setup with the exact
  content-cache path; removal of that one directory is the documented operator recovery.
  Legacy and unrelated .tools files are neither trusted nor deleted.
- Unsafe ZIP paths/types must fail before any member is extracted. Chosen: inventory-first
  validation for POSIX and Windows spellings, private staging and final directory rename.

## Offline evidence and limits

The 12-test inert suite drives production preparation functions and both real wrapper setup
sections with stand-in transport and counting final invocation boundaries. Valid prepare/reuse
needs one download; bad/missing/truncated input, wrong platform, malformed lock, modified
executable, missing bundle metadata/resource, added content, links/special entries and escaping
ZIP spellings fail. No fixture archive member runs. Removing the production digest verifier in
a mutation makes the same-invocation liveness canary fail; genuine seeded digest refusal uses
the assertion protecting normal acquisition. Existing Anabasis verdict/retry code is unchanged;
it has no separate pre-existing shell --self-test entry point to claim as executed.

The independent KI-028 diagnostic source build remains untouched: pluginval source commit
`ed19c2c16b57a6d94db391bea3ef4a80b769d5bf`, JUCE submodule
`5179f4e720d8406ebd1b5401c86aea8da6cc83c9`, debug information, no hardened runtime,
read-only cache restore. Historical diagnostics keep that provenance. No diagnostic dispatch,
native debugger, product application, installer or acceptance campaign was run here.
Offline test evidence does not establish real platform CI outcomes. Product sources, versions,
identities, signing, artwork and installer names are unchanged. C++23's separate canary stays
retired under ADR-0030. Ordinary platform CI is observed separately after source publication.

## Review correction: concurrent publication and replay

The review found that two fully verified stages could race at the final rename, failing the
loser despite a valid published winner. The follow-up reuses one `cached_tool` path for ordinary
reuse and an existing target after a failed rename. It verifies the published archive against
the lock, rebuilds the ZIP inventory and verifies the complete tree; the losing stage's bytes
cannot vouch for the winner. Invalid/incomplete winners and errors with no target still fail.
A barrier deterministically reproduces the old OS error on all three platform shapes, while
seeded invalid winners exercise refusal without replacement and verify private-stage cleanup.

The old replay example targeted the legacy executable now ignored by ordinary preparation.
TESTING now obtains the executable from the preparer and uses `&&` to stop after setup failure.
Its regression runs the exact documented Bash block with an inert counting boundary, including
an existing legacy tool and an acquisition failure. No archive member or product is executed.

Review follow-up copy reference: Anamorph
`5da57eb26e48a9e59e6c2bc8dbfd950d70ec6993`; the helper and concurrency regressions
are copied with provenance. The exact-document replay regression is Anabasis-specific.
The new cases failed on the prior implementation/example before the correction. Current
suites contain 15 Anamorph cases and 16 Anabasis cases; native product execution is not implied.
