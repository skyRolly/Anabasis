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
  drive with level compensation), and the §2.5 limiter upgrades (true-peak mode with the 4×
  ADR-0003 measurement tap — the ceiling is dBTP-aware — stereo link, Transparent/Punchy/Loud
  styles, transient preservation, two-pole auto release, shared sidechain HPF), §3
  oversampling (Off/2×/4×/8×/16× × min/linear phase wrapping Clipper→Limiter, all instances
  built at prepare, integer-latency mode so reported PDC is exact across the whole matrix,
  Force-Max offline honoured) and §4.5 dither (TPDF 16/24-bit + first-order noise shaping,
  deterministic, after the clamp), and the §2.8 click-free transition layer (asymmetric
  raised-cosine duck ~6 ms out / ~28 ms in: engine rewires — EQ position, colour model, OS
  factor/phase — execute only at the silent bottom; A/B, preset and session-load bulk swaps
  request the duck before swapping — closes KI-001 → POSTMORTEMS INC-001). All-defaults remains
  a bit-exact null; bypass stays a bit-exact null at every oversampling factor.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`). [Verified]

- **P3 metering engine (in progress)** — BS.1770-4 LUFS M/S/I with the two-stage gate
  (calibrated to the standard's compliance vector ≤ 0.1 LU), dBTP max-hold off the shared
  estimator, PLR, per-block GR published with a 43 s history ring, and the §2.7 monitor layer:
  loudness-compensated monitoring with loudness-matched bypass (Measure + Predict, monitor-only —
  the offline render is bit-identical either way) and delta monitoring. The Loudness Comp and
  Delta toggles now work (KI-002 → POSTMORTEMS INC-002).
  Evidence Source: **PR #5** (`skyRolly/Anabasis`). [Verified]

- **P4 adaptive engine (core)** — audio-thread feature extraction (crest, tilt, transient
  density) driving the §5.4 bounded trim vector around release / stereo link / sidechain HPF /
  dynamic tame, second-scale slew with hysteresis, Freeze latching the vector exactly, and the
  mode-switch invariant pinned sample-identically (`testModeSwitchIsSoundNeutral`). The trims are
  engine-internal — the host, automation and undo never see them — and the bit-exact null holds
  with adaptation live. Learn (core) is in: analyse → commit fixes the
  reference targets, serialized in the global ADAPTIVE child ("absent = never learned").
  The OQ-013-gated frozen-trim restore remains the one blocked path.
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
