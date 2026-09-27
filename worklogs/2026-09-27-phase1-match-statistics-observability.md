# 2026-09-27 — Phase 0 accepted, Phase 1 begun: MATCH ordering, STATISTICS reset and scope, numeric observability

The third round on PR #42. Asked for: accept the Phase 0 decisions (ADR-0041, ADR-0042), decide the
delivery-meter definition from the existing evidence, close the PR #42 review item by item, prepare
TEST-001 if behaviour-neutral, then begin Phase 1 of the 2026-09-26 audit — MATCH's signal ordering,
the STATISTICS reset and scope, and numeric observability groundwork — with no P2/P3 work. The
closure record is [`docs/reports/2026-09-27-phase0-closure.md`](../docs/reports/2026-09-27-phase0-closure.md);
this file is the evidence behind it and behind the Phase 1 changes.

Same container and toolchain as the two earlier rounds (Linux x86-64, Xeon @ 2.10 GHz, 4 cores; GCC
13.3 Release + LTO for the suites and the plug-in). Engine figures come from the real
`AnabasisEngine`; scratch probes are not committed, and their method is written down here.

## Phase 0 re-verification (on the tree the acceptance records)

Re-run before the acceptance commits, on the review head's code (unchanged since the review round):

| Check | Result |
|---|---|
| Steady-state TP matrix, 2718 configurations (+ the 240-configuration 8× linear addendum) | **hash-identical** to the review round's renders, 2718/2718 and 240/240; 56-configuration hot subset identical |
| Over the ceiling by > 0.1 dB (2496 main-matrix TP-on configurations) | product meter **0** (worst +0.005 dB), Annex 2 **0** (worst +0.004 dB), libebur128 98 (worst +0.183 dB) |
| TP-off identity | **222/222** hash-identical to `main` |
| Transition sweep, TP switched on mid-stream | **0 of 248** over; worst +0.001 dB (product) / +0.003 dB (Annex 2); 228 of the runs carry a +12 dB Post shelf |
| Ardour 8.4, TP switched on mid-export through the plug-in parameter API | toggled render −0.047 dB (product) / +0.000 dB (Annex 2) vs the ceiling after the toggle, 0 samples over +0.1 dB; the same window with TP off +2.948 / +3.146 dB; re-exports bit-identical to the review-round renders; TP-off export identical to the PR-head build's |
| DSP suite (latency tests in both TP modes included) | green |

**Anomaly found and resolved on the way (host):** the first Ardour re-export peaked at +6 dBFS and the
toggle had no effect — the plug-in was not in the chain. The VST3 had been relinked after Ardour's
plug-in scan, so Ardour dropped it from the session. After
`ardour-vst3-scanner -f ~/.vst3/Anabasis.vst3` the re-exports matched the earlier renders bit for bit.
Rule for this environment: rescan after every VST3 rebuild before an Ardour run.

**ADR-0042's ordering, re-checked on the accepted tree:** moving `resumeAfterReset` after the staged
ADR-0014 injection fails four DSP checks (`freezeRePrepare` ×2 premises and results) and the state
suite's "an unprimed session load restores the vector on the first block" — the same five as the
review round. Restored; both suites green.

