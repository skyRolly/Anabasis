# Anabasis — Phase 0 follow-up to the 2026-09-26 product / UX audit

**Date:** 2026-09-27 · **Branch point:** `ed06ad0` (`main`; product code identical to the audited
`e769f33`) · **Version:** 0.2.13 · **Class:** dated record in `docs/reports/` — a snapshot of one
implementation round against one audit, superseded by a later record rather than edited in place
(`docs/SOURCE_OF_TRUTH.md`).

This record states, for each finding in the audit's Phase 0
([`2026-09-26-anabasis-product-ux-audit.md`](2026-09-26-anabasis-product-ux-audit.md) §Prioritized
roadmap), what the first implementation round decided and what it verified. It keeps four things
apart: **the audit's finding** (unchanged, in the audit's records), **this round's decision**, **this
round's verification**, and **issues newly found this round**. The audit report and its finding
records are not edited. The evidence — method, matrices, host runs, rejected alternatives — is in
[`worklogs/2026-09-27-phase0-product-correctness.md`](../../worklogs/2026-09-27-phase0-product-correctness.md).

**Gate status.** Two decisions are **Proposed** and wait for the owner at the Architecture Review
Gate: [ADR-0041](../architecture/design-decisions/ADR-0041-clamp-true-peak-path-inside-the-latency-allowance.md)
(DSP-001) and [ADR-0042](../architecture/design-decisions/ADR-0042-a-frozen-latch-survives-a-host-re-prepare.md)
(STATE-004). Until the owner accepts them, the code that implements them is not to be merged.

## Decisions and verification, by finding

