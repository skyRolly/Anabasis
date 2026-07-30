# Changelog

All notable changes to **Anabasis** are documented here.

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versioning:
`MAJOR.MINOR.PATCH`, pre-1.0 line (< 1.0.0 = pre-release). Maintained per
[`docs/policies/CHANGELOG_POLICY.md`](docs/policies/CHANGELOG_POLICY.md):

- **User-visible changes only.** Refactors, cleanups, formatting and renames are not entries
  unless a commit/PR explicitly states a user-visible impact.
- **No invented history.** Every entry cites an Evidence Source (commit SHA, commit range, PR or
  release tag). An entry that cannot be tied to such evidence is marked
  `[Unverified Historical Reconstruction]`.
- **Renames are `Changed`, not `Removed`** (a display-name change with an unchanged parameter ID).
- Compatibility-affecting entries cross-link the relevant ADR and note any migration.

---

## [Unreleased]

Repository bootstrap only — no plugin exists yet, so there is nothing user-visible to record.
Project scaffolding (governance docs, policies, CI workflows, build scripts) is not a changelog
entry per `CHANGELOG_POLICY.md` rule 3; it is tracked in `docs/DOCUMENTATION_COVERAGE.md` and
`docs/HANDOVER.md`.

The first entry will be `[0.1.0]`, cut at the end of P6.

---

## Entry template

```
## [0.1.0] — 2026-MM-DD

### Added
- <user-visible change>.
  Evidence: commit 6a24b82 (or PR #NN). [Verified | Partially Verified | Unverified Historical Reconstruction]
```
