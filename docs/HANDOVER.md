# HANDOVER.md

Operational status snapshot for technical handover. Update on every release and at every phase
boundary (`docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md`). Facts are Verified from the
repository; fields with no repository evidence are marked `TODO` rather than invented
(constraint C7).

Snapshot taken at **v0.1.0 code-complete (2026-08-02, under the owner's blanket approval)**, and
carrying every boundary before it: the file opened at **P0 → P1 (2026-07-31)**, when
`docs/DESIGN.md` was signed off and the eleven ADRs it authorised were Accepted and registered,
and the P2/P3/P4/P5 phase summaries below were added at their own boundaries rather than
replacing it. The status table is the row of record for the CURRENT state; the summaries are
history and are not rewritten. This is a phase-boundary update, so all three
`DOCUMENTATION_LIFECYCLE_POLICY.md` phase-completion targets are covered: this file, the
coverage audit, and the §13 phase summary (below).

The repository's starting point was the **bootstrap** — the migration of Anamorph's governance
system, documentation library, build/CI scaffolding and working conventions into a new, otherwise
empty Anabasis repository, together with the product brief (`docs/DEVELOPMENT_BRIEF.md`).

## Operational status

| Field | Value |
|---|---|
| **Current Version** | 0.1.0 (pre-release; `project(Anabasis VERSION 0.1.0)` in `CMakeLists.txt`). `CHANGELOG.md` has no released entry; the P1 skeleton is under `[Unreleased]`. |
| **Current Phase** | **P6 — v0.1.0 CODE COMPLETE (2026-08-02, under the owner's blanket approval; every item taken under it is ⊕ for the post-v0.1.0 fine review).** The last two owner gates are decided and wired: **ADR-0013** (OQ-016 — the release trim scales the auto poles; all four adaptive behaviours audible at defaults) and **ADR-0014** (OQ-013 — the frozen-trim vector restores: captured at save, staged on ADR-0012's row, applied at the duck's silent bottom; MODE inv 3's last gap closed). OQ-014 resolved (reading 1 — listener-guard row) and OQ-007 resolved (v0.1.0 ships plain zips). The preset bank is at the brief's 12 (5 brief-named + 7 ⊕); the brand checklist is provisionally passed under the approval (the real Level-5 human pass is the first fine-review item); CI pluginval strictness is raised to **10**. Still owed before tagging v0.1.0: the human fine review (brand pass, DAW matrix audition, preset/curve listening pass), OQ-002 (licence — blocks commercial distribution only), OQ-008 first-party verification, OQ-009. Previously: P5 UI code complete at L8 (2026-08-02); P0 closed 2026-07-31; P1 closed 2026-08-01; P2/P3/P4 complete 2026-08-01. |
| **Branch Strategy** | Feature branch → PR into `main`. CI builds every branch; `main` carries shipped versions. Release tagging convention: annotated `vX.Y.Z`, wired to a `release.yml` at P6. |
| **Build Status** | **Builds green on Linux** (P1 skeleton, 2026-07-31): `CMakeLists.txt` per ADR-0008 (five targets, JUCE 9.0.0 @ the pinned SHA fetched via FetchContent, C++20, warning-free under the recommended flags), `src/` + `src/dsp/` + `src/gui/` exist. The `preflight` guard now takes its ready=true path, so the full 3-OS matrix runs in CI; Windows/macOS results arrive with the first CI run of this commit. The `docs` job continues to run on every push and gates nothing. |
| **Test Status** | **381 checks green on Linux**: `AnabasisTests` (225 — ADR-0013 auto-release-follows-the-trim-scale, null-with-defaults bit-exact, impulse-at-allowance for four lookahead values, ceiling clamp, control/gain priming, limiter window coverage and alignment, smoothing of ceiling and lookahead, hostile-input finiteness, self-heal recovery, recovery from a stage that overflows on a FINITE input (EQ biquad in BOTH positions, RMS detector, colour c⁵, polyphase IIR up and down — `testExtremeLevelDoesNotSilencePermanently`) and from the stages that emit no audio to check (the BS.1770 meters and the §5.4 feature extractor — `testExtremeLevelDoesNotBreakTheMetersOrAdaptation`) and a Learn pass that measured through an overflow never becoming the saved reference (`testALearnPassThatOverflowedIsNotCommitted`), bypass null, EQ frequency response/smoothing/positions, the ADR-0002 post-shelf ceiling stimulus, compressor static curve/detectors/mix/two-stage auto release/sidechain HPF, clipper curve/compensation/ADAA aliasing/colour models/dynamic tame, true-peak accuracy, limiter link/styles/preserve/two-stage auto/detector HPF/dBTP mode, the full OS latency matrix, OS aliasing/transparency/bypass/ceiling, dither modes, the §2.8 duck on rewires/latches/requests, LUFS calibration/gating/windows, inv-10 monitoring honesty incl. the mid-stream offline flip snap, delta, the duck-bottom hold, the post-latch refill hold, a request held through the out-leg, delta covered by the duck, the last-staged-restore rule, stale detector state, limiter control smoothing incl. link/preserve/HPF glides) plus meter publication and the GR ring in the state suite and `AnabasisStateTests` (156 — the ADR-0014 frozen-trim restore (`testFrozenTrimRestore`: both landing sites, both staging sites, the capture, the no-audio mirror, freeze-off inert — seven mutants), the 12-preset factory bank, registry snapshot vs the frozen fixture, 49/9 counts, raw-exact byte-identical round-trip and its fixed-point precondition, structural-tolerance read rules, batched latency notification, corrupt/foreign no-op, macro fixed point, restore-vs-macro-drain, A/B tier behaviour, preset contract, cache mapping, the ADAPTIVE missing-field defaults, meters reading the render not the monitor path, load-then-save with no audio between, the zero-length-block publish guard). `AnabasisTests` also pins the limiter push's chain position (`testLimiterPushDoesNotDriveTheClipper`) and that a realtime→offline flip does not duck the render (`testOfflineFlipDoesNotDuckTheRender`) while the return edge stays ducked (`testReturnFromOfflineIsDucked`), and that an EQ-position change on the offline-entry edge starts from cleared filter state (`testOfflineEntryClearsEqStateOnAPositionChange`). **pluginval L10 green ×3 in both modes on Linux (editor under xvfb)** — the P6 bar; CI now gates at 10 on all three platforms. Re-count from the suites' own output when editing this row; it has gone stale once already. |
| **Release Status** | Pre-0.1.0. Nothing has ever left this repository, which is why the compatibility contract can still be shaped at zero cost (`COMPATIBILITY_POLICY.md` §"When the contract starts"). |
| **Known Blockers** | **No code blockers.** Every formerly `Blocking` question is Resolved: OQ-013 by **ADR-0014** (2026-08-02 — the frozen-trim restore is wired and mutation-verified), OQ-016 by **ADR-0013**, OQ-014 (reading 1) and OQ-007 (plain zips) by the same owner call, OQ-010/OQ-011/OQ-004/OQ-005/OQ-015 earlier with their ADRs. What blocks the RELEASE rather than the code: the post-v0.1.0 human fine review (the blanket approval's other half — brand pass, DAW audition, listening pass over the ⊕ constants/presets), **OQ-002** (JUCE licence tier — blocks commercial distribution only), OQ-008's first-party value verification, OQ-009 (owner metadata). This row must agree with every `Blocking` entry in `docs/OPEN_QUESTIONS.md` — check it there, not here, when adding one. |
| **Pending Tasks** | **P1–P6 code work is DONE** (phase histories in the summaries below; the v0.1.0 completion summary carries the final batch). What remains is the **post-v0.1.0 fine review** — the other half of the owner's blanket approval: (a) the item-by-item Level-5 brand pass (`BRAND_CONSISTENCY_CHECKLIST.md` — provisionally passed, boxes deliberately unchecked); (b) the DAW matrix audition (`COMPATIBILITY_MATRIX.md` targets); (c) the listening pass over every ⊕ — trim mapping constants, §5.5 macro curves, tame/model weights, the 12 factory preset value sets, the gold/amber accent; (d) a second look at every decision dated 2026-08-02 (ADR-0013, ADR-0014, OQ-007/OQ-014 readings, the 7 preset names/values); (e) OQ-008's first-party value verification at release; (f) the 3-OS CI confirmation of this batch (suites + pluginval 10). Commercial release additionally waits on OQ-002 (licence tier), OQ-009 (owner metadata), and the OQ-007-deferred packaging pipeline. |
| **Roadmap** | P0 research & design → P1 skeleton (pluginval L5) → P2 DSP core → P3 metering engine → P4 Simple adaptive engine → P5 UI → P6 polish & release (pluginval L10, DAW matrix, docs). `DEVELOPMENT_BRIEF.md` §11. v2 candidates (codec preview, reference matching, dynamic EQ, multiband limiting) are out of scope — leave architectural room only. |
| **Ownership** | `TODO: no owner/team metadata in the repository. Requires project-owner input (OQ-009).` Company of record: RollyTech. |

## v0.1.0 completion summary (2026-08-02, under the owner's blanket approval)

**The directive.** Every human-review/owner-decision gate is pre-approved so the complete first
version can be assembled; the item-by-item fine review happens against the finished v0.1.0.
Everything taken under the approval is dated 2026-08-02 and flagged ⊕.

**Decisions taken and wired.**
- **ADR-0013 (OQ-016):** the release trim reaches the AUTO release path —
  `LookaheadLimiter::setAutoReleaseScale` scales both auto poles by the same `2^octaves` factor
  the manual path applies, ratio preserved. All four adaptive behaviours are now audible at
  factory defaults. Guarded by `testAutoReleaseFollowsTheTrimScale` (mutation-verified).
- **ADR-0014 (OQ-013):** the frozen-trim vector RESTORES — captured from the published latch at
  save (with the no-audio mirror rule), staged on ADR-0012's record row, applied via
  `AdaptiveEngine::injectTrims` at the duck's silent bottom or the unprimed direct-adopt, only
  for a freeze-ON adopted surface. Both restore paths stage it (`applySlotToLive` AND
  `setStateInformation` — the session-load stage was found MISSING while writing the test and
  fixed before it shipped). MODE inv 3's Freeze story is whole. Guarded by
  `testFrozenTrimRestore` (seven mutants, each killed by a distinct check).
- **OQ-014 (reading 1):** the MacroEngine guard atomics are the AsyncUpdater shape
  ADR-0005/ADR-0011 mandate; `THREADING_POLICY.md` gains the listener-guard row, no new ADR owed.
- **OQ-007:** v0.1.0 ships as plain zips; installers/release-pipeline deferred to the first
  commercial release alongside OQ-002/OQ-012.
- **Preset bank → 12** (brief §9): the 5 brief-named presets + 7 ⊕ genre/purpose additions
  (Rock Punch, Hip-Hop Low End, Acoustic Warmth, Classical Dynamics, Podcast Voice, Cinematic
  Wide, Lo-Fi Crush), all override tables through the shared lock/exclusion core.
- **Brand checklist provisionally passed** (the two deviation candidates provisionally accepted);
  the real Level-5 pass leads the fine review. **CI strictness 8 → 10** (`build.yml`).

**Also this date (before the approval):** the Windows CI state-suite crash was diagnosed and
fixed — the spectrum commit's two ScopeBuffers held 2×128 KB inline arrays per engine, and three
stack-allocated processors overflowed Windows' 1 MB default thread stack (Linux's 8 MB hid it);
storage is now ctor-allocated heap vectors, push path unchanged, and both test mains run
unbuffered so a future crash cannot eat its own output.

**What the fine review owes** (the approval's other half): the item-by-item brand pass against
running builds, the DAW matrix audition, the listening pass over every ⊕ (trim mapping, §5.5
curves, tame/model constants, the 12 preset value sets, the accent swatch), OQ-008 first-party
value verification, and a second look at every decision dated 2026-08-02.

## P5 phase summary (`DEVELOPMENT_BRIEF.md` §13)

**Changes.** The editor: family frame (46 px top bar — wordmark→About, preset browser, A/B, Copy,
Settings, ADV, Bypass-rightmost with red pill + dim overlay), Backdrop About/Settings/Save-preset,
whole-window UI scale (80–200 % composed with host DPI, persisted), the §6.2 Simple view (the one
big knob; Character/Tone/Ceiling+lock; Comp/Delta/Freeze/Learn; live out-LUFS) and the §6.3
Advanced view (four zones over the whole per-stage surface, utility + read-only macro rows, the
shared metering strip). Visualisers, all against the published atomics and rings: LoudnessMeterView
(M/S/I + targets/penalty per OQ-008's cited table + dBTP/PLR; click = meter reset),
GrHistoryView (the ring's first consumer, under the reset-epoch contract), SpectrumView (two new
engine ScopeBuffer taps, FFT strictly GUI-side, dismissible), CurveView (clip transfer + EQ
response through the DSP's own code), per-stage GR bars (comp GR now published beside the
limiter's). The §5.3 detach/re-engage grammar (ADR-0005 → Verified) and the §5.4 Learn UI grammar
(minimum pass, empty-pass readout). The P5 planned edges were settled FIRST: meter-hold reset
(+ state-load clear), GR-ring reset epoch, `AudioProcessor::reset()` deliberately un-overridden.
Accessibility names on every control (the deliberate delta). Brand system per ADR-0009 with
provenance headers; gold/amber accent ⊕.

**Blocked / owed.** The item-by-item HUMAN brand pass + DAW audition (Level 5) close the phase.
**The §7 per-slot undo machinery landed immediately after P5 closure** (see the P6 progress note
below) — the checklist's Undo/Redo deviation candidate is GONE; factory presets and the preset
dirty marker remain P6.
OQ-013 unchanged (Hard Stop). OQ-016 unchanged (release trim inert at defaults — the Advanced
overlay displays it regardless, as recorded). Variable font: fallback path taken (RISK-009).

**P6 progress (2026-08-02).** The §7 undo machinery is in: per-slot stacks over the five-field
SLOT StateSet (cap 128, never serialized — a session load clears both slots' histories),
gesture-gated coalescing (one drag = one step; host automation folds silently; off-thread
gestures degrade to the automation path rather than touching ValueTrees cross-thread), preset
applies bracketed as one step with the parse BEFORE the bracket, undo/redo restores inside
ScopedRestore, and the top-bar ↶ ↷ with live enabled states. The §7 widening rationale is now a
test: undoing a detaching edit restores the value AND its detach bit together
(`testUndoIsPerSlotGestureCoalescedAndMaskWide`, every element mutation-verified — including a
stimulus recalibration where "the stack is unchanged" had to become "the stack is EMPTY" before
the parse-first mutant would die).

**P6 progress, continued (2026-08-02).** `AnabasisBench` (DESIGN §9's procedure, OFF by default)
is in with `docs/architecture/PERFORMANCE_BUDGET.md`: the budget case — 48 kHz · 512 · 4× ·
working — measures **3.0 % of one core** on a 2.1 GHz Xeon against the ≈5 %-of-a-desktop-core
target; 16× reads 9.2 % (48 k) / 20.2 % (96 k) as the stated quality extreme. Whole-engine
numbers; the per-stage ⊕ allocation stays unclaimed pending a profiler pass. **pluginval L10
passed locally ×3 in BOTH modes** — the P6 bar holds on Linux; CI stays at 8 until the phase
formally turns.

**P6 progress, continued (2026-08-02, later).** The factory-preset MECHANISM is in (DESIGN §7's
compiled-in override tables — defaults first, then the intents, through the same lock/exclusion
core as file presets, empty detach mask, one undo step) with the FIVE brief-named presets as ⊕
draft values (same status as the §5.5 curves: tuned at the P6 listening pass, frozen before
v0.1.0), a FACTORY menu section, the ‹ › ring over factory + user, and the preset dirty marker
(slot-tree equivalence against the landed baseline, ~3 Hz poll). The ≥12-preset bank and its
wording remain owner-supplied (C8).

**Next phase plan (P6, remainder).** The rest of the preset bank (owner wording); pluginval L10 ×3 platforms; DAW matrix + automation/state smoke tests; performance
measurement against DESIGN §9's ⊕ budget (bench target + TEST_REPORT/PERFORMANCE_BUDGET);
accessibility polish (focus order audit); packaging/licensing decisions (OQ-002, OQ-007);
the owner calls that stayed open (OQ-008 first-party verification, OQ-009, OQ-012, OQ-013,
OQ-014, OQ-016, accent ratification).

**Risks.** The C++23 canary is unchanged (no library feature adopted). GUI appearance is Level 5 —
nothing headless guards visual regressions; the brand checklist is the control. The GR display's
time base approximates host blocks with the prepared size (recorded at the mapping site).

## P4 phase summary (`DEVELOPMENT_BRIEF.md` §13)

**Changes.** The §5.4 adaptive engine: audio-thread feature extraction (crest, spectral tilt,
transient density — silence-gated; the trim mapping consumes **transient density and tilt**, with
crest published for the UI and reserved for a future mapping), the bounded trim vector
(release ±1 oct, link ±0.2, scHpf
0…+30 Hz, dynTilt 0…+0.5 dB) slewed at ~2 s with hysteresis and applied to per-block effective
settings only — **three of the four are audible in the factory state**: the release trim lands on
`limReleaseMs`, which the limiter reads only in manual-release mode, so with auto release (the
default) it is computed, published, overlaid and latched but silent, which is **OQ-016**, an owner
call rather than a code edit because the alternative changes the default sound; Freeze latching the
vector to the ulp; Learn (analyse → commit reference targets,
`ADAPTIVE` child serialization with the absent-=-never-learned read rule). The exit criterion —
**switching modes does not change the sound** — is pinned sample-identically by
`testModeSwitchIsSoundNeutral`. The invariant-7 null runs with adaptation LIVE because every trim
is inert while its host stage is inert — a structural property, not a gate.

**Blocked, and only ownable by the owner:** the frozen-trim RESTORE transport (message → audio
injection of the per-slot four-vector at the duck bottom) is **OQ-013**, an Architecture Review
Gate + ADR + Hard Stop. Until that ADR lands, a session reload or A/B switch back to a frozen
slot re-latches from the live engine state instead of reproducing the saved vector — the one gap
between the current build and MODE inv 3's full Freeze story. OQ-014 (MacroEngine guard atomics
vs the THREADING_POLICY table) also awaits the owner; it blocks documentation, not code.

**Plan for P5 (UI).** Full Simple + Advanced interface against the Anamorph brand system
(BRAND_CONSISTENCY_CHECKLIST item by item — the phase exit criterion), the visualisers over the
already-published atomics and rings (meters, GR history, transfer curve from ClipSat::transfer,
spectrum rings + GUI-side FFT), Settings page, tooltips, the Learn UI grammar (duck-routed
engage, undo bracketing), trim delta-overlays in Advanced. P5 needs a fresh session with the
Anamorph GUI sources read end to end — it is visual work with a different evidence standard
(Level 5: much of it not headlessly verifiable).

**Risks.** The trim/tame/model constants and the §5.5 curves are all ⊕ drafts awaiting the P6
listening pass — they may move together, and the fixed-point test will catch any curve/default
divergence mechanically. The onset detector's constants were already re-tuned once when a test
caught under-counting; treat its numbers as provisional until listening.

## P3 phase summary (`DEVELOPMENT_BRIEF.md` §13)

**Changes.** The metering engine per DESIGN §2.9: `LoudnessMeter` (K-weighting via the ADR-0009
pre-warped design, 400 ms gating blocks at 75 % overlap, fixed-size histogram accumulator for the
gated integrated figure) calibrated against the standard's own compliance sentence at ≤ 0.1 LU
(48 and 44.1 kHz); dBTP max-hold off the shared 4× estimator; PLR; per-block GR through the first
Audio→GUI SPSC ring (`GrHistoryBuffer`); the wrapper's per-block relaxed-atomic publish (the
THREAD_MODEL meter row, implemented); and the §2.7 monitor layer — Measure+Predict loudness
compensation with loudness-matched bypass, and delta monitoring — both monitor-only, proven
bit-inert in the offline render (invariant 10 live, mutation-verified). KI-002 closed as INC-002.

**The finding of the phase:** the dropped-absolute-gate mutant survived the two obvious gating
stimuli — the relative gate masks an absolute-gate removal completely, because silence sits below
any plausible relative threshold. Its only distinct observable is the effect on the pass-1 mean
that SETS the relative threshold; the killing stimulus places a −38 LUFS band between the correct
and the silence-dragged thresholds. Redundant protection mechanisms hide each other's loss.

**Plan for P4 (Simple adaptive engine).** Feature extraction on the audio thread (§5.4: short-term
LUFS — already built — crest factor, spectral tilt, transient density), the adaptive trims around
release/stereo-link/scHpf/dynTilt within their declared bounds, Learn, and the
`testModeSwitchIsSoundNeutral` invariant. **OQ-013 is the standing Hard Stop**: the frozen-trim
message→audio transport needs its ADR before Freeze's restore path can be wired; everything else
in P4 is independent of it. The spectrum rings and all views are P5.

**Risks.** Unchanged from P2's list, plus: the §2.7 Measure convergence (~3 s) against the P4
adaptive trims' own time constants could interact audibly — evaluate in the P6 listening pass;
the monitor-only semantics (delta inert offline) are DESIGN's words but a user may expect a
rendered delta — a P6 UX question, not a DSP one.

## P2 phase summary (`DEVELOPMENT_BRIEF.md` §13)

**Changes.** The chain went from pass-through to the full §3/§4 DSP: EQ (six RBJ sections,
structural null at flat), glue compressor (log-domain, two-pole auto release), clipper/saturation
(knee-morph ADAA, colour models, dynamic HF tame), the §2.5 limiter (per-channel wedges, stereo
link, styles, transient preserve, true-peak detector per ADR-0003), oversampling (eight instances
built at prepare, latched at the duck bottom, PDC exact across the whole factor × phase matrix
including Force-Max offline), TPDF dither with shaping, and the §2.8 transition layer (KI-001
closed as INC-001). The engine is staged (base front → OS region → base back) and chunk-safe
against oversize host blocks. 183 checks (125 DSP + 58 state), every new behaviour
mutation-verified; `docs/TEST_REPORT.md` records the measured aliasing / accuracy / latency /
dither numbers with method; `REALTIME_SAFETY_AUDIT.md` audits the audio-thread paths.

**The recurring engineering lesson of the phase** (recorded across the coverage-audit entries):
four separate tests initially passed against wrong code or failed against correct code because
the STIMULUS, not the assertion, was wrong — the two-stage-release bounds a single pole could
satisfy, the ADAA stimulus whose alias physics capped improvement at 4.8 dB, the "off-grid" ISP
phase that landed on-grid, and the "fundamental untouched" bound that a real droop-recovery
effect exceeded. The working rule: derive where the property lives (algebra first), assert with
disjoint bounds, and mutation-verify both directions.

**Plan for P3 (metering engine).** Per DESIGN §2.9: K-weighted LUFS M/S/I with BS.1770-4 gating
as a fixed-size histogram accumulator (never a growing container), the dBTP meter off the shared
estimator, PLR, the GR-history SPSC ring (the first Audio→GUI ring — THREAD_MODEL's planned
edge), the two spectrum capture rings, loudness-compensated monitoring and delta monitoring
(KI-002 closes). Exit criterion: accuracy vs the EBU R128 vectors (≤ 0.1 LU) and the ISP vector
set (≤ 0.1 dB where grid-aligned).

**Risks.** OQ-013 still blocks the frozen-trim inject transport (P4 needs the ADR before Freeze
restore can be wired); OQ-014 stays an owner call; the min-phase impulse-peak ±1 convention is
documented but a host that measures latency by impulse peak on the min-phase path would see the
same ±1 (cosmetic, worth a KNOWN_ISSUES entry only if a real host complains); dynamic-tame and
model-weight constants are P6 listening material and may shift the sound pre-release
(compatibility cost: zero — nothing has shipped).

## P0 phase summary (`DEVELOPMENT_BRIEF.md` §13)

Required at the end of every phase: a summary of changes, the plan for the next phase, and the
current risks.

**Changes.** The repository went from empty to a governed P0 deliverable. Anamorph's governance
system, documentation library, CI/CD scaffolding and working conventions were migrated (never
modifying that repository, `CLAUDE.md` §3); the full Anamorph source was read across five domains
with `file:line` evidence (`worklogs/2026-07-30-p0-anamorph-research.md`); `docs/DESIGN.md` was
produced, survived four review rounds plus two adversarial verification passes, and was signed off
on 2026-07-31. Eleven ADRs are Accepted and registered. Four `DSP_POLICY.md` invariants were
amended by ADR (invariant 1 by ADR-0002; invariants 2 **and 8** by ADR-0004; invariants 2/5's open
point closed by ADR-0003) — two of those were **Hard-Stop** items ratified by the owner, not by a
green build. ADR-0005 and ADR-0011 amended `MODE_AND_ADAPTATION_POLICY.md` /
`PARAMETER_COMPATIBILITY_POLICY.md` and `THREADING_POLICY.md` respectively — `ADR_INDEX.md`
carries the five-ADR amendment table, which is the registry of record for "which rules were
rewritten, by what authority"; this row is a summary of it, not a second count to keep in step.
Five open questions closed: OQ-001, OQ-003, OQ-004, OQ-005, OQ-010.

**The four decisions that shape everything downstream.** (1) True-peak detection is a
**measurement tap**, so oversampling-off costs no detector latency. (2) Reported latency is a
**constant 10 ms lookahead allowance** plus oversampling — chosen so that browsing presets or
A/B-comparing during playback never moves host PDC, at the cost of ~8 ms nobody asked for.
(3) The **ceiling clamp is always last before dither**, downstream of the Post-position EQ, or a
post-limiter shelf would escape the product's core guarantee. (4) Simple is a **macro layer over
real Advanced parameters** written from the message thread, with per-parameter detach and
re-engage-on-touch.

**Plan for P1 (skeleton, exit criterion pluginval L5).** The eight-step order is in Pending Tasks
above. The shape of it: build system first (ADR-0008), then the parameter surface and its frozen
snapshot (ADR-0010) because IDs and ranges become permanent contract the moment a build leaves the
repository, then the POD boundary and threading shape (ADR-0001/0011), then a pass-through chain
with a basic limiter that honours the latency contract (ADR-0004), then the state harness
(ADR-0007). No DSP quality work at P1 — that is P2.

**Current risks.** RISK-008 (the measurement-tap latency contract rests on a detector group-delay
bound verified only by arithmetic — the first impulse test at P2 settles it; ADR-0004's constant
allowance makes the fallback cheap). RISK-002 (the parameter surface freezes at v0.1.0 before P2–P4
have taught us what it should be — mitigated by keeping pre-0.1.0 builds internal). RISK-003 (the
ceiling guarantee is asserted, not yet proven). RISK-009 (variable-font licence gates the P5
typography direction). RISK-006 / OQ-002 (JUCE licence tier blocks distribution, not development).

**C++23 canary status** (§2.1 requires this every phase): **not yet running** — there is no code to
compile at C++23. Scheduled for P2 per OQ-006's recommendation (DSP core + tests, weekly schedule
plus `workflow_dispatch`).

## Critical dependencies (with version-lock reasons)

| Dependency | Pin | Version-lock reason |
|---|---|---|
| **JUCE** | **9.0.0** — immutable commit `f8f8864…` (OQ-001, decided 2026-07-30; same pin as Anamorph) | Framework for all DSP, parameters/state, GUI and plugin wrappers. An unpinned bump can silently change DSP/latency/state-ABI. Sharing the pin with the sibling product makes a JUCE-attributable difference between them impossible and makes a bump a product-family decision. A bump is a Build System change (ADR + Review). `docs/policies/DEPENDENCY_POLICY.md`. |
| **C++ standard** | C++20 (+ a non-blocking C++23 canary job) | Project baseline per `DEVELOPMENT_BRIEF.md` §2.1; raising it is a build-contract change. |
| **pluginval** | latest release (downloaded) | The conformance gate. Not vendored; fetched by `scripts/run-pluginval.sh`. Pinning it is a tracked improvement. |
| Linux system libs | distro (`scripts/setup-linux.sh`) | ALSA/JACK/X11/FreeType/GTK/mesa/**EGL**/xvfb for headless build + validation. |

## Relationship to Anamorph

Anamorph is a **read-only reference**. This repository inherits its governance system
(`SOURCE_OF_TRUTH`, the policy set, ADR discipline, the documentation-lifecycle trigger map), its
CI/CD shape, its testing conventions and its brand system, and may copy and adapt first-party code
from it. Anabasis **never modifies the Anamorph repository**. What is shared and what deliberately
differs is tabulated in `docs/DEVELOPMENT_BRIEF.md` §23.

## Documentation ownership (proposed RACI — confirm with project owner)

No team structure exists in the repository, so the following is a **proposed** mapping (governance
guidance, not asserted fact):

| Documentation area | Proposed owner |
|---|---|
| `docs/architecture/` (incl. ADRs, DSP, signal flow) | DSP / Audio engineer |
| `docs/architecture/PARAMETER_*`, `SERIALIZATION_*`, `STATE_*` | Whoever owns the parameter/state surface (compatibility-critical) |
| `docs/procedures/` (BUILD, CI_CD, PACKAGING, RELEASE) | Build / Release engineer |
| `docs/policies/` | Tech lead / maintainer (these are binding) |
| `POSTMORTEMS`, `KNOWN_ISSUES`, `FUTURE_RISKS`, `HANDOVER`, `OPEN_QUESTIONS` | Maintainer |

## First steps for a new maintainer (or a resuming agent)

1. **Re-scan the workspace** (constraint C4) — the filesystem is the authoritative execution
   state, not any chat history.
2. Read `CLAUDE.md` → `docs/SOURCE_OF_TRUTH.md` → `docs/policies/AI_AGENT_POLICY.md` (Hard Stop
   conditions).
3. Read `docs/DEVELOPMENT_BRIEF.md` — Part I is the product spec, Part II is the inherited
   engineering standard.
4. Read `docs/OPEN_QUESTIONS.md` before making any decision that looks like one of them.
5. From P1 onward: build + test locally per `docs/procedures/BUILD.md` / `TESTING.md`.
