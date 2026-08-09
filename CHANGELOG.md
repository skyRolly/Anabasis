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

**Entry template.** It lives HERE, above the entries, and not at the foot of the file: the
release workflow extracts a version's notes as everything from a version heading to the next
h2 heading — and the oldest entry has none after it, so a template sitting after the entries
was published inside the release notes. **The h2 level is reserved for version entries**: any
other `## ` heading ends a release's notes where it stands, so a section appended to the foot
of this file truncates the oldest entry's notes and a sub-section demoted from `### ` to `## `
truncates its own. Sub-sections stay at `### ` or deeper, which is why the P1–P6 history below
runs in `###` sections. `scripts/check-docs.py` fails the build on both. A fenced block is
read as data, so the sample heading immediately below is not mistaken for structure.

```
## [x.y.z] — YYYY-MM-DD

### Added
- <user-visible change>.
  Evidence: commit 6a24b82 (or PR #NN). [Verified | Partially Verified | Unverified Historical Reconstruction]
```

---

## [Unreleased]

### Added
- **Preset identity** (ADR-0022, the product-family port of Anamorph's ADR-0024): a factory
  preset is identified by an immutable internal id and a user preset by its file on disk, so a
  user preset saved under a factory preset's name is now the one selected — the drop-down marks
  exactly one row, `‹ ›` steps from the row that was actually loaded, and the selection survives
  undo, A/B, Copy and a session reload. A stored identity that no longer resolves (a preset
  deleted, renamed or moved; a file loaded from outside the preset folder) selects **no** row
  rather than a same-named substitute; sound restoration is unaffected in every such case. The
  identity is three additive per-slot strings in the **session** only — user `.anabasis` preset
  files are byte-for-byte unchanged, and sessions saved before this change load exactly as
  before. Evidence Source: PR #12.

### Fixed
- A user preset sharing a factory preset's name no longer shows the selection mark on **both**
  menu rows, and saving over a factory preset's name no longer leaves the mark on the factory
  row. Evidence Source: PR #12.

---

## [0.1.1] — 2026-08-07

**The first build released from this repository.** v0.1.0 was declared code-complete on
2026-08-02 but was never tagged: the post-v0.1.0 review rounds landed first, and the owner cut
the release one patch level on. This entry therefore carries the whole P1–P6 development *and*
the 0.1.1 round — the sections below run newest first.

### Added
- **Waveform Statistics panel** — the metering panel now shows seven readings, identically in
  Simple and Advanced: momentary (400 ms), short-term (3 s), integrated, **true peak**,
  **sample peak**, **RMS** (50 ms Hann window) and **loudness range** (LRA, EBU Tech 3342),
  plus PLR. Two new Settings choose which STANDARD two of them follow: *Integrated*
  (BS.1770-2+ gated, the default, or BS.1770-1 ungated) and *RMS Reference* (AES-17, the
  default, or mathematical). The true peak is no longer hideable — the *True-Peak Meter*
  toggle and the field behind it are gone. Reading TP against the new SP row gives the
  inter-sample overshoot directly. ADR-0020; session field `int_tpMeterOn` removed,
  `int_integratedStd` / `int_rmsRef` added (an old session's value for any of the three is
  handled by the schema's read rules — no migration needed).
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]
- **Comp Stereo Link** — the glue compressor gains the adjustable stereo link the limiter
  already had (0–100 %, default 100 %). At the default it is bit-for-bit the fully linked
  detector it always was; lower values let each channel breathe on its own. Named apart from
  the limiter's *Stereo Link* so the two automation lanes cannot be confused. The parameter
  surface is now 50. ADR-0019.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]
- **Installers, and a release pipeline that builds them** — a Windows Inno Setup installer, a
  macOS `.pkg` with component selection (VST3 / AU / Standalone), and Linux
  `install.sh`/`uninstall.sh` inside the zip, each with an `INSTALL.txt`. A tag now drives a
  validate → build → draft-release pipeline that archives the per-platform trees, attaches the
  installers, publishes SHA256SUMS and a build manifest, and takes its release notes from this
  file. ADR-0021; closes the OQ-007 deferral.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]

