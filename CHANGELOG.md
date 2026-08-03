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
  drive with level compensation — and the limiter's push sits AFTER it, so raising loudness does
  not drive the clipper harder), and the §2.5 limiter upgrades (true-peak mode with the 4×
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
  density), of which **transient density and tilt** drive the §5.4 bounded trim vector around
  release / stereo link / sidechain HPF / dynamic tame (crest is published for the UI and
  reserved for a future mapping), second-scale slew with hysteresis, Freeze latching the vector
  exactly, and the
  mode-switch invariant pinned sample-identically (`testModeSwitchIsSoundNeutral`). The trims are
  engine-internal — the host, automation and undo never see them — and the bit-exact null holds
  with adaptation live. Learn (core) is in: analyse → commit fixes the
  reference targets, serialized in the global ADAPTIVE child ("absent = never learned").
  The OQ-013-gated frozen-trim restore remains the one blocked path.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`). [Verified]

### Fixed
- **An extreme input level could silence the plugin permanently.** A finite but astronomical
  sample (the kind a broken upstream plugin emits, not one a DAW produces) could overflow a stage
  that carries gain — an EQ biquad, the compressor's squaring RMS detector, the clipper's colour
  polynomial, the oversampler's filters — and the stage then held a NaN for ever. The engine's
  boundaries substituted `0.0f` for the non-finite value on the way out, so the output was silence
  and nothing signalled that anything had happened: only re-opening the session (or any host
  action that re-prepared the plugin) brought the sound back. Those boundaries now record the
  substitution and the affected stage's filter state is cleared, so the engine recovers by itself
  within the block. A hostile input buffer is unchanged: non-finite input is still zeroed before
  any state sees it, at no cost.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) —
  `testExtremeLevelDoesNotSilencePermanently`, **five** stimuli (counted from the test's own
  `run(...)` cases, not from an earlier entry — the Post-EQ case was added a round later), each
  mutation-verified against the matching half of the fix. [Verified]

- **…including with oversampling on, where the recovery was incomplete.** The first fix repaired
  the engine's own stages but not the oversampler, whose default (minimum-phase) filters are
  recursive: one infinite state fed itself and every later sample stayed non-finite, so an extreme
  input still silenced the plugin permanently at any oversampling factor. The oversampler is now
  reset when it is the stage that produced the value. The repair of the other stages also stopped
  being a blanket reset — it clears only the values that are actually non-finite, so recovering
  from a poisoned detector filter no longer snaps the compressor's gain reduction to unity.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) — the oversampler case of the same test, now at
  `FLT_MAX` (the level the polyphase filters overflow at), mutation-verified. [Verified]

- **…and with the EQ in the Post position, where the same silence survived two rounds of fixing.**
  The Post EQ sits after the limiter but before the ceiling clamp, and the limiter's *attack* — not
  the ceiling — is what bounds its input: at a short lookahead setting the gain has only fallen to
  ~0.29 by the time an extreme peak plays, and a fully boosted EQ multiplies by ~3.4, so the
  biquad overflows and the plugin goes silent for the rest of the session. Stage E now has the two
  boundaries it was missing (the decimation filters' output and the Post EQ's own), so both stages
  are repaired like the others. Separately, the limiter's detector high-pass is now checked once
  per block rather than on the recovery flag: its corruption produces no non-finite output at all
  (a `NaN` level compares false against the ceiling), so the limiter would have passed everything
  at unity gain for ever without anything to notice.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) — the Post-position case of the same test,
  calibrated against the limiter attack and mutation-verified. [Verified]

- **Meters and Simple-mode adaptation no longer break for the session on one extreme sample.**
  Both are fed signals that are finite but unbounded, and both overflow on a legal float — the
  loudness meters in their K-weighting filter, the adaptive feature extractor when it squares its
  band split. Neither emits audio, so nothing in the engine could notice: the readings became NaN,
  every gate that compares them turned false, and the result was a loudness/true-peak readout
  stuck at silence, a loudness compensation that stopped tracking, an integrated reading that
  stopped accumulating, and a Simple-mode trim vector frozen at a plausible-looking value until
  the plugin was re-prepared. Both are now checked once per block, and a Learn pass that
  accumulated a broken feature is cancelled rather than committing it into the session.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) —
  `testExtremeLevelDoesNotBreakTheMetersOrAdaptation`, one stimulus per stage (Nyquist at full
  scale for the extractor, the bypass leg for the meter), each mutation-verified. [Verified]

- **A double Learn press can no longer save a reference measured from one block.** The Learn
  command was published as a code plus a separate "pending" flag, so an engine that picked the
  command up between those two writes could have it re-raised behind it and carry it out twice —
  and a "finish and start again" press carried out twice finishes the pass it just started, one
  block old. The command is now a single value the engine takes whole.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) — `AnabasisEngine::learnCmd`; the interleaving
  itself is not headlessly reproducible, and `testStopThenStartInOneBlockKeepsBoth` pins the
  composed semantics across the change. [Partially Verified]

- **A Learn analysis that measured through an extreme sample is no longer saved as the reference.**
  Ending a Learn pass on the block after such a sample stored a broken measurement as the learned
  reference, which froze Simple-mode adaptation for good — every trim is derived from that
  reference — and was then written into the project file, so reloading did not clear it. The
  commit now refuses a measurement that overflowed, with the same outcome as an empty pass (the
  previous reference stays), and a restore that carries one reads as never-learned.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) —
  `testALearnPassThatOverflowedIsNotCommitted`, both writers mutation-verified. [Verified]

- **P5 groundwork — meter holds now reset on demand and on session load.** The integrated-LUFS
  figure and the true-peak max-hold are session-cumulative by design; until now only re-preparing
  the plugin cleared them, so loading a different project kept the previous programme's true-peak
  maximum on the meter. A reset request (wired to the P5 meter panel) clears both at the next
  audio block, and loading a session issues it automatically. Resetting during playback measures
  only post-reset material — a reset cannot be pinned at the old programme's loudness by a
  measurement window that straddles it. A transport stop still clears nothing: stop/start must
  not cancel a Learn pass or a mastering measurement (that decision is now recorded, not open).
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) — `testMeterResetClearsSessionHolds`,
  `testGrRingResetEpoch`, all halves mutation-verified. [Verified]

- **P5 — the interface exists.** The full Simple and Advanced views in the RollyTech family frame
  (Anamorph's top bar, Backdrop overlays, glass language and thin-arc controls, with Anabasis's
  gold/amber accent): the one large Loudness knob with Character / Tone / Ceiling (+lock) and the
  monitor toggles; the four Advanced zones (Comp · Clip/Colour · Limiter · EQ) over the whole
  per-stage parameter surface with a live clip transfer curve, an EQ response curve and
  per-stage GR meters; the loudness panel (LUFS M/S/I, streaming-target lines with the
  loudness-penalty estimate, dBTP, PLR — click resets the session holds); the scrolling
  GR/waveform history; the dismissible input/output spectrum overlay; Settings (oversampling,
  phase, offline quality, UI scale 80–200 %, animation, tooltips, metering options) and About;
  Learn with a minimum-pass countdown and an empty-pass readout; macro detach badges with
  reset-to-macro; accessibility names on every control. Editing a macro-managed parameter by
  hand now detaches it from the macro until the next macro gesture (or reset-to-macro) — restores
  and automation never detach.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) — suites + pluginval L8 ×3 both modes with the
  editor open under xvfb; GUI appearance itself is Level-5 manual validation. [Verified]

- **P6 (in progress) — undo, factory presets, and the measured budget.** Per-slot undo/redo in
  the top bar (one drag = one step; automation never pollutes the history; undoing an edit also
  restores its macro-detach state; preset applies undo as one step). Five factory presets from
  the brief's list (Transparent Master, Loud Pop, EDM Club, Vocal Forward, Tape Glue — settings
  are drafts until the listening pass) in a FACTORY menu section, with the preset name showing a
  dirty mark once the state is edited. A locked ceiling is never moved by any preset. The
  performance budget is now measured, not asserted: the 48 kHz / 4× working case runs at 3 % of
  one core on the recorded machine (`docs/architecture/PERFORMANCE_BUDGET.md`), and pluginval
  passes at strictness 10 locally in both modes.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) — `testUndoIsPerSlotGestureCoalescedAndMaskWide`,
  `testFactoryPresets`, `AnabasisBench`. [Verified]

- **v0.1.0 completion (2026-08-02, under the owner's blanket approval — every item ⊕ for the
  post-v0.1.0 fine review).** The adaptive release trim now reaches the AUTO release path
  (ADR-0013): with auto release on — the factory default — the limiter's two release poles scale
  by the trim's `2^octaves` factor with their ratio preserved, so all four §5.4 adaptive
  behaviours are audible at defaults instead of three. **Freeze memory now restores**
  (ADR-0014): a frozen slot saved and reloaded — session load or A/B switch — reproduces its
  latched trim vector exactly (applied at the transition duck's silent bottom, so the restore is
  click-free), instead of re-latching from whatever the engine happened to hold. The factory
  bank grows from five to the brief's **twelve** presets (adding Rock Punch, Hip-Hop Low End,
  Acoustic Warmth, Classical Dynamics, Podcast Voice, Cinematic Wide, Lo-Fi Crush — draft values
  until the listening pass). CI's pluginval gate rises to strictness 10 on all three platforms.
  Also fixed: the Windows CI state-suite crash (stack overflow from two 128 KB inline capture
  buffers per engine instance; storage is heap-allocated at construction now, and both test
  suites print unbuffered so a crash can no longer swallow its own output).
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) — `testAutoReleaseFollowsTheTrimScale`,
  `testFrozenTrimRestore` (seven mutants), `testFactoryPresets`; ADR-0013/ADR-0014;
  OQ-007/OQ-013/OQ-014/OQ-016 → Resolved. [Verified]

### Fixed

- **Undo and redo are click-free, and they restore a frozen slot's remembered adaptation**
  (2026-08-03). An undo step is a bulk swap like a preset load or an A/B switch and now dips
  through the same silent transition; before this it stepped, and on a frozen slot the remembered
  adaptation was not restored at all — the values could instead surface later, at the next A/B
  switch, changing the sound of a different setting.
- **Saving a project just after loading a frozen setting keeps the right remembered values**
  (2026-08-03). A save landing in the short window between the load being picked up and taking
  effect wrote the previous values and discarded the loaded ones permanently; the editor's own
  preset-dirty polling reached that window in ordinary use.
- **The meter reset no longer lets a slice of the previous, louder material into the fresh
  integrated reading** (2026-08-03): the measurement window that was in progress at the moment of
  the reset is now excluded, which is what the reset was always documented to do.
- **The Settings drop-downs select what they say, and keep showing it** (2026-08-03).
  Oversampling, phase and offline-render quality were each off by one — picking "Off" turned
  oversampling on, "Minimum" gave linear phase, "Follow" forced maximum quality — and all three
  opened blank until touched. They also now follow the stored setting, so a project loaded with
  the Settings panel open no longer leaves them showing the previous project's choices.
- **Automating Bypass with the window open no longer risks an audio dropout** (2026-08-03): the
  window asked the message queue for work from inside the audio callback, which can take a lock.
- **Factory presets change the sound** (2026-08-03). A factory preset expresses itself through the
  three macro knobs, and the translation from those positions to the compressor / clipper /
  limiter / EQ settings was being cancelled as the preset loaded, so every factory preset left the
  processing at its defaults. User presets were never affected (they carry every value themselves)
  and still land exactly as saved.
- **The preset name's edited mark tells the truth after a project load or an A/B switch**
  (2026-08-03). The record of "what this preset landed" is now kept per A/B slot and dropped on a
  load, so a freshly loaded preset no longer shows as edited and slot A is no longer marked
  against a preset applied in slot B.
- **Double-clicking a knob to reset it now behaves exactly like alt-clicking it** (2026-08-03): it
  is undoable, and on a Simple-mode macro parameter it detaches from the macro the same way.
- Smaller: a host that reports control gestures across threads can no longer split one drag into
  the wrong undo steps, and a macro knob grabbed right after such an edit re-engages it as
  documented; a well-formed preset file from another plugin is refused without costing an undo
  step; the transfer/EQ curve display follows a host sample-rate change; closing the plugin can no
  longer race its own background housekeeping.
  Evidence Source: **PR #6** (`skyRolly/Anabasis`) — `testUndoRequestsDuck`,
  `testFrozenTrimRestore` (undo + save-window cases), `testMeterResetIgnoresTheStraddlingSubBlock`,
  `testAGestureEndWithoutACountedBeginIsIgnored`; all mutation-verified. [Verified]

The first entry will be `[0.1.0]`, cut when the post-v0.1.0 fine review clears the tag.

---

## Entry template

```
## [0.1.0] — 2026-MM-DD

### Added
- <user-visible change>.
  Evidence: commit 6a24b82 (or PR #NN). [Verified | Partially Verified | Unverified Historical Reconstruction]
```
