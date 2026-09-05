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
| **Current Version** | **0.2.12** (`project(Anabasis VERSION 0.2.12)` in `CMakeLists.txt`), with a dated `## [0.2.12] — 2026-09-05` CHANGELOG entry — **the owner's third GR-history report: 0.2.11 had not removed the visible instability at the right edge.** The owner saw a 1–2 px horizontal stub at the right edge of the trace and of the grey level history, jumping as the content behind it was still being generated. Reproduced on the same scratch harness (real processor, real `GrHistoryView::tick`/`paintHistory`, simulated host and frame clock) and the reading confirmed before any change: every drawn vertex was frozen and rigid, as 0.2.11 claimed, but the lead-out — the flat strip from the newest complete vertex to the plot edge, the placeholder for the bucket still collecting — was on screen, 1.00–2.42 px long at 48 kHz / 512 (2.90 at 1024), and once per bucket its height jumped and the segment left of it (up to a pitch wide) snapped from flat to sloped, the one part of the trace that changes other than by scrolling, shared by the stroke and the fill because both run the same path. Fixed by a clip and nothing else: `GrHistoryView::visibleRight` = `floor (right − min (pitch, span / 2)) − 1`, the first column `paintHistory` no longer shows — left of everything the lead-out can touch (the newest drawn vertex satisfies `right − pitch ≤ x ≤ right` on every frame), with one further column of measured margin for the stroke join at the vertex before it, which re-shapes in the frame the newer one appears. The anchor, `bucketX`, the values, the timing, the smoothed head, the host-delivery behaviour and the left edge are untouched; every column still shown is pixel-identical to 0.2.11 in 480 of 480 frames on every configuration run; the plot gives up `ceil (pitch) + 2` columns on the right (four for every block up to 1024 samples at every rate from 44.1 kHz and 2048 from 48 kHz up, on either well). Validated old against new on the same frames: in the rightmost 24 visible columns the translation-compensated movement of the fill's top edge fell from a mean of 0.52 px (max 25) to 0.07 px (max 0.44), the stroke's from 0.24 px (max 18) to 0.04 px (max 0.16) — the interior's own floor — with the last visible column lit on every frame and every column beyond it background on every frame; no stroke pixel appears or vanishes frame to frame in the three columns inside the boundary; the identity holds through a 26 s settled-window run; at 75–200 % UI scale the strip stays ≥ 0.89 px clear of the visible range. **The PR review's second finding is answered by a bucket-aligned read window**: the 20 s window is a LENGTH, so `head − window` fell mid-bucket and the paint clamped the OLDEST drawn bucket's span to it — that bucket lost its earliest entries one at a time and its value changed while it was still drawn, re-shaping the segment that crosses the left edge (340 changes in 1800 frames at 48 kHz / 512, up to 1.53 dB — 5.9 px Simple, 15.5 px Advanced — and never on any other drawn bucket, in 3.9 M readings). `Buckets::first` is now `kFirst · stride`, the oldest drawn bucket's own first entry, so every drawn bucket aggregates its complete span for its whole visible life: 0 changes in every configuration afterwards, and the left-hand columns fall to the floor that a stride-1 geometry — which cannot have the defect — shows before and after (0.080 / 2.23 px Simple, 0.259 / 9.93 px Advanced). `kFull` is capped so the alignment fits the ring (costing a saturated window one bucket), and a bucket the producer has lapped into is dropped rather than truncated (`firstDrawn`). **The `span / 2` cap answers the PR review's first finding**: `buckets` floors `kFull` at 2, so a window of two entries or fewer (`want ≤ 2`, i.e. `blockSize ≥ 10 · sampleRate` — an offline render's block) reported one pitch as the whole plot and the uncapped boundary landed one column left of the left edge, blanking the history on 100 % of frames at 48 kHz / 480000 and 44.1 kHz / 441000 on both wells. `pitch == span / 2` exactly at `kFull == 3`, so the cap engages only at `kFull == 2`, holds the boundary where a three-bucket window puts it, and leaves every other geometry bit-identical (verified: every rendered column identical before and after the cap on six configurations from 512 to 479999-sample blocks, 480–720 frames each). pluginval at strictness 10, both modes ×3, green locally. OQ-017 (host delivery) unchanged. The previous entry, 0.2.11, and its investigation stand as recorded below. |
| **Current Phase** | **P6 — v0.1.0 CODE COMPLETE (2026-08-02, under the owner's blanket approval; every item taken under it is ⊕ for the post-v0.1.0 fine review).** The last two owner gates are decided and wired: **ADR-0013** (OQ-016 — the release trim scales the auto poles; all four adaptive behaviours audible at defaults) and **ADR-0014** (OQ-013 — the frozen-trim vector restores: captured at save, staged on ADR-0012's row, applied at the duck's silent bottom; MODE inv 3's last gap closed). OQ-014 resolved (reading 1 — listener-guard row) and OQ-007 resolved (v0.1.0 ships plain zips). The preset bank is Default + the brief's 12 (5 brief-named + 7 ⊕; Default added 2026-08-05, owner directive); the brand checklist is provisionally passed under the approval (the real Level-5 human pass is the first fine-review item); CI pluginval strictness is raised to the P6/release value (`build.yml`'s `env:` block — this row quotes no number). Still owed before tagging v0.1.0: the human fine review (brand pass, DAW matrix audition, preset/curve listening pass), OQ-002 (licence — blocks commercial distribution only), OQ-009. (OQ-008 was SUPERSEDED 2026-08-05: the streaming-target display it verified was removed outright by owner directive.) Previously: P5 UI code complete at L8 (2026-08-02); P0 closed 2026-07-31; P1 closed 2026-08-01; P2/P3/P4 complete 2026-08-01. |
| **Branch Strategy** | Feature branch → PR into `main`. CI builds every branch; `main` carries shipped versions. Release tagging convention: annotated `vX.Y.Z` — **wired since 0.1.1**: `release.yml` validates the tag against `CMakeLists.txt` and a dated CHANGELOG heading, reuses `build.yml`, and opens a DRAFT release with the three zips, both installers, SHA256SUMS and a build manifest (ADR-0021). |
| **Build Status** | **Builds green on Linux** (P1 skeleton, 2026-07-31): `CMakeLists.txt` per ADR-0008 (five targets, JUCE @ the pinned SHA fetched via FetchContent — **9.0.1 `e18f7f5…` since ADR-0028, 2026-08-16**; 9.0.0 `f8f8864…` before it — C++20, warning-free under the recommended flags), `src/` + `src/dsp/` + `src/gui/` exist. The `preflight` guard now takes its ready=true path, so the full 3-OS matrix runs in CI; Windows/macOS results arrive with the first CI run of this commit. The `docs` job continues to run on every push and gates nothing. **The §2.1 C++23 canary is wired as of 2026-08-05** (`cxx23-canary.yml` — weekly + `workflow_dispatch`, DSP suite built AND run at C++23 on all three platforms, non-blocking by structure and never a required check; OQ-006 Resolved ⊕). |
| **Test Status** | **1394 checks green on Linux** (`AnabasisTests` 316 + `AnabasisStateTests` 1078), re-counted from the suites' own output. 0.2.12 adds thirty-six checks to the state suite. Nine for the boundary itself: the walk in `testGrHistoryWindowNeverAsksForTheHeadSlot` gains one pin per geometry case (the visible boundary sits left of the newest drawn vertex at every head and both ends of the phase, is a constant of the window, equals `right − ceil (pitch) − 1` and leaves the plot most of its width), and `grPaint` gains the boundary's arithmetic (on this view and on both shipped wells: 910 of 10…913 on the Simple well, 610 of 10…613 on the Advanced) and re-pins the rendered snapshot at every fill of the newest bucket on both halves of the contract — the last VISIBLE column lit on every fill, every column from `visibleRight` to the plot edge untouched on every fill. Twenty more on the PR review finding that a ten-second host block blanked the plot: a boundary sweep over twenty window sizes on both wells (never empty; bit-identical to the uncapped bound wherever `kFull ≥ 3`; the two-bucket window held at the half-span bound, which is exactly the three-bucket boundary; non-increasing in the pitch; never hiding more than half the span), the two narrow-plot guards, and `testGrHistorySurvivesAHostBlockOfTenSeconds`, which RENDERS a view prepared for a ten-second block and asserts the history is drawn and still stops at the boundary — the pin no arithmetic static could carry, since every one of them was green throughout the blank. Six more on the review's second finding: `testTheOldestDrawnBucketKeepsItsValueUntilItLeaves` walks a REAL ring past 400 heads with a pattern whose minimum sits on the first entry of every bucket, so a truncation would move a drawn value by 11 dB, and asserts that no bucket ever changes value while drawn; the walk's frozen-values pin now covers the OLDEST drawn bucket rather than starting one past it; the window law pins the aligned start and the ring bound; and the race test drives the paint's own caller (`firstDrawn`) and pins that a lapped bucket is dropped with every surviving bucket reading its complete span. Mutant record in `worklogs/2026-09-05-gr-history-tip.md` §7, §8 and §9. Before 0.2.12: 0.2.11 adds thirteen checks to the state suite and changes the walk in `testGrHistoryWindowNeverAsksForTheHeadSlot`: the two 0.2.8 tip pins (pinned while filling, drifts when complete) are replaced by five — only complete buckets are drawn and the count says which; the newest drawn vertex is within one pitch of the edge and ON it the frame its bucket completes; every drawn bucket reads exactly its own span at every head; a completing bucket appears at the edge as a NEW vertex one pitch past the previous; a bucket once drawn stays drawn — plus a `grComplete` block (the bucket-boundary arithmetic at stride 3) and, in `grPaint`, a RENDERED snapshot at every fill of the newest bucket asserting the last plot column carries the trace. Mutation-verified, each mutant a one-site edit of the fixed tree: drawing the half-collected bucket (`kLast = kHead`) fails 24 checks across all six geometry cases and both degenerate cases; a trailing read window for the newest drawn bucket fails exactly the five values-frozen walks; the lead-out ending on the anchor fails exactly the rendered last-column check — the one property no static can carry; and restoring `bucketX`'s pinned-tip branch fails NOTHING, which is the intended result — under `kLast` that branch is dead code, equivalent for every drawn vertex, and the pin sits on which buckets are drawn rather than on the branch. Before that, 1345 (316 + 1029). 0.2.10 adds five checks to `testAWellFormedDocumentCannotCarryAnUnusableNumber` and a new `testAnInvalidSampleRateCannotSizeABuffer`, and each was mutation-tested rather than merely run: reverting the infinity predicate alone fails exactly the two endpoint checks, removing the fallback repair alone fails exactly the three fallback checks, and the unguarded `prepare` fails all four rate/block combinations. The endpoint checks assert the parameter lands ON the rail rather than merely staying finite — the fallback 0.2.9 substituted is finite too, so no finiteness test could have told them apart. 0.2.9 adds the six checks of `testAWellFormedDocumentCannotCarryAnUnusableNumber`, which is a regression test in the strict `TESTING_POLICY.md` rule-1 sense: reverted to the code before the fix it reports exactly four failures — the parameter value, the printed text, the re-save, and the preset path — so no facet of the guard is pinned by assertion alone. It is the third corruption case, and the one that got through: `testCorruptAndForeignState` covers random bytes and a foreign root, both rejected before anything is read, and neither ever reaches a parameter. Round 7 added the `resetObserved` truth table (six rows; the revert mutant fails exactly the backwards-count row) and `ki017c` (the published channel count, through construction, a layout change with no re-prepare, and a prepare). One change ships under the **ADR-0025 exception** with its four disclosures — `analyse`'s floor on a zero-length read, whose precondition a single-threaded suite cannot construct. Round 8 added no checks: it is a closure round that corrected records rather than behaviour — the stale-spectrum review finding audited and closed as FIXED against the shipped code, round 7's completeness argument corrected (a third leg closes the partial-refill mixture, by force, and the completeness is PER RING), KI-018 re-scoped and re-bounded, and the `source-lint` citation gate returned to zero drift against the base CI actually uses. Round 6 added the three `ki017` checks (the EQ curve's rate source, pinned through two rendered snapshots at different rates — identical on the old source, different on the new) and, for the GR lap fix, a rule in `check-realtime.py` rather than a suite check, because no deterministic test can see a memory ordering. The KI-015 follow-up added the six `specSync` checks in the DSP suite (the spectrum rings' payload type, the round-trip premise, and the `n > capacity` branch nothing in the tree had ever executed); two mutants, each killing exactly one of them. 0.2.8 added 71 checks inside `testGrHistoryWindowNeverAsksForTheHeadSlot` (a sixth geometry case, the per-entry walk, the trailing tip window, the smoothed head, its phase, the tick gate and the paint head) and, for the review round's three reader/publisher findings, ONE new test — `testGrHistoryReaderStaysInsideTheRingAndSeesEveryReset` (59 checks: the ring-safety invariant swept over every staleness and head, the same bound against a real saturated `GrHistoryBuffer`, the non-regression that the floor binds nowhere else, every stale/fresh pairing of the two published scalars, the equal-count reset, the validation path's own synchronisation, the reset-phase anchor over every refill state, the real paint path through the atomics, and the prepared pair published inside the clear and read under the epoch — through the ring and through the wrapper's `prepareToPlay`; its ring and processor fixtures live on the heap, for MSVC PREfast's summed-locals C6262); the walk collects each property over ~3·stride heads per case and asserts once, so a single bad head fails the case rather than one of hundreds of near-identical lines. 0.2.0 added ONE test and five checks — `testTheAudioPathAllocatesNothing` — and its value is not the count: it arms `tests/AllocationGuard.h` around `AnabasisEngine::process` over both channel counts × five oversample factors × two phase modes × four parameter sets, plus a mid-stream oversample rewire driven with no re-prepare, and reports **0 allocations over 2,040 armed calls with both counters proved live in the same run**. It DISCLOSES rather than passes where a counter is unavailable: under ASan the malloc half is compiled out (300 checks, one assertion skipped with a note), and under RealtimeSanitizer and valgrind the whole guard stands down (296 checks, five skipped) because its definitions would otherwise shadow RTSan's interceptors and make memcheck report every guarded delete as a mismatched free — both measured, both stated at the skip. The round's other evidence is outside the suites and is listed in ADR-0029 §Evidence: the DSP suite runs **violation-free under RealtimeSanitizer**, its liveness canary aborts with the sanitizer's own report at exit 43, the `-Wfunction-effects` canary fails with the expected diagnostic while the real leaf layer compiles clean, and the annotation on the frozen audio path produces a **byte-identical object** (clang-22, -O3, project flags, live vs emptied: 105,224 bytes, same MD5). Both suites are additionally green **at C++23 under GCC 13.3, Clang 18.1.3 and the pinned Clang 22.1.8**, the last with **zero first-party warnings** under the full gate and no baseline file. Six checkers now ship `--self-test` and run it in the job they vouch for: check-docs 67 cases, check-portability 120, check-realtime 145, check-citations 37, check-linux-abi 19, check-clang-warnings 18. (This row said "Five" while listing six, and carried check-realtime 90 and check-clang-warnings 15 — both stale: 0.2.10 added four `scan_repo` cases against real temporary trees to the realtime gate, and `CI_CD.md` already records the clang-warnings self-test growing 15 → 18. Corrected 2026-09-03 from a measured run of all six, found by the audit-record consistency sweep.) 0.1.6 added the round's two state tests — `testGrHistoryAndTheMeterLanesShareOneReductionSpan` (the GR trace's vertical mapping through the pure `grY`, including the assertions that say the fix RE-SCALED NOTHING: 12 dB still lands at half height; plus a rendered `GrMiniMeter` lane, so the "both readouts agree" claim is measured on the meter rather than re-quoted from the constant) and `testPercentTextEntryReadsFractionsAndLiteralPercents` (the fraction rule, the literal-`%` escape hatch, the spellings that already worked, the fractional DISPLAY, and `getValueForText(getText(v)) == v` across the fraction window). Four mutants were run and each killed a disjoint set: the pre-0.1.6 GR mapping 4 of 9; a `GrMiniMeter` divisor of 12 only the rendered-lane assertion; the pre-0.1.6 parser exactly the 5 fraction assertions; the integer-only formatter the fractional-display assertion **and the round-trip**, which is what says that half was destroying values rather than only under-reporting them. **The 0.1.5 JUCE 9.0.1 bump added no test and moved no number, and that is a measurement rather than an assumption**: built from the same source at both pins, the two suites' combined stdout is BYTE-IDENTICAL — the `comboFit 31 px` and `shortcutRow 5 px` layout advisories included, which is the part a graphics or font change would have moved — and `AnabasisChannelProbe` agrees digit for digit at nine decimal places over 33 configurations of the shipped VST3 bundle (ADR-0028) — from the **Clang-LTO'd** bundle as well as the GCC one, which is the configuration INC-004 taught us to check. valgrind memcheck was re-run on both suites at the new pin (0 errors from 0 contexts each); the ASan + UBSan leg named below is CI's, not this machine's. 0.1.4 added ten state tests: `testANoOpPresetApplyIsNotAUserAction`, `testANoOpPresetApplyDoesNotEatTheOldestUndoStep`, `testAMalformedStoredSlotCannotSplitSoundFromMetadata`, `testThePopupShieldActuallyCoversTheEditor`, `testAPopupRowKeepsItsLabelOutOfTheShortcutStrip`, `testTheResizableFrameOverrideDiscriminatesItsCallers`, `testEveryComboMenuFitsItsControl`, `testARootlessSurfaceDropsTheActiveSlotsMetadataToo` `testAShortcutRowIsMeasuredWideEnoughForItsOwnLabel` and `testANoOpPresetApplyIsNotAUserActionAfterASessionRestore` — each mutation-verified against its own reverted fix. `testEveryComboMenuFitsItsControl` also carries the measurement identity (`idealWidth − textW == menuMetrics::chrome`), added 2026-08-14 after the `>= 24` pixel floor was downgraded to `>= 0` + an advisory print; three mutants were run against it and the result is written at the assertion — dropping chrome from the measurement is KILLED, while redefining the shared constant and changing what `drawPopupMenuItem` actually spends both SURVIVE, the second being a real gap that only a test observing the rendered row can close; `testANoOpPresetApplyIsNotAUserActionAfterASessionRestore` gained the DIRTY-MARKER leg (2026-08-14): the retraction deliberately does not roll `presetBaseline` back, so a restored session leaves a no-op re-apply holding a working marker where it had none — asserted as clean straight after and as ANSWERING on the next edit, the mutant being a retraction that rolls the datum back, which fails exactly the second assertion; `testPresetIdentitySharedName` also gained the name-only Copy leg that is the sole guard on `presetName` surviving the undo compare. The macOS figure is 0.1.3's **1039** (`AnabasisTests` 296 + `AnabasisStateTests` 743) until the next macOS run reports 0.1.4's; the two platforms run the same suites, so the gap is a measurement date, not a coverage difference. The macOS half is newly true rather than newly counted: the macOS job had failed to COMPILE the state suite on every push from 2026-08-08 to 2026-08-10 (`POSTMORTEMS.md` INC-003), so the KI-009 regressions written in that window had never run on the platform they were written for. Both suites are also clean under **valgrind memcheck** (`--track-origins`, 0 errors from 0 contexts) and under **ASan + UBSan**. Detail below is the Linux history: **1015 checks green on Linux**: `AnabasisTests` (293 — ADR-0013 auto-release-follows-the-trim-scale, null-with-defaults bit-exact, impulse-at-allowance for four lookahead values, ceiling clamp, control/gain priming, limiter window coverage and alignment, smoothing of ceiling and lookahead, hostile-input finiteness, self-heal recovery, recovery from a stage that overflows on a FINITE input (EQ biquad in BOTH positions, RMS detector, colour c⁵, polyphase IIR up and down — `testExtremeLevelDoesNotSilencePermanently`) and from the stages that emit no audio to check (the BS.1770 meters and the §5.4 feature extractor — `testExtremeLevelDoesNotBreakTheMetersOrAdaptation`) and a Learn pass that measured through an overflow never becoming the saved reference (`testALearnPassThatOverflowedIsNotCommitted`), bypass null, EQ frequency response/smoothing/positions, the ADR-0002 post-shelf ceiling stimulus, compressor static curve/detectors/mix/two-stage auto release/sidechain HPF, clipper curve/compensation/ADAA aliasing/colour models/dynamic tame, true-peak accuracy, limiter link/styles/preserve/two-stage auto/dBTP mode (the detector-HPF tests left with the filter at 0.1.2 — ADR-0023), the full OS latency matrix, OS aliasing/transparency/bypass/ceiling, dither modes, the §2.8 duck on rewires/latches/requests, LUFS calibration/gating/windows, inv-10 monitoring honesty incl. the mid-stream offline flip snap, delta, the duck-bottom hold, the post-latch refill hold, a request held through the out-leg, delta covered by the duck, the last-staged-restore rule, stale detector state (the true-peak history half; the HPF half left with the filter), limiter control smoothing incl. link/preserve glides) plus meter publication and the GR ring in the state suite and `AnabasisStateTests` (722 — the ADR-0014 frozen-trim restore (`testFrozenTrimRestore`: both landing sites, both staging sites, the capture, the no-audio mirror, the undo case, the consume-to-bottom save window, freeze-off inert — each killed by its own mutant), the undo duck (`testUndoRequestsDuck`), gesture begin/end symmetry, the 13-preset factory bank (Default + 12), registry snapshot vs the frozen fixture, 50/9 counts, raw-exact byte-identical round-trip and its fixed-point precondition, structural-tolerance read rules, batched latency notification, corrupt/foreign no-op, macro fixed point, restore-vs-macro-drain, A/B tier behaviour, preset contract, cache mapping, the ADAPTIVE missing-field defaults, meters reading the render not the monitor path, load-then-save with no audio between, the zero-length-block publish guard, and — first in the tree to construct the EDITOR — the Settings panel following a project load in both directions, the graph-well views claiming only their corner mode chips (spectrum ⇄ GR), the R2 tooltip set (no hoverless slider/combo, the named toggles tipped — `bypass` deliberately excluded), and every knob's animated position starting where its value already is), plus `testTeardownAndReengageInvariants` (no trigger drains after `stopDraining`; a macro gesture that moves nothing re-lands the curve; a Copy is an undo step on the DESTINATION whose earlier history survives beneath it, and an ADV toggle undoes while a bypass click mints nothing — ADR-0018). The 0.1.1 round added the ADR-0019 comp stereo link (`testCompStereoLink` — full/half/zero link plus the zero-link bit-exactness), the ADR-0020 statistics DSP (`testRmsMeterReadsTrueLevels`, `testLoudnessRangeAndTheUngatedReading` — the −20 LU LRA gate and the 30-sub-block reset watermark each mutation-verified) and its wrapper publication (`testTheWaveformStatisticsRowsReadTheirStandards`), the mono→stereo wrapper case that reproduces KI-009, and `testFrequencyTextEntrySpeaksMasteringShorthand`. The 0.1.1 review rounds added case (3b) of `testTeardownAndReengageInvariants` (an ADV toggle between the freezing A/B switch and a Copy: undoing the Copy reverts the sound and leaves the view — ADR-0018's amendment) and two `testRmsMeterReadsTrueLevels` assertions separating the RMS meter's sentinel from its floor (digital silence and a −163 dBFS signal both read `kFloorDb`, never “nothing measured yet”), and four `testTheWaveformStatisticsRowsReadTheirStandards` assertions pinning the PLR row as TP minus the integrated figure the panel SHOWS — reproducing the published gated value under the default, and more than 2 LU away from it under BS.1770-1. Round 4 added case **(3c)** (three Copies in a row leave one step) and **(3d)** (a no-op Copy leaves the destination's redo line intact), and `testGrHistoryWindowNeverAsksForTheHeadSlot` grew the GR decimation geometry — five rate/block/width cases plus a barely-filled ring, asserting that the trace spans the whole panel width while carrying exactly one window of entries and that every drawn bucket is non-empty. Round 5 added `testTheCachedLoudnessReadingsAreNeverStale` (a committed gating block moves both cached readings; a reset drops both to their sentinels with no audio between) and two `grBuckets` assertions pinning the single-bucket case the draw loop special-cases. Case (3b), both floor assertions, the PLR sentinel guard, (3c), (3d), the stretched `bucketX`, the `head - 1` bucket key and both `invalidateReadings()` sites are each killed by their own mutant and no other. The ADR-0022 preset-identity port (2026-08-08) added `testPresetIdentitySharedName` (identity decides the selected row: a save under a factory name selects the USER row, both rows individually selectable, A/B and undo carry the identity, an outside-folder or deleted file selects NOTHING, and the `.anabasis` FILE gains no field), `testFactoryPresetIdIntegrity` (ids non-empty, unique, each round-tripping to its own row) and `testPresetIdentityAcrossRestore` (the full fallback matrix per SLOT — factory, unresolvable id, user vs same-named factory, nested sub-folder, missing file, pre-ADR-0022 name fallback, per-slot A/B identity, the tilde-named direct child, and a repeated restore inheriting nothing — every path asserting bit-identical parameters); an adversarial review round then added the Copy carrier cases (identity travels; an identity-ONLY move under one name and sound still mints an undo step; a pre-ADR-0022 stored slot mints no phantom), the no-AB identity reset, the all-defaults cardinality and the guarded factory-table premises; thirteen negative controls (resolver fall-through, save identity, the SLOT trio writer, the adopt decode, the restore decode, both encoder conditions, the ctor seed, a duplicated id, the compare normalisation reverted, the compare over-normalised, the reset seed deleted, a second all-defaults table) were each killed by their own assertions. A second review round added `testTheRingWalksPastAnUnreadablePreset` — the ‹ › ring must not be TRAPPED by a corrupt `.anabasis`, which resolving its position from the identity made a live hazard, since the identity moves only on a successful apply; the single-shot step is the mutant, and it fails exactly the two assertions that say the arrows moved and kept moving. `AnabasisTests` also pins the limiter push's chain position (`testLimiterPushDoesNotDriveTheClipper`) and that a realtime→offline flip does not duck the render (`testOfflineFlipDoesNotDuckTheRender`) while the return edge stays ducked (`testReturnFromOfflineIsDucked`), and that an EQ-position change on the offline-entry edge starts from cleared filter state (`testOfflineEntryClearsEqStateOnAPositionChange`). The 0.1.2 field-fix round (ADR-0023) rewrote `testNullWithDefaults` into the round's acceptance test (a −0.6 dBFS 30 Hz square with the adaptive trims live: bit-exact null, zero comp AND limiter reduction, and Delta at defaults as exact digital silence — the sub-ceiling stimulus is itself asserted hot enough that the old centred knee fails it), replaced `testLimiterDetectorHpf` with `testLimiterDetectorIsUnfiltered` (a 30 Hz over ducks with SC HPF at 300 Hz; the comp's clamped detector never reduces deeper than the raw one), rewrote the `grBuckets` geometry section for the fixed right-anchored scale (constant pitch across fill states, newest bucket on the right edge, the quarter-full negative control against the old stretch), split `testGrRingResetEpoch` into the same-config-keeps-history and changed-config-clears halves, moved the graph-switch click tests to the bottom-left toggle-anywhere pill, added the per-channel GR lanes to `testMeterPublication`, and grew the channel battery by mono→mono (accepted, applied, mastering one channel; stereo→mono still refused) plus the six KI-009 diagnostic configurations (Delta engaged with the channels pinned within 6 dB, loudness-comp on, limiter link 0 %, shaped 16-bit dither, true peak, 44.1 kHz). Eight negative controls across two mutant builds — centred knee, dropped detector clamp, deaf limiter detector, stretched `bucketX`, unconditional ring clear, segment-only pill, dropped per-channel accumulation, refused mono→mono — were each killed by their own assertions. The registry snapshot was deliberately re-frozen for the one NAME change ("Limiter Stereo Link"). The 0.1.3 polish round and its follow-up added: the KI-009 field-configuration pair to the channel battery (both links 0, comp threshold low, limiter gain at the range top: per-channel comp GR, per-channel limiter GR and both outputs asserted alive, at OS Off AND 4× — the headless twin of the owner's GR-lane observation and of the OS-toggle field experiment KI-009's 0.1.3 addendum hands back; the limiter-GR assertions demonstrated their own sensitivity by failing until the push was raised to +18), renamed the tooltip sweep's expected caption COMP → MATCH, and re-froze the registry snapshot again for the three Colour → Color NAME cells (rule 2, IDs untouched). The follow-up round then ROOT-CAUSED the KI-009 fingerprint: `testClipSatCannotLoseAChannel` pins the §2.4 stage as channel-symmetric and non-finite-free over a swept fuzz (2.4 M samples) plus an asymmetric survival case, and `testExtremeLevelDoesNotSilencePermanently` gained the SUSTAINED, ONE-CHANNEL case its four single-block runs never reached — three Clip Mix values × four magnitudes, asserting both channels alive; removing the colour polynomial's argument bound fails exactly those six assertions and no others, and only at non-zero mix, which is the field gating reproduced in the suite. `testTheWaveformStatisticsRowsReadTheirStandards` gained the RMS row's two display rules (`shouldAdoptRms` cadence + both sentinel bypasses; `rmsWithReference` applied OUTSIDE the hold so a Settings flip is never delayed — the mutant is the shipped 0.1.3 form). **pluginval green ×3 in both modes on Linux at the P6 strictness (editor under xvfb)**; CI gates at the same value on all three platforms — the number lives in `build.yml` alone. Re-count from the suites' own output when editing this row; it has gone stale once already. |
| **Release Status** | Pre-release: no `vX.Y.Z` tag has been cut, so nothing has left this repository and the compatibility contract can still be shaped at zero cost (`COMPATIBILITY_POLICY.md` §"When the contract starts"). (This row read "Pre-0.1.0" from P0 until 0.1.2 — stale twice over once versions 0.1.1/0.1.2 existed as CHANGELOG entries; the no-tag condition is the real content and is what it now states.) |
| **Known Blockers** | **No code blockers.** Every formerly `Blocking` question is Resolved: OQ-013 by **ADR-0014** (2026-08-02 — the frozen-trim restore is wired and mutation-verified), OQ-016 by **ADR-0013**, OQ-014 (reading 1) and OQ-007 (plain zips) by the same owner call, OQ-010/OQ-011/OQ-004/OQ-005/OQ-015 earlier with their ADRs. What blocks the RELEASE rather than the code: the post-v0.1.0 human fine review (the blanket approval's other half — brand pass, DAW audition, listening pass over the ⊕ constants/presets), **OQ-002** (JUCE licence tier — blocks commercial distribution only), OQ-009 (owner metadata). (OQ-008 superseded 2026-08-05 — the display it verified is gone.) This row must agree with every `Blocking` entry in `docs/OPEN_QUESTIONS.md` — check it there, not here, when adding one. |
| **Pending Tasks** | **0.2.0 adds THREE, all of them evidence a CI run has to produce rather than work to do.** (1) **The first green `realtime` job is OWED.** ADR-0029's RTSan lane was built and run on this machine against an apt.llvm.org clang-22, and the DSP suite passed violation-free — but the lane has never run in CI, and `REALTIME_SAFETY_AUDIT.md` records that as owed rather than assumed. (2) **The pinned Clang and the C++23 baseline are confirmed on Linux only.** Windows (MSVC) and macOS (AppleClang) compile the raised standard for the first time with this commit's run; neither is affected by ADR-0031, which pins nothing on those platforms. (3) **The macOS symbolication contract has never been exercised.** `-Wl,-object_path_lto` and its two assertions are correct by construction and by the sibling's measurement; the run that proves a UUID-matched dSYM comes out is this commit's. **One item is DEFERRED with a measurement rather than left open**: an LTO test lane (migration item A2-32) is worth adopting — it is the configuration INC-004 required to manifest — but it finds nothing today (both suites pass LTO-built at the same counts) and costs a measured 172 s against 27 s to rebuild the two suites, so it is recommended for a following round rather than folded into this one. A libFuzzer lane over `setStateInformation` (A2-33) is **declined on measurement**: the state suite already executes **100 % of that function's lines** and 97.5 % of `applySlotToLive`'s, so a fuzzer would add input diversity rather than reach. **0.1.5's item stands unchanged and is still the oldest: the JUCE 9.0.1 bump's Level-5 audition is OWED.** 0.1.5's row, verbatim: ** the JUCE 9.0.1 bump's Level-5 audition is OWED.** [ADR-0028](architecture/design-decisions/ADR-0028-juce-901-pin.md) cleared the Build System gate on the owner's directive and discharged `DEPENDENCY_POLICY.md` rules 3, 6 and 7 with evidence, but rule 2 has two halves — the headless gate **and** a manual audition — and only the first is done. The compliance log's first row says so in the row itself rather than leaving it to be inferred. Two things ride with it: the **three-OS half** of the same rule (Windows and macOS suites + pluginval land with this commit's CI run, and the delta's two platform-specific changes are on that side of the line), and the **re-convergence of the pin with Anamorph**, which is not work this repository can do — the sibling is read-only from here — and is therefore recorded as an open obligation rather than a task. **Both 0.1.4 architecture-review gates are CLEARED (2026-08-14).** [ADR-0026](architecture/design-decisions/ADR-0026-slot-payload-read-rules.md) — the two `SLOT` read rules, a slot carrying no `ANABASIS` child resolving to defaults as a whole and the ACTIVE slot's metadata being adopted only when the ROOT surface was restored — and [ADR-0027](architecture/design-decisions/ADR-0027-painting-thread-reads-editor-bookkeeping.md) — one editor counter read from the PAINTING thread through a relaxed `std::atomic<int>` — were both **Accepted on the owner's explicit approval**, quoted in each ADR's Status banner and indexed in `ADR_INDEX.md`; `THREADING_POLICY.md`'s **Message → Painting** row is settled rather than pending, and `SESSION_COMPATIBILITY_POLICY.md` rule 1 carries the pointer. Both changes were implemented WITHOUT being flagged as gated and review caught each on 2026-08-13. The lesson is kept in each ADR's banner rather than in this row: a rule can be quoted accurately in the same commit that fails to apply it. **P1–P6 code work is DONE** (phase histories in the summaries below; the v0.1.0 completion summary carries the final batch). What remains is the **post-v0.1.0 fine review** — the other half of the owner's blanket approval: (a) the item-by-item Level-5 brand pass (`BRAND_CONSISTENCY_CHECKLIST.md` — provisionally passed, boxes deliberately unchecked); (b) the DAW matrix audition (`COMPATIBILITY_MATRIX.md` targets); (c) the listening pass over every ⊕ — trim mapping constants, §5.5 macro curves, tame/model weights, the 12 factory preset value sets, the gold/amber accent; (d) a second look at every decision dated 2026-08-02 (ADR-0013, ADR-0014, OQ-007/OQ-014 readings, the 7 preset names/values); (e) ~~OQ-008's first-party value verification~~ — superseded 2026-08-05, the streaming-target display was removed outright; (f) the 3-OS CI confirmation of this batch (suites + pluginval at the `build.yml` strictness — the number lives there alone, as the Test Status row already says); (g) ~~an explicit owner acknowledgement of ADR-0015's schema half~~ — **CLOSED 2026-08-06**: the owner reviewed ADR-0015 and signed off all three contract changes by name (the `int_meterTargets` removal, the `ceiling` default, the `truePeakMode` default), clearing the Architecture Review Gate. The sign-off is recorded in ADR-0015's Status banner and indexed in `ADR_INDEX.md`; those three decisions are settled rather than ⊕, and the rest of the fine-review list is unaffected; (h) ~~the same clearance for ADR-0016~~ — **CLOSED 2026-08-06**: cleared separately and on its own terms, the owner's confirmation naming the semantic change, the pre-1.0 migration decision and the acceptance that stored values load with no migration path; (i) ~~the same clearance for ADR-0017~~ — **CLOSED 2026-08-06**: cleared separately again, the owner's confirmation naming the reduced ladder, the acceptance that out-of-set stored values normalise on adoption, and that this is a pre-1.0 decision with no released-session migration obligation. **All three gated records of the round-2 batch (ADR-0015/0016/0017) are now cleared, each on its own terms.** (j) the 0.1.1 round's own ⊕ items — the ADR-0018/0019/0020/0021 decisions were taken under the owner's 0.1.1 directive and its standing sign-off instruction, so their gates are cleared but their *wording and values* (the statistics panel's labels and standards defaults, the comp-link default, the About copy) join the same fine-review list as every other ⊕; (k) ~~the first CI run of the 0.1.1 packaging steps~~ — **CLOSED 2026-08-07**: run 31135082913 built both installers, all three platforms green. What ADR-0021 still carries is narrower: the Linux install scripts have not been run as root, and `release.yml` awaits its first tag (or a `workflow_dispatch` rehearsal). (l) the 0.1.2 round's own ⊕ items — ADR-0023 was taken under the owner's 0.1.2 directive (gate cleared 2026-08-09, quoted in its Status banner), and its two audible contract changes (the knee-above static curve at every loudness, the unfiltered limiter detector on bass-heavy material at raised SC HPF) join the listening pass with the other ⊕ constants. (m) **KI-009 (left channel silent) is CLOSED as of 2026-08-11** — root-caused, fixed and verified. It was never platform-scoped: `AnabasisEngine::processChunk` bounded its channel loops with a value that is always ≤ 2 at runtime but unbounded to the compiler, which made eight `float[kMaxChannels]` stack frames look like possible out-of-bounds writes; Clang at `-flto` — the configuration that builds the macOS product, and by ADR-0008 the ONLY configuration the plugin target is built in — acted on that undefined behaviour and dropped channel 0. Adding `kMaxChannels` as the third `jmin` term restores bit-for-bit agreement with the GCC build. `POSTMORTEMS.md` INC-004 carries the mechanism, why every console-target gate was green throughout, and the three reproductions that now guard it. The earlier reading of this row — "macOS-only, no Linux-side experiment can move it" — was wrong twice over: building the *plugin* with Clang on Linux reproduces it exactly, which is where it was finally fixed. What remains OPEN is **KI-012** (the Linux editor accepting no mouse input) is the reverse case — it does NOT reproduce here, under XTEST pointer injection into the built VST3 hosted over XEmbed on a real X server, bare and under a window manager, so it needs the reporter's host, desktop environment and the one discriminating observation the entry names. Commercial release additionally waits on OQ-002 (licence tier) and OQ-009 (owner metadata); the OQ-007 packaging deferral is **lifted** (ADR-0021), signing and notarization excepted. |
| **Roadmap** | P0 research & design → P1 skeleton (pluginval L5) → P2 DSP core → P3 metering engine → P4 Simple adaptive engine → P5 UI → P6 polish & release (pluginval L10, DAW matrix, docs). `DEVELOPMENT_BRIEF.md` §11. v2 candidates (codec preview, reference matching, dynamic EQ, multiband limiting) are out of scope — leave architectural room only. |
| **Ownership** | `TODO: no owner/team metadata in the repository. Requires project-owner input (OQ-009).` Company of record: RollyTech. |

## 0.1.2 (2026-08-09) — the field-fix round: thirteen owner items, ADR-0023

The owner's 0.1.2 directive named thirteen items; the investigation that preceded the fixes is
the part worth handing over. **Item 2 (gain reduction at the Default preset on legal material)
had three real mechanisms**, none a metering artifact: the comp's centred knee computing gain
from −3 dBFS up, the §5.4 adaptive scHpf trim engaging the shared sidechain HPF in the
LIMITER's detector at factory defaults (its transient overshoot over-reads LF edges by up to
~6 dB — false reduction; the same filter under-read bass overs into the CeilingClamp as hard
clips), and the by-definition ≤ 0.1 dB on peaks above the −0.1 dB Ceiling (unchanged, now the
only reduction reachable at defaults). The contract-level fixes are ADR-0023: knee above the
threshold, an unfiltered limiter detector (scHpfFreq is comp-only now), the comp's filtered
magnitude clamped to a raw-magnitude CEILING — a peak envelope, after the round's review found
the first pointwise form re-coupled the detector to the bass it exists to ignore. **Item 1 (KI-009, left channel silent) got its decisive
narrowing rather than a fix**: the owner's Delta observation algebraically proves the left
input pin is live and the loss sits in the engine's processed leg — a fingerprint two
independent audits could not express in the current source with intact state (the KI-009
0.1.2 addendum carries the full argument and the remaining hypotheses); what shipped is the
instrumentation (per-channel GR lanes), the removal of the GR confound from the Delta
diagnosis, six new battery configurations covering the exact modes the report was observed
under, and mono→mono closing the layout-negotiation surface. The display work: fixed-scale
right-anchored GR history with a zero-data unmeasured region and a ring that survives
same-config re-prepares (the pause/resume backward jump was `prepareToPlay`'s unconditional
clear feeding the stretch draw); the GR|SPEC pill bottom-left as a whole-pill toggle with GR
the default view; captions without stage prefixes over unchanged automation names (one
deliberate NAME rename: "Limiter Stereo Link", snapshot re-frozen); the Advanced macro row
removed (940×822). Everything is in `CHANGELOG.md [0.1.2]`, the decisions in ADR-0023, the
test deltas in the Test Status row above.

