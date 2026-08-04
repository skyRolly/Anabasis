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
| **Test Status** | **537 checks green on Linux**: `AnabasisTests` (230 — ADR-0013 auto-release-follows-the-trim-scale, null-with-defaults bit-exact, impulse-at-allowance for four lookahead values, ceiling clamp, control/gain priming, limiter window coverage and alignment, smoothing of ceiling and lookahead, hostile-input finiteness, self-heal recovery, recovery from a stage that overflows on a FINITE input (EQ biquad in BOTH positions, RMS detector, colour c⁵, polyphase IIR up and down — `testExtremeLevelDoesNotSilencePermanently`) and from the stages that emit no audio to check (the BS.1770 meters and the §5.4 feature extractor — `testExtremeLevelDoesNotBreakTheMetersOrAdaptation`) and a Learn pass that measured through an overflow never becoming the saved reference (`testALearnPassThatOverflowedIsNotCommitted`), bypass null, EQ frequency response/smoothing/positions, the ADR-0002 post-shelf ceiling stimulus, compressor static curve/detectors/mix/two-stage auto release/sidechain HPF, clipper curve/compensation/ADAA aliasing/colour models/dynamic tame, true-peak accuracy, limiter link/styles/preserve/two-stage auto/detector HPF/dBTP mode, the full OS latency matrix, OS aliasing/transparency/bypass/ceiling, dither modes, the §2.8 duck on rewires/latches/requests, LUFS calibration/gating/windows, inv-10 monitoring honesty incl. the mid-stream offline flip snap, delta, the duck-bottom hold, the post-latch refill hold, a request held through the out-leg, delta covered by the duck, the last-staged-restore rule, stale detector state, limiter control smoothing incl. link/preserve/HPF glides) plus meter publication and the GR ring in the state suite and `AnabasisStateTests` (307 — the ADR-0014 frozen-trim restore (`testFrozenTrimRestore`: both landing sites, both staging sites, the capture, the no-audio mirror, the undo case, the consume-to-bottom save window, freeze-off inert — each killed by its own mutant), the undo duck (`testUndoRequestsDuck`), gesture begin/end symmetry, the 12-preset factory bank, registry snapshot vs the frozen fixture, 49/9 counts, raw-exact byte-identical round-trip and its fixed-point precondition, structural-tolerance read rules, batched latency notification, corrupt/foreign no-op, macro fixed point, restore-vs-macro-drain, A/B tier behaviour, preset contract, cache mapping, the ADAPTIVE missing-field defaults, meters reading the render not the monitor path, load-then-save with no audio between, the zero-length-block publish guard, and — first in the tree to construct the EDITOR — the Settings panel following a project load in both directions, the meter tooltip quoting the OQ-008 table rather than a copy of it, and every knob's animated position starting where its value already is), plus `testTeardownAndReengageInvariants` (no trigger drains after `stopDraining`; a macro gesture that moves nothing re-lands the curve; a copied-into slot starts a fresh undo history). `AnabasisTests` also pins the limiter push's chain position (`testLimiterPushDoesNotDriveTheClipper`) and that a realtime→offline flip does not duck the render (`testOfflineFlipDoesNotDuckTheRender`) while the return edge stays ducked (`testReturnFromOfflineIsDucked`), and that an EQ-position change on the offline-entry edge starts from cleared filter state (`testOfflineEntryClearsEqStateOnAPositionChange`). **pluginval L10 green ×3 in both modes on Linux (editor under xvfb)** — the P6 bar; CI now gates at 10 on all three platforms. Re-count from the suites' own output when editing this row; it has gone stale once already. |
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
  `testFrozenTrimRestore` (every element killed by its own mutant — ADR-0014 enumerates them).
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

**Review round 24 (2026-08-03) — the first review of the v0.1.0 tree, three live defects.**
(1) `undo`/`redo` never requested the §2.8 duck, so an undo stepped rather than dipping (DSP
invariant 8 has named the undo route since ADR-0004) AND the ADR-0014 frozen-trim vector it stages
could only land at some later, unrelated duck — the wrong slot's sound. (2) The capture's
"restore still pending" test read the ADR-0012 record flag, which the block top clears ~34 ms
before the vector is actually published, so a save in that window — reachable by the editor's own
~3 Hz dirty poll — serialised the pre-restore trims and destroyed the loaded vector; now a
stage/applied generation pair advanced only by `injectTrims`, and the capture writes a local
rather than the mirror. (3) The meter-reset watermark admitted the sub-block that was partially
filled at the reset, so up to 100 ms of the old programme could still pin the fresh integrated
reading through the relative gate. Each fix is mutation-verified; smaller items in the same round:
gesture begin/end symmetry across threads, the preset gate's foreign-root case, `prepare()`
resetting the ADR-0013 auto-release scale, `SafePointer` in the editor's async callbacks, a
`static_assert` on the spectrum taps' L/R assumption.

**Review round 25 (2026-08-03), two user-visible defects.** (1) The three Settings drop-downs bound
the ComboBox's 1-based item ID onto 0-based InternalState fields, so oversampling, phase and
offline quality each selected one step high (picking "Off" turned oversampling ON) and opened
blank; now mapped index ↔ value explicitly. Level-5: no headless test constructs an editor, so the
fix carries its reasoning in a comment rather than a guard. (2) **Factory presets were inaudible**
— a factory table expresses itself through the macros, and the `ScopedRestore` around the apply
aborted the mapping those macro writes armed, leaving the nine managed parameters at M(0,0,0)
while `loudness` read 80. The guard is now scoped to the value-landing with `refreshMapping()`
after it and before the dirty baseline; both directions are pinned (a FILE preset's managed values
must still survive, since a file carries them itself). Also: the wrapper no longer posts to the
message queue from a listener callback (`triggerAsyncUpdate()` on a host thread that may be the
audio one is the red line `MacroEngine` already refused) — the detach bits ride that class's
existing 30 ms tick and the `AsyncUpdater` base is gone; the frozen record's duck request is
derived at the consume instead of stored beside the flag; `GrHistoryBuffer::reset`'s opening
seqlock increment gained its release fence. **KI-007** opens with three bookkeeping edges the round
deliberately left alone (the frozen vector surviving a factory apply, preset-ring navigation by
name, undo not restoring the dirty baseline) — semantics calls for the fine review, two of them
KI-006's question. Suites: 230 + 179.

**Review round 26 (2026-08-03).** Two regressions from the rounds before it and one ADR drift.
(1) Round 25's combo fix restored the encoding but dropped the state→widget direction, so a
project loaded with the Settings panel open showed the previous project's oversampling, phase and
offline quality; the boxes (and `uiScaleBox`) are now re-seeded from the tree on the 24 Hz tick.
(2) The EDITOR still called `triggerAsyncUpdate()` from a parameter callback while listening to
two automatable ids — the same red line round 25 removed from the wrapper, one class over; it now
posts only when already on the message thread and otherwise waits for the tick. (3) **ADR-0014
described a duck request the code deliberately does not make** — the round-24 store was replaced
by a derivation at the consume in round 25 and the ADR was not updated with it; corrected, and the
same ADR gained two known-limits entries (the generation pair is engine-wide while the mirror is
per-slot; the wrong-slot injection window is any live-slot change inside the duck, not only a
Freeze toggle). Also: `MacroEngine::startDraining()` moves the 30 ms timer out of the constructor
so it cannot read the callbacks mid-assignment; `TESTING_POLICY.md` no longer restates the
pluginval strictness in prose (`build.yml` is the single place, and the copy had gone stale).

**Review round 27 (2026-08-03).** (1) The preset dirty datum was engine-wide while the name it
describes is per-slot, so it survived a session load (which cannot restore it) and did not travel
across an A/B switch — after applying a preset in B, switching back marked A against B's baseline.
Now one per slot, swapped by `switchToSlot` and dropped with the other slot fields on a load.
(2) Round 26 made CONSTRUCTION safe against the message-thread premise and left destruction: the
30 ms tick calls into the wrapper, whose members are destroyed before `~MacroEngine` stops the
timer. `stopDraining()` + an explicit destructor closes it. (3) The drain applied detach bits
after the re-engage clear, so a detach racing a macro gesture won — the opposite of §5.3 and of
the comment above it; reachable only when both land in one off-thread tick, which the new test
builds. Also: the double-click knob reset is gesture-bracketed like alt-click (it produced neither
an undo step nor a detach); `CurveView`'s repaint fingerprint includes the sample rate. KI-006
gains the save half of its gap, KI-007 the menu's raw LookAndFeel pointer. Suites: 230 + 189.

**Review round 28 (2026-08-03).** The dBTP readout warned against a hard-coded −1 — the ceiling's
DEFAULT — so at any other ceiling (a factory preset already ships −0.5) it fired at the wrong
level; it now reads the live ceiling, which is also in the view's snapshot so a ceiling move
repaints the colour. The `testFrozenTrimRestore` mutant count lived in six places and two
disagreed; it is now in ONE (ADR-0014's evidence line, which enumerates them) with the others
carrying no number, the same treatment the pluginval strictness got. The bench target is compiled
by the Linux CI job (never run there) so `tests/bench.cpp` cannot rot unnoticed. `allCombos` stopped
being dead state — the 24 Hz tick publishes the `hov` flag `drawComboBox` prefers. A tooltip
comment citing "KI-006" now says whose KI-006 it means (Anamorph's; ours is unrelated). KI-007's
heading no longer claims a count, and gains a fifth item (the dirty marker keys on the whole slot
tree, so preset-EXCLUDED parameters mark a preset as edited); KI-003 gains the §7 undo stacks as a
third family member. Suites: 230 + 189.

**Review round 29 (2026-08-03) — loose ends, not defects.** Most findings were already recorded in
KI-003/006/007; the rest were places a rule this PR wrote had not been applied to itself. Fixed: a
restore replaced the detach mask but not the STAGED bits, so an off-thread edit racing a slot
switch could stamp a detach onto the slot that had just been restored (mutation-verified); the
host-hidden Settings controls had no accessibility titles and `uiScaleBox` also missed
`registerAnimated`, so "accessibility names on every control" was false for exactly that panel;
`ANABASIS_BUILD_BENCH` was nested inside `ANABASIS_BUILD_TESTS`, so bench-ON/tests-OFF built
nothing (verified with tests OFF now); `build.yml`'s header carried two disagreeing P6 strictness
rows in the block cited as that number's single authority; `TESTING_POLICY` stated the rule
against restating the strictness and restated it in the same sentence; OQ-013/014/016 were marked
Resolved but left among the open entries. Recorded: `startDraining`/`stopDraining` are not equally
strong (construction closes structurally, teardown cannot join a tick already executing — KI-003),
and the spectrum view freezes rather than decaying when audio stops — a listening-pass call
(KI-007 item 6). Suites: 230 + 191.

**Review round 30 (2026-08-03).** Both findings were the same failure the recent rounds keep
producing — a rule applied at one of its sites. (1) Round 29's staged-bit drop covered
`applySlotToLive` and missed `applyFactoryPreset`, `applyPresetFile`, `setStateInformation` (which
rebuilds the mask inline) and `resetToMacro`, so an off-thread edit racing a preset apply or a
project load could stamp a detach onto the freshly loaded state — and be saved with it. Fixed
structurally: `replaceDetachMask()` is the mask's only writer, so the drop cannot be forgotten by
a sixth site; all four paths are asserted. (2) The 30 ms tick ran the MAPPING before the wrapper's
bits, undoing round 27's precedence one level up: a macro gesture arriving off-thread mapped while
its parameter was still masked, then cleared the mask — the parameter read as re-engaged while
holding the user's value. Order swapped and `drainTick()` extracted so the order is testable.
Comments added at three places a future change would break silently (the preset-exclusion
predicate, the spectrum publication's full-chunk assumption, the bench's Linux-only compile).
KI-007 gains a seventh item (Copy A→B and the destination's undo history). Suites: 230 + 199.

**Review round 31 (2026-08-03).** (1) The previous round's fix was counted in prose, not enforced:
`MacroEngine::handleAsyncUpdate` — the path a message-thread macro write POSTS to — still ran
`drainPendingMapping()` alone while the comment beside it claimed "all three paths now agree". Host
automation of a macro (message thread, no gesture, so nothing re-engages) racing a gestured managed
edit delivered off-thread mapped over the user's value, and the next tick then marked that
parameter detached at the macro's value. All three triggers now call `drainTick()`; the handler is
public so the headless tests can run the posted path at all, and both it and `flushPendingMapping`
are mutation-verified. (2) The GR history frame asked for the ring's full `kSize`, whose oldest
entry aliases the slot the audio thread is filling — clamp corrected to `kSize - 1`, the reason
mirrored into `peek`'s contract, and the bound extracted to `GrHistoryView::windowEntries` so it is
testable without a graphics context. (3) The processor destructor now deregisters its listeners
instead of relying on declaration order. Comments: the double-click gesture bracket records the
settled JUCE dispatch order with its source citation; `applyFactoryPreset` states why iterating the
APVTS tree while writing it is safe. KI-007 gains an eighth item (a macro gesture that moves
nothing re-engages without re-landing the curve). Suites: 230 + 211.

**Review round 32 (2026-08-03).** A direction and a guard, each applied to some of what it names.
(1) The Settings panel's three §6.4 target checkboxes never followed a project load: hand-built
from three bits of one int (so `referTo` cannot express them) and seeded once at construction,
while `LoudnessMeterView` reads the tree every frame — the panel showed the previous project's
targets against the new project's meter. The same one-way shape round 26 removed from the combos.
Re-seeded on the same tick, and `refreshInternalSettingsBoxes()` is public because the headless
suite has no message loop to fire that tick; `testTheSettingsPanelFollowsAProjectLoad` is the first
test that CONSTRUCTS the editor and covers the round-26 combos too. (2) The macro tick's restore
guard covered only the mapping half, so a tick inside a `ScopedRestore` still wrote
`liveDetachMask` — a second concurrent writer on the off-message-thread load path, widening KI-003
rather than leaving it as found; guard moved up to `drainTick`, KI-003 updated to record the
narrowing. (3) `MacroEngine::applying` was the non-atomic half of a pair whose other half
(`restoreDepth`) is atomic for exactly the reason this one needed to be. (4) Three wrapper
parameter-listener registrations for the macro ids that `parameterChanged` discarded on its first
line: dropped. (5) `juce::TooltipWindow` was constructed parentless, so the family's `drawTooltip`
/`getTooltipBounds` never ran and tooltips rendered in the JUCE default style — `setLookAndFeel`
wired in the constructor, cleared in the destructor, and the macOS `setOpaque (false)` half that
`drawTooltip`'s own comment already described (but that had not come across with the adapted file)
carried over from Anamorph under ADR-0009. Comments: `ScopeBuffer::readLatest`'s headroom
argument (why it needs no `capacity - 1` clamp where the GR ring does), and `Knob`'s two accepted
properties (alt-press-and-drag inert; a triple-click's third click resets nothing). Reviewed and
unchanged: `AnabasisBench` already links exactly as both test apps do. Suites: 230 + 222.

**Review round 33 (2026-08-03).** Second copies, and work deferred to a block that a stopped
transport never runs. (1) A project loaded with the transport stopped left the meters showing the
previous session: the meter-hold clear was staged through the momentary-request row and consumed at
a BLOCK TOP, and opening a project with the transport stopped — the ordinary case — runs no block
at all. The engine-side clear still waits; the DISPLAY is published immediately, through the new
`publishSilentMeters()`, which is now the ONE list for all three sites (prepare, the block-top
consume, the load) that had grown their own copies. `dbTpMaxHold` deliberately stays out of it —
plain audio-thread state. (2) The editor's `Timer`/`AsyncUpdater` bases stopped themselves only in
their own destructors, i.e. after every member was gone: `stopTimer(); cancelPendingUpdate();` now
run first, matching the reasoning already applied to `MacroEngine` and the processor. (3) The
loudness meter's target ticks re-derived the row origin from `getLocalBounds()`; the arithmetic
agreed (the 2 px is a deliberate symmetric overhang on an 8 px bar, so nothing looked wrong), but
the duplicate would have drifted on any layout change — the ticks now derive from the bar
rectangles. (4) A comment reading "One factor, computed once" called `std::pow` twice; computed
once, bit-identical, with the reason the call stays unconditional written down. Comments: why the
four `referTo`-bound Settings toggles are NOT in the re-seed (and that the trade is testability),
and that `testAMacroGestureWinsADetachRacingItInOneDrain` asserts the mask only on purpose. KI-007
gains a ninth item (reset-to-macro has no undo step) and item 5 gains the freeze-OFF slot that
still serialises a stale `FROZEN_TRIMS`. Unchanged with its reason restated: the async preset
menu's raw `LookAndFeel` pointer stays KI-007 item 4. Suites: 230 + 224.

**Review round 34 (2026-08-03).** (1) The OQ-008 target values had THREE copies: the compiled
`kTargets` table, the meter tooltip's free text (names, numbers and the "as of" date) and the three
§6.4 checkbox labels — and OQ-008's per-release refresh touches only the table, so the copies are
what would have gone stale. `Target` gains a `fullName`, `kTargetsAsOf` is a constant,
`tooltipText()` builds the string and the labels come from the table; the test rebuilds its
expectation from `kTargets` and dies against a refreshed table plus a hard-coded string. (2) Round
33's editor-destructor guarantee was two thirds applied — `animVBlank` was left armed while its
callback's state was destroyed around it; `animVBlank = {}` closes it. (3) The preset dirty mark
lagged up to ~333 ms after undo/redo, an A/B toggle, a preset apply, ‹/› or a save: those callers
only advanced the ~3 Hz throttle they should have skipped, and now pass `recomputeNow`. (4) The
Learn-command paragraph, orphaned by the ADR-0014 block spliced above it, is back on
`learnCmd.exchange`. (5) `publishSilentMeters()` outran its documented row: `THREAD_MODEL` and
`THREADING_POLICY` now name the non-audio clear writers and why six independent relaxed scalars make
that concurrency benign. (6) "Transparent Master" and "Classical Dynamics" inherited `colourModel`'s
Tape default while their whole intent is "untouched"; both name Clean explicitly (⊕). Comments: the
re-entrancy that routing every trigger through `drainTick` created (and the two reasons it is safe),
the extra ~11 ms bottom hold when a frozen-trim record arrives at a bottom already reached, and the
bench's second CI residual — compiling is not running. Suites: 230 + 228.

**Review round 35 (2026-08-03).** Two documentation drifts this PR itself caused, plus three more
copies. (1) The pluginval strictness had a SECOND copy: round 29 collapsed the duplicate rows
inside `build.yml` and declared that block "the single authority", while `CI_CD.md` kept its own
`env:` snippet saying 5 under a heading reading "in one place" — the duplicate had moved, not gone,
and the lifecycle policy makes that sync mandatory. `CI_CD.md` now names where the number lives and
quotes nothing, and its pipeline section finally records the Linux job's `-DANABASIS_BUILD_BENCH=ON`.
(2) The README still described the pre-P5 project — "P5 (GUI) is next", "the eleven ADRs",
"pluginval L5", OQ-013's Hard Stop "stands" — in the first document a reader opens, while
`CLAUDE.md` and this file were updated in the same commits. (3) The target-line CARDINALITY was
still duplicated three ways after round 34 removed the names and numbers (`kNumTargets = 3`, two
fixed toggle arrays, three hand-placed rows); OQ-008 leaves a fourth line open, so `kNumTargets` is
now `std::size (kTargets)`, the toggles are one `std::array` sized from it, and the row divides
itself. (4) The editor's badge table wrote the nine managed ids out again instead of indexing
`managed_params::ids`; it now indexes that list against a parallel `Knob*` array whose fixed size
makes a mismatch a compile error. (5) The ADAPTIVE-mirror comment had been orphaned from its members
by the §5.3 block spliced between them — round 34's Learn-paragraph defect, one file over. (6)
`registerAnimated` seeded every animated property except `vpos`, so every knob swept up from its
minimum over the first frames after the editor opened. Enforcement rather than a comment: both
`refreshMapping()` callers now go through `relandMacroCurve()`, which asserts the staged detach bits
are clear — the invariant round 34 stated and nothing checked. Suites: 230 + 228.

**Review round 36 (2026-08-03).** The first finding is round 35's fix landing in the wrong place.
(1) The `vpos` seed ran BEFORE the APVTS attachment — `setupRotary` registers the widget and the
per-control helper attaches second, so at registration a slider still carries JUCE's default 0..10
range and value 0, and the seed stored exactly the minimum it was meant to stop showing. Moved into
`seedAnimatedFromValues()`, one pass over the registry at the end of the constructor; the test
asserts it over every registered slider and dies against both the unseeded state and the round-35
placement. (2) `ValueBox::mouseDrag` wrote the parameter with no gesture brackets, so dragging a
managed parameter's numeric readout neither detached it nor produced an undo step — bracketed with
`juce::Slider::ScopedDragNotification`, opened on mouse-down and closed unconditionally on mouse-up.
(3) `stopDraining()` nulled two `std::function`s, which made its own residual WORSE: a tick already
inside `drainTick` is about to invoke `onDrainTick`, so assigning it is a data race where leaving it
alone was a call into an owner still alive. Both assignments dropped. (4) Round 35's
`relandMacroCurve()` assert was debug-only; it now clears the staged bits as well, so a future third
caller cannot ship a release binary applying them mid-apply. (5) `resized()` dereferenced five view
`unique_ptr`s, safe only by construction order — one guard states the requirement. (6) The redo
stack was capped only transitively; one `pushCapped` helper is now the single place a StateSet joins
either stack. Documentation: `THREADING_POLICY`'s SPSC row said the index is published "once per
block" while the spectrum taps publish per CHUNK — re-worded around the committed unit, since the
guarantee holds identically and only the reader's cadence differs. Suites: 230 + 230.

**Review round 37 (2026-08-03) — triaged.** Only findings that violate an invariant this build has
already established; everything else stays recorded. (1) Round 36 removed the `std::function`
nulling from `stopDraining()` for a sound reason and dropped what it also bought: `drainTick`,
`flushPendingMapping` and `refreshMapping` are public, so "nothing drains after stopDraining"
became a rule rather than a structure — and after the processor destructor has called it, the
members `onDrainTick` reaches are gone. A one-way `drainStopped` latch read at the top of
`drainTick` restores it for every trigger at once, without the race the nulling had. (2) The preset
menu's raw `LookAndFeel` pointer (KI-007 item 4) is closed by construction: `withParentComponent`
makes the MenuWindow a child of the editor, which cannot be outlived and supplies `lnf` up the
parent chain, so the explicit `setLookAndFeel` is gone. (3) §5.3's re-engage was half applied on the
gesture path — `MODE_AND_ADAPTATION_POLICY` invariant 3 already says the re-engage happens "through
the normal rate-limited glide", so a macro gesture that moved nothing clearing the mask without
arming a mapping was code drifting from a written invariant; the begin now calls
`MacroEngine::armMapping()` beside the re-engage (KI-007 item 8 → Resolved). (4) Copy A→B left the
destination's undo history describing the state it overwrote; `setStateInformation` already clears
both stacks because "a load starts a fresh history", and a Copy is that event for one slot (KI-007
item 7 → Resolved). Plus a pairing hardening on round 36's own code: `ValueBox::mouseDown` resets
its bracket before opening a new one. `testTeardownAndReengageInvariants` covers the three
mechanical invariants, one mutant each. KI-003's stopDraining paragraph corrected — it still said
"clears the callbacks". Suites: 230 + 239.

**Review round 38 (2026-08-03) — triaged, five state-consistency items.** (1) `relandMacroCurve()`
asserted no detach bit was staged, in the one function written to cope with one being staged — a
debug build could abort while browsing presets, because `applyFactoryPreset` reaches it after its
`ScopedRestore` drops and `parameterChanged` stages bits from whichever thread the host chooses.
The two stores were always the mechanism; the assert is gone. (2) A load reset `openGestureBits`
and `gesturePreState` but not `managedGestureBits`, so a BEGIN delivered off-thread across a
session replacement let the next ungestured write satisfy §5.3's gesture-bracketed condition. (3)
Frozen-trim persistence: a freeze-OFF slot no longer serialises a `FROZEN_TRIMS` child at all, and
the capture now requires `hasPublishedTrims()` so an instance that never processed a block cannot
overwrite a held vector with zeros — the save half of KI-006, closed without needing the audio
half's owner call. (4) Undo/redo now restore `presetBaseline` beside the slot: a history entry is
the pair, which needed no schema change since the stacks are session-local. (5) `resetToMacro()`
pushes its pre-state, so the one Simple-view affordance that changes nine parameters at once is
undoable like every other. `testStateReplacementAndHistoryConsistency` plus the round-trip's new
freeze-OFF check; four mutants. KI-006 save half CLOSED; KI-007 items 3, 5 (partly) and 9 settled.
Suites: 230 + 249.

**Review round 39 (2026-08-03) — correctness only.** (1) Round 38's `hasPublishedTrims()` marker
was set inside `publishTrims()`, which `reset()` also calls — so it read true for every prepared
instance and the save guard was inert. It now tracks the CURRENT contents of the four atomics:
`publishTrims (bool meaningful)`, false from `reset()`'s initialisation zeros, true from an audible
`finishBlock` and from an ADR-0014 `injectTrims`. The reachable case is a host sample-rate change
on a frozen slot, where the next save wrote zeros over the slot's latch. (2) KI-006 asserted the
published atomics are never zeroed; `reset()` republishes them, so the entry was corrected against
the code, including which of its two resolutions that changes. (3) The GR ring's seqlock comment
argued from a release STORE while the code uses a release FENCE — code unchanged (it is the
canonical write-begin), justification restated as what the barrier gives, plus what it does not
(the reader's plain entry reads are discarded by the epoch re-check, not prevented). (4) The
spectrum rings survived a re-prepare while the GR ring has been cleared since P3;
`ScopeBuffer::reset()` rewinds the published index. (5) `switchToSlot` left `gesturePreState` and
the open-drag bits, so a drag open across an A/B switch pushed a step onto the new slot describing
a state it never held; both are dropped at the switch, `managedGestureBits` deliberately not.
`testPreparedStateAndSlotOwnership`, three mutants. Suites: 230 + 254.

**Review round 40 (2026-08-03) — a targeted hardening pass over ownership, four items.**
(1) `requestMeterReset()` set the flag and published nothing; the DISPLAY clear lived at the
`setStateInformation` call site, so the meter panel's click — the case where the transport is
ordinarily stopped — set a flag no block ever consumed and the panel kept the previous take's
holds. The pairing moved INTO the request, so both callers get it and a third inherits it.
(2) Frozen-trim OWNERSHIP, stated: the wrapper's `liveFrozenTrims` mirror is the durable owner and
the engine's published atomics are a copy that does not survive `AdaptiveEngine::reset()`. A latch
established LIVE existed only in that copy, so a host rate change took it — writing zeros before
round 39, nothing at all after it. `prepareToPlay` now captures the latch into the mirror before
`engine.prepare` destroys it, and the two-clause "is the copy the truth?" test lives in
`engineFrozenTrimsIfLive()`, read by the save and the capture alike. KI-006's save half is closed;
the audio half is untouched and still the owner's Freeze-semantics call. (3) Documentation
authority: `CI_CD.md` pointed at `TESTING_POLICY.md` for the strictness value, which refuses to
state it and points at `build.yml` — while itself carrying a phase table and three literal `10`s.
`build.yml` is the single source; both documents now say what they own and quote no number (the
local-repro block reads the value out of the workflow instead of pasting a stale `5`).
(4) `refreshMapping()` promised an unconditional re-land while `drainTick` silently suppressed the
whole tick inside a `ScopedRestore` and the scope's exit dropped the arm. The deferral is correct;
the promise was not. `drainTick`/`flushPendingMapping`/`refreshMapping` now report whether the tick
ran. `testPreparedStateAndSlotOwnership` case 4, `testTeardownAndReengageInvariants` case 4,
`testMeterResetClearsSessionHolds` extended; four mutants. Suites: 230 + 270.

**Review round 41 (2026-08-03) — threading correctness, and the first item is round 40's fix being
wrong.** (1) Round 40 made the wrapper's `liveFrozenTrims` `juce::ValueTree` the durable owner of a
frozen latch and had `prepareToPlay` assign it — a host callback JUCE does not deliver on the
message thread, opposite the editor's continuous `presetDirty()` read of that member, both sides
gated on Freeze being ON. ThreadSanitizer reports it as a data race on the tree's refcounted
pointer. The durable copy now lives in the lock-free layer instead: `AdaptiveEngine` keeps a
RETAINED trim set that `reset()` does not clear, joining `learned`/`refOnsetRate`/`refTiltDb` in
that function's survivors list, while the PUBLISHED set keeps its former meaning (what the DSP is
applying) so KI-006's readout half stays honest. `prepareToPlay` touches no wrapper `ValueTree`.
(2) `pubTrimEver` gated a read of four scalars while being relaxed — the shape `frozenAppliedSeq`
was corrected to at round 39. It and `retTrimEver` are release/acquire, and `THREADING_POLICY`
gained a **publication flags** row with the test and the four instances, so the distinction is a
rule instead of four separate judgements. (3) Freeze/preset questions reviewed and left as spec
questions; KI-007 item 5 gained the missing input (the frozen vector's CONTENT depends on when
Freeze was engaged). (4) Undo/redo buttons seeded in the editor constructor via a shared
`refreshUndoRedoEnablement()`. **KI-008 opened** — a pre-existing ABBA lock-order inversion between
JUCE's parameter-listener lock and the APVTS tree lock, found because this round added the suite's
first two-threaded stimulus; the §7 snapshot point is an Architecture Review Gate item, so it is
recorded, not fixed. `testTheFrozenLatchNeedsNoThreadCrossing`; verification instrument is a
`-fsanitize=thread` build (round-40 code: 2 data races; current: none). Suites: 230 + 276.

**Review round 42 (2026-08-03) — ownership and synchronisation, six items.** (1) The retained
frozen-trim set is engine-wide while `FROZEN_TRIMS` is per-slot: after an A/B switch into a
freeze-ON slot holding no vector of its own, the incoming slot serialised the outgoing slot's latch.
The retained set is a runtime cache and may only answer for the slot it was filled under, so the
wrapper re-bases a generation comparand whenever frozen ownership changes — in `adoptFrozenMirror()`,
now the single writer of the mirror. The retained flag became a counter to carry both questions.
(2) That counter keeps the release/acquire pairing; `slotFrozenBase` is relaxed because it is only a
comparand. (3) KI-003's third member CLOSED: `setStateInformation` no longer clears the undo stacks
from a possibly-off-message-thread callback — it bumps `historyEpoch` and the message thread
reconciles at `syncHistory()`, the single point every history read and write passes through. No lock.
(4) Persisted `uiScale` clamps to the nearest legal step through one `nearestScaleIndex()`, so the
rendered transform and the displayed selection cannot diverge on a load. (5) Initial UI state was
already seeded at round 41. (6) `CurveView` caches its built path behind the fingerprint that
already gates the repaint, so an expose or a window move no longer re-prepares five filters and
evaluates one magnitude per pixel column. `testAFrozenLatchDoesNotFollowTheSlotSwitch`,
`testHistoryOwnershipAcrossAStateLoad`, `testAnOutOfListUiScaleClampsConsistently`,
`testTheCurveWellCachesWithoutChangingWhatItDraws`; four mutants; TSAN re-run clean. Suites: 230 + 295.

**Review round 43 (2026-08-03) — two maintenance items.** (1) `CI_CD.md`'s local-validation block
read the pluginval strictness with `grep -oP`, a GNU-only form that BSD grep rejects — so on macOS,
one of the three platforms the gate is required on, `STRICTNESS` came out empty and the validator
ran with no strictness argument at all. Replaced with POSIX `sed` anchored to the `env:` assignment,
plus a `${VAR:?}` guard so an unreadable workflow fails loudly; ownership unchanged, and the block
now marks its one Linux-only line. (2) `resetSweep` was read by both slider draw paths and set
nowhere; removed rather than wired, because `stepMicroAnims` already snaps `vpos` while the button
is down, so the flag could not have delivered the sweep-while-held its comment described. Doing that
properly needs three sites to agree and is a listening-pass call. Behaviour-identical by
construction. Suites: 230 + 295 (unchanged — neither item is testable without a check that cannot
fail).

**Review round 44 (2026-08-03) — UB, platform guarantees, consistency.** (1) Two stored Settings
closures captured a reference variable / reference parameter by reference, which is UB by
[expr.prim.lambda.capture] however reliably it works; normalised on `[this]`+re-fetch and a
by-value pointer. (2) `AnabasisBench`'s machine line was Linux-only while the bench option is not,
so a local re-measure elsewhere silently violated PERFORMANCE_BUDGET C2; one lookup per platform
plus a loud refusal (and an `ANABASIS_BENCH_CPU` override) where none answers. (3) `CI_CD.md`'s
repro block claimed three platforms while being POSIX-only — split into POSIX and PowerShell
blocks, both reading the strictness from `build.yml`. (4) One `sliderParent()` predicate for the
ValueBox drag bracket (it was a style test in two handlers and a wider cast in the third, and left
linear sliders unbracketed); `stepPreset` remembers the applied source instead of re-deriving it
from a non-unique display name; `resized()`'s guard asserts and the constructor lays out
unconditionally, since `setSize` no-ops on an unchanged size.
`testTheSettingsCallbacksReachTheLiveTree`, two mutants. Suites: 230 + 300.

**Review round 45 (2026-08-03) — four hardening items, no behaviour change.** (1) The bench emitted
its C2 refusal before accepting `ANABASIS_BENCH_CPU`, so the documented override read as a failure;
the two identity sources are now tried in order and the refusal fires only when neither answers
(verified: override path writes nothing to stderr, no-override path still exits 2). (2) The
documented Windows repro block indexed `Select-String`'s result before its guard, so an unreadable
workflow threw a PowerShell error instead of the intended message; tested before indexed, same
single source. (3) `~MacroEngine` calls `stopDraining()` so the one-way latch is set by the object
rather than by the owner remembering — the owner still calls it first and that ordering is
unchanged; the already-executing-tick residual in KI-003 is untouched and restated at the site.
(4) `retTrimSeq`'s comment now states that it counts meaningful PUBLICATIONS (~90/s while adapting),
not Freeze latches, and that only inequality against a recorded value is supported. Also cleared two
`getScaleFactor()` deprecation warnings from round 44's test. Suites: 230 + 300 (unchanged).

**Review round 46 (2026-08-03) — a measurement defect and a gesture-model defect.** (1) The
per-stage bench timed its own stimulus generator (an LCG step and a `std::sin` per sample inside the
stamps), inflating every row of a table used to argue the §9 allocations are inside budget. Stimulus
pre-generated outside the stamps; re-measured on the same machine — EQ 0.16 → 0.10 %, Compressor
0.15 → 0.10 %, Metering 0.18 → 0.10 %, Limiter 0.44 → 0.42 %, Clipper unchanged; no verdict changed,
and the whole-engine matrix was already correct. (2) "Worst block" stays the conservative maximum
across all five runs; both the bench method line and PERFORMANCE_BUDGET now say so explicitly.
(3) `ValueBox` opened its host gesture on mouse-DOWN, so clicking the numeric readout under a §5.5
macro cleared the entire §5.3 detach mask and re-landed the curve — a destructive action from a
control whose affordance is "edit this number". The bracket now opens on the first movement; a real
drag is unchanged, the knob is unchanged, and the write is still bracketed.
`testAValueBoxClickIsNotAMacroGesture`, two mutants. (4) Preset application through
`setValueNotifyingHost` investigated and left unchanged — it is required for host/APVTS/attachment
agreement; the ~46-notification burst per preset step is now a DAW-matrix checklist line.
Suites: 230 + 307.

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
passed locally ×3 in BOTH modes** — the P6 bar holds on Linux; CI stayed at 8 at the time of this
note and was raised to 10 later the same day (see the v0.1.0 completion summary above).

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
