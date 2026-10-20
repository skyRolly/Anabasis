# 2026-10-21 — PR #42: Devin review, session TP positions and ordered merge

Starting heads: #43 `d2c1c59`, #42 `cee7bdd`, main `4252f35`. The user asked
for repository-based decisions, repairs where necessary, and conflict resolution
that preserves development code. Merge order remains **#43 → #42**, both with
merge commits, with no tag between them.

## Review dispositions

| Finding | Decision and evidence |
|---|---|
| #43 falling true-peak Ceiling | Already fixed in #42 by `dce072b`, completed by ratified ADR-0046 (`4ff71bd`, `f03d673`). `AnabasisEngine::process` predicts the glide at emission; `CeilingClamp::lowerInFlightCeilings` revises requirements after retargets. Existing automation, ascent and cut-step tests exercise both defining meters. No duplicate Phase 0 patch. |
| #43 offline engagement tail | Already fixed in #42 by `74c114e`; direct adoption clears the active tail before stage E. `testOfflineEntryDropsTheEngagementTail` checks the active-tail premise, silence, programme and both meters. |
| #43 low-rate fixed-ceiling overs | Already addressed by ADR-0046: eased attack, 16-sample floor, bounded release rise. The ratified contract engages TP from 12 kHz; below it the sample clamp and `dB` unit are intentional. Existing low-rate and unit tests guard that boundary. Do not restore the superseded all-rate dBTP assumption. |
| #43 TP-only toggle plus bulk swap at offline entry | Preserve direct adoption. The entry is the host-selected beginning of a rendered stream, and the branch deliberately discards transition fades. `a43094b` / `f03d673` clear EQ, dry audio and TP history on every entry latch, not just Force Max. New `testTpOnlyOfflineEntryWithABulkSwapStartsClean` covers the exact TP-only + forced-duck block, Oversampling Off, both EQ positions and wet/bypass legs. It compares with a clean render and with an otherwise identical entry without the bulk-swap request. KI-004 / KI-024's other no-re-prepare limitations remain disclosed. |
| #42 bypass audio in session TP | Confirmed and repaired below. The old source expressly documented the residual; that is not sufficient for amendment 4's programme-exclusion promise. |
| #42 session clock vs loudness-admission guard | Keep the existing definition. ADR-0020's 2026-09-29 note and USER_MANUAL §3.4 already explain that the clock counts open frames, while I/LRA have straddler and ring-out guards. The clock tests assert exact timing. The TP model now uses reported positions instead of current frames. |
| #42 stopped GR maximum | Keep it. `PluginEditor::refreshFromModel` retains the maximum of the stopped history graph while current GR becomes a dash. USER_MANUAL §3.4 and `testTheTickShowsTheLimiterGrReadout` explicitly pin this distinction. |

## The TP eligibility repair

`TruePeakEstimator::kReportLag` is six. Previously `processChunk` used current
`sessionOpen` for the reading at position `n - 6`. This discarded the final
processed positions after BYPASS started and admitted audition positions after
it ended. The clock regression's reference repeated that mistake.

The engine now carries six eligibility bits in fixed audio-thread storage.
Only TP admission reads the delayed bit; SP, loudness pause and duration still
read the current frame. Full reset primes the history for the estimator's silent
pre-history. Meter RESET preserves that history and independently retains the
existing six-reading skip for positions preceding RESET. No allocation, atomic,
thread, estimator kernel, emitted audio, parameter, state format or latency changes.

`testSessionTruePeakFollowsTheReportedBypassPosition` uses audio-derived
eligibility and a separate estimator at 44.1 kHz / 64 and 96 kHz / 512. It
checks each six-frame edge, immediate SP/clock pause/resume, RESET during the
audition and calls spanning multiple internal chunks. The session-clock model
now indexes a complete eligibility history by reported position, independently
of the engine's ring implementation.

