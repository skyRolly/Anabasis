# Anabasis — PR #42, fifth review round: ADR-0046 ratified, the low-rate contract, OQ-020, KI-028, the session clock, DSP-005

**Date:** 2026-09-29 · **Version:** 0.2.17 (unreleased) · **Class:** dated record in `docs/reports/` —
a snapshot, superseded by a later record rather than edited in place (`docs/SOURCE_OF_TRUTH.md`).

The closure record for the round that followed the fourth review round's head `6ee9f29` (0.2.16).
Scope, by the owner's brief of 2026-09-29: re-verify the two Devin findings the fourth round fixed
(no redesign); accept ADR-0046 and make its amended records unambiguous; state the low-rate
true-peak contract; resolve OQ-020; give KI-028 exactly one disposition; settle what the session
length counts (a new Devin comment); verify the true-peak contract over every rate and path and the
CI; classify PREfast; then — and only then — investigate DSP-005 / KI-023 and decide it. No UI
roadmap expansion, no merge, Anamorph untouched. The evidence is
[`worklogs/2026-09-29-pr42-round5-contract-ki028-clock.md`](../../worklogs/2026-09-29-pr42-round5-contract-ki028-clock.md);
the fourth round's record ([`2026-09-28-pr42-round4-closure.md`](2026-09-28-pr42-round4-closure.md))
is not edited beyond its forward pointer and one pinned anchor.

Each row keeps apart what was FOUND (by Devin, by this round), what was IMPLEMENTED, what was
VERIFIED, what was DECIDED and what REMAINS.

## 1. The review, item by item