### Changed
- **Copy (A/B) is undoable and no longer destroys history** — copying one slot's sound into the
  other now records an undo step on the DESTINATION slot and keeps that slot's earlier history
  beneath it, so one Undo reverts the Copy and further Undos walk back through the slot's own
  edits. **The ADV view switch is undoable too.** Neither travels with an A/B compare: switching
  slots still never resizes the editor. ADR-0018.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]
- **Frequency text entry speaks mastering shorthand** — on the high shelf (1–20 kHz) a bare
  number up to 20 is read as kHz, so `8` lands 8 kHz; on the full-range bells (20 Hz–20 kHz) the
  pivot is the knob's own 20 Hz floor, so `19` lands 19 kHz while `20` stays 20 Hz. `8k`,
  `8 kHz` and `8000` all mean the same thing everywhere.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]
- **Graph-well switch, GR history and zone layout** — the single-name corner chip is replaced by
  a two-segment SPEC | GR switch that shows both modes and floats translucently above the trace
  (the old chip was masked by the GR zero-line); the GR history no longer shimmers while
  scrolling (fixed-identity decimation buckets and a filled waveform path instead of a
  re-phasing comb of 1 px bars); the four zone combos span their panel, so *Transparent* no
  longer clips; the dither combo gains its caption.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]
- **About panel** — a shorter description in the Version line's colour, and the URL sits
  directly above the copyright, matching the family layout.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]
- **The knob corner dot explains itself** — each of the nine macro-managed knobs carries the
  detach legend in its tooltip, and the manual states what the dot is not: not an
  edited-since-preset mark, and not cleared by turning the knob back, because the knob is still
  off macro control.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]
- **Artifact contents match the sibling product's** — `NOTICE` and `THIRD_PARTY_LICENSES.md` are
  no longer copied inside the archives; they ship as version-named assets on the release page,
  where they accompany every distribution route including the installers. ADR-0021 amends
  `RELEASE_POLICY.md` accordingly.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]

### Fixed
- **The left channel could fall silent on a mono source** (KI-009). The plugin refused the
  mono→stereo layout the sibling accepts, so a host with a mono source had to negotiate
  stereo→stereo and feed the signal on one input pin and silence on the other; this chain is
  strictly dual-mono, so the silent pin produced a silent output channel in both modes. Mono
  input is now accepted and duplicated before the chain. The headless stereo battery gained the
  case that reproduces it.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]
- **A Bypass or monitor-toggle click no longer eats an Undo press** — those clicks used to mint
  an undo step whose restore changed nothing. ADR-0018.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]

### Removed
- **The Settings *True-Peak Meter* toggle** and its session field, with the statistics panel that
  makes it meaningless (see Added). ADR-0020.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]

---

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
  `testFrozenTrimRestore`, `testFactoryPresets`; ADR-0013/ADR-0014;
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
- **The true-peak readout turns red against YOUR ceiling** (2026-08-03), not against a fixed
  −1 dBTP: at any other ceiling — including the −0.5 the "EDM Club" preset sets — it warned at the
  wrong level, staying neutral while genuinely over or lighting up while legal.
- **A knob is no longer wrongly marked as "taken off the macro" after loading a preset or project**
  (2026-08-03). A knob edit arriving moments before the load could be applied to the freshly
  loaded settings, leaving that knob stuck at the old value and no longer following
  Loudness/Character/Tone — and the wrong state was saved with the session. Related: grabbing a
  macro knob now re-engages the whole set within the same update, instead of leaving one
  parameter reading as re-engaged while it still held the old value.
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
- **Automating Loudness, Character or Tone no longer takes back a knob you had just taken off the
  macro** (2026-08-03). When the host moved a macro while a knob edit was still being picked up,
  that knob was written from the macro curve and only afterwards marked as detached — so it stuck
  at the macro's value instead of yours, and the session was saved that way. Every route into the
  macro update now applies your edits first.
