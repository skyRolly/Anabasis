# 2026-09-29 — PR #42, fifth review round: ADR-0046 ratified, the low-rate contract, OQ-020, KI-028, the session clock, DSP-005

The round after the fourth review round
([`2026-09-28-pr42-round4-tp-lowrate-reset.md`](2026-09-28-pr42-round4-tp-lowrate-reset.md)) had been
pushed as `6ee9f29`. Asked for, in this order: re-verify the two Devin findings the fourth round fixed
(without redesign); accept ADR-0046 and make the amended records unambiguous; state the low-rate
true-peak contract; resolve OQ-020; give KI-028 exactly one disposition; settle what the session
length counts (a new Devin comment); verify the true-peak contract over every rate and path; verify
the CI of the final head; classify the PREfast alerts; and only after all of that, investigate
DSP-005 / KI-023. No UI roadmap expansion; no merge; Anamorph untouched. The closure record is
[`docs/reports/2026-09-29-pr42-round5-closure.md`](../docs/reports/2026-09-29-pr42-round5-closure.md);
this file is the evidence behind it.

Container and toolchain as in the earlier rounds (Linux x86-64, 4 cores; GCC 13.3; clang 22 for the
realtime tier). Engine figures come from the real `AnabasisEngine` compiled from frozen worktrees;
probes, harnesses, logs and cores stay in the session scratch directory, not in the tree. True-peak
figures are over the LIVE smoothed ceiling, product meter (PM) / BS.1770 Annex 2 (A2), tolerance
0.1 dB, as ADR-0043 defines dBTP. Each item keeps apart what was FOUND (by Devin, or by this round),
what was IMPLEMENTED, what was VERIFIED, what was DECIDED and what is a LIMITATION.

## 0. State at the start (inspected, not taken from the previous report)

- PR #42 open, head `6ee9f29` (0.2.16's records), base `main` `ed06ad0`; not merged.
- **CI on `6ee9f29`:** push run 36502299990 — every job success (`docs`, `preflight`,
  `source-lint`, `linux`, `linux-lto-tests`, `linux-lto-clang`, `realtime`, `sanitizers`,
  `windows`, `macos`, `macos-intel`); pull-request run 36502303279 success; PREfast check run
  109198503545 "101 new alerts" (§8). On `fe29bda`, one job had failed: `macos-intel`, AU
  randomise, the teardown abort recorded as KI-028.
- **Devin's findings on PR #42:** A "Low-rate ceiling cuts exceed dBTP limit" (`CeilingClamp.h`) and
  B "Old peaks survive statistics reset" (`AnabasisEngine.h`), both fixed by the fourth round; a new
  comment on `src/dsp/LoudnessMeter.h:205` (`setSessionPaused`) asking what the session length
  counts once I and LRA leave audio out after a bypass resume (§5).
- **Records:** ADR-0046 filed "Accepted — 2026-09-28, on the owner's direction" and flagged ⊕;
  ADR-0045, ADR-0044 and ADR-0020 amendment 4 flagged ⊕; KI-024 – KI-028 as the fourth round left
  them; OQ-020 open.

## 1. Devin A and B — FIXED, verified (no redesign)

| Finding | Regression tests on the final tree | Engine evidence this round | Verdict |
|---|---|---|---|
| **A** — low-rate ceiling cuts over the dBTP ceiling | `testTruePeakModeHoldsTheCeilingBelow44k` (fails 12 checks on 0.2.15, recorded in round 4), `testTheCeilingUnitFollowsTheRateTheTruePeakPathEngagesAt`, `testTruePeakModeLagsAnAscentByTheEntryCeiling`, `testTheClampReleaseRiseIsCapped`, `testTheClampSilencesAnAstronomicalInput` — registered and passing | The KI-025 bursts at the test's own placements, rebuilt independently of the test: 0.2.15 (`8ab0532`) reproduces the recorded failures exactly — 22.05 kHz cut PM +0.0332 / A2 +0.1566, 32 kHz cut A2 +0.1250, 16 kHz static A2 +0.1157, 8 kHz static A2 +0.1134; the head holds them (22.05 kHz −0.1596 / −0.1006, 32 kHz −0.1447 / −0.0658, 16 kHz −0.1109 / −0.0654). The whole matrix (§7): no claimed reading over 0.1 dB at any engaged rate outside the recorded splice class | **FIXED — verified** |
| **B** — statistics RESET lets old peaks in | `testStatisticsResetStartsTheSessionAtTheReset`, `testResetRightAfterALoudPassageKeepsTheOldPeakOut` — registered and passing | 1368 matrix renders with a RESET at a block top are bit-identical in their audio to the same render without it (the RESET touches statistics only); the session-clock probe (§5) reproduced the 6-reading TP skip and the 100–200 ms loudness guard exactly | **FIXED — verified** |

Suites on the final tree: see §10.

## 2. ADR-0046 — verified against the code, then ratified (`d7174ef`, `e7b6f4e`)

### 2.1 What was checked (read from the code at `6ee9f29`; nothing edited for it)

Every constant and mechanism the ADR names matches the code: the 16-sample attack floor, the ease
span 4.8 with weights by age, the 1 % per-sample release rise cap with its −80 dB floor term, the
revision lead of 1, the 12 kHz rail, the division by the weights' own float sum (`easeTotal`), and
the `min (entry, emission)` stamp in stage A, which feeds both the clamp and the limiter. One
predicate, `truePeakPathEngages` (`Latency.h`), decides both the engine's rail and the Ceiling's
unit.

### 2.2 Latency and window by rate (measured with the repo's JUCE-free headers)

`Latency.h` → `CeilingClamp.h` compiled standalone; only the engine's composition in `latchOsConfig`
replicated. Over 26 rates × oversampling 1–16× × both phases × Force Max offline, **the reported
latency is identical with TP on and off and equals the engine's composition** — the ADR's
"no reported-latency change" holds everywhere. Boundary scans over every integer rate 1–800 000 Hz:
the path would fit from 4801 Hz; `truePeakPathEngages` is true from 12 000 Hz and at no rate below;
0 rates in [4801, 800 000] where the fit or the engagement disagrees with `sr ≥ 12000`; `A` first
exceeds 16 at 66 000 Hz.