## Post-v0.1.0 development (2026-08-05) — the C++23 canary lands (OQ-006)

**The one §2.1 mandate that had no implementation behind it.** ADR-0008 mandates the canary's
*existence* (builds C++23, all three platforms, never gates); OQ-006 held only scope and cadence,
with its own written recommendation ("DSP core + tests, weekly + `workflow_dispatch`, added at
P2"); the P1 phase summary below records "scheduled for P2" — and P2 closed 2026-08-01 without
it, with no later phase summary re-raising it. Found by walking `DEVELOPMENT_BRIEF.md` against
the tree at the start of the post-v0.1.0 continuation, exactly the class of gap that walk exists
to catch.

**What landed.** `.github/workflows/cxx23-canary.yml` (three jobs, `AnabasisTests` target only,
built at C++23 and then RUN, weekly Monday 06:17 UTC plus `workflow_dispatch`; non-blocking by
structure — no `needs:` edge, no `continue-on-error`, never a required check) and the
`ANABASIS_CXX_STANDARD` cache seam in `CMakeLists.txt` (legal values 20/23, anything else refuses
loudly at configure). The seam exists because the obvious `-DCMAKE_CXX_STANDARD=23` is shadowed
by the project's unconditional `set()` and fails *silently* — the canary would have reported
green forever while validating C++20. OQ-006 → Resolved (⊕, adopting its own recommendation;
the entry records why that was takeable without an owner round-trip). `CI_CD.md` gains the
canary section and a fourth branch-protection trap (never require the canary).

**Verified before landing** (Linux, this machine): the canary configuration compiles every TU of
its target at `-std=c++23` with zero diagnostics and the DSP suite passes in full; the guard
refuses `ANABASIS_CXX_STANDARD=17` at configure; the default configuration reconfigured to
identical `-std=c++20` command lines and `ninja: no work to do` — zero recompilation, so the
shipped binaries this change ships beside are the same bytes the round-64 gate (suites ×2,
pluginval both modes ×3) already validated. Windows/macOS canary legs are unexercised until the
first scheduled or dispatched run — rehearse via `workflow_dispatch` after merge, the same advice
`msvc.yml` carries.

**C++23 canary status (§2.1 reporting duty): RUNNING from this commit** — previously "not yet
running" (P1 summary), silently unreported at P2–P6 closes. This entry is the status of record
until the next phase summary.

**Same date, `LATENCY_MODEL.md` lands** — the other contract authority
`COMPATIBILITY_POLICY.md` cited without it existing. Thin on purpose where `Latency.h` is
already the single source (the measured os tables are quoted nowhere in it); what it adds is
the map: the two-term contract, what never moves PDC versus what does, the measurement tap's
zero contribution, the five recompute triggers plus the load's documented no-op sixth, and
the property→test verification table. With it and the serialization ledger, every authority
document a binding policy points at now exists.

**Same date, `SERIALIZATION_REGISTRY.md` lands** — the schema ledger `COMPATIBILITY_POLICY.md`
has cited since bootstrap, written from the code with every node cited. Its evidence pass
produced two catches: `BASELINE` has **no originator in this build** (adopted, carried and
dropped but never created — the registry records it as schema-reserved rather than describing
a phantom producer), and the user manual had inherited the sibling's "omitted parameters keep
their default" preset claim, which is true here of sessions and factory tables but **not of
file applies** (overlay-only — an omitted parameter keeps its live value, visible the first
time a build adds a parameter). Registry states the asymmetry; manual corrected.

**Same date, the batch was adversarially verified and eleven findings fixed.** Every checkable
claim in the day's commits was independently re-derived and each discrepancy re-verified before
being trusted (the canary commit came back clean). The three worth remembering: the matrix
attributed a quotation to brief §2 that the brief does not contain (AAX is excluded by
omission there; the sentence lives in the README — and `COMPATIBILITY_POLICY.md` carries the
same pre-existing mis-attribution, recorded for its own change); the matrix quoted the
preset-burst bound in live prose while assigning its ownership to the checklist in the same
sentence; and HANDOVER's own Pending Tasks row still held the doc set's one live strictness
quote, three rows below the row that forbids it. The full list is the coverage audit's entry of
record.

**Same date, the User documentation class lands** — `docs/user/USER_MANUAL.md` +
`INSTALLATION.md`, the class whose P6 target had passed unmet. Every stated fact was read
from the tree first (registry ranges verbatim, the `kFactory` names, `userPresetDirectory()`,
`kLearnMinPassMs`, `kTiltPivotHz`, the editor's actual affordances, `isBusesLayoutSupported`);
no magic numbers shipped to users (no GR-window seconds, no undo cap, no performance figures);
INSTALLATION describes the OQ-007 zips as they are, including the executable-bit restore
`build.yml`'s own packaging NOTE makes necessary and the quarantine steps for
ad-hoc-signed bundles. Voice adapted from the sibling under ADR-0009; all wording ⊕ for the
fine review's brand pass.

**Same date, `COMPATIBILITY_MATRIX.md` lands** — the document this file's own Pending Tasks row
was already pointing the DAW-matrix audition at, and which `OQ-011` directed its supported-OS
restatement to. The audition now has a target list: rows A1 (REAPER/Windows/VST3) and A2
(Logic/macOS/AU) are the brief's §10 minimum, A3 is discretionary depth, and every host row is
`Unverified` until the audition records per-host evidence — the matrix states the C7 rule that
no row flips without it. Formats, platforms, the stereo-only I/O contract and the dependency
pins are cited to their evidence; no pluginval strictness number is quoted (single-place rule).

**Same date, the third-party attribution landing.** `RELEASE_POLICY.md` §"Third-party
attribution" had required `NOTICE` + `THIRD_PARTY_LICENSES.md` with every binary distribution
since bootstrap while neither file existed. Both now exist, produced by that section's own
prescription — inventory from the pinned JUCE `LICENSE.md` plus a compiled-TU walk (FreeType and
stb arrive transitively inside PlutoVG and are missed by every manifest-only reading),
compiled-in status from `nm` probes on this build's per-TU objects (the LTO'd image hides the
symbols — probing the `.so` says "absent" for components that are demonstrably in it),
exclusions confirmed by gate + symbol absence, structure adapted from the sibling under ADR-0009
with the findings re-derived rather than copied. All three `build.yml` staging steps now copy
both files into the customer artifact, so the obligation travels with the binaries — including
the beta zips testers get. (**Superseded 2026-08-06 by ADR-0021**: those staging copies are gone
and both files are now version-named release-page assets, `Anabasis-<version>-NOTICE.txt` and
`Anabasis-<version>-THIRD_PARTY_LICENSES.md`. The zips were never the whole story — the `.pkg`
and Inno routes carried nothing — so the release page replaces them as the one carrier every
route passes through. This paragraph records the 2026-08-05 round, not what ships.) The owner-legal half (EULA / PRIVACY / TRADEMARKS) is deliberately
NOT produced: no draft exists here (unlike the sibling), the wording is owner-supplied (C8), and
OQ-002 gates the whole commercial question. Coverage's legal row carries the split.

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

**Review round 47 (2026-08-03) — two lifecycle orderings made structural, no behaviour change.**
(1) `macroEngine->startDraining()` was armed several statements before the processor constructor
finished — before `addListener(this)` and the managed-parameter registrations — so a tick reaching
`handleAsyncUpdate()` could have landed a mapping pass the wrapper had not yet subscribed to.
Nothing can deliver such a tick (Timer callbacks come from the message loop), which is exactly the
ordering argument the startDraining/stopDraining split exists to retire; arming is now the
constructor's last statement. (2) `animVBlank` was constructed in the member-initialiser list,
before the `lastFrameTime`/`uiAnimOn` its callback reads and before `animated` was filled; it is now
assigned at the end of the constructor, matching the destructor's existing "clear it first"
discipline. (3) `ValueBox` recorded `downProp` under one predicate and consumed it under another; a
`downArmed` flag makes them one (latent, untested by design — the divergent case is unreachable).
(4) The MODE policy, THREAD_MODEL's retained row and KI-003 now state that the frozen-mirror thread
crossing was REDUCED to its pre-round-40 shape, not eliminated: `adoptFrozenMirror()` is a single
writer but is still reached from `setStateInformation`. Suites: 230 + 307 (unchanged).

**Review round 48 (2026-08-03) — one verification, one convergence, one guard.** (1) The frozen-trim
ownership boundary in `adoptFrozenMirror()` was verified and KEPT: mirror-write-then-read can only
place the boundary too late (conservative — the new slot withholds a latch briefly), while reading
first would place it too early and let the incoming slot serialise a vector measured while the
outgoing slot was live, re-opening round 42's leak. No test catches a swap (the case needs a
concurrent publication between two statements); the ordering and the consequence of inverting it are
now stated at the site. (2) An illegal persisted `uiScale` was clamped on read but never normalised,
so `getStateInformation` re-serialised it for ever; it now converges where the scale is applied,
never on the display poll, and a legal value is untouched. (3) `drainDetachBitsSoon()` reached
`handleAsyncUpdate()` without consulting `isRestoring()`, so the restore suppression rested on
reachability rather than structure; guarded, with no behaviour change for any existing caller.
Suites: 230 + 309.