| Item | Status | Found | Implemented | Verified | Evidence |
|---|---|---|---|---|---|
| **Devin A** — "Low-rate ceiling cuts exceed dBTP limit" (`CeilingClamp.h`) | **FIXED — verified** (fourth round; no redesign) | — | — (ADR-0046, `4ff71bd` / `f03d673`) | its five regression tests pass on the final tree; the KI-025 bursts rebuilt independently of the test reproduce 0.2.15's failures exactly and hold on the head; the full matrix (§3) has no claimed reading over 0.1 dB at any engaged rate | worklog §1, §7 |
| **Devin B** — "Old peaks survive statistics reset" (`AnabasisEngine.h`) | **FIXED — verified** (fourth round) | — | — (`0f162c8` / `f03d673`) | its two regression tests pass; 1368 matrix renders with a RESET are bit-identical in their audio to the same render without it; the session-clock probe reproduced the 6-reading TP skip and the 100–200 ms guard exactly | worklog §1, §5 |
| **Devin — session clock** (`LoudnessMeter.h:205`, `setSessionPaused`): the length counts audio that I / LRA leave out after a bypass resume | **Not a defect — option A kept (the open frames), pinned** | the length counts every open frame; I / LRA leave up to 0.1 s before and 0.1–0.2 s after an audition out, as after a RESET | no product code; `USER_MANUAL` §3.4 states it; an ADR-0020 implementation note (not an amendment, ⊕) | clock − open frames = 0 in every scenario at 44.1 / 48 / 96 kHz; two new tests (+32 / +11 checks); ten mutants killed, the move toward option B among them | `0d42384`; worklog §5 |
| **ADR-0046** | **Accepted — RATIFIED at the Architecture Review Gate** (the owner's approval, recorded in the four places, `e4f9205`'s convention) | D was 38–45 below 62 kHz before (not "38–42 up to 48 kHz"): the TP-mode window is shorter than 0.2.15's from 12 kHz to below 62 kHz and longer at 3901–11 999 Hz | ADR-0046's ratified banner and ratification note (a)–(d); dated notes on ADR-0041, ADR-0015, ADR-0004, ADR-0045 (which stays ⊕); stale wording removed from living text; history kept | every constant against the code; reported latency TP on == TP off == the engine's composition over 26 rates × OS 1–16× × both phases × Force Max; 22 778 renders | `d7174ef`, `e7b6f4e`; worklog §2 |
| **The low-rate contract** | **Stated** (§2) | KI-025 item (2) and three ledgers still read as if TP were simply unavailable below 12 kHz | `DSP_POLICY` invariants 3 / 4 / 8, `USER_MANUAL`, `COMPATIBILITY_MATRIX`, KI-025, RISK-003, `PARAMETER_REGISTRY` | the sample-peak promise below the rail in 5177 renders (worst sample +0.0000 dB over the live Ceiling) | `d7174ef`, `e7b6f4e` |
| **OQ-020** — whether the editor says more than the unit below 12 kHz | **Resolved, option 2** (the owner's brief); wording ⊕ for the owner's fine review | — | below 12 kHz the TP and Ceiling tooltips name the boundary; unchanged from 12 kHz up; one decider shared with the unit; no other UI string | `testTheTruePeakTipsFollowTheRateTheTruePeakPathEngagesAt` (+22); three mutants killed | `c194000`; worklog §4 |
| **KI-028** — pluginval's teardown crash | **Dispositioned: PRE-EXISTING / EXTERNAL** (§4) | the macOS abort's throw site is in Apple's AudioToolboxCore, invoking a disposed host listener's emptied callback; 0 Anabasis frames; a JUCE-example control never aborts | exit 9 on macOS reported as `CRASHED` (the step still fails); a non-gating diagnostic workflow; no product change, no retry raised | four diagnostic runs; three crash reports; a causal Linux test (5 / 567 vs 0 / 567) | `d006984` … `5562229`, `d52570f`; worklog §6 |
| **DSP-005 / KI-023** | **Decided: Modify — implemented**; KI-023 partly fixed, re-scoped, open (§5) | the predict floor counted the limiter alone; the audit's literal fix read a message-thread-written atomic and flatters parallel compression; the percussive residual cannot be removed inside ADR-0006 decision 7 | expected GR = limiter + compressor × Comp Mix + Clip/Sat measured loss | the new test fails 3 on 0.2.16, passes after; three mutants killed; 22 778 / 22 778 renders bit-identical | `86bfdf5`; worklog §9 |
| **Found on the way — KI-029** | **Recorded, not changed** | MATCH plays the processed signal up to +9.8 LU over the input for ~0.2 s after a prepare with audio in the first block (the monitor gain is not primed), and up to +1.9 LU for up to 0.4 s after a macro jump | — | measured by the DSP-005 probe | `KNOWN_ISSUES.md` KI-029 |
| **PREfast** | **Classified: G2 / G3 — no code change** | the round-4 records mislabel two C28182 as C6011 and count "7 new test functions" (five tests and two helpers) | recorded here and in the worklog (dated records not edited) | raw SARIF on `6ee9f29`: 195 results, 0 in `src/`, largest frame 367 204 B (35 % of 1 MiB) | worklog §8 |

## 2. The true-peak contract by rate — the final engine

The 22 778-render matrix of `6ee9f29`, re-run against the final engine (this round's DSP-005 change
included): **22 778 / 22 778 renders bit-identical**, so these figures are the final tree's as
measured. PM / A2 = product meter / BS.1770 Annex 2, over the live smoothed Ceiling, tolerance
**0.1 dB, unchanged**. "Claimed" = readings the contract covers; excl. = ±16 samples around a
host-drawn splice removed.

| Rate | TP engaged? | Guarantee | Worst claimed excess (PM / A2) | Tolerance | Result |
|---|---|---|---|---|---|
| 7.999 / 8 / 11.025 / 11.999 kHz | **no** | **sample peak**: every sample at or under the Ceiling, TP on or off; the Ceiling reads `dB` | worst sample over the live Ceiling **+0.0000 dB** (5177 renders below the rail); inter-sample readings up to +26.1 dB — **not claimed** | 0 (sample) | **holds (sample peak)** |
| 12 kHz | yes | dBTP | +0.0075 / +0.0099 | 0.1 dB | **holds** |
| 16 kHz | yes | dBTP | +0.0201 / +0.0074 | 0.1 dB | **holds** |
| 22.05 / 24 / 32 kHz | yes | dBTP | ≤ +0.0112 / +0.0060 | 0.1 dB | **holds** |
| 44.1 / 48 kHz | yes | dBTP | ≤ +0.0007 / +0.0031 | 0.1 dB | **holds** |
| 88.2 – 192 kHz | yes | dBTP | ≤ +0.0030 / +0.0018 | 0.1 dB | **holds** |
| 352.8 – 768 kHz | yes | dBTP | ≤ +0.0004 / +0.0015 | 0.1 dB | **holds** |

- **Paths** (claimed, excl.): steady +0.0201 / +0.0099; TP engaged mid-stream +0.0000; Force Max
  entry with and without re-prepare, return to realtime, re-prepare, re-prepare at a new rate,
  reset, statistics RESET, OS switch, BYPASS audition: each ≤ +0.0091 / +0.0099. **0 claimed readings
  over 0.1 dB.**
- **Not claimed, recorded:** the splice class (a host-drawn boundary where the stream ends: up to
  +0.9609 / +1.0669 on the continuous stream, the fourth round's KI-024 route B, identical on 0.2.15
  within 2e-6 dB); a BYPASS audition's crossfade edges (the input is audible there; up to +0.2548 /
  +0.3937 at 12 kHz, falling with rate).