| Host rate | TP engages | A | D | Limiter window, TP mode | Window, TP off |
|---|---|---|---|---|---|
| 8 / 11.025 kHz | no | — | — | the full 10 ms allowance | 10 ms |
| 12 kHz | yes | 16 | 46 | 6.17 ms | 10 ms |
| 22.05 kHz | yes | 16 | 46 | 7.94 ms | 10 ms |
| 44.1 kHz | yes | 16 | 46 | 8.96 ms | 10 ms |
| 48 kHz | yes | 16 | 46 | 9.04 ms | 10 ms |
| 66 kHz | yes | 17 | 47 | 9.29 ms | 10 ms |
| 96 kHz | yes | 24 | 54 | 9.44 ms | 10 ms |
| 192 kHz | yes | 48 | 78 | 9.59 ms | 10 ms |
| 768 kHz | yes | 192 | 222 | 9.71 ms | 10 ms |

**Found (drift, recorded in the ratification note (a)):** before ADR-0046, D was 38–45 below 62 kHz
(not "38–42 up to 48 kHz"), so the TP-mode window is shorter than 0.2.15's from 12 kHz to below
62 kHz, unchanged from 62 kHz, and longer at 3901–11 999 Hz, where 0.2.15 engaged the path and
0.2.16 gives the limiter the whole allowance.

### 2.3 Decision and records

- **Decided:** ADR-0046 ratified on the owner's approval, recorded in the four places the repository's
  convention uses (`e4f9205`): the ADR's ✅ RATIFIED banner and Status (the owner quoted verbatim),
  `ADR_INDEX`, the policy / ledger rows, `HANDOVER`. Drift recorded in the Status: the brief said
  "from Proposed", but the ADR was filed Accepted-with-review-flag; under the convention, acceptance
  at the gate is ratification, so the flag is cleared and the status word does not change.
- **Amended records made unambiguous, history kept:** ADR-0041 (both 2026-09-28 banners kept, a
  dated note), ADR-0015 item 5, ADR-0004 (the D figure), ADR-0045 (banner word for word; a separately
  dated note names which decisions stand and which are in force as amended; the ADR itself stays ⊕).