**Negative control:** with only that regression/model correction and the old
engine, the DSP suite reports **783 checks, 14 failures**: six new bypass-edge
checks and eight existing clock/model comparisons. The SP/clock edge checks
pass. The later offline-entry test is a guard for existing behavior, not a
product change.

## State-restoration crash: attribution remains limited

The #43 record's `0x6036105` crash was during state restoration, before
`SUCCESS`, with no stack. Its retry changed seeds. #42's KI-028 has actual
external-fault stacks and a causal validator-fix comparison, but those cannot
identify that particular unstacked event. No speculative state-restoration or
retry change is justified. Current validator results are recorded below;
a passing replay cannot retrospectively diagnose the original crash.

## Conflicts

#43 merged main in `d36f453`, preserving its development-side coverage record
and accepting nonconflicting action-pin updates. #42 then merged updated #43
in `a208b54`; its only conflict was the same coverage introduction. Resolution
keeps #42's Phase 1 / DSP history and adds #43's review addendum. Neither merge
changes source or tests. Original cited commit SHAs remain in the ancestry.

## Validation

- Environment: Linux x86-64, GCC 14.2.0, CMake 3.31.6, C++23, Release,
  default LTO, pinned JUCE 9.0.1. This is a local development build, not the
  pinned Clang shipping lane or a cross-platform CI result.
- #43 records: `check-docs` self-test and scan (133 files), citation self-test
  and check against its original remote head pass.
- Combined tree: `check-docs`, `check-realtime` and `check-portability` pass
  with their self-tests. Citations pass against the original #42 remote head
  and `origin/main`; ten moved source anchors were re-read and re-anchored.
- The ABI checker's 19 self-tests pass. The pre-existing main VST3 from this
  workspace requires `CXXABI_1.3.15`, above the repository's declared `1.3.9`.
  This GCC 14 toolchain limitation predates the repair; no ABI threshold or
  build configuration was changed to hide it.

- Build: DSP suite, state suite and VST3 succeed. The first-party warning gate
  is clean (ten warnings in third-party ALSA headers; the linker also reports
  serial LTRANS compilation).
- Suites: **795 DSP + 1638 state = 2433 checks**, zero failures. The DSP total
  adds ten bypass-eligibility checks and twelve offline-entry checks. Existing
  automation, low-rate, engagement-tail, reset, session-clock and GR tests pass.
- Exact-seed investigation: pluginval 1.0.4, strictness 10, editor enabled under
  Xvfb, randomised order, **seed `0x6036105` in all three fresh processes**:
  3/3 exit 0 and print `SUCCESS`, without retry. This uses the combined 0.2.17
  tree and local toolchain, not the original Phase 0 binary/environment. It does
  not reproduce or retrospectively attribute the old crash.
- Final VST3 ABI: the same `CXXABI_1.3.15` requirement and local gate failure
  as the pre-existing main artifact. The declared floor remains `1.3.9`.

- Regular pluginval gates: deterministic ×3 and randomised ×3 pass. The second
  deterministic pass printed `SUCCESS` and then exited 139; the script's
  existing Linux crash retry passed with the same seed `0x1`. All other gate
  passes succeed on attempt one. The after-`SUCCESS` event is separate from
  #43's in-test `0x6036105` event and is not counted as a clean first attempt.

- Ordered merge simulation: a merge commit for #43 into current main, followed
  by #42, has no conflicts and produces exactly the candidate #42 tree.
  Both reconciliation merges preserve all source/test files from their
  development parents. The final committed tree is checked again before push.

- Debugger follow-up: GDB 16.3 and its runtime dependencies were extracted
  under `/tmp`, outside the repository. Three fresh fixed-seed `1` processes
  and three fresh random-order `0x6036105` processes were run under GDB with
  all-thread backtraces requested on a fault. All six print `SUCCESS` and exit
  normally. Neither the old state-restoration fault nor the new after-SUCCESS
  event reproduces under the debugger, so no new fault stack was obtained.
  Both individual events remain unattributed; no product or retry change was
  made from log position alone.
