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

### Added
- **P1 skeleton — the plugin exists** (VST3 / AU-on-macOS / Standalone; JUCE 9.0.0 pinned):
  the 49-parameter surface of `DESIGN.md` §4.2 (IDs frozen, registry snapshot under
  `tests/fixtures/`), a pass-through chain with a basic lookahead limiter and the final ceiling
  clamp, constant reported latency (10 ms lookahead allowance — browsing presets never moves
  host PDC, ADR-0004), Simple-mode macro knobs mapped by the §5.5 draft curves, schema-v1
  session state with A/B slots, and a placeholder editor. No EQ/compressor/clipper processing,
  no oversampling, no metering yet — those are P2–P5.
  Evidence Source: **PR #4** (`skyRolly/Anabasis`) — commits `91ece13` (skeleton), `e0c24d5`
  (limiter alignment + state restore fixes), `79fd781` (control smoothing), `d6fa408`
  (macro mapping off the audio thread, defaults-first read rules) and `0190aee` (LF snapshot
  pin, restore guard). The PR is the stable citation: it keeps resolving after the branch is
  deleted at merge, which a branch-relative "the commit that follows" does not
  (`CHANGELOG_POLICY.md` rule 2). [Verified]

- **P2 DSP core (in progress)** — the chain stages now process audio: the §2.2 EQ (tilt pair,
  shelves, two bells, Pre/Post with the clamp-last guarantee), the §2.3 glue compressor
  (log-domain, soft knee, RMS/Peak, two-pole auto release, sidechain HPF, parallel mix), and the
  §2.4 clipper/saturation (hard↔soft knee morph with first-order ADAA — measured 14.8/10.4 dB
  alias reduction at OS Off — colour models with odd/even balance and tone, the dynamic HF tame,
  drive with level compensation). All-defaults remains a bit-exact null. Limiter upgrades,
  oversampling and dither follow in this phase.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`). [Verified]

The first entry will be `[0.1.0]`, cut at the end of P6.

---

## Entry template

```
## [0.1.0] — 2026-MM-DD

### Added
- <user-visible change>.
  Evidence: commit 6a24b82 (or PR #NN). [Verified | Partially Verified | Unverified Historical Reconstruction]
```
