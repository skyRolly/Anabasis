# CHANGELOG_POLICY.md

Repository Governance Policy. How `CHANGELOG.md` is maintained.

## Rules

1. **Format: Keep a Changelog.** Sections per version: Added / Changed / Fixed / Removed /
   Deprecated / Security, newest first. `MAJOR.MINOR.PATCH`, pre-1.0 line starting at **0.1.0**.
2. **No invented history.** Never infer that a past version contained a feature by reasoning
   backward from current code. Each entry cites an **Evidence Source** — a commit SHA, commit
   range, PR, or release tag. An entry that cannot be tied to such evidence is marked
   `[Unverified Historical Reconstruction]`. Anabasis starts with git tags available from its
   first release, so there is no excuse for an unverified entry in this project.
3. **User-visible changes only.** Refactors, cleanups, formatting, and renames are **not**
   changelog entries **unless** a PR/commit explicitly states a user-visible impact. Repository
   scaffolding and documentation passes are not entries.
4. **Renames are Changed, not Removed.** A display-name change with an unchanged parameter ID is a
   "Changed" entry; it is **not** a parameter removal.
5. **Compatibility-affecting entries cross-link** the relevant ADR and note any migration.
6. **Measured claims carry their number and its method.** "Lower CPU" is not an entry; "4× oversampling
   CPU reduced from X% to Y% on <machine/config>" is — and the method lives in `TEST_REPORT.md`
   (constraint C2). Never publish a performance or loudness figure that is not measured.

## Entry template

```
## [0.1.0] — 2026-MM-DD

### Fixed
- <user-visible change>.
  Evidence: commit 6a24b82 (or PR #NN). [Verified | Partially Verified | Unverified Historical Reconstruction]
```

## Source of truth for history

Commit messages, PRs and annotated release tags are primary. Phase summaries and worklogs are
corroborating but not authoritative on their own. When reconstructing, prefer the commit that
introduced the change.