- **Stale wording removed from living text:** emission-only stamping (comments in `CeilingClamp.h`,
  `AnabasisEngine.h`, `AnabasisEngine.cpp`), D = 42 (the ADR-0004 banner, `ADR_INDEX`, two test
  comments), "any sample rate" (`DSP_POLICY` invariant 3's guard label, one test comment), "the
  readout's unit follows the switch" (`USER_MANUAL`, `PARAMETER_REGISTRY` fn 15, an editor comment).
  CHANGELOG entries, worklogs, reports and dated banners were not edited.
- **Ratification notes:** (a) the per-rate table above; (b) below 12 kHz the TP switch still moves the
  LIMITER's detector at Oversampling Off / 2× (`limiter.setTruePeakMode (p.truePeakMode && osN < 4)`)
  — a best effort with no dBTP guarantee, recorded as intended, no DSP change; (c) after a prepare at
  a host rate ≤ 0 the unit reads dBTP while the sample clip runs — a non-conforming host's
  limitation; (d) `DSP_POLICY` invariant 3 takes the prescribed text, invariant 4 carries decision 6's
  text (its closing clause kept ahead of the rate clause, `e7b6f4e`).
- **The round's own review of these commits** (`e7b6f4e`, docs only): KI-025 item (2) still said
  "true-peak mode is not available" below 12 kHz — a dated note now says what is not available is the
  dBTP ceiling; RISK-003 attributed +0.23 dB to the wrong search; three ledgers omitted "at
  Oversampling Off / 2×"; OQ-020 did not say the tips share the host-rate ≤ 0 limitation. Two items
  were left: the `d7174ef` message's "tracked files keep their line counts" is false for two
  comment-only files outside the citation gate (harmless; rewriting the commit would be worse), and
  HANDOVER's test count (this records commit).

## 3. The low-rate true-peak contract (stated in `d7174ef`, `c194000`)

| | Below 12 kHz | 12 kHz and above |
|---|---|---|
| **What the Ceiling holds** | **sample peak** — every sample at or under the Ceiling, whatever the TP switch says | with TP on: **dBTP** (inter-sample peaks, ADR-0043's two meters, 0.1 dB); with TP off: sample peak |
| **What the TP switch does** | moves the limiter's detector to its true-peak estimate at Oversampling Off / 2× (best effort); no dBTP clamp | engages the dBTP clamp (ADR-0046) |
| **What is NOT guaranteed** | any inter-sample figure: the matrix reads inter-sample peaks up to +26.1 dB over the Ceiling there, reported and not claimed; "the switch is on" does not mean dBTP | readings across a host-drawn splice (a reset, re-prepare or offline entry without re-prepare: the stream's own end, KI-024 route B), and an audition's crossfade (BYPASS plays the input) |
| **Ceiling unit in the UI** | " dB" | " dBTP" with TP on, " dB" with it off |
| **Tooltips (OQ-020)** | name the 12 kHz boundary (§4) | unchanged |

Nothing describes sample clipping as true-peak protection, and no record claims dBTP below the rail:
`USER_MANUAL` §1 / §2.4 / §3.2–3.4 / §6 / §8, `COMPATIBILITY_MATRIX`'s rate rows, `DSP_POLICY`
invariants 3 / 4 / 8, `KI-025`, `RISK-003`, `PARAMETER_REGISTRY` fn 15.

## 4. OQ-020 — resolved, option 2 (`c194000`)

- **Decided (the owner's brief, 2026-09-29):** at host rates where the true-peak path engages, both
  tooltips unchanged word for word; below it the TP switch reads "Catch inter-sample peaks at sample
  rates from 12 kHz up - below that the Ceiling holds sample peak, not dBTP" and the Ceiling (both
  knobs and their value boxes) "The output limit - nothing leaves the plugin above it. Sample peak at
  sample rates below 12 kHz, with or without TP". The figure is formatted from
  `CeilingClamp::kMinTruePeakRate`. Every word is the repository's existing terminology (the
  provenance of each is in OQ-020); no marketing language.
- **Implemented:** `CeilingUnitSource::rateEngagesTruePeak` is the rate half of `truePeakEngaged`, so
  the unit and the tips share one decider; the editor seeds the tips in its constructor and edge-gates
  them on the 24 Hz tick (message thread only — no new atomic, writer or ordering);
  `Knob::setTooltip` forwards a knob's tip to its value box (JUCE copies it only when it builds the
  box). No other UI string changes.
- **Verified:** `testTheTruePeakTipsFollowTheRateTheTruePeakPathEngagesAt` (state suite 1605 → 1627)
  — re-prepares at 11.025 and 12 kHz with the editor open, both switch states, and an editor opened at
  8 kHz; three mutants (tick call removed, seed hard-coded, value-box forward dropped), each killed.
- **Limitation:** the wording is held ⊕ for the owner's fine review; "sample rate(s)" and the Ceiling
  tooltip's inclusion are named there for confirmation. The tips share the unit's recorded
  host-rate ≤ 0 limitation (ratification note (c)).

## 5. The session clock (Devin, `LoudnessMeter.h:205`) — option A kept, pinned (`0d42384`)

- **Found (by Devin):** after a bypass resume, I and LRA leave audio out that the session length
  counts.
- **Semantics determined, from the records:** ADR-0020 amendment 4 item 3 defines the length as the
  seconds of programme the session figures cover, stopping while a realtime bypass audition is
  audible and while no audio is processed, and returning to 0:00 at the session's reset points. Its
  item 2 lists the I / LRA watermark as a **cost** of measuring I and LRA, not as a change to what
  the session covers; the RESET precedent (the length counts from the reset while I skips up to
  0.2 s, pinned by `sessionTime: …and counts from the reset`) is the same relationship. "The audio
  admitted into I" is not a duration: I is a gated block mean.
- **Measured** (real engine in lockstep with a never-bypassed twin; 44.1 / 48 / 96 kHz × blocks 64 /
  512; local Release, LTO off): clock − open frames = 0 in every scenario (normal, audition, resume,
  RESET after a resume, RESET inside an audition, 8 × (0.3 s bypass / 0.4 s open), offline). The SP
  and TP holds are bit-identical to a model over exactly the open frames. I's first block after a
  resume lands at (fc + 4)·L − r — 100.0–199.6 ms of open frames uncovered, 0 mismatches in 720
  resumes; up to one sub-block before a pause; LRA's first reading at (fc + 31)·L − r. On steady
  programme, I moved 0.021–0.029 LU after a 1 s audition; 8 short open runs put 3.1 s on the clock
  with I and LRA bit-identical.
- **Decided:** keep option A — the length counts the open frames. Moving it toward the admitted audio
  would break the recorded RESET contract, the SP / TP ↔ clock correspondence, stall the display
  during fast A/B toggling and fail two existing tests. No hard-stop item touched; no UI copy.
- **Implemented:** no product code. `USER_MANUAL` §3.4 now says the time and the holds pick up with
  the first moment after the audition while I leaves up to 0.1 s before and 0.2 s after it out, and
  LRA moves again ~3 s after it; an ADR-0020 implementation note (not an amendment, flagged ⊕ for the
  owner) records the same.
- **Regression tests** (fail on a changed relationship, pass on the kept one):
  `testTheSessionClockCountsTheOpenFramesNotTheAdmittedAudio` (DSP suite, +32 checks) and
  `testThePublishedSessionLengthIsTheOpenFrames` (state suite, +11). Ten mutants, all killed —
  among them the move toward option B (clock skips 100 ms after a resume: 12 + 3 failures) and ramp
  frames counted as open (18 + 5; the pre-existing 30 ms-tolerance duration check did not catch
  it, the new exact check does).

## 6. KI-028 — pluginval's teardown crash

### 6.1 The exit-9 label (`d006984`)

On macOS pluginval's command-line mode installs `kill9WithSomeMercy` for SIGFPE / SIGILL / SIGSEGV /
SIGBUS / SIGABRT, which ends the process with `std::_Exit (SIGKILL)` — exit **9**, below 128, which
`scripts/run-pluginval.sh` read as "real validation failure, not a crash". It now reports it as
`CRASHED`; the pass still fails at once (macOS has no crash-retry), so the gate is unchanged —
nothing is suppressed and no retry was added.

### 6.2 Linux (local; stacks from cores)

- 177 runs under gdb and Xvfb (pluginval v1.0.4 from source on its JUCE 8.0.3; the VST3
  RelWithDebInfo, `ANABASIS_NO_LTO=ON`) at `6ee9f29` and `ed06ad0`: 5 crashes, each a SIGSEGV on
  pluginval's message thread and a use-after-free in pluginval's own JUCE 8.0.3 host code, **0 frames
  from Anabasis source on any thread**. Signature A (in-test, `juce_XEmbedComponent_linux.cpp:570`,
  a `callAsync ([this] …)` delivered after the editor was deleted) is the class the Linux crash-retry
  exists for; signature B (after `SUCCESS`, `juce_VST3PluginFormat.cpp:477`, `RunLoop::Impl`'s fd
  callback on a freed `Impl`) is the KI-028 exit crash on Linux.
- **Causal test of B:** a second pluginval, identical except for a clean port of JUCE `04e167d64`
  ("VST3 Host: Fix an occasional crash when removing callbacks from the message loop during
  shutdown"; all 5 hunks, no hand edits), against the SAME Anabasis binaries (`ed06ad0` and
  `6ee9f29`) in 711 seed-matched, lane-balanced, concurrent pairs — 1422 runs. B: **5 / 567
  unpatched, 0 / 567 patched** (full configuration, both trees pooled; Fisher one-sided p = 0.031);
  A unaffected (8 vs 5). 0 crashes of any other signature in the 1422; all 18 stacks read by hand.
- The resize-storm link between the plug-in's editor and signature A (337 of 347 `applyUiScale`
  calls inside `Editor Automation` in one run) is a measured correlation, not a tested cause.

### 6.3 macOS — the first diagnostic run (36520602892, on `0d42384`)

`ki028-diag.yml` built pluginval v1.0.4 from source without the hardened runtime and injected the
`__cxa_throw` / terminate / fault-handler interposer; the AU randomise lane ran 50 passes per job
(10 seeded `0x5161f59`, 10 fresh seeds, 30 seeded without `Editor Automation`) in three variants.

| Runner / tree | Seeded `0x5161f59` | Fresh seeds | Fast (seeded, no Editor Automation) | Jobs |
|---|---|---|---|---|
| `macos-15-intel` / head `0d42384` | **7 / 10 aborted** | 0 / 10 | 1 of the 5 that ran, then r026 hung | cancelled at 150 min |
| `macos-15-intel` / main `ed06ad0` | **8 / 10 aborted** | 0 / 10 | 2 of the 2 that ran, then r023 hung | cancelled at 150 min |
| `macos-latest` (arm64) / head | 0 / 10 | 0 / 10 | 0 / 30 | success |
| `macos-latest` (arm64) / main | 0 / 10 | 0 / 10 | 0 / 30 | success |

- Every abort: `SUCCESS` for every test, then `libc++abi: terminating due to uncaught exception of
  type std::__1::bad_function_call`, on the **main thread**. Variants B / C show the tail of the
  stack inside `-[NSApplication run]` → `JUCEApplicationBase::main()` → `main`; variant A's terminate
  dump ends in `JUCEApplicationBase::main (int, char**)`. Variant C (`MallocScribble=1`) aborts the
  same way instead of faulting on a 0x55… pointer, which points to an **empty** `std::function`
  rather than one in a freed heap block [inferred: an object freed and re-allocated, or one not on
  the heap, would not be scribbled].
