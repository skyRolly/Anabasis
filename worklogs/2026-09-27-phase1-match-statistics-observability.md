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

**Not done:** listening (no subjective claim is made); a realtime MATCH/BYPASS run in a DAW — the
realtime run was in Carla's rack, a plug-in host rather than a DAW (see the host section).

## STATISTICS — the reset, the bypass audition, the session length (ADR-0020 amendment 4)

**What the code did before** (audit UX-002, VIS-001, VIS-009, DOC-002 — each re-read against the
tree): `LoudnessMeterView::mouseDown` reset the session on any mouse-down anywhere on the panel
(left, right, the first press of a double-click, a drag start, the empty glass below the rows);
the render tap fed the session accumulators and the wrapper's TP/SP holds with no bypass condition,
so a realtime bypass of a hot input raised the holds above the ceiling and pulled I toward the input;
nothing showed what span the figures covered; a reset blanked every published reading until the
next processed block, which the manual's "the rolling windows are not reset" did not say.

**Decisions and why:** RESET as an uppercase `TextButton` (the family's action-label convention —
LEARN, LOCK, MATCH, BYPASS), title "Reset statistics" (the LOCK/"Ceiling lock" pattern), tooltip in
the existing panel tooltip's own words; the session pause **realtime-only**, so an offline render's
statistics describe the file it wrote; the session length counts MEASURED audio, so it is the scope;
DOC-002 fixed in the manual, not in the display clear (the round-33 pairing stays).

**Measured through the wrapper** (`testABypassAuditionStaysOutOfTheSessionFigures`, −6 dB ceiling,
997 Hz at 0.95 peak, 1.09 s audition in 5.12 s): processed programme SP −6.00 / TP −6.00 / I −6.00;
with the realtime audition SP −6.00 / TP −6.00 / I −6.00 while the momentary reading followed the
input (−0.44 LUFS against −6.00); offline, SP −0.45 and I −4.00; session length 5.120 s plain,
4.022 s with the audition (the 1.088 s audition plus its ~10 ms ramps), 5.120 s offline.

**Mutations (each against both suites):**

| Mutant | Killed by |
|---|---|
| the session always open | `sessionScope` ×3 (holds, I, length) |
| offline not exempt | `sessionScope` offline ×2, and the hardened extreme-level premise |
| SP hold / TP hold reading the render peak | `sessionScope` holds (each) |
| the meter's session half never paused | `sessionScope` I |
| the pause dropped at the integrated / LRA admission site | `sessionPause` I ×4 / LRA |
| no resume watermark / no straddler | `sessionPause` ×4 / ×2 |
| the reset leaves the length / the display clear skips it | `sessionTime` from-the-reset / ×3 zero-publishes + `statsReset` |
| the formatter rounds | `sessionTime` formatter |
| RESET does nothing / the whole panel resets again | `statsReset` ×2 / body checks in both layouts |
| **equivalent, recorded:** the view's child-click flag false | nothing — in the pinned JUCE `Component::hitTest` consults it only when the parent ignores clicks |
| **equivalent, removed:** `jmax` on the resume watermark | nothing — a resume always follows any reset, so its watermark is never earlier; the `jmax` was dropped rather than claimed |

## Numeric observability — the limiter's reduction as a number (audit VIS-007 / VIS-003 step 1)

Re-checked before building: no GR number existed in either view (`meterGrDb()` had no GUI caller);
every GR display outside the COMP lane is the limiter's, unlabelled. The readout reads the GR
history ring on the message thread in the editor tick — both views, either graph — rather than the
per-call atomics, so a 24 Hz read misses no block. "lim GR" = deepest entry over the last 0.3 s,
"GR max" = deepest over the history window; the span is capped 4096 entries short of the ring's
safe lap, because at a saturated pair a full-window scan starts one lap behind the head and any push
during it would fail the lap check for ever.

Scan cost (scratch bench, full ring, 200 reads each): 48 kHz / 512 — 1875 entries, 0.0025 ms;
48 kHz / 64 — 15000, 0.019 ms; 96 kHz / 32 — 60000, 0.074 ms; 384 kHz / 16 — 258047, 0.32 ms
(0.77 % of a core at 24 Hz).

Visual check: the editor rendered headlessly in both views (a temporary snapshot hook in the state
suite, not committed): Simple stacks "lim GR" / "GR max" under "out LUFS" in its columns, clear of
the well; Advanced puts them in the LIMITER foot under its lane; the STATISTICS header reads
"STATISTICS 0:00 … RESET" with the rows unmoved.

