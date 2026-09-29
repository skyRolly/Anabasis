# 2026-09-29 — PR #43: the Phase 0 evidence, re-run on this PR's own tree

**What this is.** PR #42 was split on 2026-09-29 into two stacked PRs. PR #43 holds commits 1–13, which
are Phase 0 (`ed06ad0..2a5f8a8`, 0.2.13). PR #42 holds the rest and sits on top of it. Some of this
tree's records pointed at evidence that only PR #42 contains.

This worklog re-runs that evidence on **`2a5f8a8`** itself, so a reviewer of PR #43 alone can check
every claim PR #43 makes. Where a check cannot be re-run, it says so and says why. It also records the
disposition of the review findings raised against PR #43 (§10).

**Scope.** Nothing here describes Phase 1 work, and nothing here is a Phase 1 verification. The
code measured is `2a5f8a8` unless a row says otherwise. Scratchpad tools are not committed, as in
the earlier Phase 0 worklogs.

## 0. The claims that pointed outside this PR

| Claim at `2a5f8a8` | It pointed to | Re-established here |
|---|---|---|
| `docs/reports/2026-09-27-phase0-closure.md` §2, "re-run on the accepted tree" (transition sweep, TP-off identity, steady-state matrix, latency) | `worklogs/2026-09-27-phase1-match-statistics-observability.md` §Phase 0 re-verification. That worklog is added by PR #42 (`8f6ad0b`) and ran on the review head's code. | §2–§5, re-run on `2a5f8a8` |
| closure §5, "pluginval teardown segfault (review round) — re-checked this round — see the 2026-09-27 Phase 1 worklog" | the same worklog. The re-check text is added by PR #42's `dd983ec` and ran on PR #42's tree `11c9482`. | §7, re-run on `2a5f8a8`'s VST3 |
| closure §6, "Re-verification commands and outputs: …phase1…md" | the same file | this worklog |
| ADR-0041 evidence list, "Acceptance re-verification … re-run on the accepted tree" | the closure → the same worklog | §2–§5 |
| ADR-0042, "Re-checked at acceptance …: the ordering mutation still pinned" | the closure. The closure has no ordering or mutation content, so the pointer led nowhere. | §6(a) |
| `DOCUMENTATION_COVERAGE.md`, ADR-0043 addendum: "the three new checks each fail when the detector's Annex 2 reading is removed" | the same worklog | §6(b) |
| README / HANDOVER: "1973 checks (544 + 1429)" | none. The count was stale. | §1: **1977 (548 + 1429)**, corrected in this commit |

The closure record and the two ADRs are not edited in place. Each gains a dated pointer to this
worklog.

**Code identity.** Between the review head `2071294` and `2a5f8a8`, `git diff 2071294 2a5f8a8 -- src/`
changes comments only: the engine's engagement comment and `LoudnessMeterView.cpp`'s
"Proposed"→"Accepted". The review round's own records are part of this PR:
- `worklogs/2026-09-27-pr42-review-closure.md`: the Ardour engagement export, the 248-transition
  sweep and the steady-state replay;
- `worklogs/2026-09-27-phase0-product-correctness.md`.

Those records therefore describe this PR's audio code.

## 1. Suites, gates, and the corrected count

**Build:** GCC 13.3.0, Ninja, Release, `-DANABASIS_NO_LTO=ON`, JUCE 9.0.1 at the pinned SHA. 0
`warning:` lines.

| Check | Result |
|---|---|
| `AnabasisTests` | `PASS: 548 checks, 0 failure(s)` |
| `AnabasisStateTests` | `PASS: 1429 checks, 0 failure(s)` |
| GitHub, push run 36554523764 on `2a5f8a8`, jobs `linux` and `linux-lto-tests` | `PASS: 548 checks`, `PASS: 1429 checks` |
| `check-docs` | 130 files clean |
| `check-realtime` | 42 files, 0 violations, 1 of 1 ordering requirement verified |
| `check-portability` | 50 files, 0 violations |
| `check-citations --check`, bases `ed06ad0` and `00fa1a9` | 64 anchors still point at the same text |
| `scripts/preflight.sh` | exit 1 at the Linux ABI-floor step (see Warning below). The steps after it pass when run on their own. |

