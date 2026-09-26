# Anabasis product / UX audit — findings: DSP behaviour as the user meets it and Technical robustness

Part of [`2026-09-26-anabasis-product-ux-audit.md`](../2026-09-26-anabasis-product-ux-audit.md) (audited revision `e769f33`, 2026-09-26). This file holds the complete record of each finding in these categories; the report carries the index, the systemic themes, the roadmap and the decision record. Code anchors are pinned to `e769f33`; runtime observation ids refer to [`worklogs/2026-09-26-product-ux-audit.md`](../../../worklogs/2026-09-26-product-ux-audit.md).

Each record: decision, priority and confidence after calibration; evidence; current behaviour; problem; root cause; user impact and scope; proposed improvement; alternatives considered; decision rationale (with any calibration or challenge outcome); architecture gates; dependencies; acceptance criteria; and the verification record. Terms in the records: the *candidate claim* is the claim as it entered verification; *the judge* is the verifier's decision pass (Phase 3, step 3), done per *batch* of 3–6 related findings; *Adversarial challenge* is the step-4 review and *Calibration* the Phase-4 pass that set the final decision and priority (see the report's *Evidence and method*). A paragraph marked *Merged at triage from another verifier's note* is evidence from another batch's verifier, kept in its words: 'add to X' there means it has been added to this record. 'Recorded at triage' marks a finding written from such a note. `rt/…` paths and ids such as `VER0-2` or `V24-TSAN-1` name uncommitted session captures, logs and probes; `PF-…` ids are potential findings from the uncommitted Phase-1 evidence maps.

## DSP — DSP behaviour as the user meets it

### DSP-001

**TP mode does not hold the dBTP ceiling: the clamp is sample-peak only (ADR-0006 D2/D3 unimplemented while ADR_INDEX reads Verified), so true peaks exceed a '-0.10 dBTP' ceiling by 0.03 dB at OS-Off defaults, about 0.2-0.9 dB with Punchy or high Transients, and 1.1-1.7 dB at every oversampling factor at Loudness 50 %, including the Force Max bounce**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P0** | high | confirmed | Technical robustness | The dBTP delivery ceiling is claimed but not enforced, and its warning cannot tell an over from normal operation | Phase 0 |

**Evidence**

- e769f33:src/dsp/CeilingClamp.h:10-16 — 'P1: sample-level hard clamp at the linear ceiling. The true-peak-driven gain half of ADR-0006 item 3 arrives with the TruePeak tap at P2'. Lines :29-34 hold the whole mechanism, a sample compare.
- e769f33:src/dsp/AnabasisEngine.cpp:1188 — clamp.processSample(processed, ceilingNow): no TP input. e769f33:src/dsp/AnabasisEngine.h:579 — the only output TruePeakEstimator is the meter's outTp.
- e769f33:src/dsp/AnabasisEngine.cpp:656 — limiter.setTruePeakMode(p.truePeakMode && osN < 4). :1077-1078 — the region is decimated after the limiter, before the clamp.
- e769f33:src/dsp/LookaheadLimiter.h:36-40, :253-254 — transientPreserve > 0 slews the attack, 'deliberately letting the front of a hit poke through to the downstream clamp'.
- e769f33:docs/architecture/design-decisions/ADR-0006-ceiling-guarantee.md:126-135 (D2/D3: clamp TP tap, TP-driven gain, ≤0.1 dBTP). ADR-0002:109-113 (D4, same claim). ADR-0006:9 banner says 'still with its own true-peak tap'.
- e769f33:docs/architecture/design-decisions/ADR_INDEX.md:20 — ADR-0006 evidence column 'Verified — testOutputNeverExceedsCeiling…'. e769f33:docs/policies/DSP_POLICY.md:88-91 (invariant 4, ≤0.1 dBTP in TP mode) and :304 ('partial (P2) … still needs the true-peak tap').
- e769f33:tests/dsp_tests.cpp:458 and :2334 — the ceiling tests pin truePeakMode=false and check sample peaks only. :1845 — testLimiterTruePeakMode pins transientPreserve=0 and only asserts a reduction.
- e769f33:src/PresetManager.cpp:157-160 (Loud Pop: Punchy), :180-183 (Rock Punch: Punchy + Transients 75), :185-188 (Hip-Hop Low End: Punchy).
- e769f33:docs/user/USER_MANUAL.md:171 and :206-207 (TP: 'inter-sample peaks are caught', dBTP limit); :282 (Oversampling 'for … true-peak accuracy'); :500-501 (Force Max 'maximum quality on the bounce').
- V-12: session capture `rt/visuals/28a-tp-on-20s-editor.png`, session capture `rt/visuals/27a-tp-on-after-reset-stats.png`. E14: session capture `rt/edges/49g-tpmode-12s-after-reset-crop.png`, session capture `rt/edges/49h-tpmode-repeat-crop.png` — -0.06/-0.07 dBTP red.
- verify-19 R6 (harness, OS Off, TP on, music -6, Loudness 50):
  - session capture `rt/verify-19/10-tpon-trans100-punchy-lvl6-20s-editor.png` — Ceiling '-0.10 dBTP', TP 0.57 dBTP.
  - .../rt/verify-19/13-14-pair.png — Transients 100: +0.44; Transients 75: +0.28.
  - .../rt/verify-19/12-tpon-defaults-lvl6-loud50-20s-stats.png — defaults: -0.07.
- verify-19 R7 (harness, OS 4x): .../rt/verify-19/16-os4x-tpon-defaults-lvl6-20s-editor.png — TP toggle on, Ceiling '-0.10 dBTP', TP 1.27 dBTP. .../rt/verify-19/15-16-pair.png — TP off 1.39. .../rt/verify-19/17-18-pair.png — Transients 0: 1.38; music -18: 0.21.
- verify-19 probe results in .../rt/verify-19/probe/matrix-all.txt, forcemax.txt and punchy.txt (source: probe/main.cpp; instrumentation diff = 3 read-only lines before AnabasisEngine.cpp:1188):
  - Loudness-50% values, TP on: OS Off -0.074 (27 samples clamped); 2x +1.044; 4x +1.579 (pre-clamp +2.05 dBFS, 1305 clamped); 16x +1.399.
  - Force Max +1.399, versus realtime OS Off -0.074.
  - Milder drive: realtime -0.083 vs Force Max +0.155.