| Mutant | Killed by |
|---|---|
| no lap margin | the saturated-pair liveness check (and its premises) |
| "now" boundary off by one | the edge check and the brute-force property |
| the tick direction deleted | the three tick checks |
| no stall timeout | the stall rule and the stopped-host tick check |
| the formatter prints "-0.0" | the formatter check |
| the max folded over "now" only | the edge check and the brute-force property |
| the per-call COMP figure read instead | the compressor-alone and within-0.3 dB checks |
| **survivor, recorded:** the lap re-check removed | not observable by value — a single-threaded or paced producer never laps, and a lapped min-fold reads only real measurements |

## Hosts — exactly what was and was not run

Two hosts are installed in this container: **Ardour 8.4.0** (Ubuntu `1:8.4.0+ds1-2ubuntu8`) and
**Carla 2.5.8**. Both ran the Linux VST3 built from the final code (the Release build the suites and
pluginval ran on; no source file is newer than its binary). Ardour was rescanned first
(`ardour-vst3-scanner -f ~/.vst3/Anabasis.vst3`, the rule recorded above).

**Carla 2.5.8, realtime — MATCH/BYPASS (UX-009 in a host).** Carla's engine on its Dummy driver runs
the audio thread in real time (10 ms cycle, 48 kHz, 512-sample buffers, continuous rack, 0 xruns):
Carla's internal audio-file player looping a 10 s stereo 48 kHz file with a 0.5 sample peak →
Anabasis (VST3), Loudness at 0.7, every other parameter at its default. MATCH (`Loudness Comp`) and
`Bypass` were set through the host's parameter API, 4 s allowed to settle, then Carla's per-cycle
output peak of the plug-in was sampled every 5 ms for 12 s — longer than the file's loop, so every
state sees the file's peak. The same script ran against a 0.2.13 build kept from the review round
(the monitor stage in 0.2.13's order):

| State | Anabasis output peak, this round | 0.2.13 build |
|---|---|---|
| processed | 0.9886 | 0.9886 |
| processed + MATCH | 0.2970 | 0.2970 |
| BYPASS | 0.5000 | 0.5000 |
| **BYPASS + MATCH** | **0.5000** — the input's own peak | **0.1428** (−10.9 dB) |

The host path confirms the engine result: BYPASS with MATCH on now plays the input at unity, and the
processed and matched-processed outputs are unchanged. A first run with a 6 s window (shorter than the
loop) read BYPASS on the old build as 0.4590 — a window that missed the file's peak, not a level
change; the 12 s runs are the record. Carla's own debug output printed a JUCE assertion
(`juce_VST3PluginFormat.cpp:3636`) at plug-in load — Carla's VST3 hosting code, present in every Carla
run of this PR including the ones on the `main` build, so not this round's.

**Ardour 8.4.0, offline export — the render is unchanged.** The review round's TP-off session and
its TP-switched-on-mid-export session (`off_fix`, `t1_fix`; the plug-in on the track, the toggle at
sample 24576 through the plug-in parameter API) re-exported with `ardour8-export -b float`: the audio
data of both files is **byte-identical** to the review round's renders (2 304 000 bytes each; the
files differ only in the header). Neither session engages MATCH or BYPASS — offline MATCH is inert
by design and ADR-0044 moves nothing rendered, which is what this confirms from a host.

**Not run, and not claimed:**
- a host's **own** bypass (Ardour's plug-in deactivate, a REAPER/Logic/Cubase bypass button) —
  whether a host bypasses through the plug-in's `Bypass` parameter (the STATISTICS pause applies) or
  stops calling it (nothing is measured either way) is the host-dependent half of VIS-001, the
  audit's TEST-002; the manual states both cases without naming hosts;
- the STATISTICS pause, the RESET button and the GR readout in a host's editor window — covered by
  the processor and editor tests (state suite) and by pluginval's editor tests under Xvfb, not
  inspected on screen in a host;
- a realtime MATCH/BYPASS run in a DAW (Carla is a plug-in host); a realtime print automating BYPASS
  with MATCH on;
- REAPER, Logic, Cubase, Pro Tools, Live, Bitwig, Studio One; any Windows or macOS host (CI builds and
  validates those platforms; no host ran there);
- listening, for any change in this round.

## Gates — local, and what GitHub has not yet run

