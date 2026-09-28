# Anabasis — PR #42, third review round: the true-peak contract's open paths, GR max, the copy record

**Date:** 2026-09-28 · **Version:** 0.2.15 (unreleased) · **Class:** dated record in `docs/reports/` —
a snapshot, superseded by a later record rather than edited in place (`docs/SOURCE_OF_TRUTH.md`).

> **Followed by [`2026-09-28-pr42-round4-closure.md`](2026-09-28-pr42-round4-closure.md) (0.2.16).**
> The two true-peak paths §2 and §4 leave open are settled there: KI-025 is closed by ADR-0046 at
> every rate the path engages — from 12 kHz, a stated rail; below it TP mode is not available — and
> KI-024 is dispositioned (route C fixed; its "host `reset()`" premise corrected). OQ-018 is
> resolved as this record's interim (option 1). The rows below stand as the state at `8ab0532`.

The closure record for the review findings raised against PR #42 at head `dd983ec` (0.2.14): two
true-peak correctness defects, the GR max readout's contract, and the STATISTICS tooltip's copy —
plus the CI failure found on the way. Scope, by the owner's instruction: a correctness and
review-closure round, no Phase 1 expansion, no merge. The evidence is
[`worklogs/2026-09-28-pr42-review-tp-contract.md`](../../worklogs/2026-09-28-pr42-review-tp-contract.md);
the earlier rounds' records ([`2026-09-27-phase0-closure.md`](2026-09-27-phase0-closure.md),
[`2026-09-27-phase1-roadmap.md`](2026-09-27-phase1-roadmap.md)) and the 2026-09-26 audit are not
edited.

## 1. The review, item by item