- **Below 12 kHz, stated in the user documents:** the Ceiling is a sample-peak ceiling whatever the
  TP switch says, it reads `dB`, and the TP switch still moves the limiter's detector at
  Oversampling Off / 2× — a best effort, not dBTP protection. The tooltips name the boundary
  (OQ-020). Sample clipping is not described as true-peak protection anywhere, and dBTP is not
  claimed below the rail.
- **Latency:** reported == impulse, TP on == TP off, in every render; the engine's dBTP tap agrees
  with the product meter within 1e-6 dB on every event-free render; non-finite samples 0.

## 3. KI-028 — the disposition

**PRE-EXISTING / EXTERNAL.** On `macos-15-intel` (macOS 15.7.9) with seed `0x5161f59`, pluginval
1.0.4 aborts after `SUCCESS` because a block on its main run loop runs AudioToolboxCore's
`AUParameterListener` callback after the host (pluginval's JUCE 8.0.3 `AudioUnitPluginInstance`,
whose teardown disposes its event listener first — JUCE 9.0.1 has the same order) has disposed it:
an empty `std::function`, `std::bad_function_call`. No thread holds an Anabasis frame; the plug-in
instance is already deleted; its teardown notifies nothing. The plug-in's part is legitimate event
traffic — a parameterless JUCE example AU, same pluginval, seed and test order, **never aborts
(0 / 14)** against Anabasis's 4 / 14. `main` aborts as often as the head; arm64 never (0 / 100). The
Linux crashes are pluginval's JUCE 8.0.3 host too (5 / 567 as released vs 0 / 567 with JUCE
`04e167d64` ported). **Not changed:** the product, the gate (no crash-retry on macOS, no retry count
raised, the step still fails). A red macOS AU pass ending `SUCCESS` then this exact
`bad_function_call` line is KI-028 and is re-run and recorded; any other signature is new. Not
established: which event was queued, Apple's internal ordering, why arm64 does not reproduce.

## 4. The session clock

The STATISTICS length counts the open frames — every frame with no part of a realtime bypass
audible, and every offline frame — from the first open frame after a resume or RESET; those are the
frames the SP / TP holds take. I and LRA are gated block measurements whose watermark leaves up to
0.1 s before and 0.1–0.2 s after an audition out, as after a RESET, so the length counts a fraction
of a second per audition that I does not; LRA moves again ~3 s after it. **Kept** (ADR-0020
amendment 4 item 3), stated in the manual, pinned by two tests. Moving the length to "the audio I
admits" would break the recorded RESET contract and the holds' correspondence and stall the display
during fast A/B toggling; that quantity is not a duration.

## 5. DSP-005 / KI-023 — the decision

**Modify.** Investigated first on frozen worktrees (probe on the real engine, LTO; residual =
input − matched):

| | 0.2.16 | 0.2.17 |
|---|---|---|
| calibration point (−17 dBFS pink, Loudness 70 %) | +0.63 LU | **+0.27 LU** |
| −12 dBFS, Loudness 50 / 70 / 90 % | +0.89 / +1.52 / +2.18 | +0.33 / +0.43 / +0.60 |
| compressor-heavy, −20 dBFS / at Comp Mix 50 % | +3.00 / +1.37 | +0.18 / +0.07 |
| synthetic drums / music-like bed | +2.91 / +0.94 | +2.82 / +0.92 (open) |