Run on the final code (commit 7's tree; commit 8 is records only). **Local results, not a CI claim:**
GitHub's run on the pushed head is the evidence of record, and PR #42 is not called clean from these.

| Gate | Result |
|---|---|
| GCC Release + LTO: VST3, Standalone, both suites | built; DSP 565 checks, state 1573 checks, 0 failures |
| clang-22 full build + the first-party warning gate (`check-clang-warnings.py`, self-test 18 cases) | no first-party warnings; 2 in vendored paths, not gated (pre-existing) |
| clang-22 suites | 565 / 1573, 0 failures |
| RTSan canary / effects tier | canary exits 43 with its report; effects clean compile passes, effects canary fails as required |
| RTSan DSP suite (RelWithDebInfo, `-fsanitize=realtime`, `ANABASIS_RTSAN_LANE=1`) | 560 checks, 0 failures, 0 sanitizer reports — 5 fewer than the other lanes because the allocation guard compiles out under RTSan and `testTheAudioPathAllocatesNothing` discloses and skips its assertions (the documented state, as in the review round) |
| `check-docs` / `check-realtime` / `check-portability` | 133 files clean / 42 files, 0 violations, 1 of 1 ordering verified / 50 files, 0 violations |
| `check-citations` against `origin/main`, the merge base and the push predecessor | clean after two re-anchors in commit 7 (THREAD_MODEL `PluginEditor.h:641` → `:650`, ADR-0027 `:363` → `:364`; the first preflight run failed on them against the push predecessor) |
| `scripts/preflight.sh` | passed |
| pluginval, strictness from `build.yml`, editor under Xvfb | deterministic 3 / 3, randomise 3 / 3 — every pass on its first attempt |

**The review round's pluginval anomaly, re-checked.** The review round recorded one randomise pass
(seed `0xcb2cae`) that printed `SUCCESS` and segfaulted at validator exit, retried and passed, and was
seen once on the PR-head build too (`worklogs/2026-09-27-pr42-review-closure.md`). This round: that
seed replayed 3 times directly against the final VST3 (strictness 10, `--randomise`, Xvfb) — exit 0
each time; plus 6 more unseeded randomise passes (seeds `0x4f7b604`, `0x1ec1c5`, `0x4fa84dc`,
`0x766cac5`, `0x7edfafe`, `0x7e91e90`) — all first-attempt passes. With the gate's 6 passes, 15 runs
and no crash. **Not reproduced, not diagnosed** — it stays a recorded intermittent at validator exit
that predates this PR, not a fixed defect.

**GitHub, as of the review head `2071294` (before this round's commits):** every executed check
passed; 9 PREfast C6262 (stack size) threads stay open on `tests/dsp_tests.cpp` — test-only, assessed
in the review round, not addressed here — and the C6011 thread is resolved. The Windows/macOS lanes,
the MSVC analysis and the realtime job have **not** run on this round's commits at the time of
writing.

## Decisions this round (§15 of the instruction), each with its reason

| Item | Decision | Why |
|---|---|---|
| UX-009 (MATCH ordering) | **Implemented** — ADR-0044 | the signal flow was proved first (the old order measured −6.76 LU with MATCH on and off alike); the fix moves one multiply and nothing rendered or metered |
| VIS-001 (bypass folded into the session figures) | **Implemented, realtime-only** — ADR-0020 amendment 4 | a realtime bypass is an audition of the input; an offline bypass is part of the file, so an offline session must describe the file it wrote |
| UX-002 (the whole panel resets) | **Implemented** — RESET button, inert body | the smallest change that removes the stray-click loss; what a reset clears is unchanged |
| C8 wording | **Taken from the repository's own conventions, recorded** — no "maintainer wording" invented where a convention exists | RESET (the uppercase action-label family: LEARN, LOCK, MATCH, BYPASS), "Reset statistics" (title after the header, as LOCK / "Ceiling lock"), the existing panel tooltip's words; "lim GR" / "GR max" ("lim" as in LIMITER, "GR" as in the graph pill, the "out LUFS" grammar); "-" as the product's no-reading form; tooltips in the `tidyTip` voice. All product copy stays ⊕ for the fine review, like every string taken under the standing approval |
| VIS-009 (scope invisible) | **Implemented** — the session length | it is the scope in one number, and it is also the liveness heartbeat VIS-005/VIS-012 need later |
| DOC-002 | **Documented, code kept** | the round-33 display clear is deliberate (a reset must be visible with the transport stopped); the manual was the wrong half |
| VIS-010, UX-010's cue | **Deferred, recorded** | an indicator needs a place on a signed surface, and a gain readout needs a new Audio → GUI scalar (a Thread Model confirmation) — the owner's call, not a green build's |
| DSP-005 | **Measured, recorded (KI-023), next** | not the ordering fix; the measurement contradicts the audit's "drop the clipper term", so it needs its own prototype |
| DAW evidence | **Stated exactly** — see the host section | no DAW is claimed that was not run |
