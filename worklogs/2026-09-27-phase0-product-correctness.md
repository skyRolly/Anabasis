# 2026-09-27 — Phase 0 product correctness: the true-peak ceiling, Freeze across a re-prepare, and four owner calls (0.2.13)

The first implementation round after the product / UX audit
([`docs/reports/2026-09-26-anabasis-product-ux-audit.md`](../docs/reports/2026-09-26-anabasis-product-ux-audit.md),
146 findings, 1 P0 and 9 P1), taking its Phase 0 in the order the round was asked for: the P0
true-peak ceiling (DSP-001) first, then STATE-002, STATE-004, UX-003 and DSP-004, then host evidence.
This file is the permanent record: what was measured, what was decided and why, what changed, and
what was not verified. The audit report itself is not rewritten (a dated report is superseded, not
edited); the finding-by-finding outcome is the dated follow-up record
[`docs/reports/2026-09-27-phase0-follow-up.md`](../docs/reports/2026-09-27-phase0-follow-up.md),
which points here for the evidence.

Everything below ran in this container (Linux x86-64, Intel Xeon @ 2.10 GHz, 4 cores; GCC 13.3,
Release). **Every engine figure comes from the real `AnabasisEngine`** — compiled either from a
byte-for-byte snapshot of `main`'s `src/dsp` or from the working tree — never from a model of it.
Scratchpad tools (probes, matrices, the host scripts) are not committed; their method, inputs and
outputs are written down here so the figures can be regenerated.