**Review round 49 (2026-08-03) — one display defect, two duplicated numbers, three comments.**
(1) A re-prepare left the OLD spectrum analysis on screen against the NEW rate's bin mapping: the
ring rewind made stale frames unreachable, but the reader keeps its own EMA and `analyse` returns
early on an empty read, so the previous trace kept being drawn. A write count that went backwards
now clears it — the lifecycle edge only; the idle-decay question (KI-007 item 6) is untouched.
`testARewoundSpectrumRingDropsThePreviousTrace`, two mutants. (2) The per-stage performance figures
were published twice and `TEST_REPORT.md` held the pre-correction copy; removed in favour of the
authority document, and its stale "not yet measured: CPU/performance" line corrected. `CLAUDE.md`
and HANDOVER's present-tense rows no longer restate the pluginval strictness that lives in
`build.yml`. (3) `CurveView`'s cache comment, the spectrum publication's assumption list and the
macro mapping's cost claim were each corrected to what the code does — the last by making
`isDetached` take a `StringRef`, removing nine heap allocations per mapping pass so the "nine
comparisons" claim is now true. KI-008 gained an exposure note (the editor's ~3 Hz dirty poll makes
the message thread a continuous APVTS-lock acquirer). Suites: 230 + 312.

**Review round 50 (2026-08-03) — four fixes, one assessment.** (1) The UI-scale combo's item labels
were a second copy of `kScaleSteps`; built from the ladder now, and the test strengthened to compare
each label against the transform that item renders (an index round-trip passes even with a wrong
label, because the clamp hides it). (2) A preset file was parsed twice — once for the readability
gate, once by the apply — so a file rewritten between them passed the gate and applied different
content; `applyPreset` gained a parsed-document overload and the gate now shares it. (3)
`requestMeterReset()` announced before it published, so a block completing in the gap had its fresh
readings overwritten with silence; publish-then-announce, with release/acquire on the flag. Not
testable single-threaded, recorded at the site. (4) A factory apply wrote every overridden parameter
twice (default, then intent); it computes the final value and writes once, and no longer notifies
the host for a value that does not move. (5) The parented preset menu was assessed and left: the
overlays are full-bounds, always-on-top and intercept clicks, so the menu cannot open behind one,
and a child inherits the editor transform so the logical space is 720 px at every scale.
Suites: 230 + 318.

