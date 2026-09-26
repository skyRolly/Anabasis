# Anabasis product / UX audit — findings: Metering and visualisation

Part of [`2026-09-26-anabasis-product-ux-audit.md`](../2026-09-26-anabasis-product-ux-audit.md) (audited revision `e769f33`, 2026-09-26). This file holds the complete record of each finding in these categories; the report carries the index, the systemic themes, the roadmap and the decision record. Code anchors are pinned to `e769f33`; runtime observation ids refer to [`worklogs/2026-09-26-product-ux-audit.md`](../../../worklogs/2026-09-26-product-ux-audit.md).

Each record: decision, priority and confidence after calibration; evidence; current behaviour; problem; root cause; user impact and scope; proposed improvement; alternatives considered; decision rationale (with any calibration or challenge outcome); architecture gates; dependencies; acceptance criteria; and the verification record.

## VIS — Metering and visualisation

### VIS-001

**Bypassed passages are folded into the session measurements: I, LRA, PLR (always) and TP/SP holds (when the dry peaks higher) absorb the unprocessed input and stay wrong after un-bypass**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Visualization | The session figures have no visible scope, liveness or tap, and a stray click wipes them | Phase 1 |

**Evidence**

- e769f33:src/PluginProcessor.cpp:977 and :983-984: dbTpMaxHold / samplePeakMaxHold max-hold every processed block, with no bypass condition
- e769f33:src/PluginProcessor.cpp:996-1014: I, TP, PLR, SP, RMS, I-ungated and LRA published every processed block
- e769f33:src/dsp/AnabasisEngine.cpp:1260-1264: render = delayedDry when bypassMix >= 1
- e769f33:src/dsp/AnabasisEngine.cpp:1300-1309: outMeter (integrated/LRA histogram), outRms and outTp are fed from renderFrame
- e769f33:src/dsp/AnabasisEngine.cpp:1243-1259: design intent, 'the meters report what the plugin EMITTED'
- e769f33:tests/dsp_tests.cpp:5777-5795: the output meter reached through the BYPASS leg is test-pinned
- e769f33:src/gui/LoudnessMeterView.cpp:67-70 and e769f33:src/PluginProcessor.cpp:1871: the only requestMeterReset callers; e769f33:src/PluginProcessor.cpp:807, :812: prepareToPlay clear
- e769f33:docs/user/USER_MANUAL.md:112-113: Bypass A/B is a recommended workflow step
- e769f33:docs/user/USER_MANUAL.md:229: TP 'turns red above your Ceiling'
- e769f33:docs/user/USER_MANUAL.md:271-274: reset advice does not mention bypass
- V-05: session capture `rt/visuals/10d-bypass-on-editor.png`, 10d2-bypass-on-5s-editor.png, 10d3-bypass-off-editor.png
- ST-13: session capture `rt/state/30-bypass-composite.png`
- verify-6 realistic input: session capture `rt/verify-6/02-03-dry-vs-master-stats.png` (dry I -18.3 / TP -0.15, master I -11.6 / TP -0.96 / LRA 3.9)
- verify-6: [capture](captures/07-bypass-folds-into-holds.png) (8.5 s bypass: I -11.9 -> -12.6, LRA 4.0 -> 5.9)
- verify-6: session capture `rt/verify-6/05-long-bypass-stats.png` (17 s bypass: LRA 4.0 -> 8.4, PLR 11.0 -> 12.0)
- verify-6: session capture `rt/verify-6/06-dry-recheck.png` (after un-bypass: TP hold -0.60 dBTP red vs -1.00 dBTP ceiling, LRA 10.8)
- verify-6: session capture `rt/verify-6/10-dirty-check.png` (BYPASS does not mark the preset edited)

**Current behaviour.** While BYPASS is on, the render tap is the delay-aligned dry input, and every session accumulator keeps integrating it: the gated and ungated integrated histogram, LRA, and the TP and SP max-holds. PLR is derived from them. After un-bypass, the STATISTICS panel shows figures for a mixture of processed and unprocessed audio. Nothing indicates this, and it persists until a manual panel-click reset, a session load or a re-prepare.

**Problem.** The panel the product presents as the master's session measurement ('the honest check on a sample-peak ceiling'; 'judge PLR') silently describes a blend of the master and its input after the manual's own recommended bypass comparison. Integrated loudness is biased low, LRA is inflated (by several LU in the tests above), and PLR is inflated. The TP hold can go red over the ceiling for a master that never exceeded it.

**Root cause.** The session-cumulative accumulators take the same render tap as the rolling meters. By design that tap includes the bypass leg, so the plugin meters what it emits. No rule pauses or segments the session accumulation while bypassed (PluginProcessor.cpp:977, :983-984; AnabasisEngine.cpp:1300-1309).

**User impact.** A user checking delivery after a comparison reads the wrong figures. I is ~0.5-1 LU low after typical bypass shares, which pushes toward an over-loud master. LRA and PLR are overstated. A TP false alarm can prompt an unnecessary ceiling or TP change, or distrust of the limiter. Recovery means resetting and re-playing the whole programme. *Scope:* The STATISTICS rows I, LRA, PLR, TP and SP in both views, gated and ungated. It affects any session in which BYPASS, or a DAW bypass (routed to the same parameter), is engaged during a measurement. It has no effect on audio.

**Proposed improvement.** While fully bypassed, suspend the session-cumulative accumulation on the audio thread: skip the integrated/LRA histogram commits and the TP/SP max-hold updates. Use engine bypassMix >= 1; the ~10 ms ramp blocks are handled by one documented rule. M, S, RMS and out LUFS keep measuring what is emitted (the dry signal), which preserves the pinned bypass-leg meter behaviour. During bypass, the STATISTICS panel shows the session rows in a visibly paused state (for example greyed, with 'held while bypassed'). After un-bypass, accumulation resumes on processed audio only, so I, LRA, PLR, TP and SP describe the master alone.

**Alternatives considered.**

- *Auto-reset the session measurements on each bypass toggle* — Reject: it destroys a long integrated measurement on every comparison.
- *Keep two accumulators (processed and bypassed) and show input statistics while bypassed* — Most informative, but it doubles the histogram and LRA state and the UI surface. Out of proportion for now.
- *View-only warning ('includes N s of bypassed audio') plus a reset affordance* — Honest and cheap, but the numbers stay wrong, and the user must re-play the programme to fix them.
- *Document the behaviour in USER_MANUAL §3.4 only* — Minimum; leaves the delivery-check trap in place.

**Decision: Modify · P2.** Reproduced with a realistic input and code-confirmed. It contradicts how the product presents the panel and the manual's own A/B-then-judge workflow. The fix is local to the audio-thread accumulators and the panel paint. It keeps the deliberate 'meter what is emitted' rule for the rolling rows.