- **The abort predates this branch** — as frequent on `main` as on the head. It is seed-dependent on
  Intel (the recorded seed 15 of 20 across both trees; fresh seeds 0 of 20) and absent on arm64
  (0 of 100).
- **Lost:** the job log printed only each pass's last five lines, so frames 0–10 (the throw site)
  were not in it; both Intel jobs then hung on one variant-B fast pass until the job timeout, and the
  upload steps (`!cancelled()`) did not run. CodeQL's actions analysis flagged the `workflow_dispatch`
  trigger as cache poisoning; it was removed (`c9e998f`).

### 6.4 macOS — the second run (on `839685d`)

Matrix `macos-15-intel` × {`pr-head`, `control`}: the control is JUCE's own
`examples/CMake/AudioPlugin` AU built from the JUCE commit the head pins, no Anabasis code. 8 seeded
+ 6 fast passes per job, each under a 300 s watchdog (a pass still alive is `sample`d, then sent
SIGABRT, then SIGKILL); every interposer line of a failing pass printed into the job log; uploads on
`always()`. Run 36533479226: the control job stopped at its own DWARF check (the example had been
built without `-g`; fixed in `9a19b02` and re-run, below); the pr-head job ran every pass.

**pr-head (`839685d`), job 109292164478 — reproduced, throw site captured** [Verified — the
interposer's trace, `atos` for the pluginval / Anabasis / interposer frames, `dladdr` for system
frames, three ReportCrash `.ips`; artifact `ki028-macos-15-intel-pr-head-logs`]:

| Pass | Variant | Result |
|---|---|---|
| r001 seeded | A | `SUCCESS`, then `bad_function_call` → pluginval's handler → **exit 9** |
| r003 seeded | C | `SUCCESS`, then `bad_function_call` → SIGABRT (134) |
| r012 fast | C | `SUCCESS`, then `bad_function_call` → SIGABRT |
| r014 fast | B | `SUCCESS`, then `bad_function_call` → SIGABRT |
| the other 10 | A / B / C | pass (exit 0) |

No pass hung (the watchdog never fired); every failing pass printed `SUCCESS` first.

- **The throw site is the same in all four, and it is in Apple's AudioToolboxCore**, on pluginval's
  message thread ("JUCE v8.0.3: Message Thread", `main=1`):

  ```
  #0 libthrowtrace.dylib      ki028_cxa_throw
  #1 AudioToolboxCore         (a non-exported function; nearest exported symbol std::operator+)
  #2 AudioToolboxCore         std::__function::__func<AUParameterListener::AUParameterListener(
                                std::function<void (void*, AudioUnitParameter const*, float)>,
                                CAEventReceiver, double)::$_0, …, void ()>   (nearest symbol)
  #3 CoreFoundation           __CFRUNLOOP_IS_CALLING_OUT_TO_A_BLOCK__
  #4 CoreFoundation           __CFRunLoopDoBlocks
  #5–#12                      CFRunLoopRun … HIToolbox … -[NSApplication run]
  #13 pluginval               juce::JUCEApplicationBase::main() (juce_ApplicationBase.cpp:277)
  #14 pluginval               juce::JUCEApplicationBase::main(int, char const**) (:255)
  ```

  A block queued on the main run loop by AudioToolbox's parameter-listener machinery invokes the
  listener's stored `std::function`, which is empty; the thrown `type_info` belongs to
  AudioToolboxCore's own copy. The exception escapes `-[NSApplication run]` (`objc_exception_rethrow`
  in the terminate dump) and the process aborts. The variant-A stack of the first run, which ended
  in `main (int, char**)`, was the terminate dump after that rethrow — the same event.