| Finding (audit priority) | This round's decision | Verification this round | Still owed |
|---|---|---|---|
| **DSP-001** (P0) TP mode does not hold the dBTP ceiling | **Proceed — fixed.** ADR-0006 items 2–3 implemented: the clamp's own true-peak path, its delay taken out of the constant 10 ms allowance in TP mode only (ADR-0041) | Engine, 2736 TP-mode configurations, every OS cell and Force Max: `main` 1674 of 2496 over by > 0.1 dB on the product meter (worst +4.80 dB) → **0 of 2736** on the product meter and on the BS.1770 Annex 2 filter (worst +0.005 dB). TP-off bit-identical (222/222). Reported latency unchanged. Durable guard `testTruePeakModeHoldsTheCeiling` fails on `main` (102/123). **Real host:** Ardour 8.4 offline export, `main` +0.97 dB over on Annex 2 → branch at the ceiling; TP-off renders sample-identical | Owner: ADR-0041; the yardstick (sub-item (a)) — libebur128 reads up to +0.18 dB, a long-kernel reference up to +0.98 dB (KI-020); listening review of the TP-mode voicing |
| **DSP-007** (acceptance item) backstop / clip-error at OS ≥ 2× | Not in this round's scope | Not measured | The audit's criterion stands |
| **DSP-003** TP copy per oversampling factor | Partly: USER_MANUAL §3.2, §3.3 and §6 describe the mechanism (limiter detection + the final clamp at every factor) and the tolerance | The TP tooltip ("Catch inter-sample peaks — the Ceiling then holds in dBTP…") is now true as written; not changed (C8) | Finer UI copy is the maintainer's (C8) |
| **VIS-002** TP row warns at a hold printed equal to the ceiling | Not in scope; **made more visible by DSP-001** (see *Newly found*) | Measured: 74 of 2736 TP-mode renders read 0.001–0.005 dB above the ceiling on the row's estimator | ADR-0020 Amendment 2 change (owner) |
| **VIS-008**, **TEST-004**, **UX-018** | Not in this round's scope | — | As in the audit |
| **STATE-002** a factory preset turns TP, Dither, Shaping off; LOCK holds only the number | **Defer** — the fix widens ADR-0010's lockable set, which that Accepted record rejected (hard stop). Interim: disclosed | Behaviour re-confirmed in code (`applyFactoryPreset`'s defaults pass; `isPresetExcludedParam`); KNOWN_ISSUES KI-021; USER_MANUAL §3.2, §7.3, §8, FAQ | Owner: a superseding ADR for the lockable set; the dither-under-browsing rule |
| **STATE-004** (P1) a host re-prepare drops a frozen latch while FREEZE stays lit | **Proceed — fixed** (ADR-0042): the applied vector is stashed at `reset()` and restored by the first block after it when that block's snapshot has Freeze on; retained set and generation untouched. USER_MANUAL §4 names the re-prepare and the A/B case still open | Engine: re-prepared render bit-identical to the vector restored and frozen, 48 → 48 and 48 → 96 kHz, non-vacuous control; processor: published trims back after one block, generation unchanged, slot isolation kept; negative controls fail as expected. **Real host:** Carla activate cycles preserve Freeze and every parameter — audio not observable there | Owner: ADR-0042; the Freeze-OFF carry; the A/B trigger (KI-007 item 10); DAW evidence that hosts re-prepare on transport start / bounce |
| **DSP-004** Clip Drive at Oversampling Off low-passes the top end | **Preserve the DSP; disclose and pin** — the first-order ADAA kernel's own response, by design | Measured on the engine (48 kHz Off: −2.01 dB at 10 kHz, −11.74 dB at 20 kHz, drive-independent; 4× ≤ 0.6 dB); `testClipDriveDroopIsTheDisclosedOne`; KI-005 (macro name corrected), USER_MANUAL Oversampling row and §8 | Owner: a droop remedy or a non-Off oversampling default (a reported-latency change); the Oversampling tooltip copy (C8) |
| **UX-003** (+ UX-018) Save overwrites without asking | **Defer** — no platform mechanism to reuse (the Save panel is the product's own overlay); a confirm deviates from the brand checklist §A must-match convention and needs new copy (C8). Interim: disclosed | Behaviour re-confirmed in code; KNOWN_ISSUES KI-022; USER_MANUAL §7.2 | Owner: an ADR or family-wide proposal, and the wording |
| **TEST-002** real-host evidence pass | Partly: the hosts available here | Ardour 8.4 (headless session scripting + offline export), Carla 2.5.8 (scripted, Dummy engine), pluginval (strictness from `build.yml`, both modes ×3, editor under Xvfb) | REAPER / Logic / Cubase, Windows, macOS; DAW bypass mapping, transport re-prepare, automation, save prompts, keyboard |
| **TECH-001**, **STATE-018**, **MODEL-006**, **VIS-019**, **INPUT-005** | Depend on TEST-002's DAW sessions | Not collected | As in the audit |

## Newly found this round

- **The STATISTICS TP row can warn at the ceiling in TP mode.** The fix holds the row's own
  estimator to the ceiling within the 0.1 dB tolerance, not below it exactly; the row's exact compare
  then paints warn for a hold 0.001–0.005 dB over that prints equal to the ceiling (KNOWN_ISSUES
  KI-020). This is VIS-002's remedy becoming more pressing, not a new defect in the clamp.
- **The product's dBTP estimator under-reads HF-rich programme** by up to ~1.4 dB against the BS.1770
  Annex 2 filter (and over-reads dense broadband by ~0.24 dB against an accurate interpolator);
  `TruePeak.h`'s claim of ≤ 0.1 dB accuracy held only at fs/4. Corrected in the header; KI-020.
- **A host-side JUCE assertion in Carla** (`restartComponent` off the message thread when the plug-in
  reports its latency from `prepareToPlay`) — identical on `main`; not caused by this round, not
  diagnosed further.

## Phase 1 readiness

Not ready as a whole: it needs owner decisions (the monitor-stage signal-order change of UX-009 with
an ADR-0006 D8 amendment; ADR-0020's session contract for VIS-001; the click-to-reset convention for
UX-002; a Thread Model review), maintainer copy for every new label (C8), DAW evidence this round
could not collect (VIS-005, UX-010), and Phase 0's TEST-004 statics. **TEST-001** (feedback-layer
test reach) changes no product behaviour and can start first. The worklog's §Phase 1 readiness has
the reasoning.