**Warning (local build configuration, not this PR's source).** The ABI floor reports
`exception_ptr::_M_release@CXXABI_1.3.13` in the local non-LTO build. The same symbol appears in a
non-LTO build of `main`. It is absent from the LTO build, and CI's ABI step passed on this head.

**The count.** `00fa1a9` (ADR-0043) adds **four** DSP checks and removes none:
- `testTruePeakModeHoldsTheCeiling`: one check becomes three (its premise split out, then one check
  per defining meter);
- `testTruePeakEngagementHoldsTheCeiling`: one check becomes two;
- `testClampTruePeakDetector`: gains the Annex 2 lower bound.

The commit message records 548. README and HANDOVER kept 1973 (544 + 1429), and the coverage
addendum for that commit says "+2 … +1". Both now read **1977 (548 + 1429)**.

## 2. Latency

- **Clamp delay D:** a probe against `src/dsp/CeilingClamp.h` gives D = **41 / 42 / 52 / 54 / 74 / 78**
  samples at 44.1 / 48 / 88.2 / 96 / 176.4 / 192 kHz. This matches the closure's figures.
- **Latency tests:** `testReportedLatencyMatchesImpulse`, `testOsLatencyMatrix` and
  `testTruePeakModeCapsTheWindowNotTheLatency` pass inside the 548.

## 3. Switching TP on mid-stream: the transition sweep, reconstructed

The original 248-configuration probe was never committed. This is a reconstruction with a larger
set, run on `2a5f8a8` with a negative control.

**Harness:**
- It drives the real engine.
- Meters: the product meter, an independent BS.1770 Annex 2 meter, and the sample peak.
- TP is switched on mid-stream at a hot operating point.

**Set: 10 800 renders.** 4 rates (44.1 / 48 / 96 / 192 kHz) × 9 oversampling cells × blocks 32 / 64 /
480 / 512 / 4096 × 5 programmes (hf, drums, white, square, isp) × EQ +12 dB Post / flat × ceiling −1 /
−0.1 dBTP × toggle at 0.15 / 0.25 / 0.33 s.

| Tree | TP engaged | Runs over 0.1 dB after the toggle | Worst after the toggle (product / Annex 2) | Runs over before the toggle (TP off) |
|---|---|---|---|---|
| **`2a5f8a8`** | 10800 / 10800 | **0** | **+0.0010 / +0.0047 dB** | 10696 / 10800 (worst +4.80 / +6.12 dB) |
| `3e9b343` (before the engagement fix `8e8882a`) | 10800 / 10800 | **9393** | +4.77 / +6.03 dB | 10680 / 10800 |

- The negative control reproduces the defect the review round found. `2a5f8a8` holds on every subset:
  both EQ settings, both ceilings and all three toggle times.
- The 104 `2a5f8a8` runs with no pre-toggle over are all isp at Oversampling Off with the EQ flat.

## 4. TP-off identity with `main`, reconstructed

**Set: 264 TP-off renders of 1 s.**
- 4 rates × 11 oversampling cells (Off; 2/4/8/16× minimum and linear phase; Force Max, both phases).
- Rotated across blocks 32 / 64 / 512 / 4096, lookahead 0.5 / 2 / 5 / 10 ms, six programmes, ceilings
  −1 / −0.1 / −6 and EQ Post +12 / flat / Pre +12.

| Check | `2a5f8a8` against `ed06ad0` |
|---|---|
| TP-off output hash | **264 / 264 identical** |
| Reported latency and impulse position | 0 differ |
| Control: the 44 TP-on twins | 44 / 44 differ. `2a5f8a8`: 0 / 44 over 0.1 dB (worst +0.0013 / +0.0020). `main`: 44 / 44 over (worst +4.78 / +5.99). |

## 5. The steady-state TP-on matrix on `2a5f8a8`

**Set: 3168 renders of 1 s.** 6 rates (44.1–192 kHz) × 11 oversampling cells × Post shelf 0 / +6 / +12
dB × 4 programmes × 4 rotations of block, lookahead, ceiling (−1 / −0.1 / −6), operating point and
limiter style.

| Meter | Over 0.1 dB | Worst |
|---|---|---|
| product | **0 / 3168** | +0.0070 dB |
| BS.1770 Annex 2 | **0 / 3168** | +0.0080 dB |
| sample peak | — | +0.0000 dB |

- **Not vacuous:** all 3168 engaged, and 2824 of them reach within 0.5 dB of the ceiling.
- **Against the recorded figures:** the worst readings are above the recorded +0.005 / +0.004 dB. This
  is a different configuration set, and every reading is inside the 0.1 dB tolerance.
- **Not re-run:** libebur128 and the 32× Kaiser reference are reference meters, not part of the
  definition (ADR-0043), and neither is installed here.

## 6. Mutations on `2a5f8a8` (each built, run and restored)

**(a) ADR-0042's ordering.** Move `adaptiveEngine.resumeAfterReset (p.freeze);` after the ADR-0014
injection in the reset/offline branch.
- **DSP:** `FAIL: 548 checks, 4 failure(s)`:
  - `freezeRePrepare: (premise) the vector is audible here — the comparison is not vacuous` ×2
  - `freezeRePrepare: the render is bit-identical to that vector restored and frozen` ×2
- **State:** `FAIL: 1429 checks, 1 failure(s)`:
  - `frozenRestore: an unprimed session load restores the vector on the first block`
- So the ordering is still pinned.

**(b) ADR-0043's split.** Delete the two Annex 2 `max3` lines in `src/dsp/ClampTruePeakDetector.h`.
- **DSP:** `FAIL: 548 checks, 7 failure(s)`.
- **The three Annex 2 checks fail:**
  - `clampDetector`'s lower bound;
  - `tpCeiling` on the Annex 2 filter (33 of 123 runs over, worst +0.235 dB);
  - `tpEngage` on Annex 2 (+0.117 to +0.334 dB).
- **Also failing:** four `tpCeiling` per-axis worst-case checks, which take the larger of the two
  meters.
- **The product-meter checks pass. State:** `PASS: 1429`.

**Restored tree:** 548 / 1429.

## 7. pluginval on `2a5f8a8`

**CI.** Push run 36554523764 passed every pluginval lane:
- Linux VST3, both modes ×3;
- Windows VST3;
- macOS VST3 and AU;
- macos-intel VST3 and AU, both modes ×3.

**One crash, retried.** Linux randomise pass 2/3 (seed `0x6036105`) crashed with exit 139 **during
"Plugin state restoration"**. That is the first test after "Editor Automation", and no `SUCCESS` had
been printed. The retry passed on seed `0x7a99ff4`.

**Local runs** (pluginval 1.0.4, strictness 10, under `xvfb-run`):

| Runs | Crashes | Where |
|---|---|---|
| deterministic ×3 (seed `0x1`), the gate script | 0 | — |
| `--randomise --random-seed 0xcb2cae` ×3 (the review round's seed) | 0 | — |
| unseeded `--randomise` ×30 | 1 | after `SUCCESS` (seed `0x53df58b`) |
| the split's first local gate run, randomise ×3 | 1 | after `SUCCESS`; the retry passed |

**Classification, by position in the log.** No core or stack was captured, so the frame-level match
is **not verified**.
- The in-test crash has the position of PR #42's KI-028 Linux **signature A**: in pluginval 1.0.4's
  own JUCE 8.0.3 X11/XEmbed host code, in the first test after Editor Automation.
- The after-`SUCCESS` crashes have the position of **signature B**, pluginval's JUCE 8.0.3 VST3
  run-loop teardown.

**Why the closure's "re-checked" row does not carry over.** That row (no crash in 15 runs) was
measured on PR #42's tree `11c9482`. It does not describe `2a5f8a8`: crashes of both positions
occurred here, and the Linux lane's crash retry absorbed them.

**The external evidence lives in PR #42.** Both signatures were diagnosed as external in PR #42's
`docs/KNOWN_ISSUES.md` KI-028: 0 Anabasis frames on any thread, and a causal test with one upstream
JUCE fix ported. This PR has no KI-028 entry of its own, and that dependency is stated here rather than
duplicated.

## 8. The Ardour engagement export: not re-run

**Why not.** Ardour is not installed here, and the acceptance-time session, Lua toggle script and
review-round WAVs were never committed. So "re-exported … bit-identical to the review-round render"
cannot be re-established.

**What stands instead.** The review-round Ardour export itself is recorded in this PR
(`worklogs/2026-09-27-pr42-review-closure.md`). The audio code has not changed since (§0), and §3
re-measures the same behaviour at the engine.

## 9. Static analysis on `2a5f8a8`

**PREfast** (pull-request run 36554626504) reports 158 results, **0 in `src/`**.

| Compared with `main`'s 150 | Detail |
|---|---|
| Added: 8 | All C6262 (stack size) in `tests/dsp_tests.cpp`, each on a test function this PR adds. They are G2 of the 2026-09-03 scanner audit ("TEST-ONLY ACCEPTABLE"). The largest new frame is `testAFrozenLatchSurvivesARePrepare` at 219 644 B; the largest overall is 364 452 B. Both are under the ~768 KB trigger, and the Windows suites passed. |
| Removed: 0 | — |

**Review threads.** This PR's 8 open code-scanning threads are exactly those 8 results.

**The closure's "9 open … 1 resolved (C6011)" row.** It describes PR #42's threads at `2071294`. The
C6011 was a production finding in `ClampTruePeakDetector.h` and is fixed in this PR (`e36890f`).

**Also:** the CodeQL check (actions and c-cpp analysis) and dependency review passed.

## 10. Review findings raised against PR #43

| Finding | Status | Introduced by #43? | Evidence |
|---|---|---|---|
| **Bypass audio contaminates the session peak hold** | **Pre-existing on `main`** | No | See the first note below. |
| **The session clock includes unmeasured return audio** | **Not present in this PR** | No | This tree has no session clock. The STATISTICS session length is added by PR #42 (`7166140`), and `LoudnessMeter.h` is identical to `main`'s here. PR #42 records that finding's disposition. |
| **The GR maximum persists after playback stops** (informational) | **Not present in this PR** | No | `src/gui/` changes only a comment in `LoudnessMeterView.cpp` against `main`. The numeric GR readout the finding describes is added by PR #42 (`11c9482`), and PR #42 records the disposition. |
| **Closure evidence not present in this split** | **Fixed in this PR** | No: a consequence of the split | This worklog, and the dated pointers in the closure record, ADR-0041 and ADR-0042 |
| **Published test total 1973 against 1977 measured** | **Fixed in this PR** | Yes: the drift is `00fa1a9`'s, a commit in this PR | §1. README and HANDOVER now read 1977 (548 + 1429). |

**Bypass-peak evidence.**
- **Where the hold lives.** The session peak holds are kept by `src/PluginProcessor.cpp`, fed from the
  engine's render-tap readings of every frame, bypass included. This PR does not change
  `src/PluginProcessor.*` (`git diff --stat ed06ad0 2a5f8a8 -- src/PluginProcessor.*` is empty), and
  its engine diff touches no `bypassMix`, render-tap or `outTp` line.
- **Probe.** A probe folds the holds as the wrapper does: 1 s processed at a −30 dBTP ceiling, then 1 s
  of 0 dBFS bypass, then 1 s processed. The TP hold reads 0.000 dBTP against a −29.86 processed-only
  maximum, and the result is **bit-identical on `main` and `2a5f8a8`**.
- **Where the fix lives.** Leaving the audition out of the session figures is audit VIS-001, a
  Phase 1 item. It is done in PR #42 (`7166140`), which is where the lag edges of that exclusion are
  addressed too. It is not pulled back into this PR.

## 11. Limits of this PR on its own

These are stated so that "reviewable" is not read as "releasable".

**Merged alone, PR #43 would ship TP-mode defects that later review rounds found, and PR #42 fixes
them:**

| Defect | Fixed in PR #42 by |
|---|---|
| The falling-Ceiling overshoot. Each frame is clipped at the ceiling it carried in, D samples old. | ADR-0045 (`dce072b`); then ADR-0046 |
| The offline-entry engagement tail | `74c114e` |
| KI-025 below 44.1 kHz | ADR-0046 (`4ff71bd`, `f03d673`) |
| KI-024 route C's widened trigger | `a43094b`, `f03d673` |

**Merge order.** The two PRs are to be merged back to back, `#43 → #42`, with merge commits.

**Evidence.** PR #43's description lists these with their measured sizes.