**The Annex 2 lower bound (ADR-0043's guards):** with the clamp detector's two Annex 2 readings
removed, the new `clampDetector` Annex 2 bound, the `tpCeiling` Annex 2 check and the `tpEngage`
Annex 2 check fail (plus the per-axis worst-case checks), and the product-meter checks keep passing.

## MATCH (audit UX-009) — the signal flow first

**Stage E, per base sample, before the change** (`AnabasisEngine.cpp`): clamp → duck → TP engagement
tail → dither → `delayedDry` → the §2.7 measure taps (pre-monitor) → `dryForDelta` → `wetLeg` (the
delta substitution) → **render tap** (bypass mix of `processed` and `delayedDry`; no delta, no monitor
gain; every meter, the spectrum output trace and the published readings read this) → bypass
crossfade `out = mix (wetLeg, delayedDry, bypassMix)` → **`out *= monGainNow`** → output.

So with MATCH on, settled BYPASS played g·dry: both legs scaled by the same g, the comparison kept its
ratio. The only reader of the post-mix `out` is the host buffer. Anabasis has no output-gain stage.
MATCH, DELTA and BYPASS are view-tier parameters (not in A/B, undo or presets; saved in the session
root); the host's bypass is `pid::bypass` itself, so a host bypass takes this exact path. The dry ring
is captured before input gain, and predict includes `inputGainDb`, so BYPASS at unity is the
unprocessed input. MATCH is inert offline (the engine snaps g to 1).

**The mismatch confirmed before any change** — the new tests, run on the old order:

- `matchJump` (Loudness 70 % point, Kellet pink, decorrelated channels at −17 dBFS RMS, dry short-term
  −14.3 LUFS): BYPASS − matched processed = **−6.76 LU with MATCH on, −6.76 LU with MATCH off**;
  BYPASS itself read −21.72 LUFS against a −14.33 LUFS input.
- `matchBypass`: settled BYPASS with MATCH on, and with MATCH + DELTA on, is **not** the bit-exact
  delay-aligned input.
- `matchWrapper` (processor, Loudness 70 % through the macro): settled BYPASS differs between MATCH on
  and off.

The scratch probe (the same engine compiled twice, momentary and short-term on the listened output,
32 s runs with BYPASS at 10 s) agrees: momentary jump −6.34 LU (MATCH off) / −6.30 LU (MATCH on);
settled BYPASS with MATCH on −7.33 dB against the input.

**The change** (ADR-0044): `wetLeg *= monGainNow` after the delta substitution and before the bypass
crossfade, with the exact skip at unity; the post-mix multiply removed. After it: `matchJump`
**+0.63 LU** (short-term; probe momentary +0.82 LU), settled BYPASS bit-exact in every monitor state.

**What else moved — old engine vs new, sample by sample** (probe, both sources compiled identically,
pink and a kick-and-pink bed, 1 536 000 samples each):

| Scenario | Listened output | Render-tap short-term, per block |
|---|---|---|
| MATCH off, BYPASS toggled | identical | identical |
| MATCH on, BYPASS toggled | identical until sample 479 744 (the first bypassed block), different after | identical |
| MATCH + DELTA, BYPASS toggled | identical until the first bypassed block | identical |
| DELTA alone, BYPASS toggled | identical | identical |
| offline MATCH + DELTA, BYPASS toggled | identical (and identical to offline plain) | identical |

**Interactions checked:** the bypass crossfade now runs g·processed → input (both near the input's
loudness; endpoints exact); the §2.8 duck multiplies `processed` and so commutes with g; the TP
engagement tail is part of `processed` and is scaled by g as before; the limiter, clamp and dither are
upstream and untouched; A/B, undo and preset applies do not touch the monitor state (single engine
members, view-tier parameters); a restored MATCH + BYPASS session now plays unity input at once;
host-automated BYPASS with MATCH on prints unity input in a realtime print (offline unchanged); the
statistics read the render tap and do not move (`matchWrapper` checks S, I and the TP hold exactly).

**The residual is MATCH's own estimator (audit DSP-005) — measured, not fixed:**

| Pink, per channel | input − matched (M) | clip drive 0 | compressor idle (thr 0, ratio 1.5) |
|---|---|---|---|
| −17 dBFS (dry −14.3 LUFS) | +0.82 LU (S +0.63) | +0.30 LU | +0.81 LU |
| −12 dBFS (dry −9.3 LUFS) | +1.73 LU | +0.44 LU | +1.39 LU |

At the Loudness 70 % point the clipper's level loss is the larger term, contradicting the audit's
"drop the clipper term". Recorded as KI-023; next MATCH item. The engine comment that stated the
floor's error direction backwards is corrected.

**Mutations (each against both suites):**

| Mutant | Killed by |
|---|---|
| the gain back after the bypass crossfade (0.2.13's order) | `matchBypass` ×2, `matchJump`, `matchWrapper` |
| MATCH removed | the existing inv-10 checks ×4, `meters/monitor`, every MATCH premise, `matchJump` |
| the bypass ramp made instant (delta ramp kept) | `monitorClick: BYPASS` (largest step 0.383 vs 0.023) |
| the monitor-gain smoother removed | `monitorClick: MATCH` (0.383 vs 0.023) |
| the delta ramp made instant | `monitorClick: DELTA` (1.156 vs 0.023) |

The click test's first version passed the ramp mutants: its toggle fell on a zero crossing of a
100 Hz sine, and with MATCH on the two bypass legs are nearly equal, so an instant switch hid. It now
toggles at a waveform peak with a shelf phase-shifting the processed leg, and has a BYPASS case with
MATCH off (legs ~6 dB apart) — the case that pins the bypass ramp. The BYPASS-with-MATCH case stays
insensitive to the ramp by construction (an instant switch between two matched legs is nearly
step-free) and is kept to pin that the new order added no step.

**Not done:** listening (no subjective claim is made); a realtime MATCH/BYPASS run in a DAW (see the
host section).