- **The Settings panel follows a project load completely** (2026-08-03). The oversampling, phase
  and offline-quality drop-downs were fixed earlier in this branch; the three streaming-target
  checkboxes were not, so a panel left open across a load kept showing the previous project's
  targets while the loudness meter already drew the new project's target lines — and the first
  click on a checkbox then read as toggling the wrong one.
- **Tooltips are drawn in the product's own style** (2026-08-03): the tooltip window was created
  without being told which look-and-feel to use, so it fell back to the JUCE default and the
  designed capsule never appeared.
- **Opening a project resets the meters immediately, not at the next note** (2026-08-03). Loading
  a session clears the integrated loudness and true-peak hold — but the clear only took visible
  effect once audio started, so a project opened with the transport stopped showed the previous
  session's readings for as long as it stayed stopped.
- **The preset name's edited mark updates immediately after undo, redo, A/B, a preset change or a
  save** (2026-08-03), instead of keeping the previous state's mark for up to a third of a second.
- **Knobs are drawn where they are the moment the window opens** (2026-08-03), instead of sweeping
  up from their minimum over the first few frames.
- **Dragging a knob's number behaves like dragging the knob** (2026-08-03): the edit is now
  undoable, and on a Simple-mode macro parameter it takes that control off the macro the same way.
- **Grabbing a macro knob puts every knob you had taken off it back on the curve** (2026-08-03),
  even when the grab moves nothing. Previously a click-and-release re-attached them in name only:
  they stopped showing as taken-off but kept the values you had dialled in.
- **Copying one A/B slot over the other starts that slot's undo history fresh** (2026-08-03).
  Before, the first undo after switching to the copied-into slot restored a state from before the
  copy — silently discarding both the copy and that slot's last edit.
- **Undoing a preset change restores the edited mark too** (2026-08-03): the name reverted but the
  " *" kept comparing against the preset that had just been applied.
- **"Reset to macro" can be undone** (2026-08-03). It re-attaches every knob to the macro and
  re-lands nine values at once, and it was the only change of that size the user could not take
  back.
- **An un-frozen A/B slot no longer saves a Freeze memory it does not hold** (2026-08-03), and a
  saved Freeze memory is no longer overwritten with empty values by a plugin that has been opened,
  or whose sample rate the host has just changed, before any audio has played through it again.
- **The spectrum display no longer shows the wrong frequencies right after a sample-rate change**
  (2026-08-03): the analysis it had already collected at the old rate stayed on screen for about a
  tenth of a second, drawn against the new one.
- **Switching A/B mid-drag no longer puts an undo step on the wrong slot** (2026-08-03). The step
  described the slot you left, so undoing on the slot you arrived at restored values it never had.
- Smaller: a host that reports control gestures across threads can no longer split one drag into
  the wrong undo steps, and a macro knob grabbed right after such an edit re-engages it as
  documented; a well-formed preset file from another plugin is refused without costing an undo
  step; the transfer/EQ curve display follows a host sample-rate change; the oldest column of the
  gain-reduction history can no longer be drawn from an entry the audio thread is still writing;
  closing the plugin can no longer race its own background housekeeping.
  Evidence Source: **PR #6** (`skyRolly/Anabasis`) — `testUndoRequestsDuck`,
  `testFrozenTrimRestore` (undo + save-window cases), `testMeterResetIgnoresTheStraddlingSubBlock`,
  `testAGestureEndWithoutACountedBeginIsIgnored`,
  `testThePostedDrainAlsoTakesTheWrapperBitsFirst`,
  `testGrHistoryWindowNeverAsksForTheHeadSlot`,
  `testTheSettingsPanelFollowsAProjectLoad`,
  `testTheWholeTickIsSuppressedInsideARestore`,
  `testMeterResetClearsSessionHolds` (the no-audio-at-all case),
  `testTeardownAndReengageInvariants`, `testStateReplacementAndHistoryConsistency`,
  `testPreparedStateAndSlotOwnership`; all mutation-verified. [Verified]