- **No thread holds an Anabasis frame** at the throw or at the abort, in any of the three crash
  reports (every thread's full stack). The validator thread is already in
  `PluginsUnitTestRunner::~PluginsUnitTestRunner` (the plug-in instance deleted); the Anabasis image
  is still mapped (a bundle is not unloaded), with nothing executing in it. macOS 15.7.9 (24G830),
  Intel.
- **Whose listener it is** [read from the source]: the AU listener belongs to the HOST. pluginval
  1.0.4's JUCE 8.0.3 `AudioUnitPluginInstance::createEventListener` calls `AUEventListenerCreate`
  on `CFRunLoopGetMain()` (parameter value / gesture events and four property changes), and its
  `cleanup()` — the instance's teardown — calls `AUListenerDispose` **first**, then
  `releaseResources`, then `AudioComponentInstanceDispose`. The plug-in only ever notifies:
  JUCE 9.0.1's AU wrapper calls `AUEventListenerNotify (nullptr, nullptr, …)` on a parameter
  change, and `PropertyChanged` for latency / parameter list / present preset. Anabasis's own
  `setCurrentProgram` is a no-op (one program).
- **Mechanism** [inferred from the above, not observable in Apple's code]: an event notification for
  the host's listener is still queued on the main run loop when the host disposes that listener;
  AudioToolboxCore then runs the block against a listener whose callback it has emptied, and throws.
  The seed matters because it decides which test ends the pass and how soon teardown follows it
  (the failing order ends with "Plugin programs"; 0 of 20 fresh seeds failed in the first run).

**control, run 36537705309 on `5562229`** (after `9a19b02` added `-g` and `5562229` retained the
control's LTO object for its dSYM; run 36535966698 on `9a19b02` had stopped at the same DWARF check
and was then cancelled by the next push) — job 109305735516, **14 of 14 passes clean, 0
`bad_function_call` events**, the same pluginval build, seed `0x5161f59`, test order ("Plugin
programs" last, one program, "Changing program") and variants as the Anabasis job [Verified — the
job's `runs.tsv` and pass logs, artifact `ki028-macos-15-intel-control-logs`]. The control has no
parameters, so the host's listener is registered for no parameter events and the plug-in sends none.

**Why the plug-in's traffic is legitimate** [read from the source]: Anabasis's AU sends parameter
events through JUCE 9.0.1's wrapper only when a parameter really moves — the macro layer writes its
managed parameters with host notification and skips a no-op write (`MacroEngine::setParam`, a 1e-6
guard); `setCurrentProgram` is a no-op (one program); `releaseResources` and the destructor notify
nothing. Nothing is emitted at teardown; an event sent earlier was still queued when the host
disposed its listener.

### 6.5 Disposition

**PRE-EXISTING / EXTERNAL** (recorded in `KNOWN_ISSUES.md` KI-028, `d52570f`). The throw site is in
Apple's AudioToolboxCore, invoking the callback of a listener the host has already disposed —
pluginval 1.0.4's JUCE 8.0.3 AU host disposes it first in `cleanup()`, and JUCE 9.0.1's host keeps
the same order; no Anabasis code runs at the crash; the defect needs a plug-in that sends AU events,
which Anabasis legitimately does and the parameterless control does not; `main` fails as often as
the head. The Linux signatures are pluginval's JUCE 8.0.3 host as well (§6.2). Considered and not
chosen: *reproducible product defect* (no Anabasis frame, no illegitimate or teardown-time event
found), *flaky but understood* (the product is not the flaky party), *not reproducible* (it
reproduces on demand on Intel with the recorded seed).

**What changes:** nothing in the product and nothing in the gate — no crash-retry on macOS, no
retry count raised, the step still fails and is labelled `CRASHED`. The exact signature is
documented (`TROUBLESHOOTING.md`): re-run and record; any other signature is a new failure. The
diagnostic workflow stays as the reproducer (`CI_CD.md`). Whether the signature may be re-run past at
a release gate is the owner's decision. **Not established:** which event was queued; Apple's
internal ordering; whether a real host disposes its listener the same way; why arm64 does not
reproduce.

## 7. True-peak verification over every rate and path

Harness: the real `AnabasisEngine` of the tree under test, the product meter (each tree's own
`TruePeakEstimator`) and an independent BS.1770 Annex 2 meter, the live ceiling reconstructed from
the engine's own smoother, "excl." = ±16 samples removed around a host-drawn splice (reset,
re-prepare, re-prepare at a new rate, Force Max entry with / without re-prepare), "aedge" = a reading
whose window spans a BYPASS audition's crossfade (the input is audible there; not a ceiling claim).
22 778 renders on `6ee9f29`: 19 rates (7.999 kHz – 768 kHz) × 11 programmes × automation shapes
(static, steps, ramps, reversals, zig-zags, repeated cuts) × OS × phase × EQ × blocks 1 / 7 / 64 /
512 / irregular × 11 lifecycle paths (steady, TP engaged mid-stream, Force Max entry with and without
re-prepare, return to realtime, re-prepare, re-prepare at a new rate, reset, statistics RESET,
oversampling switch, BYPASS audition). The bypass path was first run without the aedge split (its
audition edges then read as claimed, up to +0.2548 / +0.3937) and re-run with it; the table uses the
re-run. The harness is not job-for-job comparable with round 4's 24 167 renders (that harness was
lost; this one was rebuilt from the worklog and the tests), only class-for-class.

| Rate | TP | Guarantee | Claimed renders | Worst claimed, excl. (PM / A2) | Tolerance | > 0.1 dB | Unclaimed classes (not judged) | Result |
|---|---|---|---|---|---|---|---|---|
| 7.999 / 8 kHz | not engaged | sample peak | 0 | worst sample over the live Ceiling **+0.0000** | 0 | 0 | inter-sample up to +26.1 dB (not claimed) | **holds (sample peak)** |
| 11.025 / 11.999 kHz | not engaged | sample peak | 72 each (TP re-engaged after a re-prepare to 12 kHz) | SP +0.0000; the 72: +0.0000 / +0.0000 | 0 | 0 | as above | **holds (sample peak)** |
| 12 kHz | engaged | dBTP | 1286 | +0.0075 / +0.0099 | 0.1 | 0 | splice ≤ +0.4064 / +0.6348; aedge ≤ +0.2548 / +0.3937 | **holds** |
| 16 kHz | engaged | dBTP | 1287 | +0.0201 / +0.0074 | 0.1 | 0 | splice ≤ +0.6640 / +0.9069; aedge ≤ +0.1935 / +0.3172 | **holds** |
| 22.05 kHz | engaged | dBTP | 1287 | +0.0005 / +0.0054 | 0.1 | 0 | splice; aedge ≤ +0.1098 / +0.2107 | **holds** |
| 24 kHz | engaged | dBTP | 1286 | +0.0008 / +0.0040 | 0.1 | 0 | splice ≤ +0.9223 / +1.0669; aedge ≤ +0.1040 / +0.1435 | **holds** |
| 32 kHz | engaged | dBTP | 1287 | +0.0112 / +0.0060 | 0.1 | 0 | splice; aedge ≤ +0.0886 / +0.1405 | **holds** |
| 44.1 kHz | engaged | dBTP | 1286 | +0.0007 / +0.0027 | 0.1 | 0 | splice; aedge ≤ −0.0041 / +0.0714 | **holds** |
| 48 kHz | engaged | dBTP | 1286 | +0.0005 / +0.0031 | 0.1 | 0 | splice; aedge ≤ +0.0647 / +0.0906 | **holds** |
| 88.2 – 192 kHz | engaged | dBTP | 974 each | ≤ +0.0030 / +0.0018 | 0.1 | 0 | splice; aedge ≤ +0.0440 | **holds** |
| 352.8 – 768 kHz | engaged | dBTP | 974 each | ≤ +0.0004 / +0.0015 | 0.1 | 0 | splice; aedge ≤ +0.0011 | **holds** |

- **By path** (claimed, excl.): steady +0.0201 / +0.0099 (5997 renders); TP engaged mid-stream
  +0.0000; every lifecycle path (Force Max ± re-prepare, return to realtime, re-prepare, re-prepare
  at a new rate, reset, statistics RESET, OS switch, BYPASS) ≤ +0.0091 / +0.0099. **0 claimed
  readings over 0.1 dB in 22 778 renders.**
- **The splice class** (continuous stream, up to +0.9609 / +1.0669): a host-drawn boundary where the
  stream ends (a reset, re-prepare or offline entry at that instant reads the same), the fourth
  round's KI-024 route B class; at 44.1 kHz the fs/4 case reads +0.406427 / +0.634824 on `6ee9f29`
  and +0.406425 / +0.634822 on 0.2.15 — pre-existing, not a steady-state figure. The
  "+0.0426 / +0.0380" of the round-4 table is the same splice at the other truncation phase, not a
  steady state (event-free at 48 kHz: +0.000163 / +0.000034).
- **Identity:** latency reported == impulse, TP on == TP off, in 22 778 / 22 778; non-finite samples
  0; statistics-RESET renders bit-identical to no-reset 1368 / 1368; engine dBTP tap == the harness's
  product meter (event-free, within 1e-6 dB) everywhere.
- **On the final tree:** the same 22 778 jobs against the final engine (this round's DSP-005 change
  included; the engine otherwise differs from `6ee9f29` only in comments) — **22 778 / 22 778
  renders bit-identical** to `6ee9f29`, reported latency == impulse in all. Every figure in the
  table above therefore holds for the final tree as measured, not by inference.

## 8. PREfast and CodeQL

- **PREfast on `6ee9f29`** (raw SARIF, artifact 11005844674, 195 results; the check run says 101 new
  in the PR, the annotation API returns 100): every first-party alert is under `tests/`, **0 in
  `src/`**. Exactly 7 C6262 are new since `8ab0532`, none gone (94 + 7 = 101): five test functions and
  two helpers they call (`lowrate::renderBurst`, `statsReset::drive`), 44 716 – 89 704 B each. The
  largest first-party frame is `testTeardownAndReengageInvariants` at 367 204 B (35 % of the 1 MiB
  Windows default stack), under the scanner audit's ~768 KB trigger; the Windows self-tests passed on
  `6ee9f29` (job 109195756093). **Classified:** G2 (test-only C6262, watched against its trigger) —
  no code change. The annotation byte figures are wrong for 59 of 100 alerts (not per-function); only
  the SARIF figures are quoted.
- **Corrections to earlier records** (not edited in place — dated records): the two null-dereference
  alerts at `state_tests.cpp` `findFirstChildOfType` / `collectCombos` are **C28182**, not C6011 (the
  only C6011 is `countSliders`, not in the PR's set) — G3, DO NOT FIX; "the 7 added are this round's
  new test functions" is five tests and two helpers.
- **This round's new tests** add fixtures by value in the same G2 pattern (`sessionClock::Rig` holds
  two engines; the published-length test two processors; `matchmon::settle` one engine); their
  PREfast figures are the records head's CI, recorded by the follow-up commit.
- **CodeQL:** `0d42384` raised one high-severity actions alert (cache poisoning through
  `workflow_dispatch` in `ki028-diag.yml`); fixed by removing the trigger (`c9e998f`); CodeQL's actions analysis on `839685d` and later heads
  succeeded. The records head's CodeQL result is the follow-up's.

## 9. DSP-005 / KI-023 — investigated, decided **Modify**, implemented (`86bfdf5`)

Started only after the round's other items had a disposition. Investigated on frozen worktrees of
`839685d` (the engine identical to `6ee9f29` but for comments): probe executables linked with
`juce_recommended_lto_flags`, Release, 48 kHz / 512, OS Off, TP off, Ceiling −0.1, the Loudness 70 %
constants of `matchmon::loudness70`; residual = input − matched, short-term at 10 s unless stated.

### 9.1 Problem and root cause

MATCH applies `min (measure, predict)`, `predict = −max (0, inputGain + limGain + expectedGR)`.
`expectedGR` was the limiter's deepest reduction over the previous call alone
(`AnabasisEngine.cpp`, the block-top predict). Whenever another stage takes level out, the floor
over-states the lift and `min` keeps it after the measure has converged: the matched signal sits
under the input — since ADR-0044 the whole BYPASS comparison gap.

- **Clip/Sat** — KI-023's claim confirmed: the larger term at the calibration point. At OS Off it is
  mostly the first-order ADAA kernel's cos(πf/fs) droop (KI-005 / DSP-004), not peak shaving: the
  stage's energy change on −17 dBFS pink is −0.40 dB at OS Off, −0.05 dB at 4×; at −12 dBFS −0.68 /
  −0.39 dB. Clip drive 0 removes 0.55 of 0.67 LU at −17 dBFS.
- **Compressor** — does not engage at −17 dBFS (GR exactly 0.00: "compressor idle" is identical to
  the baseline); −0.48 dB at −12 dBFS; dominant at heavy settings (threshold −24, ratio 4, limGain
  +6, −20 dBFS pink: **+3.00 LU** under).
- **Not in KI-023 — the limiter term's per-block swing on percussive programme:** the deepest-GR-per-
  block term swings −4.8 dB on hit blocks / −1.1 dB on tails, so predict swings −6.9 … −10.6 dB
  against a measure of −6.5 dB, and `min` plus the 200 ms smoother follow the deep values: **+2.91 LU**
  on synthetic drums (peak −1 dBFS, L70), **+0.94 LU** on a music-like bed. Structural to a
  stateless per-block floor under `min`.
- **The audit's literal fix is flawed twice over:** (1) reading the published `compGrDb` on the audio
  path would give the monitor gain a message-thread writer — `clearPublishedStageGr` writes it on
  RESET / state load / prepare, which is exactly why `grMinLinear` is kept out of that clear; (2) the
  compressor's full reduction, unweighted by Comp Mix, flatters parallel compression: +1.51 LU above
  the input before the measure exists at Comp Mix 50 %.
- **Comment drift:** the block-top comment said the deepest-GR choice made the floor "slightly more
  aggressive … the safe direction"; the deepest GR gives a SHALLOWER floor. Corrected in the change.

HEAD re-measured against KI-023's table: short-term reproduces exactly (+0.63 LU through
`matchJump` at the calibration point; +0.67 on the dry-meter harness); the momentary figures differ
by 0.1–0.15 LU, inside single-instant momentary noise on this pink (±0.3 LU — the measure-only
variant, whose true residual is ≈ 0, reads +0.26 at one instant and −0.01 averaged).