| Item | Status | Root cause | Implementation | Tests | Evidence |
|---|---|---|---|---|---|
| **Offline renders inherit engagement audio** (`AnabasisEngine.cpp`) | **Fixed** | The offline-entry branch forced the §2.8 duck to unity but left a TP engagement's decay running, so the last realtime frame's decay played into the head of the render, summed onto the unducked path | The branch resets the decay with the duck (history included); the output dBTP tap restarts when it cut one | `testOfflineEntryDropsTheEngagementTail` (63 checks; 20 fail with the fix removed, 4 with the tap restart removed) | unfixed: up to 256 samples of decay at 48 kHz, +1.46 dB over the ceiling (both meters, and the engine's own dBTP tap); fixed: exact silence where only the decay could sound, 0 over; 480 other renders bit-identical |
| **Descending ceiling leaks delayed peaks** (`CeilingClamp.h`, `AnabasisEngine.cpp`) | **Fixed at 44.1 kHz and above — ADR-0045, on the owner's direction, ⊕ for review; a residual below 44.1 kHz recorded (KI-025)** | TP mode delays its audio by D and judged every frame against the ceiling it carried IN, so while the 20 ms glide descended the output answered to a value D samples old | Each frame answers to the ceiling in force at its EMISSION: the smoother's glide run ahead exactly, frames in flight revised downward at a retarget with their requirements re-derived, and the limiter plays to the same value in TP mode | `testTruePeakModeHoldsTheCeilingUnderAutomation` (100 TP renders: instant and ramped cuts, zig-zag, rises, static, OS off–16×, 44.1–192 kHz; TP-off control) | unfixed: up to +2.72 dB (Annex 2, −1 → −20 dB instant, 44.1 kHz) over the live ceiling in a 1936-configuration matrix, 64 of the test's 100 renders over (worst +2.44 / +2.57 dB), sample peaks too; fixed: ≤ +0.004 dB over 2951 prototype-round renders and ≤ +0.023 dB over a 3532-run adversarial sweep (22.05–384 kHz), no sample above the live ceiling; a search for worst-case bursts finds nothing above the static-ceiling floor at 44.1 / 48 kHz (+0.058 / +0.026 dB) but +0.157 / +0.125 dB at 22.05 / 32 kHz at the bottom of a fast cut (0.2.14: +3.6 / +3.1 dB on the same bursts) — KI-025; TP off and a static ceiling bit-identical; each mechanism removed fails the test; a one-sample step at a downward retarget bounded and pinned (`testTruePeakModeBoundsTheStepAtACeilingCut`) |
| **GR max can omit visible history** (`GrHistoryView.h`) | **Fixed** | The max scanned a width-free `windowEntries` span less a lap margin; the graph draws a bucket-aligned range that reaches further back | Contract: GR max is the deepest entry of the history the GR graph draws at the reading's head in the current layout, plus the newest not-yet-drawn bucket (max ≥ now); read in two lap-certified chunks | `testTheGrReadoutReadsTheRingItNames` (rewritten), `testTheTickMaxIsTheDeepestEntryTheGraphDraws` | missed 9–11 drawn entries (96–117 ms) at 48 kHz / 512 Simple, ~3.4–3.8k at saturated pairs; the old range fails 7 checks; each lap check removed fails 2 |
| **STATISTICS bypass tooltip introduces unapproved wording** | **Needs decision — recorded as OQ-018; interim applied** | The 0.2.14 edit that removed "Click to reset…" also added a behaviour announcement no repository convention or record covers (C8) | `docs/OPEN_QUESTIONS.md` OQ-018 (placeholder, intended meaning, owner decision, surface, options); interim option 1 — the tooltip is the recorded 0.1.1 wording again; OQ-019 records the "lim GR" / "GR max" captions, which are in the same position | a truth check: the tip must not tell the user to click the inert body | the phrase history and the C8 analysis in the worklog §4 |
| **CI `sanitizers` red on `dd983ec`** (found in this round's inspection) | **Fixed** | the GR readout test's threaded premise fails intermittently under valgrind's serialised scheduling | a deterministic producer (a ring adaptor that pushes at a chosen peek) | as above | the state suite under valgrind with the CI flags: 1584 / 1584, 0 memcheck errors locally; CI's `sanitizers` job green on `f048328` (§3) |

## 2. DSP correctness status — the true-peak promise

| Path | Status | Guard |
|---|---|---|
| TP steady state | Holds — unchanged, static-ceiling output bit-identical to 0.2.14 | `testTruePeakModeHoldsTheCeiling` (123 runs, both meters) |
| TP engagement mid-stream | Holds — unchanged | `testTruePeakEngagementHoldsTheCeiling` |
| Offline rendering (entry during an engagement) | **Fixed this round** | `testOfflineEntryDropsTheEngagementTail` |
| Ceiling automation | **Fixed this round at 44.1 kHz and above**, a worst-case search included | `testTruePeakModeHoldsTheCeilingUnderAutomation` (44.1–192 kHz) |
| The price of the automation fix, bounded | With the output AT the ceiling, a downward retarget drops the clamp gain within one sample by about (16 + A/2 + 1) glide steps (0.0185 at 48 kHz on DC; TP-off glides 0.0008) — invariant 8 yielding to invariant 4, the owner's to confirm or trade against the tolerance (ADR-0045) | `testTruePeakModeBoundsTheStepAtACeilingCut` |
| Residual, recorded not asserted | An engagement decay checked once at the toggle can read up to +0.071 dB over a ceiling cut one host block later (1-sample blocks; ≤ 0 at 64 samples and above) — inside the 0.1 dB tolerance | ADR-0045 Consequences |
| **Open, pre-existing, found this round** | A host `reset()` or an unducked offline-entry latch cuts the stream to zero at full gain; readings straddling the cut read up to +0.96 / +0.98 dB. The render read as a file starts from silence | `KNOWN_ISSUES.md` KI-024 — the next true-peak item |
| **Open, found this round: below 44.1 kHz** | The clamp's gain law bounds a segment's peak only while the gains it reads are equal; an attack ramp beside it raises it. A constructed burst at the bottom of a fast full-range cut reads +0.157 dB at 22.05 kHz and +0.125 dB at 32 kHz (Annex 2) over the live ceiling — 0.2.14 +3.6 / +3.1 dB, the clamp-only alternative +0.31 dB; below 22.05 kHz a static ceiling reaches +0.23 dB, bit-identical in 0.2.14. Every programme matrix holds | `KNOWN_ISSUES.md` KI-025 — with KI-024, the true-peak items before Phase 1 resumes |

## 3. CI — the actual result

**GitHub, on `f048328`** (commits 1–4 of this round, pushed; the head after it, `c43be87`, changes one
comment in `CeilingClamp.h` and documents only):

| | Result |
|---|---|
| Passed | Build & Validate on push (run 36388515614) — docs; preflight; source-lint, the citation gate against `dd983ec` included; linux (suites, engine reproduction, channel probe, pluginval deterministic ×3 and randomise ×3, ABI floor, clang warning gate); linux-lto-tests; linux-lto-clang; realtime (RTSan canary, effects tier, RTSan DSP suite); **sanitizers** (ASan + UBSan suites, valgrind memcheck on both suites — the job that failed on `dd983ec`); windows, macos, macos-intel (suites and pluginval lanes, AU included on macOS). Pull-request run: merge-check. CodeQL (actions, c-cpp): no new alerts in the changed code. PREfast. Dependency review |
| Failed | none |
| Warnings | one new PREfast C6262 (46 KB of stack in `testTheTickMaxIsTheDeepestEntryTheGraphDraws`, a test fixture) — 37 open threads, all under `tests/`; 2 vendored clang warnings, not gated |
| Pre-existing | the 36 earlier PREfast threads (C6262 on test fixtures; two C6011 in the TEST-001 helpers, the 2026-09-03 scanner audit's group G3, "DO NOT FIX") |

Local gates on the final tree are listed in the worklog §5; they are not offered as a CI result.

## 4. Phase 1 status and what comes next

Unchanged by this round, which added no Phase 1 work; the per-finding table of
[`2026-09-27-phase1-roadmap.md`](2026-09-27-phase1-roadmap.md) stands. **Done:** UX-009 (ADR-0044),
UX-002, VIS-001, VIS-009 in part (ADR-0020 amendment 4), DOC-002, TEST-001, VIS-007 / VIS-003 step 1
(the GR readout — its max corrected this round). **Remaining:** DSP-005 (KI-023), VIS-010 and UX-010's
cue, VIS-005 / VIS-012 after TEST-004, VIS-014, STATE-008, VIS-004, UX-023, UX-008, VIS-013.
**Next recommended priority:** first the owner's review of the ⊕ records this PR carries (ADR-0044,
ADR-0020 amendment 4, ADR-0045 — with its step-versus-tolerance choice) and the open copy decisions
(OQ-018, OQ-019); then the two true-peak paths still open, under the owner's principle that the
promise is closed before UX work resumes — **KI-024** (a checked decay at a reset or an unducked latch)
and **KI-025** (the clamp's gain law below 44.1 kHz: a longer attack at low rates or a requirement that
bounds a ramp's effect on its neighbours, an owner decision because it moves a ⊕ voicing constant); then
DSP-005 — MATCH's predict floor, now the whole of the BYPASS comparison's residual (+0.63 LU at the
calibration point).

## 5. Owner decisions carried

| Decision | Record |
|---|---|
| ADR-0045 — the emission-time ceiling and the limiter moved onto it in TP mode (an amendment of ADR-0041 decision 3) | ADR-0045, ⊕ |
| How KI-025 is closed: a longer clamp attack below 44.1 kHz (a ⊕ voicing constant; the true-peak delay composition moves, the reported latency does not) or a requirement that bounds an attack ramp's effect on its neighbours | `KNOWN_ISSUES.md` KI-025 |
| The STATISTICS tooltip's bypass announcement, and its words | OQ-018 (interim: the recorded 0.1.1 wording) |
| The GR readout's captions and tips | OQ-019 (interim: shipping as placeholders) |
| Carried from earlier rounds: ADR-0044 and ADR-0020 amendment 4 (⊕); STATE-002 (KI-021); UX-003 (KI-022); VIS-002 (KI-020); ADR-0042 option A / KI-007; the TP clamp's voicing constants | as recorded in `2026-09-27-phase1-roadmap.md` §3 |

## 6. Evidence limits

No listening was performed for any change in this round. Every true-peak figure is the real engine on
synthetic programme (five hostile generators and a driven sine) at 44.1–192 kHz (22.05–384 kHz in the
adversarial review) and, for KI-025, on hill-climbed 32-sample bursts at 4–48 kHz — a search, so a
worse burst than the ones found may exist below 44.1 kHz; no host was run under ceiling automation, and the offline-entry route needs a
host that flips to offline without re-preparing — no such host was run. The PREfast C6262 threads and
the two C6011 threads on the TEST-001 test helpers stay open as recorded warnings (the 2026-09-03
scanner audit's group G3, "DO NOT FIX"); they are test-only.
