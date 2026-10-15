# 2026-09-27 — PR #42 review closure: the true-peak engagement leak, the `realtime` gate, and the owner's decision material

The second round on PR #42 (Phase 0 of the 2026-09-26 product / UX audit), after its first round
([`2026-09-27-phase0-product-correctness.md`](2026-09-27-phase0-product-correctness.md)) had been
pushed as `3e9b343`. Asked for: close the review — a true-peak leak at engagement and a red
`realtime` job — re-run the true-peak matrix, validate realtime safety again, keep ADR-0041 and
ADR-0042 **Proposed** and prepare the owner's decision material, re-check STATE-002 / UX-003 without
forcing them, and assess Phase 1 without starting it. This file is the evidence; the decision material
is the dated record [`docs/reports/2026-09-27-phase0-owner-decisions.md`](../docs/reports/2026-09-27-phase0-owner-decisions.md).

Same container and toolchain as the first round (Linux x86-64, Xeon @ 2.10 GHz, 4 cores; GCC 13.3
Release + LTO for the suites and the plug-in; clang 22.1.8 for the realtime tiers and the warning
gate). Every engine figure is the real `AnabasisEngine`, compiled from the PR head `3e9b343` ("pre")
or this revision ("fix"). Scratchpad tools are not committed; their method is written down here.

**Status of record.** ADR-0041 (revised here) and ADR-0042 stay **Proposed**. PR #42 is not merged
and is not to be merged before the owner decides both at the Architecture Review Gate.

## Chronology