**Review round 51 (2026-08-04) — five fixes, two investigations that closed without a code change
to their subject.** (1) The badge table in `PluginEditor::paint` was declared `[kCount]` with a
comment promising a compile error on mismatch; aggregate initialisation with FEWER initialisers is
legal and value-initialises the rest, so raising `kCount` would have produced a null `juce::Slider*`
dereference at paint time. It deduces its bound and `static_assert`s against `kCount` now, and
`managed_params::kCount` itself is asserted equal to `std::size(ids)` at its declaration — the
hand-written count and the deduced list could previously disagree in either direction. (2) The
meter-target checkboxes wrote `iid::meterTargets` from `onStateChange`, which JUCE fires on every
button-state transition including hover and press; a hover inside the ~42 ms before the next re-seed
stamped the widget's bit over a freshly loaded mask. They compute the wanted mask and write only
when it differs, so a non-toggle transition is a no-op; the load direction is unaffected
(`dontSendNotification`). (3) `CurveView`'s paint cache stamped geometry with `shownFingerprint` —
a member only `refresh()` advances — while building the path from the CURRENT values, so a repaint
no refresh preceded (host expose, `resized()`) labelled the cache with a state it was not built
from. One `readInputs()` now reads and fingerprints in a single pass, and `paint()` both compares
and stamps with its own read, then builds from those very values: the label is exact by
construction rather than eventually. (4) The dirty marker compared SLOT trees, which carry the view
tier, `freeze`, the exact-`raw` attribute, `BASELINE` and `FROZEN_TRIMS` — none of which a preset
file stores — so resizing the window, switching panels, toggling Freeze or a mid-step raw move lit
"edited" on a preset whose file would have been byte-identical. `presetShapeFromLive()` projects the
live state onto exactly `savePreset`'s content, through a `presetValueOf` now shared by the writer
and the projection. That closes KI-007 item 5, and closes the KI-008 "editor is a continuous
acquirer of M1" exposure as a side effect: the ~3 Hz poll no longer reaches `apvts.copyState()` or
any wrapper `ValueTree`. (5) `applyPresetFile` left the previous state's `BASELINE` standing while
`applyFactoryPreset` cleared it, so a stale macro baseline travelled into A/B and the session save;
the two paths are the same operation and now leave the same state. Investigated and deliberately
NOT redesigned: the MacroEngine teardown window (on the message thread `stopTimer()` +
`cancelPendingUpdate()` shut it completely — the residual is exactly the off-message-thread host of
KI-003, now made checkable with a `jassert`, not a spin-join, which would deadlock against a message
thread waiting on the caller) and the frozen-trim mirror boundary (writer and readers unchanged;
what changed is that the display poll is no longer one of the readers — closing it needs the
thread-model decision, an Architecture Review Gate item). Three mutants, each killed.
Suites: 230 + 337.

