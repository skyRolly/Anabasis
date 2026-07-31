# RELEASE_COMPATIBILITY_CHECKLIST.md

Hard compatibility gate. **Every box must be checked before a release ships.** This enforces
`docs/policies/COMPATIBILITY_POLICY.md` and its subset policies. A failed item blocks the release
(or requires the COMPATIBILITY_POLICY exception: ADR + migration + Architecture Review).

## Checklist

- [ ] **Parameter IDs unchanged** — diff the parameter set against the previous release; no `pid::`
      ID renamed or removed. (Display-name changes are allowed; record in CHANGELOG.)
      Ref: `docs/architecture/PARAMETER_REGISTRY.md`, `docs/policies/PARAMETER_COMPATIBILITY_POLICY.md`.
      *Automated:* the registry-snapshot test in `tests/state_tests.cpp` fails on any
      ID/name/order/range/automation-flag change vs `tests/fixtures/parameter_registry.snapshot`.

- [ ] **Macro mapping unchanged (or migrated)** — the Simple → Advanced mapping curves produce the
      same Advanced values for the same macro position as the previous release, so a recorded
      macro-automation lane still sounds the same.
      Ref: `docs/policies/MODE_AND_ADAPTATION_POLICY.md` invariant 6.

- [ ] **Serialization schema verified** — no field removed or semantically changed; additions
      tolerate absence.
      Ref: `docs/architecture/SERIALIZATION_REGISTRY.md`, `docs/policies/SESSION_COMPATIBILITY_POLICY.md`.
      *Automated:* schema-shape + raw-exact round-trip + the legacy-format fixtures. The
      cross-version step below stays manual.

- [ ] **Presets migrated** — factory presets and a representative user `.anabasis` still load and
      sound identical. Parameter locks (Ceiling at minimum) still behave.
      *Partially automated:* the state suite proves save→reload structural equality + exclusion
      rules + factory loadability; "sound identical" remains a Level-5 check.

- [ ] **Pluginval passed (both modes)** — `scripts/run-pluginval.sh 10 deterministic` **and**
      `scripts/run-pluginval.sh 10 randomise` pass at strictness 10 on all three platforms.
      Ref: `docs/procedures/TESTING.md`.

- [ ] **Latency reporting verified** — reported PDC matches the actual chain delay across the
      **lookahead × oversampling** matrix, at both ends of the lookahead range; with oversampling
      off, the reported value is exactly the **lookahead allowance** — the constant 10 ms maximum,
      *not* the engaged value (`DSP_POLICY.md` invariant 2, as amended by **ADR-0004**: the engaged
      lookahead is a read offset inside a fixed line, so browsing presets never moves host PDC).
      A build that reports the engaged value fails this item. A latency change between releases
      desyncs every saved session's PDC.
      Ref: `docs/architecture/LATENCY_MODEL.md`, `docs/policies/DSP_POLICY.md` invariant 2
      (which records why "reports 0" is **not** a reachable state at all: **ADR-0004** resolved
      OQ-010 as *no* zero/off lookahead position, and forecloses widening the 0.5–10 ms range).

- [ ] **Ceiling guarantee re-verified** — output never exceeds the ceiling (≤ 0.1 dBTP) under the
      hostile-input sweep, at every supported sample rate and oversampling factor.
      Ref: `docs/policies/DSP_POLICY.md` invariant 4.

- [ ] **Metering accuracy re-verified** — LUFS ≤ 0.1 LU against the EBU R128 vectors; true peak
      ≤ 0.1 dB. A metering regression is a correctness regression, not a cosmetic one: users make
      release decisions from these numbers.

- [ ] **Host matrix verified** — load in the target hosts and confirm load + automation + offline
      render + state restoration. Minimum: **Reaper (Windows)** and **Logic Pro (macOS / AU)**.
      Requires manual DAW testing.

- [ ] **Automation playback verified** — recorded automation on host-visible parameters plays back
      with unchanged meaning.

- [ ] **Session reload verified** — save a session in the previous version, load it in the new
      version: sound, preset name, dirty marker, both A/B slots and any parameter locks reproduce
      exactly.
      *Partially automated:* the round-trip + legacy-fixture tests prove the CURRENT binary reads
      the modelled older formats; the true vN−1-binary → vN load remains this manual step.

## If any box cannot be checked

Stop. Either fix the regression, or — if the change is intentional — satisfy the
`COMPATIBILITY_POLICY.md` exception: an **ADR** + a **migration plan** + **Architecture Review**
sign-off. Document the migration in `STATE_SERIALIZATION.md` / `PARAMETER_REGISTRY.md` and the
CHANGELOG.

## Notes

- The headless gate verifies several of these structurally (latency, bypass null, no-NaN, ceiling,
  registry snapshot), but **Host matrix**, **Automation playback**, **Session reload** and
  "presets sound identical" require manual validation — they cannot be fully proven headlessly.
- **For v0.1.0 specifically**: there is no previous release, so the cross-version items have
  nothing to compare against. That does not make them vacuous — v0.1.0 is where the baseline is
  *created*. Freeze the registry snapshot and a session fixture for the 0.1.0 format as part of
  the release, or every later run of this checklist has no baseline to diff against.