1. Starting state read: `main` = `ed06ad0` (audit PR #41 merged); PR #42 = six commits on it, head
   `3e9b343`. CI on the head: `realtime` **failed**; linux, linux-lto-tests, linux-lto-clang,
   sanitizers, windows, macos, macos-intel, docs, CodeQL and PREfast green. Code scanning opened seven
   PREfast threads (one C6011, six C6262). The review finding "True-peak ceiling leaks during
   engagement" (`src/dsp/AnabasisEngine.cpp` R1242–1246) came with the round's request; it is not a
   GitHub thread on the PR.
2. The leak reproduced on the engine; mechanism established (§Review finding).
3. The `realtime` failure root-caused — introduced by the PR, deterministic (§CI).
4. The detector moved into a JUCE-free header; bitwise identity proven; the effects tier widened.
5. Transition options measured; the checked decay implemented; the regression test written, run
   against the PR head (fails) and mutation-checked.
6. The 248-configuration transition sweep, before and after.
7. PREfast threads assessed; C6011 fixed.
8. The whole `realtime` job reproduced locally, including a seeded violation in the widened tier.
9. Documentation synchronised (ADR-0041 revision, DSP_POLICY, LATENCY_MODEL, REALTIME_SAFETY_AUDIT,
   USER_MANUAL, CODE_STYLE, REPOSITORY_MAP, KNOWN_ISSUES, TEST_REPORT, TESTING).
10. The steady-state matrix re-rendered on the revision — hash-identical to the PR head.
11. A libebur128-reading prototype measured for the owner's meter question; detector cost measured.
12. ADR-0042 re-verified path by path, with an ordering mutation.
13. Real hosts: an Ardour mid-export TP toggle on both builds; Carla latency / activate / state.
14. The decision record written; gates; commits.

---

## Review finding — "True-peak ceiling leaks during engagement"

**Verdict: a real defect, confirmed and fixed.**

**Smallest reproducer** (engine probe, pre): 48 kHz, 512-sample blocks, Oversampling Off, a hot
limiter point, +12 dB Post shelf, HF-heavy programme; 0.25 s with true-peak mode OFF, then ON at the
block top at sample 48128. Trace:

| Offset from the toggle | What the engine does |
|---|---|
| +0 | the request sets `latchWanted`; the §2.8 duck starts its out-leg; `appliedTpClamp` is still false, so stage E runs `clamp.processSample` — the sample clip |
| +0 … +104 | **39 readings over the ceiling by > 0.1 dB, the first at +0** (duck gain 1.000, path = sample clip); worst +3.42 dB (product meter) / +3.65 dB (Annex 2) |
| +287 | the duck reaches its bottom |
| +512 | the next block top latches the TP composition (`latchOsConfig`) |
| +1024 | the fade-in starts, on the true-peak path |

**Mechanism.** A TP toggle moves the region line (ADR-0041 decision 4), so it is latched at the duck's
silent bottom like an oversampling change. For an oversampling change the out-leg is harmless — both
compositions hold the same ceiling. For TP **on** it is not: the out-leg is emitted by the composition
being replaced, whose clamp is the sample clip, while the user has already asked for dBTP. The overs
are real at emission and visible on the plug-in's own meter; a latency-compensated host places them
**before** the toggle on its timeline (all 39 here: the reported latency is 480 samples, the out-leg
ends at +287), which is why the timeline alone does not show a violation "after" the toggle.

**The fix options** (the round's A and B; measured where measurable):

| | Ceiling from the toggle | Transition splatter, clean 0.5 tones (HF energy, 100 Hz / 1 / 3 / 6 kHz, dB) | Latency | State / params |
|---|---|---|---|---|
| status quo (out-leg on the old path) | **no** (+4.66 / +5.46 dB worst) | −83.3 / −32.2 / −25.5 / −44.7 | — | — |
| **A** — silent until applied (instant mute) | yes | −8.2 / −9.5 / −21.3 / −24.9 | none | none |
| **A1** — silent until applied, entered by a checked decay of the last emitted frame | yes | −63.8 / −26.0 / −21.7 / −27.8 | none | none |
| **B** — enforce TP on the fade | only with the TP path's lookahead, which the replaced composition does not have: a gain step at the toggle, or D more latency (ADR-0004 / decision 4) | — | + D, or a step | none |

B is physically blocked: holding a true-peak ceiling from the first post-toggle sample *continuously*
needs to see the peak before it arrives, and the audio already past the old composition's clamp has
no lookahead left. A holds the ceiling at the cost of a full-scale step at every engagement (~56 dB
more splatter than A1 on a 100 Hz tone). **A1 chosen** — the smaller and safer change: no latency,
parameter, state or TP-off change; the steady state untouched; realtime-bounded.

**Implementation.** `EngagementTail` (`CeilingClamp.h`): a raised-cosine decay of the last emitted
frame to zero over 6 ms. Before its first sample, `start` replays the last 32 emitted frames and the
first 31 tail frames through a private `ClampTruePeakDetector` and requires every reading of a segment
at or after the toggle — including those whose windows still reach back into the emitted audio — and
the tail's first value itself to be at or under the ceiling; if the unscaled decay fails, a 12-step
bisection finds the largest scale that passes (scale 0 — silence — is the floor; see the ceiling-cut
case below for when even it cannot meet a lowered target). `AnabasisEngine::process` enters the
bottom at the toggle block (`duckState = bottom`, `duckGain = 0`), so the existing bottom branch
latches the TP composition in the same block; stage E adds the tail to the (silent) processed path and
feeds every emitted frame to the tail's history. Disengaging (on → off) keeps the out-leg: its audio
comes from the TP path, which holds both ceilings.

## TP transition — verification

- **Sweep** (248 configurations: 44.1 / 48 / 96 / 192 kHz, nine OS cells, blocks 32 / 64 / 480 / 512 /
  4096, five programme kinds, toggles at several programme phases): pre **186 / 248** over by more
  than 0.1 dB after the toggle (worst +4.662 dB product / +5.456 dB Annex 2; first violating reading 0–106
  samples after the toggle) → fix **0 / 248** (worst +0.001 / +0.003 dB). The decay was scaled in 141
  runs, never below 0.8245 (−1.7 dB); a synthetic full-scale Nyquist-rate history needs 0.69 (−3.2 dB).
- **A ceiling cut in the same block** (a preset / A/B swap that turns TP on and lowers the ceiling):
  the check uses min(smoothed ceiling, new target). On hostile programme (three configurations,
  cuts of 0 / 3 / 6 / 10 dB from −1.0 dBTP) the readings of the first 6 ms after the toggle stayed at
  or under the **new** ceiling (worst −0.056 dB; the decay scaled down to 0.17), and nothing after the
  toggle exceeded the smoothed ceiling by more than +0.001 dB. The bound behind "silence always
  fits" (history-tap L1 ≤ 0.59) is relative to the ceiling the history was emitted under, so a cut
  deeper than ~4.6 dB under adversarial near-Nyquist history could leave the old audio's ringing
  above the new target — under the smoothed ceiling, as any ceiling move is. Recorded in the code
  and in ADR-0041 decision 5; not observed.
- **Regression test** `testTruePeakEngagementHoldsTheCeiling` (13 hostile runs + a 100 Hz continuity
  run; details in `TEST_REPORT.md`). **Against the PR head** (a worktree at `3e9b343` with this
  round's test file): all 13 runs over, +2.43 to +4.68 dB — `FAIL: tpEngage: no reading at or after a
  mid-stream TP-on toggle exceeds the ceiling by > 0.1 dB`. **Mutations** (each reverted): the decay
  replaced by an instant mute → fails the continuity check and `testDuckWrapsTruePeakLatch`
  ("the true-peak switch never steps the output"); the decay without its check → ceiling check fails
  (+0.27 to +0.60 dB).
- **Real host** (Ardour 8.4, headless): a session with the first round's hot generator and Anabasis on
  the master bus (Limiter Gain 12 dB, ceiling −0.10, TP off), plus a pass-through Lua DSP processor
  that turns Anabasis's True Peak parameter on through Ardour's plug-in parameter API once the
  playhead reaches a set sample; exported with `ardour8-export -b float`. The same session without the
  toggle gives the reference, and the first sample at which the two exports differ is the first
  sample the plug-in emitted after seeing TP on. From that sample on (0.5 s):

  | Toggle | PR head (pre) | This revision | TP-off export, same window |
  |---|---|---|---|
  | 0.512 s (broadband bursts) | **+1.82 / +2.12 dB** over (product / Annex 2), 12 samples > +0.1 dB | **−0.05 / +0.00 dB**, none | +2.95 / +3.15 dB |
  | 4.608 s (mixed programme) | −0.53 / −0.11 dB (the out-leg began on a quiet stretch) | −0.53 / −0.11 dB | +3.49 / +4.35 dB |

  The TP-off exports of the two builds are sample-identical. One export of the revision's first
  toggle session stalled inside Ardour before rendering a sample (main thread in
  `Session::start_audio_export` → `Butler::wait_until_finished`, no plug-in thread busy, the WAV only a
  header); killed and re-run, it completed — an Ardour-side export-start hang, not a plug-in effect.
- **Steady state untouched:** the P0 matrix (2718 main + 240 8×-linear + 56 hot-subset renders)
  re-rendered on the revision is **hash-identical** to the PR head; TP off is still 222 / 222
  hash-identical to `main`.

## CI — the `realtime` job

**Failure:** the "Realtime effect diagnostics (first-party leaf layer)" step at `3e9b343`:
`src/dsp/TruePeak.h:3:10: fatal error: 'juce_audio_basics/juce_audio_basics.h' file not found`. That
step compiles `tests/realtime_effects.cpp` with `-I src -I src/dsp` only (ADR-0029: Clang can prove
a routine non-blocking only through definitions it can see, so the tier is JUCE-free by
construction). `CeilingClamp.h` is one of its leaf headers, and PR #42 made it include `TruePeak.h`
(for the detector, which includes `juce_audio_basics`) and `juce_core` (for the ownership macro); the
compile stops at the first. **Introduced by the PR, deterministic** — `main`'s `CeilingClamp.h`
includes only `<cmath>`, and its `realtime` job is green.

**Fix.** `ClampTruePeakDetector` and the product meter's phase design moved into the new JUCE-free
`src/dsp/ClampTruePeakDetector.h`; `TruePeak.h` (the estimator) includes it; `CeilingClamp.h` includes
only it and standard headers. The JUCE helpers used there are reproduced expression for expression
(`jmin` / `jmax` / three-argument `jmax`, `MathConstants` π and 2π), so no arithmetic moved: a bitwise
fingerprint of the estimator's, the detector's and the clamp's true-peak outputs over 200 000 frames,
plus the designed phases, is identical before and after the move. Copy operations are deleted
explicitly (no leak-detector macro without JUCE — `CODE_STYLE.md` records the leaf-header exception).
The tier was then **widened**: the driver now calls `processFrameTruePeak` and every
`EngagementTail` entry point, so the ceiling stage's true-peak half is inside the compile-time proof;
a seeded allocation in the true-peak path fails the effects compile, as it must.

**Reproduced locally, the whole job** (clang 22.1.8, the workflow's own commands): the RTSan canary
fires (non-zero exit, a RealtimeSanitizer report); the effects clean compile passes and the effects
canary fails with `-Wfunction-effects`; the RTSan RelWithDebInfo build of `AnabasisTests` runs the DSP
suite with zero RealtimeSanitizer reports (re-run on the final tree — §Gates).

**PREfast threads (7).** C6011 at `TruePeak.h:308`, "Dereferencing NULL pointer 'w[c]'": real in the
function's contract — `w[]` starts null and stays null when `numCh ≤ 0` — though the engine never calls
it so. Fixed: an early return for `nCh ≤ 0` in the detector and in `processFrameTruePeak`. C6262 × 6 in
`tests/dsp_tests.cpp` (37 592–83 868 bytes of stack): test functions holding `AnabasisEngine` fixtures
(41 216 bytes each) — the scanner audit's G2 disposition (C6262 in tests, TEST-ONLY ACCEPTABLE); no
change. The threads are left for the code-scanning service to re-evaluate; none was dismissed.

## Realtime safety, re-validated

`EngagementTail` holds fixed arrays only; `prepare` sizes nothing on the heap (the length is a
count); `start` runs once per engagement and is bounded at 13 replays of 63 detector frames. Measured
on this machine as thread CPU time over 3 × 2000 calls: **~7 µs mean** when the decay fits unscaled,
**~80–88 µs mean** when the bisection runs; the slowest single call ~0.25 ms (this container is
shared; a 64-sample block at 48 kHz is 1.33 ms). `value` / `advance` / `pushEmitted` are a few
multiplies and stores per sample. The allocation guard's TP matrix crosses engagements, so `start`
runs under it; the effects tier now covers it at compile time; the RTSan suite exercises it at run
time.

## Delivery ceiling definition — what "dBTP" is measured on

The owner's question (KI-020; audit DSP-001 sub-item (a)) is laid out in the decision record §1: Option
1 (the product meter + the Annex 2 filter — what the code does) against Option 2 shapes (2a also
hold libebur128's reading; 2b a longer accurate kernel; 2c a margin), each with method, standards,
discrepancies, affected content, consequences, tests, CPU, ADR changes, behaviour change and wording.
**No option is chosen.**

New measurement for 2a — a scratch prototype only (never in the product): libebur128 1.2.6's own
interpolator (49-tap Hann-windowed sinc, unnormalised, 4× below 96 kHz, 2× below 192 kHz, none above)
as a fourth detector reading. Its polyphase phases are symmetric about the segment the detector reads
and fit the existing 32-sample window (x[j−5..j+6] at 4×, x[j−11..j+12] at 2×), so **no latency
change**. Checked against the library (three signals × five rates × two channels): agreement within
8·10⁻⁷ dB. On the full matrix: libebur128 over by > 0.1 dB **130 / 2736 → 0 / 2736** (worst
+0.18 → +0.004 dB); product meter and Annex 2 unchanged (0; +0.005 / +0.004 dB); the 32×/128-tap
reference on the hot subset unchanged (+0.98 dB); level: 1858 of 2736 TP-mode renders change, by at
most 0.017 dB RMS; TP off 222 / 222 bit-identical. Detector cost (stereo frame, −O2, best of 7):
~121–127 ns → ~157–161 ns.

## ADR-0041 — revised, still Proposed

Decision 5 (engaging no longer rides the out-leg), decision 8's prescribed text, decision 2 (the
detector's header), Consequences, Related code, Evidence and a new "What the owner is asked to decide"
section. The latency composition was re-verified against the code: D = attack + 30 with attack =
max(8, round(0.25 ms · fs)) — 41 / 42 / 52 / 54 / 74 / 78 samples at 44.1 / 48 / 88.2 / 96 / 176.4 /
192 kHz; the longest engaged window in TP mode 10 ms − D; reported latency `maxLookahead(10 ms) +
osLatency` in both modes (pinned by `testReportedLatencyMatchesImpulse` and `testOsLatencyMatrix` in
both modes). The engagement fix changes when the latch happens, not what it latches: no latency change.
Carla re-checked with the revision: 480 samples at 48 kHz, unchanged across four TP toggles. Policy text
synchronised: DSP_POLICY invariants 4 and 8 and the test map, LATENCY_MODEL, USER_MANUAL.

## ADR-0042 — unchanged, still Proposed

No code change this round. Re-verified against the host lifecycle: the engine resets the adaptive
state only through `prepare` (the processor does not override `AudioProcessor::reset()`; entering an
offline render resets nothing adaptive); `prepare` stashes once per sequence; the first block's
snapshot decides; a staged ADR-0014 restore survives `prepare` and is injected after the carry. **New
mutation:** the carry moved after the restore injection fails four DSP checks
(`freezeRePrepare: the render is bit-identical to that vector restored and frozen`, and its
non-vacuity premise, at two configurations) and the state check `frozenRestore: an unprimed session
load restores the vector on the first block` — so "a staged restore still has the last word" is
pinned. The decision record §3 tabulates every path (re-prepare at the same / a new rate, Freeze
engaged while stopped, Freeze off, session load before `prepareToPlay`, save after a re-prepare, A/B
with and without a vector, preset load, offline entry, host activate cycles) with its evidence, the
alternatives A–F, the trade-offs and a technical recommendation (accept C; decide option A and KI-007
item 10 separately). Carla with the revision: three deactivate / activate cycles keep Freeze, TP and
Ceiling; a state round trip restores them. Not observed in a DAW: a re-prepare during playback.

## STATE-002 and UX-003 — re-checked

Unchanged in code (no file on either path was touched) and still owner-blocked: STATE-002 needs a
superseding ADR for ADR-0010's lockable set or a preset-contract change; UX-003 needs an ADR or a
family decision against the brand checklist's must-match save flow, and maintainer copy (C8). Options
restated in the decision record §4. Not forced.

## Phase 1 readiness

Not ready as a whole, unchanged from the first round's assessment: UX-009 (a monitor-stage
signal-order change — hard stop — and an ADR-0006 D8 amendment), VIS-001 against ADR-0020, UX-002's
family convention, C8 copy for every new caption, DAW evidence for VIS-005 / UX-010 that TEST-002 has
not collected, TEST-004's statics. Phase 0 itself closes only with the owner's rulings on ADR-0041,
ADR-0042 and the meter definition. TEST-001 can start first (no product behaviour). **No Phase 1 code
was written.**

## Gates

On the final tree, and commit by commit where a commit carries code:

- **GCC 13.3 Release + LTO:** `AnabasisTests` **544** and `AnabasisStateTests` **1429** checks, all
  passing (1973), no build warnings. Each code commit built and passed on its own in a separate
  worktree: the realtime-gate commit 540 + 1429, the engagement commit 544 + 1429.
- **clang 22.1.8:** all 30 first-party translation units rebuilt; the CI first-party warning gate
  (`check-clang-warnings.py`) on that log: no first-party warnings. DSP suite 544 checks passing.
- **`realtime` job, reproduced locally with the workflow's commands:** the RTSan canary exits 43 with
  a RealtimeSanitizer report; the effects tier compiles clean and its canary fails with
  `-Wfunction-effects` — on the final tree and at both code commits; allocations seeded into
  `processFrameTruePeak`, `EngagementTail::start` and `ClampTruePeakDetector::processFrame` are each
  reported (at the realtime-gate commit, the detector's through `processFrameTruePeak`); the RTSan
  RelWithDebInfo build runs the DSP suite — 539 checks (the allocation guard stands down in that lane),
  0 RealtimeSanitizer reports, the engagement test included (`AnabasisEngine::process` is
  `[[clang::nonblocking]]`, so `start` runs inside the sanitizer's realtime scope).
- `check-docs` clean (and at the docs commit on its own); `check-citations` — two anchors in ADR-0013
  and ADR-0014 had drifted with the engine's new lines, re-anchored with `--fix` in the engagement
  commit and read back; `check-realtime` 42 files, 0 violations; `check-portability` 50 files,
  0 violations.
- **pluginval**, strictness 10 (`build.yml`), editor under Xvfb, on the VST3 rebuilt from the final
  code: deterministic 3 / 3 passes; randomise 3 / 3 passes — **with one warning:** the third randomise
  pass (seed `0xcb2cae`) printed `SUCCESS` for every test and then segfaulted at validator exit
  (exit 139); the script's Linux crash-retry (documented there for host-side X11/XEmbed crashes) re-ran
  it and it passed. Not reproducible: the same seed replayed 5 times, and 10 unseeded randomise passes,
  all clean on this build; the PR head's build crashed the same way (after `SUCCESS`) in 1 of 10
  unseeded passes — so it predates this round. Not diagnosed further.
- `scripts/preflight.sh` on the final tree: passed — every checker and its self-test, the Linux ABI
  floor on the built VST3, the citation gate against all three bases, and both suites (544 + 1429).

## Not verified

- Listening: the decay's sound on real programme (clean-tone splatter only); the TP-mode voicing.
- A DAW re-prepare during playback (ADR-0042's audio half in a host); REAPER / Logic / Cubase; Windows
  and macOS hosts; host bypass mapping; automation (True Peak is not automatable — the Ardour toggle
  went through the plug-in parameter API, the path a generic editor or control surface takes).
- The meter question on a music corpus (the matrix programme is synthetic).