**Review round 52 (2026-08-04) — three fixes, one investigation that confirmed the existing
behaviour and pinned it.** (1) The persisted UI scale only converged when the DISPLAYED step
changed: `refreshInternalSettingsBoxes` reached `applyUiScale()` — which owned the write-back —
solely on the branch where `nearestScaleIndex(stored)` differed from the combo's selection, so a
session carrying 92 while the box already showed 90 % clamped on every read and healed on none, and
`getStateInformation` re-serialised the illegal percent for ever. The normalisation moved out of
`applyUiScale` into `normalisedUiScale()`, which every reader of `iid::uiScale` now goes through, so
convergence follows from reading the value rather than from a branch. Deliberately not "call
`applyUiScale()` unconditionally": that path also calls `glContext.triggerRepaint()`, which is not a
no-op, and the re-seed is the 24 Hz poll. (2) `AdaptiveEngine::injectTrims` clamped with
`juce::jlimit`, whose two comparisons are both false for a NaN — so ADR-0014's "clamped at the
boundary … holds against hostile state" did not hold: a non-finite `FROZEN_TRIMS` property entered
the trim vector, was published (so the wrapper could serialise it back out) and reached `std::pow`
in the release mapping. `sanitiseState()` caught it a block later, which is a bounded recovery, not
the stated contract. Fields are finite-checked before clamping, per field rather than whole-struct,
each rejected field taking its `reset()` seed. (3) The dirty-marker projection and the preset writer
applied the same two rules but walked different collections; `PresetManager::forEachPresetParameter`
is now the single traversal, visiting in id order so existing preset files keep their byte layout
(`getParameters()` is registration order — the tree's was id order). (4) Investigated and
deliberately unchanged: a preset apply does not clear the frozen latch. `freeze` is preset-excluded,
so the apply never changes whether the slot is frozen; the engine's latch is untouched and is still
what the DSP applies; and `liveFrozenTrims` is only the fallback for the staged-but-unapplied
window, where clearing it would make a save report "no latch" while a vector was staged and about to
land. That is the opposite of the BASELINE rule for a reason — BASELINE is derived from the
parameter surface the apply replaces — and it is now pinned by a test rather than left as a
question. Four mutants, each killed. Suites: 233 + 351.

**Review round 53 (2026-08-04) — cleanup pass; no intended behaviour changed.** (1) The README's
quick-start still passed `5` to both `run-pluginval.sh` invocations after the gate was raised to 10,
so the first document a contributor opens told them to validate at a weaker bar than the build
enforces — the one site rounds 35/40/43 missed when they made `build.yml` the single authority. It
now reads the value out with the same `sed` extraction `CI_CD.md` documents, and the validation-gate
table points at the workflow instead of restating the ladder. (2) The README said "fourteen ADRs" in
its status section and "twelve Accepted ADRs: ADR-0001…0011 plus ADR-0012" in its Documentation
section. The enumeration is gone and both numerals with it; `ADR_INDEX.md` is the registry, and the
README carries no count for the same reason it carries no strictness number. (3) The Save-Preset name
field's focus glow was WIRED rather than deleted: the LookAndFeel branch keys on a `"glow"` property
nothing set, but its fallback comment ("value boxes unchanged") shows it discriminates between two
kinds of text field in THIS editor, which is a design statement about Anabasis rather than migrated
state. (4) `rawEditText`'s `"unit" == "bal"` branch was REMOVED: it decodes the sibling product's
"L 25"/"C"/"R 30" Balance display, while `colourBalance` here registers a plain signed decimal the
generic path already handles. The `"unit"` read had no other reader and went with it.
(5) `CompactComboLookAndFeel` and `SimpleComboLookAndFeel` removed — their own comments named
Anamorph controls (Input Channel / M/S Solo, Simple-mode Widen) that do not exist in this product,
and nothing instantiated them. (6) `hasPublishedTrims()`/`pubTrimEver` KEPT with the reservation made
explicit in code: no production reader today, but it is the published set's validity marker (the
question a §6.3 trim readout must ask, since all-zeros is ambiguous) and the only observation of the
published/retained split KI-006's two halves rest on. A contradicting comment claiming the P5 UI
already reads `publishedTrim*()` was corrected. One new test, one mutant. Suites: 233 + 354.

**Review round 54 (2026-08-04) — the single-authority sweep finished, and one reset invariant
raised.** (1) Three documents still carried a stale ADR count or roster after round 53 fixed the
README: `CLAUDE.md` ("the **fourteen** Accepted ADRs"), `REPOSITORY_MAP.md` (twice) and
`SOURCE_OF_TRUTH.md` ("ADR-0001…0011 are Accepted"). Three of the four statements were already
wrong — stale through ADR-0012/0013/0014 — and all now name `ADR_INDEX.md` instead of a number.
`SOURCE_OF_TRUTH.md` names the authority LEVEL rather than its membership, which is what makes it
stale-proof. `CHANGELOG.md`'s preset count was left: a changelog entry records what a released
version did. (2) `SpectrumView` inferred a ring rewind from `writeCount()` going backwards, a
predicate that holds only while the observed count is still below the reader's last one — one
delayed tick and the producer republishes past it, after which the reset is missed permanently and
the previous lifecycle's EMA is drawn against the new rate's bin mapping, silently, for the rest of
the session. `ScopeBuffer` now carries a reset generation, bumped release-after the index rewind,
and the view samples it on both sides of its analysis batch — the same reader contract
`GrHistoryBuffer::resetEpoch()` states. A plain generation rather than that class's odd/even
seqlock: `ScopeBuffer::reset()` writes one atomic and touches no sample, so there is nothing for a
reader to observe half-done. No new synchroniser class (the generation-counter row is already
permitted), no audio-thread cost beyond the existing publication. The test gained the
count-never-dips case; one mutant, killed. Suites: 233 + 357.

**Review round 55 (2026-08-05) — the component-ID half of the LookAndFeel migration audit.** Round 53
cleared the property side; four `getComponentID()` branches still had no arming site. The sibling
product settles each, being where all four are armed: `"apply"` styles its "Apply Gain" button,
`"metersicon"` its show/hide level-meters toggle, `"vtoggle"` its Mono / Swap / M-S / polarity row —
none of which exist here (no auto-gain match, the §6.3 metering strip is always present, no
stereo-field controls), so all three are removed. `"icon"` is the opposite case and is now WIRED: its
header names its owner ("Undo/Redo glyphs"), this product has undo/redo in the same top bar, and
unarmed they rendered a ~13 px character in a 30 px glyph button — the absence of the sizing the
branch exists to provide, not a stylistic difference. Flagged for the brand pass rather than decided:
the treatment also rotates 180°, specified against the sibling's U+21BA/U+21BB circle arrows where
that is subtle, while these are U+21B6/U+21B7 semicircle arrows where it visibly moves the arc from
top to bottom (direction is safe — rotation preserves the curl). Test pins both arming sites; one
mutant, killed. Suites: 233 + 359.

**Review round 56 (2026-08-05) — one contract made explicit, one aliasing hazard removed; no
behaviour changed.** (1) `injectTrims` publishes with `meaningful = true`, so an ADR-0014 restore
advances `retTrimSeq` exactly as an audible `finishBlock` does — and that is load-bearing for slot
ownership, not incidental. With Freeze ON `finishBlock` publishes nothing, so after
`adoptFrozenMirror` re-bases `slotFrozenBase` the injection is the ONLY event that can carry the
generation past it and let the incoming slot answer for its own latch; publish without counting and
a freeze-ON slot restored from disk withholds its latch from every save until the next audible block
— never, with the transport stopped. Stated at the injection site and in `THREAD_MODEL`'s
retained-trim row as the property a future change must preserve. It was also **untested where it
matters**: a `publishTrims(false)` mutant left the state suite green (only the DSP suite's
`hostileTrims` caught it, incidentally), so `testFrozenTrimRestore` case (1) now asserts the
generation advances across the landing block and that `hasRetainedTrims()` follows — the value checks
could not see it, because mirror and engine hold the same vector at that moment. (2) Three
`juce::ValueTree` assignments shared the refcounted node between logically independent owners
(`copySlotToOther`'s `storedPresetBaseline`, `undo()`/`redo()`'s `presetBaseline`); all three gained
`createCopy()`. Correct before — a baseline is only ever replaced wholesale, never edited in place —
and a trap the moment one is not. Not tested: unobservable through the public API without adding an
accessor purely for the test. Two mutants, killed. Suites: 233 + 362.

**Review round 57 (2026-08-05) — the third preset walk converted; no behaviour change.**
`PresetManager::applyFactoryPreset`'s defaults pass still iterated `apvts.state`'s PARAM children
while `savePreset` and `presetShapeFromLive` shared `forEachPresetParameter` over
`getParameters()`. The round-52 argument applies here with more force than it did to the other two:
"one tree child per parameter" is a fact about JUCE, not an invariant of this code, and a parameter
the tree walk missed would keep the value the PREVIOUS preset left it at — the blend-two-presets
failure the defaults pass exists to prevent, and audible rather than cosmetic. The shared walk now
yields `(id, RangedAudioParameter&)` instead of `(id, double)`, since the factory apply needs the
parameter's DEFAULT and then writes to it while the other two ask `presetValueOf` — handing over a
value would have left the third caller unable to use the function at all, which is why it never
did. Write order, values, exclusions, ceiling-lock skip and host-notification behaviour are all
unchanged: the sorted-by-id order the walk visits IS the tree order the loop had (round 54 measured
it, and `written == fromTree` pins it). The ceiling lock stays at the call site — a locked ceiling
is never written by a preset but is still saved and still compared, so it is an apply-side rule,
not a member of the set. It also retires a caveat: the old loop wrote the APVTS tree while
iterating it, safe only because a property write adds and removes no children; the shared walk
touches no `ValueTree` at all. The parity test gained the invariant as idempotence against
arbitrary prior state — park every non-excluded parameter at the far end of its range, re-apply the
same preset, require every one back — which needs no second copy of the table logic. One mutant
(a parameter the pass skips), killed by that check and by `factory:`'s existing default check.
Suites: 233 + 365.