*Calibration:* the verifier judged Proceed / P1; the final judgement is Modify / P2. Challenge accepted: the mechanism is real and LRA/PLR pollution is large, but the TP false alarm was not demonstrated (row already red before bypass) and I moves ~0.1-0.3 LU at realistic shares; recovery is the documented reset (harm carried by [UX-002](findings-ux.md#ux-002)), so P2. Modify: suspend through LoudnessMeter's existing integratedFrom/lraFrom watermarks while bypassMix>0 and resume with straddle offsets (no clearSessionCumulative); separate engine-side session TP/SP holds gated per frame, leaving renderPeakChunk and the GR-history feed alone; settle offline/automated-bypass semantics (limit to !nonRealtime or document); replace the dsp_tests route that would go vacuous; ADR-0020 amendment with owner sign-off.

*Adversarial challenge:* evidence holds: yes; priority justified: no (suggested P2); decision justified: no (suggested Modify). (1) Evidence: the mechanism is real and code-confirmed, and the LRA/PLR pollution is reproduced at a large size. But the headline TP false alarm is not demonstrated: the TP row was already red (-0.96 vs -1.00, no slack) before bypass. The I shift has no control run, and at realistic full-programme shares it is ~0.1-0.3 LU, not 0.5-1 LU.
(2) Priority: the rubric's P1 needs material harm to a common workflow or a frequent trap with costly recovery. The harm needs bypass to be engaged mid-measurement AND no reset before reading. The manual already prescribes a one-click reset after changes (USER_MANUAL.md:271-274). Parameter tweaks in the same A/B loop blend the session figures the same way, so a clean delivery measurement is normally a fresh pass anyway. The error directions are mostly conservative or small: TP over-reported, I slightly low. LRA/PLR inflation is large but informational. Recovery is one reset plus the replay that any measurement pass costs. That fits P2: a meaningful correctness and clarity improvement in a lower-cost situation.
(3) Decision: the issue is worth fixing, and it has strong coherence support: the product removed Delta/Comp from the meters for the same reason, and DESIGN §1.2 already places the meter tap above bypass. But the proposal as written does not meet its own acceptance criteria and misses the risks in (a)-(f). Modify it to reuse LoudnessMeter's existing watermark gates with a bypassMix>0 suspend and resume-watermark rule, add separate engine-side session TP/SP holds that leave the GR-history feed alone, and settle the offline/automated-bypass case. Replace the test route that becomes vacuous. Take it through an ADR-0020 amendment with owner sign-off.

Suggested scores: user_impact 3, frequency 3, severity 3, discoverability 4, workflow_efficiency 3, coherence 4, change_risk 3, complexity 3, evidence_confidence 4. *Proposal risks:* (a) The pause criterion is wrong-sized. Pausing only at bypassMix>=1 still admits dry audio after every un-bypass. Gating blocks average the last 4 sub-blocks (LoudnessMeter.h:368-372), so up to 300 ms of dry-straddled blocks enter I. LRA samples are 3 s short-term windows (:414-416), so ~29 dry-straddled samples enter LRA per toggle. Repeated A/B keeps inflating LRA, and the acceptance criterion 'LRA within 0.2 LU' would fail for multi-toggle sessions. The smaller, correct mechanism already exists: resetIntegrated's watermarks (LoudnessMeter.h:123-156). Suspend (integratedFrom/lraFrom = max) while bypassMix>0. On return to exactly 0, set integratedFrom = subCount+4+straddler and lraFrom = subCount+30+straddler, WITHOUT clearSessionCumulative. The ungated accumulator is already behind the same integratedFrom gate.
(b) TP/SP need separate engine-side session accumulators, gated per frame on bypassMix>0. Do not gate renderPeakChunk/renderTpMaxChunk: renderPeakChunk also feeds the GR-history waveform (histPeak, AnabasisEngine.cpp:770), and gating it would change that display during bypass. Host blocks can also straddle the toggle, so a per-block processor flag is insufficient.
(c) Offline render semantics. bypass is automatable (DESIGN.md:462, Auto=yes) and is the host bypass parameter. The render tap's stated rationale is 'exactly what an offline render emits' (DOCUMENTATION_COVERAGE.md:4758). An unconditional pause makes session I/LRA/TP disagree with a bounce that contains automated bypass. Either limit the pause to !nonRealtime (snapshot.nonRealtime already exists) or document the divergence.
(d) Test coverage silently degrades. The integrated half of e769f33:tests/dsp_tests.cpp:5777-5846 (intBefore :5811, check :5843-5845) runs entirely in bypass. With the pause, both reads return kSilentLufs (-100, LoudnessMeter.h:57), so the check passes vacuously. The histogram's non-finite defence then loses its only engine-level route, because the programme path is ceiling-bounded (DOCUMENTATION_COVERAGE.md:3970). It needs a replacement test, not only 'keep passing'.
(e) Paused-state UI. A whole-editor dimOverlay below the top bar is already shown while bypassed (PluginEditor.cpp:678-680, :1427, :2033-2035). A 'greyed' paused state for the session rows would be indistinguishable from it, so use text ('held while bypassed') or a treatment that reads through the dim.
(f) Gate. This is not a signal-order, latency, threading, parameter or schema change. It does change the session-cumulative contract of an Accepted ADR (ADR-0020: the quantities are 'all on the existing render tap'; the reset/accumulate contract is in Consequences). It also reverses the documented 'meters report what was EMITTED' choice (AnabasisEngine.cpp:1243-1259; DOCUMENTATION_COVERAGE.md:4625-4626). Under ADR_POLICY rule 4 (append-only), it needs an owner-approved amendment or a superseding ADR before code: treat it as the 'conflict with an Accepted ADR' review gate, not just a dated note. Reconcile DESIGN §1.2 in the same change.

**Architecture gates.**

- No ARCHITECTURE_REVIEW_GATE hard-stop category: the audio path, signal order, latency, parameters, serialization and threading are unchanged, since bypassMix is already audio-thread state. The metering semantics of ADR-0020's statistics rows (fed from the render tap) and DESIGN §2.9 change, so record it as a dated note or amendment rather than a silent change. e769f33:tests/dsp_tests.cpp:5777ff (M/S through the bypass leg) must keep passing.

**Dependencies.** [UX-002](findings-ux.md#ux-002); [UX-008](findings-ux.md#ux-008); [VIS-004](findings-visualisation.md#vis-004)

**Acceptance criteria.**

- Engine/processor test: after N blocks of processed audio, M blocks with bypass=true on a dry signal with higher peaks, and K more processed blocks, the published TP/SP holds equal those of the processed-only run (bit-identical or within 0.01 dB).
- Same test: I and I-ungated are within 0.1 LU, and LRA within 0.2 LU, of a run that omits the bypassed span.
- While bypassed, M/S/RMS/out LUFS still follow the dry signal (the existing bypass-leg meter test is unchanged and passes).
- Runtime (harness, music -12, Ceiling -1.00 dBTP with TP on): after a 17 s bypass the TP row does not turn red and LRA stays within 0.5 LU of its pre-bypass value.
- During bypass the STATISTICS session rows (I, TP, SP, LRA, PLR) render in a visibly distinct paused state that clears on un-bypass.
- USER_MANUAL §3.4 states that session figures pause while bypassed.

<details><summary>Verification record</summary>

**Method.** Code read at e769f33: PluginProcessor.cpp:963-1014 (holds and publication every processed block), :926-931 and :807/:812 (reset sites), PluginProcessor.h:627-650; AnabasisEngine.cpp:1260-1266 (render = delayedDry at bypassMix>=1) and :1297-1309 (outMeter/outRms/outTp fed from renderFrame); LoudnessMeterView.cpp:67-70; grep of requestMeterReset callers. Checked that the render-tap inclusion of the bypass leg is intended (AnabasisEngine.cpp:1243-1259) and test-pinned (e769f33:tests/dsp_tests.cpp:5777-5795). Viewed V-05 and ST-13 screenshots. Reproduced on :136 with a REALISTIC input (music at -12 dB: dry I -18.3 LUFS, dry TP between -0.60 and -0.15 dBTP, i.e. below 0 dBFS). Master: Loudness 60 %, Ceiling -1.00 dBTP with TP on, BYPASS toggled with stepped clicks. Also tested whether BYPASS dirties the preset.

**Corrections to the candidate claim.** (1) The TP/SP pollution depends on content. In an 8.5 s and a 17 s bypass the TP/SP holds did not move (-0.96 dBTP / -1.00 dBFS), because the dry peaks in those windows stayed below the master's. The +5.5 dBFS figures in V-05 come from the hot harness signal. With the realistic input, a bypass window that contained higher dry peaks left TP held at -0.60 dBTP, red against a -1.00 dBTP ceiling, after un-bypass. (2) I, LRA and PLR pollution is systematic, because a maximizer's dry is always at a different loudness: I -11.9 -> -12.6 LUFS and LRA 4.0 -> 5.9 LU after 8.5 s of bypass; LRA 4.0 -> 8.4 LU and PLR 11.0 -> 12.0 after 17 s. (3) Metering the dry signal in the rolling rows during bypass is deliberate: 'the meters report what the plugin EMITTED', and it is test-pinned. The defect is limited to the session-cumulative accumulators. (4) Besides the two requestMeterReset callers, prepareToPlay also clears the holds (PluginProcessor.cpp:807, :812). (5) ST-13's aside that BYPASS dirties the preset is refuted: after loading 'Transparent Master', BYPASS on and off leaves the name clean (10-dirty-check.png).

</details>

<sub>Verifier scores (1-5): impact 4 · frequency 4 · severity 4 · discoverability 4 · efficiency 4 · coherence 4 · change risk 2 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-002

**The TP row is red at the shipped defaults on every limited pass, and still red in TP mode when TP equals the ceiling, so the warning colour carries no information**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Visualization | The dBTP delivery ceiling is claimed but not enforced, and its warning cannot tell an over from normal operation | Phase 0 |

**Evidence**

- e769f33:src/gui/LoudnessMeterView.cpp:229-244 — the TP warn is `shownTp > shownCeiling`, exact and mode-blind. The comment calls it 'a product call, not a bug fix'.
- e769f33:src/gui/LoudnessMeterView.cpp:265-267 — the SP row uses kCeilingWarnSlackDb = 0.005 dB.
- e769f33:src/gui/LoudnessMeterView.cpp:206-215 — statRow has only a binary warn/text colour.
- e769f33:docs/architecture/design-decisions/ADR-0015-pre-ship-contract-refreeze.md:217-229 — open question: 'What should the row's warn colour mean?'
- e769f33:docs/architecture/design-decisions/ADR-0020-waveform-statistics-panel.md:229-231 — 'The TP row keeps its exact comparison'.
- e769f33:docs/user/USER_MANUAL.md:229-230
- V-12 (visuals.md): session capture `rt/visuals/28b-tp-off-20s-editor.png` — TP 0.49 red, SP -0.10 white. session capture `rt/visuals/28a-tp-on-20s-editor.png` — TP on, -0.06 red.
- E13: session capture `rt/edges/40a-square50-plus6-simple.png` — TP 1.75 red, SP -0.10 white.
- verify-19 R1: session capture `rt/verify-19/01-defaults-tpoff-15s-stats.png` — TP 0.62 dBTP red, SP -0.10 white.
- verify-19 R2: .../rt/verify-19/02-lvl18-loud50-tpoff-15s-stats.png — TP 0.12 red.
- verify-19 R5: .../rt/verify-19/06-tpon-trans0-15s-stats.png and 07-tpon-trans0-25s-stats.png — TP mode, the row prints '-0.10 dBTP' in RED against a -0.10 dBTP ceiling (SP -0.23 and -0.10, white).

**Current behaviour.** The TP row turns warn-red whenever the session true-peak hold exceeds the live Ceiling value by any amount.
- TP off (the default): the Ceiling is a sample-peak limit, so any limited programme shows ordinary inter-sample overshoot. The row is red in every limited pass: 0.04, 0.12, 0.49, 0.62 dBTP observed.
- TP on: the row is red even when the hold prints exactly the ceiling value, because the comparison has no tolerance.

The SP row beside it is white in all these cases.

**Problem.** The only red on the metering panel is lit in essentially every configuration a user masters in, so it cannot tell the user anything. It conflates two different states:
- the ceiling you set is sample-peak and the programme has inter-sample peaks above it (expected with TP off);
- the ceiling in force was violated (TP on).

In TP mode a value printed equal to the ceiling is still painted as a failure.

**Root cause.** The comparand and the tolerance are an unresolved owner product call. ADR-0015 leaves the TP warn semantics open, and the exact comparison is re-affirmed in the ADR-0020 amendment. statRow offers only one warn colour, so a mode-aware distinction cannot be expressed. The TP row did not receive the dB-domain print-resolution slack that the SP row got in the same amendment.

**User impact.** A user following the quick start at defaults sees a permanent red 'over' on a correctly limited master. They either:
- distrust the ceiling promise;
- lower the ceiling by 0.5-1.5 dB and lose loudness for a warning they did not need; or
- learn to ignore red.

The last is the costly one. In TP mode the same red then fails to stand out when output genuinely exceeds the dBTP ceiling by more than 1 dB ([DSP-001](findings-dsp-tech.md#dsp-001)). *Scope:* The Statistics panel TP row, which is identical in the Simple and Advanced views. It affects every session at the shipped defaults whenever the limiter works, and TP mode at any ceiling.

**Proposed improvement.** Keep the mode-blind comparison, but make the colour and label say which situation applies:
1. TP off, TP hold > Ceiling: draw the value in an advisory style (accent/amber, not warn red) with a compact qualifier such as 'ISP' or 'over sample-peak ceiling'. The row tooltip says 'Inter-sample peaks above your sample-peak Ceiling — engage TP to hold dBTP'. The SP row stays the red 'ceiling exceeded' signal in this mode.
2. TP on: red only when the TP hold exceeds the ceiling by more than a stated tolerance. At minimum that is the SP row's half-print-resolution slack, so a value printed equal to the ceiling is never red. Whether to use the policy's 0.1 dBTP is the owner's call.
3. Result: red means exactly one thing in both modes — the ceiling in force was exceeded on the quantity it guarantees.

Record the answer to ADR-0015's open question as an ADR-0015/ADR-0020 amendment.

**Alternatives considered.**

- *Leave as-is (honest arithmetic, the documented behaviour).* — Rejected. The number stays honest under the proposal. The colour, lit in every configuration, is what carries no information.
- *Compare against the ceiling only while TP is engaged (ADR-0015's first alternative).* — Rejected. It removes the only on-screen cue that a default master has inter-sample overs, trading over-warning for silent under-warning on delivery-critical data.
- *Compare the TP row against the sample peak or use the TP−SP gap.* — Duplicates the SP row, which ADR-0020 added for exactly that reading. It could be additive, e.g. an 'ISP +x.xx dB' annotation, but it does not replace a clear warn meaning.
- *Default truePeakMode on (ADR-0015 option H).* — Out of scope. It is a signed-off default, and it would not help anyway: TP mode is itself red, partly with real overs ([DSP-001](findings-dsp-tech.md#dsp-001)).
- *A Settings option for the warn reference.* — Rejected. It adds an int_ field, which is a serialization-schema hard stop, for something that should simply be unambiguous.

**Decision: Modify · P2.** Change the colour and label semantics, not the comparand. The obvious fix of switching the comparand trades an over-warning for an under-warning, as the ADR itself notes. A mode-aware severity keeps every exceedance visible and makes red mean 'violated'. The change is display-only in LoudnessMeterView.

It resolves an ADR-recorded open question, so it needs the owner's product call and an ADR amendment rather than a silent code edit.

*Calibration:* the verifier judged Modify / P1; the final judgement is Modify / P2. Challenge accepted: the TP-off red is a true, documented statement next to the SP discriminator; only the TP-mode red within print resolution is false and it needs TP on, so P2. Constrained Modify: apply the SP row's half-print slack to the TP row at least in TP mode, plus an on-row text qualifier. No amber/accent style (conflicts with the CVD palette decision at e769f33:src/gui/LookAndFeel.h:53-60 and the pending accent ratification) and no tooltip-only explanation (tips ship off). Needs an ADR-0020 amendment-2 update and resolves part of ADR-0015's open fine-review question (owner). Genuine TP-mode overs stay red until [DSP-001](findings-dsp-tech.md#dsp-001) lands, which is correct.

*Adversarial challenge:* evidence holds: yes; priority justified: no (suggested P2); decision justified: no (suggested Modify). Priority. The colour sits on an honest number. The adjacent SP row (ADR-0020 D6) already provides the discriminator, and the manual (USER_MANUAL:229-230) documents the behaviour. The only demonstrably false signal is the TP-mode red within print resolution, which needs TP on (not the default). That makes this a clarity improvement, not a material workflow harm: P2 under the rubric.

The 'every session' frequency attaches to the TP-off red, which is truthful: the actual defect there is the default configuration's ISP exposure (ADR-0015 Consequences), not the colour.

The interaction with [DSP-001](findings-dsp-tech.md#dsp-001) is real. Always-red in TP mode masks genuine overs, but the number still shows them (1.27 dBTP in session capture `rt/verify-19/16-os4x-tpon-defaults-lvl6-20s-editor.png`).

Decision. Modify is right in spirit, but the proposed amber/accent downgrade and the tooltip-borne explanation conflict with the recorded palette/CVD decision and the tooltips-off default. Constrain it to the slack fix plus an on-row text qualifier. *Proposal risks:* 1. Amber conflicts with the palette's recorded CVD decision. The 'advisory style (accent/amber)' for TP off contradicts e769f33:src/gui/LookAndFeel.h:53-60: `warn` was moved OFF amber to a desaturated red so that over-ceiling states stay distinguishable from ordinary accent fills under deuteranopia (the §8 colour-blind requirement). accent (0xfff0b432) and accent2 (0xffe07830) are the M/S/I bar gradient right above this row. An amber TP value would read as an ordinary accent state, not a caution, and a third colour between gold and red is the pairing the palette says does not survive CVD. The accent swatch also 'awaits owner ratification (C8-adjacent product identity)', so this touches the brand checklist. The judge named none of this.
2. The tooltip copy is invisible by default. int_tooltipsOn ships off (InternalState.h setDefaults; ADR-0015 cites it as the opt-in pattern), so a row tooltip explains nothing at defaults. Only on-row text can carry the meaning.
3. Per-row tooltips need hit-testing. LoudnessMeterView paints every row in one paint(), so per-row tooltips mean a getTooltip/mouse-position hit test. That is small but not free.
4. Downgrading TP-off salience is the under-warning ADR-0015 (:227-229) declined to trade into. The default ceiling (-0.1) with TP off is the configuration most likely to fail a -1 dBTP delivery spec, and the red TP row is its only continuous on-screen cue.
5. A 0.1 dBTP tolerance in TP mode would be fine against invariant 4. Until [DSP-001](findings-dsp-tech.md#dsp-001) lands, though, it will still show genuine 0.3-1.7 dB overs as red, which is correct.

Smaller change that is sufficient:
(a) Apply the SP row's half-print-resolution slack (0.005 dB) to the TP row, at least in TP mode. This removes the demonstrably false '-0.10 vs -0.10' red. It needs only an ADR-0020 amendment-2 text change, same rationale as :219-226.
(b) Leave the TP-off comparand and colour as warn, and add a non-colour qualifier on the row itself (e.g. an 'ISP' suffix when TP is off). The final TP-off wording remains the owner's ADR-0015 call.

**Architecture gates.**

- ADR-0015 §Consequences open fine-review question (:217-229): resolving it is the owner's product call and needs an ADR amendment record.
- ADR-0020 Amendment 2 (:229-231) records the TP row's exact comparison as deliberate. Changing the TP tolerance conflicts with that clause until it is amended.
- Adding a Settings option for the warn reference would be a Serialization Registry change (hard stop). The proposal avoids it.

**Dependencies.** [DSP-001](findings-dsp-tech.md#dsp-001) (until the TP-mode guarantee holds, a tolerance-based red in TP mode will often be genuinely red); [VIS-008](findings-visualisation.md#vis-008); [VIS-009](findings-visualisation.md#vis-009)

**Acceptance criteria.**

- At defaults (TP off, ceiling -0.10), music driven into the limiter: TP hold > ceiling renders in the advisory style with an ISP/sample-peak qualifier, not in colours::warn. The SP row renders in colours::warn only when SP > ceiling + 0.005 dB.
- TP on: a TP hold whose 2-dp print equals the ceiling (e.g. '-0.10 dBTP' vs '-0.10 dBTP') is never drawn in colours::warn.
- TP on: a TP hold above the ceiling by more than the chosen tolerance is drawn in colours::warn in both Simple and Advanced views.
- A state-level test pins the three cases (TP off over, TP on within tolerance, TP on over) through the view's colour selection.
- ADR-0015's open question is marked answered, and ADR-0020's amendment text matches the implemented comparison.

<details><summary>Verification record</summary>

**Method.** Code read at e769f33:
- LoudnessMeterView.cpp:133 (the live ceiling is read every tick), :206-215 (statRow has a single binary warn colour), :229-244 (a TP comparison that is mode-blind and exact), :265-267 (the SP row has 0.005 dB of slack).
- ADR-0015:217-229 (open fine-review question).
- ADR-0020:111-118 and :229-231 (the amendment keeps the TP row's comparison exact on purpose).
- USER_MANUAL.md:229-230.

Screenshots viewed: visuals/28a, 28b; edges/40a, 49g.

Reproduced on :149 with stepped-motion clicks on the panel to reset:
- R1: defaults, music -6. TP 0.62 dBTP red, SP -0.10 white.
- R2: Loudness 50, music -18. TP 0.12 red, SP white.
- R4: TP on. -0.09 red.
- R5: TP on, Transients 0. The row printed '-0.10 dBTP' in red against a '-0.10 dBTP' ceiling.

The probe gave the same results with the real engine.

**Corrections to the candidate claim.** The behaviour does not contradict the manual. USER_MANUAL:229 documents 'turns red above your Ceiling', and with TP off the number really is an inter-sample over.

The defect is that one colour carries two meanings, and that it is red in every limiting configuration tested at a -0.1 ceiling:
- TP off: 0.04-0.62 dBTP.
- TP on, Transients 0: the value printed equals the ceiling, yet it is red, because the comparison is exact.

With TP on, some of the red is genuine over-tolerance output ([DSP-001](findings-dsp-tech.md#dsp-001)), not only a missing tolerance. The observers' line cites (visuals.md V-12 :370-371/:392-394, edges.md E13 :245-252) do not exist at e769f33. The correct anchors are :243-244 and :265-267.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 5 · severity 3 · discoverability 3 · efficiency 2 · coherence 4 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-003

**Every gain-reduction display outside the Advanced COMP lane is limiter-only and unattributed: the clipper's reduction (which the Loudness macro engages) and the compressor's never appear in the GR history or anywhere in Simple**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | partially-confirmed | Visualization | The graphs are qualitative and partly mis-calibrated | Phase 2 |

**Evidence**

- e769f33:src/dsp/AnabasisEngine.cpp:1034-1042 — 'this is the LIMITER's reduction, not the chain's … pubGrDb, the GR history ring and the §2.7 predict floor … all describe the limiter alone … A P5 item (the meter legend has to say which reduction it is showing)'
- e769f33:src/dsp/AnabasisEngine.cpp:1056 (grMinChunk folds limiter gains[] only) and :769-775 (histMinGain -> grHistory->push)
- e769f33:src/gui/PluginEditor.cpp:1793-1795 — compGrMeter/limGrMeter visible only when adv
- e769f33:src/MacroEngine.h:51-54 — macro drives limGain 0-18 dB, compThreshold 0->-12 dB, clipDrive 0->9 dB above Loudness 30 %
- e769f33:src/gui/CurveView.h:20-23 vs e769f33:src/dsp/AnabasisEngine.cpp:1040-1042 (contradictory status of the legend item)
- e769f33:docs/architecture/design-decisions/ADR-0023-012-field-fix-contracts.md:254-258 (decision 10: history ring, pubGrDb and predict floor keep reading the combined limiter figure); :82-83 (decision 4)
- e769f33:docs/user/USER_MANUAL.md:101-102 (first push judged from the GR history) vs :236-237 ('how hard and how often the limiter is working')
- V-02: session capture `rt/visuals/12b-adv-comp-only-editor.png` and 12b-adv-comp-only-comp-panel.png — COMP lane ~22 % (about 5 dB) while the well trace stays flat
- verify-20 R1: session capture `rt/verify-20/01-adv-loud70-music6.png` (macro clip drive 5.1 dB: trace median 5.1 dB, p90 6.4 dB) vs session capture `rt/verify-20/clip0-full.png` (clipDrive 0: median 8.6 dB, p90 12.5 dB)
- E13: session capture `rt/edges/40a-square50-plus6-simple.png` — Loudness 0, +6 dBFS square, trace ~6 dB (correct limiter work)

**Current behaviour.** In both views the GR history, and the unread pubGrDb, show only the limiter's per-block deepest-channel reduction. Compressor reduction appears only in the Advanced COMP lane. Clipper reduction appears nowhere; no figure for it exists. Nothing on screen says the trace is the limiter's.

**Problem.** The product's one processing display in Simple leaves out a stage the Loudness macro turns on above 30 %. The manual's first-push step asks the user to judge 'steady, musical work' from that display. With the clipper absorbing several dB of peaks, the trace looks modest while the chain is working much harder than shown.

**Root cause.** The GR tap is limiter gains[] alone, because the same figure feeds the §2.7 predict floor (AnabasisEngine.cpp:700-702). ADR-0023 decision 10 kept the history ring on that combined limiter figure. The legend item was treated as answered by the Advanced-only per-stage lanes. ClipSat is a waveshaper with no reduction measurement at all.

**User impact.** A Simple-view user at Loudness 50-100 % under-reads the chain's total peak reduction by several dB (about 3.5 dB median and 6 dB p90 on the test programme at Loudness 70). They may push further than intended, or misjudge where the distortion comes from. An Advanced user sees the comp in its lane, but still gets no clipper figure. *Scope:* GR history in both views (GrHistoryView); Simple view entirely; engine metering taps (AnabasisEngine processChunk / process); manual §2.4 and §3.4.

**Proposed improvement.** Target: the user can tell which stage each GR trace belongs to, and in both views can see clipper (and compressor) reduction next to the limiter's. Step 1, small: an attribution legend inside the graph well in both views, naming what the trace is. The maintainer specifies the wording (AI_AGENT_POLICY C8). Step 2: publish a per-entry clipper peak-reduction figure (the dB by which the clip stage lowered the chunk's peak, from its input and output peaks) and the compressor's effective reduction. Draw them as distinct, legend-keyed traces or a stacked band in the history. Leave the limiter trace's source and the §2.7 predict-floor input (grDbNow) unchanged. Simple then shows limiter plus clipper, with the compressor at least in the combined band.

**Alternatives considered.**

- *Rewire the existing history/pubGrDb tap to total-chain reduction* — Rejected. The same tap feeds the §2.7 predict floor, so this would change MATCH's monitor gain. It also contradicts ADR-0023 decision 10.
- *Legend only (step 1 alone)* — Acceptable as an interim. It makes the display honest, but the under-report stays in Simple.
- *Show the Advanced COMP/LIMITER lanes in Simple too* — Adds per-channel detail to a view built around one knob, and the clipper is still missing.
- *Leave as-is and rely on the manual's 'limiter' wording* — Rejected. Nothing on screen carries the attribution, and the manual's own first-push step leans on this display.

**Decision: Modify · P2.** The evidence is code-confirmed and reproduced. The obvious fix, retargeting the existing tap to chain GR, would move the predict floor and conflict with ADR-0023 decision 10. The constrained route is attribution first, then separate clip/comp figures, with the limiter tap untouched. The Loudness-0 sub-claim is dropped: that reduction is decided, truthful ceiling work.

*Calibration:* the verifier judged Modify / P1; the final judgement is Modify / P2. Challenge accepted: the limiter-only history matches the documented model and DELTA is the prescribed over-processing check; the comp half needs an Advanced detach; a frequent but low-cost clarity gap, P2. Commit now to attribution only (maintainer wording, C8). Defer clip/comp traces behind an owner metric definition and an ADR-0040 amendment (Slot layout assertion at e769f33:tests/state_tests.cpp:10258; ring storage 2 -> 3-4 MiB); no summed 'chain GR' band. Keep 'limiter trace and predict floor unchanged'. Fix the stale 'read only by the tests' comment (e769f33:src/dsp/AnabasisEngine.cpp:1035-1036).

*Adversarial challenge:* evidence holds: yes; priority justified: no (suggested P2); decision justified: yes (suggested Modify). Evidence is real, reachable and code-confirmed, but P1 is over-rated under the rubric. (a) The limiter-only history matches the documented model, not a silent defect. USER_MANUAL.md:236-237 describes it as 'how hard and how often the limiter is working'. :49-50 and :439-440 tell the user that the clipper absorbs transients before the limiter in the middle of the dial. DESIGN.md:73 and :927 give GR meters to COMP and LIMITER only and a curve view to CLIP. (b) The product's documented over-processing check is DELTA (manual :107-108 and :446), not the GR height, so a misjudgement is caught by the tools the workflow already prescribes. Recovery costs a knob turn or an undo. (c) The compressor half needs a detached Advanced edit to matter. What remains is a frequent but low-cost clarity gap: no on-screen attribution, and a 'GR' pill label that reads as generic reduction. That fits P2, 'lower-cost situation'. Keep Modify, with a narrower commitment: attribution now (maintainer wording), and the clip/comp representation deferred behind an owner metric definition and an ADR-0040 amendment. Acceptance criterion 4 (limiter trace and predict floor unchanged) is sound and should be kept. *Proposal risks:* STEP 2 IS A DSP/ARCHITECTURE CHANGE, NOT A VIEW CHANGE. (1) Adding per-entry clip/comp fields to GrHistoryBuffer::Slot breaks the pinned layout assertion at e769f33:tests/state_tests.cpp:10258 (sizeof(Slot)==2*sizeof(float), 'the ring's size and the push are unchanged'). Storage would grow from the 2.00 MiB per instance recorded in ADR-0040 (§ measured table, 1<<18 slots) to 3-4 MiB, and ADR-0040's measured clear and GUI decimation costs would grow with it. So ADR-0040 needs amending, which the judge did not name. The judge's ADR-0023 d10 flag is arguably misplaced: d10 only says the ring KEEPS reading the combined limiter figure, and an additive field leaves that true. The conditional Thread-Model flag is likely not triggered: the gate's trigger is 'new cross-thread path, new atomic ordering' (ARCHITECTURE_REVIEW_GATE.md:13), and ADR-0020 set the precedent that new relaxed meter atomics are 'no new cross-thread path, so no threading-model change'. (2) 'Clipper peak reduction' has no defined meaning yet. ClipSat is clip + colour + dynamic tame + an unmanaged clipMix blend, so an in/out peak ratio would credit colour and tame effects to 'the clipper' and can go negative. The metric needs an owner definition, not an implementer's. (3) A stacked 'combined band' would add figures on incompatible time bases: comp is an end-of-block ballistic value (AnabasisEngine.cpp:783), the clip figure an instantaneous waveshaper peak ratio, and the limiter figure a per-entry deepest gain. Their sum is not a physical gain and would invite reading it as 'chain GR'. (4) The Simple well is about 91 px tall (3.8 px/dB), and V-03 already reports the single trace buried in a saturated peak fill at high Loudness. More traces worsen legibility exactly where the clipper is active. (5) Legend placement is constrained by ADR-0023 d7's corner reasoning: top-right sits on the newest GR data and bottom-left is the pill. The wording falls under C8. The owner has removed an explanatory UI legend before (USER_MANUAL.md:342, 0.1.3), so whether a legend is wanted at all is the owner's call. SMALLER SUFFICIENT CHANGE: on-screen attribution of the existing trace, plus at most a single clip-activity cue in Simple. Defer the per-stage history traces until the owner has defined the metric and ADR-0040 has been amended.

**Architecture gates.**

- ADR-0023 decision 10 (history ring, pubGrDb and §2.7 predict floor read the combined limiter figure) — step 2 amends it
- ARCHITECTURE_REVIEW_GATE Thread Model change — only if step 2 adds a ring payload field or a new audio->GUI publication beyond the existing 'Meters -> GUI' row (THREAD_MODEL.md; ring reader contract of ADR-0038/ADR-0040)
- Must not alter the §2.7 predict-floor input (grDbNow): doing so changes MATCH monitor gain

**Dependencies.** [VIS-007](findings-visualisation.md#vis-007) (the numeric readout must state which reduction it is); [VIS-006](findings-visualisation.md#vis-006) (the legend belongs to the same annotation layer); E13 (input-over cue for the Loudness-0 case); AI_AGENT_POLICY C8: legend wording to be specified by the maintainer

**Acceptance criteria.**

- Simple and Advanced: the graph well carries a visible label or legend naming the limiter trace, in maintainer-approved wording.
- Advanced, music -6 dB, Loudness 70: a clipper-reduction representation is non-zero in the well, and it returns to zero when clipDrive is set to 0 dB.
- With compThreshold -30 dB and the COMP lane at about 5 dB, the well (in Simple too) shows non-zero compressor reduction distinct from the limiter trace.
- The limiter trace values and the §2.7 predict floor/MATCH gain are unchanged before and after (existing DSP/state suites green, with no re-baselined expectations for the predict floor).
- Loudness 0 with a +6 dBFS input: the roughly 6 dB shown is attributed to the limiter trace.

<details><summary>Verification record</summary>

**Method.** Read the anchors: e769f33:src/dsp/AnabasisEngine.cpp:1032-1058 and :764-791; e769f33:src/dsp/AnabasisEngine.h:340-364; e769f33:src/gui/PluginEditor.cpp:1576, 1609, 1793-1795, 2093-2097; e769f33:src/MacroEngine.h:51-54; e769f33:src/gui/CurveView.h:20-23; ADR-0023 decisions 4 and 10. Viewed 12b-adv-comp-only-editor.png, 12b-adv-comp-only-comp-panel.png, 12a-adv-loud70-editor.png and 40a-square50-plus6-simple.png. Reproduced on :150 (verify-20, R1): Advanced view, music at -6 dB, Loudness 70 (the macro sets clip drive 5.1 dB, comp -12 dB / 1.85:1, lim gain 11.7 dB), then clipDrive detached to 0 dB. Pixel-scanned the whole 20 s GR trace in each state (9.83 px/dB).

**Corrections to the candidate claim.** (1) At Loudness 70 on this programme the COMP lane read ~0 dB, in 12a and in my R1 capture. The 5 dB of hidden compression in V-02 came from a manual Advanced threshold of -30 dB, not from the macro. The reduction the macro actually hides in Simple is mainly the CLIPPER's (drive 0->9 dB over Loudness 30-100 %, MacroEngine.h:54). Detaching the macro's 5.1 dB clip drive raised the limiter trace from median 5.1 to 8.6 dB and from p90 6.4 to 12.5 dB. So the clipper was taking roughly 3.5-6 dB off peaks, and no display showed it. The size of this depends on the signal (synthetic programme); its direction does not. (2) 'Ceiling work at Loudness 0 looks the same as push' is not a GR defect. The reduction shown is real limiter reduction, which ADR-0023 decision 4 ('Ceiling-definition GR stands') decides. The manual says so (USER_MANUAL.md:170). What is missing there is an input-over cue (E13). (3) ADR-0023:33-36 is Context item 3; the binding clause is decision 4 at :82-83. (4) The code disagrees with itself. The engine comment (AnabasisEngine.cpp:1034-1042) still calls the legend an open P5 item. CurveView.h:20-23 and AnabasisEngine.h:346-349 call the per-stage lanes 'the answer' to 'which reduction is the meter showing', but the lanes exist only in Advanced, and the history in both views stays unattributed.

</details>

<sub>Verifier scores (1-5): impact 4 · frequency 5 · severity 3 · discoverability 4 · efficiency 2 · coherence 4 · change risk 3 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-004

**Under BYPASS the GR history trace and the COMP/LIMITER lanes keep showing processed-path gain reduction over a waveform and meters that switched to the dry signal**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | confirmed | Visualization | The session figures have no visible scope, liveness or tap, and a stray click wipes them | Phase 1 |

**Evidence**

- e769f33:src/dsp/AnabasisEngine.cpp:1032-1033: limiter.processSample produces gains on the processed path
- e769f33:src/dsp/AnabasisEngine.cpp:1056-1058: grMinChunk/grMinChunkCh folded with no bypass term
- e769f33:src/dsp/AnabasisEngine.cpp:769-775: GR history entry = limiter min gain (processed) + histPeak from renderPeakChunk (render tap)
- e769f33:src/dsp/AnabasisEngine.cpp:1305-1308: renderPeakChunk taken from renderFrame (dry at bypassMix>=1, :1262)
- e769f33:src/dsp/AnabasisEngine.cpp:782-790: limiter/comp per-channel GR atomics stored unconditionally
- e769f33:src/gui/PluginEditor.cpp:2094-2097: COMP/LIMITER lanes display those atomics
- e769f33:src/dsp/AnabasisEngine.cpp:1250-1253: the stated rule 'the meters report what the plugin EMITTED' is applied to levels only
- e769f33:docs/user/USER_MANUAL.md:236-237: the history shows 'how hard and how often the limiter is working'; nothing describes bypass
- ST-13: session capture `rt/state/30a-before-bypass.png`, 30b-bypass-1s.png, 30c-bypass-4s.png
- V-04: session capture `rt/visuals/10d-bypass-on-editor.png`
- verify-6: session capture `rt/verify-6/04-well-rightedge-zoom.png`: under bypass the waveform fill drops to the quieter dry level while the GR trace keeps per-kick dips that deepen in the loud section
- verify-6: session capture `rt/verify-6/07-lanes-zoom.png` and 07b-adv-bypass-3s.png: the LIMITER lane shows GR while bypassed (Advanced view)
- verify-6: session capture `rt/verify-6/01-dry-only-16s-editor.png`: 16 s of bypass with GR dips across the whole span

**Current behaviour.** With BYPASS on, the user hears the dry signal. STATISTICS, out LUFS and the history's waveform fill all switch to the dry signal. The GR history trace and the Advanced COMP/LIMITER lanes keep drawing the reduction the (unheard) processed path is applying, all under the dim. After un-bypass, the 20 s history still shows that span as wet GR over a dry waveform.

**Problem.** One display pairs two different signals on one axis: limiting drawn on a waveform that is not being limited, and often on peaks lower than those the limiter is acting on. It contradicts the bypassed state and the product's own emitted-signal metering rule.

**Root cause.** The GR taps (grMinChunk/grMinChunkCh and comp GR) sit on the processed path upstream of the bypass crossfade and are never weighted by bypassMix. The waveform peak and all level meters come from the bypass-mixed render tap.

**User impact.** During the most common comparison, the GR display suggests the plugin is still acting, which undermines trust that bypass is really bypassing. After un-bypass, the recent history mixes two regimes. It does not persist beyond the 20 s window and does not change audio. *Scope:* The GR history (Simple and Advanced), the COMP and LIMITER lanes (Advanced), and pubGrDb. Only during bypass and for the 20 s afterwards in the history.

**Proposed improvement.** Apply the emitted-signal rule to the GR taps. When bypassMix >= 1, publish 0 dB reduction into the history entry, the per-channel limiter and comp lane atomics, and pubGrDb. During the ~10 ms ramp, weight the processed GR by (1 - bypassMix) or keep it, and document the choice. The bypassed span then reads as dry waveform with no reduction. Pair this with the [UX-008](findings-ux.md#ux-008) caption so that a flat trace during bypass reads as 'bypassed', not 'limiter idle'. Add one sentence to USER_MANUAL §3.4 describing what the well shows during bypass.

**Alternatives considered.**

- *Per-entry bypass flag in GrHistoryBuffer and a shaded or labelled span in the history* — The most informative option, and it would also give the event markers V-04 lacks. It changes the SPSC ring payload that ADR-0011 (amended), ADR-0038 and ADR-0040 govern, which is Architecture Review Gate territory. Worth deferring to a history-markers item.
- *View-only: grey the GR stroke and lanes while bypass is on* — Cheap and pure GUI, but after un-bypass the history still pairs wet GR with the dry waveform for 20 s.
- *Keep showing 'what the limiter would do' and document it* — No documented intent supports this reading, and the waveform under it would still be the dry signal.
- *Leave as-is* — Keeps the incoherence on every bypass.

**Decision: Proceed · P2.** Reproduced with realistic input and code-confirmed. The fix is a display-tap change with no audio, order, latency or threading impact. It brings GR into line with the rule the engine already applies to levels.

**Architecture gates.**

- The proposed tap gating touches no hard-stop category: display values only, and the render and output paths are unchanged. The per-entry-flag alternative would change the GrHistoryBuffer payload governed by ADR-0011 (2026-09-02/09-07 amendments), ADR-0038 and ADR-0040, which is a possible 'conflict with an Accepted ADR' needing Architecture Review Gate review if chosen.

**Dependencies.** [UX-008](findings-ux.md#ux-008); [VIS-001](findings-visualisation.md#vis-001)

**Acceptance criteria.**

- Engine test: with bypass=true, after the ramp completes, lastLimGrDbCh(0/1), the comp lane values, and every GR history entry pushed while bypassed read 0 dB; with bypass=false on the same input they match today's values.
- Runtime (music -12, Loudness 60 %): 2 s into BYPASS the history's GR trace over the bypassed span is flat at 0 dB and the LIMITER lane is empty, while the waveform fill still shows the dry peaks.
- On un-bypass, GR reappears within one history entry, with no spike or step artefact beyond the ~10 ms ramp.
- USER_MANUAL §3.4 describes what the graph well and lanes show during bypass.

<details><summary>Verification record</summary>

**Method.** Code read at e769f33: AnabasisEngine.cpp:1032-1033 and :1056-1058 (limiter gains folded before any bypass term), :769-775 (history entry = limiter min gain + render-tap peak), :1308 (renderPeakChunk from renderFrame), :782-790 (lane atomics), :1260-1264 and :1279-1282 (bypass mix), :792 and :1299 (adaptive engine); PluginEditor.cpp:2094-2097 (lanes read the per-channel GR atomics). Reproduced on :136 with stepped clicks, realistic input (music -12, Loudness 60 %): captured the well at 0.5, 2 and 6 s into bypass and 2 s after, zoomed the right edge, and captured the Advanced view with bypass on and off, zooming the LIMITER and COMP lanes.

**Corrections to the candidate claim.** ST-13's 'flat for the first second, then GR dips reappear' was not reproduced. GR dips continued immediately at 0.5 s (04-well-rightedge-zoom.png), so the earlier flat stretch was programme content, not a bypass effect. The adaptive engine does keep running (:792, :1299), but it is fed monFrameDry, i.e. the input, identically bypassed or not; that is not a display inconsistency. The COMP lane showed no GR in either state at this setting, so only the LIMITER lane was observed at runtime; the COMP lane follows by code (:790).

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 4 · severity 2 · discoverability 3 · efficiency 1 · coherence 4 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-005

**When the host stops calling processBlock, the rolling readouts (M, S, RMS, out LUFS), the COMP/LIMITER lanes and the spectrum freeze at their last values with no stale indication**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | medium | partially-confirmed | Feedback/observability | The session figures have no visible scope, liveness or tap, and a stray click wipes them | Phase 1 |

**Evidence**

- e769f33:src/PluginProcessor.cpp:963-964: a short-circuited or absent block publishes nothing; e769f33:src/PluginProcessor.cpp:997-1014 are the only per-block meter publications
- e769f33:src/gui/LoudnessMeterView.cpp:136-146: the tick repaints only on value change and has no liveness or timeout
- e769f33:src/gui/PluginEditor.cpp:2040-2043: out LUFS prints the last published S
- e769f33:src/gui/PluginEditor.cpp:2094-2097: the lanes show the last published GR, with no decay
- e769f33:src/gui/SpectrumView.cpp:394-396: the idle gate keeps the last trace
- e769f33:docs/KNOWN_ISSUES.md:555-563: KI-007 item 6, freeze-vs-decay recorded as an open listening-pass decision
- e769f33:src/gui/GrHistoryView.h:675-679 plus e769f33:docs/user/USER_MANUAL.md:265-267: the GR history park is documented ('Pausing and resuming continues the timeline')
- e769f33:src/PluginProcessor.h:615-617 and e769f33:src/PluginProcessor.cpp:1867-1868: the code treats 'host stopped -> no block runs' as ordinary
- e769f33:src/PluginProcessor.h:138-141 plus JUCE 9.0.1 juce_audio_plugin_client_VST3.cpp:3722-3734 and juce_audio_plugin_client_AU_1.mm:2192-2202: DAW bypass goes to the parameter; suspend clears the buffer without processBlock
- e769f33:CMakeLists.txt:285-290: shipped formats are VST3, AU (Apple) and Standalone
- V-06: session capture `rt/visuals/14e2-hostbypass-on-7s-editor.png`, session capture `rt/visuals/15a-hostbypass-repeat-2s-editor.png`
- E07: [capture](captures/09-host-bypass-frozen.png), session capture `rt/edges/49c-hostbypass-on-t9.png`
- ST-14: session capture `rt/state/30e-hostbypass-1s.png`
- verify-6 proxy: session capture `rt/verify-6/09a-hostbypass-1.5s.png` and 09b-hostbypass-4.5s.png are pixel-identical (bbox None); 09c-hostbypass-off.png resumes

**Current behaviour.** If no processBlock runs, every published meter atomic keeps its last value. STATISTICS M/S/RMS, the Simple-view out LUFS, the COMP/LIMITER lanes and the spectrum trace all stay on screen, looking live. The GR history parks, as documented. Session rows keep their values. Nothing indicates that no audio is being measured.

**Problem.** The manual defines the rolling readouts (M, S, RMS) as 'the last few seconds'. A value persisting for minutes with no audio contradicts that, and it is visually indistinguishable from a steady live signal. The lanes can show a non-zero GR that is not happening. KI-007 records that a frozen spectrum 'can be mistaken for live signal'.

**Root cause.** Publication is push-only from the audio thread with no liveness signal. The views detect only value changes, so a stopped producer and an unchanging signal look the same.

**User impact.** Modest. The user usually stopped playback themselves, so the risk is misreading the last values as current, for example on returning to the screen or when the host suspends processing unseen. It does not affect audio or the session figures. *Scope:* STATISTICS M/S/RMS, out LUFS (Simple), the COMP/LIMITER lanes (Advanced) and the spectrum. Only in hosts or states where processBlock stops being called. Excludes the GR history and the session rows, which behave as documented.

**Proposed improvement.** Derive a liveness signal from an already-published counter, for example the GR ring head the history view already reads; otherwise add one relaxed per-block counter in the existing meter row. After ~0.5 s with no new processed block: M/S/RMS/out LUFS render '-' or a dimmed 'no audio' state, and the lanes empty. I/TP/SP/LRA/PLR keep their values with a subtle 'held' marker. The GR history keeps its documented park. The spectrum's freeze-vs-decay behaviour follows the owner's KI-007 item-6 decision, using the same liveness signal. Live values return on the first new block.

**Alternatives considered.**

- *Decay all readouts and the spectrum to the floor on idle (KI-007 item 6's 'three lines')* — Matches common analyser convention, but it discards the last reading. For the spectrum, this is the owner's open call.
- *Keep values frozen but add a 'STOPPED' badge* — Preserves the last reading and removes the ambiguity. Slightly more UI.
- *Leave as-is until KI-007 item 6 is decided* — Consistent with the record, but it leaves the rolling rows and lanes, which are not part of that open question, misleading.

**Decision: Modify · P2.** The freeze is code-confirmed and reproduced by proxy, but the proxy path is not a shipped host route, and the real-DAW frequency is unverified. The harm is narrower than claimed: the GR history and session rows are fine. A constrained change to the rolling rows and lanes is justified. The spectrum half must wait for the recorded owner decision (KI-007 item 6) rather than be guessed.

**Architecture gates.**

- A new relaxed per-block counter would be one more atomic in THREADING_POLICY's existing audio->message meter row (ADR-0011 permitted paths). That is an existing path kind, not a threading-model change, but it should be checked against ADR-0011. Reusing the GR ring head avoids any new publication. The spectrum's idle behaviour is an open owner decision (KI-007 item 6) and must not be pre-empted.

**Dependencies.** KNOWN_ISSUES KI-007 item 6 (owner listening-pass decision); [UX-002](findings-ux.md#ux-002); [VIS-001](findings-visualisation.md#vis-001)

**Acceptance criteria.**

- Harness: 'hostbypass 1' (processBlock not called) for 2 s leaves M, S, RMS and out LUFS in the defined stale state within 1 s, and the COMP/LIMITER lanes empty.
- In the same run, I, TP, SP, LRA and PLR keep their pre-stop values and show the held marker. The GR history remains parked, with no new entries and no clear.
- 'hostbypass 0': live values and normal rendering return within one editor tick of the first processed block.
- A host feeding silence while stopped shows no behaviour change from today (M/S fall to '-' as now).
- The spectrum idle behaviour matches whatever KI-007 item 6 records as decided, and that record is updated in the same change.

<details><summary>Verification record</summary>

**Method.** Code read at e769f33: PluginProcessor.cpp:963-1014 (publication only after engine.process); LoudnessMeterView.cpp:72-151 (tick compares values, no timeout or decay); PluginEditor.cpp:2037-2044 (out LUFS) and :2094-2097 (lanes, GrMiniMeter with no decay); SpectrumView.cpp:394-396 (idle gate); GrHistoryView.h:675-679 (park); KNOWN_ISSUES.md:555-563 (KI-007 item 6 open); USER_MANUAL.md:265-267 (pause continues the timeline); PluginProcessor.h:138-141 and :615-617; PluginProcessor.cpp:1867-1868. Read the JUCE 9.0.1 wrapper bypass and suspend paths (VST3 :3722-3734, AU_1.mm :2192-2202) and CMakeLists formats. Reproduced the proxy on :136: 'hostbypass 1' left the whole editor pixel-identical over 3 s (09a vs 09b, ImageChops bbox None), and it resumed on 'hostbypass 0' (09c).

**Corrections to the candidate claim.** (1) The harness 'hostbypass' path (processBlockBypassed) is unreachable in the shipped formats. VST3 and AU route a DAW bypass to pid::bypass (the processor states this intent at PluginProcessor.h:138-141), so a DAW bypass is [VIS-001](findings-visualisation.md#vis-001)/VIS-004, not this finding. The harness run is only a proxy for 'processBlock not called'. (2) The real triggers are a host that stops calling processBlock (stopped transport in such hosts; plugin suspend, where the wrappers clear the buffer without calling processBlock). The codebase itself calls a stopped host running no blocks 'the ordinary condition' (PluginProcessor.h:615-617). Which DAWs do this was NOT tested, so it is unverifiable here. (3) The GR-history park is documented behaviour and honest for a time series (USER_MANUAL.md:265-267). Session holds (I, TP, SP, LRA, PLR) staying after a stop is correct session semantics. The misleading part narrows to the rolling readouts, the lanes and the spectrum. The spectrum half is an explicitly open owner decision (KI-007 item 6: 'a listening-pass call'). (4) Hosts that keep calling processBlock with silence while stopped do not freeze: M/S fall to '-' naturally.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 3 · severity 2 · discoverability 3 · efficiency 1 · coherence 3 · change risk 2 · complexity 2 · evidence 3</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-006

**The GR history, spectrum, COMP/LIMITER lanes and clip/EQ curves carry no scale, tick, unit, legend or over-range cue; GR displays pin silently at 24 dB**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | partially-confirmed | Visualization | The graphs are qualitative and partly mis-calibrated | Phase 2 |

**Evidence**

- e769f33:src/gui/GrHistoryView.cpp:543-550 — the GR history paints only a waveFill and a 1.4 px gr stroke; no text or ticks
- e769f33:src/gui/LookAndFeel.h:376 (grSpanDb = 24) and e769f33:src/gui/GrHistoryView.h:132 (kWindowSeconds = 20) — the scale is known only to code and the manual (USER_MANUAL.md:266-269)
- e769f33:src/gui/SpectrumView.cpp:829-830 (20 Hz-20 kHz, -90..0 dB) and :922-927 (two strokes, then the pill; no ruler)
- e769f33:src/gui/CurveView.h:88-114 (GrMiniMeter paints bars only; header :20-23 says 'labelled by its own stage', which on screen is only the panel caption)
- e769f33:src/gui/CurveView.cpp:189-196 (curves: reference line only)
- e769f33:worklogs/2026-08-21-gr-scale-and-percent-entry.md:61 — 'Add dB gridlines / an axis label … Rejected. AI_AGENT_POLICY.md C8'
- Anamorph fd78c3b:src/gui/LevelMeter.cpp:266-279 (dB ruler 0/-6/-12/-18/-24/-48) and :90-98 (numeric dB); fd78c3b:src/gui/SpectrumImager.cpp:1255-1263 (Hz ruler + 'Hz')
- V-01: session capture `rt/visuals/01-music-18-well.png`, session capture `rt/visuals/27d-sine-limgain24-editor.png` — about 9 dB and about 15 dB of GR drawn at y 649/672 with no way to read the dB
- LAY-06: session capture `rt/layout/46-adv-panel-feet-1x.png`, session capture `rt/layout/04c-clip-curve.png` — no tooltip after 4 s on the lanes or curves
- E12: session capture `rt/edges/38e-lim-meter-3x.png` — limiter lane pinned full width, no over-range cue
- V-11: session capture `rt/visuals/06-square200-stats.png` — the bar is full at +2.0 LUFS, but the number beside it reads +2.0 (no information lost)

**Current behaviour.** Every graph is a bare trace or bar on a dark ground. The GR history has 24 dB over the full height and 20 s across, with no ticks. The spectrum has log-f against -90..0 dB, with no Hz or dB marks. The lanes are unlabelled 14 px bars. The curves have only a reference line. A reduction deeper than 24 dB draws at the bottom edge with no marker.

**Problem.** Users cannot read an amount (dB of GR, dB of spectrum level) or a position (frequency, seconds) off any graph. The scale is stated only in the manual's prose.

**Root cause.** The Anabasis visualisers (GrHistoryView, SpectrumView, GrMiniMeter, CurveView) were written without an annotation layer. They did not port the sibling's ruler idiom. When the GR scale was fixed in 0.1.6, labels were set aside under AI_AGENT_POLICY C8 pending maintainer specification, rather than rejected on merit.

**User impact.** On every session, the graphs support only qualitative reading: 'more or less reduction', 'a peak somewhere'. 12 dB and 30 dB of GR look the same at full scale. *Scope:* GrHistoryView (both views), SpectrumView (both views), GrMiniMeter and CurveView (Advanced). The M/S/I bars are excluded (they have numbers).

**Proposed improvement.** Add a fixed, low-contrast annotation layer, overlaid inside each plot so no plot geometry changes. The maintainer specifies the text (C8). GR history: dB ticks at fixed depths (e.g. 0/6/12/24) along one edge, with faint gridlines. Spectrum: Hz ticks (100 / 1k / 10k) and two or three dB ticks. COMP/LIMITER lanes: a stage-GR caption and 0/12/24 dB tick marks. Clip/EQ curves: minimal axis ticks. Over-range: when a GR entry or lane value is deeper than grSpanDb, draw a distinct cap marker at the bottom edge or lane end. Keep the fixed scales (ADR-0023 decision 6). Add no streaming-target lines (owner directive, LoudnessMeterView.h:33-39).

**Alternatives considered.**

- *Hover readouts (dB/Hz/time under the pointer) instead of static ticks* — A useful complement. It needs mouse handling on views that are currently non-interactive (apart from the pill), and it does nothing for glanceable reading.
- *Port Anamorph's ruler code (ADR-0009 reuse path)* — A good source of the idiom (LevelMeter ruler, SpectrumImager Hz ruler). Reuse is a product-family decision recorded in an ADR.
- *Leave the scale in the manual only* — Rejected. The manual is not on screen, and nothing tells a user that full height means 24 dB.
- *Add ticks to the M/S/I bars as well* — Not justified: the numeric value beside each bar already carries the reading.

**Decision: Modify · P2.** The absence is real and code-confirmed. The constrained version is to annotate only the graphs that have no number, overlay without changing geometry, and keep fixed scales and the no-targets directive. It waits on maintainer-specified wording because of C8, but that is a copy spec, not a design block.

**Dependencies.** [VIS-007](findings-visualisation.md#vis-007) (the numeric GR readout covers the highest-value half; the GR ticks should match it); [VIS-003](findings-visualisation.md#vis-003) (the legend naming each GR trace); [VIS-018](findings-visualisation.md#vis-018) (lane caption and tick semantics); AI_AGENT_POLICY C8 maintainer copy spec

**Acceptance criteria.**

- The GR history in both views shows at least three labelled dB ticks at fixed depths, legible at UI scale XS, and still aligned when the Advanced well re-lays out to its taller size.
- SPEC view shows labelled 100 Hz / 1 kHz / 10 kHz positions and at least two dB levels.
- The COMP and LIMITER lanes carry a GR caption and scale marks; an entry deeper than 24 dB shows a distinct over-range marker in both the history and the lanes.
- The plot area geometry is unchanged: the existing GrHistoryView/SpectrumView geometry tests pass without re-baselining, and the GR|SPEC pill stays readable.
- No streaming or platform target lines are introduced.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/GrHistoryView.cpp:536-550, e769f33:src/gui/SpectrumView.cpp:826-932, e769f33:src/gui/CurveView.h:88-114, e769f33:src/gui/CurveView.cpp:189-196 and e769f33:src/gui/LoudnessMeterView.cpp:160-196. Grepped drawText/drawFittedText in GrHistoryView, SpectrumView and CurveView: no hits. Grepped setTooltip in src/gui: none on these views. Compared the sibling at Anamorph fd78c3b (LevelMeter.cpp, SpectrumImager.cpp). Read worklogs/2026-08-21-gr-scale-and-percent-entry.md:57-61 and AI_AGENT_POLICY.md:29-33. Viewed 01-music-18-well.png, 27d-sine-limgain24-editor.png and 38e-lim-meter-3x.png, and my own Advanced captures on :150.

**Corrections to the candidate claim.** (1) The M/S/I bars are not scale-less readouts. Each has its numeric LUFS value beside it (LoudnessMeterView.cpp:184), so the missing ticks and the +2.0 LUFS full bar (V-11) lose no information. The bars are low-harm and out of the proposal. (2) The curves have a reference line (unity diagonal / 0 dB line, CurveView.cpp:189-196), though no ticks or labels. (3) The suspected root cause is refuted. The sibling is not unlabelled: Anamorph's LevelMeter draws a dB ruler and numeric dB readouts (Anamorph fd78c3b:src/gui/LevelMeter.cpp:90-98, 266-279), and SpectrumImager draws a Hz ruler (fd78c3b:src/gui/SpectrumImager.cpp:1255-1263). Anabasis's SpectrumView adopted SpectrumImager's column-read rule (SpectrumView.cpp:832-834) but not its ruler. For the GR history specifically, dB gridlines and an axis label were considered in 0.1.6 and rejected only under AI_AGENT_POLICY C8 (UI text and furniture are maintainer-specified); the span went into the manual instead. (4) Pinning past 24 dB needs extreme settings (Loudness 100 gives only 18 dB limGain; E12 also used +24 dB input gain), so the over-range part is an edge case. (5) The fill and stroke are at GrHistoryView.cpp:543-550.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 5 · severity 2 · discoverability 4 · efficiency 2 · coherence 3 · change risk 2 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-007

**There is no numeric gain-reduction readout, current or peak, for any stage in either view**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P1** | medium | confirmed | Visualization | The graphs are qualitative and partly mis-calibrated | Phase 2 |

**Evidence**

- e769f33:src/PluginProcessor.cpp:1016-1021 — pubGrDb published every block as 'The §2.9 GR METER'
- e769f33:src/PluginProcessor.h:532 — meterGrDb() has no GUI caller (grep: only a comment at e769f33:src/gui/PluginEditor.cpp:2084)
- e769f33:src/gui/CurveView.h:88-114 — lanes draw bars only
- e769f33:src/gui/LoudnessMeterView.cpp:160-215 — STATISTICS rows M/S/I/TP/SP/RMS/LRA/PLR, no GR
- e769f33:src/dsp/GrHistoryBuffer.h:18-20 — ring reads are stateless const peeks; 'any number of message-thread/GL-paint read sites stay safe'
- e769f33:docs/user/USER_MANUAL.md:101-102 — first push is judged from the GR history
- e769f33:docs/DEVELOPMENT_BRIEF.md:34 (main view focus is GR history + LUFS metering) and :151 (GR history, Pro-L 2 as reference)
- V-01 / V-17: session capture `rt/visuals/11a-adv-on-editor.png`, session capture `rt/visuals/12a-adv-loud70-lim-panel.png` — no GR number in either view
- E12: session capture `rt/edges/38e-advanced-all-extremes-crop.png`
- verify-20 R1: session capture `rt/verify-20/01-adv-loud70-music6.png` — about 5 dB median GR, readable only by pixel measurement

**Current behaviour.** No stage's reduction is ever shown as a number. The only GR information is the trace position in the well (3.8 px/dB in Simple, 9.8 px/dB in Advanced) and the 14 px lane fill in Advanced. The per-block figure that was published for a GR meter is computed and never read.

**Problem.** The most basic maximizer reading, 'how many dB am I limiting, now and at peak', cannot be stated or targeted in either view.

**Root cause.** The GR metering was built as a history trace plus (Advanced-only) bar lanes with no text layer. pubGrDb was kept but lost its only consumer when the lanes moved to per-channel atomics in 0.1.2 (PluginEditor.cpp:2083-2090, AnabasisEngine.h:405-412).

**User impact.** On every mastering pass the user must estimate GR from trace height. They cannot compare passes, cannot reproduce a setting ('3 dB of limiting'), and cannot tell 9 dB from 15 dB (27d). *Scope:* Graph well (both views); optionally the Advanced lanes.

**Proposed improvement.** A compact numeric GR readout in a corner of the graph well, visible in both views at least in GR mode. It shows the current reduction (deepest over roughly the last 300 ms) and a peak-hold reduction (held until the STATISTICS reset, or over the visible window), in dB to 0.1. Compute it on the GUI side from the existing GrHistoryBuffer by peeking the newest entries under the existing epoch bracket. That needs no DSP change and no new cross-thread path, and it sees every entry, unlike the 24 Hz sampled per-block atomics. Its label states the stage ([VIS-003](findings-visualisation.md#vis-003)). The maintainer specifies the wording (C8). Advanced lanes may add the same figure per stage later.

**Alternatives considered.**

- *A ninth 'GR' row in the STATISTICS panel* — A natural place, but ADR-0020 decision 6 fixes the panel at M/S/I plus five numeric rows, identical in both views. It needs an ADR amendment.
- *Read pubGrDb in the editor timer and hold peaks GUI-side* — Simpler, but it samples one processBlock call per 24 Hz tick and misses most blocks' minima (see [VIS-018](findings-visualisation.md#vis-018)). The ring is the better source.
- *Numbers in the Advanced lanes only* — Leaves Simple, the primary audience, without the reading.
- *Leave as-is* — Rejected. The core maximizer reading is absent.

**Decision: Modify · P1.** Confirmed by code and runtime. The data exists and the ring contract already permits extra GUI readers, so this is a low-risk view-layer addition with high value. It is scoped to the graph well to avoid touching ADR-0020.

*Calibration:* the verifier judged Proceed / P1; the final judgement is Modify / P1. Challenge agreed P1 (no quantitative GR reading exists in either view for the core maximizer quantity, on every pass) but changed Proceed->Modify: current GR = deepest over ~300 ms of ring entries plus peak over the visible window, both ring-derived; or a processor-side hold beside samplePeakMaxHold cleared by requestMeterReset (ADR-0020 precedent, not a gate). Drop the self-contradictory 'no new atomic + reset-coupled peak' criteria. Placement must respect ADR-0023 d7's corner analysis; needs a no-data rule on ring-head stall; label is maintainer copy. Higher than [VIS-003](findings-visualisation.md#vis-003)/VIS-011 at equal judge scores because those challenges showed documented alternatives and small real deltas; this one has none. Confidence medium: if [VIS-006](findings-visualisation.md#vis-006)'s GR ticks land first the residue is P2.

*Adversarial challenge:* evidence holds: yes; priority justified: yes (suggested P1); decision justified: no (suggested Modify). The absence is real and code-confirmed, and it affects every mastering pass. The well has no dB scale either (V-01), so no quantitative GR reading exists anywhere in either view. P1 is defensible for the core maximizer reading. Caveat: if [VIS-006](findings-visualisation.md#vis-006)'s scale ticks land first, what remains of this item is P2. The finding should be addressed, but 'Proceed' with the proposal as written is not justified. Its peak-hold contract cannot be met under its own 'no new atomic' constraint when resets come from state loads or the editor reopens, and its 'corner of the well' placement ignores ADR-0023 d7's corner analysis. Modify: current GR (deepest over about 300 ms of ring entries) plus peak over the visible window, both ring-derived. Or, if a session peak is wanted, a processor-side hold on the existing meter row, following the ADR-0020 precedent. The placement should respect d7 and the maintainer supplies the wording. *Proposal risks:* (1) The acceptance criteria contradict each other. 'Peak holds until the STATISTICS reset and clears on that reset' cannot be met together with 'no new audio-thread publication or atomic'. Resets also come from the processor: setStateInformation calls requestMeterReset (PluginProcessor.cpp:1871), and no reset generation is published that the editor could observe. A GUI-side hold also dies when the editor closes, while the adjacent TP/SP holds are processor-side session holds (PluginProcessor.h:672-678). Two neighbouring 'peak' readouts would then have different lifetimes. Two fixes: (a) define peak as the maximum over the visible 20 s window, derived from the ring with no reset coupling; or (b) add a processor-side GR hold beside samplePeakMaxHold, cleared by requestMeterReset. By ADR-0020's precedent ('five new published atomics … no new cross-thread path, so no threading-model change'), option (b) is not a gate item, so the self-imposed 'no new atomic' criterion should go. (2) Placement: ADR-0023 d7 moved the pill to bottom-left because top-right 'sat on the newest GR data'. The remaining corners hold either the newest data (top-right, bottom-right) or the trace start (top-left). The Simple well is about 91 px tall, and the readout would also sit over the spectrum in SPEC mode. Simple has free space beside 'out LUFS' (40a), and the STATISTICS panel has space below PLR, but that panel is locked by ADR-0020 d6 as the judge noted. (3) Staleness: a ring-derived 'current' figure freezes under host bypass (V-06) and keeps its last value with no audio, so it needs a no-data rule keyed on the ring head not advancing. (4) C8: the label and the zero/no-data forms are maintainer copy. (5) Until [VIS-003](findings-visualisation.md#vis-003)'s attribution lands, a bare 'GR' number carries the same limiter-only ambiguity and understates chain work above Loudness 30 %.

**Architecture gates.**

- ADR-0020 decision 6 (eight-row STATISTICS panel) — only if the readout is placed in the STATISTICS panel; the recommended graph-well placement does not touch it

**Dependencies.** [VIS-003](findings-visualisation.md#vis-003) (which reduction the number states); [VIS-006](findings-visualisation.md#vis-006) (ticks on the history should agree with the number); AI_AGENT_POLICY C8 maintainer copy spec

**Acceptance criteria.**

- Simple and Advanced: with a -3 dBFS 1 kHz sine and limGain +12 dB, the readout shows current GR within 0.3 dB of the engine's value (about 8.9 dB), and within 0.3 dB of about 14.9 dB at +18 dB.
- The peak readout holds the deepest reduction seen since the last STATISTICS reset, clears on that reset, and is never shallower than the deepest ring entry in its span.
- With no reduction, the readout shows 0.0 (or the maintainer-specified zero form), not blank. With no processed audio it shows the no-data form.
- The readout is legible at UI scale XS and does not overlap the GR|SPEC pill or the newest data at the right edge.
- No new audio-thread publication or atomic is added (the readout is derived from existing ring peeks).

<details><summary>Verification record</summary>

**Method.** Grepped meterGrDb, pubGrDb and meterCompGrDb across src. The only GUI mention is the comment at e769f33:src/gui/PluginEditor.cpp:2084. Read e769f33:src/PluginProcessor.cpp:1016-1021, e769f33:src/PluginProcessor.h:532, 545-549, e769f33:src/gui/CurveView.h:88-114, e769f33:src/gui/LoudnessMeterView.cpp:160-215 (no GR row) and e769f33:src/dsp/GrHistoryBuffer.h:8-32 (reader contract). Viewed 12a-adv-loud70-lim-panel/editor and 27d-sine-limgain24-editor. My :150 captures (verify-20) show no GR number in Advanced, where I had to pixel-scan to get dB values.

**Corrections to the candidate claim.** The processor comment calls pubGrDb 'The §2.9 GR METER' (PluginProcessor.cpp:1016-1019), and DESIGN.md's wireframes place a '[GR meter]' in the COMP and LIMITER zones (DESIGN.md:73, :927). The lanes implement those meters without numbers. That the final UI deliberately 'replaced' a numeric meter is not documented; only the absence is established.

</details>

<sub>Verifier scores (1-5): impact 4 · frequency 5 · severity 3 · discoverability 5 · efficiency 3 · coherence 3 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-008

**Peak holds are judged against the current ceiling and TP mode: lowering the ceiling paints earlier legal holds red (SP included) until a manual reset, and raising it hides overs recorded under the old ceiling**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Visualization | The dBTP delivery ceiling is claimed but not enforced, and its warning cannot tell an over from normal operation | Phase 0 |

**Evidence**

- e769f33:src/gui/LoudnessMeterView.cpp:133 — `ceil` is read live every tick. :141/:147-148 — shownCeiling is re-snapshotted.
- e769f33:src/gui/LoudnessMeterView.cpp:243-244, :266-267 — the TP and SP session-max holds are compared with the live ceiling.
- e769f33:src/gui/LoudnessMeterView.cpp:67-70 (click → requestMeterReset); e769f33:src/PluginProcessor.cpp:807, :926-931, :1871 — the only reset sites.
- e769f33:src/PluginProcessor.h:627-650 — requestMeterReset clears ALL meters, including integrated LUFS and LRA.
- e769f33:docs/architecture/design-decisions/ADR-0020-waveform-statistics-panel.md:113-116 — SP warn means 'clamp genuinely exceeded'.
- e769f33:src/InternalState.h:110 — int_tooltipsOn defaults false. e769f33:docs/user/USER_MANUAL.md:271 — click to reset.
- E12: session capture `rt/edges/38b-ceiling-min-simple.png` — ceiling -20, SP -0.10 and TP 0.07 held in red.
- G-17: session capture `rt/gestures/33b-tp-on-simple.png`, session capture `rt/gestures/33c-tp-on-adv.png` — TP toggled on, stale 1.18 dBTP red, and no row changes.
- verify-19 R3: session capture `rt/verify-19/03-tp-engaged-noreset-editor.png` — the TP-off hold of 0.12 stays red after engaging TP.
- verify-19 R8: .../rt/verify-19/20-23-grid.png:
- top-left: ceiling -0.10, TP 0.04 red, SP -0.10 white.
- top-right: 0.8 s after ceiling → -1.0, SP -0.10 RED.
- bottom-left: after reset, TP -0.85 red, SP -1.00.
- bottom-right: 0.8 s after ceiling → -0.10, the same TP -0.85 WHITE.

**Current behaviour.** The TP and SP rows are session max-holds that persist until a panel click, a state load or a re-prepare. Their warn colours are recomputed every frame against whatever the Ceiling and mode are now.
- After lowering the ceiling, holds recorded legally under the old one turn red. The SP row's red is defined as 'clamp exceeded'.
- After raising the ceiling, overs recorded under the old one turn white.
- Engaging TP to fix a red TP row changes nothing on the row.

**Problem.** The colour judges old measurements against a new setting. The warning therefore reports failures that did not happen and hides ones that did, right after the user makes a correction. No indication shows that the holds predate the change.

**Root cause.** The view stores only scalar session maxima (published by the audio thread) and a live ceiling. Nothing records which ceiling or mode a hold was measured under. A ceiling or mode change does not re-scope or mark the holds, and the only reset path (requestMeterReset) also wipes integrated LUFS and LRA, so auto-resetting is not a clean option.

**User impact.** A user who lowers the ceiling or engages TP to fix an over sees the warning stay red, and SP turns red too, so the fix appears to have failed or broken the clamp. They may keep lowering the ceiling and lose loudness.

A user who raises the ceiling has earlier overs silently absolved. Recovery is a click, but only if they know the panel is clickable; the tooltip is off by default. *Scope:* The Statistics panel TP and SP rows in both views. It triggers on any ceiling change (knob, automation, typed value), a TP toggle and, by code, preset or A/B loads with a different ceiling.

**Proposed improvement.** A GUI-only change in LoudnessMeterView:
1. Record the ceiling value and TP mode in force at the last reset, and whenever a hold rises. When the current ceiling or mode differs from those, stop judging the holds. Render TP and SP in a neutral 'stale' style (dimmed) with an inline hint such as 'held · ceiling changed — click to reset', instead of warn or white.
2. Optionally track in the view the maximum exceedance (hold − ceiling in force) observed tick by tick, and warn on that, so an over recorded under the old ceiling stays flagged after the ceiling is raised.
3. Make reset discoverable without tooltips: a small reset glyph or 'click to reset' affordance on the STATISTICS header.

**Alternatives considered.**

- *Auto-call requestMeterReset() on every ceiling or TP change.* — Rejected. It wipes integrated LUFS and LRA (PluginProcessor.h:627-650), destroying a full-programme measurement on a knob nudge. A knob drag would also reset continuously.
- *Audio-thread per-hold 'exceedance vs ceiling in force' atomics.* — Most accurate, but it adds published atomics and a meter-row edit in THREAD_MODEL, which is a Thread Model change needing review. That is disproportionate to a display question.
- *Leave as-is and rely on manual reset.* — Rejected. The stale SP red directly contradicts the SP row's documented meaning, and the reset affordance is undiscoverable at defaults.

**Decision: Modify · P2.** The defect lives entirely in how the view colours held values, so a view-only 'stale' treatment fixes the false judgement with no DSP, threading or serialization impact. The obvious engine-side fix (a reset on change or new published holds) is either destructive or gated.

**Dependencies.** [VIS-009](findings-visualisation.md#vis-009) (hold reset semantics); [VIS-002](findings-visualisation.md#vis-002) (warn colour semantics; the stale style must fit the same scheme)

**Acceptance criteria.**

- With a TP/SP hold recorded at ceiling -0.10, setting the ceiling to -1.0 does not render SP or TP in colours::warn. Both render in the stale style with a visible reset hint until the panel is clicked.
- With a TP hold that exceeded ceiling -1.0, raising the ceiling to -0.10 does not render that hold as plain white/valid. It is either still flagged as an earlier over or shown as stale.
- Toggling TP on or off marks the existing holds stale in the same way.
- Clicking the panel clears the stale state, and holds measured afterwards are judged normally against the live ceiling.
- A reset affordance is visible on the Statistics panel with tooltips disabled (the default).

<details><summary>Verification record</summary>

**Method.** Code read at e769f33:
- LoudnessMeterView.cpp:133, :141, :147-148 (the live ceiling is snapshotted every tick), :243-244 and :266-267 (both warns compare holds with the live ceiling).
- The only hold resets are the panel click (LoudnessMeterView.cpp:67-70), setStateInformation (PluginProcessor.cpp:1871) and prepareToPlay (PluginProcessor.cpp:807), all through requestMeterReset (PluginProcessor.h:627-650), plus the block-top consume (PluginProcessor.cpp:926-931).

Screenshots viewed: edges/38b, gestures/33b, 33c.

Reproduced on :149 (stepped clicks):
- R3: engaged TP with a 0.12 dBTP hold from TP-off. It stayed red, with no change on the row.
- R8: ceiling -0.10 → -1.0 via paramtext. The SP hold -0.10 turned RED immediately. After a reset at -1.0, TP read -0.85 red (a genuine over). Raising the ceiling back to -0.10 turned the same -0.85 hold WHITE.

**Corrections to the candidate claim.** Confirmed in both directions.

Additional consequence: after lowering the ceiling, the SP row turns red. ADR-0020 §Decision 6 (:113-116) defines that as 'the clamp was genuinely exceeded', so the stale hold makes the one violation-grade row assert a failure that did not happen.

From code, preset loads and A/B switches also do not reset the holds (only the three requestMeterReset sites do), so a preset or slot with a different ceiling re-judges old holds too. This was not reproduced at runtime.

The manual documents the click-to-reset (USER_MANUAL:271), but the panel tooltip that says so is off by default (InternalState.h:110).

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 3 · severity 3 · discoverability 4 · efficiency 2 · coherence 4 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-009

**The scope of the session measurement is invisible and inconsistent: I, LRA, TP, SP and PLR span A/B switches, preset loads and bypassed passages, but reset on a host state load and on every prepareToPlay, while the GR history follows a third rule**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | State management | The session figures have no visible scope, liveness or tap, and a stray click wipes them | Phase 1 |

**Evidence**

- e769f33:src/gui/LoudnessMeterView.cpp:69 and e769f33:src/PluginProcessor.cpp:1871: the only two requestMeterReset callers (click, state load)
- e769f33:src/PluginProcessor.cpp:776 (engine.prepare), :807 (dbTpMaxHold = samplePeakMaxHold = -144), :812 (publishSilentMeters): every prepare clears the session holds
- e769f33:src/dsp/AnabasisEngine.cpp:131-132 and e769f33:src/dsp/LoudnessMeter.h:61-80: outMeter.prepare leads to reset(), clearing the histograms and rolling windows unconditionally
- e769f33:src/dsp/GrHistoryBuffer.h:217-224: the GR ring survives a same-pair re-prepare; e769f33:docs/architecture/design-decisions/ADR-0023-012-field-fix-contracts.md:99-102 ('a transport-start re-prepare keeps the timeline')
- e769f33:src/PluginProcessor.cpp:778-781: 'Hosts re-prepare on transport start'
- e769f33:docs/architecture/THREAD_MODEL.md:316-319: 'a transport stop must not clear a mastering measurement … hosts that need a flush re-prepare, which reaches everything'
- e769f33:src/dsp/AnabasisEngine.cpp:1250-1263: the render tap includes duck fades and, with bypassMix >= 1, the dry signal
- e769f33:src/PluginProcessor.cpp:1846: a state load also clears the undo history
- e769f33:src/gui/LoudnessMeterView.cpp:153-281: no elapsed or 'since' indicator in the panel
- e769f33:docs/user/USER_MANUAL.md:271-274 (reset 'after changing the section'), :434 ('Compare candidates with A/B + Copy, judge PLR', no reset step), :265-267 (GR timeline continues on pause)
- Runtime V-08: session capture `rt/visuals/10b-after-AB-editor.png`; session capture `rt/visuals/10c2-after-preset-next-3s-editor.png` (after preset load I reads -3.9 while S reads -13.4, a 9.5 LU gap carried from earlier material); session capture `rt/visuals/17b-after-state-load-0.8s-editor.png`; session capture `rt/visuals/12c-sr96k-1.5s-wellstats.png`
- Runtime V-04: the GR history is not cleared by A/B, preset, bypass or state load; it is cleared only by a pair change
- Runtime ST-16 session capture `rt/state/32d-after-load.png` and ST-17 session capture `rt/state/33-freeze-sr-composite.png`
- Runtime V-05 session capture `rt/visuals/10d3-bypass-off-editor.png`: after plugin BYPASS the SP hold reads +5.53 dBFS on a -0.10 dBFS-clamped master
- Runtime verify-1 R4 (display :131): session capture `rt/verify-1/33-sameprep-strip.png`. A same-pair re-prepare continues the GR trace but restarts I/S/LRA/TP

**Current behaviour.** The session statistics (I in both standards, LRA, the TP and SP holds, and PLR derived from them) reset on:
- a click on the panel;
- a host state load (setStateInformation);
- every prepareToPlay, including a re-prepare at the same rate and block size.
They do not reset on:
- an A/B switch;
- a preset load from the browser;
- plugin BYPASS (the dry signal then accumulates into the holds);
- a change of integrated standard.
The rolling M/S/RMS windows reset only on prepare. The GR history clears only when the (rate, block) pair changes, and survives clicks, loads and same-pair re-prepares. Nothing on screen says when the current measurement began, how much audio it covers, or whether settings changed during it.

**Problem.** The number a user reads for delivery (I, and PLR for dynamics) has an invisible and host-dependent scope. On hosts that re-prepare on transport start, stop and play restarts the statistics while the GR trace beside them continues. The documented A/B workflow then reads PLR, which is a max-hold and an integral spanning both slots, and nothing marks the mixture.

**Root cause.** The session accumulators are tied to lifecycle events rather than to the user's measurement:
- prepare treats the output LoudnessMeter as DSP state and resets it unconditionally (AnabasisEngine.cpp:131 leading to LoudnessMeter.h:66), and the wrapper clears the holds at PluginProcessor.cpp:807. There is no same-pair gate like the one GrHistoryBuffer::prepare got in 0.1.2 (ADR-0023 item 6).
- The only deliberate reset points are the click and the state load, chosen in P5 (THREAD_MODEL.md:306-307).
- The view has no notion of measurement start, duration or segment, so the scope cannot be shown.

**User impact.** A user can:
- read I or PLR after comparing presets or slots and get a figure polluted by the other candidate or by earlier material (10c2: I -3.9 against a -13.4 programme);
- read SP and TP holds that describe the unprocessed input after a bypass comparison;
- lose a long integrated measurement to a stop and start on some hosts, while the adjacent GR history suggests continuity.
In none of these cases can they tell what period the numbers cover, which leads to wrong delivery-loudness or dynamics decisions. *Scope:* All hosts for A/B, preset and bypass mixing. Stop/start loss affects hosts that re-prepare on transport start (asserted by the code, not verified per host). Any sample-rate or block change affects all hosts. Both views.

**Proposed improvement.** Make the scope visible and make the reset rules coherent, without auto-resetting on user actions:

(1) Add a scope readout to the STATISTICS header, e.g. '3:42' or 'since reset 3:42'. It shows the audio time measured since the last session reset, advances only while audio is processed, and returns to 0:00 on every reset path (control, state load, a pair-changing prepare). Source option A is GUI-only: derive it from the GR ring head, since entries x prepared block / rate is processed audio, and snapshot it on reset. Option B is a relaxed atomic on the existing meter row.

(2) Add a quiet 'changed since reset' marker, e.g. a dim '±' or 'mixed' tag beside the readout. It lights when an A/B switch, preset load or plugin BYPASS toggle happens after the last reset and clears on the next reset. This is editor-side; the editor already observes all three events.

(3) Gate the prepare-time clear of the session accumulators on the (rate, block) pair, mirroring GrHistoryBuffer::prepare. A same-pair re-prepare keeps I, LRA and the TP/SP holds, honouring THREAD_MODEL's 'a transport stop must not clear a mastering measurement'. The rolling windows and filter states may still reset.

(4) Do not auto-reset on A/B or preset. That would be a new silent destruction.

(5) Update the manual. §3.4 lists what resets and what does not. §8 step 4 says: reset after switching, then replay the same passage before judging PLR.

**Alternatives considered.**

- *Auto-reset the statistics on every A/B switch and preset load* — Removes the mixing but silently destroys measurements on common actions, which is the same class of trap as [UX-002](findings-ux.md#ux-002). Rejected.
- *Per-slot (A/B) accumulators* — Accurate per candidate, but duplicates the audio-thread meters and publish atoms (threading/DSP review) for a comparison that a reset plus replay already serves. Defer until the scope cues show whether users need it.
- *Documentation only (list the reset events in the manual)* — Cheap, but the scope stays invisible at the moment of reading, and the stop/start loss on re-preparing hosts remains. Insufficient alone.
- *Leave the prepare behaviour and add only the scope readout* — Acceptable fallback: users would at least see '0:02' after a stop/start. It still contradicts THREAD_MODEL's stated intent and ADR-0023 item 6's rationale.

**Decision: Modify · P2.** Confirmed by code and a direct reproduction of the same-pair re-prepare. The complete fix, segmented or per-slot measurement, is disproportionate. The constrained version is enough: a visible scope, a mixed-settings marker, and a same-pair gate consistent with the product's own stated intent (THREAD_MODEL.md:316-317, ADR-0023 item 6). It fixes the decision-quality problem with display-side work plus one small metering-lifecycle change. P1: it affects the integrated and PLR readings in every comparison pass, and the documented A/B workflow reads a mixed figure.

*Calibration:* the verifier judged Modify / P1; the final judgement is Modify / P2. Challenge accepted: mixing across A/B, presets and bypass is ordinary integrated-meter semantics whose remedy is [UX-002](findings-ux.md#ux-002)'s P1 (a second P1 double-counts it); the stop/start restart matches THREAD_MODEL.md:316-319 and rests on unverified host behaviour; the cited misleading figure was leftover synthetic signal. So P2. Modify now: a 'since reset' readout via option B (sample counter cleared with dbTpMaxHold, relaxed on the meter row) and USER_MANUAL §3.4 / §8 step 4 fixes. The same-pair gate moves to Investigate further: it needs a per-host prepareToPlay/setNonRealtime matrix (rate, block, channels) and an owner ruling reversing THREAD_MODEL's recorded contract; key must include outChannels and an offline bounce must stay a reset point. Drop the marker; keep no auto-reset on A/B or preset.

*Adversarial challenge:* evidence holds: yes; priority justified: no (suggested P2); decision justified: yes (suggested Modify). The behaviour is real and certain from the code, but the finding bundles three sub-problems with unequal evidence.

(i) Mixing across A/B, preset and bypass. This is ordinary integrated-meter semantics. Its remedy is a visible reset, which is [UX-002](findings-ux.md#ux-002)'s P1, plus a reset-and-replay step at USER_MANUAL.md:434. Rating the same harm as a second P1 double-counts it.

(ii) Stop/start restart. This depends on host behaviour that no one has verified. It also matches THREAD_MODEL's recorded contract, so it is an owner-level product decision, not a confirmed defect.

(iii) The missing duration readout. This is a real clarity gap. Its impact is lower once a reset control exists.

The one runtime figure cited as a misleading reading is a leftover of earlier synthetic signal. Under the rubric, the residual problem is a meaningful clarity and consistency improvement: P2, not P1.

Modify, constrained further than the judge proposed:
- Now: add the duration-since-reset readout using option B only, and fix USER_MANUAL §3.4 (enumerate what resets and what does not) and §8 step 4 (reset and replay per candidate before judging PLR).
- Move the same-pair gate to Investigate further. It needs a per-host matrix of prepareToPlay and setNonRealtime calls on transport start/stop and at offline-render start, with rate, block and channel count, across the product's DAW matrix, plus an owner ruling against THREAD_MODEL.md:316-319. If adopted, key it on (rate, block, channels).
- Drop the marker.
- Keep 'no auto-reset on A/B or preset'. *Proposal risks:* 1. Same-pair gate (item 3) reverses a decision. It overturns a recorded P5 decision (THREAD_MODEL.md:316-319, 'reaches everything') rather than aligning with it. It needs an explicit owner decision and a THREAD_MODEL update.
2. The key misses channel count. A mono/stereo layout change re-prepares at the same (rate, block), and ADR-0023 item 5 says mono reads about 3 LU lower. A retained histogram would mix two layouts. The key must include outChannels, which is already read at PluginProcessor.cpp:775.
3. Offline bounce. A host that re-prepares for an offline render at the same pair currently gets a clean whole-bounce I, LRA and TP, which is the most trustworthy delivery reading the product can give. The gate would fold the preceding realtime playback into it. This is unverified per host, and setNonRealtime (PluginProcessor.cpp:886) would need to become an explicit reset point.
4. Implementation reach. The change touches two clear sites (LoudnessMeter::prepare→reset and AnabasisEngine::reset→outMeter.reset), the integratedFrom and lraFrom watermarks, and the mutation-verified testMeterResetClearsSessionHolds. It is a meter-lifecycle change inside the DSP layer, not display work.
5. Option A fails under the judge's own fallback. Deriving the readout from the GR ring head does not work if the gate is not adopted. The ring survives clicks, state loads and same-pair re-prepares, so the readout would keep counting while I and LRA restart, which is exactly the case it is meant to expose. An off-thread state load would also need a message-thread snapshot. The judge's claim that the fallback would show '0:02' holds only for option B. Option B is a sample counter cleared at the same sites as dbTpMaxHold and published relaxed on the meter row, which ADR-0020 Consequences treats as no threading-model change. It is correct under both outcomes and should be the only option.
6. The 'changed since reset' marker (item 2) implies a completeness it lacks. Knob moves, automation, undo/redo and Learn/Freeze change the measured programme as much as A/B, preset or BYPASS, or more. If it is keyed on all changes, it stays lit through every working pass. Drop or defer it.
7. Acceptance criteria 1 and 3 depend on the gate and the marker, so they should leave this Modify's scope.

**Architecture gates.**

- Thread Model (review only): option B, a published 'measured time since reset' atomic, would join THREAD_MODEL's meter row. ADR-0020 Consequences treats same-contract meter atomics as 'no new cross-thread path'; option A, derived from the GR ring head, adds none.
- THREAD_MODEL.md:316-319 reset-contract text ('hosts that need a flush re-prepare, which reaches everything') changes if a same-pair re-prepare keeps the session holds. This needs a documentation sync and owner acknowledgement, but it is not a listed hard-stop category. No conflict with ADR-0020 or ADR-0023; it aligns with ADR-0023 item 6.

**Dependencies.** [UX-002](findings-ux.md#ux-002) (a reset control is where the scope readout and its 0:00 feedback live); [VIS-012](findings-visualisation.md#vis-012) (held/live styling shares the same header state); V-05 plugin-BYPASS-meters-dry finding (other batch): the mixed marker covers it only partially

**Acceptance criteria.**

- After 10 s of audio, a re-prepare at the same rate and block size (harness 'sr 48000 512' at 48k/512) leaves I, LRA, TP, SP and PLR continuing: no '-', TP not lowered. A re-prepare at a different pair still resets them.
- The STATISTICS header shows the measured audio duration since the last reset. It advances only while audio is processed, stops while host-bypassed or stopped, and reads 0:00 immediately after a reset control press, a state load, or a pair-changing prepare.
- After a reset, an A/B switch, preset load or plugin BYPASS toggle lights a visible 'changed since reset' marker that stays lit until the next reset. A/B, preset and bypass never clear the statistics themselves.
- USER_MANUAL §3.4 enumerates the events that reset the statistics and those that do not. §8 step 4 instructs a reset and replay before judging PLR after switching.

<details><summary>Verification record</summary>

**Method.** Read the cited anchors at e769f33:
- the two requestMeterReset callers (LoudnessMeterView.cpp:69, PluginProcessor.cpp:1871);
- prepareToPlay :776-812, with :807 clearing the holds and :812 publishing silence;
- AnabasisEngine.cpp:129-132 (outMeter.prepare leads to LoudnessMeter::reset, LoudnessMeter.h:61-80);
- GrHistoryBuffer.h:217-224;
- the render tap, AnabasisEngine.cpp:1243-1263, which includes the bypass mix;
- the historyEpoch bump at PluginProcessor.cpp:1846;
- ADR-0023 item 6 (:99-102);
- THREAD_MODEL.md:316-319.
Viewed 10c2, 17b, 12c and state/33. Reproduced on :131 a same-(rate, block) re-prepare ('sr 48000 512' while music played), the case the code says hosts perform on transport start. The GR history continued unbroken. In the Statistics panel, I restarted (-11.3 to -13.7 at +1 s), S and LRA went to '-', and the TP hold fell from 1.15 to 0.04 dBTP; see session capture `rt/verify-1/33-sameprep-strip.png`.

**Corrections to the candidate claim.** 'Bypassed passages' applies to the plugin's own BYPASS, which feeds the dry signal into the render tap (AnabasisEngine.cpp:1262), not to host bypass. Host bypass runs no processBlock, so everything freezes and nothing accumulates (V-06/E07). The manual lines for 'pausing continues the GR timeline' are USER_MANUAL.md:265-267, not :262-264. The finding understates one point: the product's own architecture says 'a transport stop must not clear a mastering measurement' (THREAD_MODEL.md:316-317), and the code says 'Hosts re-prepare on transport start' (PluginProcessor.cpp:778-779). On such hosts that intent is broken for the statistics while it is honoured for the GR history (ADR-0023 item 6). Which real hosts re-prepare on start was not verified here; only the code's own comment asserts it.

</details>

<sub>Verifier scores (1-5): impact 4 · frequency 4 · severity 4 · discoverability 4 · efficiency 3 · coherence 4 · change risk 3 · complexity 3 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-010

**With MATCH or DELTA on, every meter, the out-LUFS readout and the spectrum output trace still read the pre-monitor render tap, and nothing on screen says so**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Feedback/observability | The session figures have no visible scope, liveness or tap, and a stray click wipes them | Phase 1 |

**Evidence**

- e769f33:src/dsp/AnabasisEngine.cpp:1243-1266 — §2.9 render tap and spectrum output tap take the bypass-mixed programme without delta substitution or monitor gain
- e769f33:src/dsp/AnabasisEngine.cpp:1284-1287 — the monitor gain is applied after the taps
- e769f33:src/PluginProcessor.cpp:963-972 — rationale: metering the listening buffer made Delta show the difference loudness and biased the integrated/dBTP holds
- e769f33:tests/state_tests.cpp:10592-10643 — pins bit-identical published readings with comp/delta on
- Runtime rt/verify-7/bypass.log (L=70 music -6, 8 s): published meterS -8.84 for plain, DELTA and MATCH+DELTA. Listening output S: -8.84 / -12.55 / -21.21. MATCH+BYPASS: meter -16.25 vs heard -24.95
- Screenshots: session capture `rt/visuals/14c-loud70-match-on-editor.png`, session capture `rt/visuals/14d-loud70-delta-on-editor.png` — no tag, badge or state change on the Statistics panel or out-LUFS readout

**Current behaviour.** All meters (M/S/I, TP/SP/RMS/LRA/PLR), the Simple 'out LUFS' readout and the SPEC output trace show the programme render. With MATCH on the user hears a level 7-9 dB lower than the readout. With DELTA on they hear a difference signal while the panel reports full-programme loudness. Nothing marks the readings as pre-monitor.

**Problem.** The metering choice is correct (delivery numbers must not be bent by listening aids, and it is test-pinned), but it is unlabelled. The manual's first push says to watch out LUFS climb and then switch MATCH on and hear the jump disappear, with no word on what the meters show while monitoring.

**Root cause.** This is a deliberate design (PluginProcessor.cpp:963-972). The UI has no concept of 'monitoring active', so the meters give no cue about which signal they describe.

**User impact.** Low-to-moderate confusion in every session that uses MATCH or DELTA: a user may think MATCH is broken ('the number didn't change') or misread DELTA's loudness. No wrong delivery number results, because the readings are the true render. *Scope:* The Statistics panel in both views, the Simple out-LUFS readout, the SPEC trace; whenever MATCH or DELTA is on.

**Proposed improvement.** Keep the render tap. While MATCH or DELTA is on, show a small persistent tag on the Statistics panel header and next to 'out LUFS' ('OUTPUT — pre-monitor' or 'render'). Show MATCH's applied gain next to the MATCH toggle ('MATCH -8.7 dB'), so the user can see what is being compensated. That readout would also expose the [DSP-005](findings-dsp-tech.md#dsp-005) bias. Add one sentence to manual §2.4/§3.2.

**Alternatives considered.**

- *Meter the listening path while monitoring* — Rejected: this was the prior behaviour, removed for good reason. It poisons the integrated LUFS and dBTP holds and is pinned by testMetersReadTheRenderNotTheMonitor.
- *Dim the meters while DELTA is on* — Acceptable secondary cue, but dimming valid delivery numbers can read as 'meter off'. A label is clearer.
- *Leave as is* — Not preferred: the observers read it as a defect, and the fix is a label.

**Decision: Modify · P2.** The underlying behaviour should stay (test-pinned, principled). Only a labelling/readout layer is needed, which is smaller than the obvious 'make the meters follow the monitor' change. P2: a clarity issue in a frequent situation, with no wrong output.

**Architecture gates.**

- ADR-0011 threading model — a MATCH-gain readout needs a new audio->UI display scalar; it must follow the existing relaxed-atomic meter-row pattern or it becomes a threading-model gate item
- ADR-0020 — a tag on the Statistics panel is not a new row; adding a row would amend ADR-0020

**Dependencies.** [UX-009](findings-ux.md#ux-009) (shared monitor-state indicator); [DSP-005](findings-dsp-tech.md#dsp-005) (a MATCH gain readout would surface its bias)

**Acceptance criteria.**

- With MATCH or DELTA on, the Statistics panel and the Simple out-LUFS readout carry a visible tag identifying the readings as the output/render (pre-monitor); with both off the tag is absent.
- With MATCH on, the applied compensation gain is displayed in dB and tracks the engine's monitor gain within 0.5 dB.
- testMetersReadTheRenderNotTheMonitor still passes unchanged.
- The manual states what the meters show while MATCH/DELTA are on.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/dsp/AnabasisEngine.cpp:1243-1266 (render tap + spectrum tap before the delta leg and the monitor gain), :1284-1301, e769f33:src/PluginProcessor.cpp:963-972, e769f33:tests/state_tests.cpp:10592-10643 (testMetersReadTheRenderNotTheMonitor), e769f33:src/gui/PluginEditor.cpp:2037-2040 (out-LUFS readout = meterLufsS). Viewed session capture `rt/visuals/14c-loud70-match-on-editor.png` and 14d-loud70-delta-on-editor.png. Probe rt/verify-7/bypass.log and offline.log compare the published render meter (proc.meterLufsS()) with the measured listening output.

**Corrections to the candidate claim.** The differences between the V-13 captures (MATCH off -8.9, MATCH on -7.6, DELTA -6.5 S) are programme variation (the harness music alternates a louder chorus every 4 bars), not a MATCH effect. At equal times the render meter is identical with and without the monitors (S -8.84 plain, MATCH+DELTA, realtime MATCH, realtime DELTA). Under MATCH+BYPASS the meters show the unattenuated dry (-16.25) while the output is -24.95. The GR well is the limiter's GR and is correctly unchanged.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 4 · severity 2 · discoverability 4 · efficiency 2 · coherence 3 · change risk 1 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-011

**The adaptive trims have no readout, so FREEZE and LEARN act on invisible state and the Advanced knobs show values the engine is not applying. The display-only overlay required by ADR-0005 decision 10 was never built.**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Visualization | The adaptive engine changes the audio from state the user cannot see, keep or reset | Phase 5 |

**Evidence**

- e769f33:src/dsp/AdaptiveEngine.h:586-592 — 'The readout does not exist yet — nothing in src/gui reads publishedTrim*'; :694-700 — 'the P5 UI has no trim readout yet'
- e769f33:src/dsp/AdaptiveEngine.h:20-25 — the four trims and their bounds; :118-120, :166, :418 — Freeze before any audio holds the reset zeros
- e769f33:src/gui/PluginEditor.cpp:42 — FREEZE tooltip 'Hold the adaptive trims exactly where they are now' (the only description of what is held); :621-640, :2046-2075 — Learn button only (countdown, accent, empty-pass warn)
- e769f33:docs/architecture/design-decisions/ADR-0005-macro-layer-architecture.md:84, 156-160, 283 — option J chosen; decision 10 requires a display-only delta overlay in the Advanced view; PluginEditor listed as its site
- e769f33:docs/architecture/design-decisions/ADR-0013-release-trim-reaches-auto-poles.md:14, 27-28 — the overlay is described as existing and used in the option analysis
- e769f33:docs/DESIGN.md:769-775, 930; e769f33:docs/policies/MODE_AND_ADAPTATION_POLICY.md:221-222, 268-269; e769f33:docs/KNOWN_ISSUES.md:353-354, 402-404 — all refer to an overlay that does not exist
- e769f33:docs/user/USER_MANUAL.md:308-309 — 'freeze when the engine has settled' (nothing shows settling)
- runtime: session capture `rt/gestures/20b-advanced-after-macro-reset.png` — Advanced shows Stereo Link 100 %, Release 100.0 ms, SC HPF 20 Hz, Dynamic Tame 0.0 dB, with no delta marks
- probe rt/verify-18/probe/probe.out — after 10 s of a sustained pad the engine applies link 0.89, SC HPF ≈49 Hz (+29), release ×1.46 (+0.55 oct), while those knobs read 100 % / 20 Hz / 100 ms
- runtime rt/verify-18/a-before-sr.xml — harness music latched +0.109 oct / −0.022 link / +1.0 Hz / +0.126 dB. These are visible only in the saved XML, never in the UI.

**Current behaviour.** The adaptive engine runs by default in every session and moves limiter release, limiter stereo link, the comp's SC HPF and the dynamic tame within their bounds. No view shows these deltas. FREEZE's only feedback is its own toggle. LEARN shows a countdown, then accent text, and on stop either nothing (success) or a warn flash (empty pass). The Advanced knobs show the parameter values, not the effective values.

**Problem.** The core adaptive promise and its two controls act on hidden state. Users cannot see whether the engine has 'settled' (the manual's cue for freezing), what they froze, whether a freeze before playback latched nothing, whether a Learn changed anything, or that e.g. Stereo Link '100 %' is actually running at 89 % and SC HPF '20 Hz' at about 49 Hz. It also hides [STATE-004](findings-state-model.md#state-004) and [DSP-008](findings-dsp-tech.md#dsp-008) completely.

**Root cause.** ADR-0005 decision 10's display-only delta overlay (DESIGN §5.4/§6.3) was never implemented in the P5 UI. The engine-side accessors and the validity flag (publishedTrim*, hasPublishedTrims) exist and are explicitly reserved for it, but PluginEditor never reads them. The documents were not updated to reflect the omission.

**User impact.** Every user who opens Advanced sees values that differ from what is applied. Every user of FREEZE or LEARN (including the manual's podcast workflow) works blind. Advanced users can make wrong adjustments (e.g. setting link to 100 % to lock the image, or judging SC HPF at 20 Hz) based on the displayed value. *Scope:* All sessions (adaptation is on by default). Advanced view: four controls. Simple view: FREEZE and LEARN.

**Proposed improvement.** Build the display-only overlay ADR-0005 decision 10 requires, plus a minimal Simple-view status. (1) Advanced: on Limiter Release, Limiter Stereo Link, SC HPF and Dynamic Tame, draw a thin secondary marker on the knob or slider ring at the effective value (parameter ∘ trim), with a dim suffix on the value text (e.g. '100 % · 89 %'). It is hidden while hasPublishedTrims() is false (nothing measured yet) and styled as locked while Freeze is ON. (2) Simple: a compact state beside FREEZE/LEARN: 'adapting' / 'settled' / 'frozen' / 'frozen – nothing measured yet'. Its tooltip lists the four effective deltas. (3) LEARN: on a successful stop, a brief positive confirmation (e.g. 'reference updated') that mirrors the existing empty-pass warn flash. Implementation constraints: message-thread reads of the existing relaxed atomics plus the acquire flag in the existing ~24 Hz timer; no parameter writes, no host-visible change, no undo entry. It must fit the fixed 940×822 Advanced layout and pass the brand checklist. If deferred, correct ADR-0013's context sentence, DESIGN §5.4/§6.3, MODE policy 221-222 and KI-006 354/403 so they stop describing an existing overlay.

**Alternatives considered.**

- *Simple-only status line (no Advanced markers)* — Cheapest, and it fixes FREEZE/LEARN blindness. It leaves the Advanced misrepresentation and ADR-0005 decision 10 unmet.
- *Show the effective value instead of the parameter value in the knob text* — Rejected. The knob must show the parameter the host automates and undo records (invariant 1). Replacing it would make automation and undo look inconsistent.
- *An 'Adaptive' block in the Statistics panel (four trims plus onset rate, tilt, crest, learned reference)* — A good complement for power users and Learn verification. On its own it does not tie the deltas to the affected controls.
- *Leave as is and correct the docs* — Rejected as the end state. Freeze and Learn stay unverifiable and an Accepted ADR decision stays unmet. Acceptable only as an interim doc-drift fix.

**Decision: Modify · P2.** Confirmed in code, docs and runtime. It affects every session, and it is the missing half of an Accepted ADR decision. It is also the prerequisite for users (and testers) to perceive [STATE-004](findings-state-model.md#state-004) and [DSP-008](findings-dsp-tech.md#dsp-008). The change is display-only, reads existing atomics, and touches no gate.

*Calibration:* the verifier judged Proceed / P1; the final judgement is Modify / P2. Challenge accepted: the knob showing the parameter is the ADR-0005 design, real-programme deltas are small and Freeze/Learn are optional, so P2; the ADR-0005 d10 / ADR-0013 coherence gap stands. Modify: show the release delta as an auto-path scale when limAutoRelease is on (the 100 ms knob is not in the signal path at defaults); drive the Freeze status from both published and retained sets so [STATE-004](findings-state-model.md#state-004)'s state reads truthfully; drop or precisely define 'settled'; dim markers when the host stage is inert; reconcile acceptance criteria with the dependency note. Must stay display-only (writing trims to parameters would be a macro contract change). [DSP-008](findings-dsp-tech.md#dsp-008)'s +29 Hz SC HPF rail is a key display case.

*Adversarial challenge:* evidence holds: yes; priority justified: no (suggested P2); decision justified: no (suggested Modify). The evidence holds, and on the ADR side it is stronger than the judge states: an Accepted ADR's decision and its compliance argument both assume the overlay. The user-facing severity is overstated, though. Nothing produces wrong output or destroys work. The knob showing the parameter rather than the applied value is the intended ADR-0005 design, which the overlay would complement, not correct. On the observed real programme the hidden deltas are small. The headline Release example is misframed because the knob is inactive under the default AUTO mode. What remains is lack of feedback for the optional Freeze and Learn controls, plus a doc and ADR coherence gap. Under the rubric that is 'meaningful improvement to usability, clarity, consistency' (P2), not 'materially harms a common workflow' (P1). Frequency 5 applies to adaptation running, not to users being harmed by not seeing it. The decision should be Modify: build the ADR-0005 d10 overlay and the Learn success cue, but show release as an auto-path scale when AUTO is on, drive the Freeze status from both the published and retained sets (so that [STATE-004](findings-state-model.md#state-004)'s state is described truthfully), and drop 'settled' or define it precisely. The acceptance criteria also need to be reconciled with the dependency note. *Proposal risks:* (1) The Release marker as proposed ('effective value = parameter ∘ trim', e.g. '100 ms · 146 ms') would be false at factory defaults. With AUTO on, the limiter uses neither 100 ms nor 146 ms, only the auto poles 40/600 ms scaled by 2^oct. The release delta must be shown as a scale (e.g. 'auto ×1.46' on or near the AUTO toggle) whenever limAutoRelease is on. The ms marker on the knob belongs only to manual mode. (2) The proposed status vocabulary misreports [STATE-004](findings-state-model.md#state-004)'s state. After a re-prepare with a live latch, hasPublishedTrims()==false while hasRetainedTrims()==true (the pinned liveLatch state, state_tests.cpp:3325-3328). 'frozen – nothing measured yet' would tell the user nothing is latched when the session will in fact save and restore a latch. The status must consult the retained set/generation as well, e.g. 'frozen – host reset the applied trims; saved latch kept'. The dependency note ('overlay will show zeros after a re-prepare … the honest reading') also contradicts acceptance criterion 2 ('no marker when hasPublishedTrims() is false'). (3) 'settled' has no engine definition: nothing publishes targets or a convergence flag. A UI-side heuristic (trims static for N s within their deadbands, 0.05 oct / 0.01 / 1 Hz / 0.02 dB) is new product semantics that needs a stated threshold. Otherwise it will flicker, or claim 'settled' while the slew is still moving toward a rail. Drop it, or specify it. (4) Markers on the SC HPF and Dynamic Tame controls show a delta whose audibility depends on the comp and EQ being active. Consider dimming the marker when the host stage is inert. ADR-0013's context records exactly this 'displays a trim the user cannot hear' trap. (5) No hard-stop gate is crossed if it stays read-only (atomic loads in the existing timer; the THREADING_POLICY relaxed display-atomic row plus the acquire flag). However, if the owner instead chooses NOT to build the overlay, that reverses ADR-0005 decision 10. It then needs a superseding ADR under the append-only rule, and the inv-1/6 compliance argument must be re-argued; editing the doc text alone is not enough. (6) The Simple-row status must fit next to MATCH/DELTA/FREEZE/LEARN/out-LUFS in the fixed layout, and it touches the brand checklist, whose Level-5 boxes are still unchecked.

**Architecture gates.**

- None: this implements ADR-0005 decision 10. It must remain display-only; any variant that wrote trims to parameters would conflict with ADR-0005 decision 10 and be a Macro-layer contract change

**Dependencies.** [STATE-004](findings-state-model.md#state-004) (the overlay will show zeros after a re-prepare until that fix lands, which is the honest reading); [DSP-008](findings-dsp-tech.md#dsp-008) (the link marker exposes the one-sided drift)

**Acceptance criteria.**

- In Advanced, with Freeze OFF on a sustained sparse programme, within about 10 s the Stereo Link, SC HPF, Release and Dynamic Tame controls show an effective-value marker that matches publishedTrim*() (e.g. link 100 % → 89 %, SC HPF 20 → ~49 Hz)
- Before any audible block (hasPublishedTrims() false) no marker is drawn, and the Simple status reads 'nothing measured yet'. Freeze ON before audio reads 'frozen – nothing measured'.
- With Freeze ON the markers stop moving and use the frozen style; with Freeze OFF they track the engine within one UI frame period
- A successful Learn stop shows a positive confirmation distinct from the empty-pass warn flash
- Overlay updates change no parameter value, host automation or undo stack; testModeSwitchIsSoundNeutral and pluginval (editor under xvfb) stay green; the timer adds only atomic loads
- ADR-0013 context, DESIGN §5.4/§6.3, MODE policy and KI-006 texts match the shipped UI

<details><summary>Verification record</summary>

**Method.** Grepped src/gui for any reader of publishedTrim*, retainedTrim* or an overlay (none; the editor reads adaptiveReadout() only for the Learn button, e769f33:src/gui/PluginEditor.cpp:621-640, 2046-2075). Read the AdaptiveEngine.h reservations (:586-592, :694-700), ADR-0005 (Accepted) options and decision 10, ADR-0013 context, DESIGN §5.4/§6.3, MODE policy 158-161/219-222/268-270, and KI-006. Viewed session capture `rt/gestures/35c-freeze-tip.png`, 34e-learn-10s.png, rt/state/33a/33-composite and the Advanced view session capture `rt/gestures/20b-advanced-after-macro-reset.png`. Quantified the hidden deltas with the engine probe and a harness savexml.

**Corrections to the candidate claim.** The finding is stronger than stated. The overlay is not only a DESIGN idea but part of an Accepted ADR's decision: ADR-0005 decision 10 says 'the Advanced view shows them as a display-only delta overlay'. ADR-0013's context (line 14) describes the release trim as 'displayed as the Advanced view's overlay', which is factually wrong at e769f33. Learn is not entirely feedback-free: it has a 5 s countdown, accent text while running, and a warn flash on an EMPTY pass (MODE policy 158-161). What is missing is any confirmation or display of a successful pass's learned reference. The zero-latch case holds (Freeze before audio holds reset zeros, and the UI is identical), but that zero latch is not persisted: engineFrozenTrimsIfLive returns nothing while generation == 0.

</details>

<sub>Verifier scores (1-5): impact 4 · frequency 5 · severity 3 · discoverability 5 · efficiency 3 · coherence 4 · change risk 2 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-012

**Held, live and no-signal are never distinguished: in silence I, TP (red), LRA and PLR hold with live styling; '-' covers 'no reading yet', measured silence and 'no audio device'; units print beside the dash**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | confirmed | Feedback/observability | The session figures have no visible scope, liveness or tap, and a stray click wipes them | Phase 1 |

**Evidence**

- e769f33:src/gui/LoudnessMeterView.cpp:156-158: one rule, v <= -99 prints '-'
- e769f33:src/gui/LoudnessMeterView.cpp:243, :266, :268: ' dBTP' / ' dBFS' appended unconditionally, giving '- dBTP' and '- dBFS'
- e769f33:src/gui/LoudnessMeterView.cpp:272-275: LRA and PLR blank rules; no held, idle or stale styling anywhere in paint (:153-281)
- e769f33:src/dsp/LoudnessMeter.h:414 and :154: LRA samples start only 30 sub-blocks (3 s) after a reset; :290-291 a range is printed as soon as two values survive
- e769f33:src/dsp/LoudnessMeter.h:433-436 and :445: sentinel -100 until the window fills; silence computes to about -120.7 LUFS
- e769f33:src/dsp/RmsMeter.h:79-80: kSilentDb -144 (not measured) and kFloorDb -140 (silence) are kept distinct by the meter, and the view's fmt collapses them
- e769f33:docs/user/USER_MANUAL.md:224-233: rows described as live or max-hold; no held or idle state; :232 'A steady master reads near 0'
- Runtime V-10: session capture `rt/visuals/08b-silence-3s-editor.png`, session capture `rt/visuals/08c-silence-15s-editor.png`, session capture `rt/visuals/08d-silence-27s-stats.png`. After 27 s of silence, M and S read '-' while I -3.2, TP 1.35 (red), LRA 24.2 and PLR 4.6 are unchanged in live styling; RMS reads '- dBFS'
- Runtime E01: session capture `rt/edges/01-standalone-nodevice-crop.png` ('- dBTP', '- dBFS', no explanation)
- Runtime verify-1 R4: session capture `rt/verify-1/33-sameprep-strip.png` (LRA '-' at +1 s, '0.1 LU' at +4 s)
- Runtime verify-1 R2: session capture `rt/verify-1/10-hostbypass-before-click-ed.png` (frozen live-looking values with no audio processed)

**Current behaviour.** The panel renders every value the same way whatever its state:
- live;
- held from earlier audio, including during silence or after audio stops;
- warming up (LRA's first seconds);
- measured silence;
- not measured at all.
A single dash stands for 'not measured', 'silence' and 'below -99'. TP, SP and RMS keep their unit beside the dash. The red TP warn colour stays on a historic over with no hint that it is historic.

**Problem.** The panel cannot answer 'is this still measuring, finished, or stale?', and it makes idle states look broken. Examples: 'I -3.2' after half a minute of silence, a red TP from a test tone long past, '- dBFS', and an LRA of 0.1 LU seconds after a reset.

**Root cause.** The view formats with one sentinel threshold (LoudnessMeterView.cpp:157) and has no model of signal presence, measurement age or warm-up. It receives no liveness signal, so a stopped processBlock is invisible to it. The DSP already separates 'not measured' from 'silence' (exact sentinels), but the view discards the distinction. Units are concatenated without regard to the placeholder.

**User impact.** Returning to the screen, a user cannot tell whether I or LRA still describe the programme or are left over from earlier material. A red TP may be read as a current over. The no-device or all-dash state reads as a broken plugin. An early LRA of about 0 can be taken as 'very compressed or steady' when it only means 'not enough data yet'. *Scope:* Both views, every session. It is most visible between passes, during silence, under host bypass or stopped hosts, in the no-device Standalone, and for about 3-4 s after every reset or prepare.

**Proposed improvement.** View-only changes in LoudnessMeterView:

(1) Formatting. 'Not measured' prints a dim unit-less placeholder '—'. A real reading below the display floor prints '< -99' or '-inf', with its unit, in normal text. No unit ever follows a placeholder.

(2) Held state. Liveness comes from the GR ring head, which already advances per processed block and which GrHistoryView uses to park. When no audio has been processed for more than about 0.5 s, or when M is below the -70 LUFS absolute gate, the session rows (I, TP, SP, LRA, PLR) render dimmed and the header shows a small 'HOLD' tag. The rolling rows show idle. Everything returns to live styling within a frame of audio resuming. TP keeps its warn hue, dimmed, so a real historic over is still flagged but reads as historic.

(3) No audio. If no block has been processed since the editor opened, the header reads 'NO AUDIO' in place of values, which covers the no-device Standalone.

(4) LRA warm-up. Show LRA as provisional (dim, e.g. '…' or dimmed digits) until a minimum amount of short-term data has accrued. The exact threshold is a product call to record, not something to invent here. The manual's table then describes these states.

**Alternatives considered.**

- *Leave as is* — The panel keeps looking live when it is not and broken when it is idle. Rejected.
- *Fix only the '- dBFS' formatting slip* — Trivial and worth doing, but it does not address held-versus-live, which is the larger clarity problem.
- *Decay or blank held values after silence* — Would destroy a valid session measurement, which conflicts with the max-hold and integrated semantics users rely on. Rejected in favour of styling.
- *A new audio-thread 'processing' heartbeat atomic for liveness* — Works, but it adds a published atomic (threading review). The GR ring head already provides the signal GUI-side.

**Decision: Proceed · P2.** Confirmed in code and at runtime. The change is display-only and can use a liveness signal the GUI already reads, so it touches no gate. It removes a recurring ambiguity at the moment users read delivery figures. P2 rather than P1: the held values are correct numbers and the harm is interpretive. The case where the ambiguity matters most, what period the numbers cover, is carried by [VIS-009](findings-visualisation.md#vis-009).

**Dependencies.** [VIS-009](findings-visualisation.md#vis-009) (shared header state: scope readout plus HOLD tag); [DOC-002](findings-doc-test.md#doc-002) (what a reset blanks while no audio flows); [UX-002](findings-ux.md#ux-002); V-06/E07 host-bypass freeze finding (other batch): the HOLD state is its display half

**Acceptance criteria.**

- No capture in any state shows a unit directly after a placeholder: the strings '- dBTP' and '- dBFS' never appear.
- With audio flowing and then 'signal silence' for 5 s, I, TP, SP, LRA and PLR keep their values but render in a visibly distinct held style, and a HOLD tag appears in the header. Both revert within one frame after audio resumes.
- Under 'hostbypass 1' (no processBlock) the panel enters the held or idle state within about 1 s instead of looking live.
- In the no-device Standalone the panel states that no audio is being processed instead of showing only dashes.
- Digital silence (M, RMS) and 'not yet measured' (right after a reset) render differently.
- In the first seconds after a reset or prepare, LRA is shown as provisional rather than as a final-looking 0.x LU, and the manual's §3.4 table describes the held, idle and provisional states.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/LoudnessMeterView.cpp:156-158, 243, 266, 268, 272-275, 33-35 and 50. Read LoudnessMeter.h:414 (LRA needs subCount >= 30 and >= lraFrom), :154 (reset watermark), :290-291 (a range once 2 values survive), :433-436 (sentinel until the window fills) and :445 (the 1e-12 energy floor, so digital silence computes to about -120.7 LUFS). Read RmsMeter.h:79-80 (sentinel -144, floor -140). Viewed 08d and edges/01 crop. On :131, a same-pair re-prepare showed LRA '-' at +1 s and then a final-looking '0.1 LU' at +4 s (session capture `rt/verify-1/33-sameprep-strip.png`). Under host bypass the panel froze with live styling until a click blanked it into the same all-dash look as the no-device state (session capture `rt/verify-1/11-hostbypass-after-click-ed.png`).

**Corrections to the candidate claim.** Holding I, TP, SP, LRA and PLR through silence is correct session-metering behaviour (BS.1770 gating, max-holds). The defect is that nothing marks the values as held, not that they hold. The '-' for M and RMS in silence is not the 'no reading' sentinel. Silence is a real reading (about -120.7 LUFS, and -140 dB RMS at the RMS floor), which the view's single '<= -99' rule prints as the same dash. The data to tell the two apart already exists (exact sentinels -100 and -144 against computed values). 'No audio device' applies only to the Standalone wrapper. In a DAW, the equivalent no-processBlock state (host bypass, hosts that stop processing) shows frozen, live-looking numbers rather than dashes (V-06/E07). Added: for a few seconds after every reset or prepare, LRA prints a near-zero value (0.1 LU at +4 s), which the manual reads as 'a steady master' (USER_MANUAL.md:232).

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 4 · severity 3 · discoverability 3 · efficiency 2 · coherence 3 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-013

**The GR well does not distinguish 'no data yet', 'unmeasured history' and 'silence': it is blank before the first entry, draws zero-GR data over the unmeasured region, and looks the same after 20 s of silence**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | partially-confirmed | Feedback/observability | The session figures have no visible scope, liveness or tap, and a stray click wipes them | Phase 1 |

**Evidence**

- e769f33:src/gui/GrHistoryView.cpp:264 — 'if (head <= 0) return;' (blank well, no zero line)
- e769f33:src/gui/GrHistoryView.cpp:412-433 with comment :414-418 — zero line and floor drawn over the unmeasured region; 'a just-reset ring shows the honest zero line … rather than nothing'
- e769f33:src/gui/GrHistoryView.h:341-344 — drawsZeroRegion predicate (filling only)
- e769f33:docs/architecture/design-decisions/ADR-0023-012-field-fix-contracts.md:99-102 — decision 6: 'the unmeasured region drawn as zero data'
- e769f33:docs/user/USER_MANUAL.md:242-244 — 'the region to the left stays empty until twenty seconds of audio have actually been measured'
- E01: session capture `rt/edges/01-standalone-nodevice-crop.png` — blank well
- V-10: session capture `rt/visuals/08d-silence-27s-well.png` — after 27 s of silence: flat zero line + floor
- verify-20 R3: session capture `rt/verify-20/10-hostbypass-reprepare-well.png` (blank after re-prepare with no processing) and session capture `rt/verify-20/11-hostbypass-off-1s-well.png` (zero line over the ~19 s unmeasured region)

**Current behaviour.** With the ring empty (head 0: no device, or after a re-prepare in a host that is not processing) the well shows only the GR|SPEC chip. Once one entry exists, the unmeasured left region shows a 0 dB GR line and a floor line, identical to measured silence.

**Problem.** 'Nothing measured' has two presentations (blank vs zero line), and the zero line in the unmeasured region can be read as 'no reduction happened'. The effect is transient (at most 20 s after a rate/block change) and mitigated by the empty fill.

**Root cause.** The paint path has an early return for an empty ring that runs before the zero-region branch. The zero-data presentation of the unmeasured region is an owner decision (ADR-0023 decision 6).

**User impact.** Low. It can briefly look broken (blank) in hosts that do not process when stopped or with no device, and there is a 20 s window after rate/block changes where the left of the well reads as zero GR. *Scope:* GrHistoryView::paintHistory; USER_MANUAL §3.4 wording.

**Proposed improvement.** Make the empty ring (head <= 0) draw the same zero line and floor that a just-reset ring draws, so 'nothing measured' has one look, which is the look ADR-0023 decision 6 prescribes. Keep the unmeasured-region zero-data presentation. Correct the manual's 'stays empty' to describe the flat zero line with no waveform. Any distinct 'unmeasured' styling or 'no signal' text is for the owner, via an ADR-0023 amendment and C8 copy.

**Alternatives considered.**

- *Style the unmeasured region differently (hatched or dimmed)* — Conflicts with ADR-0023 decision 6. Deferred to the owner; the gain is small because the empty fill already signals 'no audio'.
- *A 'no signal' caption when the ring is empty or silent* — Useful for the no-device case, but it is new UI text (C8) and overlaps the stale/holding-state work (V-10).
- *Leave as-is* — Acceptable. The only defect is the blank-vs-zero-line inconsistency and the manual wording.

**Decision: Modify · P3.** The broad claim, that unmeasured is shown as zero, is a decided, Accepted behaviour, so it is Preserved. What remains is a small consistency fix (head <= 0) and a doc correction, neither of which touches the ADR.

**Architecture gates.**

- ADR-0023 decision 6 ('the unmeasured region drawn as zero data') — only if the unmeasured region is restyled; the recommended head<=0 consistency change does not conflict

**Dependencies.** V-10 (holding/stale-state indication, if a 'no signal' state is adopted); [VIS-006](findings-visualisation.md#vis-006) (annotation layer)

**Acceptance criteria.**

- With an empty ring (re-prepare with no processing, or a no-device Standalone) the GR view shows the same zero line and floor as a just-reset ring, and the GR|SPEC pill still works.
- After a sample-rate change with audio flowing, the unmeasured region's presentation is unchanged from today (the ADR-0023 decision 6 tests stay green).
- USER_MANUAL §3.4 describes the unmeasured region as it is drawn.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/GrHistoryView.cpp:262-265, 410-433 (including the comment at :414-418), e769f33:src/gui/GrHistoryView.h:325-344, ADR-0023 decision 6 and USER_MANUAL.md:241-244. Viewed 01-standalone-nodevice-crop.png, 08d-silence-27s-well.png and 12c-sr96k-1.5s-wellstats.png. Reproduced in the plug-in, not only the no-device Standalone, on :150 (verify-20, R3): 'hostbypass 1' then 'sr 44100 512' gave a completely blank well (no zero line) and '-' statistics. After 'hostbypass 0' and 1.2 s, a flat 0 dB GR line and floor spanned the unmeasured ~19 s, with data only at the right.

**Corrections to the candidate claim.** (1) Drawing the unmeasured region as zero data is an explicit Accepted decision, not an oversight: ADR-0023 decision 6, 'the unmeasured region drawn as zero data' (ADR-0023:99-102). (2) Silence drawn as a 0 dB GR line over an empty waveform is truthful data. The empty fill already separates silence from 'audio with no reduction', so the silence half of the claim is low-harm. (3) The real inconsistency is narrower. The code's own comment says a just-reset ring shows 'the honest zero line across the whole panel rather than nothing' (:414-418), but at head == 0 the early return at :264 draws nothing. So the same 'nothing measured' state has two looks. (4) The manual says the left region 'stays empty' (USER_MANUAL.md:243-244), while code and ADR draw a 0 dB GR line there: minor doc drift.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 2 · severity 1 · discoverability 2 · efficiency 1 · coherence 2 · change risk 2 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-014

**The active Integrated standard and RMS reference are not shown on the rows they change; under AES-17, RMS can read above the sample peak or in positive dBFS with no reference shown**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | confirmed | Visualization | The session figures have no visible scope, liveness or tap, and a stray click wipes them | Phase 1 |

**Evidence**

- e769f33:src/gui/LoudnessMeterView.cpp:79-80 — `ungated` / `aes17` read from int_integratedStd / int_rmsRef on every tick; the defaults (0) are gated and AES-17
- e769f33:src/gui/LoudnessMeterView.cpp:84 — the I value switches between meterLufsIUngated() and meterLufsI(); :94 PLR follows the shown I
- e769f33:src/gui/LoudnessMeterView.cpp:50 — AES-17 adds +3.0103 dB to the mathematical RMS; :132 applies it after the hold
- e769f33:src/gui/LoudnessMeterView.cpp:174 — the tags are the constant {"M","S","I"}; :268 statRow("RMS", … + " dBFS"); :274-275 PLR tag is also fixed
- e769f33:src/gui/LoudnessMeterView.cpp:6-10 — the panel tooltip is static and mentions neither standard (tooltips are off by default)
- e769f33:src/InternalState.h:40-41 — int_integratedStd 0=BS.1770-2+ / 1=BS.1770-1; int_rmsRef 0=AES-17 / 1=mathematical
- e769f33:src/gui/PluginEditor.cpp:777-784 — the only place the choice is visible is inside the Settings popup; the combo tooltip calls the row 'INTEGRATED' while the panel tags it 'I'
- e769f33:docs/user/USER_MANUAL.md:228,231 — the I and RMS rows defer to 'a Settings choice (§3.5)'; :288-289 — the Settings table explains both choices
- e769f33:docs/architecture/design-decisions/ADR-0020-waveform-statistics-panel.md Decision 4 (the view resolves the standard, flips are instant) and Decision 6 (eight rows, identical in both views, 202/234 px)
- ST-12: Integrated → BS.1770-1: 'I -9.2' before and after, label unchanged; RMS Reference → Mathematical: -12.4 → -14.9 dBFS, label 'RMS' unchanged — session capture `rt/state/25a-crop.png`, 25b-crop.png, 26a-crop.png
- V-18: session capture `rt/visuals/22b-integrated-1770-1-0.5s-stats.png`, 23a-rms-mathematical-0.5s-stats.png
- V-11: square at -0.5 dB → RMS +2.5 dBFS beside SP -0.10 — session capture `rt/visuals/06-square200-stats.png`
- E13: +6 dBFS 50 Hz square → RMS 2.9 dBFS vs SP -0.10 — session capture `rt/edges/40a-square50-plus6-simple.png`
- verify-21 (own run, :151): music -6 dB, Loudness 100 %, Character 1.00 → RMS -4.1..-8.3 dBFS; music 0 dB → RMS up to -0.8 dBFS vs SP -0.10; the row label stays 'RMS … dBFS' — session capture `rt/verify-21/01-composite.png`, 02-composite.png

**Current behaviour.** Two Settings choices change the numbers in the STATISTICS panel: int_integratedStd switches the I row, and with it PLR, between gated and ungated integrated loudness, and int_rmsRef adds or removes 3.01 dB on the RMS row. The view applies both instantly, but the row tags stay 'I', 'PLR' and 'RMS … dBFS' in every state. Nothing outside the Settings popup shows which standard is active, and the panel tooltip is silent about it. Under the default AES-17, a signal with a crest factor below 3 dB reads RMS above SP and above 0 dBFS.

**Problem.** The reading depends on a hidden, session-persistent setting that the reading does not show. A user comparing the RMS row with another meter, or with an earlier project where the reference differed, sees an unexplained 3.01 dB offset. A user who once selected BS.1770-1 reads an ungated I, and a PLR derived from it, under the same 'I' tag that a delivery spec means as gated. On dense material the Integrated switch changes nothing visible, so the user cannot confirm it took effect. A positive-dBFS RMS above the sample peak looks like a metering bug, although it is correct under AES-17.

**Root cause.** ADR-0020 Decision 4 resolves the standard in the view (LoudnessMeterView::tick, :79-84, :132), but paint() draws constant tag strings (:174, :268, :274) and a constant unit, and never uses the resolved booleans. The reference is surfaced only in the Settings popup and the manual.

**User impact.** There is a misread risk on every session where a non-default standard is active or where the RMS row is compared with another meter. The typical outcome is distrust ('the meter is 3 dB off') or a confirmation trip into Settings. A wrong integrated figure under BS.1770-1 could mislead a delivery check, but only after a deliberate non-default choice. The default display is correct, so this does not reach P0/P1. *Scope:* LoudnessMeterView::paint (the I, PLR and RMS row tags or units) and tooltipText. The fix is display-only: no DSP, parameter, serialization or threading change. Both views are affected identically.

**Proposed improvement.** Make each row say which standard produced its number, on the row itself, in both views. (a) RMS row: the unit names the reference in both states, e.g. '-12.4 dBFS AES' and '-15.4 dB RMS' (mathematical), so a positive AES-17 value is self-explaining. (b) I row: when BS.1770-1 is selected, show a visible marker, e.g. tag 'I-1' or a dim 'ungated' suffix after the value. The PLR row carries the same marker because it is derived from I. Whether the gated default shows a marker ('I') is a copy decision. (c) The panel tooltip names the active standard and reference. (d) The Settings combo tooltip refers to the row by its panel tag ('I'). Keep the instant, no-reset switch, which is correct because both integrals cover the same interval.

**Alternatives considered.**

- *Leave as-is and rely on Settings and the manual* — Rejected. The choice persists silently with the session, so the user who most needs the reminder (the one who changed it weeks ago) is the least likely to open Settings, and the manual itself sends the reader to Settings.
- *Mark only non-default choices* — Acceptable for the I row (keeps the default uncluttered), but not enough for RMS. The AES-17 default is exactly the state that produces positive-dBFS or RMS-above-SP readings, so the RMS reference should always be named.
- *Reset the integrated measurement on a standard switch* — Rejected. Both integrals already run in parallel from the same reset, so a reset would throw away valid data and contradict ADR-0020 Decision 4's instant flip.
- *Change the default RMS reference to Mathematical* — Rejected. AES-17 is the owner-directed and documented mastering convention (ADR-0020 Decision 5; USER_MANUAL.md:289), and changing it silently moves every existing reading by 3 dB.

**Decision: Proceed · P2.** The defect is confirmed in code and at runtime. It is cheap, paint-only, and gate-free, and it makes the panel consistent with ADR-0020's own principle that the fields 'select which STANDARD a shown reading follows'. The label is the missing half of that principle. The default display is correct, so this is clarity work rather than a correctness fix, hence P2.

**Dependencies.** None.

**Acceptance criteria.**

- With int_rmsRef=0 (AES-17) the RMS row visibly names the AES-17 reference, and with int_rmsRef=1 it names the mathematical reference. Both are readable at UI scale XS in Simple and Advanced.
- With int_integratedStd=1 the I row and the PLR row both show a BS.1770-1/ungated marker, and with int_integratedStd=0 that marker is absent or shows the gated standard.
- Flipping either Settings combo updates the marker in the next painted frame with the transport stopped (no audio needed), and the numeric values are bit-identical to today's rmsWithReference and plrFromShown output.
- The panel tooltip text states the currently active Integrated standard and RMS reference.
- All eight rows still fit the Advanced strip without relayout (ADR-0020 Decision 6 height budget), and the row set and order are unchanged in both views.
- USER_MANUAL §3.4 and §3.5 describe the on-row markers.

<details><summary>Verification record</summary>

**Method.** Read every cited anchor at e769f33. LoudnessMeterView.cpp:79-80 resolves both choices on every tick, :84 swaps the I source, :94 derives PLR from the shown I, :50 and :132 add the +3.0103 dB offset after the readout hold, :174 fixes the tags to M/S/I, :268 prints RMS as '… dBFS', and :6-10 is a static tooltip that mentions neither standard. Also read InternalState.h:40-41, PluginEditor.cpp:777-784 (the Settings combos), the ADR-0020 Decisions 4-6, and USER_MANUAL.md:222-233 and :286-289. Viewed rt/state/25a, 25b and 26a crops, rt/visuals/22b, 23a and 06, and rt/edges/40a. Reproduced on :151 (rt/verify-21) with music at -6 and at 0 dBFS, Loudness 100 % and Character 1.00: the RMS row stayed 'RMS … dBFS' and read -4..-8 dBFS and up to -0.8 dBFS, against SP -0.10.

**Corrections to the candidate claim.** (1) The manual anchor is USER_MANUAL.md:288-289, not :287-288. (2) The 'between machines' claim is unsupported. int_rmsRef and int_integratedStd are saved with the session, so one project reads the same on any machine. The unlabelled 3 dB difference appears between projects or instances with different settings, and new instances default to AES-17. (3) 'RMS above sample peak' was reproduced only on square waves (V-11, E13). On heavily limited programme the RMS came within 0.7 dB of SP but did not cross it, so this is an edge case of the convention and not a common reading. It is also correct under AES-17. (4) 'Integrated switch looks inert' is an accurate perception, but only on dense programme: gated and ungated agree within 0.1 LU there (-9.2→-9.2, -13.4→-13.3). With quiet passages they differ by more than 2 LU (ADR-0020's own test). (5) Not resetting on a switch is correct: both integrals are accumulated in parallel from the same reset (PluginProcessor.cpp:999, :1013), so the new standard applies to the whole measured interval at once. Only the missing label is a defect.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 3 · severity 2 · discoverability 4 · efficiency 2 · coherence 3 · change risk 1 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-015

**There is no input metering: a hot or clipping input is invisible, and no input-versus-output loudness ('how much louder') figure exists anywhere**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | medium | confirmed | Visualization | The session figures have no visible scope, liveness or tap, and a stray click wipes them | Phase 2 |

**Evidence**

- e769f33:src/PluginProcessor.cpp:966-1014 — the whole meter publish set (M/S/I, TP hold, PLR, SP hold, RMS, I-ungated, LRA) comes from engine.outputLoudness()/outputRms()/lastRender*, the render tap; nothing from the input
- e769f33:src/dsp/AnabasisEngine.cpp:894 — dryRing stores the raw input (before Input Gain); :1219, :1225 — the delay-aligned dry signal feeds monFrameDry; :1297 — dryMeter.processFrame(monFrameDry)
- e769f33:src/dsp/AnabasisEngine.cpp:689-693 — dryMeter's only consumer: compMeasureDb = dry S − wet S, clamped −24..+6, for MATCH; e769f33:src/dsp/AnabasisEngine.h:572,623 — private, never published
- e769f33:src/dsp/AnabasisEngine.cpp:874-878 — spectrum input tap is post-InputGain; e769f33:src/gui/SpectrumView.cpp:830 — dbHi = 0 dB clamps the trace; :924-925 — input drawn as a dim 1 px line
- e769f33:docs/DEVELOPMENT_BRIEF.md:147-153 — the §6 metering spec has no input meter, only an 'overlaid input/output' spectrum
- e769f33:docs/user/USER_MANUAL.md:101-102 — the first-push workflow says 'watch the out LUFS readout climb', an absolute value with no baseline; :170 — 'At 0 it applies no push — but the Ceiling still holds, so anything already hotter than it is still limited'
- E13: +6 dBFS square, Loudness 0 %, GR flat at about -6 dB, no input indicator — session capture `rt/edges/40a-square50-plus6-simple.png`, session capture `rt/edges/50a-square-plus6-advanced.png`
- V-09: no input LUFS and no delta LUFS anywhere; V-17 / session capture `rt/visuals/11a-adv-on-editor.png` — the Advanced Input Gain slider has no meter beside it
- ST-13: BYPASS switches the rows to the dry input, and after un-bypass the TP/SP holds keep the red over-0 values — session capture `rt/state/30b-bypass-1s.png`
- verify-21 (own run, :151): Loudness 0 %, music at +4 dB → GR history shows active gain reduction, and the panel shows only output figures; nothing names the input as hot — session capture `rt/verify-21/03-loud0-level+4.png`

**Current behaviour.** All level and loudness readouts describe the output (the render tap). The engine already measures the delay-aligned dry input's M/S loudness for MATCH but keeps it internal. There is no input peak, input loudness, or output-minus-input loudness figure in either view. The only input depiction is the spectrum's dim trace, which is not the default graph, sits after Input Gain, and clamps at 0 dB. At Loudness 0 a hot input produces visible GR with no cue that the input is the cause.

**Problem.** A user cannot check gain staging into the maximizer, cannot see that the source is already flat-topped or over full scale, and cannot read how many LU of loudness the processing added. The manual's own first workflow ('watch the out LUFS readout climb') asks the user to track a rise with no baseline. The only in-plugin way to see the input (BYPASS) contaminates the session TP/SP/I holds until a manual reset and re-play.

**Root cause.** ADR-0020's owner-directed scope defines the STATISTICS panel as output-only readings from the render tap, and the brief's §6 never asked for input metering. The dry measurement the engine already makes (dryMeter) exists only to drive MATCH's compensation and was never published.

**User impact.** Every mastering pass loses two orientation figures, input level and loudness gain. The user compensates by ear with MATCH, by inference from the GR history, or with the bypass trick, which poisons the session statistics. The output stays correct, so there is no audio-correctness or destroyed-work harm. *Scope:* Processor meter publish set (a new relaxed atomic or two from the dry tap), publishSilentMeters' clear list, the view (Simple has about 300 px unused under PLR in the STATISTICS panel; Advanced has 32 px left of its 234 px budget), USER_MANUAL §3.4, and the THREAD_MODEL 'Meters → GUI' row.

**Proposed improvement.** Constrained version (Modify). (1) Publish the existing dryMeter's short-term loudness, plus a cheap input sample-peak max hold taken from the same dry frame, as new relaxed atomics in the same once-per-block publish, cleared by publishSilentMeters and the panel-click reset. (2) Show two figures: 'IN' (input sample-peak hold in dBFS, warn-coloured above 0 dBFS, and/or input S LUFS) and 'GAIN' (output S − input S in LU, time-aligned because dryMeter is delay-aligned). In Simple they belong in the unused lower part of the STATISTICS panel or in the out-LUFS slot (see [VIS-016](findings-visualisation.md#vis-016)). In Advanced they belong beside the Input Gain slider in the utility row, so the 202/234 px panel budget is untouched. (3) GAIN reads 0.0 LU under BYPASS by construction (the render tap is dry then), which teaches the user what the number means. Do not add a full input meter strip.

**Alternatives considered.**

- *Leave as-is: MATCH answers 'louder or better' by ear, and the DAW shows source levels* — Weak. MATCH gives no number, and on a master bus the DAW's meters usually show this plugin's output, not its input. The BYPASS workaround poisons the session statistics.
- *Full input meter strip (peak + RMS + LUFS bars), Ozone-style I/O meters* — Larger layout change that collides with ADR-0020's Advanced height budget. Worth considering only if the owner widens the metering scope, which is why the constrained version is preferred.
- *Show only the loudness gain (out S − in S), no input peak* — The cheapest option and fully reuses the existing measurement, but it leaves hot or flat-topped input undetected. An acceptable first step if new DSP is to be avoided.
- *Expose MATCH's internal compMeasureDb as the gain figure* — Rejected. It is clamped to −24..+6, frozen under the −70 LUFS gate, and sign-inverted for monitoring, so it is a control signal, not a meter.

**Decision: Modify · P2.** The gap is real, confirmed in code and at runtime, and it hits the maximizer's core loop. The obvious fix (full I/O meters) conflicts with ADR-0020's owner-directed panel contract and the Advanced space budget. Publishing the measurement that already exists (dryMeter S) plus a trivial input peak hold, and placing two figures where space already exists, gets most of the value with minimal DSP. It still needs an ADR-0020 amendment signed by the owner. Priority is P2: this is missing orientation, not a trap or a wrong output, and the brief never promised it.

**Architecture gates.**

- Accepted ADR-0020 Decision 6 conflict: the panel's row set is fixed at eight rows, identical in both views, within 202/234 px of the Advanced strip. Adding input or gain readings to the panel, or anywhere as a new metering surface, needs an ADR-0020 amendment or a new ADR with owner sign-off
- Thread Model (ARCHITECTURE_REVIEW_GATE): new published relaxed atomics fed from the DRY tap, while the THREAD_MODEL 'Meters → GUI' row describes the set as render-tap-fed. ADR-0020 §Consequences' precedent treats extra same-contract meter atomics as not a threading-model change, but the reviewer must confirm, and THREAD_MODEL plus publishSilentMeters' list must be updated
- DSP Graph change (ARCHITECTURE_REVIEW_GATE) applies only if a new input peak detector is added as a metering node; publishing the existing dryMeter is not a new node

**Dependencies.** [VIS-016](findings-visualisation.md#vis-016)

**Acceptance criteria.**

- With Loudness 0 % and a programme peaking at +4 dBFS, an input readout shows a value above 0 dBFS in the warn colour while the output SP row reads at or below the ceiling.
- A GAIN figure equals output S − input S (±0.1 LU) on the same frame. It reads 0.0 ±0.1 LU with BYPASS engaged, and 0.0 ±0.1 LU at Loudness 0 % with an input below the ceiling.
- At Loudness 100 % on the audit music signal at -6 dB, GAIN reads a positive LU value that tracks the S rise shown by the STATISTICS S row.
- Clicking the STATISTICS panel clears the input peak hold together with the output holds, and prepareToPlay/setStateInformation clear them through publishSilentMeters.
- The Advanced STATISTICS panel keeps its eight rows and fits its 234 px budget unchanged. The new figures appear in both views (in Advanced, beside Input Gain).
- The audio thread gains no allocation or lock (RT-safety tests and pluginval pass). THREAD_MODEL, ADR-0020 (amendment) and USER_MANUAL §3.4 describe the new readings.

<details><summary>Verification record</summary>

**Method.** Read PluginProcessor.cpp:966-1014: every published meter comes from the render tap. Read AnabasisEngine.cpp:894 (dryRing stores the raw input, before Input Gain), :1219/:1225 (a delay-aligned dry signal feeds monFrameDry), :1297 (dryMeter.processFrame) and :689-693 (its only consumer is MATCH's compMeasureDb). Read AnabasisEngine.h:572/:623 (private). Read AnabasisEngine.cpp:874-878 (the spectrum input tap is post-Input Gain) and SpectrumView.cpp:830 (dbHi = 0) and :924-925. Read DEVELOPMENT_BRIEF.md §6 (:147-153) and ADR-0020. Viewed rt/edges/40a and rt/visuals/11a (the Advanced Input Gain slider has no meter). Reproduced on :151 (rt/verify-21/03): Loudness 0 % with music 4 dB hot. The GR history shows active reduction, and the panel shows only output figures (SP -0.10, TP 1.41 red). Nothing identifies the input as the cause.

**Corrections to the candidate claim.** (1) The 'claimed_behaviour' overstates the brief. DEVELOPMENT_BRIEF §6 (:147-153) does not say 'honest metering' and does not ask for input metering. Its only input display is the in/out spectrum overlay, and ADR-0020's owner directive (the Accepted scope) also lists output readings only. This is a product gap, not drift from the spec. (2) The spectrum is weaker than 'the only view of the input' suggests. It is not the default graph (GR history is, since 0.1.2), it shows spectral density rather than level, it is taken after Input Gain, and it clamps at 0 dB (SpectrumView.cpp:830), so an over-full-scale input flat-lines at the top edge. (3) 'Clipping input': a +6 dBFS float input is not clipped inside the plugin (the limiter handles it). The unmet need is gain-staging visibility and detecting flat-topped (already-clipped) source material. (4) There is a workaround, and it is costly. BYPASS switches the STATISTICS rows to the delayed dry input, but the session holds keep what the bypassed input produced (ST-13: TP 4.03 red persists after un-bypass), so reading the input poisons the TP/SP/I measurement until the next reset. (5) The dry loudness needed for a gain figure already exists and is time-aligned (dryMeter), so a 'how much louder' number needs a new publish path but no new measurement.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 4 · severity 2 · discoverability 4 · efficiency 3 · coherence 3 · change risk 3 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-016

**'out LUFS' (Simple only) is an exact duplicate of the STATISTICS S row under a different name; the manual calls it 'live' without saying it is short-term**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | confirmed | Information architecture | The session figures have no visible scope, liveness or tap, and a stray click wipes them | Phase 2 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:646 — caption 'out LUFS' ('§6.2 wireframe'); no tooltip on either label
- e769f33:src/gui/PluginEditor.cpp:2037-2044 — outLufsValue.setText(meterLufsS()) inside `if (! advanced)`, driven by the 24 Hz editor timer (:1000)
- e769f33:src/gui/PluginEditor.cpp:1795-1801 — outLufsCaption/outLufsValue in simpleOnly (hidden in Advanced)
- e769f33:src/gui/LoudnessMeterView.cpp:83 — the S row reads the same processor.meterLufsS() at FrameClock pace
- e769f33:docs/DESIGN.md:901-910 — the wireframe's right panel shows 'LUFS M S I -9.5' and the toggle row shows 'out LUFS -9.5', the same value
- e769f33:docs/DEVELOPMENT_BRIEF.md:120 — Simple mode must include a 'live output LUFS readout'
- e769f33:docs/user/USER_MANUAL.md:101-102 ('watch the out LUFS readout climb'), :177 ('the live out LUFS readout'), :227 (S row = 'Short-term loudness, the last 3 s') — the manual never identifies the two
- V-09: equal pairs -11.9/-11.9, -7.1/-7.1, -13.1/-13.1, -3.0/-3.0, -34.5/-34.5 — session capture `rt/visuals/02-music-6-outlufs.png`, 02-music-6-stats.png; V-17 — session capture `rt/visuals/11a-adv-on-editor.png`
- verify-21 (own run, :151): 30 frames at about 0.2 s, 29 identical, one S -15.5 vs out -15.3 — session capture `rt/verify-21/04-S-vs-outLUFS-pairs.png`; S -9.2 vs out LUFS -9.1 — session capture `rt/verify-21/03-loud0-level+4.png`

**Current behaviour.** In Simple, the toggle row beside the macro shows 'out LUFS <value>'. The value is the render short-term (3 s) loudness, the same atomic the STATISTICS S row prints on the same screen. It updates at 24 Hz against the panel's FrameClock, so it occasionally differs by 0.1-0.2 for a frame. It is hidden in Advanced, has no tooltip, and the manual calls it 'live' without naming the window.

**Problem.** Two labels ('out LUFS' and 'S') for one number, sampled at different instants, suggest two different measurements, and the brief momentary mismatches reinforce that. The most prominent readout position next to the macro repeats a panel value instead of carrying a figure that exists nowhere else, such as a loudness gain or numeric GR.

**Root cause.** DESIGN §6.2 placed both a Simple right-hand M/S/I panel and the brief §5.1 'live output LUFS readout' on the same screen and gave them the same quantity (the wireframe shows -9.5 twice). Neither DESIGN nor ADR-0020 assigned the readout a distinct role, and it reads the atomic from a different timer than the panel.

**User impact.** Low. It causes mild confusion ('which LUFS is right?') and wastes the prime readout slot. There is no wrong output, and the values agree except for a frame here and there. *Scope:* PluginEditor (outLufsCaption/outLufsValue text, source and timer) and USER_MANUAL §2.4 and §3.2. Simple view only.

**Proposed improvement.** Keep a live output-loudness figure in the slot (brief §5.1) but stop it being an unexplained duplicate. Preferred, once [VIS-015](findings-visualisation.md#vis-015) publishes the dry short-term: show the output short-term with the loudness gain beside it, e.g. 'out S -9.1 LUFS (+6.2 LU)', so the slot carries the one number the panel lacks. Minimum standalone change: rename it to state its window ('out S LUFS' / 'short-term'), give it a tooltip saying it is the STATISTICS S value, and read it from the same tick as the S row so the two never disagree. Update USER_MANUAL.md:101-102 and :177 to say short-term.

**Alternatives considered.**

- *Remove the readout (the S row already shows it in Simple)* — Cleanest information architecture, but it contradicts DEVELOPMENT_BRIEF §5.1 and the signed-off DESIGN §6.2, so it needs an owner decision. It also frees the slot without filling it.
- *Repurpose the slot to a numeric GR (pubGrDb is already published and unread by any GUI)* — Adds a figure found nowhere else without new DSP, but it drops the brief's 'output LUFS readout' from the slot, so it also needs an owner decision.
- *Show output INTEGRATED instead of short-term* — Still duplicates a panel row (I), so it does not solve the duplication.
- *Leave as-is* — Harmless to audio but keeps a duplicated, unexplained label in the most prominent readout position. Acceptable only if nothing else is planned for the slot.

**Decision: Modify · P3.** The duplication is confirmed and designed-in rather than accidental, and the slot is a brief requirement, so outright removal is not justified without the owner. A constrained change keeps the output-loudness readout and removes the ambiguity: label and window, a shared tick, and ideally the gain figure from [VIS-015](findings-visualisation.md#vis-015). The harm is small, so P3.

**Dependencies.** [VIS-015](findings-visualisation.md#vis-015)

**Acceptance criteria.**

- In Simple, the slot's caption or tooltip states that the value is short-term (3 s) output loudness, or it shows a figure not already printed in the STATISTICS panel.
- In any capture where both are visible, the slot's output-loudness value and the S row never differ (they are read from the same snapshot or tick); a 60-frame capture at 0.1 s intervals shows zero mismatches.
- If [VIS-015](findings-visualisation.md#vis-015) lands, the slot shows the output−input loudness gain in LU, and it reads 0.0 ±0.1 LU with BYPASS on.
- USER_MANUAL.md §2.4 step 2 and §3.2 name the readout's window, or describe its new content.

<details><summary>Verification record</summary>

**Method.** Read PluginEditor.cpp:646 (caption 'out LUFS', no tooltip anywhere), :1000 (24 Hz editor timer), :2037-2044 (outLufsValue ← meterLufsS()), :1795-1801 (simpleOnly), and LoudnessMeterView.cpp:83 (S ← meterLufsS(), FrameClock pace). Also read DESIGN.md:895-912 (the §6.2 wireframe), DEVELOPMENT_BRIEF.md:120, USER_MANUAL.md:101-102, :177 and :227, and the pre-ADR-0020 LoudnessMeterView.h at c69bc9a. Viewed V-09 and V-17 screenshots. Reproduced on :151: 31 sampled frames at about 0.2 s apart (rt/verify-21/04 plus 03). They were identical in 29 frames and differed momentarily twice (S -15.5 vs out -15.3; S -9.2 vs out -9.1).

**Corrections to the candidate claim.** (1) 'Identical in every frame' is slightly overstated. Both widgets read the same atomic but at different instants (24 Hz editor timer vs the FrameClock vblank pace), so while short-term moves they can disagree by 0.1-0.2 LU for a frame. That makes 'two measurements' more believable, not less. (2) The suspected root cause is inaccurate. The duplication was designed in: the DESIGN §6.2 wireframe (:901-910) already shows 'LUFS M S I -9.5' in the Simple right panel beside 'out LUFS -9.5'. The pre-0.1.1 panel (c69bc9a LoudnessMeterView.h, 'DESIGN §6.2 right panel') already had M/S/I in Simple. ADR-0020 did not add S to Simple. (3) The word 'live' is at USER_MANUAL.md:177; :101-102 is the 'watch the out LUFS readout climb' step. Neither says short-term. (4) The readout itself is a brief requirement (DEVELOPMENT_BRIEF.md:120: 'live output LUFS readout' in Simple), so removal is a product-spec decision, not a pure cleanup.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 4 · severity 1 · discoverability 2 · efficiency 1 · coherence 3 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-017

**The GR-history waveform fills to the top on a pushed master, so it carries no information there and lowers the GR trace's contrast; its linear mapping squeezes quiet passages to the floor**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | medium | partially-confirmed | Visualization | The graphs are qualitative and partly mis-calibrated | Phase 2 |

**Evidence**

- e769f33:src/gui/GrHistoryView.cpp:454 — wh = area.getHeight()·jlimit(0,1,peak) (linear)
- e769f33:src/gui/GrHistoryView.cpp:547-550 — fill textDim·0.35 under a 1.4 px accent stroke
- e769f33:src/dsp/AnabasisEngine.cpp:770 and :1308 — peak = max |render| (post-limiter, linear)
- V-03: session capture `rt/visuals/03-music-6-loud70-well.png` — the fill reaches the top on every bucket; the trace runs inside it
- V-01: -3 dBFS sine fills about 71 % (linear)
- verify-20 R1: session capture `rt/verify-20/01-adv-loud70-music6.png` — same saturation in the 250 px Advanced well

**Current behaviour.** The grey layer is each entry's post-chain sample peak on a linear 0..1 scale. On a limited master it is a near-solid block at the Ceiling, with the gold GR trace drawn through it. On quiet material it hugs the floor.

**Problem.** In the product's main use case the waveform layer adds no information and costs trace contrast. In quieter material the dynamics are compressed into the bottom few pixels.

**Root cause.** Plot choice. What is plotted is the OUTPUT peak, which a limiter pins at the Ceiling by design, on a linear amplitude mapping.

**User impact.** Minor. The trace remains readable, but the well reads as 'a line in a grey block' when pushed, and the waveform tells the user little about programme dynamics. *Scope:* GrHistoryView paint only (the recommended part); the ring payload only for the deferred alternative.

**Proposed improvement.** Paint-only. (a) Give the GR trace a thin dark under-stroke, or lower the fill alpha within a few px of the trace, so the trace separates from a saturated fill. (b) Map the fill in dB over a fixed range (e.g. -48..0 dBFS, converted in the view from the stored linear peak), so quiet and mid-level passages are legible; keep the scale fixed per ADR-0023. Deferred owner question: whether the fill should show the pre-chain input level, so GR reads as the gap between input and output as in the brief's Pro-L 2 reference. That changes the ring payload.

**Alternatives considered.**

- *Fill from the input (pre-chain) peak* — The most informative layer when pushed, but it changes what the ring stores (ADR-0038/ADR-0040 reader contract, DESIGN §2.9 waveform definition) and bypass semantics. Defer to owner.
- *Drop the waveform layer* — Loses the level context the brief asks for ('scrolling waveform with the gain-reduction trace overlaid').
- *Leave as-is* — Tolerable, since the trace stays readable, but a cheap paint-only gain is available.

**Decision: Modify · P3.** The claim is confirmed in substance but overstated ('hidden') and partly mis-attributed (linear mapping does not cause the pushed-case saturation). The constrained paint-only change improves contrast and quiet-case legibility without touching the ring. The input-level fill is deferred as an owner product decision.

**Architecture gates.**

- Only for the deferred input-level fill: ring payload change under ADR-0038/ADR-0040's reader contract and a new audio->GUI field (ARCHITECTURE_REVIEW_GATE Thread Model change); the recommended paint-only change touches none

**Dependencies.** [VIS-006](findings-visualisation.md#vis-006) (dB ticks should share the fill's dB range if both are adopted); [UI-007](findings-ui.md#ui-007) (pill overlap half of the original candidate)

**Acceptance criteria.**

- At Loudness 70 on a limited programme, the GR trace is distinguishable from the saturated fill at 1x and at XS (a measurable luminance contrast between trace and surrounding pixels is at or above today's contrast against the empty background).
- A -20 dBFS passage occupies visibly more than 10 % of the well height (e.g. about 58 % on a -48..0 dB mapping).
- Bucket identity and scrolling behaviour are unchanged (existing GrHistoryView geometry and jitter tests stay green).

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/GrHistoryView.cpp:446-454 and :541-550, and e769f33:src/dsp/AnabasisEngine.cpp:770 and :1301-1310. Viewed 03-music-6-loud70-well.png and 03-music-6-loud70-editor.png. My own Loudness-70 captures on :150 (verify-20 01-adv-loud70-music6.png, clip0-full.png) show the grey fill reaching the top on nearly every bucket, with the gold trace inside it.

**Corrections to the candidate claim.** (1) The height expression is at :454, not :452; the fill alpha (textDim at 0.35) is at :547 and the 1.4 px stroke at :550. (2) 'Hiding the trace' is overstated. In V-03 and in my captures the gold trace stays readable inside the grey block, with reduced contrast. (3) The root cause is only half right. In the pushed case the fill saturates because the plotted quantity, the post-limiter render peak per entry, is held at the Ceiling by the limiter itself. A dB mapping would also draw it at the top. The linear mapping is what squeezes the QUIET case (-20 dBFS at 10 % of the height; -3 dBFS sine at about 71 %).

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 4 · severity 1 · discoverability 2 · efficiency 1 · coherence 2 · change risk 2 · complexity 2 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-018

**The COMP and LIMITER GR lanes differ in meaning and are sampled without peak-hold: the COMP lane shows detector reduction that ignores Comp Mix, the LIMITER lane shows one block in four, and neither has GR, L/R or dB labels**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | medium | partially-confirmed | Visualization | The graphs are qualitative and partly mis-calibrated | Phase 2 |

**Evidence**

- e769f33:src/dsp/AnabasisEngine.cpp:786-790 — limGrDbCh = per-call per-channel minimum; compGrDbCh = comp.currentGainReductionDb(ch) at call end
- e769f33:src/dsp/MasteringComp.h:319-323 — per-channel GR getter returns the envelope, not Mix-weighted; mix applied separately at :295-300
- e769f33:src/gui/CurveView.h:77-87 — setGrDb stores and repaints the raw value (no hold or fall-back); :88-114 bars only, no text
- e769f33:src/gui/PluginEditor.cpp:1000 (startTimerHz 24) and :2093-2097 (lanes fed from the latest per-call atomics)
- e769f33:docs/user/USER_MANUAL.md:197-198, 208 ('its gain-reduction meter … two lanes … L above R') and :266-269 ('so the two read against each other directly')
- V-17 / LAY-06: session capture `rt/visuals/12a-adv-loud70-lim-panel.png`, session capture `rt/layout/04c-comp-bottom-bar.png`, session capture `rt/layout/04c-lim-bottom-bar.png` — unlabelled 14 px bars
- verify-20 R2: session capture `rt/verify-20/comp30-mix100.png` and session capture `rt/verify-20/comp30-mix50.png` — COMP lane about 5 dB at Mix 100 % and still about 6 dB at Mix 50 %
- verify-20 R4: rt/verify-20/lane_vals.txt and well-after-lane.png — lane max 5.8 dB vs history max about 6.6 dB over the same ~10 s

**Current behaviour.** Two 14 px right-anchored bars (L above R) at the foot of the COMP and LIMITER panels, on the shared 24 dB span, updated at 24 Hz from the latest processBlock's figures. The LIMITER lane shows that one call's deepest gain. The COMP lane shows the detector envelope at the end of the call, regardless of Comp Mix. There is no peak-hold, fall-back, number, caption, L/R tag or tick.

**Problem.** The manual presents the lanes and the history as directly comparable readouts of the same span. But the COMP lane over-reports effective reduction under parallel compression (Mix below 100 %), and the LIMITER lane can miss the deepest block that the history does show. Unlabelled, users cannot tell what either bar measures.

**Root cause.** The 0.1.2 lanes (ADR-0023 decision 10) reuse per-call atomics, sampled by the 24 Hz editor timer, with no common definition of 'stage GR' (effective vs detector) and no GUI ballistics.

**User impact.** Advanced users setting parallel compression read about 2-3 dB more reduction than is applied at 50 % Mix. Limiter-lane peaks under-read the history by a dB or more on transient material. The unlabelled bars add discovery cost. *Scope:* GrMiniMeter (CurveView.h), the editor timer feed (PluginEditor.cpp), and the engine per-stage publication (AnabasisEngine / MasteringComp getters).

**Proposed improvement.** Define one lane semantic: the effective reduction this stage applies, deepest since the previous frame, with a short peak-hold (e.g. about 1 s) and a fixed fall rate. For COMP, publish or derive the Mix-weighted effective reduction, or, if detector GR is kept by owner choice, caption it as such. For deepest-since-last-frame without a new handshake, the LIMITER lane's combined figure can be read from the GR history ring's newest entries. Per-channel lanes get GUI-side hold and fall ballistics on the sampled values. Add a stage-GR caption, L/R tags and scale marks per [VIS-006](findings-visualisation.md#vis-006) (maintainer wording, C8).

**Alternatives considered.**

- *Audio side publishes a min-since-last-read per channel (read-and-reset)* — Exact, but it is a new RMW handshake across threads: an ARCHITECTURE_REVIEW_GATE Thread Model item.
- *GUI-side hold and fall only* — No gate and cheap. It smooths the reading but still misses unsampled blocks. A good first step.
- *Keep detector GR in the COMP lane and document it* — A common convention in compressors; acceptable only if the lane is labelled as such and the manual says so.
- *Leave as-is* — Not justified. The Mix mismatch is a real misreading in a common Advanced adjustment.

**Decision: Modify · P2.** The core mismatches (Mix-blind COMP lane, sampled LIMITER lane, no labels) are code-confirmed and reproduced. 'Incomparable block-end vs block-min' is overstated. A constrained view-layer change (ballistics, ring-derived limiter peak, labels) plus an owner decision on effective vs detector COMP GR resolves it without a Thread Model gate.

**Architecture gates.**

- ARCHITECTURE_REVIEW_GATE Thread Model change — only if the audio side adds a read-and-reset (min-since-last-read) publication
- ADR-0023 decision 10 — the per-channel lanes are the KI-009 disambiguator; any COMP-lane re-definition must keep per-channel depth meaningful below 100 % link

**Dependencies.** [VIS-006](findings-visualisation.md#vis-006) (lane caption and scale marks); [VIS-007](findings-visualisation.md#vis-007) (per-stage numbers could share the same peak logic); [VIS-003](findings-visualisation.md#vis-003) (common definition of stage GR if clip and comp enter the history)

**Acceptance criteria.**

- compThreshold -30 dB with about 5 dB of detector GR: at Comp Mix 50 % the COMP lane reads the effective reduction (about 2.2 dB), or the lane is visibly captioned as detector GR with matching manual text.
- A single-block limiter event (e.g. one 10 ms burst) is shown in the LIMITER lane at its full depth for at least the hold time, at 48 kHz/512 and 48 kHz/64.
- Over any 10 s span the LIMITER lane's peak reading and the history trace's deepest point agree within 0.5 dB.
- Each lane shows a stage-GR caption and L/R tags (mono layout: no L/R), legible at UI scale XS.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/dsp/AnabasisEngine.cpp:764-791, e769f33:src/dsp/MasteringComp.h:268-300 and :308-323, e769f33:src/gui/CurveView.h:77-114, e769f33:src/gui/PluginEditor.cpp:1000 and :2093-2097, and USER_MANUAL.md:197-198, 208, 266-269. Reproduced on :150 (verify-20). R2: compThreshold -30 dB gave a COMP lane of about 4.5-7.5 dB (mean 5.2) at Comp Mix 100 % and about 4.7-9.0 dB (mean 6.9) at Mix 50 %. The lane does not follow Mix; the effective reduction at 50 % mix for 5 dB of detector GR is about 2.2 dB. R4: sampled the LIMITER lane about 60-70 times over 9.6 s, against the history trace over the same span.

**Corrections to the candidate claim.** (1) 'Cannot be compared' overstates the block-end vs block-min difference. The comp figure is a smoothed dB envelope (attack at least 5 ms, release at least 50 ms), so its end-of-block value is close to its in-block extreme. The material semantic gaps are elsewhere: the COMP lane is detector GR, not Mix-weighted (MasteringComp.h:319-323 vs mix at :295-300), and the lanes are sampled. (2) The GUI reads per-call atomics at 24 Hz (PluginEditor.cpp:1000) with no max-since-last-read, so at 48 kHz/512 about three of four blocks' minima are never displayed. The runtime comparison is consistent with this (lane peak 5.8 dB vs history peak about 6.6 dB; with clip drive 0, 12.6 vs 15.3 dB), but my capture itself sampled at about 7 Hz, so this is supporting rather than conclusive. (3) Flicker cannot be judged from stills. The large sample-to-sample swings seen (2-5.5 dB) follow the programme's 2 Hz kick, not meter noise.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 3 · severity 2 · discoverability 3 · efficiency 1 · coherence 3 · change risk 2 · complexity 2 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-019

**On hosts that deliver audio in bursts (render-ahead or anticipative processing), the GR trace jumps and then stalls; OQ-017 is still open and no one has measured REAPER or Cubase**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Investigate further** | **P2** | medium | partially-confirmed | Visualization | The graphs are qualitative and partly mis-calibrated | Phase 0 |

**Evidence**

- e769f33:src/gui/GrHistoryView.h:641-648 — smoothedHead = jlimit(head, head + 1, previous + dt/period). There is no slack below head, so a burst of n entries snaps the estimate forward by n entries.
- e769f33:src/gui/GrHistoryView.h:617-640 — the banner states 'never behind the data … either degrades to per-entry stepping at the host's cadence'.
- e769f33:docs/OPEN_QUESTIONS.md:544-590 — the measured burst response (1.447 px at n = 4 … 30.39 px at n = 64; a 4 s prefill gives 180.9 px in one frame, then a 3.97 s freeze). Options 1-3 are at :592-603, and the recommendation at :605-607 is 'None without the owner's REAPER observation'.
- e769f33:docs/OPEN_QUESTIONS.md:608-610 — Evidence [Verified for the display arithmetic, Unverified for the hosts].
- e769f33:src/PluginProcessor.cpp:1045-1049 — 'OQ-017's OTHER half … is untouched and still open … no measurement in this repository can supply one'.
- e769f33:docs/user/USER_MANUAL.md:258-259 — 'The trace scrolls continuously — it advances a fraction of a pixel with every processed block'. This is unqualified, and there is no mention of bursty hosts anywhere in the manual or in KNOWN_ISSUES (grep for burst/anticipative/ASIO-Guard: 0 hits).
- e769f33:docs/architecture/design-decisions/ADR-0038-gr-history-display-scalars-cross-the-painting-boundary.md:99-108 — clause 3's claim that every pairing is safe by value rests on 'smoothHead ≥ head holds for every published pair (smoothedHead's lower clamp)'.
- e769f33:tests/state_tests.cpp:6797-6798 — grPhase pins 'snaps FORWARD to a head the estimate did not expect, never behind the data', which is the current Option-1 behaviour.
- e769f33:docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md:64-66 — the host matrix's minimum is REAPER (Windows) plus Logic, but it has no GR-history motion item, which is the observation OQ-017 needs.
- Independent re-derivation: rt/verify-22/burst.py (output: n=4 → 1.447 px / 21.9 %; n=64 → 30.389 px / 95.2 %).
- *Added from another verifier's note:* Add a second render-ahead effect to [VIS-019](findings-visualisation.md#vis-019)'s host measurement. Entries are produced when audio is PROCESSED (GrHistoryView.h:641-648). On REAPER with anticipative FX, or Cubase with ASIO-Guard, the newest edge of the trace should therefore lead audible playback by the render-ahead depth, so GR appears before the audio is heard. This is inferred from the code and the host model, not measured. The OQ-017 REAPER/Cubase observation should record this lead as well as the stepping.

**Current behaviour.** The view anchors its scroll estimate to the newest ring entry and holds it within [head, head + 1]. Entries are timestamped by when they are PROCESSED. A host that processes n blocks in one burst therefore moves the whole trace (n − 1) entry-pitches in a single frame, lets it creep one entry, and then holds it still until the next burst. On a steady host the motion is exact.

**Problem.** On a render-ahead host, the display that the Simple view opens on would move in visible lurches and stalls, while the manual promises continuous scrolling. For such hosts the product cannot currently say whether this happens during normal playback or only at transport events, because no host has been measured.

**Root cause.** GrHistoryView::smoothedHead has a lower bound equal to the newest head, so there is no lag allowance, and the phase is referenced to processing time rather than to playback time. By the definition of render-ahead, entries on such a host also describe audio that has not been heard yet, so the trace would also LEAD playback by the render-ahead amount. That is an inference from the host model and has not been measured. The size of a lag allowance L is a property of the host, and no measurement in the repository sizes it (OQ-017).

**User impact.** If REAPER's anticipative FX (on by default) or Cubase's ASIO-Guard deliver in bursts during steady playback, users of those hosts see the GR trace jump and freeze on every pass, and the reduction events drawn may not line up with what they hear. If bursts happen only at play-start, seek or loop, the impact is a single lurch-and-freeze per transport event. The values themselves are correct in both cases, so no mastering number is wrong. *Scope:* GrHistoryView scroll estimation, in both the Simple and Advanced wells. It affects hosts that process ahead of real time: REAPER with anticipative FX, Cubase with ASIO-Guard, and Live's process-ahead according to worklogs/2026-09-01 §7 item 5. Steady-delivery hosts and the harness are unaffected. The spectrum view has no head or phase and is not affected (worklog 2026-09-05 §12.2).

**Proposed improvement.** Target experience: during steady playback on any host the trace scrolls at a constant speed. At play-start, seek or loop, the trace catches up by gliding at no more than about 2× nominal speed and never jumps. A steady host keeps today's zero-latency, exact motion. The way there is evidence first, then OQ-017 Option 3 if the evidence warrants it. That means an adaptive allowance [head − L, head + 1], with L learned from the observed maximum entries per arrival over a sliding window (L = 0 on a steady host) and a rate-limited catch-up rule. The phase/frame arithmetic and ADR-0038 clause 3's pairing argument would be re-derived for a phase that can be negative. In the meantime, qualify USER_MANUAL.md:258 to say that hosts which process ahead may step the trace.

**Alternatives considered.**

- *Leave as is (OQ-017 Option 1)* — Exact on steady hosts, and never ahead of the data by more than one entry. It is the right answer if REAPER and Cubase turn out to burst only at transport events. It cannot be chosen responsibly without the host measurement.
- *Fixed lag allowance L of 4-8 entries (Option 2)* — Simple, but it adds L entries of display latency on every host and still fails for render-ahead depths larger than L. REAPER's is described as hundreds of milliseconds in the worklog.
- *Adaptive L (Option 3)* — Fits every host and costs nothing on steady hosts. It is the most machinery, and it breaks ADR-0038 clause 3's smoothHead ≥ head invariant, so it needs a gated amendment.
- *Place entries by host playhead time (AudioPlayHead timeInSamples) instead of arrival* — This would also fix the lead-ahead of playback on render-ahead hosts. However, playhead availability and meaning vary by host, and transport-stopped processing has no playhead. That makes it heavier than a lag allowance and is out of proportion before the measurement exists.

**Decision: Investigate further · P2.** The display arithmetic is confirmed and I reproduced it independently, but whether real hosts produce bursts during steady playback, and how large they are, is exactly the missing evidence. OQ-017 is an open owner decision, and CLAUDE.md says open questions must not be guessed at. The evidence needed is this. In REAPER on Windows (the release checklist's minimum host) with anticipative FX on (the default) and off, and in Cubase with ASIO-Guard on and off, collect: (a) a diagnostic-build log of the wall-clock timestamp and numSamples of every processBlock call over 30 s of steady playback, plus play-start, a seek and a loop wrap; and (b) a real-refresh (≥60 fps) screen capture of the GR well over the same actions. That settles Option 1 versus 2/3 and gives L. The one-sentence manual qualifier can go in now.

**Architecture gates.**

- Conflict with Accepted ADR-0038 clause 3: its safe-by-value pairing argument rests on smoothHead ≥ head (smoothedHead's lower clamp). A lag allowance [head − L, head + 1] removes that invariant, so frameFor/phaseOf and grPair must be re-derived and ADR-0038 amended through the Architecture Review Gate.
- Threading model (Message → Painting boundary, ADR-0027 clause 4 / ADR-0038 clause 8): any new published scalar (e.g. the learned L) that paintHistory reads is gated.
- Open question OQ-017 is an owner decision and must not be guessed at (CLAUDE.md).

**Dependencies.** [TEST-003](findings-doc-test.md#test-003) (an in-tree frame-sequence harness is needed to pin whichever OQ-017 answer is chosen); OQ-017 owner decision

**Acceptance criteria.**

- OQ-017 has host logs and screen captures for REAPER (anticipative FX on/off) and Cubase (ASIO-Guard on/off) attached. For each host they state whether steady playback delivers more than one prepared block per callback, and the maximum entries per arrival at play-start, seek and loop.
- USER_MANUAL §3.4 no longer states unconditionally that the trace 'scrolls continuously'. It names hosts that process ahead as a case where the trace can step, until OQ-017 is resolved.
- If an allowance ships: with simulated bursts of n ≤ L entries at 48 kHz / 512 and 60 Hz, 0 % of frames are motionless during steady playback and the frame-to-frame travel σ is ≤ 0.01 px. Today the figure is 21.9 % motionless at n = 4.
- If an allowance ships: on a steady host (n = 1) the learned L is 0, the travel σ is ≤ 0.00004 px with 0 clamp bindings, and the display latency is unchanged from e769f33.
- If an allowance ships: after a 4 s prefill, no single frame moves the trace by more than 2× the nominal per-frame travel, and catch-up completes within a documented time.
- If an allowance ships: ADR-0038 is amended (clause 3) and accepted, and grPhase/grPair are updated to the new invariant and pass.

<details><summary>Verification record</summary>

**Method.** I read the smoothedHead clamp at e769f33:src/gui/GrHistoryView.h:641-648 and its banner at :617-640, OQ-017 at e769f33:docs/OPEN_QUESTIONS.md:544-610, the processor comment at e769f33:src/PluginProcessor.cpp:1045-1049, and ADR-0038 clause 3. I then re-implemented the clamp independently in rt/verify-22/burst.py (48 kHz / 512, 60 Hz frames, 0.482372 px per entry, bursts of n entries every n periods). It reproduced OQ-017's figures: jumps of 1.447 / 3.377 / 7.236 / 14.954 / 30.389 px at n = 4 / 8 / 16 / 32 / 64, with 21.9 / 61.0 / 80.5 / 90.3 / 95.2 % of frames motionless. At n = 1 it gave 0.754 px per frame and 0 % motionless, which matches the steady-host figures. This could not be reproduced in a real host: the audit harness delivers regular blocks, and there is no REAPER or Cubase in this environment.

**Corrections to the candidate claim.** (1) The unqualified claim 'The trace scrolls continuously' is at e769f33:docs/user/USER_MANUAL.md:258, just outside the cited :244-257. (2) The 180.9 px jump and 3.97 s freeze are OQ-017's simulated 4 s prefill, not an observation in a host. (3) None of the owner's three reports ([TEST-003](findings-doc-test.md#test-003)) was about bursts. They concerned the bucket-rate stepping in 0.2.8, vertex revision in 0.2.11 and the lead-out stub in 0.2.12, all on the steady-host path. It is the same display, but a different mechanism. (4) Whether REAPER or Cubase deliver in bursts during STEADY playback, rather than only at play-start, seek or loop, is unverified. OQ-017 itself says so ('Unverified for the hosts', :608-610).

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 2 · severity 2 · discoverability 2 · efficiency 1 · coherence 3 · change risk 4 · complexity 3 · evidence 3</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-020

**The GR history timeline has no event markers: A/B switches, preset loads, state loads and bypass leave no boundary, so one 20 s trace can mix two settings**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Defer** | **P3** | high | confirmed | Visualization | The session figures have no visible scope, liveness or tap, and a stray click wipes them | — |

**Evidence**

- e769f33:src/dsp/GrHistoryBuffer.h:104-108 — Entry { grDb, peak }: there is no event or slot field.
- e769f33:src/dsp/GrHistoryBuffer.h:217-224 — prepare() clears only when the (rate, block) pair changes.
- e769f33:src/dsp/AnabasisEngine.h:102-114 — prepareHistoryTimeline and resetHistoryTimeline. The latter has no production caller.
- e769f33:src/PluginProcessor.cpp:778-806 — the ring deliberately survives a re-prepare at the same pair so that the timeline continues (0.1.2 item 6).
- e769f33:docs/user/USER_MANUAL.md:265-267 — 'Pausing and resuming continues the timeline; it restarts only when the sample rate or block size changes'. The manual makes no claim about events.
- V-04 (obs/visuals.md:26-31): A/B, preset load, BYPASS, pill switch, ADV and state load all continue the trace with no mark. Screenshots: session capture `rt/visuals/10b2-after-AB-3s-well.png`, session capture `rt/visuals/10c-after-preset-next-editor.png`, session capture `rt/visuals/10d-bypass-on-editor.png`
- Reproduction on :152: session capture `rt/verify-22/gr-after-state-load-3s.png` (Loudness 60 % → 100 % → state load back to 60 %: three unlabelled segments). session capture `rt/verify-22/editor-after-AB.png` (after the A/B click the B slot is lit, and the trace continues with no boundary).

**Current behaviour.** The 20 s GR history is a continuous (GR, peak) series. A/B slot switches, preset '<'/'>' applies, host state loads and plugin BYPASS on/off leave no trace-level mark. The only visible hint is a change in the trace's character, if the change is large enough to see. A bypassed passage is recorded like any other segment, with no label.

**Problem.** While judging A/B, or after a preset step, the user cannot tell which part of the 20 s trace belongs to which setting, or whether a passage was bypassed. With subtle A/B differences the boundary cannot be seen at all.

**Root cause.** The history has no event channel. The ring stores only per-entry statistics, and nothing on the message thread records where in the entry stream a sound-changing action happened. The ring clears only on a changed prepared pair.

**User impact.** This is an attribution ambiguity for a secondary feedback display during comparisons. The values are correct, the user's ears and the loudness-matched A/B remain the primary judgement tools, and a large change is usually visible anyway. It is a clarity cost, not a wrong reading. *Scope:* GrHistoryView in both wells. The affected events are A/B switch, preset apply (browser and ‹ ›), setStateInformation and the BYPASS parameter. ADV and the GR/SPEC switch do not change the sound and need no marker.

**Proposed improvement.** The history should show thin vertical tick marks at the entry where the sounding configuration changed, and the marks should scroll with the trace. An A/B switch gets a tick labelled with the new slot ('A'/'B') at the top edge. A preset apply or state load gets a tick with a dot. A BYPASS span gets a translucent band over the bypassed entries. The mechanism: record events on the message thread by reading the ring's published head at the moment of the action, and hold them in a small fixed-capacity, view-owned list keyed by the ring epoch. Entries drop off when they leave the window and clear with the ring. There is no audio-thread write.

**Alternatives considered.**

- *Leave as is* — The values are correct, and large changes are visible from the trace's shape. It is acceptable as the v0.1 baseline, but it fails exactly in the subtle A/B comparisons the history could support.
- *Clear the history on A/B or preset apply* — Rejected. It destroys the comparison context and contradicts the deliberate continuity decision (0.1.2 item 6, PluginProcessor.cpp:778-806).
- *Tint the trace per A/B slot using a per-entry slot bit* — The clearest possible attribution, but it needs a ring schema change on the audio thread (an ADR-0011 time-series amendment, gated). That is out of proportion for this.
- *A/B markers only (smallest version)* — Covers the main comparison use with the least surface. It still needs the painting-boundary decision.

**Decision: Defer · P3.** This is a real clarity gap, confirmed at runtime and in code, but it is a new capability rather than a defect, and two prior decisions govern it. (1) Whether the meters and history keep measuring the dry signal during BYPASS (observation V-05). A bypass band only makes sense once what the history records during bypass is settled. (2) A marker list read by paintHistory is a new payload on the Message → Painting boundary, which ADR-0027 clause 4 and ADR-0038 clause 8 route to the Architecture Review Gate (ADR-0039 is the precedent mechanism). Revisit once V-05 is decided and the gate route is chosen.

**Architecture gates.**

- Threading model: a marker list written on the message thread and read by paintHistory on the GL render thread (macOS/Windows) is a new payload on the Message → Painting boundary. ADR-0027 clause 4 and ADR-0038 clause 8 route it to the Architecture Review Gate (ADR-0039 is the precedent).
- If events are stored in the ring instead: changing GrHistoryBuffer::Entry is an ADR-0011 time-series/protocol amendment (gated), and the ring's slot memory under ADR-0040 changes.

**Dependencies.** V-05 (bypass metering: what the history records while BYPASS is on); V-08 (statistics reset scope: a shared 'what does this measurement span' event model); [VIS-019](findings-visualisation.md#vis-019) (a lag allowance would shift where a marker lands relative to the trace); [TEST-003](findings-doc-test.md#test-003) (a frame-sequence harness to verify that markers scroll rigidly with the trace)

**Acceptance criteria.**

- With music playing and the GR well visible, clicking A/B places a tick at the trace's newest edge within one frame, labelled with the new slot. It scrolls left at exactly the trace's speed, with 0 px drift relative to the trace over 20 s.
- Preset '<'/'>' apply, a browser load and host setStateInformation each place a distinguishable marker.
- BYPASS on→off draws a band covering exactly the bypassed entries, to within ±1 entry.
- ADV toggle, the GR/SPEC switch and a Settings change that does not alter the sound place no marker.
- Markers leave with the 20 s window and clear together with the ring on a (rate, block) change, keyed by the ring epoch.
- No new audio-thread write and no allocation in the paint or tick path. TSAN is clean, and the gate item is recorded and accepted before merge.

<details><summary>Verification record</summary>

**Method.** Code: GrHistoryBuffer::Entry holds only grDb and peak (e769f33:src/dsp/GrHistoryBuffer.h:104-108). prepare() clears only on a changed (rate, block) pair (:217-224). resetHistoryTimeline (e769f33:src/dsp/AnabasisEngine.h:109-114) has no production caller: grep across src/ finds only its definition and header comments. I viewed V-04's screenshots 10b2 (A/B), 10c (preset '>') and 10d (BYPASS): the trace continues with no mark. I also reproduced it on display :152 with stepped pointer motion. I saved state, raised Loudness to 100 %, loaded the state, then clicked A/B. The trace continued through all of it with no boundary (session capture `rt/verify-22/gr-after-state-load-3s.png`, session capture `rt/verify-22/editor-after-AB.png`).

**Corrections to the candidate claim.** The cited session capture `rt/visuals/20b-gr-after-state-load-0.8s-well.png` shows the SPEC view, because the loaded state restored the saved graph-well mode. It therefore does not show the GR trace after a state load, and my own reproduction (gr-after-state-load-3s.png) replaces it. The claim itself stands. Also, 'parameter jumps' in the title is broader than what was observed: a single knob move is continuous and arguably needs no marker. The observed cases are A/B, preset apply, state load and BYPASS.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 3 · severity 2 · discoverability 3 · efficiency 2 · coherence 2 · change risk 3 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-021

**The spectrum reads 6.02 dB hot, so every tonal component between -6 dBFS and 0 dBFS collapses into the same flat plateau at the top of the fixed -90..0 dB range. The input trace is dim (about 2.6:1 contrast), and its lead over the output is real but negligible.**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | partially-confirmed | Visualization | The graphs are qualitative and partly mis-calibrated | Phase 2 |

**Evidence**

- e769f33:src/gui/SpectrumView.h:395-396 — `juce::dsp::WindowingFunction<float> window { kSize, hann }` is built with JUCE's default normalise=true (juce-src juce_Windowing.h:73-74; juce_Windowing.cpp:161-171 scales the table so its sum equals size, i.e. coherent gain 1)
- e769f33:src/gui/SpectrumView.cpp:202-203 — the comment 'Hann coherent gain 0.5; normalise so a full-scale sine reads ~0 dB' and `norm = 2.0f / ((float) kSize * 0.5f)`, which applies the 0.5 compensation a second time: +6.02 dB
- e769f33:src/gui/SpectrumView.cpp:829-830 (fLo/fHi 20-20k, dbLo -90, dbHi 0); :912 jlimit(dbLo, dbHi) clamps each column, which produces the flat top
- e769f33:tests/state_tests.cpp:8503-8505 + :8532 — the project's own measurement: a 0.5f-amplitude bin-centred 6 kHz tone reads '−0.00 dB' (also ADR-0039 line 112), i.e. -6.02 dBFS displays as 0 dB
- Anamorph fd78c3b:src/gui/SpectrumImager.cpp:794 — the sibling uses norm = 2/N with the same default-normalised window (SpectrumImager.h:175)
- e769f33:src/gui/SpectrumView.cpp:924-927 — input drawn as textDim at alpha 0.55 and 1.0 px, output as accent at 1.3 px, input drawn first; e769f33:src/gui/LookAndFeel.h:47,52 (bg 0e1014, textDim 8b94a3)
- e769f33:src/dsp/AnabasisEngine.cpp:874-878 — spectrum tap 1 is post-InputGain, pre-everything (not delayed); :1243-1266 — tap 2 is the post-chain render, delayed by the latency
- Runtime (verify-23, :153): session capture `rt/verify-23/sine750_0.png`, sine750_-6.png, sine750_-12.png, sine750_-18.png — peak rows y=657/657/662/668 with the floor at 749.5; session capture `rt/verify-23/sine750_-6-well.png` shows the -6 dBFS tone as a flat-topped plateau
- Runtime (verify-23): session capture `rt/verify-23/sweep-1.png`, sweep-2.png, sweep-3.png — grey leads gold by 1-2 px (latency=480 samples, logged in rt/verify-23/app.log)
- Runtime (verify-23): session capture `rt/verify-23/music-l70-3-well.png` — input trace visible about 5 dB under the output; measured AA pixel colours (59-79, 64-85, 72-95) on bg (16,19,24), contrast 2.5-2.7:1
- Observation V-14 with session capture `rt/visuals/05b-sine1k-3-spec-well.png` (a -3 dBFS sine drawn at the clamp), 06b-square200-spec-well.png (fundamental plateau), 07b-sweep-6-spec1-well.png (grey offset right)

**Current behaviour.** Each bin is displayed 6.02 dB above its true level: a bin-centred sine at X dBFS draws at X+6 dB. The plot is clamped to -90..0 dB, so any tonal component between about -6 dBFS and 0 dBFS draws as the same flat-topped plateau. At runtime a -6 dBFS and a 0 dBFS 750 Hz tone are indistinguishable. The input trace is 1 px of textDim at 55 % alpha under the output; it is hidden where the traces coincide and faint where they separate. It leads the output by the plugin latency (1-2 px on a fast sweep). There is no peak hold.

**Problem.** The display is wrong by 6 dB, and the error removes the top 6 dB of dynamic range. That is the region where a maximizer's output lives (ceiling -0.1..-1 dBFS) and where limiting acts. When input and output are both clamped there, the in/out difference the view exists to show is erased, and the flat top looks like clipping. The code comment and the sibling both say 0 dBFS should read about 0 dB. The low-contrast input trace (below 3:1) makes the comparison harder to read.

**Root cause.** The Hann coherent gain is compensated twice. JUCE's WindowingFunction normalises the table to unit DC gain by default (SpectrumView.h:395-396 passes no `normalise` argument), and SpectrumView.cpp:203 divides by N*0.5 as if the raw window were used. The fixed dbHi = 0 (:830) with a hard clamp (:912) then turns the offset into a plateau. The input dimness is a colour choice (:924). The lead is the tap placement (the engine's input tap is undelayed), which is specified by DESIGN §2.9.

**User impact.** A user who opens SPEC on hot tonal or bass-heavy material sees the loudest components flattened into identical plateaus. Their input and output cannot be compared there, which invites a wrong 'bass is clipping / nothing changed up top' reading. The well has no dB scale (LAY-12), so the offset cannot be noticed or corrected mentally. There is no audio impact. *Scope:* SpectrumView only: both traces, in both views, at every sample rate. Only the display: no audio, parameter or state is affected. The plateau needs per-bin content above about -6 dBFS, which the harness's synthetic music at Loudness 70 did not reach.

**Proposed improvement.** 1) Calibrate: use norm = 2/N (the sibling's constant) or construct the window with normalise=false and keep 4/N. A bin-centred full-scale sine then reads 0.0 dB ±0.1, and the -3 dBFS 1 kHz sine reads about -3.5 dB with visible space above it. Correct the :202 comment and the '−0.00 dB' measurement text in state_tests.cpp:8503-8505. 2) Give the top a little headroom: dbHi of about +3 dB, or keep 0 but mark clamped columns. Limited or clipped material whose fundamental exceeds 0 dBFS (a square's fundamental is +2.1 dB) is then drawn rather than flattened. 3) Raise the input trace to at least 3:1 contrast, for example textDim at alpha 0.75-0.8 or a 1.2 px stroke, while keeping it clearly subordinate to the accent output. Do NOT delay-align the taps and do NOT add a peak hold as part of this finding.

**Alternatives considered.**

- *Leave as is* — Rejected: a measurement display that is 6 dB wrong, contradicts its own comment and diverges from its ported source.
- *Only raise dbHi to +6 dB (the finding's suggested root cause)* — Hides the symptom and keeps every reading 6 dB wrong. Any later dB axis (LAY-12) would then be mislabeled. Calibration is the fix; headroom is secondary.
- *Delay-align the input trace (a delayed engine tap, or reading the input ring at head − latency)* — Not justified. The offset is at most about 1-2 frames of a 120 ms EMA. It would touch DESIGN §2.9's tap definition, or ADR-0039's one-pair-one-span read, for an effect of about 2 px on a 5 s sweep.
- *Add peak hold / trail* — A feature request with no evidence of harm; defer to product decision.

**Decision: Modify · P2.** The verified defect is the 6.02 dB calibration error, plus a clamp with no headroom. Fixing it is a one-constant change that restores what the code, the comment and the sibling all intend. Of the merged sub-claims, input visibility justifies only a small contrast bump. The alignment lead is confirmed but negligible, so it is not addressed. Peak hold is not a defect. P2, not P0/P1: the spectrum is the non-default view (GR is default since ADR-0023 §7), it has no numeric scale, and the plateau needs tonal content above -6 dBFS per bin. It is still a correctness error in a measurement display, and it hides exactly the top region a maximizer works in.

**Dependencies.** LAY-12 (no dB/frequency axes on the graph well) — calibrate BEFORE any dB axis is added, or the labels will be 6 dB wrong; [UI-007](findings-ui.md#ui-007) (the pill sits over the plot's floor/LF corner)

**Acceptance criteria.**

- A bin-centred 0 dBFS sine (both channels) produces a peak bin of 0.0 dB ±0.1 in analysedOutDb()/analysedInDb(). A unit test pins this so a double compensation cannot return.
- At runtime a 750 Hz sine at -6 dBFS and at 0 dBFS (limited to -0.1) draw at visibly different heights, about 6 px apart in the Simple view at 1x, and neither is flat-topped.
- A 200 Hz square at -0.5 dBFS draws its fundamental as a peak, not a plateau: with headroom, or with clamped columns explicitly marked.
- The input trace measures ≥ 3:1 contrast against the well background and remains visually subordinate to the output trace.
- The SpectrumView.cpp:202 comment and the state_tests.cpp specAxis measurement text match the new calibration.

<details><summary>Verification record</summary>

**Method.** Read every cited anchor at e769f33. Also read the window construction in SpectrumView.h:395-396 and JUCE's WindowingFunction (pinned juce-src, juce_Windowing.h:73-74 and juce_Windowing.cpp:161-171): `normalise` defaults to true, which already scales the Hann window to DC/coherent gain 1. Reproduced the arithmetic in numpy with JUCE's hann formula and normalisation and norm = 2/(N*0.5): a bin-centred full-scale sine reads +6.02 dB, and a -3 dBFS 1 kHz sine reads +2.39 dB, so it is clamped. Found the project's own measurement of the same offset: a 0.5-amplitude (-6.02 dBFS) bin-centred 6 kHz tone, fed at e769f33:tests/state_tests.cpp:8532, is recorded as reading -0.00 dB (state_tests.cpp:8503-8505; ADR-0039 line 112). Runtime on my display :153, harness Simple view at 1x, default parameters: a bin-centred 750 Hz sine at 0, -6, -12 and -18 dBFS. The peak rows were y=657 (0 dBFS limited to -0.1), 657 (-6), 662 (-12) and 668 (-18), with the floor at 749.5 and about 1.02 px/dB. So 0 dBFS and -6 dBFS draw identically at the clamp, and -12 dBFS draws at about -5 dB. Log sweep, 20 Hz-20 kHz in 5 s, with the reported latency of 480 samples: the grey input trace leads the gold by 1-2 px at three heights on three frames. That matches the predicted 1.8 px (10 ms x 0.6 decades/s over a 3-decade, ~904 px axis). Music at -6 with Loudness 70: the grey input trace sits about 5 dB under the gold and can be seen, but it is dim. Measured contrast of the 55 %-alpha textDim trace against the well background: 2.48-2.68:1. Viewed rt/visuals/05b, 06b, 07b (zoomed), 07d and 09b.

**Corrections to the candidate claim.** 1) The main cause of the plateau is a calibration error, not only the missing headroom above 0 dB. SpectrumView.cpp:202-203 compensates for a 0.5 Hann coherent gain that JUCE's default-normalised window (SpectrumView.h:395-396) has already removed. Every reading is therefore +6.02 dB. The code comment 'normalise so a full-scale sine reads ~0 dB' is false; a full-scale sine reads +6 dB. The sibling this is ported from uses norm 2/N (Anamorph fd78c3b:src/gui/SpectrumImager.cpp:794), so Anabasis diverges from its provenance. 2) The input/output misalignment is real: the input is tapped pre-lookahead and the output post-latency. But it equals the plugin latency (10 ms at defaults) inside an 85 ms window with a 120 ms EMA. It is 1-2 px on a fast 5 s sweep and zero on steady material, so it does not compromise the comparison. 'Compromised on moving material' is overstated. 3) 'Barely visible' is overstated. The input is hidden only where it coincides with the output, because it is drawn underneath by design. Where the traces separate it is visible but low-contrast (2.5-2.7:1). 4) 'No peak hold' is true: the EMA has instant attack and ~120 ms release. It is a feature absence with no evidence of harm. 5) I could not reproduce the plateau with the harness's synthetic music at Loudness 70: the LF peaked about 11 dB below the top. It needs tonal content above about -6 dBFS per bin (tones, squares, and probably sustained sub-bass). Real bass-heavy masters were not tested.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 2 · severity 3 · discoverability 4 · efficiency 2 · coherence 4 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-022

**The spectrum mono-sums L+R, so side-only or out-of-phase content is invisible or under-reads, and the manual does not say so**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | confirmed | Documentation | The graphs are qualitative and partly mis-calibrated | Phase 2 |

**Evidence**

- e769f33:src/gui/SpectrumView.cpp:198 — `fftData[(size_t) (off + i)] = 0.5f * (srcL[i] + srcR[i]);`
- e769f33:src/gui/SpectrumView.h:29 — banner: 'Display: 4096-point Hann FFT, mono-summed, log-f 20 Hz–20 kHz, −90..0 dB'
- e769f33:docs/user/USER_MANUAL.md:235 — 'Spectrum — the input/output spectrum overlay (input dim, output in the accent).' No mention of the mono sum
- e769f33:docs/DESIGN.md:292-295 — two capture points, 'FFT'd on the GUI side and drawn as a dual-trace overlay'; channel handling is unspecified
- e769f33:docs/DEVELOPMENT_BRIEF.md:152 — 'Spectrum: overlaid input/output display, dismissible' (no stereo requirement); :630 — the product has no vectorscope/correlation metering
- Anamorph fd78c3b:src/gui/SpectrumImager.cpp:589 — same 0.5*(L+R) sum (family consistency)

**Current behaviour.** Both traces show the spectrum of (L+R)/2. Perfectly anti-phase content draws at the -120 dB floor. Decorrelated wide content reads about 3 dB lower than the same per-channel level would if correlated. Side-only changes made by processing do not appear.

**Problem.** The limitation is undocumented where users look (the manual). A user comparing Anabasis's spectrum with a stereo-max or per-channel analyser sees wide material read lower, and would not know that anti-phase low end simply vanishes. Nothing else in the product shows stereo content.

**Root cause.** A deliberate single-FFT-per-trace design, inherited from the sibling's analyser and recorded only in the class banner. No recorded rationale beyond that.

**User impact.** Low. For a maximizer's in/out comparison the mono sum represents the stereo-linked tonal and dynamic changes adequately. The cost is a misread on wide or phase-problem material, and a mild mismatch against other analysers. It could also be argued as useful mono-compatibility information, once the user knows. *Scope:* Both spectrum traces, all configurations. Display only.

**Proposed improvement.** Add one sentence to USER_MANUAL §3.4's Spectrum bullet: 'the spectrum shows the mono sum (L+R)/2 of each signal; content that is out of phase between the channels cancels in it.' Keep the SpectrumView.h:29 banner. Any stereo mode (per-bin power sum |L|²+|R|², max(L,R), or M/S) is a product decision to record in OPEN_QUESTIONS, not something to guess at here.

**Alternatives considered.**

- *Switch to a per-bin power sum (two FFTs per trace, i.e. four per tick)* — Shows wide content at its true level and never cancels. It doubles the GUI-side FFT cost, diverges from the sibling, and changes what the display means. Worth considering only as an owner-decided mode.
- *Add an M/S or L/R selector* — Adds UI surface and a persisted setting (a serialization addition). Out of proportion to the evidence.
- *Leave undocumented* — Rejected: the only cost of fixing is one line of documentation.

**Decision: Modify · P3.** The behaviour is confirmed and deliberate, and consistent with the sibling. Evidence of harm is limited to the undocumented blind spot. The constrained fix is documentation. Changing the analysis is deferred to an owner decision because it changes what the view means.

**Architecture gates.**

- If a stereo/M-S mode with a persisted choice were pursued instead: serialization schema change (hard stop) and an OPEN_QUESTIONS entry; the documentation-only change touches no gate

**Dependencies.** [VIS-021](findings-visualisation.md#vis-021) (same view; document calibration and summing together)

**Acceptance criteria.**

- USER_MANUAL §3.4 states that the spectrum shows the mono sum (L+R)/2 and that anti-phase content cancels in it.
- The SpectrumView.h banner and the manual describe the same channel handling.
- (If a stereo mode is ever added) a test feeding L = -R shows a non-floor trace in that mode.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/SpectrumView.cpp:196-199: `fftData[off+i] = 0.5f * (srcL[i] + srcR[i])` before windowing, for both traces (the same `analyse` serves in and out). The header banner at SpectrumView.h:29 states 'mono-summed'. For R = -L, x + (-x) is exactly +0.0f in IEEE float, so the FFT input is all zeros and every bin takes the -120 dB floor. For uncorrelated equal-level channels the summed power is half that of a correlated pair (-3 dB). Checked the manual (e769f33:docs/user/USER_MANUAL.md:235) and DESIGN §2.9 (e769f33:docs/DESIGN.md:292-295): neither mentions the sum. The sibling mono-sums identically (Anamorph fd78c3b:src/gui/SpectrumImager.cpp:589). Runtime: not reproduced, because the harness signal set (silence/sine/pink/music/burst/square/sweep) has no L=-R or side-only source. The arithmetic is exact, so runtime adds nothing.

**Corrections to the candidate claim.** The suspected root cause, 'mono-summed to fit the per-tick message-thread budget', has no support in the code or docs. The only record is the banner's bare statement 'mono-summed' (SpectrumView.h:29), and it matches the sibling's port. Because both traces are summed identically, the in/out comparison stays valid for mid content; the blind spot is only for side-only changes and anti-phase content.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 2 · severity 2 · discoverability 4 · efficiency 1 · coherence 2 · change risk 1 · complexity 1 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-023

**Display resolution degrades at high sample rates and large prepared blocks: a fixed 4096-point FFT blurs the spectrum's low end already at 96 and 192 kHz, and a 4096-sample prepared block coarsens the GR history to ~85-93 ms per point**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | confirmed | Visualization | The graphs are qualitative and partly mis-calibrated | Phase 2 |

**Evidence**

- e769f33:src/gui/SpectrumView.h:60-62 — kOrder = 12, kSize = 4096, kBins = 2048, all constant at every rate.
- e769f33:src/gui/SpectrumView.cpp:829 — fLo = 20, fHi = 20000 on a log axis. Bins are rate / 4096: 11.7 Hz at 48k, 23.4 at 96k, 46.9 at 192k and 93.75 at 384k.
- e769f33:src/gui/SpectrumView.cpp:194-199 — each tick transforms the newest kSize frames: 85 ms at 48k, 21 ms at 192k and 10.7 ms at 384k. At 384k with 60 Hz ticks, roughly a third of the audio would therefore never be analysed. That is inferred from the code and not measured.
- e769f33:src/dsp/ScopeBuffer.h:116 — capacity = 16384 frames, which bounds any longer read.
- e769f33:src/PluginProcessor.cpp:1022-1031 and :806 — one GR entry per PREPARED block, the host's declared maximum.
- e769f33:src/gui/GrHistoryView.h:606-610 — entryPeriod = block / rate: 10.7 ms at 48k/512, 85 ms at 48k/4096, 93 ms at 44.1k/4096.
- e769f33:docs/architecture/design-decisions/ADR-0039-spectrum-frame-publication.md:136,216,417 — the published frame is sized to 2048 + 2048 bins, allocated at construction, with a measured cost of 210.5 µs per tick.
- E15 (obs/edges.md:111-116): session capture `rt/edges/48-spec-384k-48k-44k.png`, session capture `rt/edges/48a-sr44k4096-loud60-gr-crop.png`, session capture `rt/edges/46-gr-well-44k-4096.png`
- Reproduction on :152: session capture `rt/verify-22/spec-48-96-192-strip.png` (the LF structure at 48k becomes a smooth interpolated curve at 96k and 192k)
- Reproduction on :152: session capture `rt/verify-22/gr-48k-blocks-strip.png` (512/1024/2048 look alike; 4096 visibly loses the attack shape)

**Current behaviour.** The spectrum always uses a 4096-point FFT over the newest 4096 samples. At 96 kHz and above, the lowest octave or more of the 20 Hz-20 kHz axis is drawn by Catmull-Rom interpolation between a handful of bins, as a smooth curve that looks like measured data. The GR history stores one point per prepared block, so at a 4096-sample prepared block it becomes a polyline with 85-93 ms segments.

**Problem.** In 96/192 kHz sessions the spectrum's low end, where kick and bass decisions are made, shows no real detail, and nothing tells the user it is interpolated. At 384 kHz that region is meaningless. The GR history's coarseness at 4096-sample prepared blocks is a smaller, honest loss of detail.

**Root cause.** There are two independent mechanisms. (a) The FFT length is fixed in SAMPLES rather than in time or frequency resolution, so bin spacing grows and the analysis window shrinks linearly with rate. (b) The GR history's time base is one entry per prepared block (ADR-0011's 2026-09-07 amendment), so its temporal resolution is block / rate by design.

**User impact.** Users mastering at 96/192 kHz get an unreliable low-frequency spectrum. The input and output traces are equally blurred, so broad tonal deltas remain comparable, but narrow LF features cannot be resolved. Users at very large prepared blocks see a coarser GR trace. No meter value is wrong. *Scope:* SpectrumView at rates above 48 kHz, and GrHistoryView at prepared blocks of 4096 samples or more. The spectrum is not the default well view (the GR history is).

**Proposed improvement.** Modify, spectrum only. Keep the analysis's frequency resolution roughly constant across rates: at rates above ~48 kHz, read rate/48k × 4096 frames, low-pass and decimate them to ~48 kHz, then run the existing 4096-point FFT. The display tops out at 20 kHz, so nothing drawn is lost, the published frame keeps its 2048 bins and ADR-0039's payload is unchanged. At 352.8/384 kHz this needs a read longer than the ScopeBuffer's in-flight-safe window, so either grow ScopeBuffer capacity or cap the decimation factor and state the residual. Preserve the GR history's per-prepared-block resolution. It is the documented, honest time base (the manual already says it records 'one point per buffer', USER_MANUAL.md:250-251), and finer resolution would need sub-block entries (an ADR-0011 amendment) plus N× ring memory (ADR-0040) for a gain visible only at 4096-sample prepared blocks or more.

**Alternatives considered.**

- *Leave both as is* — Acceptable for the GR history. For the spectrum it leaves a smooth curve that looks like data but is not, in common high-rate sessions.
- *Scale the FFT order with rate (13 at 96k, 14 at 192k, 15 at 384k)* — This achieves the same resolution, but it changes ADR-0039's published payload size, its 48 KB-per-view figure and its measured tick cost (roughly 2-8×), and order 15 exceeds the ScopeBuffer capacity. That is heavier than decimation.
- *Show an 'LF resolution reduced' hint, or grey out the interpolated region, at high rates* — Cheap and honest, but it discloses the loss instead of fixing it. It is a reasonable interim step or companion.
- *Sub-block GR entries for large prepared blocks* — Rejected for now. It needs gated ADR-0011/ADR-0040 changes for a cosmetic gain.

**Decision: Modify · P3.** The spectrum degradation reaches ordinary mastering rates (96/192 kHz), and nothing tells the user the low end is interpolated. That is worth addressing with the smallest change that keeps ADR-0039's payload intact: decimate before the FFT. The GR history's coarseness is the product's own documented time base, visible only at very large prepared blocks, and fixing it would cost gated protocol and memory changes, so that half should stay as it is.

**Architecture gates.**

- ADR-0039 (Accepted): if the FFT order is raised instead of decimating, the published frame's size (kBins, 4099 values, 48 KB per view) and its recorded cost change, which needs an amendment through the gate. The decimation route keeps the payload identical.
- ScopeBuffer capacity (e769f33:src/dsp/ScopeBuffer.h:116; an ADR-0009 copy-and-adapt of Anamorph's ring): growing it for 352.8/384 kHz changes audio-side memory. The protocol is unchanged, and its provenance header must record the divergence.

**Dependencies.** None.

**Acceptance criteria.**

- A two-tone test signal (40 Hz + 55 Hz, equal level) shows two resolved peaks, each with ≥ 6 dB of dip between them, at 44.1, 48, 88.2, 96, 176.4 and 192 kHz.
- For the same programme, the output trace between 20 and 200 Hz at 96 kHz and at 192 kHz stays within 2 dB per column of the 48 kHz trace.
- ADR-0039's published frame keeps 2048 + 2048 bins, and the spectrum tick at 192 kHz costs at most 2× the 48 kHz tick on the same machine.
- At 384 kHz, either the same two-tone resolution holds, or the residual is stated in the manual or disclosed on screen.
- The GR history ring, its entries and its per-prepared-block time base are byte-identical to e769f33.

<details><summary>Verification record</summary>

**Method.** Code: kOrder = 12 (e769f33:src/gui/SpectrumView.h:60-62), a log axis from fLo 20 to fHi 20000 (e769f33:src/gui/SpectrumView.cpp:829), and each tick reads only the newest kSize frames (:194-199). GR entries are one PREPARED block (e769f33:src/PluginProcessor.cpp:1022-1031, :806), and entryPeriod = block / rate (e769f33:src/gui/GrHistoryView.h:606-610). I viewed E15's screenshots 48-spec-384k-48k-44k.png, 48a and 46. I reproduced on display :152 (music at -6 dB, Loudness 60 %, stepped pointer motion to the pill). The spectrum at 48k, 96k and 192k with 512-sample blocks is in session capture `rt/verify-22/spec-48-96-192-strip.png`. The GR history at 48 kHz with 512, 1024, 2048 and 4096-sample blocks is in session capture `rt/verify-22/gr-48k-blocks-strip.png`.

**Corrections to the candidate claim.** The spectrum part is broader than claimed. The low-frequency smoothing is not confined to 384 kHz: it is already visible at 96 kHz (about the left fifth of the axis, 23.4 Hz bins) and at 192 kHz (about the left third, 46.9 Hz bins). Both are ordinary mastering rates. 'Large host blocks' should read 'large PREPARED (maximum) blocks': an entry is the prepareToPlay maximum, not the delivered size, so a host that declares a large maximum and delivers small buffers also gets coarse entries. The GR coarsening is modest. At 48 kHz, 512 to 2048 samples were indistinguishable at 2× zoom, and only 4096 visibly lost the transient shape. It is an honest decimated series, not a wrong one.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 2 · severity 2 · discoverability 3 · efficiency 1 · coherence 1 · change risk 3 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### VIS-024

**The spectrum under-reads high-frequency tonal content by tens of dB: above about 2.3 kHz (48 kHz, Simple at M), each pixel column is the dB-domain MEAN of every covered FFT bin, so a tone's peak is averaged with its empty neighbours. A 5.86 kHz sine draws near -53 dB; a -12 dBFS 750 Hz sine draws within a few dB of the top.**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | recorded at triage | Visualization | The graphs are qualitative and partly mis-calibrated | Phase 2 |

**Evidence**

- e769f33:src/gui/SpectrumView.cpp:886-897 — dbForColumn: span < 1.5 bins → dbCubic; otherwise the arithmetic mean of bins[ka..kb] (inclusive floor/ceil span), and the bins hold dB values
- e769f33:src/gui/SpectrumView.cpp:832-866 — comment: 'BOTH REGIMES WORK IN THE dB DOMAIN … A dB-domain mean IS a geometric mean of magnitudes … matching it is what preserves the family's display behaviour'
- e769f33:src/gui/SpectrumView.cpp:195-213 — per-bin dB = gainToDecibels(mag·norm, -120), attack-instant / ~120 ms decay EMA; bins floor at -120 dB
- e769f33:src/gui/SpectrumView.cpp:829-830 and :912 — 20 Hz-20 kHz log axis, -90..0 dB, jlimit per column
- e769f33:src/gui/SpectrumView.h:60-62 — kSize 4096, so 11.72 Hz bins at 48 kHz. On a ~913 px well the 1.5-bin boundary falls near 2.3 kHz, and it moves with sample rate, view width and UI scale
- Anamorph fd78c3b:src/gui/SpectrumImager.cpp:676-690 — the sibling's magForColumn uses the identical reducer over dB values (its floor is -90 dB, not -120)
- Runtime verify-23 (:153): session capture `rt/verify-23/01-after-click-spec.png` — bin-centred 5859.375 Hz sine peaks at y=712 against the -90 dB floor at y=749.5 (about -53 dB displayed)
- Runtime verify-23: .../rt/verify-23/sine750_0.png, sine750_-6.png, sine750_-12.png, sine750_-18.png — the 750 Hz tone peaks at y=657/657/662/668; at -12 dBFS it draws within a few dB of the top
- Arithmetic, including [VIS-021](findings-visualisation.md#vis-021)'s +6.02 dB offset: a column covering bins 498-502 around a -12 dBFS tone gives (-120 -12 -6 -12 -120)/5 ≈ -54 dB
- e769f33:docs/architecture/design-decisions/ADR-0009-code-reuse-from-anamorph.md:111-113 — a copied file 'is thereafter an Anabasis file: it is maintained here, reviewed here, and diverges here'

**Current behaviour.** Each pixel column of the spectrum trace comes from dbForColumn.
- Where a column spans fewer than 1.5 FFT bins (low and mid frequencies), the value is a Catmull-Rom interpolation of bin dB values. This preserves a tone's peak.
- Where a column spans 1.5 bins or more, the value is the arithmetic mean of the dB values of every covered bin, over an inclusive span of about 5-10 bins. This starts near 2.3 kHz at 48 kHz in the Simple view at M.

A Hann-windowed tone occupies about 3 bins, so in that regime its peak is averaged with bins at the programme's floor (down to -120 dB in near-silence). An isolated HF tone therefore draws tens of dB below the same tone at LF. On dense programme the dilution is smaller, but HF tonal peaks still read low, and broadband HF shows a geometric rather than a power mean.

**Problem.** The analyser's response to tonal content is not flat across frequency. The same tone draws at very different heights depending on which side of the regime boundary it falls. HF resonances, whistles, sibilant tones and harmonic lines read far lower than they are, which is exactly what a mastering user inspects when choosing Tone, Tilt or HS moves. The HF slope of the trace looks steeper than the programme. [VIS-021](findings-visualisation.md#vis-021)'s +6 dB offset flattens LF tones against the top of the plot, and together the two defects exaggerate the on-screen LF/HF imbalance.

**Root cause.** The column reducer was ported verbatim from Anamorph's magForColumn, whose array holds dB values. Averaging dB values is a geometric mean of magnitudes, and the lowest bins dominate it. The code comment defends this as preserving family display behaviour. No test checks that tone heights stay flat across the regime boundary. Anabasis's -120 dB bin floor (Anamorph uses -90) makes the dilution deeper in near-silence.

**User impact.** A user reading the SPEC view to judge top-end tonal balance or find an HF resonance sees it much lower than it is. They may miss it, or add HF (Tone, Tilt, HS gain) to correct a darkness that is a display artefact. The spectrum is not the default well view (GR is, ADR-0023 item 7), so this affects users who switch to SPEC. The magnitude on real music is smaller than on the synthetic tone but has not been measured.

**Proposed improvement.** Paint-side only, in dbForColumn's averaging regime: reduce the covered bins by their MAXIMUM, which preserves peaks and is the usual analyser convention when many bins share a pixel. Keep the Catmull-Rom regime for sparse columns, and check that the two regimes join without a visible step.

Land this together with [VIS-021](findings-visualisation.md#vis-021)'s calibration fix, so one set of tests pins absolute level and flatness across frequency.

Update the SpectrumView.cpp:832-866 comment and its provenance note to record the deliberate divergence from Anamorph's reducer; ADR-0009 decision 2 allows a copy to diverge. Tell the owner that the read-only sibling carries the same reducer.

The bins published in ADR-0039's frame are unchanged, and there is no DSP, parameter or state change.

**Alternatives considered.**

- Power-domain mean: convert the covered bins to power, average, and convert back to dB. This is energy-correct for broadband content, but a lone tone still reads about 10·log10(N) dB low (≈7-10 dB), so tonal peaks stay under-read.
- Keep the family's dB mean and state in USER_MANUAL §3.4 that above a few kHz the trace averages bins and under-reads tones. This costs least but leaves the display misleading.
- Use a larger FFT or constant-Q/log-frequency binning so each column holds about one bin. This costs more and interacts with [VIS-023](findings-visualisation.md#vis-023)'s high-rate resolution work.

**Decision: Proceed · P2.** The mechanism is code-confirmed and backed by a runtime capture. The fix is a one-function, paint-side change with no ADR, parameter, state or DSP impact. The comment's family-consistency argument does not justify a display whose tonal response varies by tens of dB with frequency, and ADR-0009 lets an adapted copy diverge with a provenance note. It is P2 rather than P1 because SPEC is not the default well view and dense programme dilutes less than the synthetic tone.

**Architecture gates.**

- No hard-stop category. Family consistency: this deliberately diverges from Anamorph's SpectrumImager column rule, so record it in the ADR-0009 provenance header and as a brand-checklist deviation candidate (see [UI-018](findings-ui.md#ui-018)).

**Dependencies.** [VIS-021](findings-visualisation.md#vis-021) (calibration in the same function family; land together and share tests); [VIS-023](findings-visualisation.md#vis-023) (sample rate and FFT resolution move the regime boundary); [UI-018](findings-ui.md#ui-018) (family-deviation list for differences from Anamorph); [VIS-006](findings-visualisation.md#vis-006) (the well has no dB scale, so users cannot cross-check heights)

**Acceptance criteria.**

- At 48 kHz, bin-centred sines of equal level at 750 Hz, 5.86 kHz and 12 kHz draw peak heights within 1 dB of each other, in both the Simple and Advanced wells at every UI scale.
- A test through SpectrumView's column reader (or an extracted pure function) feeds a frame with one bin at 0 dB and the rest at the floor. It asserts that the painted value of the column covering that bin is ≥ -1 dB in the averaging regime. The test fails on the current mean.
- On pink noise the trace shows no step greater than 1 dB at the 1.5-bin regime boundary.
- ADR-0039's frame layout and 2048-bin publication are unchanged, and SPEC tick/paint cost stays within the budget [TECH-004](findings-dsp-tech.md#tech-004) records.
- The SpectrumView.cpp column-rule comment and provenance line state the divergence from Anamorph's dB-mean reducer.

<details><summary>Verification record</summary>

Re-read e769f33:src/gui/SpectrumView.cpp:826-935 and :195-213.
- dbForColumn has two regimes. Below 1.5 bins per column it uses Catmull-Rom interpolation of bin dB values. At or above 1.5 bins it takes the arithmetic mean of the bins' dB values over the inclusive floor/ceil span (:886-897), then clamps with jlimit (:912).
- Bins are gainToDecibels(mag·norm, -120) with an attack-instant EMA, so the floor is -120 dB.
- Anamorph fd78c3b:src/gui/SpectrumImager.cpp:676-690 uses the same reducer with a -90 floor (read-only check).

Viewed the verify-23 captures.
- In 01-after-click-spec.png the topmost trace pixel of the bin-centred 5859.375 Hz tone (bin 500) is at (804, 712).
- In sine750_-12.png the -12 dBFS 750 Hz tone (bin 64, cubic regime) peaks at (534, 661).
- The floor row is y=749.5 (-90 dB).
- The 750 Hz series at 0/-6/-12/-18 dBFS peaks at y=657/657/662/668, about 1 px/dB.

The 5.86 kHz tone's level (-12 dBFS) is the verifier's figure. app.log records only 'signal sine 5859.375', not the launch level. The roughly 50 dB gap is therefore verifier-measured. The mechanism is code-confirmed and does not depend on that level. The magnitude on real programme was not measured; dense material dilutes less than a lone tone.

</details>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