- *Merged at triage from another verifier's note, quoted as written:* This resolves the yardstick caveat the [DSP-001](findings-dsp-tech.md#dsp-001) challenge left open (caveat 3). The product's TruePeakEstimator feeds both the limiter's TP detector and the Statistics TP row. On lightly limited, HF-heavy programme it reads 0.6-0.75 dB below a validated 32x/128-tap reference. Re-checked in the probe outputs:
  - matrix-all.txt: OS Off, TP on, Transients 0, limGain 0: estimator -0.100 vs reference +0.643 dBTP.
  - forcemax.txt (realtime row): limGain 3: -0.083 vs +0.659.
  - punchy.txt: +0.096 vs +0.817.

  At the Loudness-50 operating point (limGain 7.8) the gap shrinks to about 0.07 dB (-0.074 vs -0.008).

  On sines (val-output.txt), the reference is accurate to ≤0.003 dB and the estimator under-reads by 0.219-0.301 dB at 16 kHz. That is the 4x grid limit. TruePeak.h:29-33 states the bound as '~0.15 dB at fs/4', which is true at 12 kHz but understates content above fs/4: any max-reading 4x estimator can be up to about 0.69 dB low near Nyquist.

  Caveats: the synthetic 'snap' is white-noise and Nyquist-heavy, and BS.1770-4 permits 4x, so BS.1770 4x QC meters may agree with the plugin.

  Add to [DSP-001](findings-dsp-tech.md#dsp-001):

  (a) The owner states in DSP_POLICY invariant 4 which yardstick the ≤0.1 dBTP promise is defined on. The acceptance test currently says 'BS.1770 4x estimator'.

  (b) The hostile-input test also reports a high-accuracy reference.

  (c) The new clamp TP tap either uses a more accurate estimator or carries a stated margin.

  (d) Correct the TruePeak.h bound comment.
- *Merged at triage from another verifier's note, quoted as written:* Add the risk-register side, which [DSP-001](findings-dsp-tech.md#dsp-001) lacks. The trigger of FUTURE_RISKS RISK-003 ('inter-sample peaks after saturation … an oversampling-factor switch', e769f33:docs/FUTURE_RISKS.md:54-66, re-checked) has now occurred, and KNOWN_ISSUES has no true-peak entry. [DSP-001](findings-dsp-tech.md#dsp-001)'s interim path should add a KNOWN_ISSUES entry now and mark RISK-003 as triggered, whenever D2/D3 land. The test-gap anchors (dsp_tests.cpp:458, :1845, :2334) are already in [DSP-001](findings-dsp-tech.md#dsp-001).

**Current behaviour.** With TP on, the Ceiling reads '-0.10 dBTP'. Only the limiter's detector is TP-aware, and only below 4x. The final clamp hard-clips samples.

Output true peak (meter estimator):
- OS Off: within tolerance at Transparent with Transients ≤50 %; about +0.2…+0.9 dB over (+0.10 to +0.82 dBTP measured) with Punchy or Transients ≥75 %.
- 2x-16x: +1.1…+1.7 dB over at a mid Loudness setting.
- Force Max offline render: runs at 16x, so the bounce is up to 1.5 dB over while the realtime monitor (OS Off) showed compliance.

**Problem.** The product's core promise, 'nothing leaves the plugin above' the Ceiling, has its TP-mode form (DSP_POLICY invariant 4, ≤0.1 dBTP) violated by up to 17× the tolerance. This happens in documented configurations: TP on with the manual's recommended 4x/Force Max, and three factory presets.

The mechanism ADR-0006 and ADR-0002 specify for this (a clamp-level TP estimate driving gain) does not exist. The records say it does (ADR-0006:9 banner, ADR_INDEX 'Verified', ADR-0015 D7 'the DSP was right about its own guarantee').

**Root cause.** ADR-0006 D2/D3 and ADR-0002 D4 were never implemented. CeilingClamp remains the P1 sample-level backstop, and the engine passes it no TP estimate.

The limiter cannot stand in for it:
- At ≥4x the TP switch does not reach it: it reads the oversampled region signal directly (AnabasisEngine.cpp:653-656), which is sound for its own detector (ADR-0003 item 6) but cannot see what the down-filter adds afterwards.
- It limits the pre-decimation region signal, which then overshoots through the down-filter.
- Its attack is deliberately slewed by transientPreserve and Punchy.

No test asserts TP-mode output true peak ≤ ceiling + 0.1 dB at any OS factor. The ceiling tests pin TP off, which is why the suite stays green.

**User impact.** A user who engages TP to meet a dBTP delivery spec (e.g. -1 dBTP) and follows the manual's advice to use 4x or Force Max delivers a master whose true peak is about 1.5 dB above the spec. The Force Max bounce differs from what they monitored, and the error shows only on an external meter or in QC rejection or codec clipping.

Factory presets Loud Pop, Hip-Hop Low End and Rock Punch exceed the tolerance in TP mode even at OS Off. The only workaround is lowering the ceiling by an unknown margin, which costs loudness. *Scope:* Every TP-mode render at OS 2x/4x/8x/16x, any Force Max offline render, and OS Off with Punchy style or Transients above ~50 %. Measured on synthetic music; magnitudes will vary with material. The mechanism (decimation regrowth and a sample-only clip) is general.

**Proposed improvement.** Implement ADR-0006 D2/D3 as recorded:
- The clamp gets its own TruePeakEstimator tap on its input (after the Post-EQ, base rate).
- A short base-rate lookahead: at least the estimator's 6-sample reporting lag plus a short attack.
- A smooth gain that holds the TP estimate ≤ ceiling while TP mode is on, at every OS factor and style.
- The sample hard clip stays as the backstop.

Add the missing guard: a hostile-input test asserting TP-mode output true peak ≤ ceiling + 0.1 dB across OS Off…16x × min/linear phase × Force Max × styles × transientPreserve {0, 0.5, 1} × both EQ positions.

Correct the records through the ADR process:
- ADR_INDEX evidence for ADR-0006: Partially Verified until D2/D3 land.
- ADR-0006 banner (:9) and 'Related code' (:222).
- ADR-0015 D7 (:174-179).

Interim, if implementation waits: the manual and tooltip state that TP mode holds dBTP only at OS Off with Transparent/Loud and Transients ≤50 %. Remove 'true-peak accuracy' from the Oversampling row (USER_MANUAL:282).

**Alternatives considered.**

- *Limiter margin (threshold slightly below the ceiling in TP mode or when OS is on).* — Conflicts with ADR-0006 D5 (one ceiling, no independent limiter threshold). It cannot cover Post-EQ boosts, and the margin needed (~1.7 dB at 4x) would audibly cost loudness.
- *Move the clamp inside the oversampled region.* — Rejected by ADR-0006 option D and ADR-0002 D3 (it must follow the base-rate Post-EQ). It is a signal-order change.
- *Cap or ignore transientPreserve and Punchy in TP mode.* — Reduces the OS-Off overshoot but does nothing for the 1-1.7 dB decimation overshoot at OS ≥2x. It also changes limiter voicing.
- *Amend DSP_POLICY and ADR-0006 to weaken the TP-mode tolerance, or restrict TP mode to OS Off.* — Weakens the core promise, which is itself a Ceiling-guarantee gate item. Restricting TP to OS Off contradicts ADR-0003 item 6 and the Force Max feature.
- *Documentation-only disclosure.* — Acceptable only as an interim measure. The guarantee is the product's contract.

**Decision: Proceed · P0.** This is a verified loss of audio correctness against the product's core, documented guarantee. The evidence is two independent runtime methods agreeing (the GUI harness meter, and the real engine instrumented at the clamp) and it is code-explained. The remedy is already the Accepted design (ADR-0006 D2/D3), so the change realigns code with its ADRs rather than inventing architecture.

It still touches the ceiling mechanism and probably the reported latency, so it must go through human review.

*Adversarial challenge:* evidence holds: yes; priority justified: yes (suggested P0); decision justified: yes (suggested Proceed). The finding is a reproduced, code-explained violation of the product's core guarantee in TP mode: invariant 4, TESTING_POLICY:143 level-3 gate. TP mode fails it by 10-17× the tolerance at every OS factor and on the Force Max bounce, which the manual recommends (USER_MANUAL:282, :442-444, :500-501). The project's own TROUBLESHOOTING.md:35 classifies 'Output exceeds the ceiling' as 'a release blocker, not a tuning issue'.

The configurations are opt-in: TP, OS and Force Max all default off (InternalState.h setDefaults). That lowers frequency but not severity. Under the rubric, loss of audio correctness against a stated delivery guarantee is P0.

The remedy is the Accepted design (ADR-0006 D2/D3, ADR-0002 D4), so Proceed through the Architecture Review Gate is right. The judge missed the ADR-0003 latency-neutrality conflict and the TP-off consistency issue with [DSP-007](findings-dsp-tech.md#dsp-007). *Proposal risks:* 1. Missed ADR-0003 conflict: truePeakMode must stay latency-neutral. ADR-0003 option D (:79-86) and Consequences (:239-241) make truePeakMode 'genuinely not latency-affecting' (footnote ⁴), and that premise is why it is frozen non-automatable. A clamp-stage TP lookahead engaged only in TP mode would make TP latency-affecting. So the lookahead must either be:
   - mode-independent: raising reported latency for every user, including TP-off defaults (ADR-0004 constant-allowance contract, Latency gate); or
   - carved from the existing max-lookahead allowance: not available at the 10 ms maximum.
   ADR-0003's false premise that the 'backstop enforces ≤ 0.1 dBTP' needs amending too.
2. Audible voicing change in TP mode. A TP-driven gain at the clamp will absorb the Transients/Punchy poke-through the limiter deliberately lets pass (LookaheadLimiter.h:36-40). TP-on sessions and presets will therefore sound different, and ADR-0006 Consequences ('limiter voicing … none of them is load-bearing for invariant 4') becomes false in TP mode. Review this under SESSION_COMPATIBILITY if any build has left the repository.
3. At OS ≥ 2x the gain must remove 2-3.6 dB of decimation regrowth. That is effectively a second fast limiter after the main one, and the ADR-0006:192-195 premise ('normally keeps it from engaging') will not hold at those factors without a matching limiter-side change. Listening review is needed.
4. Contradiction with [DSP-007](findings-dsp-tech.md#dsp-007). The acceptance criterion 'TP off bit-identical at OS Off' contradicts [DSP-007](findings-dsp-tech.md#dsp-007)'s proposed sample-peak-driven gain with TP off (also ADR-0006 D3 'with it off, on the sample peak'), which would change TP-off output wherever the clamp engages today (343 samples at OS Off in m4). The two must be reconciled in one design.
5. The new ≤ 0.1 dBTP test must name its yardstick (BS.1770 4x estimator vs a finer reference), because the two diverge by up to ~0.9 dB on broadband material.

**Architecture gates.**

- Ceiling guarantee change: it modifies the invariant-4 stage (CeilingClamp). The intent is to strengthen it, but ARCHITECTURE_REVIEW_GATE review is still required.
- Latency change (probable): a clamp-stage TP gain needs its own base-rate lookahead of at least the estimator's 6-sample lag, so the processed and dry paths and the reported figure move. ADR-0004 contract, LATENCY_MODEL.
- DSP Graph change: adds the clamp's TP tap and gain node specified by ADR-0006 D2/D3 and ADR-0002 D4, which are missing from the current graph.
- The current code does not implement Accepted ADR-0006 D2/D3 and ADR-0002 D4. ADR_INDEX evidence status and ADR-0006/ADR-0015 D7 statements need correcting under ADR_POLICY.

**Dependencies.** None.

**Acceptance criteria.**

- A dsp_tests case with TP mode on, ceiling -1.0 and -0.1 dBTP, and a transient-rich hostile stimulus asserts output true peak (BS.1770 4x estimator) ≤ ceiling + 0.1 dB for every OS factor (Off/2x/4x/8x/16x), both phase modes, Force Max offline, all three styles, transientPreserve 0/0.5/1, and both EQ positions.
- With TP on at 4x, the harness Statistics TP row after a reset reads ≤ ceiling + 0.1 dB on the audit music at Loudness 50, where it currently reads 1.27-1.39 dBTP.
- Output sample peak still never exceeds the ceiling (the existing testOutputNeverExceedsCeiling and testCeilingUnderOs stay green).
- With TP off, output stays bit-identical to today at OS Off defaults (invariant-7 null and the existing tests unchanged), unless the owner approves otherwise.
- ADR_INDEX, the ADR-0006 banner and Related code, ADR-0015 D7 and DSP_POLICY:304 describe the implemented mechanism and its test.

<details><summary>Verification record</summary>

**Method.** Code read at e769f33:
- CeilingClamp.h:10-16 and :29-34.
- AnabasisEngine.cpp:653-656, :1077-1078, :1188; AnabasisEngine.h:579 (the only engine TP tap is the meter's outTp).
- LookaheadLimiter.h:36-40, :253-254; TruePeak.h:29-33.
- ADR-0006:126-135, :192-195; ADR-0002:109-113; ADR_INDEX:20; DSP_POLICY:88-91, :304.
- e769f33:tests/dsp_tests.cpp:458, :1845, :2334.

Harness on :149 (stepped clicks; OS 4x set by loading a saved state with int_oversample=2, latency 480→486):
- OS Off TP on: Transients 0 -0.10, Transients 50 -0.09/-0.07, Transients 75 +0.28, Transients 100 +0.44, Transients 100 Punchy +0.57 dBTP.
- OS 4x: TP off 1.39, TP on 1.27, TP on + Transients 0 1.38.

Scratch probe: the real AnabasisEngine.cpp at e769f33, copied, with a read-only counter at the clamp input, and the harness's music generator. 20 s after a 2 s warm-up; TP measured with the product's own TruePeakEstimator.

The same probe also uses a 32x/128-tap Kaiser-sinc reference, validated to ≤0.003 dB on sines (probe/val-output.txt).

**Corrections to the candidate claim.** The observer could not tell limiter overshoot from estimator disagreement. The probe shows real output overshoot, from two mechanisms:
1. OS Off: the limiter's slewed attack (transientPreserve, Punchy ×1.5) lets fronts through. Pre-clamp max rises -0.100 → -0.067 → +0.34 → +1.07 dBFS for Transients 0/50/75/100, and the TP reading tracks it.
2. OS ≥2x: the decimation filter's overshoot after the limiter (pre-clamp up to +2.0 dBFS at 4x and +3.6 at 16x) is sample-clipped by the base-rate clamp, leaving inter-sample peaks.

At OS-Off defaults (Transients 50, Transparent) the overshoot is about 0.03 dB, inside the 0.1 dBTP tolerance. The title's case alone is a display problem ([VIS-002](findings-visualisation.md#vis-002)). The real defect is beyond defaults:
- Punchy with default Transients (used by the Loud Pop and Hip-Hop Low End factory presets): +0.10…+0.24 dBTP.
- Punchy + Transients 75 (Rock Punch): +0.47.
- Any OS factor: 2x +1.04, 4x +1.58, 16x +1.40.
- Force Max offline bounce: +1.40 vs -0.07 on the OS-Off realtime monitor at the same settings.

ADR_INDEX:20 has Status 'Accepted'; 'Verified' is its evidence-confidence column. The tests it cites pin truePeakMode=false or transientPreserve=0 and never assert the TP-mode tolerance.

</details>

<sub>Verifier scores (1-5): impact 5 · frequency 3 · severity 5 · discoverability 3 · efficiency 4 · coherence 5 · change risk 4 · complexity 4 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DSP-002

**The transition duck dips to silence, not to dry, on every A/B, preset load, undo and session load, including no-op ones (identical slots, re-selecting the loaded preset, inaudible rewires, an ADV-only undo), and the fades enter integrated LUFS**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | medium | partially-confirmed | Technical robustness | Listening aids and default voicing do not do what the product says | Phase 4 |

**Evidence**

- e769f33:src/PluginProcessor.cpp:618, :645, :1576, :1614, :1699, :1839 — unconditional engine.requestForcedDuck()
- e769f33:src/PluginProcessor.cpp:527-531 — no-op preset retraction 'does not reverse the apply' (duck stands)
- e769f33:src/dsp/AnabasisEngine.cpp:124-125 — 6 ms out / 28 ms in increments
- e769f33:src/dsp/AnabasisEngine.cpp:486-548 — bottom → in only at a host-block top (holdForRequest), so the bottom lasts to the next process() call
- e769f33:src/dsp/AnabasisEngine.cpp:1190-1194 — duck on processed path only (dips to silence, no dry blend)
- e769f33:src/dsp/AnabasisEngine.cpp:1245-1259 — render/meter tap deliberately includes the fade
- e769f33:tests/state_tests.cpp:1165-1203 — testAbSwitchRequestsDuck: A/B between identical default slots must dip < 0.02
- e769f33:docs/KNOWN_ISSUES.md:690-739 — KI-010 no dry-fill; owner decision pending for the fine review
- e769f33:docs/architecture/design-decisions/ADR-0018-copy-and-advanced-join-the-undo-history.md:93-95 — ADV-only undo ducks 'for uniformity … inaudible-by-design'
- e769f33:docs/architecture/design-decisions/ADR-0014-frozen-trim-restore.md:81-99 — every stager must request the duck; the duck is also DERIVED from the staged record at the consume
- e769f33:docs/policies/DSP_POLICY.md:144-151 — invariant 8, the three bulk-swap routes each owed a test
- VER8-10 (engine probe, request with no parameter change): rt/verify-8/duck/probe.out (source rt/verify-8/duck/main.cpp)

**Current behaviour.** Every A/B switch, preset apply, undo, redo and mid-playback session load requests a raised-cosine duck to exact silence on the processed path: 6 ms out, a hold at the bottom until the next host-block top, then 28 ms in. This happens whether or not anything changes. It is ≈38 ms total at 48 kHz/512, with ~5 ms of true silence. The same happens for direct colour-model or EQ-position edits that are inaudible at Depth 0 or with a flat EQ. The fade is included in the meters and the render tap.

**Problem.** The core compare gesture always inserts an audible dropout, including between two identical slots (for example right after Copy) and on an undo whose only change is the view. The documented duration understates it at common host buffer sizes. Two Accepted-ADR statements contradict what ships: the dry-fill 'best masking mode' (ADR-0004) and the undo duck being 'inaudible-by-design' (ADR-0018).

**Root cause.** Requests are issued before it is known whether the swap changes anything: switchToSlot requests first, preset applies request inside the bracket, undo and redo request unconditionally, and only the preset bracket detects a no-op, afterwards. The engine has no 'nothing changed' input. ADR-0014's frozen-trim landing needs a bottom, but it already derives its own duck from the staged record, so the explicit request is not what guarantees it. Dry-fill was never ported (KI-010).

**User impact.** Every A/B and undo press produces an audible dip of 38-70 ms depending on the host buffer, even when the two states are identical. It can read as a glitch, and it interrupts the programme exactly at the moment of comparison. The effect on the integrated reading is negligible (≈0.02 LU for 30 switches in 3 minutes). *Scope:* All bulk swaps in realtime playback. It is inactive in the first block after prepare and on offline entry (AnabasisEngine.cpp:444-484), and inaudible while bypassed.

**Proposed improvement.** Constrained change, in three parts.
1. Skip the explicit forced duck only where the processor can prove before applying that nothing restorable changes:
   • switchToSlot, when the stored slot and the live slot are equivalent under strippedForUndoCompare — the same test the Copy guard uses at e769f33:src/PluginProcessor.cpp:397-424;
   • undo/redo, when the entry differs from live only in advancedMode (the ADR-0018 ADV-only case).
   A staged frozen-trim record still forces the duck through ADR-0014's record-derived path, so the landing site is unaffected. Real changes keep today's duck.
2. Leave dry-fill to the KI-010 owner decision and its listening pass. Note for that pass: at non-zero Loudness the delay-aligned dry signal is typically much quieter than the output, so dry-fill replaces a dip to silence with a dip to dry level. It is not self-evidently better for a maximizer.
3. Doc sync:
   • state the duration as roughly one host buffer plus 28 ms (at least 34 ms) in the manual FAQ, ADR-0014 and the code comments;
   • correct ADR-0018:95's 'inaudible-by-design'.

**Alternatives considered.**

- *Leave as is* — Policy-compliant (invariant 8 only requires click-free) and documented. It keeps an avoidable dropout on identical-state compares, and two ADR statements stay wrong.
- *Implement dry-fill now (KI-010 option a)* — Deferred. It is an audible change to every bulk swap, needs a listening pass the owner has reserved for the fine review, and is of doubtful benefit for a loud maximizer.
- *Crossfade old-processed into new-processed* — Rejected for now: it needs two engine instances in parallel (CPU, and adaptive-state duplication).
- *Shorten the legs or exit the bottom mid-block* — Rejected. The rewires must execute at a block boundary (ADR-0004: OS latency never changes mid-block), and shorter legs risk the clicks invariant 8 forbids.

**Decision: Modify · P2.** The duck on real changes is mandated by DSP_POLICY invariant 8 and ADR-0014 and is documented to users, so the finding's broad framing ('every … dips to silence') is not a defect to remove wholesale. The no-op subset (identical-slot A/B, ADV-only undo) can be removed safely with an exact pre-check that already exists in the codebase. Dry-fill is an open owner decision (KI-010) and should stay there. P2: audible on a frequent gesture but documented, click-free, and with a negligible metering effect.

**Architecture gates.**

- Conflict with Accepted ADR-0018 §Consequences (e769f33:docs/architecture/design-decisions/ADR-0018-copy-and-advanced-join-the-undo-history.md:93-95: an ADV-only undo ducks 'for uniformity') — skipping it requires an ADR-0018 amendment (Accepted-ADR gate)
- ADR-0014 (e769f33:docs/architecture/design-decisions/ADR-0014-frozen-trim-restore.md:81-99): 'every stager must request the duck' — a skip is valid only when no frozen-trim record is staged; the record-derived duck must remain intact (amend the ADR's wording if the explicit request is dropped for no-op swaps)
- DSP_POLICY invariant 8 (e769f33:docs/policies/DSP_POLICY.md:144-151): testAbSwitchRequestsDuck's fixture switches identical slots and must be re-fixtured to differing slots, plus a new test pinning the no-op skip; the invariant itself (click-free) is unchanged
- Dry-fill (not proposed here) = ADR-0004 §Consequences / KI-010 owner decision — an audible change to every bulk swap
- No signal-order, reported-latency, threading or serialization change in the proposed skip

**Dependencies.** KI-010 (dry-fill owner decision; e769f33:docs/KNOWN_ISSUES.md:690-739); [UX-011](findings-ux.md#ux-011) (the A=B equality query is the same comparison the no-op switch skip needs); [STATE-008](findings-state-model.md#state-008) / [VIS-009](findings-visualisation.md#vis-009) (fade legs in the integrated figure: measured as immaterial)

**Acceptance criteria.**

- Offline processor test at 48 kHz/512: Copy, then A/B → the output over the following 150 ms is bit-identical to a run without the switch
- A/B between slots that differ in any sound parameter still dips below 0.02 envelope and recovers (re-fixtured testAbSwitchRequestsDuck passes)
- An undo whose entry differs from live only in advancedMode produces no dip (after the ADR-0018 amendment); any other undo still dips (testUndoRequestsDuck passes)
- Switching into a Freeze-ON slot with a staged vector still ducks and injects at the bottom (testFrozenTrimRestore passes unchanged)
- Manual/ADR/code-comment duration statements read as buffer-dependent (≥34 ms, ≈38 ms at 48 kHz/512); ADR-0018:95 no longer claims the duck is inaudible
- KI-010 remains open, with the probe numbers and the dry-level caveat attached for the listening pass

<details><summary>Verification record</summary>

**Method.** Code:
• Unconditional requestForcedDuck at e769f33:src/PluginProcessor.cpp:618 (undo), 645 (redo), 1576 (switchToSlot), 1614 (factory apply), 1699 (file apply) and 1839 (setStateInformation). The no-op retraction does not reverse the duck (:527-531).
• Engine: the state machine is evaluated once per host process() call (e769f33:src/dsp/AnabasisEngine.cpp:332-357, 486-548), per-sample legs at :1104-1127, applied on the processed path only (:1190-1194), and the render/meter tap includes it (:1245-1259). rewireWanted at :423-426.
• testAbSwitchRequestsDuck (e769f33:tests/state_tests.cpp:1165-1203) switches between two identical default slots and asserts the dip below 0.02, so it pins the no-op duck.
Measurement: I built a scratchpad probe against e769f33 AnabasisEngine.cpp (rt/verify-8/duck/main.cpp), sending requestForcedDuck with NO parameter change on a 300 Hz, 0.4 sine. Results:
• at 48 kHz, dip span 34.2 ms at a 64-sample host buffer, 38.3 at 256/512, 48.8 at 1024, 70.3 at 2048; exact silence 0.8, 4.9, 15.4 and 36.7 ms respectively;
• 400 ms-window energy −0.24 to −0.68 dB.
Not auditioned by ear: the harness has no audio monitoring.

**Corrections to the candidate claim.**

1. '~34 ms' is the floor, not the typical value. The bottom holds until the next host-block top, so the dropout grows with the host buffer: ≈38 ms at 512, 49 ms at 1024, 70 ms at 2048 (48 kHz).
2. An oversampling rewire (the ~45 ms case) never occurs on A/B, preset, undo or Copy. OS, phase and offline quality are ANABASIS_INTERNAL and never travel with them (ADR-0004 §Consequences :256-260). The colour-model and EQ-position 'inaudible rewire' cases arise from direct edits, not bulk swaps.
3. The integrated-LUFS pull is real and deliberate (comment at e769f33:src/dsp/AnabasisEngine.cpp:1245-1253) but immaterial. Each duck removes ≈26 ms-equivalent of energy at 512 samples, so 30 switches in a 3-minute pass move integrated by ≈0.02 LU.
4. 'Biases the comparison' is unevidenced; no listening test exists.
5. A contradiction the finding misses: ADR-0018:93-95 calls the undo duck 'inaudible-by-design', while the manual (e769f33:docs/user/USER_MANUAL.md:362-365, 490-492) and KI-010 describe an audible dip.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 4 · severity 2 · discoverability 2 · efficiency 2 · coherence 3 · change risk 4 · complexity 3 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DSP-003

**The TP toggle is a DSP no-op at oversampling 4x and above (it only relabels the Ceiling), and at those factors neither setting holds a dBTP ceiling, while the manual and tooltip say engaging TP catches inter-sample peaks**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Documentation | The dBTP delivery ceiling is claimed but not enforced, and its warning cannot tell an over from normal operation | Phase 0 |

**Evidence**

- e769f33:src/dsp/AnabasisEngine.cpp:653-656 — the ADR-0003 item 6 comment and `limiter.setTruePeakMode (p.truePeakMode && osN < 4)`, the only DSP read of truePeakMode.
- e769f33:src/PluginParameters.cpp:303-308 and e769f33:src/PluginParameters.h:106-110 — the unit follows truePeakMode only, not the OS factor.
- e769f33:src/gui/PluginEditor.cpp:76 — tooltip 'Catch inter-sample peaks - the Ceiling then holds in dBTP instead of sample peak'.
- e769f33:docs/user/USER_MANUAL.md:171, :206-207 — 'on, detection moves to the oversampled rate … inter-sample peaks are caught'. :282 — Oversampling 'for … true-peak accuracy'.
- e769f33:docs/architecture/design-decisions/ADR-0003-oversampling-scope-and-true-peak-measurement-tap.md:147-162 — at ≥4x the oversampled signal is read directly. :160-162 — the clamp's own 4x tap (not implemented; see [DSP-001](findings-dsp-tech.md#dsp-001)).
- G-17: session capture `rt/gestures/33b-tp-on-simple.png`, session capture `rt/gestures/33c-tp-on-adv.png` — the toggle relabels to dBTP and nothing else changes.
- verify-19 R7: [capture](captures/04-tp-over-ceiling-os4x.png) — 4x, TP off 1.39 vs TP on 1.27 dBTP.
- verify-19 probe: .../rt/verify-19/probe/matrix-all.txt — os=2 tp=0 vs tp=1, identical to the last digit. .../rt/verify-19/probe/forcemax.txt — Force Max tp=0 vs tp=1 identical.

**Current behaviour.** At Oversampling 4x/8x/16x, and on every Force Max offline render, toggling TP changes only the Ceiling's unit text (dB ↔ dBTP) in both views. The limiter, the clamp and the meters process identically. At those factors the output true peak sits 1.2-1.7 dB above the ceiling whether the label says dB or dBTP.

**Problem.** A user testing the switch hears and measures no difference at ≥4x and may conclude the switch is broken, or trust the dBTP label. The manual and tooltip describe an effect (detection moves to the oversampled rate, inter-sample peaks caught) that does not occur at these factors.

**Root cause.** ADR-0003 item 6 made the limiter read the oversampled signal directly at ≥4x in both TP positions. That is sound for the limiter's detector. The clamp-level TP estimate (ADR-0003:160-162, ADR-0006 D2) that would give TP mode an effect at every factor was never built. The UI copy and the Ceiling unit were written against the intended architecture, not the implemented one.

**User impact.** Only users who raise Oversampling are affected. For them the TP switch is inert and its dBTP label is misleading, which feeds directly into [DSP-001](findings-dsp-tech.md#dsp-001)'s delivery risk. Low frequency on its own, but it removes the user's only lever for true-peak compliance at those settings. *Scope:* The TP toggle (Simple and Advanced) and the Ceiling unit at int_oversample ≥ 4x or Force Max offline. Also USER_MANUAL §3.2/§3.3 and the Oversampling row, and the TP tooltip.

**Proposed improvement.** Resolve through [DSP-001](findings-dsp-tech.md#dsp-001), with no separate limiter change. Once the clamp's TP-driven gain exists, TP mode has a real effect at every factor: the clamp decides on its own TP estimate. The toggle then means the same thing everywhere, and the dBTP label becomes true.

Then rewrite USER_MANUAL :171, :206-207 and :282 and the tooltip at PluginEditor.cpp:76 to describe what TP does per factor:
- limiter detection at Off/2x;
- clamp TP gain at every factor.

If [DSP-001](findings-dsp-tech.md#dsp-001) is deferred, correct the manual and tooltip now to state that at 4x and above the toggle currently changes only the unit and that the output is not held in dBTP.

**Alternatives considered.**

- *Give the limiter a TP estimator at ≥4x too.* — Rejected. It double-resamples (ADR-0003 option E) and still cannot catch the post-decimation overshoot that dominates at ≥4x.
- *Show 'dBTP' whenever OS ≥ 4x, or disable the toggle at ≥4x.* — Rejected. The output is not TP-held at ≥4x, so the label would be false. It would also contradict ADR-0015 D5 (the unit follows the mode).
- *Leave as-is.* — Rejected. The documented behaviour is false at these factors.

**Decision: Modify · P2.** The toggle's inertness is a symptom of the missing clamp TP stage, not a limiter bug. A constrained fix — let [DSP-001](findings-dsp-tech.md#dsp-001) give TP its effect, then correct the copy — avoids redundant DSP work. The documentation correction can land immediately.

**Architecture gates.**

- Changing the Ceiling unit rule (e.g. to follow the OS factor) would conflict with Accepted ADR-0015 Decision 5. The proposal does not do this.
- Any DSP effect for the toggle arrives through [DSP-001](findings-dsp-tech.md#dsp-001)'s clamp change and carries its gates (Ceiling guarantee, Latency, DSP Graph).

**Dependencies.** [DSP-001](findings-dsp-tech.md#dsp-001)

**Acceptance criteria.**

- After [DSP-001](findings-dsp-tech.md#dsp-001): at 4x, 8x, 16x and Force Max, rendering the same stimulus with TP off vs on gives a measurably different output. With TP on, output true peak ≤ ceiling + 0.1 dB.
- USER_MANUAL §3.2 (:171), §3.3 (:206-207), the Oversampling row (:282) and the TP tooltip state the implemented per-factor behaviour, with no claim contradicted by the probe or harness measurements.
- If [DSP-001](findings-dsp-tech.md#dsp-001) is deferred: the manual and tooltip say explicitly that at 4x and above the TP toggle changes only the Ceiling unit and that inter-sample peaks are not held.

<details><summary>Verification record</summary>

**Method.** Code: grep of src/ for truePeakMode. The only DSP consumer is AnabasisEngine.cpp:656 (`p.truePeakMode && osN < 4`). Every other use is UI or label: PluginEditor.cpp:76/547/593/2103, PluginParameters.cpp:303-308/377, PluginProcessor.cpp:27.

Probe (real engine) at 4x min-phase with Loudness-50% values: TP off and TP on produce identical statistics — +1.579 dBTP, pre-clamp +2.047 dBFS, 1305/1918976 samples clamped. Force Max (16x) TP on and off are identical at +1.399.

Harness at 4x on :149: TP off 1.39 and TP on 1.27 dBTP (different music windows, same regime). Viewed gestures/33b and 33c.

**Corrections to the candidate claim.** Confirmed. One correction: at 4x with TP off, the finding said the knob reads 'dB' while the detector already behaves as true-peak. That holds only for the limiter's detector on the pre-decimation region signal. The OUTPUT is not TP-held at ≥4x in either position (+1.2…+1.6 dBTP), so the 'dB' label with TP off is the honest one and the 'dBTP' label with TP on is the false claim ([DSP-001](findings-dsp-tech.md#dsp-001)).

The missing meter feedback is by design: the TP row always measures true peak (ADR-0020). Stale holds on toggling are [VIS-008](findings-visualisation.md#vis-008).

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 2 · severity 3 · discoverability 4 · efficiency 2 · coherence 4 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DSP-004

**Any non-zero Clip Drive low-passes the whole programme at OS Off (ADAA-1: about -2.0/-5.1/-11.7 dB at 10/15/20 kHz at 48 kHz); the Loudness macro switches this on abruptly at 30 %, while OS defaults to Off**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P1** | high | confirmed | Technical robustness | Listening aids and default voicing do not do what the product says | Phase 0 |

**Evidence**

- e769f33:src/dsp/ClipSat.h:28-47 (droop, half-sample delay, discontinuous engage; 'audible on bright material the moment drive leaves zero')
- e769f33:src/dsp/ClipSat.h:195 and e769f33:src/dsp/ClipSat.h:249-253 (clipOn on any non-zero drive; ADAA divided difference)
- e769f33:src/dsp/ClipSat.h:437-443 (antiderivative 0.5a² in the linear region)
- e769f33:src/MacroEngine.h:54-56 (clipDrive 0 below l=0.3, then ramps)
- e769f33:src/InternalState.h:105-107 (oversample Off, offlineQuality Follow: the bounce also runs at base rate)
- e769f33:docs/TEST_REPORT.md:30-37 (+1.3 dB 'recovery' at 11.72 kHz on a saturated stimulus)
- e769f33:tests/dsp_tests.cpp:2407-2410 (fundamental delta tolerance < 2.5 dB)
- e769f33:docs/KNOWN_ISSUES.md:292-333 (KI-005 engage step only; :296-297 says 'Character-macro')
- e769f33:docs/user/USER_MANUAL.md:282 ('Higher = cleaner') and e769f33:docs/user/USER_MANUAL.md:442-444 (OS advice only at heavy clipping)
- e769f33:docs/user/USER_MANUAL.md:424-430 (the transparent-master workflow raises Loudness from 25 %, which crosses 30 %)
- V15-03 runtime: Loudness 30 % -> clipDrive 0.0 dB, 30.5 % -> 0.07 dB (engaged), latency 480 (OS Off) — rt/verify-15/app.log
- V15-04 numeric re-implementation of the ClipSat path — rt/verify-15/droop.py

**Current behaviour.** At default settings (OS Off, Offline Follow), once Loudness exceeds 30 % the clipper engages. Its ADAA-1 kernel then filters the whole programme through (1+z^-1)/2, costing -2.0/-5.1/-11.7 dB at 10/15/20 kHz at 48 kHz, or -2.4/-6.4/-16.7 dB at 44.1 kHz, plus a half-sample delay. The full loss arrives in one step at the 30 % crossing, regardless of drive amount, and is printed into the offline bounce. Nothing in the UI or the manual says so.

**Problem.** The product's central control silently darkens the top end of every master pushed past 30 % on the default path. It contradicts the level-compensated, peak-shaving description of the clipper and the 'transparent master' workflow. The only cure, oversampling, is described in the manual as a heavy-clipping refinement.

**Root cause.** First-order ADAA at the base rate is inherently a two-tap boxcar in the curve's linear region. The design pairs it with an OS Off (and Offline Follow) default, and the macro curve switches the stage from exact skip to ADAA at a fixed Loudness position. There is no compensation stage, and nothing outside code comments and TEST_REPORT records the trade-off.

**User impact.** On bright material the air band drops audibly the moment Loudness passes 30 % and stays down. In MATCH comparisons the user hears 'the maximizer dulls the mix' and may push Tone or add a high shelf. A ±2 dB Tone tilt around 700 Hz cannot restore 15-20 kHz without over-lifting 1-8 kHz. The loss is baked into bounces at default settings. *Scope:* Every session at OS Off with clipDrive > 0: 9 of 13 factory presets, any Loudness above 30 % and any manual Drive. The loss is worst at 44.1 kHz. At 2x oversampling about -2 dB at 20 kHz remains; at 4x it is about -0.5 dB.

**Proposed improvement.** Target: at default settings, the small-signal (linear-region) response of the clip stage stays within ±0.5 dB up to 16 kHz at 44.1 and 48 kHz, and a Loudness sweep through 30 % produces no spectral step. Candidate routes, ranked: (a) Design a fixed droop-compensation pre-emphasis, a short FIR or biquad approximating 1/cos(πf/fs) up to about 0.75 of Nyquist, active only on the driven branch. Build it together with the KI-005 time-based engage ramp so that the identity-to-ADAA join is crossfaded and drive exactly 0 stays bit-identical. (b) Make a non-Off oversampling default an owner decision, backed by these figures (costs latency and CPU; 2x alone leaves -2 dB at 20 kHz). Immediately, regardless of route: add a KNOWN_ISSUES entry (or widen KI-005) with the steady-state figures; state the OS Off top-end cost in the manual's Settings Oversampling row and in the §8 workflows; correct KI-005's 'Character' to 'Loudness'; and add the linear-region figure to TEST_REPORT beside the +1.3 dB saturated-stimulus figure.

**Alternatives considered.**

- *Leave as-is (documented only in code)* — Rejected. It is an audible, default-path tonal change hidden from users and printed into bounces.
- *Default oversampling to 2x or 4x* — Effective (4x leaves about -0.5 dB at 20 kHz), but it is a reported-latency change and raises CPU; owner decision. 2x alone does not meet the target at 20 kHz.
- *Bypass ADAA in the linear region (output u directly)* — Removes the droop, but the delay and transfer then switch at every knee entry. KI-005's discontinuity would become a per-transient artefact. Rejected.
- *Change the clipDrive curve (e.g. engage drive from Loudness 0)* — A macro-layer contract change with a post-freeze recall cost. It moves the step to 0 % and spreads the droop to every setting. Rejected.
- *Documentation-only fix* — Necessary as a first step but not sufficient for a mastering product's default path.

**Decision: Modify · P1.** Confirmed by code, by a numeric re-implementation of the exact code path, and at runtime for the 30 % threshold and the OS Off default. It affects the default configuration on most factory presets and every pushed master, and ends up in the bounce. The engineering fix passes through gates, so documentation should land now and the DSP fix go through review.

*Calibration:* the verifier judged Proceed / P1; the final judgement is Modify / P1. Challenge agreed P1 (reproduced on the real engine: -2.01/-5.11/-11.74 dB at 10/15/20 kHz, 48 kHz, OS Off; printed into the bounce on every pass above 30 % Loudness, 9 of 13 presets); not P0 (default patch clean, cure via OS 4x). Changed Proceed->Modify: now, doc/UI-copy/tooltip fixes, KI-005's Character->Loudness correction, the '~0.9 dB' test comment (dsp_tests.cpp:2408), and a linear-region regression test pinning today's droop; the DSP remedy is the owner's ⊕ oversampling-default decision at the listening pass (reported-latency change gate). A compensation filter is demoted: it erodes invariant-6 alias margins (the >8 dB assertion is left ~0.15 dB even for an ideal filter), and an FIR form is a reported-latency hard stop under ADR-0003 item 2; minimum-phase only, re-measured.

*Adversarial challenge:* evidence holds: yes; priority justified: yes (suggested P1); decision justified: no (suggested Modify). The evidence holds and is now reproduced on the real engine; the judge's figures are exact at both rates. P1 is the right priority. The droop affects every mastering pass above 30 % Loudness at the default OS/offline settings, including 9 of 13 presets and the manual's own §8 workflows, and it is printed into the bounce. It is not P0: the default patch (Loudness 0) is clean, the output is tonally coloured but not broken, the user can cure it with OS 4x, and the behaviour is a trade-off already documented in code. Severity 4 and frequency 4-5 are fair, and evidence_confidence should now be 5. I change the decision to Modify because the proposal ranks the compensation filter first without its costs. The filter trades away invariant-6 alias rejection: measured, the >8 dB assertion is left with 0.15 dB margin even for an ideal filter. It needs +6 to +8 dB of top-octave boost, and in its FIR form it collides with ADR-0003 item 2. The constrained change: (a) now, the doc and UI-copy fixes the judge lists, plus the tooltip, the test-comment correction and KI-005's Character-to-Loudness fix; (b) now, a linear-region regression test that pins today's droop (for example -2.01 dB at 10 kHz, 48 kHz, OS Off) so any later DSP change is deliberate and not accidental; (c) the DSP remedy framed as an owner ⊕ decision on the oversampling default, where 4x is the only route verified to meet the target. That decision waits for the fine-review listening pass. A compensation filter should be kept only as a minimum-phase option to be re-measured under invariant 6, not as the leading route. *Proposal risks:* (1) Route (a), a droop-compensation filter ranked first, gives back part of what ADAA-1 suppresses, and the judge's gate list omits DSP_POLICY invariant 6. I ran testClipAdaaReducesAliasing's exact stimulus through the real ClipSat and applied an IDEAL zero-phase 1/cos(πf/fs) compensation after the ADAA (session probe `rt/challenge-dsp004/alias.cpp`, not committed). The folded-5th margin falls from 10.43 to 8.15 dB against the > 8 dB assertion at e769f33:tests/dsp_tests.cpp:1009. The folded-3rd margin falls from 14.79 to 11.27 dB. The f0 delta moves from -1.10 to +1.75 dB against < 2 at :1010-1011. A realisable filter would sit on or past those thresholds, and the alias figures TEST_REPORT records under invariant 6 would regress by 2.3-3.5 dB. Placing the filter before the clipper avoids that but drives the high band harder into the curve, which means more HF clipping, more aliasing, and different shaving. (2) The stated target (±0.5 dB to 16 kHz at 44.1 kHz) needs +7.6 dB of boost at 16 kHz (+6.0 dB at 48 kHz), rising toward Nyquist. Placed after the clipper, that lifts HF peaks into the limiter, so bright programme gets more gain reduction and GR/loudness behaviour changes. (3) The judge offers a 'short FIR or biquad' 'on the driven branch only'. A linear-phase FIR there adds a delay that depends on the parameter. That conflicts with Accepted ADR-0003 item 2 (e769f33:docs/architecture/design-decisions/ADR-0003-oversampling-scope-and-true-peak-measurement-tap.md:125-128: latency is a pure function of (factor, phase) and no automatable parameter can move it; clipDrive is automatable) and with DSP_POLICY invariant 2. It is a reported-latency hard-stop the judge did not name for route (a). Only a minimum-phase design with no added bulk delay is admissible. (4) I also checked a route nobody listed: ADAA on the nonlinear residual only, y = u + ADAA[f(u)-u] (session probe `rt/challenge-dsp004/resid.py`, not committed). The small-signal response is flat and the alias margins are unchanged. But on the +12 dB hard-clip 11.72 kHz tone the peak rises from 0.251 (the 1/g flat top) to 0.691 and f0 rises 6.7 dB, so the clipper stops shaving bright peaks. At base rate there is no cheap cure; the oversampling factor is the only clean lever. (5) Route (b) is verified: 4x meets the target (-0.30/-0.36 dB at 16 kHz) and 2x does not (-1.25/-1.49 dB). A 4x default raises reported latency for new instances and costs CPU. Check how a restored session that lacks int_oversample resolves, because a default change would otherwise move old sessions' PDC. This decision belongs in the owner's pending ⊕ listening pass (e769f33:docs/HANDOVER.md:32, item (c)). (6) An alternative the judge did not list: an Offline 'Force Max' default fixes only the bounce and makes what you hear differ from what you print, which is a worse trap. (7) The acceptance criterion at Loudness 60/100 % sends -20 dBFS through the full engine, where limGain (up to +18 dB) and the compressor act. The 'level-matched response' must isolate the clip stage (for example via the StageTrace clipOut tap), or the test is also measuring the comp and limiter. (8) The doc and copy fixes should also cover the Oversampling tooltip at e769f33:src/gui/PluginEditor.cpp:759-762 ('Cleaner nonlinear stages... CPU and adds latency'; no top-end cost) and the dsp_tests.cpp:2408 comment. Tooltips default to off (InternalState.h:111), so the manual row at USER_MANUAL.md:282 is where most users will read it. None of these copy edits crosses a gate.

**Architecture gates.**

- Latency change — if the fix is a non-Off oversampling default (reported latency grows by the OS filter latency)
- DSP Graph change — if a droop-compensation sub-block is added to the Clip/Sat stage (ADR-0003 oversampled region; DSP_POLICY invariant 7 bit-identity at drive 0 must hold)
- COMPATIBILITY_POLICY recall — saved sessions and presets above Loudness 30 % will sound brighter after any DSP fix
- Macro-layer contract change + PARAMETER_COMPATIBILITY rule 7 — only for the rejected curve-change alternative

**Dependencies.** KI-005 (the time-based engage ramp must be co-designed with any compensation); [MODEL-005](findings-state-model.md#model-005) (the Tone tilt cannot offset the droop); Owner ⊕ decision on the oversampling default

**Acceptance criteria.**

- A new dsp_tests case drives -20 dBFS sines at 1, 5, 10 and 16 kHz through the engine at 44.1 and 48 kHz with default OS/offline settings. At Loudness 31, 60 and 100 %, the level-matched response differs from Loudness 29 % by at most 0.5 dB at every test frequency (or by an owner-set figure recorded in the test).
- Sweeping Loudness from 29 % to 31 % during playback produces no step above 0.5 dB below 16 kHz, and the KI-005 curvature measurement shows no engage transient.
- testClipDriveZeroIsBitExact stays green (drive exactly 0 remains bit-identical).
- Until the DSP fix lands, KNOWN_ISSUES carries the steady-state droop figures at 44.1 and 48 kHz; the USER_MANUAL Oversampling row and §8 workflows state the OS Off top-end cost; KI-005 names Loudness, not Character.
- TEST_REPORT records the linear-region droop at 11.72 kHz / 48 kHz (-2.85 dB) next to the +1.3 dB saturated-stimulus figure.

<details><summary>Verification record</summary>

**Method.** Code read at ClipSat.h:23-47 and 183-270: clipOn = drive != 0 exactly, and in the linear region the ADAA-1 divided difference of 0.5a² is (u+u1)/2. Read ClipSat.h:437-450 (antiderivative), MacroEngine.h:54-56, InternalState.h:105-107 (defaults OS Off, offline quality Follow), TEST_REPORT.md:26-38, KNOWN_ISSUES.md:292-333, dsp_tests.cpp:2400-2410 and USER_MANUAL.md:282-284 and 424-446. I re-implemented the ClipSat drive -> ADAA -> 1/g path numerically, driving a -12 dBFS sine through the exact formulas (rt/verify-15/droop.py). At 48 kHz: -0.47/-2.01/-5.11/-11.74 dB at 5/10/15/20 kHz. At 44.1 kHz: -0.56/-2.42/-6.35/-16.74 dB. Results were identical at 0.01 dB and 3.86 dB drive. Runtime on :145: 'param loudness' 0.29, 0.30, 0.305, 0.31 and 0.40 gave clipDrive 0.0, 0.0, 0.07, 0.13 and 1.3 dB, with latency 480 samples (OS Off). The plugin's output was not captured; the harness has no analyser.

**Corrections to the candidate claim.** The 48 kHz figures are right, and at 44.1 kHz the loss is larger. 'Shaves peaks, not tone' paraphrases ClipSat.h:23-26, which says 'shaves peaks (crest reduction) instead of getting louder'. The TEST_REPORT +1.3 dB comes from a +12 dB hard-clip stimulus. The linear-region droop at 11.72 kHz / 48 kHz is -2.85 dB, so the report understates what below-knee programme loses, and the test's < 2.5 dB tolerance would not catch it. KI-005 covers only the one-sample engage step. The steady-state droop appears in no KNOWN_ISSUES entry and not in the manual. KI-005 also blames a 'Character-macro move'; Loudness drives clipDrive. At 30 % the droop switches on in one step, at full depth for any drive > 0, while the drive value itself rises continuously from 0. Of the 13 factory presets, 9 sit above 30 %; Acoustic Warmth's 30 lands at drive 0.

</details>

<sub>Verifier scores (1-5): impact 4 · frequency 5 · severity 4 · discoverability 4 · efficiency 3 · coherence 4 · change risk 4 · complexity 4 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DSP-005

**MATCH only ever attenuates, rests on a limiter-only predict floor for the first ~3 s, and the engine comment on the floor's error direction contradicts its own arithmetic**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | partially-confirmed | Technical robustness | Listening aids and default voicing do not do what the product says | Phase 1 |

**Evidence**

- e769f33:src/dsp/AnabasisEngine.cpp:686-704 — measure = clamp(dryS - wetS) gated on momentary > -70; predictDb = -max(0, inputGain + limGain + limiter deepest GR); applied = min(measure, predict)
- e769f33:src/dsp/AnabasisEngine.cpp:1034-1042 — comment: 'the floor therefore UNDER-estimates the lift whenever the compressor is doing the work, and min(measure, predict) hides that once the measure converges' (arithmetic says over-estimate, kept by min)
- e769f33:src/dsp/AnabasisEngine.h:566 — 'average measured GR' vs code's per-call minimum (grMinLinear, AnabasisEngine.cpp:695-702, 782)
- e769f33:src/dsp/LoudnessMeter.h:180, 433-441 — shortTermLufs = kSilentLufs until 30 x 100 ms sub-blocks
- e769f33:src/MacroEngine.h:52-54 — Loudness drives compThreshold to -12 dB and ratio 1.5-2.0 with no makeup gain; the compressor's reduction is outside the predict term
- e769f33:docs/architecture/design-decisions/ADR-0006-ceiling-guarantee.md:152-163 (D7: predict = input gain + limiter gain − expected GR, floor-only) and :203 ('compensation never gets louder')
- e769f33:tests/dsp_tests.cpp:3426, 3466 — the only level test uses a sine with no compressor and allows ±3.5 dB from dry, so this bias is untested
- Runtime rt/verify-7/steady.log (MATCH on from start). Pink L=50/70/90 at 11 s: dS -0.73/-0.63/-0.79. Music L=50/70/90: -1.78/-2.36/-2.69. First 3 s dM range +0.26..-1.07
- Runtime rt/verify-7/steady_mo.log (predict removed): the same runs converge to pink dS -0.05..0.00 and music dS within ±1.4 of the chorus transition. But the first 2-4 s run +3.4..+9.6 LU louder, which is why predict exists
- Runtime rt/verify-7/comp.log vs comp_measureonly.log (compThr -24, ratio 4, limGain 6). With predict: music out S -20.06 vs dry -16.25 (render -14.24), pink -19.43 vs -14.04 (render -13.43). Predict removed: music -16.12, pink -14.05 (matched)
- Runtime rt/verify-7/move.log (L 20->80 at 10 s, MATCH on): music dM +0.52 at +0.5 s then -1.3..-1.7. Pink +0.62 then -0.5..-0.9

**Current behaviour.** With MATCH on, the monitored processed signal settles below the dry loudness by ~0.6-0.9 LU (pink) and ~1-2.7 LU (music) at Loudness >= 50. At compressor-heavy settings it settles 4-5 LU below, and stays there because min() always prefers the over-estimating predict floor. At transport start and after Loudness moves, the predict floor tracks within ~1 LU, erring low. When processing lowers loudness, MATCH does nothing (by ADR design).

**Problem.** MATCH is the product's honesty feature ('if it only sounded better because it was louder, you just found out'). A systematic bias that makes the processed signal quieter than the dry reverses the louder-is-better bias rather than removing it. It also varies with how much of the work the compressor does, so A/B comparisons between slots with different compression are unfair by different amounts. The in-code comment tells maintainers the opposite of what happens.

**Root cause.** The predict floor's 'expected GR' counts only the limiter's deepest per-block reduction. It omits the compressor's (and clipper's) reduction, so the predicted lift exceeds the true lift whenever the compressor works, and min(measure, predict) keeps the too-deep floor permanently. The comment at :1034-1042 got the direction wrong, so the defect was classed as 'a P5 legend item rather than a defect today'.

**User impact.** Judging at 'matched' loudness, a user hears their processed master 1-3 LU (up to 5 LU comp-heavy) quieter than the input. They may under-push, reject a good setting, or prefer the less-compressed A/B slot for level reasons alone. The bias is invisible, because nothing displays the applied gain (see [VIS-010](findings-visualisation.md#vis-010)). *Scope:* Every realtime MATCH use at moderate-to-high Loudness and any compressor-heavy Advanced setting. Offline renders are unaffected. It compounds with [UX-009](findings-ux.md#ux-009): the bypass comparison is also scaled by this g.

**Proposed improvement.** Smallest change within ADR-0006 D7: make 'expected GR' include the compressor's current gain reduction (MasteringComp::currentGainReductionDb already exists, MasteringComp.h:308-319), and evaluate including the clipper's level reduction. Keep predict floor-only and stateless, keep attenuation-only. Correct the comments at AnabasisEngine.cpp:1034-1042, :695-699 and AnabasisEngine.h:566. Add a test: at the macro's L=70 and a comp-heavy setting on pink noise, the converged monitored S-LUFS lies within ±0.5 LU of dry, and the first 3 s never exceed dry by > 1 LU for longer than 0.5 s. Attenuation-only itself: Preserve (ADR decision). Surface it instead through the MATCH gain readout proposed in [VIS-010](findings-visualisation.md#vis-010).

**Alternatives considered.**

- *Hand over from predict to measure once the short-term measure is valid and parameters have been static for >= 3 s* — Removes the persistent bias regardless of predict accuracy. But it makes the combination stateful, against ADR-0006 D7's 'absolute, stateless' predict, so it needs a gate/ADR amendment. Keep it as a fallback if the comp-GR term is not accurate enough.
- *Allow MATCH to boost when processing lowers loudness* — Conflicts with ADR-0006 D7 and the 'never gets louder' consequence. Rare for a maximizer. Reject for now; the readout makes the case visible.
- *Fix only the comment* — Insufficient: the measured bias is user-audible (1-5 LU).
- *Leave as is* — Rejected: the reproduced bias undermines the product's primary comparison tool.

**Decision: Modify · P2.** The defect is reproduced and causally attributed (predict removed -> matched). The obvious 'rewrite MATCH' is unnecessary: adding the compressor term to the ADR's own 'expected GR' stays inside ADR-0006 D7 and inside the monitor-only path. The attenuation-only sub-claim is a deliberate Accepted-ADR decision and is preserved. The '~3 s louder' sub-claim was refuted, so priority rests on the persistent over-attenuation. The verifier rated it P1 because it affects the recommended judging workflow in most MATCH sessions; calibration lowered it to P2 (below).

*Calibration:* the verifier judged Modify / P1; the final judgement is Modify / P2. Challenge accepted: the converged bias is ~0.7-1.3 LU at macro settings, in the ADR-intended conservative direction; the 4-5.5 LU case needs a heavy manual compressor; so P2. Keep stateless and attenuation-only (ADR-0006 D7); reuse the block-end compGrDb at the next block top (no new cross-thread path); drop the clipper term (level-compensated, no tap); prototype both acceptance criteria before committing; fix the inverted and stale comments (e769f33:src/dsp/AnabasisEngine.cpp:1034-1042). Sequence after [UX-009](findings-ux.md#ux-009), when this bias becomes the whole bypass gap.

*Adversarial challenge:* evidence holds: yes; priority justified: no (suggested P2); decision justified: yes (suggested Modify). The defect, its direction and its cause are all confirmed, and the comment is wrong in two places. Modify is the right, minimal decision, and attenuation-only is correctly preserved as an ADR choice. Priority is overstated. At Simple-mode macro settings the converged bias is about 0.7-1.3 LU (short-term), not up to 2.7. It is in the ADR-intended conservative direction: the wet is never flattered (ADR-0006 Consequences). Nothing is rendered wrong. The large 4-5.5 LU bias needs a heavy manual Advanced compressor with no makeup, which is less frequent. A ~1 LU conservative bias leads at worst to a slightly under-pushed master, which is lower cost. That is P2 under the rubric (borderline; P1 only if a ~1 LU bias in the matched comparison is treated as material in every session). It should be sequenced after [UX-009](findings-ux.md#ux-009), because once the bypass is unity this residual bias becomes the whole of the bypass gap. *Proposal risks:* (1) compGrDb and MasteringComp::currentGainReductionDb (MasteringComp.h:308-319) are the envelope GR at block end, with a fast/slow blend under auto-release. That is a peak-ish per-block figure, not a loudness-weighted average. On kick-driven material it swings per block, so the predict, and during the first ~3 s after prepare the monitor gain itself, would modulate with the beat. That approaches the 'no continuous AGC' limit of inv 10 and could over-state the loudness loss. (2) The two acceptance criteria may conflict for a stateless floor: '±0.5 LU converged' pushes the predict shallower, and 'never >1 LU over dry for >0.5 s in the first 3 s' needs it accurate from above. The measure-only run shows +3.4..+9.6 LU early excess once the floor stops covering the lift. Prototype both before committing to the criteria. (3) Reuse the existing per-block compGrDb stored at :783, read at the next block's top the way grMinLinear is. That keeps the change on the audio thread with no new cross-thread path. Do not route it through the GUI-published atomics' contract. (4) Drop 'evaluate the clipper's level reduction'. ClipSat drive is level-compensated (ClipSat.h:23), it adds no lift, and no clipper-GR tap exists, so adding one is new DSP instrumentation with no evidence behind it. (5) Keep the change stateless and attenuation-only (ADR-0006 D7), as the judge noted. The handover alternative would be a D7 conflict and a hard stop.

**Architecture gates.**

- ADR-0006 D7 — the change must stay within 'input gain + limiter gain − expected GR, floor-only, stateless'. Adding compressor GR to expected GR complies. A predict/measure handover (stateful) or any boost would conflict with D7 and the 'never gets louder' consequence, which is an Accepted-ADR conflict and a hard stop
- DSP_POLICY invariant 10 — must remain monitor-only and must not become a continuous AGC

**Dependencies.** [UX-009](findings-ux.md#ux-009) (matched bypass uses the same g); [VIS-010](findings-visualisation.md#vis-010) (MATCH gain readout would expose residual bias)

**Acceptance criteria.**

- Pink noise at -12 dBFS and the harness music at -6 dB, Loudness 50/70/90, MATCH on: after >= 6 s the monitored short-term loudness is within ±0.5 LU of dry (pink), and within ±1 LU of dry on music outside chorus transitions.
- compThreshold -24, ratio 4, limGain 6: monitored S-LUFS within ±1 LU of dry after >= 6 s. Today it is -3.9 (music) / -5.3 (pink).
- From transport start and after a Loudness 20->80 jump, the monitored momentary loudness never exceeds dry by more than 1 LU for longer than 0.5 s.
- Offline output with MATCH on stays bit-identical to MATCH off.
- The comments at AnabasisEngine.cpp:695-699 and :1034-1042 and AnabasisEngine.h:566 describe the implemented GR term and error direction correctly.
- A committed test pins the converged matched level with the compressor engaged.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/dsp/AnabasisEngine.cpp:668-704, :1034-1042, :1054-1056, e769f33:src/dsp/AnabasisEngine.h:562-570, e769f33:src/dsp/LoudnessMeter.h:179-180, 433-441, e769f33:src/MacroEngine.h:52-58 (macro curves: comp threshold to -12 dB, ratio up to 2, no makeup; clip drive level-compensated, ClipSat.h:23), ADR-0006 D7 (:152-163) and Consequences (:203). Probe rt/verify-7/probe.cpp on the real processor (realtime, 48 kHz/512), measuring dry vs listening output with the product's LoudnessMeter: MATCH on from transport start at L=30/50/70/90 (steady.log), a Loudness jump 20->80 mid-stream (move.log), a comp-heavy manual setting (compThr -24, ratio 4, limGain 0/6; comp.log). Causal check: a scratchpad copy of AnabasisEngine.cpp with applied = min(0, measure), i.e. predict removed (eng_measureonly.cpp -> steady_mo.log, comp_measureonly.log). The repository was not touched.

**Corrections to the candidate claim.** Confirmed. (a) Attenuation-only (applied = min(measure, predict <= 0)). This is an explicit ADR-0006 D7/Consequences decision, not an oversight. (b) Predict alone acts for the first ~3 s (short-term = kSilentLufs until 30 sub-blocks). (c) The comment at :1034-1042 is inverted. A missing compressor-GR term makes the predicted lift larger and predictDb more negative, and min() KEEPS it. It is not hidden; it persists after the measure converges. Refuted or overstated: 'for seconds after a Loudness move the wet can still be louder'. Measured wet-minus-dry momentary is +0.26..-1.0 LU during the first 3 s from transport start. After a 20->80 jump the wet exceeded dry by at most +0.5/+0.6 LU for ~0.5 s, then went BELOW dry. The dominant error is the opposite one: persistent over-attenuation, and it is larger than the finding implied. Converged matched wet vs dry: pink -0.6..-0.9 LU and music -1.1..-2.7 LU at L>=50. Comp-heavy manual setting with limGain 6: -3.9 (music) / -5.3 (pink) LU, where the render is actually +0.7..+1.7 LU louder than dry. With predict removed, the same runs converge to dry (pink within +/-0.1 LU), which attributes the bias to the predict floor. The header comment (AnabasisEngine.h:566) says 'average measured GR', while the code uses the per-call deepest limiter GR (:695-702). The 'more aggressive than a mean' remark at :695-699 reads inverted if 'aggressive' means more attenuation (the deepest GR yields a smaller predicted lift). Where processing lowers loudness (comp-heavy, limGain 0: render 3.8-5.4 LU below dry), MATCH leaves the wet that much quieter, as ADR-0006 intends.

</details>

<sub>Verifier scores (1-5): impact 4 · frequency 4 · severity 4 · discoverability 5 · efficiency 2 · coherence 4 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DSP-006

**Changing oversampling mid-playback reports the new PDC to the host at once, while the engine keeps the old delay for up to one block plus 6 ms, inside the duck fade, and then mutes for about 45 ms (KI-004)**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Preserve** | **none** | high | confirmed | Technical robustness | Listening aids and default voicing do not do what the product says | — |

**Evidence**

- e769f33:src/PluginProcessor.cpp:37 — onLatencyInputChanged calls updateLatency
- e769f33:src/PluginProcessor.cpp:896-915 — setLatencySamples(predictLatencySamples) on the message thread
- e769f33:src/InternalState.h:259-265 — recompute on oversample, osPhase or offlineQuality
- e769f33:src/dsp/AnabasisEngine.cpp:423-424,486-498 — rewire latched at the silent bottom; bottom held for delaySamples + osLatBase
- e769f33:docs/KNOWN_ISSUES.md:204-290 — KI-004, accepted by design (ADR-0004's trade), including the ~45 ms mute
- e769f33:docs/architecture/design-decisions/ADR-0004-latency-contract-constant-lookahead-allowance.md — Decision items 1, 4 and 5
- e769f33:docs/user/USER_MANUAL.md:357-361 — 'take effect at a click-free moment'
- ST-11: measured latency steps — session capture `rt/state/21a-settings.png`
- verify-9 repro: latency=480, then 541 after selecting 4x Linear — session capture `rt/verify-9/09c.png`

**Current behaviour.** Selecting a new Oversampling factor or Phase immediately calls setLatencySamples with the new total.

The engine ducks the processed path (about 6 ms out), latches the new oversampler at the silent bottom on a block boundary, and holds that bottom until the lookahead line and the oversampler refill. It then fades back in over about 28 ms.

The end-to-end mute is about 45 ms. The alignment disagreement is 67 samples or fewer and falls within the fade.

**Problem.** Any PDC change during playback lets the host re-align, which some hosts do audibly. The plugin itself adds a short deliberate mute.

Both follow directly from ADR-0004's decision that reported latency includes osLatency(factor, phase), which is recomputed on Settings changes.

**Root cause.** This is the latency contract by design (ADR-0004 items 1 and 5). PDC is recomputed on the message thread when the setting changes, and the engine latches at the next silent duck bottom to avoid a mid-block latency change (item 4).

**User impact.** Rare and low. Oversampling and Phase are session-level Settings, usually changed once. A mid-playback change costs about 45 ms of silence plus whatever re-sync the host does. Offline renders are unaffected, because entering offline adopts the configuration directly. *Scope:* Only live playback with a Settings change to Oversampling, Phase or Offline Render. Presets, A/B, undo and Lookahead never change PDC.

**Proposed improvement.** Keep the engine behaviour and the PDC timing.

Optionally add one sentence to USER_MANUAL §3.5/§6: 'Changing Oversampling or Phase changes the delay Anabasis reports; some hosts briefly re-sync, so change it while stopped.'

The in-panel latency readout from [UX-014](findings-ux.md#ux-014) covers visibility.

**Alternatives considered.**

- *Report the new PDC only once the engine has latched* — Rejected. It needs audio-to-message signalling of the latch (threading-model review) and still triggers the host re-sync, so the user gains nothing.
- *Always report the maximum oversampling latency (10 ms + 67 samples) so Settings never change PDC* — Removes host re-sync on Settings changes, but adds up to 1.4 ms in every configuration. It contradicts ADR-0004 item 1 and DSP_POLICY invariant 2's exactness clause (reported-latency gate).
- *Crossfade old and new oversampling paths instead of ducking to silence* — Runs two oversamplers at once during the switch, needs a dry-fill or double-path design (ADR-0004 consequences, KI-010 territory) and DSP-order review. That is disproportionate for a rare Settings action.

**Decision: Preserve · none.** The behaviour is exactly the Accepted ADR-0004 trade and is recorded as KI-004, pinned by testDuckWrapsOsLatch and testOsLatencyMatrix.

The misaligned window is inaudible because it lies inside the fade. Every alternative either touches a hard-stop gate (reported latency, threading) or costs more than a rare Settings action justifies.

Only the one-line manual clarification is worth doing. It is documentation, not a behaviour change.

**Architecture gates.**

- reported-latency change — alternatives 1 and 2 only; the Preserve decision touches none
- threading-model change — alternative 1 only
- Accepted ADR-0004 items 1 and 5 — alternative 2 conflicts
- DSP_POLICY invariant 2 (exact reported latency) — alternative 2

**Dependencies.** [UX-014](findings-ux.md#ux-014); KI-004

**Acceptance criteria.**

- testDuckWrapsOsLatch and testOsLatencyMatrix remain green, and the reported latency per Oversampling x Phase matches Latency.h.
- KI-004 remains an accurate description of the in-flight window and the ~45 ms mute.
- If the documentation note is taken, USER_MANUAL §3.5/§6 states that changing Oversampling or Phase changes the host-reported delay and may make the host re-sync, and recommends changing it while stopped.

<details><summary>Verification record</summary>

**Method.** Code read at e769f33:
- e769f33:src/PluginProcessor.cpp:37 and :896-915 (updateLatency calls setLatencySamples)
- e769f33:src/InternalState.h:259-265 (fired on any change to oversample, osPhase or offlineQuality)
- e769f33:src/dsp/AnabasisEngine.cpp:423-424 and :486-498 (latch at the duck bottom; bottomHoldSamples = delaySamples + osLatBase)

Docs read: KNOWN_ISSUES KI-004 (:204-290) and ADR-0004 items 1, 4 and 5.

Runtime on :139: reported latency moved 480 to 541 on selecting 4x Linear, matching ST-11.

Not observed:
- the ~45 ms mute, because the harness has no output capture
- any host re-sync, because the harness has no host PDC consumer

**Corrections to the candidate claim.** - The window in which the engine still runs the old delay lies on a path that is already fading to silence (the duck out-leg). No misaligned audio is audible, and the disagreement is bounded to 67 samples or fewer (KI-004).
- The ~45 ms mute is the documented, deliberate refill hold, and USER_MANUAL §6 and its FAQ describe the dip as intended.
- 'DAWs react with a re-sync or dropout' is host behaviour that was not observed here. Any PDC change triggers it, and ADR-0004 item 5 explicitly accepts PDC updates on Settings changes.
- The only user-facing gap: USER_MANUAL.md:360-361 says the change 'takes effect at a click-free moment' without mentioning that the host may briefly re-sync.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 1 · severity 2 · discoverability 2 · efficiency 1 · coherence 1 · change risk 5 · complexity 4 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DSP-007

**The base-rate hard clamp sits after the decimation filter, so raising Oversampling makes it clip more: at mid Loudness the clamp hard-clips 4-5x more samples, with ~45 dB more clip-error energy, at 2x-16x than at OS Off, silently**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Investigate further** | **P2** | low | confirmed | Technical robustness | The dBTP delivery ceiling is claimed but not enforced, and its warning cannot tell an over from normal operation | Phase 0 |

**Evidence**

- e769f33:src/dsp/CeilingClamp.h:6-10 — base rate, outside the oversampled region, downstream of the Post-EQ.
- e769f33:src/dsp/AnabasisEngine.cpp:1077-1078 — processSamplesDown after the limiter. :1188 — the clamp at base rate after the (optional) Post-EQ.
- e769f33:docs/architecture/design-decisions/ADR-0006-ceiling-guarantee.md:192-195 — 'The base-rate hard-clip backstop is itself a nonlinearity outside the oversampled region, which is acceptable only because the TP-driven gain is what normally keeps it from engaging.'
- e769f33:docs/architecture/design-decisions/ADR-0002-serial-signal-chain-and-clamp-placement.md:104-107 — clamp placement.
- e769f33:tests/dsp_tests.cpp:2315-2352 — testCeilingUnderOs acknowledges 'down-filter ringing after the limiter could overshoot' and asserts only the sample-peak bound (truePeakMode=false at :2334).
- e769f33:docs/user/USER_MANUAL.md:282 ('Higher = cleaner'), :442-443 ('raise Oversampling (4× and up) — at heavy clipping it audibly cleans the top end'), :500-501 (Force Max 'maximum quality on the bounce').
- verify-19 probe: rt/verify-19/probe/matrix-all.txt (m4 block) — clamp counts, pre-clamp max and clipErr per OS factor. .../probe/forcemax.txt — Force Max 1492 samples clamped, clipErr -42.3 dB. Source: .../probe/main.cpp and .../probe/AnabasisEngine_instr.cpp (3 added read-only lines before :1188).
- verify-19 R7: .../rt/verify-19/15-16-pair.png and 17-18-pair.png — at 4x, SP exactly -0.10 dBFS with TP 1.27-1.39 dBTP, including Transients 0.
- *Merged at triage from another verifier's note, quoted as written:* [DSP-007](findings-dsp-tech.md#dsp-007) already requires qualifying USER_MANUAL.md:442-443. Add that the same §8 step sends the user to 'Advanced' for Oversampling, which lives in the Settings overlay (USER_MANUAL.md:282), and names the row 'Offline quality' where the panel says 'Offline Render'. Fix the location and the row name in the same rewrite.

**Current behaviour.** The limiter and clipper run inside the oversampled region, and the region is then decimated. On limited or clipped material the decimation low-pass regrows peaks by up to 2-3.7 dB above the ceiling. The base-rate CeilingClamp hard-clips them.

At mid Loudness on hot material, this is 0.05-0.08 % of samples at 2x-16x versus 0.001-0.02 % at OS Off. The clip-error energy is about 40-50 dB higher.

No meter shows it: SP reads the ceiling either way.

**Problem.** The user action the manual recommends for quality at loud settings — raising Oversampling, or Force Max for the bounce — increases unoversampled hard clipping at the output stage instead of reducing it. The ADR's acceptance of a base-rate clip assumed it would rarely engage.

**Root cause.** The accepted architecture places the only post-decimation stage, the clamp, as a sample-level clip with no gain stage (ADR-0006 D2/D3 unimplemented). Nothing between decimation and the ceiling absorbs the filter's peak regrowth smoothly. The limiter's threshold is the ceiling itself and it acts before decimation, so every regrown peak reaches the clamp.

**User impact.** Mastering engineers who raise OS or render with Force Max for a cleaner top end on loud masters get more base-rate clipping on transients. It is likely audible as transient crunch or aliasing at -42 to -47 dB clip-error energy (not listened to), and it is invisible on every meter.

It also drives [DSP-001](findings-dsp-tech.md#dsp-001)'s 1-1.7 dB TP overs at those factors. *Scope:* Any OS factor ≥2x and every Force Max offline render, whenever the limiter is working. It scales with drive (at limGain 0 / music -6, 4x clamped only 8 samples). Both EQ positions, since the clamp is after the Post-EQ as well.

**Proposed improvement.** Deliver it through [DSP-001](findings-dsp-tech.md#dsp-001)'s clamp-stage gain. Give the clamp the gain half ADR-0006 D3 specifies:
- a smooth, short-lookahead base-rate gain driven by its own detector (the TP estimate in TP mode; the sample peak when TP is off);
- the hard clip remains the backstop and should then almost never engage at any OS factor.

Add a guard test on the hostile stimulus: at OS 2x-16x and Force Max, clamp engagements and clip-error energy are no higher than at OS Off.

Surface clamp activity to the user, e.g. an SP-row qualifier or a small CLAMP indicator counting backstop engagements since the last reset, so the output stage stops being invisible.

Interim: qualify USER_MANUAL :282, :442-443 and :500-501 until this lands.

**Alternatives considered.**

- *Move the clamp into the oversampled region.* — Rejected. It is a signal-order change, conflicts with ADR-0002 D3 and ADR-0006 D1 and option D, and it would put the clamp upstream of the Post-EQ.
- *A linear-phase or gentler decimation filter.* — Insufficient. Linear phase still regrew +1.66 dBFS pre-clamp at 4x in the probe. It also changes the reported latency (Latency gate).
- *A limiter margin whenever OS is on.* — Conflicts with ADR-0006 D5 (no independent limiter threshold) and costs loudness at every factor.
- *Disclosure only (the ADR already accepts the backstop).* — Rejected as the end state. The ADR's acceptance is explicitly conditional on the missing TP-driven gain.

**Decision: Investigate further · P2.** The measured behaviour inverts the manual's advice and falls outside the condition under which ADR-0006 accepted a base-rate clip. The remedy is the same Accepted mechanism as [DSP-001](findings-dsp-tech.md#dsp-001), so the two should be designed and reviewed together. The visibility indicator and the doc qualification are independent, low-risk steps.

*Calibration:* the verifier judged Proceed / P1; the final judgement is Investigate further / P2. Challenge: the standalone claim is not established. More clamp engagement at OS>=2x under heavy drive is confirmed, but net output quality was never measured (clipErr omits the clipper aliasing OS removes), the inversion is absent at mild drive (4x clamped 8 vs 20 samples at OS Off), 2x is 2.5x not 4-5x, and nothing was listened to. Evidence needed: whole-output folded-component energy OS Off vs 2x/4x/16x/Force Max at Loudness 50 % on a multitone or swept sine; the clamp-error spectrum at 4x/16x; the same on real programme; a loudness-matched blind listening comparison. Meanwhile track 'clamp engagements at OS>=2x and Force Max <= OS Off' as a [DSP-001](findings-dsp-tech.md#dsp-001) acceptance item and defer the CLAMP indicator (new audio->GUI scalar, ADR-0020 panel budget). Do not qualify the OS advice yet; but the merge-noted §8 errors (Oversampling lives in Settings, not Advanced, USER_MANUAL.md:282; the row is 'Offline Render', not 'Offline quality') are plain doc drift for the doc pass.

*Adversarial challenge:* evidence holds: no; priority justified: no (suggested P2); decision justified: no (suggested Investigate further). The unique, confirmed content is a mechanism: more clamp engagement at OS ≥ 2x under heavy drive. It shares [DSP-001](findings-dsp-tech.md#dsp-001)'s root cause and remedy, so it belongs as an acceptance criterion inside [DSP-001](findings-dsp-tech.md#dsp-001): clamp engagements at OS ≥ 2x and Force Max ≤ OS Off on the hostile stimulus.

The standalone P1 rests on user_impact 4 and severity 4 for 'likely audible' degradation and an inverted manual recommendation. Neither was measured, and the metric used cannot show it because it omits the clipper aliasing that OS removes.

Evidence needed to decide:
(a) Net output aliasing and inharmonic distortion at the Loudness-50% settings, OS Off vs 2x/4x/16x/Force Max, on a multi-tone or swept-sine stimulus. Folded (non-harmonic) component energy must be reported for the whole output, not only the clamp error.
(b) The spectrum of the clamp error signal at 4x/16x: in-band harmonics vs folded components.
(c) The same on real programme material rather than the synthetic noise-burst 'snap'.
(d) A loudness-matched blind listening comparison of OS Off vs 4x vs Force Max at hot settings.

If net distortion at OS ≥ 2x exceeds OS Off, restore P1 and qualify the manual. Otherwise track it only as a [DSP-001](findings-dsp-tech.md#dsp-001) sub-criterion, and defer the CLAMP indicator. *Proposal risks:* 1. The CLAMP indicator touches the threading model and the panel layout. It needs a new audio→GUI published scalar on THREAD_MODEL.md's Meters→GUI row (:35). ADR-0020's own amendment calls touching a published atomic 'a threading-model edit', and ARCHITECTURE_REVIEW_GATE:13 gates new cross-thread paths. The judge did not name this. It also consumes the ADR-0020 D6 panel budget (202 of 234 px) in both views.
2. The TP-off gain changes default renders. A sample-peak-driven clamp gain with TP off (ADR-0006 D3) changes TP-off output at OS Off wherever the clamp engages today (343 samples in m4). That contradicts [DSP-001](findings-dsp-tech.md#dsp-001)'s acceptance criterion 'TP off bit-identical at OS Off' and alters the default render for every session, not just OS ≥ 2x.
3. The same ADR-0003/ADR-0004 latency-neutrality constraint as [DSP-001](findings-dsp-tech.md#dsp-001) applies, because truePeakMode must not become latency-affecting.
4. Qualifying USER_MANUAL :282, :442-443 and :500-501 before a net-quality measurement risks publishing a claim ('OS makes it dirtier') the evidence does not support.

**Architecture gates.**

- Ceiling guarantee change: it modifies the invariant-4 clamp mechanism, shared with [DSP-001](findings-dsp-tech.md#dsp-001).
- Latency change (probable): a base-rate lookahead gain at the clamp.
- DSP Graph change: the clamp gains a detector and gain node (ADR-0006 D2/D3).
- Rejected alternatives would hit the Signal Flow gate and conflict with ADR-0002 D3 / ADR-0006 D1 (clamp relocation), or with ADR-0003 D3 plus Latency (decimation filter change).

**Dependencies.** [DSP-001](findings-dsp-tech.md#dsp-001)

**Acceptance criteria.**

- On the hostile stimulus at the Loudness-50% settings, the count of samples reaching the hard-clip backstop at OS 2x, 4x, 8x, 16x and Force Max is ≤ the OS-Off count. Clip-error energy re programme at those factors is ≤ the OS-Off figure (currently about -90 to -94 dB).
- Output sample peak ≤ ceiling holds at every factor (testCeilingUnderOs stays green).
- The UI exposes clamp backstop activity since the last reset in both views.
- USER_MANUAL :282, :442-443 and :500-501 match the measured behaviour.

<details><summary>Verification record</summary>

**Method.** Code read at e769f33:
- CeilingClamp.h:6-10.
- AnabasisEngine.cpp:1077-1078 (processSamplesDown) and :1188 (clamp).
- ADR-0002:104-107; ADR-0006:121-124 and :192-195.
- e769f33:tests/dsp_tests.cpp:2315-2352 (testCeilingUnderOs checks sample peak only).

Scratch probe: the real engine at e769f33 with read-only counters at the clamp input — samples over the ceiling, max pre-clamp level, and clip-error energy relative to programme. Runs over the OS matrix, both phase modes and Force Max, with the Loudness-50% macro values (limGain 7.8, clip drive 2.6, comp 1.75:1 at -10) and music at -6 dBFS.

The harness at 4x showed SP pinned at -0.10 dBFS with TP 1.27-1.39 dBTP, consistent with sample clipping. No listening test or aliasing spectrum was taken.

**Corrections to the candidate claim.** The mechanism is broader than claimed and, at OS ≥2x, different. At OS Off the clamp is fed by what the limiter lets through (the Transients slew, Punchy). At OS ≥2x the dominant feed is regrowth through the decimation filter after the limiter, independent of Transients: 4x with Transients 0 clamped 1293 samples, Transients 50 clamped 1305.

The clamp works harder with oversampling ON (probe, Loudness-50% values):

| OS | Samples clamped | Clip-error energy re programme |
|---|---|---|
| Off | 0.0014 % | -94.3 dB |
| 2x | 0.045 % | -46.7 dB |
| 4x | 0.068 % | -46.3 dB |
| 16x | 0.078 % | -42.3 dB |

The TP-off OS-Off figures are 0.018 % and -90 dB. Pre-clamp overshoot reaches +2.0 dBFS (4x) and +3.6 dBFS (16x) against a -0.1 ceiling.

ADR-0006 Consequences (:192-195) accept the base-rate backstop 'only because the TP-driven gain is what normally keeps it from engaging'. That gain does not exist ([DSP-001](findings-dsp-tech.md#dsp-001)), so the acceptance condition is unmet. Audibility is inferred from magnitude, not listened to.

</details>

<sub>Verifier scores (1-5): impact 4 · frequency 3 · severity 4 · discoverability 5 · efficiency 3 · coherence 4 · change risk 4 · complexity 4 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DSP-008

**At the default 100 % link the adaptive stereo-link trim works in one direction only: sparse programme loosens the link to about 0.89 (0.80 after a denser Learn), and dense programme cannot tighten it**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Defer** | **P2** | medium | confirmed | Technical robustness | The adaptive engine changes the audio from state the user cannot see, keep or reset | — |

**Evidence**

- e769f33:src/dsp/AnabasisEngine.cpp:632 — `pApplied.stereoLink = juce::jlimit (0.0f, 1.0f, p.stereoLink + t.stereoLink);` and :652 limiter.setStereoLink
- e769f33:src/dsp/AdaptiveEngine.h:425-426 — target (onsetRate − refOnsetRate)·0.03 clamped ±0.2; :93 kDefaultRefOnset = 4; :454 deadband 0.01
- e769f33:src/PluginParameters.cpp:375 — stereoLink default 100 %; e769f33:src/dsp/LookaheadLimiter.h:25-26, 258-260 — level = link·max + (1−link)·own
- e769f33:docs/architecture/design-decisions/ADR-0013-release-trim-reaches-auto-poles.md:58-59 — 'all four adaptive behaviours are audible at defaults' (true for link only in the loosening direction)
- e769f33:docs/user/USER_MANUAL.md:300-303 ('stereo linking … follow[s] the programme'), :317-319 (Learn for sparse material and spoken word)
- e769f33:docs/policies/MODE_AND_ADAPTATION_POLICY.md:270 — 'Trim mapping constants are ⊕ drafts, tuned by ear'; e769f33:docs/HANDOVER.md:195-197 — fine review owes the listening pass over every ⊕ trim mapping
- probe rt/verify-18/probe/probe.out — pad: link trim −0.1096 at 10 s, −0.1100 by 20 s (applied 0.89); clicks 1/2/4/8/12 per s → −0.061/−0.040/+0.0003/+0.120/+0.190 (applied at 100 %: 0.939/0.960/1.0/1.0/1.0; at 60 %: 0.539/0.560/0.600/0.720/0.790)
- probe: 'link 0.88 vs 1.00 (asym test prog)' — R min GR −9.24 vs −9.79 dB, balance 2.049 vs 2.216 dB, diffRel −34.9 dB
- runtime rt/verify-18/a-before-sr.xml — harness music latched stereoLink −0.0218
- *Merged at triage from another verifier's note, quoted as written:* Widen [DSP-008](findings-dsp-tech.md#dsp-008) from the link trim to the other ⊕ trim mapping it already defers to the listening pass. On a bass-heavy sustained pad the SC HPF trim hits its +30 Hz rail within 10 s: probe.out shows hpf +29.001 Hz at t=10 s and at every later sample (rt/verify-18/probe/probe.out). The comp detector's HPF then runs at about 49 Hz while the SC HPF slider reads 20 Hz. Re-checked e769f33:src/dsp/AdaptiveEngine.h:427-440, which flags exactly this: 6 Hz/dB against a -6 dB reference saturates once the programme is about 5 dB darker than the reference. The first-order 800 Hz split biases tiltDb low. A pinned trim moves the shared SC HPF to 50 Hz in BOTH detectors, so bass transients reach the CeilingClamp. testTrimBounds and testAdaptationConvergesAndHolds cannot tell convergence from a rail. Add acceptance items: the listening pass measures how often scHpf sits on the rail on representative programme, and compares clamp engagement with the trim pinned against the trim at zero. The missing readout stays in [VIS-011](findings-visualisation.md#vis-011), which already cites the +29 Hz probe.

**Current behaviour.** At the default Stereo Link of 100 %, programme with fewer than about 4 onsets/s (sustained pads, spoken word, classical, sparse acoustic material) slowly loosens the limiter link, by up to 11 points (20 after a Learn on denser material). Programme with more onsets computes a positive trim that the [0,1] clamp discards. The Advanced knob keeps showing 100 %.

**Problem.** At the default, the documented 'linking follows the programme' works only as 'linking can only loosen'. The loosening is invisible ([VIS-011](findings-visualisation.md#vis-011)) and overrides a value the user may have set to 100 % deliberately to lock the image. It also narrows ADR-0013's 'all four adaptive behaviours are audible at defaults' to a one-sided behaviour for link. Whether loosening on sparse programme is musically wanted is undocumented and untested by ear.

**Root cause.** The link trim is an additive delta (±0.2) around the user value, clamped to [0,1], and the default sits on the upper rail. The draft mapping direction (sparse → less link) has no recorded rationale and is a ⊕ constant awaiting the listening pass.

**User impact.** Small but real and hidden: on sparse, stereo-asymmetric programme the image can shift by about 0.1–0.2 dB during limiting, and the channels' gain reduction can diverge by about 0.5 dB, while the UI says 100 % linked. Users who follow the manual and Learn on the material largely neutralise it. *Scope:* Sessions on sparse or low-onset programme with the default (or any near-100 %) link. The upward half is inert only when the user link is at or near 100 %.

**Proposed improvement.** At the ⊕ listening pass, decide the link trim's semantics at the user's extremes. Audition the status quo against 'rail-respecting' scaling (trim weight fading to 0 as the user link approaches 0 % or 100 %, so that 100 % is a true lock) on sparse, asymmetric programme (classical, off-centre spoken word, sparse acoustic) at link 0.89 and 0.80. If the status quo stays, have USER_MANUAL §4 state that at 100 % the link trim can only loosen (≤ 11 points, ≤ 20 after Learn), and rely on [VIS-011](findings-visualisation.md#vis-011) to show the effective link. If rail-respecting scaling is chosen, amend ADR-0013's consequence, because the link trim would then be inert at defaults.

**Alternatives considered.**

- *Status quo, documented and made visible ([VIS-011](findings-visualisation.md#vis-011))* — Lowest risk. It keeps an audible link behaviour at defaults and needs only doc and UI honesty.
- *Rail-respecting scaling: weight the trim by min(1, p/0.2, (1−p)/0.2)* — Makes 100 % and 0 % explicit locks. The link trim becomes inert at the factory default, which contradicts ADR-0013's consequence: owner call and ADR amendment.
- *Move the stereoLink default off the rail (e.g. 90 %) so both directions act* — Changes the factory sound and a parameter default: a Parameter Registry change under the gate, with compatibility review. Hard to justify for this effect size.
- *Allow only positive (tightening) trims* — Also inert at the default; removes the one audible direction without evidence it is wrong.

**Decision: Defer · P2.** The mechanism is confirmed, but harm depends on a musical judgement the project has explicitly assigned to the ⊕ listening pass (MODE policy line 270; HANDOVER fine-review list), and the effect is bounded and small. Until then the actionable part is visibility ([VIS-011](findings-visualisation.md#vis-011)) and an honest manual sentence. The deferral waits for the P6/fine-review listening pass on representative sparse programme.

**Architecture gates.**

- ADR-0013 (Accepted) consequence 'all four adaptive behaviours are audible at defaults': rail-respecting scaling or positive-only trims would make the link trim inert at defaults and need an ADR amendment
- MODE_AND_ADAPTATION_POLICY Enforcement if the trim's authority or semantics change (beyond ⊕ constant tuning)
- Parameter Registry change (ARCHITECTURE_REVIEW_GATE) if the stereoLink default is moved off 100 %

**Dependencies.** [VIS-011](findings-visualisation.md#vis-011) (makes the effective link visible); The ⊕ listening pass / post-v0.1.0 fine review (HANDOVER.md:195-197)

**Acceptance criteria.**

- The listening pass records a decision on link-trim behaviour at 100 % and 0 %, with measured L/R GR divergence and balance shift on at least three sparse, asymmetric programmes
- If rail-respecting scaling is chosen: with stereoLink = 100 %, a 60 s onset-free programme leaves the applied limiter link at exactly 1.0 (engine test), testTrimBounds passes, and ADR-0013's consequence is amended
- If the status quo is kept: USER_MANUAL §4 states the one-sided behaviour and its bound, and the [VIS-011](findings-visualisation.md#vis-011) overlay shows the effective link (e.g. '100 % · 89 %')
- The invariant-7 null (testNullWithDefaults) stays bit-exact

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/dsp/AnabasisEngine.cpp:632 and :652, AdaptiveEngine.h:93, :424-426, :454, the limiter's link law (LookaheadLimiter.h:25-26, :258-260), the parameter default (PluginParameters.cpp:375, EngineParameters.h:61), ADR-0013's consequences, USER_MANUAL §4 and MODE policy line 270. Engine probe at e769f33: a 60 s sustained pad, plus click trains at 1/2/4/8/12 per second, reading publishedTrimLink and the published onset rate. Measured the audible effect of link 0.94 and 0.88 vs 1.00 on an L/R-asymmetric drum programme at +10 dB limiter gain. Harness music latch via savexml.

**Corrections to the candidate claim.** 'About 0.88': the steady state on onset-free programme is 0.89 (target −0.12 at the default reference of 4 onsets/s, minus the 0.01 deadband). It reaches the −0.2 bound (0.80) only when a Learn has raised the reference. The harness's 120 BPM music latched −0.022 (link 0.978). 'Dense cannot raise it' holds only at the top of the range: clicks at 8–12/s produced +0.12 to +0.19, which is inert at 100 % but gives 0.72–0.79 at a 60 % preset. This is inherent to a [0,1] range, not a defect in itself. Measured effect of 0.88 vs 1.00 on asymmetric programme: R-channel peak GR 0.55 dB lower, L−R level difference 2.22 → 2.05 dB (≈0.17 dB image shift), difference signal −35 dB relative. The effect is small but not zero. The manual already points sparse material and spoken word to Learn (USER_MANUAL.md:317-319), which re-centres the reference and pulls the trim toward 0. No listening data exists, so whether loosening is harmful is unproven.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 3 · severity 2 · discoverability 5 · efficiency 1 · coherence 3 · change risk 3 · complexity 2 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DSP-009

**Dither quantises the processed path to the 16- or 24-bit grid even in a float session, and the ~10 ms bypass crossfade is off-grid because the dry leg is undithered. Both are documented design choices, not defects.**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Preserve** | **none** | high | confirmed | Documentation | The dBTP delivery ceiling is claimed but not enforced, and its warning cannot tell an over from normal operation | — |

**Evidence**

- e769f33:src/dsp/AnabasisEngine.cpp:1081-1085 — ditherOn, the q grid, and shaping state cleared when not in use
- e769f33:src/dsp/AnabasisEngine.cpp:1201-1214 — TPDF quantiser after the clamp and the duck
- e769f33:src/dsp/AnabasisEngine.cpp:1260-1278 — bypass crossfade downstream of dither, accepted in favour of the bit-exact bypass null
- e769f33:docs/policies/DSP_POLICY.md:266-287 — invariant 12 scope: three legs downstream of the quantiser, accepted
- e769f33:docs/policies/DSP_POLICY.md:99-107 — invariant 4: dither may exceed the ceiling by ~1.5 LSB
- e769f33:docs/policies/DSP_POLICY.md:312 — testDitherModes: 16-bit lands on the 2^-15 grid
- e769f33:src/gui/LoudnessMeterView.cpp:259-265 — SP warning slack of 0.005 dB absorbs the dither LSBs
- e769f33:docs/user/USER_MANUAL.md:215-216 — 'Dither (Off / 16-bit / 24-bit TPDF ...) for final exports'
- e769f33:src/gui/PluginEditor.cpp:89 — tooltip 'Bit-depth dither for the final export'
- e769f33:docs/architecture/design-decisions/ADR-0010-parameter-surface.md:133-136 — dither is non-automatable as a conservative v1 freeze and is carried by presets

**Current behaviour.** When Dither is 16-bit or 24-bit, the processed path is TPDF-quantised to 2^-15 or 2^-23 after the ceiling clamp, whatever the host's sample format.

The bypass leg carries the undithered, delay-aligned dry signal, so the steady states are on the grid but the ~10 ms bypass ramp is a mix of dithered and undithered samples.

Dither is Off by default and is a true no-op when Off.

**Problem.** No product defect is shown.

A user who leaves 16-bit dither on while exporting at 24-bit or float, or who places processing after Anabasis, gets a needlessly reduced or wasted dither. That is a usage error common to every maximizer with built-in dither.

The manual gives only 'for final exports' as guidance.

**Root cause.** By design, dither is a user-selected final-export stage inside the plugin (ADR-0002, DSP_POLICY invariant 12). A plugin cannot detect the export format. The bypass-leg exception exists to keep invariant 7's bit-exact bypass null.

**User impact.** Negligible for correct use. There is a potential resolution or double-dither mistake if the user misapplies dither, which the documentation could prevent. *Scope:* The dither stage (Advanced utility row: Dither and SHAPE) and USER_MANUAL §3.3. Factory preset browsing resetting dither to Off is tracked separately as [STATE-002](findings-state-model.md#state-002).

**Proposed improvement.** Keep the DSP as it is.

Optionally add a short placement note to USER_MANUAL §3.3:
- enable Dither only for the final fixed-point export, at that bit depth
- keep Anabasis last in the chain, with no fader moves or plugins after it
- turn the host's own dither off
- leave Dither Off for float or 24-bit masters that feed further processing

**Alternatives considered.**

- *Auto-disable dither when the host renders or runs in float* — Not feasible. No plugin API reliably exposes the export format.
- *Move dither out of presets (preset-excluded tier or ANABASIS_INTERNAL)* — This would also fix [STATE-002](findings-state-model.md#state-002), but it changes ADR-0010's frozen exclusion tiers and the serialization surface (PARAMETER_COMPATIBILITY rule 6). It belongs in [STATE-002](findings-state-model.md#state-002)'s decision, not here.
- *Move the bypass crossfade upstream of dither, or dither the dry leg during the ramp* — Rejected. It breaks invariant 7's bit-exact bypass null and changes signal order (ADR-0002), all to put 10 ms of an audition toggle on the grid.

**Decision: Preserve · none.** Every element of the claim is intended behaviour recorded in an Accepted ADR (ADR-0002) and in DSP_POLICY invariants 4, 7 and 12, and is pinned by testDitherModes and testNullWithDefaults.

Changing it would cross the signal-order or serialization gates for no demonstrated user harm.

The small documentation note is the only worthwhile action.

**Architecture gates.**

- serialization schema change / ADR-0010 exclusion tiers — alternative 2 only
- DSP signal-order change / Accepted ADR-0002 (clamp always last before dither) and DSP_POLICY invariants 7 and 12 — alternative 3 only

**Dependencies.** [STATE-002](findings-state-model.md#state-002)

**Acceptance criteria.**

- testDitherModes and testNullWithDefaults remain green; Off is still a bit-exact no-op, and 16-bit still lands on the 2^-15 grid.
- DSP_POLICY invariant 12's scope enumeration stays accurate.
- If the documentation note is taken, USER_MANUAL §3.3 states when to enable dither, that nothing should follow it, and that host dither should be off.

<details><summary>Verification record</summary>

**Method.** Code read at e769f33:src/dsp/AnabasisEngine.cpp:1081-1085 and :1201-1214 (the quantiser follows the clamp and the duck) and :1260-1278 (bypass crossfade downstream of dither).

Policy and docs read: DSP_POLICY invariants 4 and 12 (e769f33:docs/policies/DSP_POLICY.md:99-107, 266-287), the test table (:312, testDitherModes pins the 2^-15 grid), LoudnessMeterView.cpp:250-265 (SP slack), USER_MANUAL.md:215-216, and ADR-0010 on the dither tier.

Runtime: not reproduced, because the harness cannot capture output samples. The claim rests on code and on the policy-pinned test.

**Corrections to the candidate claim.** - All three behaviours are deliberate and documented. ADR-0002 fixes the clamp-then-dither order. DSP_POLICY invariant 12 records the bypass-crossfade and monitor legs downstream of the quantiser, and invariant 4 records the ~1.5 LSB above-ceiling allowance.
- Quantising to 2^-15 when 16-bit is selected is the control's stated purpose: 'for final exports' (USER_MANUAL.md:216; tooltip at PluginEditor.cpp:89). The plugin has no reliable way to know the export format, and it happens even in a float session because the user selected it.
- The off-grid ramp is bounded to the ~10 ms bypass crossfade and reaches a render only when bypass is automated.
- There is no evidence that a user was harmed. The only real gap is that the manual gives no placement guidance: dither last, nothing after it, host dither off, depth matching the export.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 1 · severity 2 · discoverability 2 · efficiency 1 · coherence 1 · change risk 5 · complexity 3 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DSP-010

**Lookahead is non-automatable on a rationale (pitch or comb artefacts from dragging the audio tap) that the implementation does not have, because only the detector tap moves**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | confirmed | Documentation | User and design documents contradict the shipped product | Phase 5 |

**Evidence**

- e769f33:src/dsp/AnabasisEngine.cpp:1001-1021 (detector tap slides with the smoothed W)
- e769f33:src/dsp/AnabasisEngine.cpp:1060-1072 (audio readPos = writePosOs - delayOs, constant)
- e769f33:src/dsp/AnabasisEngine.cpp:561-575 (windowSamples smoother)
- e769f33:docs/architecture/design-decisions/ADR-0004-latency-contract-constant-lookahead-allowance.md:107-116 (items 2-3: audio path invariant)
- e769f33:docs/architecture/design-decisions/ADR-0004-latency-contract-constant-lookahead-allowance.md:144-149 (item 6: pitch/comb rationale)
- e769f33:docs/architecture/design-decisions/ADR-0010-parameter-surface.md:132-135 and e769f33:docs/architecture/design-decisions/ADR-0010-parameter-surface.md:151-152
- e769f33:docs/DESIGN.md:407-413 and e769f33:docs/DESIGN.md:519-520
- e769f33:docs/policies/MODE_AND_ADAPTATION_POLICY.md:62-66
- e769f33:docs/architecture/PARAMETER_REGISTRY.md:155
- e769f33:src/PluginParameters.cpp:359-366
- e769f33:tests/dsp_tests.cpp:372-390 (unsmoothed W steps the detector tap without stepping the output)
- e769f33:docs/user/USER_MANUAL.md:506-512

**Current behaviour.** Lookahead is flagged non-automatable, with an advisory flag. Moving it glides only the limiter's detection window and detector tap over about 20 ms. The audio path delay stays at the constant 10 ms allowance, so no audio sample is repeated or skipped. Six documents plus a source comment justify the flag by audio-tap pitch or comb artefacts.

**Problem.** The stated reason is false. A maintainer or reviewer weighing a change, for example a user request to automate lookahead or the adaptive-engine bar in MODE inv 4, reasons from a mechanism that does not exist. ADR-0004 contradicts itself.

**Root cause.** Item 6 of ADR-0004, and the texts copied from it, kept an earlier variable-read-offset rationale after the same ADR fixed the audio delay at the constant allowance.

**User impact.** Direct user impact is minimal. Lookahead is a set-and-leave control for most mastering work, and the flag is advisory anyway (REAPER can still write it; the result is benign). The cost is in governance and future decisions. *Scope:* Documentation and rationale for the lookahead parameter: ADR-0004, ADR-0010, DESIGN, MODE_AND_ADAPTATION_POLICY inv 4 (as enacted through ADR-0005), PARAMETER_REGISTRY and the PluginParameters.cpp comment. No DSP or behaviour change.

**Proposed improvement.** Rewrite the rationale in all six places to the true one. A lookahead move glides the detector window: while it moves, detector samples repeat or skip (a coverage error the clamp bounds), and the limiter's attack lead changes. Lookahead is kept non-automatable as a conservative set-and-leave freeze for v1. Add a dated correction note to ADR-0004 reconciling item 6 with items 2-3, following ADR_POLICY. Keep the flag. Defer any loosening (kVersion bump + ADR) until there is user demand. Re-ground or explicitly re-decide MODE inv 4's bar on adaptive lookahead moves, since its current ground is the same false premise.

**Alternatives considered.**

- *Make lookahead automatable now* — Technically benign, but it is a Parameter Registry change needing a kVersion bump and an ADR, conflicts with ADR-0004 D6, and no demand is evidenced. Defer.
- *Leave docs as they are* — Rejected. An Accepted ADR contradicts itself, and the false mechanism is quoted as authority in policy text.
- *Correct the rationale only (chosen)* — Zero behaviour change. Removes the internal contradiction.

**Decision: Modify · P3.** The evidence is conclusive: the code, a test comment and the ADR's own items 2-3 all show that the audio tap never moves. The smaller change, correcting the rationale while keeping the flag, removes the harm (misleading governance text) without touching the parameter surface. Loosening the flag waits for evidence of user demand.

**Architecture gates.**

- Accepted ADR-0004 item 6 and ADR-0010 text — a rationale-only correction needs an amendment note per ADR_POLICY (no decision change)
- MODE_AND_ADAPTATION_POLICY inv 4 text is enacted by ADR-0005's prescribed block — editing it needs the ADR instrument (ADR_POLICY rule 5)
- Parameter Registry change (automatable flag, kVersion bump) and conflict with ADR-0004 D6 — only for the deferred loosening alternative

**Dependencies.** [MODEL-006](findings-state-model.md#model-006) (same automation FAQ and registry rationale section)

**Acceptance criteria.**

- ADR-0004, ADR-0010, DESIGN §3.3 and footnote ³, MODE_AND_ADAPTATION_POLICY inv 4, PARAMETER_REGISTRY and the PluginParameters.cpp lookahead comment give the same reason: a detector-window glide and attack-lead change, kept as a v1 set-and-leave freeze. None of them mentions pitch or comb artefacts or an audio read-offset move.
- ADR-0004 carries a dated correction note reconciling item 6 with items 2-3.
- tests/fixtures/parameter_registry.snapshot is unchanged: lookahead stays non-automatable unless a new ADR decides otherwise.
- MODE_AND_ADAPTATION_POLICY inv 4 states a ground for barring adaptive lookahead moves that holds against AnabasisEngine.cpp:1060-1072, or the bar is explicitly re-decided.

<details><summary>Verification record</summary>

**Method.** Code read at AnabasisEngine.cpp:556-575 (a windowSamples smoother glides W), 999-1033 (detector tap detPos = writePos - (delayOs - wOs)) and 1060-1072 (audio readPos = writePos - delayOs, fixed). Compared ADR-0004:107-116 (items 2-3: audio delay fixed at 10 ms, 'no audio sample is ever skipped or repeated when lookahead moves') against ADR-0004:144-149 (item 6: 'drags the tap ... pitch/comb artefacts'). Also read ADR-0010:132-135 and 150-153, DESIGN.md:407-413 and 519-520, MODE_AND_ADAPTATION_POLICY.md:62-66 (enacted through ADR-0005:203-208), PARAMETER_REGISTRY.md:153-157, PluginParameters.cpp:359-366, and the test comment at dsp_tests.cpp:372-390, which says an unsmoothed W 'steps the detector tap without stepping the output'. No runtime reproduction was needed; E12's 20 -> 10 ms clamp is range behaviour, not automation.

**Corrections to the candidate claim.** The rationale is contradicted by ADR-0004's own items 2-3, not only by the code. 'Drags the tap' is literally true of the detector tap: it slides during the 20 ms glide, repeating or skipping detector samples, a bounded coverage error with the ceiling clamp downstream. What does not exist is an audio-tap drag, and therefore no pitch or comb artefact. The drift is uneven across the locations: PARAMETER_REGISTRY.md:155 already says 'drags the detector tap', while ADR-0004, ADR-0010, DESIGN, MODE policy inv 4 and PluginParameters.cpp still claim a live audio read offset. No evidence was found that users need per-section lookahead automation.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 1 · severity 1 · discoverability 2 · efficiency 1 · coherence 3 · change risk 2 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

## TECH — Technical robustness

### TECH-001

**KI-012 is still open: the owner reports that the Linux editor accepts no mouse input on a real session. The one measured mechanism is closed in current builds, but nothing here can reproduce or clear the report**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Investigate further** | **P1** | low | partially-confirmed | Technical robustness | Verification stops at the headless boundary, and gesture/restore threading can crash the host | Phase 0 |

**Evidence**

- e769f33:docs/KNOWN_ISSUES.md:772-776: KI-012 is Severity High, Status 'Reported — not reproduced', and host and desktop are not recorded; :839-848: the data that would settle it (host, WM or compositor, CI or local build, whether meters move)
- e769f33:docs/KNOWN_ISSUES.md:850-911: the 9.0.1 addendum. Three candidate mechanisms are closed (libXi.so dlopen, vblank ms-vs-Hz timer restart, message-queue starvation) and the entry stays OPEN
- e769f33:CHANGELOG.md:1094-1103: 'This does not close the outstanding report'
- e769f33:docs/architecture/design-decisions/ADR-0028-juce-901-pin.md:123: 'This ADR does not claim KI-012 is fixed'
- e769f33:worklogs/2026-08-16-juce-901-bump.md:108-118: the decisive hide-the-symlink XTEST experiment was 'NOT done, deliberately'
- e769f33:docs/architecture/design-decisions/ADR-0025-regression-test-exception.md:62-66: the GUI-automation harness is deferred; the KI-012 probe lives only in worklogs/
- JUCE 9.0.1 (fetched) juce_gui_basics/native/juce_XSymbols_linux.h:660: DynamicLibrary xinputLib { "libXi.so.6" }
- Runtime V13-5, library trace: rt/verify-13/lddebug.log (dlopen libXi.so.6)
- Runtime V13-5, namespace probes: rt/verify-13/ns/dlprobe.txt and rt/verify-13/ns/libs.txt
- Runtime V13-5, input with libXi.so hidden: session capture `rt/verify-13/14-ns-before.png`, session capture `rt/verify-13/15-ns-after.png`, session capture `rt/verify-13/15-crop.png` (drag and click landed)

**Current behaviour.** On Xvfb with synthetic XTEST input, with or without twm, and now also on an end-user-like library set without libXi.so, the editor built at JUCE 9.0.1 receives clicks, drags and hover. The owner's original report (no control responds, hover gives no reaction) has not been re-tested on the owner's machine with any 9.0.1 build.

**Problem.** A report that the plugin is completely inoperable on some Linux setups remains open, with its environment unknown. It cannot be confirmed or closed from here, and it undermines confidence in every Linux UI result, including this audit's.

**Root cause.** Not established. The measured precondition of JUCE 9.0.0's libXi.so failure is real, and 9.0.1 builds no longer depend on it (verified here). The remaining explanations are environment-specific: window manager or compositor, XWayland, host embedding, or a 9.0.0-era build on the reporter's machine.

**User impact.** If the report still holds on current builds, Linux users on the affected setup cannot operate the plugin at all (VST3 and Standalone). If it was the libXi mechanism, 0.1.5 and later already fixed it, and the open entry overstates current risk. *Scope:* Linux only. The affected fraction of Linux setups is unknown. The formats in the report are unrecorded.

**Proposed improvement.** Obtain the discriminating data from the owner on a current build (the e769f33 CI artifact, or the last release): 1) re-test on the same machine and say whether it still reproduces; 2) host and version, desktop environment and window manager, compositor on or off, X11 or XWayland, CI or local build; 3) whether meters move while audio plays; 4) `ldconfig -p | grep libXi`; 5) if it still reproduces, the output of `xinput test-xi2 --root` while clicking on the editor, and the same test with the Standalone. Record the namespace experiment above in KI-012 as 'on 9.0.1 the library precondition no longer matters'. Close KI-012 only on the owner's confirmation.

**Alternatives considered.**

- *Close KI-012 now because the 9.0.1 fixes cover the candidates* — Rejected. There is no reporter confirmation, and ADR-0028 and KI-012 explicitly refuse this.
- *Build an XTEST GUI harness into CI (ADR-0025 option 4)* — Deferred. It would guard against regressions, but it runs in the same environment class and would not reach the owner's configuration.
- *Do nothing until the report recurs* — Rejected. A High-severity open entry with a cheap, decisive re-test available should be settled before the fine review signs off Linux.

**Decision: Investigate further · P1.** The evidence here cannot reproduce or refute the owner's report. The needed evidence is a re-test on the owner's real desktop with a 9.0.1 build, plus the environment data listed. The one mechanism that was measured is now shown closed in shipped binaries, which lowers but does not remove the risk.

*Adversarial challenge:* evidence holds: yes; priority justified: yes (suggested P1); decision justified: yes (suggested Investigate further). The evidence stands: the entry is open and the anchors match. The runtime probe is genuine and was run on a verified 9.0.1 build. The rubric supports P1. If the report holds on current builds, affected users cannot operate the plugin at all, which is P0-class impact. Reach and current-build recurrence are unknown, and confidence is low, so P1 plus an investigation is the right placement. The sibling point makes recurrence on 9.0.1 more likely, not less, which argues against downgrading. 'Investigate further' is the correct decision, because nothing here can reproduce or clear the owner's setup. The requested evidence should be tightened as listed in the risks. The claim that KI-012 'undermines confidence in every Linux UI result, including this audit's' is rhetorical. The audit's Xvfb results are what they are, and KI-012 is specific to one environment. *Proposal risks:* There is no code risk, and the gate flags are correct: patching JUCE or moving the pin would conflict with ADR-0028 and be a build-system change. The proposal has three weaknesses. (a) It does not ask for the most discriminating comparison now available: Anamorph and Anabasis are both on JUCE 9.0.1, so an A/B on the same machine, host, format and window is cheap. The owner should also record which Anamorph build, host and format the original 'Anamorph works' observation used, and whether it was the same machine. (b) `xinput test-xi2 --root` shows only that the server delivers device events, which is already implied if other apps work. It cannot show whether the plugin window selected XI2 or core events, or whether another window intercepts the click. `xwininfo` clicked on the editor, plus `xwininfo -root -tree`, would show which X window actually sits under the pointer (a host or compositor overlay, or an XWayland surface). (c) The environment list leaves out screen resolution and scale factor, and host CPU load while the editor is open. On Linux the editor paints in software (the GL attach is gated to Mac and Windows), so message-thread paint cost grows with the backing store. The audit's 1600x1100 Xvfb framebuffer does not exercise that, and the message-starvation branch KI-012 names depends on it. The 'record the namespace experiment in KI-012' step is harmless but low-value, and it must say the 9.0.0 half was not run. Otherwise it repeats the overstatement above.

**Architecture gates.**

- None for the investigation
- Any fix that moves the JUCE pin or patches JUCE would conflict with the Accepted ADR-0028 (JUCE 9.0.1 pin), need a new ADR and be a build-system change

**Dependencies.** [TEST-002](findings-doc-test.md#test-002) (the same real-desktop Level-5 session can collect this)

**Acceptance criteria.**

- KI-012 records the owner's re-test on a named build (version and commit) with host and version, desktop and window manager, compositor state, X11 or XWayland, meters moving yes or no, and the libXi ldconfig output
- KI-012 status becomes either Closed (not reproducible on build X) or Confirmed, with a reproduction recipe
- KI-012 gains the 2026-09-26 note that the 9.0.1 build accepts input with the unversioned libXi.so absent

<details><summary>Verification record</summary>

**Method.** Read e769f33:docs/KNOWN_ISSUES.md:772-911 (status 'Reported — not reproduced', plus the 2026-08-16 addendum), CHANGELOG.md:1094-1103, ADR-0028:107-125, the worklog 2026-08-16-juce-901-bump.md:89-118 and ADR-0025:62-66. New checks on my display :143: (1) `strings` on the built Standalone lists only libXi.so.6 among the X11 libraries, and LD_DEBUG=files shows `dlopen libXi.so.6` at runtime. (2) I ran the experiment the JUCE-bump worklog left undone, for the 9.0.1 build only. In a private mount namespace (unshare -m; the system was untouched) with the unversioned libXi.so hidden (probe: libXi.so FAIL, libXi.so.6 OPENED — an end-user library set), the Standalone accepted a stepped XTEST knob drag (Loudness moved, label 'Default *') and a stepped click (ADV: window 942x778 to 942x880).

**Corrections to the candidate claim.** The CHANGELOG anchor is 1094-1103, not 1097-1106. The report (2026-08-10) predates the JUCE 9.0.1 bump (2026-08-16), and nothing on record shows it recurring on a 9.0.1 build. 'Every UI improvement is moot on that platform' holds only if the report still reproduces on current builds. My checks run in the same environment class as the original harness (Xvfb, XTEST, no WM, no compositor), so they exclude one mechanism on current builds but cannot reproduce or clear the owner's setup.

</details>

<sub>Verifier scores (1-5): impact 5 · frequency 2 · severity 5 · discoverability 3 · efficiency 4 · coherence 2 · change risk 1 · complexity 2 · evidence 2</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### TECH-002

**Editor paint() reads the non-atomic detach-mask StringArray, a cross-thread read outside THREADING_POLICY's Message→Painting row and unrecorded in THREAD_MODEL; at the JUCE pin only an off-thread state restore can race it**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | partially-confirmed | Technical robustness | Verification stops at the headless boundary, and gesture/restore threading can crash the host | Phase 4 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:1359-1363 — `proc.detachMask().contains (managed_params::ids[i])` inside paint() (line 1361)
- e769f33:src/PluginProcessor.h:187,468 — `const juce::StringArray& detachMask()` over plain `juce::StringArray liveDetachMask`
- e769f33:src/gui/PluginEditor.cpp:1020-1022 — `glContext.attachTo (*this)` under JUCE_MAC || JUCE_WINDOWS
- e769f33:docs/policies/THREADING_POLICY.md:26,28 — Message→Painting row lists three sites; any other path is a new cross-thread path (gate)
- e769f33:docs/policies/THREADING_POLICY.md:49-50 — 'No direct access to non-atomic shared state across threads'
- e769f33:docs/architecture/THREAD_MODEL.md:88-91 — 'at P1 the only such read is getLatencySamples() in paint()', with no mention of the mask read
- build/_deps/juce-src/modules/juce_opengl/opengl/juce_OpenGLContext.cpp:405,547 (JUCE 9.0.1) — try-lock emplaced before paintComponent, which asserts the MM lock
- e769f33:docs/architecture/THREAD_MODEL.md:214-229 — records the MM lock and refuses to rely on it
- e769f33:src/PluginProcessor.cpp:1957,1549-1554 — setStateInformation (not promised on the message thread, :1833-1837) writes liveDetachMask
- e769f33:docs/KNOWN_ISSUES.md:46-91 — KI-003 lists liveDetachMask among the restore-thread exposures, pending a thread-model decision

**Current behaviour.** In Advanced, every paint of the editor calls StringArray::contains on the processor's live detach mask nine times. On macOS/Windows this runs on the GL render thread under JUCE's MessageManager try-lock, and on Linux on the message thread. The mask is written on the message thread by the drain, reset-to-macro, A/B and undo. It is also written by setStateInformation, which may arrive on a host thread.

**Problem.** This is a cross-thread read of a non-atomic, heap-backed container that the threading policy does not list. The policy says such a path must go through the Architecture Review Gate, and the tree explicitly refuses to count the vendored JUCE lock as its safety argument. The one realistic race (an off-thread restore while the editor is open) can in principle read a reallocating array, whatever the platform.

**Root cause.** The badge paint predates the painting-boundary audit that produced ADR-0027, ADR-0038 and ADR-0039, and it was never added to the Message→Painting row. The mask has a single representation, a StringArray, which is used for serialization, the mapper and display alike.

**User impact.** No user-visible failure is reachable through normal interaction at the JUCE 9.0.1 pin. The residual risk is a crash or mis-drawn badges when a host restores state off the message thread with the Advanced editor open. That is KI-003 territory, rated Low. Its practical weight is on [UI-001](findings-ui.md#ui-001): any badge redesign that reads detach state from paint widens an ungoverned path. *Scope:* All platforms for the off-thread-restore race. On macOS/Windows there is additional reliance on an undocumented JUCE lock for message-thread writers. Only the Advanced view, where the badges are painted.

**Proposed improvement.** (a) Now, no gate: record the detach-mask paint read in THREAD_MODEL 'Which context paints', replacing the stale 'at P1' sentence, and cross-reference KI-003. (b) Together with [UI-001](findings-ui.md#ui-001)'s badge rework, publish a display bitmask. It would be a std::atomic<uint32_t>, one bit per managed_params index, stored relaxed, lock-free and static_asserted. It is written wherever liveDetachMask is written (replaceDetachMask and handleAsyncUpdate, the single writer family). Paint and the 24 Hz tick then read only that scalar. Every stale/fresh read is a badge set the writer published, which is the ADR-0038 property. This goes through the gate as an ADR-0027/0038 Message→Painting row amendment with THREAD_MODEL and THREADING_POLICY updates.

**Alternatives considered.**

- *Leave the StringArray read and document reliance on JUCE's MM try-lock* — Rejected: THREAD_MODEL.md:224-227 already refuses 'safe because a vendored renderer holds a lock'. It also does not cover the off-thread restore writer.
- *Editor tick copies the mask into an editor-owned atomic bitmask* — This closes the paint read, but the tick's own StringArray read still races an off-thread restore (KI-003). Publishing from the processor's mask writers closes both UI reads for the same gate cost.
- *Make the canonical mask a bitmask everywhere* — This would also close the mapper read, but it loses unknown ids on round-trip from a newer build's mask, which is a serialization read-rule question (ADR-0026). Out of proportion here.

**Decision: Modify · P3.** The policy gap and the doc drift are real and are confirmed from code. The claimed user-facing failure is narrower than stated: it is not reachable through drags at the pin, only through KI-003's off-thread restore. The proportionate response is to record the path now and repair it through the gate when the badge code is next touched for [UI-001](findings-ui.md#ui-001), rather than as a standalone threading change.

**Architecture gates.**

- Part (b): threading-model change — new Message→Painting site (THREADING_POLICY.md:26-28; ADR-0011; ADR-0027 clause 4 as amended by ADR-0038/ADR-0039) — Architecture Review Gate, owner approval
- Part (a) (THREAD_MODEL documentation only): none

**Dependencies.** [UI-001](findings-ui.md#ui-001) (do the publication together with the badge rework); KI-003 (off-thread restore exposure; this closes the display half of its liveDetachMask member)

**Acceptance criteria.**

- THREAD_MODEL.md 'Which context paints' names the detach-badge read and its current synchronisation (MM lock on GL; none against an off-thread restore) and links KI-003.
- After (b), grep shows no `detachMask()` call reachable from any paint() or from the editor timer. The badge loop reads a single relaxed atomic bitmask, and a static_assert pins it as lock-free.
- After (b), THREADING_POLICY's Message→Painting row lists the detach-bitmask site, and an ADR records the gate decision.
- A state_tests case restores a state with a non-empty mask and asserts the published bitmask equals the mask's managed indices; a reset-to-macro asserts it becomes 0.

<details><summary>Verification record</summary>

**Method.** Code read at e769f33. The paint loop at e769f33:src/gui/PluginEditor.cpp:1352-1363 is the only processor read in the editor's paint() (grep of paint 1295-1420); line 1361 is `proc.detachMask().contains(...)`. The accessor returns const StringArray& (e769f33:src/PluginProcessor.h:187) over liveDetachMask (:468). GL attaches on Mac/Windows (e769f33:src/gui/PluginEditor.cpp:1020-1022). The THREADING_POLICY row enumerates three sites and says 'Any path not in this table is a new cross-thread path → Architecture Review Gate' (e769f33:docs/policies/THREADING_POLICY.md:26,28). THREAD_MODEL.md:88-91 was read. Every writer of the mask was checked: handleAsyncUpdate (message thread only; drainDetachBitsSoon checks existsAndIsCurrentThread, :698), replaceDetachMask (:1549-1554), and the setStateInformation call site (:1957). Pinned JUCE 9.0.1 was read: juce_OpenGLContext.cpp:405 emplaces the MessageManager try-lock before paintComponent, which asserts the lock (:547). THREAD_MODEL.md:214-229 and KNOWN_ISSUES KI-003 (:46-91) were read. Not runtime-observable on the Linux harness.

**Corrections to the candidate claim.** (1) The claimed failure 'mis-draw or crash while a managed knob is dragged' is NOT reachable at the pinned JUCE. The GL render thread paints components only while holding the MessageManager lock, and every drag-path writer (handleAsyncUpdate, resetToMacro, applySlotToLive) runs on the message thread, which that lock excludes. THREAD_MODEL.md:214-229 records this lock and why the tree refuses to rely on it. (2) The genuinely unsynchronised writer is setStateInformation → replaceDetachMask (PluginProcessor.cpp:1957,1553) on hosts that restore off the message thread. That races the paint read on ALL platforms, Linux included, where paint is on the message thread and the restore thread is not excluded. It also races the editor tick's joinIntoString (PluginEditor.cpp:2122) and the mapper's isDetached lambda (PluginProcessor.cpp:76-77). KI-003 already lists liveDetachMask among the unclosed restore-thread exposures (KNOWN_ISSUES.md:86-91). (3) THREAD_MODEL.md:90 is prefixed 'at P1', so it is stale by omission rather than false.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 1 · severity 2 · discoverability 1 · efficiency 1 · coherence 3 · change risk 3 · complexity 2 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### TECH-003

**An off-message-thread state restore can crash the host: the KI-008 lock-order inversion aborts on a knob drag, and the KI-003 restore races corrupt the heap**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Technical robustness | Verification stops at the headless boundary, and gesture/restore threading can crash the host | Phase 3 |

**Evidence**

- e769f33:src/PluginProcessor.cpp:221-277 — the gesture-begin listener takes the §7 pre-state `gesturePreState = saveSlotFromLive()` at :275 on the message thread, from inside JUCE's parameter listenerLock (M0)
- e769f33:src/PluginProcessor.cpp:313-356 — gesture-end calls saveSlotFromLive() at :353, again inside endChangeGesture's listenerLock (a second M0->M1 site that KI-008 does not list)
- e769f33:src/PluginProcessor.cpp:1061-1071 — copyStateWithRaw -> apvts.copyState() (M1); :1080-1106 adoptParamsTree -> apvts.replaceState at :1105
- JUCE 9.0.1 (tag e18f7f5): juce_AudioProcessorParameter.cpp:78,101 (begin/endChangeGesture hold listenerLock while dispatching), :113 (sendValueChangedMessageToListeners takes it); juce_AudioProcessorValueTreeState.cpp:176 (setNormalisedValue -> setValueNotifyingHost), :391/:398/:419 (valueTreeChanging in copyState, replaceState and updateParameterConnectionsToChildTrees); juce_SharedCode_posix.h:42-44 (CriticalSection = PTHREAD_MUTEX_RECURSIVE + PTHREAD_PRIO_INHERIT)
- e769f33:src/PluginProcessor.cpp:1822-1884 — setStateInformation ('VST3 does not promise setStateInformation arrives on the message thread') calls resetSlotFieldsToDefaults at :1884, which writes livePresetName (:1738), liveSelection (:1747), liveDetachMask via replaceDetachMask (:1751 -> :1553) and presetBaseline (:1762) with plain stores
- e769f33:src/PluginProcessor.h:147,151,187 — currentPresetName/currentPresetSelection/detachMask return references to those non-atomic members; :165-170 presetDirty reads livePresetName and presetBaseline
- e769f33:src/gui/PluginEditor.cpp:2122,2146,2163,2217-2218 — the editor's 24 Hz tick reads detachMask().joinIntoString, currentPresetName, presetDirty (~3 Hz) and currentPresetSelection
- e769f33:docs/KNOWN_ISSUES.md:46-201 (KI-003, Low; 'no case of an off-message-thread restore has been observed') and :604-689 (KI-008, Medium; gesture-begin edge only)
- runtime V24-TSAN-1: rt/verify-24/tsan-e769.log + tsan-e769.err — TSAN state suite at e769f33: PASS 1423 checks, 23 'lock-order-inversion (potential deadlock)' reports (all the M0/M1 cycle), 0 data races, exit 66, 141 s wall
- runtime V24-DL-1: .../rt/verify-24/probe/deadlock_probe.cpp, dl-run1..3.log, gdb-bt.log — 'Fatal glibc error: pthread_mutex_lock.c:445 ... EDEADLK' in 4 of 4 runs. Message thread: beginChangeGesture -> gestureBegin :275 -> saveSlotFromLive :1241 -> copyStateWithRaw :1069 -> copyState (blocked on M1). Host thread: setStateInformation -> adoptParamsTree :1105 -> replaceState -> setNewState -> setValueNotifyingHost -> listenerLock (EDEADLK)
- runtime V24-RACE-1: .../rt/verify-24/probe/restore_race_probe.cpp. Plain build: p1-5.out and q1-3.out give 8 of 8 aborts with glibc heap-corruption messages; control c1-5.out (same restores, no concurrent reads) gives 5 of 5 clean exits. TSAN build: r1-3.err give 15-17 data races (resetSlotFieldsToDefaults :1738/:1747/:1751 against the message-thread reads) plus a SEGV
- runtime V24-PV-1: pluginval develop source .../rt/verify-24/pv_basic.cpp:688-719 ('Background thread state', requires GUI) and pv_utils.h:235-270 (state marshalled to the message thread only 'IfVST3'); macOS CI job 107767666230 at e769f33, .../rt/verify-24/mac_job.log:1408-2279 — the AU runs 'Background thread state' 6 times and passes
- *Merged at triage from another verifier's note, quoted as written:* Widen Step 2's scope and fix the doc record. KI-003's enumeration and the matching comment at e769f33:src/PluginProcessor.h:400-406 (re-checked) omit four members that resetSlotFieldsToDefaults also writes off-thread (e769f33:src/PluginProcessor.cpp:1730-1763, re-checked): presetBaseline, storedSlot, activeSlot and liveBaseline. presetBaseline is a juce::ValueTree. presetDirty() reads it (PluginProcessor.h:165-170) on the editor's roughly 3 Hz poll (PluginEditor.cpp:2158-2163). So the comment at PluginProcessor.h:160-164 ('no ValueTree member is touched') is wrong, and so is KI-006's round-51 implication that the dirty poll reads no ValueTree. The poll reads a refcounted ValueTree member that the restore replaces, which is the same heap-race class as V24-RACE-1. Step 2's staging must cover these members, and the KI-003 enumeration must list them.

**Current behaviour.** The §7 undo snapshot is taken inside JUCE's per-parameter listener lock (M0) and then takes the APVTS tree lock (M1), at both gesture begin and gesture end. A state restore holds M1 and notifies parameter listeners, which takes M0. setStateInformation, on whatever thread the host uses, also rewrites plain juce::String, Selection, StringArray and ValueTree members that the open editor reads 24 times a second. On the message thread all of this is safe. From a second thread, a concurrent drag start or release deadlocks: it aborted on Linux in the probe. A concurrent editor tick corrupts the heap.

**Problem.** Two defects on the same premise, a host restoring state off the message thread while the editor is open. (1) A knob grab or release that coincides with the restore's replaceState window aborts the host process on Linux, and is expected to hang it elsewhere. (2) Any restore that overlaps the editor tick is a data race on refcounted strings and arrays that corrupts the heap. The documentation rates these Low/Medium 'potential' or 'torn read' issues, and KI-008's table misses the gesture-end site.

**Root cause.** (1) An ABBA lock order across JUCE's two locks. §7 captures its pre-state with apvts.copyState() from within parameterGestureChanged, which holds the parameter's listenerLock. APVTS setNewState/setDenormalisedValue calls setValueNotifyingHost while holding valueTreeChanging. (2) The wrapper's session members (livePresetName, liveSelection, liveDetachMask, presetBaseline, liveFrozenTrims, and the APVTS tree via replaceState) have no publication mechanism. ADR-0011 assigns bulk swaps to the message thread, but the host chooses the thread that delivers setStateInformation, and nothing marshals or orders it.

**User impact.** If the premise occurs, the DAW crashes (Linux, verified) or freezes (expected on macOS/Windows), and unsaved project work is lost. The trigger is a knob grab or release during a host-driven state load, or simply an open editor during an off-thread load. It needs a host or format that restores off the message thread. That is not observed in a DAW so far, but pluginval deliberately does it for AU. The user cannot diagnose or avoid it. *Scope:* All platforms and formats. AU is the format where off-thread state calls are exercised (pluginval AU); VST3 by pluginval convention arrives on the message thread. Only when the editor is open (for the race) or during a drag gesture (for the inversion). The restore window is milliseconds, so real-world frequency is low.

**Proposed improvement.** Target behaviour: grabbing or releasing a knob never blocks on, or contends with, the APVTS tree lock. A host restore on any thread leaves the editor showing either the complete old or the complete new session, and never crashes or hangs. Step 1 (smallest, removes KI-008): capture the §7 pre-state without taking M1 inside the listener, at the same instant, so the undo grammar is unchanged. For example, build the PARAM children from each parameter's atomic value and raw value (the data copyStateWithRaw already overlays), or keep a snapshot maintained on the message thread that the callback only reads. Apply this at both gesture begin (:275) and gesture end (:353). Step 2 (KI-003, separate decision): an off-message-thread setStateInformation stages the decoded session, applies it on the message thread, and getStateInformation answers from the staged copy until then. Alternatively, the session members move behind a publication mechanism that the editor reads atomically. Both steps need a two-thread regression stimulus under TSAN (see [TEST-005](findings-doc-test.md#test-005)).

**Alternatives considered.**

- *Leave as documented KIs* — Not justified. The hazard is reproducible at e769f33: a Linux abort and heap corruption with the real wrapper, not a theoretical 'potential deadlock'. The KIs also understate it (they miss the gesture-end edge, the crash mode, and the pluginval AU premise).
- *Defer the pre-state snapshot to the next message tick* — Rejected. KI-008 itself explains that it captures a state the first edit has already changed, so it changes the undo grammar.
- *Synchronously marshal setStateInformation onto the message thread (post and wait)* — Rejected. It deadlocks whenever the host's calling thread holds or waits on the message thread, which is a worse failure than the one it removes.
- *Asynchronously stage the restore and apply it on the message thread* — Closes both halves, but it is a threading-model change. It also risks the first blocks of an immediate offline render running with the old state unless the audio side also consumes the staged record. It needs an ADR, and the ADR-0012 staged-record row may be the right precedent.
- *Lock-free pre-state capture only (chosen first step)* — Smallest change that removes the deadlock edge without touching the restore thread model. It must be proven equivalent to the copyState() snapshot, including the raw-exact overlay and any non-PARAM children of the APVTS tree.

**Decision: Modify · P2.** The obvious fix, marshalling every restore, is a large threading-model change with its own render-correctness risk. The KI-008 crash can be removed by a constrained change to where the §7 pre-state reads its data, at both gesture begin and end, with the undo grammar unchanged. That step should go through the gate now. The KI-003 heap-race half remains a separate gated decision, but it should be re-rated in KNOWN_ISSUES from a torn read to a crash, based on this evidence.

**Architecture gates.**

- Architecture Review Gate: KI-008 names moving the §7 undo pre-state snapshot point as an undo-architecture change requiring review
- Thread Model change (THREAD_MODEL.md / THREADING_POLICY synchroniser set): any restore-staging or marshalling path, or new synchroniser, for the KI-003 half is gated and an AI-agent Hard Stop
- ADR-0011 (message thread owns preset/A-B/undo bulk swaps; permitted-path table): a restore-staging path needs an amendment
- ADR-0012 (bounded staged record row) if staging is used for the restore
- ADR-0013/raw-exact restore and ADR-0018 undo grammar must remain unchanged by the lock-free snapshot (no conflict intended; must be demonstrated)

**Dependencies.** [TEST-005](findings-doc-test.md#test-005)

**Acceptance criteria.**

- A two-thread stimulus runs 60 s on Linux with no abort or hang: the message thread begins and ends gestures on one parameter in a loop while a second thread calls setStateInformation alternating two sessions that differ in that parameter. Today it aborts within about 0.1 s.
- A TSAN build of AnabasisStateTests reports 0 lock-order-inversion warnings (e769f33: 23).
- Undo behaviour is unchanged: the existing undo tests pass unmodified; one drag produces one step; the restored pre-state is raw-exact.
- KI-003 half, if the gate approves it: a TSAN stimulus of an off-thread restore against the editor-tick reads (name, selection, detach mask, presetDirty) reports 0 data races. The plain build completes 300 restores without heap corruption (today 8 of 8 abort).
- KNOWN_ISSUES KI-008 lists the gesture-end site and the Linux abort mode. KI-003 is re-rated, lists presetBaseline among the restore-written members, and records that CI's pluginval AU exercises off-thread setStateInformation with the editor open.

<details><summary>Verification record</summary>

**Method.** (1) I read every cited anchor at e769f33, plus the pinned JUCE 9.0.1 lock sites. (2) I built AnabasisStateTests with -fsanitize=thread (gcc 13, RelWithDebInfo) from the e769f33 tree and ran it. (3) I built scratch probes outside the repo that link the real wrapper sources. DeadlockProbe has the message thread calling beginChangeGesture/endChangeGesture on `ceiling` while a second thread calls setStateInformation, alternating two sessions that differ in `ceiling`; I ran it plain and under gdb. RestoreRaceProbe has a second thread call setStateInformation 300 times while the message thread reads exactly what the editor tick reads. I ran it plain, under TSAN, and as a control with the reads removed. (4) For the host premise, I read pluginval's develop-branch source and pulled the e769f33 CI job logs (run 36039432935). No GUI reproduction was needed, and the harness cannot restore off-thread.

**Corrections to the candidate claim.** The claim holds, but it understates the impact. (a) On Linux the inversion does not hang; the process aborts. JUCE's POSIX CriticalSection is recursive with PTHREAD_PRIO_INHERIT, so the kernel detects the priority-inheritance cycle and glibc asserts on EDEADLK. It aborted in 4 of 4 runs, within 24-64 ms of process start under the stress loop. A hang on macOS or Windows is the expected outcome but I did not reproduce it. (b) KI-008 tabulates only gesture-BEGIN as an M0->M1 site. Gesture-END is a second one: endChangeGesture holds listenerLock, and the processor calls saveSlotFromLive at PluginProcessor.cpp:353. Releasing a knob is therefore as exposed as grabbing it. (c) The KI-003 residual is not merely a 'torn' preset name or badge. The plain build aborted with glibc heap corruption in 8 of 8 runs ('double free or corruption', 'unaligned fastbin chunk'). The control run with the reads removed was clean in 5 of 5. TSAN reported 15-17 data races (on juce::String, StringArray, Selection and operator delete[]) and then a SEGV in 4 of 4 runs. (d) 'Undo' in the hypothesis applies only to host-initiated restores; the plugin's own undo()/redo() is message-thread only. (e) The premise is less hypothetical than KI-003's 'no off-message-thread restore observed' suggests. In pluginval's develop source, state calls are marshalled to the message thread only for VST3. For AU they are made from a background thread with the editor open. The macOS job runs that 'Background thread state' test 6 times on the AU at e769f33. The pluginval binary is downloaded unpinned, so this is the source's behaviour, not the exact binary's. (f) The M1->M0 edge is reachable only from an APVTS tree write (replaceState or setNewState). Audio-thread host automation does not take M1.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 1 · severity 5 · discoverability 5 · efficiency 2 · coherence 3 · change risk 4 · complexity 4 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### TECH-004

**Editor message-thread cost has no recorded budget; the FFT and the micro-animation loop are small, and on Linux most of the cost is spectrum painting**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | partially-confirmed | Technical robustness | Verification stops at the headless boundary, and gesture/restore threading can crash the host | Phase 5 |

**Evidence**

- e769f33:src/gui/SpectrumView.cpp:200 (4096-pt performFrequencyOnlyForwardTransform inside analyse)
- e769f33:src/gui/SpectrumView.cpp:552-553 (two analyse calls per drawn tick)
- e769f33:src/gui/SpectrumView.cpp:394 (idle gate: no new committed frames -> return before FFT)
- e769f33:src/gui/SpectrumView.cpp:37-89 (clock started/stopped with visibility)
- e769f33:src/gui/FrameClock.h:21-25 (~125 Hz even-cadence cap)
- e769f33:src/gui/PluginEditor.cpp:1046-1052 (micro-anim driver on a raw VBlankAttachment, not FrameClock)
- e769f33:src/gui/PluginEditor.cpp:2900-2949 (stepMicroAnims loop; repaint only when moved at :2946-2947)
- e769f33:src/gui/PluginEditor.cpp:1020-1022 (GL attached on Mac/Windows only; Linux paints on the message thread)
- e769f33:docs/DESIGN.md:990-996 (§9 budget is the audio engine's)
- e769f33:docs/architecture/PERFORMANCE_BUDGET.md:1-127 (no GUI/editor row)
- e769f33:docs/architecture/design-decisions/ADR-0011-threading-model.md:93-99 (option F: analysis worker rejected; FFT stays GUI-side)
- runtime V25-CPU (display :155): per-thread CPU SPEC 15.5/15.1/15.25 %, GR 8.8/12.75/10.5 %; views: session capture `rt/verify-25/01-spec.png`, session capture `rt/verify-25/02-gr.png`
- runtime V25-PROF: 450 gdb samples of the main thread in the SPEC view: rt/verify-25/spec-samples.txt and spec-hover-samples.txt (FFT 5, stepMicroAnims 1, SpectrumView::paint 42, other paint 19, idle 381)

**Current behaviour.** With the spectrum visible and audio flowing, SpectrumView::tick runs two 4096-point FFTs per FrameClock tick, capped at about 125 Hz, on the message thread. The editor's micro-animation loop polls every registered widget on every vblank, uncapped, and repaints only widgets whose eased value moved. On Linux all painting also runs on the message thread. No document records the editor's total message-thread cost.

**Problem.** The GUI-side cost is not budgeted anywhere, so a regression or a high-refresh or multi-instance cost cannot be judged against a reference. The specific worry in the finding (FFT and micro-animation polling starving the message thread) does not match the measured profile: those two are about 1 % and 0.2 % of the thread, and painting is about 13 %.

**Root cause.** PERFORMANCE_BUDGET.md and DESIGN §9 cover only the audio engine. The GUI has piecemeal numbers but no aggregate measurement. ADR-0011 keeps the FFT on the message thread by design, which is correct and matches the sibling.

**User impact.** No measurable user harm at these loads: about 15 % of one core with the spectrum shown and about 10 % with GR history, under Linux software rendering. The absence of a reference number is a maintainability risk. It is not a current usability defect. *Scope:* Editor message thread on all platforms. The paint share applies to Linux, where there is no GL. On macOS and Windows the paint moves to the GL thread, and the tick (FFT) and micro-animation loop stay on the message thread.

**Proposed improvement.** Add a GUI section to docs/architecture/PERFORMANCE_BUDGET.md with measured editor message-thread CPU for the GR and SPEC views, split into FFT/tick, paint and micro-animation. Measure on the Linux software renderer and on one GL platform, at 60 Hz and at 120 Hz or more. Record the method: per-thread /proc accounting plus stack sampling, as here. Do not move the FFT, and do not port Anamorph's generation/FNV idle gate on this evidence. If optimisation is ever wanted, the spectrum paint path is where the cost is.

**Alternatives considered.**

- *Leave as is* — Acceptable for users today. The measured load is modest, but a later regression would have no reference to be compared with.
- *Port Anamorph's S11/H15 idle gate for stepMicroAnims* — Not justified: the driver measured about 0.2 % of the thread. The sibling's gate later needed its own fix (KI-025), so it brings complexity and risk for no measured gain.
- *Move the FFT to a worker thread* — Rejected. The FFT is about 1 % of the thread, and a worker would be a threading-model change that ADR-0011 option F explicitly rejected (hard-stop gate).
- *Cap the micro-animation driver with FrameClock pacing* — Cheap and harmless, but the measured cost does not motivate it. At most P3 polish, and only if the budget measurement shows high-refresh cost.

**Decision: Modify · P3.** The claim is right that nothing is budgeted, but wrong about where the cost is. The smaller, correct action is a recorded measurement rather than FFT or animation restructuring. Priority follows the rubric: no user-facing harm was found, so this is maintainability and robustness polish.

**Architecture gates.**

- None for the proposed measurement/doc change
- Moving the FFT off the message thread would be a threading-model change (ARCHITECTURE_REVIEW_GATE; conflicts with ADR-0011 option F) and is not recommended
- Any change to SpectrumView::tick's publication path touches ADR-0039's bracket; paint-only work does not

**Dependencies.** None.

**Acceptance criteria.**

- PERFORMANCE_BUDGET.md contains a GUI/editor section with message-thread CPU for the GR view and the SPEC view with audio flowing, measured on the Linux software renderer and on at least one GL platform, at a 60 Hz display and at a display of 120 Hz or more
- The section splits the SPEC-view cost into SpectrumView::tick (FFT), paint and stepMicroAnims, and states the measurement method and machine
- No FFT or threading change is made on the basis of this finding

<details><summary>Verification record</summary>

**Method.** Opened every anchor at e769f33. Ran the harness on :155 (music at -18 dB, 48 kHz/512, pointer parked outside the editor) and read /proc per-thread CPU over 8-10 s windows, three windows in the SPEC view and three in the GR view. Took 450 gdb stack samples of the message thread in the SPEC view: 250 with the pointer outside the editor and 200 hovering the Loudness knob. Sorted each sample by the frame it landed in. Read JUCE's Linux vblank source (juce_Windowing_linux.cpp:645-657). Its fallback is 100 Hz when the display reports no refresh rate, and the burst captures showed about 10 ms frame spacing, so the SPEC measurements ran near the ~125 Hz cap scenario.

**Corrections to the candidate claim.** (1) The FFT work is gated in two ways. The spectrum clock runs only while the spectrum is the visible view (SpectrumView.cpp:37-89), and GR and SPEC share one well, so only one of them runs at a time. A tick with no new frames returns before any FFT (SpectrumView.cpp:394). (2) The micro-animation driver is not FrameClock-paced. It is a raw VBlankAttachment (PluginEditor.cpp:1046-1052), so the ~125 Hz cap does not apply and it runs every vblank on 144/240 Hz displays. It does gate repaints per widget: only widgets that moved repaint (:2946-2947). 'No idle gate' is true of the polling only. (3) DESIGN §9 budgets the audio-thread engine; it does not cap GUI or metering-display cost. PERFORMANCE_BUDGET.md has no GUI row, so 'unmeasured' is correct at the documentation level. Scattered figures do exist: 132 µs per FFT in the SpectrumView comment and THREAD_MODEL.md:250, and 163.7 µs for the GR scan in HANDOVER. (4) Measured here: the message thread used 15.1-15.5 % of one 2.1 GHz Xeon core in SPEC and 8.8-12.8 % in GR (Linux software renderer). Stack samples: FFT/analyse 5/450 (~1 %), stepMicroAnims 1/450 (~0.2 %), SpectrumView::paint 42/450 (~9 %), other painting 19/450 (~4 %), idle 381/450. The two mechanisms the finding names are minor, and painting dominates. The hypothesis that knob or automation responsiveness suffers is not supported at these loads.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 3 · severity 1 · discoverability 3 · efficiency 1 · coherence 2 · change risk 1 · complexity 2 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