**Status of record.** Two records carry this round's hard-stop items and are **Proposed**, not
Accepted: [ADR-0041](../docs/architecture/design-decisions/ADR-0041-clamp-true-peak-path-inside-the-latency-allowance.md)
(the clamp's true-peak path; amends ADR-0004 for TP mode) and
[ADR-0042](../docs/architecture/design-decisions/ADR-0042-a-frozen-latch-survives-a-host-re-prepare.md)
(a frozen latch survives a host re-prepare). The PR that carries them is **not to be merged until
the owner decides both at the Architecture Review Gate** — a green build does not clear them.

| Finding | Outcome | Where it lives |
|---|---|---|
| **DSP-001** (P0) — TP mode does not hold its ceiling | **Proceed — fixed and verified** (behind ADR-0041's gate) | §Investigation 2, §Implementation 1, §Verification 1–6 |
| **STATE-002** — a factory preset turns TP, Dither and Shaping off; LOCK holds only the number | **Defer** (to the owner; conflicts with Accepted ADR-0010) + interim disclosure | KNOWN_ISSUES KI-021, USER_MANUAL §3.2 / §7.3 / §8 / FAQ |
| **STATE-004** — a host re-prepare drops a frozen latch while FREEZE stays lit | **Proceed — fixed and verified** (behind ADR-0042's gate) | §Implementation 2, §Verification 3 and 6 |
| **UX-003** — Save Preset overwrites an existing preset without asking | **Defer** (brand checklist §A must-match, C8 wording, owner/ADR) + interim disclosure | KNOWN_ISSUES KI-022, USER_MANUAL §7.2 |
| **DSP-004** — Clip Drive at Oversampling Off low-passes the top end | **Preserve the DSP; disclose and pin** (the oversampling-default decision stays the owner's) | KNOWN_ISSUES KI-005, USER_MANUAL Oversampling row / §8, `testClipDriveDroopIsTheDisclosedOne` |

---

## Investigation

### 1. Baseline — `main` is the audited code

`main` was `ed06ad0` (the audit PR #41, merged). `git diff e769f33 ed06ad0 -- src tests CMakeLists.txt`
is empty: the audit's product-code anchors (`e769f33:…`) hold unchanged on the branch point, so the
audit's findings were re-checked against current code, not assumed.

### 2. DSP-001 — why true-peak mode did not hold its ceiling

**The code.** `CeilingClamp.h` never grew past its P1 comment — *"sample-level hard clamp … The
true-peak-driven gain half of ADR-0006 item 3 arrives with the TruePeak tap at P2"*. In both modes the
clamp compared SAMPLES with the ceiling and clipped them. ADR-0006 (Accepted) decided that with
`truePeakMode` on the clamp's gain acts on its own true-peak estimate of its own input, with the
sample clip as backstop (items 2–3); that half was never built, while ADR_INDEX, ADR-0006's banner
and ADR-0015 decision 7 described the guarantee as in force. The TP switch did reach the **limiter**:
its detector uses the 4× estimator at oversampling Off/2×, and at ≥ 4× reads the oversampled signal
directly (ADR-0003 item 6).

**The measurement.** A scratchpad probe drives the real engine with deterministic synthetic
programmes and measures the OUTPUT with four meters: the product's own `TruePeakEstimator` (what the
dBTP display reads), an independent transcription of the **ITU-R BS.1770-5 Annex 2** example filter
(order 48, 4 phases; the table was cross-checked value by value against the Recommendation's text),
**libebur128** 1.2.6 (`67b33ab`, 49-tap Hann interpolator), and on a 56-configuration hot subset a
**32×, 128-tap Kaiser (β 12) reference**. Macro positions follow the DESIGN §5.5 curves copied from
`MacroEngine.h`. The matrix (2718 configurations, 8 s each; 2496 with TP on):

| Tag | What varies (TP on unless stated) | n |
|---|---|---|
| A | 6 programmes (music, transient, LF, HF, sustained, fs/4 45° ISP vector) × Loudness 0/25/50/75/100 % × 8 OS cells (Off; 2× min/lin; 4× min/lin; 8× min; 16× min/lin) × 4 voicings (Transparent/Punchy/Loud at Transients 50 %, Transparent at 100 %) × ceilings −0.1 / −1.0 dBTP | 1920 |
| B | the Force Max bounce (offline, 16×) × the same programmes / Loudness / voicings / ceilings | 240 |
| C | a +6 / +12 dB Post-EQ high shelf into the clamp, Off/4×/16× | 72 |
| D | 44.1 / 96 / 192 kHz, Off/2×/4× | 108 |
| E | block sizes 64 / 480 / 4096 | 24 |
| F | limiter lookahead 0.5 / 5 / 9.8 / 10 ms | 96 |
| G | Character 0.5 | 36 |
| H | **TP off** — the bit-identity control (hashes of the whole output) | 222 |

The one OS cell tag A did not carry, 8× linear phase, was added as an addendum (240 more
configurations, same axes) — §Verification 1.

**Before (`main`), TP on, over the ceiling by more than 0.1 dB:**

| Meter | configurations over | worst | median |
|---|---|---|---|
| Product dBTP meter | **1674 / 2496** | **+4.80 dB** | +0.53 dB |
| BS.1770 Annex 2 example filter | 1759 / 2496 | +6.12 dB | +0.59 dB |
| libebur128 | 1710 / 2496 | +5.41 dB | +0.56 dB |
| 32×/128-tap reference (hot subset) | 53 / 56 | +7.80 dB | — |

By axis (tag A; before = `main`, after = this branch):

| OS cell | before over (product / Annex 2) | before worst | after worst (product / Annex 2 / libebur128) |
|---|---|---|---|
| Off | 105 / 136 of 240 | +2.79 / +3.05 | +0.000 / +0.001 / +0.101 |
| 2× min | 177 / 184 | +3.66 / +4.28 | +0.000 / +0.002 / +0.083 |
| 2× lin | 166 / 155 | +3.47 / +3.96 | +0.001 / +0.001 / +0.121 |
| 4× min | 182 / 186 | +3.85 / +4.53 | +0.000 / +0.001 / +0.065 |
| 4× lin | 180 / 186 | +3.73 / +4.06 | +0.000 / +0.004 / +0.124 |
| 8× min | 174 / 184 | +3.98 / +4.69 | +0.000 / +0.002 / +0.081 |
| 16× min | 162 / 163 | +3.79 / +4.46 | +0.002 / +0.000 / +0.076 |
| 16× lin | 155 / 161 | +3.78 / +4.48 | +0.001 / +0.004 / +0.165 |
| Force Max (tag B) | 162 / 163 | +3.79 / +4.46 | +0.002 / +0.000 / +0.076 |

| Programme | before over (product / Annex 2) | before worst | after worst (product / Annex 2 / libebur128) |
|---|---|---|---|
| transient-heavy | 303 / 305 of 320 | +3.98 / +4.69 | +0.000 / +0.004 / +0.165 |
| HF-heavy | 300 / 320 | +3.78 / +4.20 | +0.001 / +0.004 / +0.121 |
| music | 230 / 247 | +3.53 / +3.76 | +0.000 / +0.000 / +0.083 |
| sustained | 282 / 264 | +1.33 / +1.56 | +0.002 / +0.002 / +0.086 |
| fs/4 ISP vector | 186 / 219 | +3.11 / +3.13 | +0.000 / +0.001 / +0.000 |
| LF-heavy | **0 / 0** | +0.08 / +0.08 | +0.000 / +0.000 / +0.011 |

| Loudness | 0 % | 25 % | 50 % | 75 % | 100 % |
|---|---|---|---|---|---|
| before over (product), of 384 | 163 | 258 | 280 | 292 | 308 |
| before worst (product / Annex 2) | +1.98 / +2.32 | +2.45 / +3.05 | +3.27 / +3.86 | +3.75 / +4.44 | +3.98 / +4.69 |

The +12 dB Post shelf (tag C) is the worst case before (+4.80 product / +6.12 Annex 2). Only the
LF-heavy programme passes on `main`: it has no content for the mechanisms below to act on.

**Root cause — in terms of the stages the round asked about.** The defect is in **clamp application**:
the only stage placed to enforce a true-peak ceiling (ADR-0006 put the guarantee there precisely
because it is last) enforced a sample ceiling. Everything that puts the overs in front of it is by
design upstream and cannot be the guarantee:

- **oversampling / domain conversion** — the down-sampling filter regrows peaks after the region
  limiter at every factor (the limiter works on the oversampled signal; the decimator's ringing is
  added after it), and at ≥ 4× the limiter's own TP detection is off by ADR-0003 item 6 — correct
  for its detector, blind to what the down-filter adds;
- **the limiter's attack is slewed on purpose** (Transients, the Punchy style) and lets fronts through
  to the clamp — the voicing the product sells;
- **ordering** — a Post-position EQ boost sits between the limiter and the clamp (ADR-0002);
- **the clamp itself** — at base rate a sample clip manufactures inter-sample overs of its own.

Not the cause: level detection in the limiter (it holds what it measures), latency alignment (the
impulse and reported figure matched before and after), gain staging. So the fix is ADR-0006
items 2–3 as decided, not a limiter change, and not a margin.

**Which true peak — the estimator question the fix could not skip.** The clamp must act on a reading
of the waveform between samples, and the meters disagree about it. On the 56-configuration hot
subset (`sub.txt`: every programme at the hottest points, OS Off/2×/4×/16×, the shelf), a
prototype clamp (16 phases, attack 0.25 ms) driven by each candidate:

| Clamp driven by | product meter | Annex 2 | libebur128 | 32× reference |
|---|---|---|---|---|
| (before: `main`) | +4.80 | +6.12 | +5.41 | +7.80 |
| F — the product's 4× estimator alone (ADR-0006 item 2 read literally) | +0.004 | **+1.44** | +0.66 | +2.67 |
| 4× estimator + Annex 2 | +0.003 | +0.005 | +0.23 | +1.73 |
| G — an accurate interpolator alone (16 phases × 32 taps, Kaiser β 8) | **+0.63** | +0.34 | +0.71 | +0.93 |
| accurate + 4× estimator | +0.003 | +0.29 | +0.27 | +0.93 |
| **H — max of accurate, 4× estimator, Annex 2** | +0.003 | +0.004 | +0.18 | +0.93 |
| H as shipped (4 points + parabolic refinement) | +0.003 | +0.004 | +0.18 | +0.98 |
| H with a 64-tap accurate kernel | +0.002 | +0.004 | +0.16 | +0.36 |

(Worst over the ceiling, dB, 56 configurations.) The 12-tap Blackman estimator reads HF-rich output
up to ~1.4 dB **below** the Annex 2 filter and dense broadband up to ~0.24 dB **above** an accurate
interpolator — so a clamp driven by any one meter holds the ceiling on that meter only. A longer
attack does not close that gap: on option G alone, 0.5 / 1 / 2 ms took the product-meter residual
from +0.63 to +0.49 / +0.38 / +0.38 dB — still over — and on option H, 0.5 ms against 0.25 ms moved
nothing a meter could resolve (libebur128 +0.18 both, reference +0.93 both) while doubling the attack's
share of the allowance. 0.25 ms was kept. Filtering the output to 20 kHz before the reference makes it
read **higher** (+0.98 → +2.15 dB), not lower: the peak of near-Nyquist content depends on the
reconstruction filter, which is why no meter is "the" truth there (KNOWN_ISSUES KI-020).

**The lookahead constraint.** A gain that must not step needs to see the peak before it arrives: the
detector's 16-sample lag plus an attack. ADR-0004 (Accepted) fixes the reported latency at
`maxLookahead(10 ms) + osLatency` and gives the whole 10 ms to the limiter's line, so either the
reported figure grows (a PDC change on every session, or one that moves with the TP switch — the
PDC spray ADR-0004 exists to prevent) or the line shrinks. ADR-0041 records options A–E; the chosen
one (C) takes D = attack + 30 samples out of the allowance **in TP mode only** and latches the
composition like an oversampling change.

### 3. STATE-002 — what a factory preset does to TP, Dither and Shaping

- `PresetManager::applyFactoryPreset` writes every non-excluded parameter once, at "default + the
  table's intent" (`src/PresetManager.cpp`); `isPresetExcludedParam` (`src/PluginParameters.cpp`)
  excludes the view tier, Freeze and `advancedMode` only, and its own comment says why anything it
  does not name is reset by browsing. No factory table names `truePeakMode`, `dither` or
  `ditherShaping`, so every factory preset sets all three to their defaults — **off** (TP off by
  default since ADR-0015). This is the designed "defaults + intents" semantics, not an accident.
- A **user** preset (`applyPreset`) restores what it was saved with — including TP and dither.
  Loading and applying are the same operation in this design; the factory/user difference is the
  defaults pass.
- With LOCK on, the ceiling's VALUE is skipped (`ceilingLocked && id == pid::ceiling`) and survives;
  TP is not lockable, so "−1.00 dBTP" becomes "−1.00 dB", a sample-peak limit. ADR-0010 (Accepted)
  fixes the lockable set at `{ceiling}` and rejected a wider set (its option I).
- What a user reasonably expects: LOCK exists for the delivery ceiling, and the delivery ceiling is
  value **and** unit. The audit's evidence stands; nothing on `main` disproves it. Undo restores all
  three.

### 4. STATE-004 — what FREEZE means, and what a re-prepare did to it

- Freeze is the repeatability control: while frozen the adaptive layer contributes a constant
  (`MODE_AND_ADAPTATION_POLICY.md` invariant 3). What is frozen is the **applied trim vector** (four
  trims — limiter release, stereo link, comp detector HPF, Dynamic Tame tilt).
- Every host `prepareToPlay` runs `AdaptiveEngine::reset()`, which zeroed the applied vector. With
  Freeze on, `finishBlock` never re-slews, so the zeros were HELD: the audio ran on no adaptive trims
  while FREEZE stayed lit and the session save still wrote the latched vector (the retained set,
  rounds 40–42). The audit measured ~0.3–0.4 dB RMS and ~0.5 dB per-channel GR between the frozen
  audition and a render after a re-prepare. KNOWN_ISSUES KI-006 had carried this as its open
  "audio half" since 2026-08-03.
- The frozen state is persistent (saved, ADR-0014 restores it), so a session-local reset was
  inconsistent with it — the indicator, the save and the audio gave three answers.
- Two constraints from earlier rounds bound the fix: no ValueTree access on a host callback (the
  round-40 TSAN race), and no retained-generation bump on reset (the round-42 slot-isolation
  defect: an engine-wide vector serialised by the wrong A/B slot). The audit's own challenge
  recorded the second.
- A related shape, not the re-prepare trigger: an A/B switch into a freeze-ON slot with no
  `FROZEN_TRIMS` keeps the outgoing slot's latch in the audio while that slot saves none
  (code-inferred; KI-007 item 10).

### 5. UX-003 — the Save panel

- The Save panel is the product's own overlay (`src/gui/PluginEditor.cpp`, the Save panel's OK
  handler) writing `<user preset folder>/<name>.anabasis` through `PresetManager::savePreset`, which
  replaces unconditionally. **No platform file chooser is involved on save**, so there is no platform
  confirmation to reuse (the chooser is used for *loading*, and that is preserved).
- The overwrite is documented (USER_MANUAL §7.2: "Saving over an existing name overwrites it"), and it
  is the inherited family convention: Anamorph's manual documents the same (read-only reference).
  `BRAND_CONSISTENCY_CHECKLIST.md` §A lists the preset system as **must match**, and any confirmation
  needs new UI strings, which `DEVELOPMENT_BRIEF.md` C8 reserves to the maintainer.
- Affected: user presets only (factory presets are compiled in and are never written). The
  prefilled, all-selected name makes Return an intentional one-keystroke update of the loaded user
  preset; a typed name that exists — or becomes one after illegal characters are stripped — replaces
  that file silently.

### 6. DSP-004 — the clipper's top-end droop

- The clip stage is a first-order ADAA clipper (`ClipSat.h`); in its linear region its response is
  the kernel's own `(1 + z⁻¹)/2`, i.e. `cos(πf / (N·fs))` at N× oversampling, **whenever the drive is
  non-zero, whatever its amount** — it is by design (the ADAA trade recorded in `ClipSat.h`), not a
  defect. At the default oversampling (Off, Offline Follow) that is the base rate, and the Loudness
  macro engages the clipper above 30 %.
- Measured on the real engine (−30 dBFS sine, clipper linear, drive 0.07 dB — what Loudness 30.5 %
  reaches — and 3 dB, identical), relative to drive exactly 0: at 48 kHz Off −0.47 / −2.01 / −5.11 /
  −6.02 / −11.74 dB at 5 / 10 / 15 / 16 / 20 kHz; at 44.1 kHz Off −2.42 dB at 10 kHz and −16.74 dB at
  20 kHz; 4× ≤ 0.56 dB at 20 kHz. The full table is in KNOWN_ISSUES KI-005.
- What was missing was disclosure: nothing outside the code said so, and KI-005 named the wrong macro
  ("Character").

---

## Decisions

**DSP-001 — Proceed (ADR-0041, Proposed).** ADR-0006 is still the accepted direction and still
consistent with the product contract; the discrepancy is that its items 2–3 were never implemented.
Implementing them is the smallest architecture-consistent fix, but it is not gate-free, and the gate
items are named instead of avoided: (1) the TP path needs lookahead, which amends ADR-0004 items 1, 2,
7 **for TP mode only** — the reported latency does not move by a sample, only its composition;
(2) the clamp's reading departs from ADR-0006 item 2's wording ("the shared estimator") in one stated
respect — the largest of three readings on one window; (3) a DSP signal-flow change and a
DSP_POLICY amendment (invariants 2 and 8, prescribed text). Rejected alternatives, each measured or
argued in ADR-0041: reporting more latency (PDC change), a constant clamp delay in both modes
(TP-off stops being bit-identical), a reactive gain (cannot meet the tolerance), oversampling the
clamp (ADR-0006 option D), the shared estimator alone (holds only its own meter), an accurate
interpolator alone (the product meter reads +0.63 dB over), a fixed margin (the under-read is
0–1.4 dB, programme-dependent).

**The tolerance and the yardstick.** `DSP_POLICY.md` invariant 4 already fixes the tolerance —
≤ 0.1 dBTP in true-peak mode — and it was not changed. It does not name a yardstick; the audit put that
to the owner (DSP-001 sub-item (a)). This round's criterion: **≤ ceiling + 0.1 dB on the product's
dBTP meter AND on the BS.1770 Annex 2 example filter** — the Recommendation's own meter and the one
the user reads — with libebur128 and a long-kernel reference reported, not hidden (KI-020).

**STATE-002 — Defer, with interim disclosure.** The fix the audit recommends (LOCK holds value **and**
mode; the owner rules on dither under browsing) widens ADR-0010's lockable set, which that Accepted
record explicitly rejected — a hard stop ("conflict with an Accepted ADR") and a Parameter /
Serialization Registry semantic change to `int_ceilingLock`. Nothing was changed in code; KI-021 and
the manual (§3.2 Ceiling row, §7.3, the §8 transparent-master step 1, the Presets FAQ) now say
exactly what a preset does to TP, Dither and Shaping and what LOCK holds. No regression test: no
behaviour changed.

**STATE-004 — Proceed (ADR-0042, Proposed).** The round's own requirement — the visible FREEZE state
and the processing state must agree — decides between "make the indicator follow the audio" and
"make the audio keep the latch" in favour of the latter: the audio thread may not write a parameter,
and turning Freeze off would defeat its purpose. Among the carries (ADR-0042 options A–D), the chosen
one stashes the applied vector at `reset()` and lets the first block after it decide from its own
snapshot — so a Freeze engaged while stopped still counts — and republishes the applied set only, so
the retained set and its generation (round 42) do not move. Carrying the vector with Freeze OFF
(option A) is a separate owner call and is not done. The A/B trigger (KI-007 item 10) is not resolved;
ADR-0042 preserves its pre-existing shape and a test pins that the carry does not make it worse.

**UX-003 — Defer, with interim disclosure.** No platform mechanism exists to reuse; a confirmation
is a deviation from a must-match family convention (needs an ADR and the owner's sign-off, or a
family-wide proposal) and needs new UI copy (C8). The audit marks it "not a hard stop", but
implementing it would mean inventing the wording and overriding the brand checklist unilaterally.
KI-022 and USER_MANUAL §7.2 now state the behaviour precisely (including the stripped-character
collision) and carry the audit's recommended shape for the decision.

**DSP-004 — Preserve the DSP, disclose it, pin it.** Intentional and unavoidable for a first-order
ADAA kernel at the base rate; the remedies (a droop-compensating pre-emphasis on the driven branch, or
a non-Off oversampling default — a reported-latency change) are owner decisions and were not taken.
Disclosure: KI-005 (macro name corrected, the measured table, the workaround), the USER_MANUAL
Oversampling row and §8 step 1. The Oversampling **tooltip** is UI copy (C8) and was not changed.
Pinned by `testClipDriveDroopIsTheDisclosedOne`.

---

## Implementation

### 1. DSP-001 — the clamp's true-peak path (ADR-0041)

| | |
|---|---|
| **Finding** | DSP-001 (P0); the audit's merged sub-items (a) yardstick and (c) accurate estimator or stated margin |
| **Previous behaviour** | TP mode held the ceiling only on sample values; 1674 of 2496 TP-mode configurations over by > 0.1 dB on the product's own meter, worst +4.80 dB (+6.12 dB Annex 2), at every OS cell and in the Force Max bounce |
| **Root cause** | ADR-0006 items 2–3 never implemented — the clamp stayed the P1 sample clip in both modes (§Investigation 2) |
| **Change** | `ClampTruePeakDetector` (`TruePeak.h`): per step, the largest of (i) a 32-tap Kaiser β 8 interpolator at the quarter points with parabolic refinement (bounded to +0.5 dB), (ii) `TruePeakEstimator`'s own phases (now designed by the shared `designPhases`, arithmetic unchanged), (iii) the BS.1770 Annex 2 table; one 32-sample window, lag 16; both channels in one pass, the filters' mirror symmetry folded. `CeilingClamp::processFrameTruePeak`: one linked gain; per-segment requirement against the lower of its two samples' ceilings (each frame carries the ceiling the limiter used); minimum over the window; forward minimum over a 0.25 ms attack (≥ 8 samples) and its moving mean (a linear ramp never above the requirement); one-pole 10 ms release on the reduction with a snap to exactly unity; the sample clip kept as the backstop; below the ceiling an exact delay. `AnabasisEngine`: in TP mode the region line is `delaySamples − D` and the clamp delays D (D = attack + 30: 41/42/54/78 samples at 44.1/48/96/192 kHz); the limiter's window is capped at `delaySamples − D`; a TP toggle is a latched rewire at the §2.8 duck bottom, like an oversampling change; stage E computes the post-EQ frame for all channels and runs the clamp frame-wise. TP-off takes the old per-channel path, unchanged |
| **Why this change** | It is ADR-0006 as decided; reported latency constant in both modes; TP-off bit-identical; the three readings each close a failure the other two leave (the estimator table in §Investigation 2) |
| **Tests added / updated** | `testClampTruePeakDetector` (Annex 2 table symmetry and DC gains, centre taps, lag 16, the fs/4 vector, never below the product meter), `testCeilingClampTruePeakPath` (D = 42 at 48 kHz, exact delay below the ceiling, the ISP vector held, backstop, linked gain, release back to exactly 1.0), **`testTruePeakModeHoldsTheCeiling`** (the durable guard, below), `testTruePeakModeCapsTheWindowNotTheLatency`, `testDuckWrapsTruePeakLatch` (both directions), `testTruePeakModeIsExactBelowTheCeiling`; TP-mode loops added to `testReportedLatencyMatchesImpulse`, `testOsLatencyMatrix` (incl. Force Max), `testBypassNullUnderOs` and the allocation guard's rewire loop; `AnabasisBench` gains the `working+TP` mode and a clamp row |
| **Runtime validation** | The before/after engine matrix (2718 + 240 configurations, four meters), the negative control, the mutation checks, the bench (§Verification 1–5) |
| **Host validation** | Ardour 8.4 offline export through the built VST3, before and after, and Carla 2.5.8 (§Verification 6) |
| **Acceptance criterion** | Output true peak ≤ ceiling + 0.1 dB (DSP_POLICY invariant 4) on the product meter and the Annex 2 filter, at every OS cell × both phases, Force Max, three styles, Transients 50/100 %, both EQ positions, ceilings −0.1 / −1.0 dBTP, Loudness 0–100 %; reported latency unchanged; TP-off bit-identical; allocation-free |
| **Result** | **Met.** 0 of 2736 over on either held meter (the 2496 of the main matrix and the 240 of the 8× linear addendum), worst +0.005 dB; libebur128 130 of 2736 over, worst +0.18 dB; reference +0.98 dB on the hot subset (KI-020, owner decision). TP-off 222/222 hash-identical. Latency unchanged. Allocation-free |

**The durable guard — `testTruePeakModeHoldsTheCeiling` (`tests/dsp_tests.cpp`).** What it protects
against: TP-mode output exceeding the ceiling — the DSP-001 defect, and any later change that
re-opens it (a clamp that stops using its TP path, a detector that loses a reading, a lookahead that
stops fitting). Why the signals are representative: five programme shapes, each chosen for a
mechanism — transient-heavy (clicks, noise bursts, rim shots: the attack slew), HF-heavy (tones to
0.8·Nyquist over a hard square: decimation regrowth and the estimator's HF under-read), LF-heavy
(kick + sub — the shape that already passed on `main`, kept as the control), sustained (dense
near-ceiling limiting) and the canonical fs/4 45° vector; two macro operating points (Loudness 50 %
at the −0.1 dBTP default; Loudness 100 % at −1 dBTP with Punchy + Transients 100 %); a +12 dB Post
shelf; 44.1 and 96 kHz, where D differs. Tolerance: ≤ 0.1 dB (invariant 4) on the product meter AND
an independently implemented Annex 2 meter, per run. OS paths: all ten — Off, 2×/4×/8×/16× ×
minimum/linear, and the Force Max bounce — 123 runs of 0.3 s. A premise check fails if the renders
do not reach within 1 dB of the ceiling, so a clamp that "passed" by muting fails too. On `main` the
same test fails: 102 of 123 runs over, worst +6.04 dB.

### 2. STATE-004 — a frozen latch survives a host re-prepare (ADR-0042)

| | |
|---|---|
| **Finding** | STATE-004 (P1); KNOWN_ISSUES KI-006's audio half |
| **Previous behaviour** | A host re-prepare zeroed the applied trim vector and Freeze held the zeros; FREEZE stayed lit, the save wrote the latched vector, the audio ran on none |
| **Root cause** | `AdaptiveEngine::reset()` zeroes the applied vector unconditionally, and nothing restores it under Freeze |
| **Change** | `AdaptiveEngine::reset()` stashes the applied vector and its "real vector" flag once per reset sequence (the engine's `prepare` resets twice). At the top of the first block after it, before any pending ADR-0014 restore, `AnabasisEngine::process` calls `resumeAfterReset (p.freeze)`: Freeze ON → the stashed vector is applied and published again; OFF → nothing (the old restart from rest). The carry republishes the applied set only (`publishApplied`); the retained set and its generation are untouched. No thread crossing, no allocation |
| **Why this change** | The only option that makes indicator, save and audio agree without a thread crossing (round 40) or moving slot ownership (round 42), and that counts a Freeze engaged while stopped (ADR-0042 options A–F) |
| **Tests added / updated** | `testAFrozenLatchSurvivesARePrepare` (48 → 48 kHz and 48 → 96 kHz with a new block: render bit-identical to the same vector restored and frozen, with every trim-reached stage engaged; a zero-trim control proves the vector audible; published vector equals the latch; generation unchanged; Freeze engaged while stopped; Freeze OFF unchanged); `testPreparedStateAndSlotOwnership` case 4 (state suite: nothing applied between the reset and the next block, the latched vector back after one block, generation unchanged, save intact); `testAFrozenLatchDoesNotFollowTheSlotSwitch` gains a re-prepare (slot B still saves no vector; slot A unchanged) |
| **Runtime validation** | Engine and processor level (the tests above) plus negative controls: with the carry removed every assertion above fails; written with a retained-generation bump (`publishTrims(true)`), the two slot-isolation assertions fail |
| **Host validation** | Carla 2.5.8 deactivate/activate ×3 with Freeze and every parameter preserved — but its Dummy engine carries no audio, so the audio half was **not observable in a host** (§Verification 6) |
| **Acceptance criterion** | Freeze on with a latched vector: prepareToPlay at the same rate and block, a new rate and a new block size leaves the published trims equal to the pre-prepare values and the audio equal to that vector frozen; "A/B into a vectorless Freeze-ON slot → re-prepare → save" writes no `FROZEN_TRIMS`; `testAFrozenLatchDoesNotFollowTheSlotSwitch` and `testNullWithDefaults` stay green |
| **Docs** | ADR-0042; `MODE_AND_ADAPTATION_POLICY.md`; KNOWN_ISSUES KI-006 (fix pending the owner) and KI-007 item 10 (the A/B trigger); USER_MANUAL §4 (Freeze's promise names the re-prepare and points at the A/B case) |
| **Result** | **Met at engine and processor level; not verified in a DAW** |

### 3. DSP-004 — disclosure and a pin

`testClipDriveDroopIsTheDisclosedOne` measures the clip stage's linear-region response on the real
engine at 48 kHz: −2.01 ± 0.05 dB at 10 kHz and −11.74 ± 0.1 dB at 20 kHz at Oversampling Off,
identical at drive 0.07 and 3 dB, and better than −0.6 dB at 20 kHz at 4×. It fails if the droop
changes in either direction, so a future remedy has to update the disclosure with it. Docs:
KNOWN_ISSUES KI-005, USER_MANUAL (Oversampling row, §8 step 1), TEST_REPORT.

### 4. STATE-002 and UX-003 — documentation only

KI-021 and KI-022 (new), USER_MANUAL §3.2, §7.2, §7.3, §8 and the FAQ. No code.

The manual's true-peak text changed with DSP-001 as well: §3.2's Ceiling row and §3.3's TP switch
describe the mechanism (the limiter's detection and the final clamp, at every oversampling
setting), §6 states the tolerance and points at KI-020, the STATISTICS TP row says it can warn at
the ceiling in TP mode, and §3.3's Lookahead range states the TP-mode cap.

### 5. Documentation synchronised with the code

ADR-0041 and ADR-0042 (Proposed) and their ADR_INDEX rows; dated banners on ADR-0004 (proposed
amendment), ADR-0006 (implemented by 0041; the evidence cell in ADR_INDEX corrected), ADR-0014
(extended by 0042) and ADR-0015 (correction of record for decision 7's last sentence) — nothing in
an Accepted body rewritten. `DSP_POLICY.md` invariants 2, 4, 8 and the enforcement map;
`MODE_AND_ADAPTATION_POLICY.md` (the two-set rationale, Freeze across a re-prepare);
`LATENCY_MODEL.md`, `REALTIME_SAFETY_AUDIT.md`, `PERFORMANCE_BUDGET.md` (re-measured table);
KNOWN_ISSUES KI-005, KI-006, KI-007 item 10, KI-020 – KI-022; FUTURE_RISKS RISK-003 (triggered,
mitigated); TEST_REPORT; `procedures/TESTING.md`; USER_MANUAL; CHANGELOG 0.2.13; version 0.2.13;
HANDOVER; README's check count. `TruePeak.h`'s header comment corrected where it overstated the
estimator's accuracy (it cited ≤ 0.1 dB, true only for the fs/4 vectors).

---

## Verification

### 1. The P0 matrix, after

2496 TP-mode configurations, TP on, over the ceiling:

| Meter | before (`main`) | **after** |
|---|---|---|
| Product dBTP meter | 1674 over, worst +4.80 dB | **0 over, worst +0.005 dB** (median +0.000) |
| BS.1770 Annex 2 example filter | 1759 over, worst +6.12 dB | **0 over, worst +0.004 dB** |
| libebur128 | 1710 over, worst +5.41 dB | 98 over, worst +0.183 dB (median +0.007) |
| 32×/128-tap reference, hot subset | 53/56, worst +7.80 dB | 39/56, worst +0.98 dB |

The 98 libebur128 overs are HF- and transient-heavy synthetic programme and the Post-shelf cases
(tag C: 16 of 72). **8× linear phase addendum** (240 configurations, tag A's axes): `main` 161 / 173
over (product / Annex 2), worst +3.67 / +4.58 dB; **after 0 / 0 over, worst +0.001 / +0.001 dB**;
libebur128 32 over, worst +0.177 dB — the transient and HF programmes, the same pattern as the other
linear-phase cells (2× lin 16, 4× lin 32, 16× lin 24 of 240; the minimum-phase cells 0 at 8× and 16×).
With it, every OS cell has been measured: **0 of 2736 over on either held meter.**

The product meter is the estimator the STATISTICS TP row reads, on the same render tap; with bypass
and the monitor aids off that tap is the output — so the audit's harness criterion ("after a reset,
the TP row reads ≤ ceiling + 0.1 dB at 4×, Loudness 50 %"; `main` read 1.27–1.39 dBTP there) is met at
engine level. The GUI harness itself was not re-run.

**TP off:** 222 of 222 configurations hash-identical to `main` (every OS cell, Force Max, the
shelf, lookahead 2 and 10 ms). **Level cost in TP mode** (RMS, after − before): median −0.064 dB;
the largest drops (−5 to −6.6 dB) are the +12 dB Post-shelf stress cases that rendered ~+4.7 dB
true-peak overs before.

### 2. The regression guard fails on `main`

`testTruePeakModeHoldsTheCeiling` transplanted against `main`'s engine: **102 of 123 runs over on
either meter, worst +6.04 dB** — the test FAILS; on the branch 0 of 123, worst +0.001 dB.

### 3. Mutation checks (each reverted after)

| Mutation | Caught by |
|---|---|
| the region line not shortened in TP mode (clamp delay added on top) | `testReportedLatencyMatchesImpulse` / `testOsLatencyMatrix` TP loops (impulse ≠ reported) |
| the engine not routing TP mode through the TP path (the clamp stays the sample clip) | `testTruePeakModeHoldsTheCeiling` |
| the limiter window not capped | `testTruePeakModeCapsTheWindowNotTheLatency` |
| the TP toggle applied without the latch | `testDuckWrapsTruePeakLatch` |
| STATE-004 carry removed | `testAFrozenLatchSurvivesARePrepare`, state case 4 |
| STATE-004 carry written with the retained bump | `testAFrozenLatchDoesNotFollowTheSlotSwitch` (both new assertions) |

### 4. The contract the fix must not move

- **Reported latency** — unchanged in both modes: `testReportedLatencyMatchesImpulse` and
  `testOsLatencyMatrix` run in both modes (every factor, both phases, Force Max; the impulse lands
  on the reported figure); Carla reported 480 samples at 48 kHz before and after four TP toggles.
- **DSP order** — unchanged; the clamp's position is ADR-0002's. **Bypass** — `testBypassNullUnderOs`
  in both modes. **Output gain** — below the ceiling the TP path is an exact delay
  (`testTruePeakModeIsExactBelowTheCeiling`). **Oversampling** — unchanged; TP now latches with the
  factor at the duck bottom. **Parameter/state semantics** — no parameter, ID, range or schema change.
- **Realtime** — the allocation guard (2040 `process()` calls across 80 configurations, TP
  alternating per rewire pass) sees no `new`/`malloc`; `scripts/check-realtime.py` clean.
- **Suites** (rebuilt after the version bump): `AnabasisTests` **540 checks**, `AnabasisStateTests`
  **1429 checks**, 0 failures.

### 5. Cost

`AnabasisBench`, 5 runs/cell, median: the clamp's TP path alone 107 ns/sample (0.51 % of one core at
48 kHz). Budget case (48 kHz · 512 · 4× · working) 3.00 % → **3.60 %** with TP on; 48 kHz · 512 · Off
1.34 → 2.17 % (TP mode also turns on the limiter's estimator at Off/2×); 16× 9.37 → 10.05 %. Inside
the ≈ 5 % target at the budget case, over DESIGN §9's limiter + TP-detection row at ≥ 4× — recorded in
`PERFORMANCE_BUDGET.md`. First attempt was 603 ns/sample; the 4-point grid + parabola, split
accumulators, folded symmetry and idle counters brought it to 107.

### 6. Hosts

Available here: **Carla 2.5.8** (plugin host, scripted through its Python backend, Dummy engine),
**Ardour 8.4** (DAW, headless: `ardour8-lua` session scripting + `ardour8-export`), **pluginval**
(the repository's gate). Not available: REAPER, Logic, Cubase, Pro Tools, Studio One, Bitwig; Windows
and macOS; any audio device; any GUI host session (no DAW editor, no keyboard/mouse through a host).

**pluginval** — strictness 10 (`ANABASIS_PLUGINVAL_STRICTNESS`), both modes × 3 consecutive passes,
editor under Xvfb, on the rebuilt 0.2.13 VST3: passed.

**Ardour 8.4 — offline render through the built VST3 (real DAW, headless).** Ardour's VST3 scanner
(`ardour-vst3-scanner`) indexed the bundle; a Lua session script built a 48 kHz session with a custom
Lua DSP generator (deterministic hot programme, four 1.5 s segments: broadband bursts at up to
+4 dBFS; an fs/4 45° sine with bursts; a 55 Hz sine with bursts; all together) followed by Anabasis
on the master bus, parameters set through Ardour's own plugin-control API (Limiter Gain 12 dB,
Ceiling −0.10 at its default, True Peak on or off; Oversampling at its default Off, which a host
cannot set), a 6 s session range, and `ardour8-export -b float`. Measured on the exported file:

| Render (Ardour export, 32-bit float) | sample peak | product meter | Annex 2 | libebur128 | 32× ref |
|---|---|---|---|---|---|
| `main` 0.2.12, **TP on** | −0.39 | **+0.02 dBTP** | **+0.87 dBTP** | +0.40 | +2.14 |
| branch 0.2.13, **TP on** | −0.39 | **−0.10 dBTP** | **−0.10 dBTP** | −0.01 | +0.74 |
| `main`, TP off | −0.10 | +3.43 | +4.25 | +3.83 | +5.25 |
| branch, TP off | −0.10 | +3.43 | +4.25 | +3.83 | +5.25 |

The defect reproduces in the host (`main` TP on: +0.97 dB over the ceiling on Annex 2) and the fix
holds there (branch: exactly the ceiling on both held meters; libebur128 +0.09 dB over, inside the
0.1 dB tolerance; the reference's residual is KI-020's). **The TP-off renders are sample-identical
between `main` and the branch.** Two exports of the same session are sample-identical (their file
hashes differ only in the header). The TP-off rows show what TP off promises and does not: the
sample peak sits exactly at the ceiling, inter-sample peaks do not.

**Carla 2.5.8 (Dummy engine, 48 kHz / 512).** Load; latency reported 480 samples; four TP toggles
(latency unchanged); TP, Freeze and Ceiling set; three deactivate/activate cycles (the host-driven
re-prepare Carla offers with the Dummy engine — its buffer/rate change call is refused there) with
every value preserved; the plugin's Bypass parameter toggled; a state chunk restored into a second
instance round-trips TP, Freeze and Ceiling. Its engine carries no audio, so nothing audio-side was
observed there.

**What this does and does not establish** — real host: the plugin loads, reports and keeps its
latency, renders offline in a DAW with the TP ceiling held and TP-off unchanged, survives host
activate cycles and a state round trip. **Simulated only** (engine / processor tests): the Freeze
carry's audio, the re-prepare triggers themselves. **Not verified**: whether any DAW re-prepares on
transport start or before a bounce (the audit's TEST-002 question behind STATE-004's priority);
Force Max in a host bounce (Ardour's export did not enable it — Offline Render is a Settings value a
host cannot set); DAW bypass mapping; automation recording; the preset Save flow and LOCK inside a
host editor; keyboard delivery; REAPER / Logic / Cubase; Windows / macOS.

---

## Remaining limitations

- **The yardstick is the owner's (KI-020).** The ceiling holds on the product meter and the Annex 2
  filter; libebur128 reads up to +0.18 dB and a 32×/128-tap reference up to +0.98 dB on synthetic
  programme with near-Nyquist energy. A 64-tap accurate kernel was measured to bring the reference to
  +0.36 dB at twice the lookahead share and CPU; not adopted without the owner.
- **Voicing not listened to.** The attack (0.25 ms) and release (10 ms) are ⊕ listening material. At
  OS ≥ 2× the TP path works as a second fast limiter on decimation regrowth and on the
  Transients/Punchy poke-through, so TP-mode voicing changes where it used to over-shoot.
- **TP mode caps the limiter's longest lookahead at 10 ms − D** (9.125 ms at 48 kHz). Invisible below
  that setting; the manual says so.
- **The GR meter still shows the limiter only**; the clamp's own reduction is not displayed.
- **The STATISTICS TP row can warn at the ceiling in TP mode.** The clamp holds the row's estimator
  to the ceiling within the 0.1 dB tolerance, not exactly below it: 74 of 2736 TP-mode renders read
  0.001–0.005 dB over (a gain moving inside the interpolation window), which the row's exact compare
  paints warn while printing the ceiling. VIS-002's half-print slack would clear all but one; it
  amends ADR-0020 Amendment 2 and is the owner's. Disclosed in KI-020 and the manual's TP row; the
  meter view's comment that the TP row "measures a quantity the clamp does not bound" is corrected.
- **CPU:** TP mode at ≥ 4× is over DESIGN §9's limiter + TP row (inside the ≈ 5 % target at the
  budget case). TP is off by default.
- **Hosts:** see §Verification 6 — no Windows/macOS host, no commercial DAW, no GUI host session;
  STATE-004's audio carry is verified at engine/processor level only.
- **The audit's other Phase 0 workstreams were not in this round's scope** and remain open:
  DSP-003 (TP-per-factor manual/tooltip copy — the tooltip "Catch inter-sample peaks — the Ceiling
  then holds in dBTP" is now true, the finer copy is C8), VIS-002 (TP row slack), VIS-008 (stale
  holds after a ceiling change), TEST-004 (paint-rule statics), UX-018 (Save status line), DSP-007
  (backstop/clip-error acceptance at OS ≥ 2× — not measured), and TEST-002's COMPATIBILITY_MATRIX
  rows (need the hosts above).
- **Owner decisions this round leaves open:** ADR-0041 and ADR-0042 themselves; the yardstick;
  STATE-002 (lockable set, dither under browsing); UX-003 (confirm-on-overwrite); DSP-004 (droop
  remedy or oversampling default); the Freeze-OFF carry (ADR-0042 option A); the A/B trigger
  (KI-007 item 10).

### Newly discovered this round

- README's check count was stale (1787); corrected to the current 1969. (The trigger map's DSP rows
  also name `DSP_ALGORITHMS.md`, `SIGNAL_FLOW.md` and `DSP_GRAPH_REFERENCE.md` — not drift:
  `REPOSITORY_MAP.md` lists them as still planned. The change was synced to the documents that exist.)
- `TruePeak.h`'s header claimed ≤ 0.1 dB estimator accuracy generally; true only at fs/4 (~0.15 dB
  there, up to ~0.69 dB near Nyquist; ~1.4 dB below Annex 2 on HF-rich programme). Corrected; KI-020.
- ADR_INDEX's evidence cell for ADR-0006 described the TP guarantee as implemented; corrected with a
  dated note. ADR-0015 decision 7's "the DSP was right about its own guarantee the whole time" was
  true of the policy text, not the code; a correction-of-record banner says so.
- Carla (a JUCE-based VST3 host here) prints a host-side JUCE assertion, `restartComponent` called
  off the message thread, when the plugin reports its latency from `prepareToPlay` — **identically on
  `main`** (4 occurrences in the same script on both builds). Most likely an artefact of driving Carla
  from a Python thread; not diagnosed further, not caused by this round.

---

## Phase 1 readiness

Phase 1 ("comparisons and session figures tell the truth": matched BYPASS, one monitor-state
indicator, an explicit STATISTICS reset with visible scope, live/held/stale states) is **not ready to
implement as a whole**. It needs, before code:

- **Architecture decisions:** UX-009 moves the MATCH gain onto the wet leg — a DSP signal-order change
  on the monitor stage (hard stop) and a dated ADR-0006 D8 amendment; VIS-001 conflicts with Accepted
  ADR-0020's session-cumulative contract (amendment or superseding ADR); UX-002 changes a family
  convention (Anamorph's click-to-reset: owner acknowledgement + brand checklist note); a Thread
  Model review for the new meter-row scalars (ADR-0011).
- **UX copy first:** every new tag, caption and indicator word (RESET, "since reset", HOLD,
  "pre-monitor", "NO AUDIO", the MATCH readout) is maintainer copy (C8).
- **Host evidence:** VIS-005 depends on which hosts stop calling `processBlock` when stopped or
  bypassed, and UX-010 on the realtime-print path — TEST-002 data this round could not collect
  (§Verification 6).
- **Phase 0 dependencies still open:** TEST-004's `formatReading`/warn statics, which VIS-012 and
  VIS-014 build on.

What this round does give it: the TP ceiling the STATISTICS TP row is judged against now holds, so
Phase 1's session figures will be measuring a correct master; the durable meters used here (Annex 2,
the product estimator) are in the suite. **One Phase 1 item can start without a gate:** TEST-001 (a
public `refreshFromModel()` and mutation-checked tick tests) is test infrastructure, lands first by the
audit's own ordering, and changes no product behaviour.