**Review round 58 (2026-08-05) — two accuracy fixes; no behaviour changed.** (1) The `injectTrims`
comment described the wrong execution order. It claimed `sanitiseState()` catches a poisoned trim "a
block LATER", on the premise that the injection sites run after that block's sanitise. They do not:
both — the unprimed direct adopt and the §2.8 duck bottom — run inside `AnabasisEngine::process`
BEFORE its `adaptiveEngine.sanitiseState()` call, and the first `currentTrims()` read comes after
it, in the same call. So on the AUDIO path the existing recovery is already complete and nothing
consumes a poisoned vector. The comment now says that, and states the exposure the finite check
actually closes, which the old prose missed entirely: `publishTrims (true)` writes the published and
retained atomics AT INJECTION TIME, `sanitiseState` repairs the plain struct and never re-publishes,
and nothing else will — an ADR-0014 restore is staged only for a freeze-ON surface and `finishBlock`
holds while frozen — so a NaN would sit in the wrapper-visible atomics indefinitely and
`engineFrozenTrimsIfLive()` would serialise it into the saved session. That outlives the block, the
session and the file, and no per-block sanitiser could have closed it. Validation logic, ordering and
DSP behaviour are untouched. (2) `AnabasisLookAndFeel` regained
`JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR`, lost when the class was rewritten for Anabasis and
leaving it the one member of the new GUI set that was silently copyable and untracked (`CurveView`,
`GrHistoryView`, `SpectrumView`, `LoudnessMeterView`, `FrameClock` and the editor all carry it). A
Debug build with the leak detector active runs the state suite green, so nothing copies or leaks one.
Suites: 233 + 365 (unchanged — neither fix is a behaviour a test can observe).

**Review round 59 (2026-08-05) — one UI interaction fix.** `SpectrumView` left
`setInterceptsMouseClicks` at JUCE's default, so it hit-tested true across its whole area while
acting only on clicks in the top-right × — every other click inside the overlay was consumed with no
affordance and no effect, the one region of the editor that took a click and did nothing.
`GrHistoryView`/`CurveView` opt out wholesale with `setInterceptsMouseClicks (false, false)`; this
view cannot, because it owns the dismiss ×, so it opts out PER-PIXEL through a `hitTest` override —
the JUCE-idiomatic answer for a partly-interactive overlay. `LoudnessMeterView` stays intercepting
because its whole surface IS the affordance. The hit-area moved into one `dismissHitArea()` that
`hitTest` and `mouseDown` share: two separately computed rectangles could accept a click and then
ignore it, which is the same swallowing one pixel at a time. The rectangle is exactly the set of
in-bounds points the old predicate matched, so the touch target is unchanged, and `paint` is
untouched. ONE VISIBLE CONSEQUENCE, inseparable from the fix rather than added to it: hit-testing is
also what makes a component "under the mouse", so the `setTooltip ("Spectrum")` identifier now
appears over the × only, not the whole trace — there is no way to stop claiming clicks in a region
while still claiming the pointer there. That wording is a C8 owner TODO, so the scoping is a
brand-pass call if it is wanted back. `testTheSpectrumOverlayOnlyClaimsItsDismissCorner` checks both
halves (a click over the trace is not claimed; the × still dismisses); one mutant restoring the JUCE
default, killed. Suites: 233 + 370.

**Review round 60 (2026-08-05) — About-panel regression: the overlay opened blank.**
`Backdrop::aboutText` had exactly ONE reader — the dismiss-anywhere branch in `mouseDown` — so the
flag named a panel whose copy nothing painted: clicking the wordmark opened an empty glass rectangle
with only the hyperlink, and `ANABASIS_VERSION_STRING`/`ANABASIS_BUILD_NUMBER` had no consumer
anywhere in `src/` while `CMakeLists.txt` still defined them for two targets and `CI_CD.md` still
described the run number as "the About-box build number". `Backdrop::paint` now renders the copy —
wordmark, the top bar's own `MASTERING MAXIMIZER` subtitle, `Version <x>   build <n>`, `RollyTech`
and the copyright line — inside the 176 px `resized()` had already reserved above the link, so the
layout, the link and the dismissal are untouched. Deliberately NO prose description, which is where
this departs from the sibling's About: free prose is owner-supplied wording (C8) and this file's own
header says it is not invented in code, so the panel identifies the build completely and the
description is a one-line addition when the copy lands. `testTheAboutPanelShowsTheBuildItIsRunning`
opens it through the real path (the wordmark's ghost button) and asserts the copy area shows
HORIZONTAL variation — an empty panel is a vertical glass gradient, so every row is near-constant
across x, which makes the check font-independent rather than a pixel count tuned to one machine's
font. One mutant restoring the blank paint, killed. Suites: 233 + 374.

**Review round 61 (2026-08-05) — preset source tracking: saving did not record its own source.**
*(Superseded 2026-08-08 by ADR-0022 — the editor-local hint this round patched, and the "editor
state, not processor state" placement argument below, were both replaced by the wrapper-held
preset identity: `savePresetFile` now sets the identity itself, `rememberPresetSource` no longer
exists, and the test named here passes against the identity. The record is left standing rather
than rewritten.)*
Every route that changes the live preset tells the editor where it came from — the menu, the ‹ ›
ring, the load chooser — because `stepPreset` cannot recover that from the display NAME: names are
not unique across the factory table and the user files, which is the reason the remembered source
exists. The SAVE button was the one route that did not. Saving over a factory name (apply
"EDM Club", Save Preset as "EDM Club") therefore left the factory hint standing, and the unchanged
name CONFIRMED it, so the arrows walked the factory ring from an entry the user had just replaced —
silently changing the sound. `saveOkButton.onClick` now calls `rememberPresetSource (file)` on a
successful save. It is editor state, not processor state, and stays in the editor: the remembered
source is this window's idea of where it is in the list, and the wrapper deliberately holds no view
of it (its job is the name and the dirty datum, which it already did). No name matching was added
and no other preset path changed. `testSavingOverAFactoryNameKeepsTheArrowsOnTheUserPreset` drives
the real buttons and distinguishes the two same-named presets by CONTENT — the saved one carries a
parameter the factory table does not name, so a factory apply parks it at its default — then checks
that next-then-previous, which is exactly reversible through the ring, returns to the SAVED preset.
It is the one test that writes into the real user preset directory (the save handler and
`stepPreset` both resolve it themselves and neither takes an injected path) and it restores whatever
it displaces. One mutant dropping the call, killed. Suites: 233 + 382.

**Review round 62 (2026-08-05) — two maintainability items; no runtime behaviour changed.**
(1) The three visualisers with a `FrameClock` — `SpectrumView`, `GrHistoryView`,
`LoudnessMeterView` — declared `clock` BEFORE the state their ticks read, and used
`~View() = default`, so reverse-order destruction freed that state while the vblank attachment was
still armed over it. No defect is reachable (a message-thread destructor cannot interleave with a
message-thread vblank callback) but it is exactly the "safe by declaration order" argument
`~AnabasisAudioProcessorEditor` refuses for its own `animVBlank`, which it detaches explicitly and
first. Each destructor now calls `clock.stop()` — `FrameClock::stop()` clears the attachment and the
callback and is idempotent — so the guarantee is stated rather than inherited from member order, and
a future reorder cannot quietly remove it. `CurveView` needs nothing: it has no `FrameClock` and is
driven by the editor timer. (2) `PERFORMANCE_BUDGET.md` described the measurements as "Release with
the shipped flag set", which is not exact: `AnabasisBench` links `AnabasisHardening` +
`juce_recommended_config_flags` + `juce_recommended_warning_flags` (the same set the two test apps
use) while the `Anabasis` plugin target links **`juce_recommended_lto_flags`** as well, and
`AnabasisHardening` additionally adds Release debug info (`-g`, or `/Zi` + `/DEBUG` on MSVC). A
build-configuration note now states the difference, why it is not closed by changing the target (that
would change the results, not the documentation, and force a re-measure under C2), and how to read
the figures: the DSP is header-only and already instantiated in the bench's single translation unit,
so most of what LTO buys is available to the optimiser anyway — but the residual gap is UNMEASURED,
so these are the bench target's numbers, not the plugin binary's. `TEST_REPORT.md`'s summary points
at that note rather than repeating it. No methodology, build configuration or measured value changed.
Suites: 233 + 382 (unchanged — neither item is a behaviour a test can observe).

**Review round 63 (2026-08-05) — one state-ownership move, one truthfulness guard, one invariant
documented.** (1) `normalisedUiScale()` wrote `iid::uiScale` back to `InternalState` when the
persisted percent was not a legal ladder step, and it is reached from the 24 Hz settings re-seed —
so a DISPLAY TIMER was a writer of a wrapper `ValueTree`, opposite `InternalState::replaceFrom`,
which `setStateInformation` reaches on whatever thread the host chose (KI-003). The correction moved
to `replaceFrom`, where every other field's §4.4 read rule already lives and where an illegal value
actually enters (a hand-edited session, or one from a build whose ladder has changed). To do that
without a second copy of the ladder, `kScaleSteps`/`nearestScaleIndex` MOVED to `InternalState.h`
beside the identifier whose legal values they define — the editor now aliases them, and
`normalisedUiScale()` is a pure read that still clamps so the rendered transform and the displayed
step stay one decision. Both editor polls are now read-only with respect to the wrapper's trees.
(2) `showPresetMenu` and `showLoadPreset` recorded the preset source regardless of whether
`applyPresetFile` succeeded, so a corrupt or foreign file left the editor believing it was the
active source while the processor had not moved; both now gate on the return value. The ‹ › ring is
deliberately NOT gated — advancing its hint past an unreadable file is what stops the arrows
stalling on it. (3) The factory defaults pass writes `getDefaultValue()` unsnapped while overrides
are snapped; the asymmetry is now stated as an invariant (a registered default must already be
legal — it is what the plugin reports before anything writes it — whereas an override is
hand-written table data free to be an approximate intent). Behaviour unchanged. Two mutants, killed;
the UI-scale test's convergence checks were rewritten to drive a session load, which is where
convergence now happens. Suites: 233 + 386.

**Review round 64 (2026-08-05) — two robustness/documentation follow-ups from round 63.** (1) The
`iid::uiScale` normalisation in `replaceFrom` read the property with no stated default, which is
total only because `setDefaults()` two lines above always writes it. That is "correct because of
what the line above did", and here it would fail QUIETLY: an absent property reads as `var()`, which
converts to 0, and 0's nearest ladder step is **80** — so a missing field would become the smallest
legal scale rather than the default one. The read names `ui_scale::defaultPercent` now, and that
constant is the same one `setDefaults()` writes (it was a literal `100` at both sites) with a
`static_assert` that it is itself a legal step. `testAnOutOfListUiScaleClampsConsistently` gained the
§4.4 case directly: a session whose `ANABASIS_INTERNAL` omits the field must load at 100, not 80.
Two-stage mutation measured which line does the work — with `setDefaults()`'s write removed the
check still passes (the fallback carries it), with the fallback ALSO removed it fails at exactly the
80 % the review predicted. (2) `SpectrumView::hitTest`'s comment named one consequence of declining
the trace region (the tooltip narrowing) and not the other: clicks over the trace now reach whatever
is beneath, which today is the editor — the correct outcome and the point of the change, but a live
routing decision rather than a void. Both are now stated as the same trade seen from opposite ends,
with the guidance a future affordance under that strip needs: widen the hit-area, do not revert to
intercepting everywhere, which would restore the swallow round 59 removed. Suites: 233 + 388.

## 0.1.1 (2026-08-06/07) — the release round: sixteen owner items, four ADRs, the tag pipeline

The owner's 0.1.1 directive is the largest single round in this repository's history and the one
that turns a code-complete tree into a release. Four Architecture Review Gate items were cleared
under the round's standing instruction (anything needing human confirmation is confirmed by the
owner directly; anything needing an ADR gets one immediately rather than a hard stop), and each
is recorded in its own ADR's Status banner rather than in this file.

**KI-009 closed — the left channel, root-caused on the third attempt.** Two prior rounds probed
the engine and the wrapper and found nothing, because there was nothing there: a fresh
line-by-line diff of the bus/CMake/format glue against the sibling, plus a re-audit of every
channel-asymmetric expression in `AnabasisEngine` and `LookaheadLimiter`, established that the
audible path is provably symmetric (every `ch == 0` asymmetry is an analysis tap). The defect was
in the **bus contract**: `isBusesLayoutSupported` demanded stereo→stereo exactly, so a host with
a mono source had to negotiate stereo→stereo and feed the programme on one input pin and silence
on the other — and this chain is strictly dual-mono (the stereo "link" shares only the detector
level), so the silent pin produced a silent output channel in both modes. Mono in is now accepted
and duplicated. The battery gained the seventh case KI-009 itself predicted it would.

**Four ADRs, each clearing its own gate.** **ADR-0018** — Copy becomes an undo step on the
DESTINATION slot that keeps that slot's earlier history, and the Advanced toggle joins the undo
history while staying pinned across A/B (partially superseding ADR-0010 option E: its A/B half
survives, its undo half does not). The same change removes a class of dead undo step — a bypass
or monitor click used to mint an entry whose restore changed nothing. **ADR-0019** — the comp's
stereo link becomes adjustable, the 50th parameter, blended before the RMS integrator so the
100 % default is bit-for-bit the glue that shipped. **ADR-0020** — the Waveform Statistics panel:
a new 50 ms Hann `RmsMeter` (cost bounded at 5 multiply-adds per sample at *every* rate and block
size), LRA and the BS.1770-1 ungated integrated reading in `LoudnessMeter`, the sample-peak hold
published, `int_tpMeterOn` removed and two standard selectors added. **ADR-0021** — the packaging
and release pipeline, lifting the OQ-007 deferral for everything but signing.

**What the round did NOT change, verified rather than assumed.** The UI-scale × host-DPI compose
(item 10) was audited against the sibling and found **already complete**: `setScaleFactor` stashes
`hostScale` and re-applies the composed transform, which is the whole of the sibling's mechanism;
Anabasis's only divergence is reading the user step from the persisted percent through
`normalisedUiScale()` instead of a combo index, which is the stricter of the two. No code changed
and none was owed — recorded here so the next audit does not re-derive it.