- **Changed under the same PR:** the "Transparent Master" and "Classical Dynamics" factory presets
  now select the **Clean** colour model explicitly. Both are described as untouched, and both were
  inheriting the parameter's Tape default — inaudible at their settings, but only because the
  colour depth the macro layer writes happens to be ~0 there. ⊕ with the rest of the factory values,
  pending the listening pass.
  Evidence Source: **PR #6** (`skyRolly/Anabasis`). [Verified]

### Changed (round-2 owner directive, 2026-08-05; recorded by **ADR-0015**)
- **Defaults:** every true-peak switch (limiter mode and the true-peak meter row) now defaults
  **off**; the Ceiling defaults to **−0.1** everywhere (parameter, engine POD, and the one factory
  override that pinned −0.5 is gone). The registry snapshot was deliberately re-frozen — nothing
  has shipped, so the change is compatibility-free by the contract's own terms, and
  [`ADR-0015`](docs/architecture/design-decisions/ADR-0015-pre-ship-contract-refreeze.md) records
  both the rationale and the condition that closes that window.
- **The Ceiling now says which limit it is holding.** With true-peak mode off — the new default —
  the ceiling is a **sample-peak** limit, so its readout prints `dB`; engaging **TP** moves
  detection to the oversampled rate and the readout becomes `dBTP`. It used to print `dBTP`
  unconditionally, which claimed an inter-sample guarantee the default configuration does not
  make. The number, the automation and the saved state are unaffected — only the unit shown.
- **A "Default" preset** opens the plugin: factory index 0 with zero intents, the bank now
  Default + the brief's 12. A fresh instance reads clean and stars on the first edit.
- **Streaming-platform analysis removed outright:** the loudness-target lines, penalty rows,
  target checkboxes and their session field (`int_meterTargets`) are gone — platforms normalise; a
  master is pushed against the ceiling, not a platform figure. Old sessions carrying the field
  load unaffected: the defaults-first read rules ignore it and a re-save does not write it back.
  The schema removal is recorded by
  [`ADR-0015`](docs/architecture/design-decisions/ADR-0015-pre-ship-contract-refreeze.md).
- **One graph well, two views:** the spectrum and the scrolling GR history are switchable modes
  of the same panel in both Simple and Advanced, toggled by the chip on the graph's own corner
  ("GR" / "SPEC" — it names the view you switch to); the Settings toggle is gone. The spectrum's
  low end no longer staircases (the sibling's two-regime column read: Catmull-Rom under 1.5
  bins/column, bin averaging above).
- **A highly visible TP switch** joins the Ceiling in the Simple view (stacked above LOCK) —
  the same parameter as the Advanced limiter zone's TP.
- **The sibling's UI grammar lands:** circle-arrow undo/redo icons, its About layout (left-aligned
  link, no underline) with a generated one-sentence product description, its shorter A/B oval, the
  XS/S/M/L/XL five-step UI scale (M = original), Title-Case Settings terminology ("Offline
  Render", "UI Scale", "UI Animations", "Follow Online"), and Save left / Cancel right in the
  save dialog.
- **A complete tooltip set** in the sibling's voice — one terse line per parameter, held in one
  table; accessibility titles stay the registry names. The graph-well chips explain themselves;
  Bypass deliberately carries no tip.
- **Layout coherence, verified against rendered snapshots:** every Advanced zone's mode combo
  sits in the zone header (the truncated "AUTO"/"TP"/"LOCK" labels now fit their cells), Input
  Gain and SC HPF ride the utility strip as faders, and the Settings panel shrinks to its
  content.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]

### Known issue reports under investigation
- **A field report of the left channel falling silent** could not be reproduced headlessly: a
  six-configuration wrapper battery (defaults, driven macros, editor alive, 16× linear
  oversampling, factory preset, session round-trip) measures both channels carrying audio within
  6 dB in every case, and remains in the suite as a permanent guard. `docs/KNOWN_ISSUES.md`
  KI-009 records what the probes exclude and the environment details needed to proceed.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]

These P1–P6 sections belong to the `[0.1.1]` entry above: the release the tag cuts is the first
one this repository has produced, so its notes are the whole development, not a delta against a
predecessor that was never published.