### 9.2 Alternatives, each prototyped on the same harness

| Option | −17 L70 | −12 L50 / L70 / L90 | Comp-heavy −20 | Comp Mix 50 %, predict-only max | Drums / music | D7 / inv 10 | Verdict |
|---|---|---|---|---|---|---|---|
| (a) Preserve | +0.67 | +0.89 / +1.52 / +2.18 | +3.00 | −1.19 | +2.91 / +0.94 | ok | the gap stays |
| (b) raw comp GR (the audit's form) | +0.67 | +0.80 / +1.12 / +1.75 | +0.18 | **+1.51 — flatters** | +2.90 / +0.94 | ok only if read from the stage | rejected |
| (b') comp GR × Comp Mix | +0.67 | +0.80 / +1.12 / +1.75 | +0.18 | +0.06 | +2.90 / +0.94 | ok | half the fix |
| (c) clip: measured energy out / in | +0.32 | +0.40 / +0.82 / +1.01 | +3.00 | −1.19 | +2.83 / +0.93 | ok (one previous-block figure, as `grMinLinear`) | half the fix |
| **(d') (b') + (c)** | **+0.31** | **+0.33 / +0.43 / +0.60** | **+0.18** | **+0.06** | +2.82 / +0.92 | **ok** | **chosen** |
| (e1) measure only once short-term is valid | +0.05 | ≈ +0.05 | +0.10 | — | +0.02 / +0.06 | **conflicts with D7** (no pre-duck after 3 s): +4.63 LU over the input for 2.4 s after a macro jump | rejected — hard stop |
| (e2) measure only | +0.05 | ≈ 0 | +0.10 | — | ≈ 0 | **conflicts with D7**: +9.5 LU over the input at an onset | rejected — hard stop |
| (e3) (d') + a held GR (3 dB/s release) | +0.06 | +0.08 / +0.25 / +0.21 | +0.15 | +0.06 | +0.12 / +0.06 | **conflicts with D7** (cross-block state): +3.3 LU over at an onset | rejected — hard stop |
| (f) measured pre-limiter loss | as (d') | | | | | ok | rejected: unweighted energy mis-reads EQ (+6 dB low shelf → +2.61 LU, HEAD +0.37) |
| drive-derived clip estimate | not built | | | | | ok | rejected: the loss depends on the programme, not the drive (KI-005: the droop is identical at 0.07 and 3 dB drive; 1 kHz sine −0.02 dB vs pink −0.40 dB) |

(d') elsewhere: OS 4× −17 L70 / −12 L70 / −12 L90 +0.08 / +0.09 / +0.24; Colour Tape 100 % +0.29
(HEAD +0.44); EQ-pre −6 dB high shelf +1.07 (HEAD +1.25 — the EQ is not counted); EQ-pre +6 dB low
shelf +0.12 (HEAD +0.37); comp-heavy silence → onset, drums / music-like +0.58 / +0.29 (HEAD +1.74 /
+1.47). While only the predict floor acts (the first 3 s), pink −12 settles at −4.1 … −4.4 dB
(HEAD −5.2 … −5.3) and stays below the input throughout; drums and music follow HEAD within ~0.3 dB.
The floor's per-block modulation on drums: 1.36 dB standard deviation vs HEAD 1.32.

**"Attenuation-only"** holds in every run of every variant (the applied gain never exceeded
unity). **"Never louder than the input"** does not hold strictly at HEAD either, and did not before
this change: after a macro jump (the 200 ms monitor ramp against the 20 ms limGain ramp) the
matched signal exceeds the input by up to +1.9 LU momentary for 0.41 s (0.50 s with (d')), and after
a prepare with audio in the first block, see §9.5.

### 9.3 Decision: **Modify**

The audit's shape is right — extend decision 7's "expected GR", keep the floor stateless and
attenuation-only — and its content is corrected three ways: the clipper term is kept (it is the
larger term at the default OS Off); the compressor term is weighted by Comp Mix; it is read from the
stage (audio thread only), not from the published atomic. **Not a hard stop:** ADR-0006 decision 7
reads "input gain + limiter gain − expected GR" and names no stage, so extending "expected GR" is
within its text; no parameter, serialization, threading, signal-order, latency or macro contract
changes. It does change what is HEARD on ADR-0044's path (⊕), and it reads decision 7's "stateless"
as "no figure older than the previous block" (`clipLevelDb`, like `grMinLinear`) — both flagged ⊕ for
the owner. **KI-023 stays open**, re-scoped to the percussive term and the uncounted EQ: the two
designs that remove the percussive term (e1, e3) each need a decision-7 amendment and each flatters by
several LU for seconds.

### 9.4 Implementation and regression test

- **Code** (`AnabasisEngine.{h,cpp}`, audio thread only): at the block top
  `expectedGR = grDbNow + compDbNow + clipLevelDb`, where `compDbNow` is the gain of
  `1 − m + m · 10^(compGR/20)` in dB (m = Comp Mix, `compGR` read from `comp` — the stage — not from
  `compGrDb`) and `clipLevelDb` is `10·log10 (Σ out² / Σ in²)` across the Clip/Sat stage over the
  previous call (accumulated per region frame in `processChunk`, folded per call; exactly 0 dB when
  the stage passes every sample through, 0 dB for a silent call, clamped to [−24, +6] dB, a
  non-finite figure dropped). One `float` and two `double` members; no atomic, no new cross-thread
  path, no parameter, no state. Offline the monitor gain is snapped to unity as before.
- **Test** `testMatchPredictCountsEveryLevelTakingStage` (DSP suite, 4 checks): −17 dBFS pink at
  Loudness 70 % settles within 0.45 LU; a compressor-heavy setting at −20 dBFS within 0.5 LU; at Comp
  Mix 50 % after 2 s of silence the matched signal is never 0.75 LU above the input while the predict
  floor acts alone, and settles within 0.5 LU.
- **Verified here, independently of the investigation** (fresh worktrees of `5562229`, Release):
  the test alone on the unchanged engine — **773 checks, 3 failures** (+0.67 / +2.97 / +1.36 LU); with
  the change — **773 / 773** and the state suite **1638 / 1638**; `matchJump` +0.63 → **+0.27 LU**;
  three mutants each killed by its own check (Clip/Sat term removed: check 1, +0.67; compressor term
  removed: checks 2 and 4, +2.97 / +1.36; the compressor term unweighted by Comp Mix: check 3,
  +1.50 LU above the input); the source restored byte-identical and passing. No first-party warning
  (the build's only notes are JUCE's vendored splash-screen `#pragma message`). `check-realtime`
  0 violations, `check-portability` 0, `check-docs` clean.
- **Renders:** the full 22 778-render true-peak matrix (§7) re-run against this change's engine and
  compared render by render with `6ee9f29`: **22 778 / 22 778 bit-identical**, reported latency ==
  impulse in every one. The matrix runs with MATCH off, so it pins what the change must NOT move
  (every rendered sample, TP on and off, every path and rate); what it moves is pinned by the test
  above and by the investigation's offline MATCH-on-vs-off identity.
- **CPU** (the investigation's LTO probe, three alternating rounds, best of 7, ns per stereo frame at
  48 kHz, pink −12 dBFS, L70): 297 / 308 / 307 before, 307 / 309 / 295 after; OS 4× 691 / 694 / 706
  vs 689 / 714 / 697 — within this shared host's noise. Latency: no delay touched; the latency tests
  pass unchanged.

### 9.5 Found on the way, recorded, not changed (not part of this change)

- **F1 — the monitor gain is the one smoother not primed on the first block after a prepare**
  (`monitorGain` starts at unity and ramps to its target over 200 ms): with audio in that first block,
  MATCH on, the matched signal plays **up to +9.8 LU over the input for ~0.2 s** (drums from t = 0).
  Priming it (a prototype, not applied) leaves +0.3 LU; the first 10 ms of output are the empty
  lookahead line's zeros, so the step would be inaudible. Recorded as KI-029.
- **F2 — after a macro jump** the matched signal exceeds the input by up to +1.9 LU momentary for
  up to 0.41 s at HEAD (0.50 s with this change), from the monitor's 200 ms ramp against limGain's
  20 ms. Recorded with KI-029.
- **MATCH never raises the processed leg** (decision 7: attenuation only), so a setting whose render
  is quieter than the input keeps that gap in full (−12 dBFS pink, threshold −24, ratio 4, limGain 0:
  +8.99 LU in every variant). By design; stated in KI-023.

## 10. Gates and CI

Local, on the final code (`86bfdf5`'s tree; the records commit changes documents and the version
number only):

- **Suites** (Release; the test targets do not link the LTO flags, as in CI's `linux` job):
  `AnabasisTests` **773 / 773**, `AnabasisStateTests` **1638 / 1638** (under Xvfb). The round's
  additions: DSP +32 (session clock) +4 (DSP-005) over 737; state +22 (OQ-020) +11 (session length)
  over 1605.
- **Mutation:** OQ-020 3 / 3 killed; the session clock 10 / 10; DSP-005 3 / 3 (re-run independently of
  the investigation).
- **True-peak matrix:** 22 778 / 22 778 renders bit-identical to `6ee9f29`, latency identity in all
  (§7, §9.4).
- **Checks:** `check-docs` 139 files clean; `check-realtime` 42 files, 0 violations, 1 / 1 ordering
  requirement; `check-portability` 50 files, 0 violations; `check-citations` clean against `5562229`,
  `6ee9f29`, `ed06ad0` and `origin/main` after re-anchoring 10 anchors (each read) and pinning one in a
  dated record.
- **Not run locally:** pluginval, RealtimeSanitizer, valgrind, PREfast, CodeQL — GitHub CI's (§10.1).

### 10.1 GitHub CI

- **`6ee9f29`** (the start): push run 36502299990 all success; pull-request run 36502303279 success.
- **`0d42384`**: push run 36520602785 all success (`macos-intel` AU randomise ×3 included);
  pull-request run 36520606657 success; CodeQL one new high-severity actions alert (the diagnostic's
  `workflow_dispatch`), fixed in `c9e998f`.
- **`839685d`** (with `c9e998f`) and **`9a19b02`**: push runs 36533479182 and 36535966626 cancelled by
  the next push (the concurrency group); their pull-request runs 36533485002 and 36535972581 success.
- **`5562229`**: push run 36537705175 — `docs`, `preflight`, `source-lint`, `linux`,
  `linux-lto-tests`, `linux-lto-clang`, `realtime`, `windows`, `macos`, `macos-intel` success
  (`macos-intel`'s AU randomise ×3 passed; KI-028 did not recur on its CI seeds) and `sanitizers`
  (ASan / UBSan, valgrind on both suites) — **every job success**; pull-request run 36537713446
  success. DSP-005 was committed on this green head.
- **The KI-028 diagnostic** (not a gate): runs 36520602892 (Intel jobs cancelled at the timeout after
  reproducing; arm64 green), 36533479226 (pr-head red — reproduced; control stopped at its DWARF
  check), 36535966698 (cancelled by the next push), 36537705309 (control **green**, 14 / 14; pr-head
  recorded by the follow-up).
- **The records head `43d1bbc`** (recorded by the follow-up commit): push run 36541576163 — `docs`,
  `preflight`, `source-lint`, `linux` (pluginval ×3 both modes), `linux-lto-tests`, `linux-lto-clang`,
  `realtime`, `sanitizers` (ASan / UBSan, then valgrind on both suites: **success**), `windows`,
  `macos` and `macos-intel` (AU randomise ×3 included) — **every job success**; pull-request run
  36541582708 `merge-check` success; CodeQL "No new alerts in code changed by this pull request";
  dependency review success; PREfast success, "107 new alerts" — raw SARIF (artifact
  `prefast-sarif-9e56ba5…`): 201 results, C6262 186 / C28182 13 / C6011 1 / C26495 1 (JUCE), 0 in
  `src/`; the six added since `6ee9f29` are this round's tests (C6262: the session-clock test
  177 936 B, `matchmon::settle` 73 864 B, the tooltip test 46 188 B; C28182: the tooltip test's three walk
  helpers, G3); largest frame unchanged, `testTeardownAndReengageInvariants` 367 332 B.
- **The KI-028 diagnostic, run 36537705309's pr-head job** (on `5562229`, finished after the records
  were written): **13 of 14** passes aborted with the same AudioToolboxCore stack; **0 Anabasis frames
  in all 8 crash reports** (every thread). Pass r014 (fast, variant B) never exited after `SUCCESS`:
  the watchdog's `sample` shows pluginval's main thread idle in `-[NSApplication run]`'s event wait,
  the NSEvent thread and one idle worker, nothing else — the validator thread gone, no Anabasis code
  running. It is the third such hang (run 36520602892 had one per Intel job), all three in variant B
  (pluginval's crash handler refused, the interposer's own installed); none in variant A or in any CI
  stock lane. Recorded in KI-028 as a diagnostic-only observation; not investigated further.

## 11. Corrections to earlier records

- Round 4's worklog says the reported latency equals the measured impulse delay except Force Max
  (reported + 1). This round's harness measured, on a fresh engine at each render's final
  configuration (EQ flat): OS off and linear phase → impulse argmax − reported = 0; **minimum-phase
  oversampling → 0 or 1** (2×, 8× and 16× read + 1, 4× reads 0). The argmax of a minimum-phase
  response is not its group delay, so this is a property of the measurement, not a latency error;
  the round-4 sentence is incomplete. Not edited in place.
- PREfast rule and count (§8).
- ADR-0046's "the splice render is bit-identical in 0.2.15": the splice readings agree within 2e-6
  dB; whole-render hashes differ (TP-on output changes wherever the clamp acts); sample identity
  around the splice alone was not checked.

## 12. Not verified

- **Listening, hosts:** nothing was heard; no DAW was run below 44.1 kHz or across the tooltips'
  change of rate; the tooltips were driven headlessly (state suite, Xvfb).
- **An all-input true-peak bound:** the matrix and searches give lower bounds on the worst case.
- **KI-028's last step** — which event was queued, and Apple's ordering inside AudioToolboxCore —
  inferred from the stack and the host's source; whether a real host disposes its AU listener the
  same way (no host run); why arm64 does not reproduce.
- **DSP-005 on programme material** — the MATCH figures are pink noise, synthetic drums and a
  music-like bed; the investigation's MSVC / Clang / arm64 behaviour of the new test is CI's.
- **KI-029's prototype fix** was measured, not applied.