**Two brief-audit gaps closed alongside the directive's own items**, found by walking
`DEVELOPMENT_BRIEF.md` against the tree. (1) **§8 keyboard operability** had no implementation:
accessibility NAMES were done (`setTitle`/`setDescription` on every control) but nothing in
`src/` called `setWantsKeyboardFocus`. **The substance is the forty KNOBS**, and the reason is a
JUCE default that differs by widget class — read from the vendored source rather than assumed:
`Slider` ends its constructor with `setWantsKeyboardFocus (false)`, while `Button` sets it true
unconditionally and `ComboBox` sets `! isLabelEditable`. So tab traversal was **not** dead, as
this entry claimed for one commit: it reached every button and combo and skipped every knob,
and `Slider::keyPressed`'s arrow handling — which JUCE already implements — was unreachable on
all of them. The calls added to the combo and toggle helpers are therefore redundant against the
default, kept deliberately and labelled as such. Every control now ACCEPTS focus — and only
accepts: `EDITOR_WANTS_KEYBOARD_FOCUS` stays FALSE, so the plugin still never takes the host's
transport keys. Swept by the tooltip test's sibling collector; mutation-verified, and the same
mutation run is what showed the combo half to be inert. (2) **§14.2's `SUPPORT.md`**
was a named member of the Internal/testing documentation class that did not exist. It exists
now, and is deliberately shorter than the sibling's: that class restates the legal class, and
Anabasis has no approved licence, EULA or privacy document to restate — the file says so rather
than inventing terms or a support contact.

**A four-lens migration audit closed the directive's item 5**, sweeping for other places where
the sibling port was left half-done: GUI widget setup, wrapper/state plumbing, DSP module
completeness, and doc claims about the code. 38 candidate findings, each then handed to an
adversarial verifier told to REFUTE it — **17 were refuted, 21 survived**, and the refutations
are as load-bearing as the confirmations (several named a deliberate ADR-recorded divergence the
finder had read as an omission). Fixed in this round: the Save-Preset field's Return/Escape keys
(inert — the sibling's two `onReturnKey`/`onEscapeKey` lines were never ported, so the dialog
could only be left with the mouse) and its missing palette; five hand-built buttons that bypassed
`registerAnimated`; the `int_integratedStd`/`int_rmsRef` read rule, whose absence let the
statistics panel *show* one standard while *computing* the other; and seven doc-drift sites
including `SOURCE_OF_TRUTH.md`, which had told every contributor since P0 that `src/` and
`tests/` did not exist and that every runtime claim must be marked `Unverified`.

**One audit finding was disproved by its own fix**, and the correction matters more than the
finding: "the Settings panel is unreachable by keyboard" is false. Mutation-testing the fix
showed removing the combo call changes nothing, and JUCE's vendored source says why —
`ComboBox` sets `setWantsKeyboardFocus (! isLabelEditable)` and `Button` sets it true
unconditionally, while `Slider` sets it **false**. So §8's real gap was the forty KNOBS alone,
tab traversal was never dead, and this file's own first draft of that paragraph was wrong. It is
corrected above rather than quietly amended.

**Two findings were recorded rather than fixed** (`KNOWN_ISSUES.md` KI-010, KI-011). KI-010 is
the sharpest: ADR-0004's §Consequences argues the constant-latency contract makes every bulk swap
"always dry-fillable" and that "the forced duck keeps its best masking mode" — and the duck never
dry-fills. It applies `duckGain` to the processed path only, so every preset load, A/B switch and
undo step dips to silence. No invariant is broken (invariant 8 asks for click-free, and a
raised-cosine dip is click-free; the duck tests pass and cannot tell the two modes apart), so
this is a documentation-vs-code contradiction, not a defect in either alone. Implementing
dry-fill is an audible change to every bulk swap, arriving on the day of a release round, on the
one path no listening pass has covered — the entry states both ways out and deliberately does
not choose.

**Mutation verification** was applied to the two constants most likely to be quietly wrong: the
LRA relative gate (−20 LU, *not* the integrated reading's −10) and the LRA reset watermark (30
sub-blocks, the short-term window's span, *not* the integrated one's 4). Each mutant fails
exactly its own assertion and no other.