- **Root cause:** expected GR counted the limiter alone; the Clip/Sat stage's loss (at OS Off mostly
  the ADAA droop) is the larger term at the calibration point, the compressor at heavy settings.
- **Rejected:** the audit's literal form (the compressor's raw GR read from the published atomic —
  a message-thread writer; and unweighted, +1.5 LU above the input at Comp Mix 50 % before the
  measure exists); a drive-derived clip estimate (the loss follows the programme, not the drive); a
  measured pre-limiter loss (mis-reads EQ); measure-only, handover and held-GR designs (each removes
  the percussive term, each conflicts with ADR-0006 decision 7, each plays the matched signal several
  LU over the input for seconds — hard stops).
- **Implemented** (`86bfdf5`): expected GR = limiter + compressor × Comp Mix (read from the stage)
  + Clip/Sat measured level change, all previous-block figures on the audio thread. Within decision
  7's text; no parameter, state, latency, signal-order or threading change; CPU within noise; the
  render and the invariant-7 null unchanged (22 778 / 22 778 renders bit-identical).
- **Tests:** `testMatchPredictCountsEveryLevelTakingStage` fails 3 of 773 checks on 0.2.16 and passes
  after; each of three mutants fails its own check.
- **Remains:** KI-023 open for the percussive term (needs a decision-7 amendment — an owner decision)
  and the uncounted EQ; MATCH never raises the processed leg (by design); KI-029.

## 6. CI — the actual result

- **`6ee9f29`** (the start): push run 36502299990 — every job **success**; pull-request run
  36502303279 success; CodeQL no new alerts; PREfast "101 new" (all `tests/`, §1).
- **`0d42384`** (ADR-0046, OQ-020, review fixes, the KI-028 diagnostic, the session clock): push run
  36520602785 — every job **success**, `macos-intel`'s AU randomise ×3 included; pull-request run
  36520606657 success. **CodeQL raised one high-severity actions alert**: cache poisoning through
  the diagnostic workflow's `workflow_dispatch` trigger — fixed by removing the trigger (`c9e998f`).
- **`839685d`, `9a19b02`** (the diagnostic's own fixes): their build runs were cancelled by the next
  push (the workflow's concurrency group); no job failed.
- **`5562229`** (the last pre-DSP-005 head): push run 36537705175 — `docs`, `preflight`,
  `source-lint`, `linux` (pluginval ×3 both modes), `linux-lto-tests`, `linux-lto-clang`,
  `realtime`, `sanitizers` (ASan / UBSan and valgrind on both suites), `windows`, `macos` and
  **`macos-intel` (AU randomise ×3 included)** — **every job success**; pull-request run 36537713446
  success. This is the green head DSP-005 was committed on.
- **The diagnostic workflow** (never a gate): red by design on the pr-head jobs that reproduced
  KI-028; the control job green.
- **`43d1bbc`** (DSP-005 and these records; recorded by the follow-up commit, as `6ee9f29` recorded
  `fe29bda`'s): push run 36541576163 — `docs`, `preflight`, `source-lint`, `linux` (pluginval ×3 both
  modes), `linux-lto-tests`, `linux-lto-clang`, `realtime`, `sanitizers` (ASan / UBSan, then valgrind
  on both suites: **success**), `windows`, `macos` and `macos-intel` (AU randomise ×3 included) —
  **every job success**; pull-request run 36541582708 `merge-check` success; **CodeQL: "No new alerts
  in code changed by this pull request"**; dependency review success; **PREfast** success with "107
  new alerts" (raw SARIF: 201 results — C6262 186, C28182 13, C6011 1, C26495 1 in JUCE — **0 in
  `src/`**). The six added since `6ee9f29` are this round's test code: C6262 on
  `testTheSessionClockCountsTheOpenFramesNotTheAdmittedAudio` (177 936 B — two engines),
  `matchmon::settle` (73 864 B) and `testTheTruePeakTipsFollowTheRateTheTruePeakPathEngagesAt` (46 188
  B), and C28182 on the tooltip test's three component-walk helpers (the scanner audit's G3
  `dynamic_cast` pattern, DO NOT FIX). The largest frame is still `testTeardownAndReengageInvariants`
  (367 332 B), under the ~768 KB trigger; the Windows self-tests passed. Classified G2 / G3, no code
  change.
- **The KI-028 diagnostic's last run** (36537705309 on `5562229`): the control 14 / 14 clean; the
  pr-head job reproduced in **13 of 14** passes, every one the same AudioToolboxCore stack, **0
  Anabasis frames in all 8 crash reports**. One pass never exited after `SUCCESS` (sampled by the
  watchdog: pluginval's main thread idle in its run loop, nothing else running, no Anabasis code) —
  the third such hang, all three in the diagnostic's handler-refusing variant B, none in CI's stock
  lanes; recorded in KI-028, not investigated further.

## 7. Roadmap

**Completed this round:** Devin A / B verified; ADR-0046 ratified; the low-rate contract stated;
OQ-020 resolved; KI-028 dispositioned; the session clock settled and pinned; the true-peak contract
verified on the final engine; PREfast classified; DSP-005 implemented (KI-023 partly fixed).

**Next, re-evaluated** (no UI roadmap expansion this round; the order of the Phase 1 roadmap's §2
stands with DSP-005 done):

1. **VIS-010 + UX-010's cue** — the monitor-state indicator's place (a signed surface) and whether it
   carries the MATCH gain (a new published scalar: a Thread Model gate item).
2. **KI-029** — prime the monitor gain on the first block after a prepare (measured to remove the
   +9.8 LU start transient); a MATCH behaviour change on ADR-0044's ⊕ path, so the owner's call
   before it is scheduled.
3. **TEST-004, then VIS-005 / VIS-012**; then **VIS-014, STATE-008, VIS-004**; then **UX-023, UX-008,
   VIS-013**.
4. **Recorded, not scheduled:** KI-023's percussive term (a decision-7 amendment), KI-026, KI-027,
   KI-024 D / Dd.

## 8. Owner decisions carried

| Decision | Record |
|---|---|
| ADR-0045, ADR-0044 (now with DSP-005's dated note) and ADR-0020 amendment 4 (now with the session-length note) — ⊕ | the ADRs |
| DSP-005's change to what MATCH plays, and its reading of decision 7's "stateless" as "no figure older than the previous block" | ADR-0044's dated note; KI-023 |
| KI-023's percussive term: amend ADR-0006 decision 7 (a held GR or a handover) or accept it | KI-023 |
| KI-029: prime the monitor gain, and whether the post-jump excess needs a matched ramp | KI-029 |
| KI-028: whether its exact signature may be re-run past at a release gate, or blocks until pluginval ships a host that does not dispose a listener with events queued | KI-028 |
| OQ-020's words (⊕ — "sample rate(s)", the Ceiling tooltip's inclusion) | `OPEN_QUESTIONS.md` OQ-020 |
| An erratum in the round-4 records for the C28182 / C6011 label and the "7 new tests" count | this record §1; worklog §8 |
| Carried: KI-024 D / Dd (KI-004); KI-026; KI-027; OQ-019; STATE-002 (KI-021); UX-003 (KI-022); VIS-002 (KI-020); ADR-0042 option A / KI-007; the TP clamp's voicing constants (listening) | as recorded |

## 9. Evidence limits

Nothing was listened to. No DAW was run: not below 44.1 kHz, not across the tooltips' change of
rate, not with MATCH, not for KI-028 (whether a real host disposes its AU listener the same way is
unknown). Every true-peak figure is the real engine on synthetic programme, constructed bursts and
searched vectors, on two meters; searches bound the worst case from below. The KI-028 mechanism's
last step (Apple's code) is inferred from its stack and the host's source. The MATCH figures are
pink noise and synthetic drums / a music-like bed, not programme material. PREfast, CodeQL,
RealtimeSanitizer, valgrind and the Windows / macOS pluginval lanes are GitHub CI's, not local.