**Two post-directive review rounds landed on top of the batch.** Round 1 closed the one place
ADR-0018's view pin did not reach — the Copy undo entry is `storedSlot`, frozen by the A/B
switch, so an ADV toggle taken after that switch left the entry carrying a pre-toggle view that
undoing the Copy then wrote back and resized the editor; the pin moved to PUSH time
(`slotWithLiveAdvancedMode`, recorded as that ADR's amendment) — and corrected five packaging
documents still promising `NOTICE` / `THIRD_PARTY_LICENSES.md` inside the archives after
ADR-0021 made the release page their sole carrier. Round 2 fixed the release-notes extractor,
whose `^## \[` terminator ran the NEWEST entry to end of file and read fenced text as structure
(ADR-0021's amendment; the 0.1.1 notes are byte-identical either way), and separated the RMS
meter's SENTINEL from its FLOOR. `kSilentDb` meant "nothing measured yet" while the computed
range, floored on the mean square at 1e-15, reached −150 dB *beneath* it — so a real reading of
a near-silent passage was indistinguishable from an absent one, and exact digital silence was
reported as no measurement at all. `kFloorDb` (−140) is now the lowest LEVEL, every computed
reading is clamped up to it, silence included, and only a non-finite accumulation still reverts
to the sentinel; `LoudnessMeterView`'s AES-17 guard became the exact `>= kFloorDb` test in place
of a tolerance around the sentinel. No audio path, parameter, serialized field or displayed
value moves — the statistics rows floor their text at −99, above both constants.

**Round 3** corrected two statistics rows and the last of the attribution-delivery drift. **PLR**
was published as `TP − I_gated` and printed verbatim, while the I row directly above it follows
§3.5's standard choice — so under BS.1770-1 the row was not the difference of the two rows it
sits under, by however far a trailing silence had driven the two standards apart. It is derived
in the view now (`plrFromShown`), reproducing the published figure exactly under the gated
default; `pubPlr` keeps publishing as the canonical gated figure and stays in the cleared set.
The **SP** row's warn compared in dB a bound `CeilingClamp` holds in LINEAR: `ceilingLinear` is
`dbToGain(ceiling)` and the row reads `gainToDecibels(|x|max)`, a round trip the parameter never
makes, so a fully limited master could read back −0.09999997 against a −0.1 dB ceiling and trip a
warning ADR-0020 promises means a genuine exceedance. The comparison gains a 0.005 dB tolerance —
half the row's own print resolution, four orders of magnitude above the round-trip error, below
anything the row can display. The TP row's exact test is untouched: its over-warning is the
deliberate, documented one. On the documentation side, three LIVE status statements still said CI
copies `NOTICE` and `THIRD_PARTY_LICENSES.md` into every customer artifact — `REPOSITORY_MAP.md`,
Coverage's legal row, and Coverage's `.github` row, which also still called `release.yml` deferred
to the first commercial release — while the two dated round entries that describe the old
delivery keep their text and gain supersession markers instead, because they record what those
rounds did. Finally the panel is **eight rows**, not the "seven readings" the manual, ADR-0020's
title and the ADR index all carried: seven is the count the owner's directive names, PLR being
the 0.1.0 row it never mentioned.

**Round 4** — one visible regression, one dead undo step, and a signed-off confirmation.

The **GR history graph** left a permanent blank strip down its left side. The 0.1.1 shimmer fix
gave buckets a fixed absolute identity — bucket k is the entry range [k·stride, (k+1)·stride), so
a completed bucket's decimated max never changes — but drew one bucket per pixel column anchored
at the newest. `stride` rounds up, so `cols` buckets span `cols·stride` entries while the window
only ever holds `want`; the surplus buckets were older than the window, drew nothing, and the
trace covered ≈ `want/stride` of the panel: ~31 % blank in the Simple well at 48 kHz/512, ~48 %
at 1024, ~22 % in the Advanced well. **Bucket count and pixel column are separate questions** —
the window still bounds the DATA, and the buckets it yields are now stretched across the full
width (`bucketX`), which is also what the pre-0.1.1 draw did while the ring was filling. The
reviewer's alternative, widening the window to `cols·stride`, fills the panel by showing more
TIME — 38.6 s at 48 kHz/1024, outside `kWindowSeconds` and DESIGN §2.9's 10–30 s band — so the
window length would have become a spare variable for making the arithmetic come out. Bucket
identity is untouched, so the shimmer fix stands: only a bucket's X moves, and the whole trace
moves with it, which is the scroll. `kHead` is keyed to entry `head - 1` rather than `head`, which
also removes the one case where the rightmost bucket came out empty and dropped a column. The
geometry moved into `buckets`/`bucketX` in the header for the reason `windowEntries` already
lives there — arithmetic reachable only from `paint` is arithmetic no test can pin, which is how
this shipped.

**Copy pushed a dead undo step when it changed nothing.** After the first Copy the destination
already holds the live state, so a second press with no edit between pushed an entry restoring
what it replaced — one Undo press that visibly does nothing, the same defect shape §Decision 4 of
ADR-0018 removed from the gesture path. It takes the same answer: that path's change test,
`strippedForUndoCompare` both sides plus the dirty datum, and the redo line left alone when the
test says nothing changed. Recorded as ADR-0018's second amendment.

**The cross-slot ADV undo is confirmed, not changed.** An entry taken in slot A carries the view
the user had in A; an ADV toggle made later in B does not update it, so undoing in A can return
the editor to Simple. Unlike the Copy entry, that entry IS contemporaneous with its own step, so
this is the contract working rather than the first amendment's defect. The owner reviewed it on
2026-08-07 and asked for no change; the reasoning and the cost of both alternatives are recorded
as ADR-0018's review-confirmation note so the next audit does not re-derive it as a bug.

**Round 5** — one blank-panel case, one enforced CHANGELOG rule, and one real audio-thread cost.

The **GR history graph drew nothing while the ring held a single bucket's worth of entries** —
the first few blocks after every reset and every transport start, ≈ 32 ms at 48 kHz/512 where
`stride` is 3. With one bucket there is no second vertex for the stretch to reach, so `bucketX`
returns the left edge, a one-point polyline strokes nothing and a one-point fill closes a
zero-width shape. The limit of the stretch as the bucket count falls to one is a CONSTANT trace,
so the single reading is emitted at BOTH edges — the same two x's the two-bucket case already
uses, and continuous with it. The rectangles the pre-0.1.1 draw used are deliberately not
restored: a filled path under a polyline is the design, and this is that design's own degenerate
case handled inside it.

**`check-docs.py` now enforces the CHANGELOG rule the release extractor depends on.** `release.yml`
ends a release's notes at the next h2, so the h2 level is reserved for version entries — and
nothing checked it: demoting an entry's sub-section from `###` to `## ` truncates that release's
notes where it stands, and appending a section to the foot of the file truncates the oldest
entry's, which today is 0.1.1, whose notes deliberately run to end of file. Neither failure is
visible until a tag is cut. The rule is "below the first version entry, every `## ` heading must
be a version entry"; older version headings are the mechanism working, not findings, and
`## [Unreleased]` sits above the first entry and is outside it. Both directions are in the
script's self-test, and both were driven against the real `CHANGELOG.md` before and after.

**The two histogram-walk loudness readings are cached.** `integratedLufs()` walks the 751-bin
histogram twice and `lraLu()` walks it three times, and the wrapper read BOTH once per
`processBlock`. The figure that decides it is the block RATE: 94 blocks/s at 48 kHz/512 is
invisible, but ~6000 blocks/s at 192 kHz with 32-sample buffers is ~22 M iterations/s — the same
order as DESIGN §9's entire ≤ 0.5 % metering allocation, and the shape ADR-0020 §Decision 1 had
already rejected once for `RmsMeter`. Both figures are pure functions of the session-cumulative
accumulators, which change in exactly two places, so each reading is held behind a validity flag
those two clear: the walks run at **10 Hz** and the value is **bit-identical**, not approximated.
Measurement, update timing and the public surface do not move — every existing loudness
assertion passes unedited, which is the evidence. `momentaryLufs`/`shortTermLufs`, the readings
the engine's §2.7 compensation calls from another site, are deliberately not cached.
ADR-0020's "one extra histogram walk per gating block" now describes the code rather than the
intent; recorded as that record's third amendment.

**Reviewed and intentionally unchanged (owner sign-off, 2026-08-07).** Eleven findings from the
same round were reviewed and accepted as they stand, and are recorded here so a later audit does
not re-derive them as defects: the **mono→stereo layout negotiation** (KI-009's fix; the
duplication covers the audio path and `getBypassParameter()` keeps JUCE off the default bypass
route); **LRA algorithm semantics** (the cost fix above changes none of them); the **Inno
uninstall icon and Start-menu group** on a VST3-only install; the **graph-switch hit area**, which
now tracks the drawn pill rather than the old corner rectangle; the **loudness binning
asymmetry** (`ceil` for the threshold bin, truncation for the insert bin — bounded by the 0.1 LU
bin width, and shared with the pre-existing integrated path); the **LRA reset-watermark
structure** (`lraFrom` cleared inside `clearSessionCumulative` then re-set by `resetIntegrated`);
the **`MasteringComp` GR meter scanning `kMaxChannels`** (unreachable while the engine is
prepared from `getTotalNumOutputChannels()`); the **About-panel and Settings-panel geometry
comments**; the **gesture-filter ordering** (safe on `juce::Array`'s bounds-checked accessor);
the **`release.yml` implementation assumptions** (brace expansion, per-artifact subdirectories,
the fail-closed `chmod`, the prefix-compare anchor); and the **`RmsMeter` sanitise hook** that
cannot currently fire.

**Round 6 — documentation only.** One finding, and it is about a constraint rather than a defect.
`LoudnessMeter::integratedLufs()`/`lraLu()` are `const` and, since round 5, write `mutable`
non-atomic members; the invariant that makes that safe — every caller on the audio thread — was
stated in the header and nowhere a maintainer would look before adding a reader. It is now
recorded in `THREAD_MODEL.md` §"Audio-thread-only state behind a `const` accessor", which is where
the file already keeps constraints that are not cross-thread edges (beside "Which context
paints"). The section names what makes it worth writing down: `AnabasisEngine::outputLoudness()`
hands out a public `const LoudnessMeter&`, and a `const` method that mutates advertises nothing,
so a GUI-side reader added through that reference compiles cleanly and races. It also states the
broader and older rule the cache did not create — **nothing** on `LoudnessMeter` is safe to read
from a second thread, since the sliding-window readings walk `subRing` while the audio thread
writes it; the two cached getters are the sharper case because they also WRITE. What a GUI-side
reader should use instead is named: the published `meterLufsI()`/`meterLra()` atomics. Adding a
second reader thread to those getters is a threading-model change and therefore an Architecture
Review Gate item. **The accessor is deliberately not narrowed**: its other consumer is the
engine's own §2.7 compensation reading `dryMeter`/`wetMeter` from inside `AnabasisEngine::process`
— the same thread — so there is no GUI-side use to remove. No implementation, no atomics, no API
change; ADR-0020's third amendment and the header comment now point at the record.

**Reviewed and intentionally unchanged (owner sign-off, 2026-08-07, round 6).** Six items from the
same round stand as implemented: the **CHANGELOG boundary validation** behaviour (the rule and its
seven self-test cases); the **test count arithmetic** (259 + 522 = 781, derived from the suites'
own output); the **GR history single-bucket rendering** (the reading emitted at both edges, which
is the stretch's own limit rather than a return to per-column rectangles); the **`LoudnessMeter`
cache invalidation coverage** (`finishSubBlock` and `clearSessionCumulative`, each mutation-killed
by its own assertions); the **cached LRA/integrated implementation** itself; and **every existing
audio-thread execution assumption**, which this round documents rather than alters.

**Gate: suites 259 + 522 = 781, `check-docs.py` clean over 72 files, pluginval both modes ×3 at
`build.yml`'s strictness with the editor under xvfb.** The one thing local evidence cannot cover
was stated in ADR-0021 rather than implied — and **the first CI run on this branch closed it**:
ISCC produced `Anabasis-0.1.1-Windows-Installer.exe`, `build-pkg.sh` passed its own
component/identifier self-checks on the macOS runner, and all three platforms are green. Two
narrower gaps remain, both named in that ADR: the Linux `install.sh`/`uninstall.sh` have not
been EXECUTED as root anywhere (CI stages them and sets the mode bit, which is not the same
thing), and `release.yml` itself has never run — it is tag-triggered, so its first exercise is
the `v0.1.1` tag or a `workflow_dispatch` rehearsal.

**Architecture Review Gate — CLEARED (2026-08-06). The first gate clearance in this repository.**
Two of ADR-0015's three contract changes are `ARCHITECTURE_REVIEW_GATE.md` items in their own
right — the `int_meterTargets` removal is a **Serialization Registry change**, the `ceiling` and
`truePeakMode` defaults are a **Parameter Registry change** — and both are on `CLAUDE.md`'s
Hard Stop list, which a green build explicitly does not clear. They landed in PR #8 on the round-2
directive plus a self-authored ADR, which the review flagged twice: correctly, because the gate's
Procedure puts the human review *before* the merge, so an ADR written in the same PR is a record,
not a clearance. **The owner has now reviewed and signed off all three by name** — the field
removal, the −1.0 → −0.1 ceiling and the enabled → disabled true-peak default — with the
instruction that they are confirmed and not to be reverted. The sign-off is quoted in ADR-0015's
Status banner (an ADR's Status is where its authority lives), indexed in `ADR_INDEX.md` so the
question "was the gate cleared?" is answerable without opening the file, and struck from the
Pending Tasks row's item (g) above. Those three decisions are now **settled, not ⊕**; every other
item taken under the v0.1.0 blanket approval keeps its ⊕, including the ⊕ on the mode-aware unit's
*wording*. The ordering is recorded as the part not to repeat: the next gated change flags and
waits. Gate Procedure step 4 (`RELEASE_COMPATIBILITY_CHECKLIST.md`) is a release-time gate with no
previous release to diff against, so it binds at the first release, not here.

**Architecture Review Gate — second clearance, and a third record opened (2026-08-06).**
ADR-0016 (`int_spectrumOn`'s repurposing) was **cleared**, separately from ADR-0015 and on its own
terms: the owner's confirmation names the semantic change, the decision to keep it a pre-1.0
migration change, and — the item worth reading twice — the acceptance that stored values keep
loading with **no migration path** because the window is open. That third point is an acceptance
of the read delta ADR-0016 tabulates, not a claim that none exists. Reviewing that area surfaced a
third member of the same class, previously unrecorded anywhere: **`int_uiScale`'s ladder narrowed
from seven steps to five**, which leaves the field's type, unit and default untouched but changes
its accepted DOMAIN — and the ladder is named in `SERIALIZATION_REGISTRY.md` §1.6 and §2 as part of
this field's read contract, so a stored 80/90/175/200 now converges (→ 75/85/150/150) and the
correction persists at adoption. 100/125/150 are common to both ladders and survive untouched, so
the ordinary session is unaffected. **ADR-0017** records it, at the gate's bar even though it is a
domain change rather than a semantic one — over-recording a pre-ship change costs a paragraph,
under-recording one costs a user's window size. Its gate is **open**; neither 2026-08-06 sign-off
names `int_uiScale`, and it is deliberately its own record rather than a fourth item in ADR-0016,
which had just been signed off naming three. Carried as Pending Tasks item (i).

**Architecture Review Gate — third clearance; the round-2 batch is fully cleared (2026-08-06).**
ADR-0017 (`int_uiScale`'s ladder narrowing) was cleared separately again, the owner's confirmation
naming the reduced ladder, the acceptance that out-of-set stored values normalise on adoption, and
that this is a pre-1.0 decision with **no released-session migration obligation** — there is no
released session to owe one to. That closes the batch: **ADR-0015, ADR-0016 and ADR-0017 are each
cleared on their own terms**, which is the sentence the three-record split was for. One record
would have made the second and third clearances read as extensions of the first.

Two code items landed with it. The GR history view drew its "SPEC" chip FIRST so it would survive
the reader contract's three early returns, then stroked the trace and waveform over the whole
`area` — which includes the chip's footprint, so at zero reduction the trace crosses the glyph
while `SpectrumView` (which draws its chip last) does not. The traces move into a private
`paintHistory` that keeps all three early returns verbatim, and the chip is drawn after it: same
order as the sibling view, same hit-area, no geometry change. And `iid::spectrumOn`'s identifier
comment still read "bool (dismissible, brief §6)" — the meaning ADR-0016 supersedes — in the very
file the ADR names as the field's home; it now states the mode semantics and cites the ADR.

**Architecture Review Gate — fourth clearance: the ADR-0022 preset-identity port (2026-08-08).**
The owner approved the migration plan in writing — the plan named the exact serialized shape
before any code was written, and the gate's procedure ran in order this time: plan → approval →
ADR + registry rows → implementation. The port brings Anamorph's validated preset-identity design
(its ADR-0024 as amended, merged in its PR #100) across as the ADR-0009 product-family decision:
factory presets carry immutable internal ids, a user preset is identified by its file, the menu
mark and `‹ ›` stepping resolve identity first (a known identity absent from the list selects
NOTHING — never a same-named substitute), saving selects what was written, and the identity
travels as **three additive strings on the `SLOT` unit** — the one carrier Anabasis needs where
Anamorph needed nine root+slot fields, because here the live state IS `slot[active]` and undo,
A/B, Copy and the session all read the same tree. User `.anabasis` files are untouched, absence
decodes to the pre-ADR-0022 name fallback, and every fallback path restores parameters
bit-identically. The editor's `rememberPresetSource` hint — window-lifetime, name-confirmed, the
pattern Anamorph's review explicitly recorded as "not the model" — is deleted, not wrapped.
**Deliberately not ported, each for an architectural reason recorded in the ADR:** Anamorph's
`onSaved` re-baseline hook (Anabasis pushes pre-state snapshots, so a save is transparent to undo
by construction), its identity-moved redo narrowing (no same-sound coalescing branch exists here
to narrow), its root-level property trio, and the rest of its 0.9.2 change set (menu lifetime —
already present here — and wording items). One residual is imported with its sign-off context: a
gesture open across a save snapshots the pre-save identity, the same narrow shape Anamorph's
maintainer accepted. The earlier Round-4 disclosure is also closed in this round: the Copy-guard
mutants that never finished their builds when that run was stopped were re-run to completion —
the dropped change test was killed by four checks (both (3c) assertions among them) and the
unconditional redo clear by exactly the two (3d) assertions.

**A four-dimension adversarial review of the finished change set (correctness, serialization
compatibility, test coverage, scope/hygiene; every finding independently re-verified against the
tree) confirmed ten items, all fixed in the same round.** The one correctness item: the Copy
change-guard read a pre-ADR-0022 stored slot (no trio) against a fresh save (empty trio) as a
difference — `isEquivalentTo` fails on property count before values — and minted a dead undo
step on the first Copy after loading an old session; `strippedForUndoCompare` now defaults the
absent trio to `""`, so the compare distinguishes exactly what the decoder distinguishes, and an
identity-ONLY move still mints its step (both directions mutation-killed). The rest: Copy — a
named identity carrier — gained its own cases; the `resetSlotFieldsToDefaults` seed gained the
no-AB chimera test that can actually kill it (a stale user-file selection under a freshly
defaulted name); `testFactoryPresetIdIntegrity` gained the all-defaults cardinality assertion
the ADR had over-claimed; both new tests gained the suite's guarded factory-table premise; the
ADR's "two SLOT trees verbatim" sentence now says what `getStateInformation` does, and the
registry records the inactive-slot absence-survives-re-save exception; KI-007 item 2 is marked
RESOLVED; KI-003's restore-writer enumerations gained `liveSelection`; and the four hint-era
records in DOCUMENTATION_COVERAGE/HANDOVER carry supersession markers per the house convention.

**A second review round found the one behavioural regression the identity port introduced: the
`‹ ›` ring could be TRAPPED by an unreadable preset file.** The identity moves only on a
*successful* apply, so a step onto a corrupt `.anabasis` left the position where it was and the
next press re-offered the same file for ever. The deleted editor hint was advanced
*unconditionally* for exactly this reason (`DOCUMENTATION_COVERAGE.md`, round 63 item 2) — that
rationale outlived its mechanism with no carrier, which is the gap. `stepPreset` now keeps
stepping until an entry loads; identity resolution is untouched.

**Gate: suites 259 + 634 = 893 (the identity work added 112 checks), `check-docs.py` clean over
73 files, pluginval both modes ×3 at `build.yml`'s strictness with the editor under xvfb — re-run
after the ring fix, the last change to reach the binary — and fourteen negative controls, each
reverted independently and each killed by its own assertions, none by another's.** The final one
is the ring: restoring the single-shot step fails exactly the two assertions that say the arrows
moved and kept moving.

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
~~accessibility polish (focus order audit)~~ — **DONE 0.1.1**: every slider, combo and toggle
accepts keyboard focus, closing the §8 half that had no implementation (the names half was
already done); packaging ~~(OQ-007)~~ — **DONE 0.1.1**, ADR-0021; licensing decisions (OQ-002);
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
| **JUCE** | **9.0.1** — immutable commit `e18f7f5…` (ADR-0028, 2026-08-16). OQ-001 pinned 9.0.0 / `f8f8864…` on 2026-07-30; **Anamorph is still on that revision**, so the pin is no longer shared | Framework for all DSP, parameters/state, GUI and plugin wrappers. An unpinned bump can silently change DSP/latency/state-ABI. Sharing the pin with the sibling product made a JUCE-attributable difference between them impossible and made a bump a product-family decision — ADR-0028 moved this repository alone, on the owner's directive, and Anamorph is read-only from here so nothing in this change could carry it along. What bounds the divergence is measured: of the fifteen modules linked, **six are byte-identical between the two tags apart from their module-declaration version string — `juce_dsp`, `juce_audio_basics`, `juce_audio_processors`, `juce_data_structures`, `juce_audio_utils`, `juce_audio_plugin_client`** — so no DSP, parameter, state or wrapper behaviour can differ for this reason; the nine that changed are GUI, platform-native, file-format and web-view surfaces, and ADR-0028 tabulates each with its diff size and why it is or is not reachable here. A bump is a Build System change (ADR + Review). `docs/policies/DEPENDENCY_POLICY.md`. |
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
