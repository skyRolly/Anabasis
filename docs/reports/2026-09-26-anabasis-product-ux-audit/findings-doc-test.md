# Anabasis product / UX audit — findings: Documentation and product contract and Test infrastructure

Part of [`2026-09-26-anabasis-product-ux-audit.md`](../2026-09-26-anabasis-product-ux-audit.md) (audited revision `e769f33`, 2026-09-26). This file holds the complete record of each finding in these categories; the report carries the index, the systemic themes, the roadmap and the decision record. Code anchors are pinned to `e769f33`; runtime observation ids refer to [`worklogs/2026-09-26-product-ux-audit.md`](../../../worklogs/2026-09-26-product-ux-audit.md).

Each record: decision, priority and confidence after calibration; evidence; current behaviour; problem; root cause; user impact and scope; proposed improvement; alternatives considered; decision rationale (with any calibration or challenge outcome); architecture gates; dependencies; acceptance criteria; and the verification record. Terms in the records: the *candidate claim* is the claim as it entered verification; *the judge* is the verifier's decision pass (Phase 3, step 3), done per *batch* of 3–6 related findings; *Adversarial challenge* is the step-4 review and *Calibration* the Phase-4 pass that set the final decision and priority (see the report's *Evidence and method*). A paragraph marked *Merged at triage from another verifier's note* is evidence from another batch's verifier, kept in its words: 'add to X' there means it has been added to this record. 'Recorded at triage' marks a finding written from such a note. `rt/…` paths and ids such as `VER0-2` or `V24-TSAN-1` name uncommitted session captures, logs and probes; `PF-…` ids are potential findings from the uncommitted Phase-1 evidence maps.

## DOC — Documentation and product contract

### DOC-001

**The user-facing entry documents contradict the product: the manual says the output is always stereo although mono-to-mono ships, README calls the main knob 'Push', and the manual says there are 'no installers yet' although INSTALLATION.md documents installers**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P3** | high | confirmed | Documentation | User and design documents contradict the shipped product | Phase 5 |

**Evidence**

- e769f33:docs/user/USER_MANUAL.md:92-93 — '(A mono source also works: it is duplicated to both channels; the *output* is always stereo.)'
- e769f33:docs/user/USER_MANUAL.md:36-38 and :470-472 — the same manual documents mono→mono since 0.1.2
- e769f33:src/PluginProcessor.cpp:744-752 — isBusesLayoutSupported accepts mono out with mono in
- e769f33:README.md:3 ('one large "Push" knob') and :56 ('Loudness/Push knob'), against e769f33:src/PluginParameters.cpp:281 (parameter name 'Loudness'). Runtime screenshot session capture `rt/layout/01c-bigknob.png` shows the caption 'Loudness'
- e769f33:docs/user/USER_MANUAL.md:63-64 ('plain per-platform ZIPs (no installers yet)'), against e769f33:docs/user/INSTALLATION.md:6-10 ('A Windows installer and a macOS .pkg are built too, as separate downloads') and :17 (Linux ships install.sh)
- e769f33:docs/user/INSTALLATION.md:6 and e769f33:docs/user/USER_MANUAL.md:63 say 'v0.1.x', while e769f33:CMakeLists.txt:20 says VERSION 0.2.12
- e769f33:docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md:16-37 — 'Packaging / signing' routes only to RELEASE_PROCESS/INSTALLATION.md, and no row routes an I/O-layout change or a user-visible label change to USER_MANUAL.md or README.md
- *Merged at triage from another verifier's note, quoted as written:* Add the core-law document to [DOC-001](findings-doc-test.md#doc-001). COMPATIBILITY_MATRIX's I/O-layouts table (e769f33:docs/architecture/COMPATIBILITY_MATRIX.md:58-64, re-checked) has no mono→mono row and marks 'anything → mono / other' Not Supported ('the output main must be stereo'). That contradicts isBusesLayoutSupported (e769f33:src/PluginProcessor.cpp:744-752, re-checked), CHANGELOG 0.1.2 ADR-0023 item 5 (CHANGELOG.md:1497-1502) and USER_MANUAL.md:36-38. [DOC-001](findings-doc-test.md#doc-001)'s fix list should add a mono→mono row (Verified (headless), citing its test) and narrow Not Supported to stereo→mono and more than 2 channels. As a COMPATIBILITY-family document it outranks the manual fix and should be corrected first.

**Current behaviour.** The manual contradicts itself on channel layouts: §2.3 says the output is always stereo, while §1 and §9 say mono→mono is offered. The README names the main control 'Push', which the UI never shows. The manual's install section says installers do not exist and gives a stale version line; the installation guide it links to lists a Windows installer and a macOS .pkg.

**Problem.** The first documents a tester or new user opens give contradictory basics: what comes out of the plug-in on a mono track, what the main control is called, and whether an installer exists.

**Root cause.** The edits were made where they were triggered and missed sibling sections. The 0.1.2 layout change updated manual §1 and §9 but not §2.3. The 0.1.4 packaging round updated INSTALLATION.md, as the 'Packaging / signing' trigger row requires (DOCUMENTATION_LIFECYCLE_POLICY.md:16-37), but no row sends packaging or I/O-layout changes to USER_MANUAL.md, and README is triggered only by 'Ship a version (status/version)'. The README carried the brief's draft name (DEVELOPMENT_BRIEF.md:118) and was never reconciled with the shipped caption.

**User impact.** The impact is low cost but lands at first contact. A user with a mono track or a dual-mono rack reads that the output is always stereo, while the host may negotiate mono→mono. A tester told 'no installers' may not look for the Windows installer or .pkg. A reader of the README looks for a 'Push' knob that is labelled 'Loudness'. Each confusion is quick to recover from, but together they undermine trust in the manual, which is the user-facing contract. *Scope:* Documentation only: docs/user/USER_MANUAL.md §2.1 and §2.3, README.md :3 and :56, and the version string in INSTALLATION.md:6. No code, UI or state is involved.

**Proposed improvement.**

1. Rewrite manual §2.3 to match §1 and §9. For example: 'On a mono track the host may insert it mono→mono (the output stays mono, for dual-mono racks) or mono→stereo (the source is duplicated to both channels); stereo→mono is not offered.'
2. Rewrite manual §2.1 to say the build ships as per-platform ZIPs, plus an unsigned Windows installer and macOS .pkg as separate downloads (Linux: install.sh inside the ZIP), and link to INSTALLATION.md for the steps.
3. Replace 'v0.1.x' in both user documents with version-agnostic wording, or with the current line taken from CMakeLists.
4. Change README :3 and :56 to 'Loudness', matching the editor caption and the parameter name.
5. Optional, and separately: add trigger-map rows so these facts are re-synced next time — 'Packaging' → USER_MANUAL §2.1, 'I/O layout' → USER_MANUAL + COMPATIBILITY_MATRIX, 'user-visible control name' → README/USER_MANUAL.

**Alternatives considered.**

- *Leave as-is* — Rejected. The contradictions are inside the documents the user reads first, and one of them sits inside a single file.
- *Fix only the three sentences* — Acceptable as the minimum, and it is the core of the proposal. Without a trigger-map row, the next packaging or layout change will drift the same way.
- *Single-source the layout and packaging facts: the manual links to INSTALLATION.md and the compatibility matrix instead of restating them* — Good long-term shape. It reduces restatement drift, at the cost of a less self-contained quick start.

**Decision: Proceed · P3.** Each claim is confirmed against the code, the other documents and a runtime screenshot. The fix is text-only, carries no risk, and removes contradictions from the first-contact documents.

*Calibration:* the verifier judged Proceed / P2; the final judgement is Proceed / P3. Lowered P2->P3 for uniformity with [DOC-002](findings-doc-test.md#doc-002)..011: text-only drift with no wrong product outcome, equal in severity x frequency to [UX-014](findings-ux.md#ux-014), [VIS-022](findings-visualisation.md#vis-022) and [UI-009](findings-ui.md#ui-009) (P3). Merge note: include the core-law COMPATIBILITY_MATRIX I/O table (e769f33:docs/architecture/COMPATIBILITY_MATRIX.md:58-64 marks 'anything -> mono' Not Supported although isBusesLayoutSupported accepts mono->mono, e769f33:src/PluginProcessor.cpp:744-752) and correct it first, as the COMPATIBILITY family outranks the manual: add a mono->mono 'Verified (headless)' row and narrow Not Supported to stereo->mono and >2 channels, so nobody 'fixes' the code to the stale law (a bus-layout change).

**Architecture gates.**

- No hard-stop category is touched by the text fixes
- Amending the DOCUMENTATION_LIFECYCLE_POLICY trigger map is a Policy change; ADR_POLICY.md rule 5 says a Policy change is enacted by an ADR (this is not a hard stop)

**Dependencies.** [DOC-005](findings-doc-test.md#doc-005); the COMPATIBILITY_MATRIX.md:58-64 mono-output row (merged into this record; see Evidence)

**Acceptance criteria.**

- USER_MANUAL.md §2.3 no longer says the output is always stereo, and describes stereo→stereo, mono→stereo and mono→mono the same way §1, §9 and isBusesLayoutSupported do
- USER_MANUAL.md §2.1 no longer says 'no installers yet', and names the Windows installer and macOS .pkg as INSTALLATION.md:6-10 does
- README.md contains no 'Push knob' wording; the main control is called 'Loudness', matching the editor caption
- No user document states a version line that contradicts CMakeLists.txt's project VERSION
- grep over docs/user and README.md for 'always stereo', 'no installers' and '"Push"' returns nothing

<details><summary>Verification record</summary>

**Method.** I read every anchor at e769f33: USER_MANUAL.md:30-38, :63-64, :92-93 and :470-472; README.md:3 and :56; INSTALLATION.md:6-10 and :17; PluginProcessor.cpp:744-752; PluginParameters.cpp:281. I viewed the runtime screenshot session capture `rt/layout/01c-bigknob.png`, where the knob caption reads 'Loudness'. I cross-checked CHANGELOG.md:1492-1502 (0.1.2 adds mono→mono, ADR-0023 item 5) and CHANGELOG.md:1115-1125 (the 0.1.4 installer round). I also looked for where 'Push' comes from and grepped the docs for the version string.

**Corrections to the candidate claim.** The claim holds, and its scope is wider than stated. (1) README uses 'Push' twice: ':3' says 'Push' and ':56' says 'Loudness/Push'. The word comes from the brief's draft naming 'Loudness / Push' (DEVELOPMENT_BRIEF.md:118), not from the shipped name. (2) Both user documents also give a stale version: INSTALLATION.md:6 and USER_MANUAL.md:63 say 'v0.1.x', while CMakeLists.txt:20 says 0.2.12. (3) The same mono-output contradiction also sits in an architecture document in the COMPATIBILITY family, COMPATIBILITY_MATRIX.md:64. At triage that note was merged into this record; see the merged note under Evidence. (4) I found no row in the lifecycle trigger map that would have caught these edits (see root_cause). I make no git-history claim, because the history at e769f33 contains a root-level migration commit.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 2 · severity 2 · discoverability 2 · efficiency 1 · coherence 3 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DOC-002

**A meter reset also blanks M, S and RMS (indefinitely while no audio flows), contradicting the manual's 'the rolling windows (M, S, RMS) are not reset'**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P3** | high | confirmed | Feedback/observability | The session figures have no visible scope, liveness or tap, and a stray click wipes them | Phase 1 |

**Evidence**

- e769f33:src/PluginProcessor.h:627-650: requestMeterReset runs publishSilentMeters(), then meterResetPending.store(true, release)
- e769f33:src/PluginProcessor.cpp:865-884: publishSilentMeters stores sentinels into M, S, I, TP, PLR, GR, SP, RMS, ungated I and LRA, and calls engine.clearPublishedStageGr()
- e769f33:src/dsp/AnabasisEngine.h:420-428: clearPublishedStageGr zeroes the comp and limiter per-channel GR lanes
- e769f33:src/PluginProcessor.cpp:926-931: the block-top consume publishes the same full list
- e769f33:src/gui/PluginEditor.cpp:2037-2044: the Simple out-LUFS readout reads meterLufsS, so it prints '-' after a reset with no audio
- e769f33:src/gui/PluginEditor.cpp:2094-2097: the Advanced GR mini-meters read the cleared lanes
- e769f33:src/gui/LoudnessMeterView.cpp:33-35 (RMS adopts the sentinel immediately) and :156-158 ('-' at or below -99)
- e769f33:tests/state_tests.cpp:1343-1357: the test pins GR lanes blanked by a no-audio reset
- e769f33:tests/state_tests.cpp:3768-3769: 'the rolling windows kept measuring (not blanked)', with audio running only
- e769f33:docs/architecture/THREAD_MODEL.md:303-304: 'the rolling M/S windows keep running'
- e769f33:docs/user/USER_MANUAL.md:271-274: 'The rolling windows (M, S, RMS) are not reset'
- Runtime E07: session capture `rt/edges/49ef-strip.png` (every row '-' after a click under host bypass)
- Runtime verify-1 R2: session capture `rt/verify-1/10-hostbypass-before-click-ed.png`, session capture `rt/verify-1/11-hostbypass-after-click-ed.png`, session capture `rt/verify-1/12-hostbypass-after-click-4s-ed.png` (all rows and out LUFS '-', unchanged at 4 s)
- Runtime verify-1 R3: session capture `rt/verify-1/22-adv-limGR-and-stats-before-after.png` (the LIMITER GR mini-meter is cleared by the Statistics click)

**Current behaviour.** A reset clears the published display atomics for every meter:
- M, S and RMS as well as the session holds, so the panel becomes all dashes;
- the Simple out-LUFS readout;
- the Advanced per-stage GR mini-meters.
With audio running, the rolling values and GR lanes return one block later. With no processBlock running (stopped or host-bypassed hosts), everything stays blank until audio resumes. The manual says M, S and RMS are not reset.

**Problem.** The visible effect of a user reset contradicts the manual, THREAD_MODEL's statement and the intent of the audio-running test. In the ordinary stopped condition it reaches meters in other panels, so the whole metering surface looks like the no-device or broken state.

**Root cause.** publishSilentMeters is one shared 'blank every published meter' list. It is right for prepareToPlay and a state load, where a new session starts and old readings are stale. It is broader than the user reset's contract, which is session-cumulative state only (THREAD_MODEL.md:299-304; ADR-0020 Consequences). The 0.1.2 review moved the per-stage GR lanes into that list and pinned the result in a test, so the user reset inherited it.

**User impact.** A user who resets while stopped, which THREAD_MODEL calls the ordinary case (:310-312), sees the whole panel plus the out-LUFS readout and GR meters go blank. That reads as a broken meter and contradicts the manual. It resolves on playback, so it costs no data, but it undermines trust in the reset. This matters more once [UX-002](findings-ux.md#ux-002) makes the reset a deliberate, discoverable action. *Scope:* Only when no audio blocks are running: a stopped host that stops processing, host bypass, or a paused Standalone. Both views; out LUFS in Simple; GR mini-meters in Advanced.

**Proposed improvement.** Narrow what the user reset publishes, and keep the full clear for new-session events:

(1) Add publishSilentSessionHolds(), which covers I, ungated I, TP hold, SP hold, PLR and LRA. requestMeterReset() and the block-top consume use it. The rolling M/S/RMS, pubGrDb and the per-stage GR lanes keep their last values and keep being refreshed by audio as today.

(2) prepareToPlay and setStateInformation keep the full publishSilentMeters(). setStateInformation calls it alongside requestMeterReset, and the pairing stays inside a helper so no caller can forget it. Keep the publish-then-flag ordering argued at PluginProcessor.h:629-647.

(3) Re-pin tests. Change state_tests.cpp:1343-1357 to assert that a user reset leaves the GR lanes untouched while a state load clears them. Add an assertion that, with no audio, M, S and RMS keep their values after requestMeterReset while I, TP, SP and LRA go to their sentinels.

(4) Sync THREAD_MODEL's meter-reset row and keep the manual sentence, which becomes true. If the owner prefers the current full blank, correct the manual instead: 'with playback stopped, a reset blanks every meter until audio resumes'.

**Alternatives considered.**

- *Documentation-only correction (manual describes the full blank, including out LUFS and GR meters)* — This is the smallest correction under SOURCE_OF_TRUTH, since the code and test outrank the manual. But it enshrines a confusing behaviour in which a click on STATISTICS clears the LIMITER panel's GR meter. Acceptable fallback only.
- *Leave as is* — The contradiction and the 'broken meter' look remain. Rejected.
- *Narrow only the M/S/RMS stores but keep blanking the GR lanes* — Would leave a Statistics click still clearing meters in other panels. It fixes the manual sentence but not the cross-panel reach.

**Decision: Proceed · P3.** Confirmed and extended at runtime. The target behaviour, where a reset clears exactly the session-cumulative readings, is what the manual, THREAD_MODEL and ADR-0020 describe. It keeps the 0.1.2 review's real goal, the two GR lanes behaving identically, since both stay untouched. The change adds no atomics and no new cross-thread path or ordering: it is the same relaxed stores and the same release/acquire flag, with a shorter list. It does reverse a test-pinned review assertion (state_tests.cpp:1343-1357), so it should be acknowledged at the fine review. P3: the effect is transient, costs no data and causes no wrong reading. It should ship together with [UX-002](findings-ux.md#ux-002), which makes deliberate resets while stopped more common.

**Dependencies.** [UX-002](findings-ux.md#ux-002) (the explicit reset control; ship together); [VIS-012](findings-visualisation.md#vis-012) (idle/held styling is the right way to show stopped rolling rows, rather than blanking them)

**Acceptance criteria.**

- With no processBlock running (harness 'hostbypass 1'), triggering the reset leaves M, S and RMS, the Simple out-LUFS readout and the Advanced COMP/LIMITER GR mini-meters unchanged, while I, LRA, TP, SP and PLR show their not-measured state.
- A state load with no audio still blanks every meter, as testMeterResetClearsSessionHolds asserts today.
- With audio running, a reset produces no visible blank frame on the M/S/RMS rows.
- e769f33:tests/state_tests.cpp:1343-1357 is re-pinned: a user reset leaves both GR lanes untouched and a state load clears both. A new no-audio assertion covers M, S and RMS surviving a user reset.
- USER_MANUAL §3.4 and the THREAD_MODEL meter-reset row describe the same behaviour the code exhibits.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/PluginProcessor.h:604-650 (requestMeterReset publishes first, then raises the flag) and PluginProcessor.cpp:853-884. publishSilentMeters stores sentinels into all ten atomics and calls engine.clearPublishedStageGr(), which clears the COMP and LIMITER per-channel lanes (AnabasisEngine.h:406-428). Also read PluginProcessor.cpp:926-931 (block-top consume), LoudnessMeterView.cpp:33-35 and 156-158, PluginEditor.cpp:2037-2044 (out LUFS reads meterLufsS) and :2094-2097 (GR mini-meters), and e769f33:tests/state_tests.cpp:1343-1357 and 3762-3769. Viewed edges/49ef. Reproduced on :131 under 'hostbypass 1' (no processBlock) with stepped clicks:
- Simple: all eight rows went to '-', and so did the out-LUFS readout outside the panel. The capture was pixel-identical at +0.8 s and +4 s (rt/verify-1/11 and 12).
- Advanced: the LIMITER panel's GR mini-meter was also cleared by the Statistics click (session capture `rt/verify-1/22-adv-limGR-and-stats-before-after.png`).

**Corrections to the candidate claim.** The reset reaches further than the finding says. With no audio flowing it also blanks two things outside the STATISTICS panel: the Simple 'out LUFS' readout and the Advanced COMP/LIMITER GR mini-meters. The GR-lane blanking is deliberate and pinned by a test from the 0.1.2 review (state_tests.cpp:1343-1357, 'a meter reset with no audio blanks both GR lanes on both stages'), whose motive was consistency between the two lanes. Meanwhile state_tests.cpp:3768-3769 asserts the rolling windows are 'not blanked', but only with audio running, and THREAD_MODEL.md:303-304 says 'the rolling M/S windows keep running'. Under the repo's authority order the manual is the stale document, though the documents also disagree with each other about intent. During playback the rolling rows return within one block (V-07), so the effect is confined to the no-audio case.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 3 · severity 2 · discoverability 2 · efficiency 1 · coherence 4 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DOC-003

**User presets carry the detach mask, but the manual says loading a preset re-attaches detached knobs and omits the mask from 'what a preset contains'**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P3** | high | confirmed | Documentation | User and design documents contradict the shipped product | Phase 4 |

**Evidence**

- e769f33:src/PresetManager.cpp:26-28 — savePreset writes DETACH_MASK
- e769f33:src/PresetManager.cpp:125-128 — applyPreset returns the file's mask
- e769f33:src/PresetManager.cpp:359 — factory presets clear the mask
- e769f33:src/PluginProcessor.h:171-173 — savePresetFile passes liveDetachMask
- e769f33:src/PluginProcessor.cpp:1549-1554 — replaceDetachMask installs the mask unfiltered
- e769f33:docs/DESIGN.md:719-727 — 'Presets carry the mask', a reversal of 'preset-excluded, cleared on load'
- e769f33:docs/architecture/design-decisions/ADR-0007-state-schema-v1.md:57-61 — option G (clear on load) lost; :111 presets carry the mask
- e769f33:docs/user/USER_MANUAL.md:340-341 — '…or loading a preset is what re-attaches it'; :397-405 §7.3 omits the mask
- VER0-5: rt/verify-0/home/.config/RollyTech/Anabasis/Presets/DetTest.anabasis (DETACH_MASK limGain); rt/verify-0/s12.xml (mask empty after Loudness drag); rt/verify-0/s14.xml (mask limGain after load); session capture `rt/verify-0/14-crop.png` (corner dot on Limiter Gain after load)
- *Merged at triage from another verifier's note, quoted as written:* Add a DESIGN §5.3 drift to the preset-mask doc sync. e769f33:docs/DESIGN.md:737-744 says a factory preset whose patch cannot be reached from one macro triple ships a non-clear mask. Code: every factory apply clears the mask (PresetManager.cpp:359) and re-lands the curve (PluginProcessor.cpp:1652), and the tables deliberately leave out managed parameters (PresetManager.cpp:209-215 'express the intent through the macros'). Factory presets are therefore curve-consistent by construction. Add a forward pointer; no code change.

**Current behaviour.** A user preset records which managed parameters were detached, and loading it restores that detach state. Factory presets always load fully attached. The manual says loading any preset re-attaches detached parameters, and its 'what a preset contains' section does not mention the mask.

**Problem.** The manual contradicts the implemented and ADR-mandated behaviour. A user who reloads such a preset sees corner dots (and the Simple edited dot) that the manual says a preset load clears.

**Root cause.** The user-manual text was not synced with DESIGN §5.3 and ADR-0007's 'presets carry the mask'.

**User impact.** Minor confusion about why knobs show as detached, or why Simple's edited dot is lit, right after loading one's own preset. Values and sound are unaffected. *Scope:* USER_MANUAL.md §5 (:340-341) and §7.3 (:397-405). Only user presets saved with a non-empty mask trigger it.

**Proposed improvement.** Correct the manual, not the code. §5: 'Moving a macro or clicking Simple's edited dot re-attaches it. Loading a preset replaces the detach state with the preset's own: factory presets load fully attached; a user preset restores whichever knobs were detached when it was saved.' §7.3: add 'A user preset also records which macro-managed knobs were detached (the corner dots) and restores them.'

**Alternatives considered.**

- *Change code to clear the mask on every preset load* — Rejected. It conflicts with Accepted ADR-0007 (option G lost) and DESIGN §5.3, which is a Hard Stop, and it would reintroduce the 'off-curve but marked engaged' dishonesty those documents reject.
- *Leave as-is* — Rejected. The manual contradicts the code and an Accepted ADR, and the fix is two sentences.

**Decision: Proceed · P3.** This is clear doc drift from an Accepted ADR, confirmed by a runtime round trip. Correcting the manual is the smallest evidence-backed fix, per SOURCE_OF_TRUTH (code and ADR outrank the manual). Priority is low because the situation is uncommon and does not change the sound.

**Dependencies.** None.

**Acceptance criteria.**

- USER_MANUAL.md §5 no longer states that loading any preset re-attaches detached knobs, and distinguishes factory (all attached) from user presets (restore their saved detach state)
- USER_MANUAL.md §7.3 lists the detach state among what a user preset carries
- Runtime check matches the text: save a user preset with Limiter Gain detached, re-engage via a Loudness move, reload; the corner dot returns, and loading any factory preset clears it

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/PresetManager.cpp:26-28, :125-128 and :359, e769f33:src/PluginProcessor.h:171-173, e769f33:src/PluginProcessor.cpp:1549-1554 (replaceDetachMask installs the list unfiltered), e769f33:docs/DESIGN.md:719-727, ADR-0007 :57-61 and :111, and e769f33:docs/user/USER_MANUAL.md:336-343 and :397-405. Reproduced the round trip on :130 with stepped motion (VER0-5). Dragged Limiter Gain in Advanced, which put limGain in the live mask. Saved 'DetTest', and the file holds DETACH_MASK limGain. Dragged Loudness in Simple, which emptied the mask. Loaded DetTest: the mask was limGain again and the Advanced corner dot returned on Limiter Gain.

**Corrections to the candidate claim.** The manual sentence is correct for factory presets (the mask is cleared, :359) and for user presets saved with a clear mask. It is wrong only for user presets saved with detached parameters, which reload detached by design (ADR-0007 option G rejected; DESIGN §5.3 'Presets carry the mask'). The claimed root cause, that the manual generalises the factory behaviour, is not evidenced over the alternative, that the sentence reflects the earlier 'cleared on load' position DESIGN :719 records as reversed. Either way it is doc drift against an Accepted ADR.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 1 · severity 2 · discoverability 3 · efficiency 1 · coherence 3 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DOC-004

**The Learn contract text disagrees with itself and with the code: the tooltip says 'Play the loudest section' while the manual says 'representative section', DESIGN §5.4 and a header comment promise a duck-routed, undo-bracketed Learn, and the rule that a stop within 5 s is ignored is undocumented**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P3** | high | partially-confirmed | Documentation | User and design documents contradict the shipped product | Phase 5 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:641-642 — 'Play the loudest section and Learn measures it as the adaptive reference - click again to stop'
- e769f33:docs/user/USER_MANUAL.md:312-315 — 'play a *representative* section (at least 5 seconds — the button counts), press it again'
- e769f33:docs/user/USER_MANUAL.md:450 — podcast: 'LEARN on a representative minute'
- e769f33:docs/user/USER_MANUAL.md:100 — 'Play the loudest section of your track' (the Loudness quick start, the likely source of the tooltip wording)
- e769f33:docs/DESIGN.md:794-799 — Learn 'engage is duck-routed … the commit is gesture/undo-bracketed as one step'
- e769f33:docs/DESIGN.md:962 — 'per-slot adaptive/Learn memory' against ADR-0007:83 (ADAPTIVE global)
- e769f33:src/PluginProcessor.h:652-653 — 'the P5 UI adds the duck-routed engage + undo bracketing around these'
- e769f33:src/PluginProcessor.h:659-661 — 'The P5 Learn grammar owes an acknowledged commit … which is where this closes' (not implemented, see [UX-005](findings-ux.md#ux-005))
- e769f33:docs/policies/MODE_AND_ADAPTATION_POLICY.md:161-166 — commit deliberately OUTSIDE undo
- e769f33:docs/policies/MODE_AND_ADAPTATION_POLICY.md:267-269 — the same policy still says 'The Learn UI grammar (duck-routed engage, undo bracketing, running readout display) lands with the P5 UI'
- e769f33:docs/architecture/design-decisions/ADR-0005-macro-layer-architecture.md:305-306 — Anamorph bracketing 'reused for Learn' (Related code list of an Accepted ADR)
- e769f33:src/gui/PluginEditor.cpp:632 and e769f33:src/gui/PluginEditor.h:630 — an early stop is ignored under kLearnMinPassMs=5000; no user doc says the click is discarded
- e769f33:src/InternalState.h:110 — tooltipsOn default false
- Runtime G-18: session capture `rt/gestures/34-sheet.png` (tooltip text as rendered)

**Current behaviour.** The in-product tooltip (off by default) tells users to learn the loudest section, while the manual's §4 and podcast workflow say a representative section. The design doc, a processor header comment, the MODE policy's closing paragraph and an ADR-0005 reference all describe a duck-routed, undo-bracketed Learn with an acknowledged commit. The code does neither, and the MODE policy's own §5.4 note records that the commit is deliberately outside undo. No user-facing text says an early stop click is discarded.

**Problem.** Users get conflicting instructions on what material to calibrate on. Maintainers and agents, who are required to read the policies first, meet a policy that contradicts itself on whether Learn is undo-bracketed, a topic next to the ADR-0007 gate.

**Root cause.** The P0/P4-era promises (DESIGN §5.4 and §7, the PluginProcessor.h comment, the MODE :267-269 closing paragraph, the ADR-0005 related-code note) were not swept when the P5 decision at MODE :158-166 was taken. The tooltip was written separately and appears to reuse the Loudness quick-start wording (manual :100).

**User impact.** A user who follows the tooltip learns on the densest/loudest material, while one who follows the manual learns on typical material; the two give different reference targets for the adaptive trims. The impact is modest because the trims are bounded and tooltips are off by default. For maintainers, the self-contradicting policy can lead to implementing undo bracketing that conflicts with ADR-0007's design rationale. *Scope:* Tooltip text (PluginEditor.cpp:641-642), USER_MANUAL §4, DESIGN §5.4 and §7, the PluginProcessor.h comments, MODE_AND_ADAPTATION_POLICY :267-269, and the ADR-0005 related-code note. No code behaviour changes.

**Proposed improvement.**

1. Use one wording for the Learn material in the tooltip and the manual. 'Representative' is recommended, since it matches manual §4 and §8, the MODE policy ('an analysed passage') and the brief ('play a passage'), and the learned features (onset density, tilt ratio) do not depend on level. The owner confirms the intent, because 'loudest' could be deliberate chorus calibration.
2. Have the tooltip carry the rule, e.g. 'Play a representative section for at least 5 s, then click again to stop'.
3. Rewrite PluginProcessor.h:652-653 and :659-661 to point to MODE :158-166 and to the [UX-005](findings-ux.md#ux-005) acknowledgement once it exists.
4. Fix MODE :267-269 to match :158-166.
5. Add supersession pointers at DESIGN §5.4 :794-799 and §7 :962, to the MODE policy and ADR-0007.
6. Annotate ADR-0005 :305-306 through the ADR amendment process: a related-code note only, the decision unchanged.
7. Have manual §4 state what a click inside the minimum pass does, or the new behaviour once [UX-005](findings-ux.md#ux-005) lands.
Sync these per DOCUMENTATION_LIFECYCLE_POLICY.

**Alternatives considered.**

- *Implement the promised duck-routed engage and undo bracketing to match DESIGN* — Rejected. The duck masks nothing, since Learn start and commit are audio-continuous. Undo bracketing conflicts with ADR-0007 and the policy's recorded rationale (see [STATE-009](findings-state-model.md#state-009)).
- *Change the manual to 'loudest'* — Viable only if the owner intends chorus-calibration; that is product intent and must not be guessed.
- *Leave the drift* — Rejected. The policy is a mandatory first read for contributors and agents and currently contradicts itself.

**Decision: Proceed · P3.** Every drift item is confirmed at e769f33, the fixes are text-only, and together they remove a contradiction inside a core policy plus a contradictory user instruction. It is P3 because user exposure is limited (tooltips are off by default, Learn is optional), but the fix should land with or before any [UX-005](findings-ux.md#ux-005)/STATE-009 work so those changes are documented against one contract.

**Architecture gates.**

- None for the text changes. The ADR-0005 edit touches an Accepted ADR's related-code note only and must follow the ADR amendment process without changing its decision. Implementing the stale undo-bracketing promise instead would conflict with ADR-0007 (StateSet undo unit).

**Dependencies.** [UX-005](findings-ux.md#ux-005) (the final Learn grammar determines the documented click rule and acknowledgement); [STATE-009](findings-state-model.md#state-009) (reset path and A/B scope to document); Owner confirmation of 'representative' versus 'loudest' intent

**Acceptance criteria.**

- The Learn tooltip and USER_MANUAL §4 use the same term for the material to play, confirmed by the owner.
- No text in src/, docs/policies/ or docs/architecture/ still says the Learn commit is undo-bracketed or duck-routed without a supersession pointer (grep 'undo bracketing' and 'duck-routed engage').
- MODE_AND_ADAPTATION_POLICY.md has one consistent statement on Learn and undo.
- DESIGN §5.4 (Learn bullet) and §7 (:962) carry supersession pointers to the MODE policy and ADR-0007.
- USER_MANUAL §4 states what a click inside the minimum pass does, matching the code.
- DOCUMENTATION_COVERAGE records the sync.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/PluginEditor.cpp:641-642 (tooltip); USER_MANUAL.md:100 (Quick start 'loudest section', for setting Loudness), :312-315 (Learn 'representative section') and :450 (podcast 'representative minute'); DESIGN.md:794-799 and :962; e769f33:src/PluginProcessor.h:652-653 and :655-661; MODE_AND_ADAPTATION_POLICY.md:158-166 and :267-269; ADR-0005:305-306; HANDOVER.md:1410-1416; e769f33:src/gui/PluginEditor.h:627-630; AdaptiveEngine.h:386-413 (the Learn features are the integrated onset rate and the hi/lo tilt ratio). Viewed session capture `rt/gestures/34-sheet.png`. Grepped src/ and docs/ for 'loudest'.

**Corrections to the candidate claim.**

1. The 5 s minimum IS documented, in MODE :158-159 ('a 5 s minimum pass') and in the manual ('at least 5 seconds — the button counts'). What users are not told is that a click inside it is ignored rather than queued.
2. The manual does say 'press it again'. The in-product 'click again to stop' exists only in the tooltip, which is off by default (InternalState.h:110).
3. DESIGN is superseded section by section (CLAUDE.md), so its staleness is expected-class drift.
4. A stronger drift than the finding named: the MODE policy contradicts itself. Lines 158-166 say the Learn commit stays OUTSIDE undo, while :267-269 still says 'The Learn UI grammar (duck-routed engage, undo bracketing, running readout display) lands with the P5 UI'.
5. Further stale promises: ADR-0005:305-306 (Related code) says Anamorph's single-step commit bracketing is 'reused for Learn'; PluginProcessor.h:659-661 says the P5 grammar closes the uncommitted-stop gap with an acknowledged commit, which never happened; DESIGN §7 :962 says 'per-slot adaptive/Learn memory', against ADR-0007's global ADAPTIVE.
6. The duck promise should be deleted, not implemented. Learn start changes no audio and a commit only retargets trims that slew over about 2 s, so there is no discontinuity to mask.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 2 · severity 2 · discoverability 3 · efficiency 1 · coherence 3 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DOC-005

**Undo/Redo and Settings are documented wrongly: the build ships ↺/↻ (U+21BA/U+21BB) rotated 180°, but the manual shows ↶/↷ and a Settings 'gear', and the in-code comment still calls the glyphs U+21B6/U+21B7 and an open brand question**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P3** | high | confirmed | Documentation | User and design documents contradict the shipped product | Phase 5 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:334-335 — undo U+21BA, redo U+21BB ('the sibling's circle arrow (#7)')
- e769f33:src/gui/PluginEditor.cpp:348-356 — stale: 'These are U+21B6/U+21B7 SEMICIRCLE arrows … a Level-5 question the brand checklist still holds open'
- e769f33:src/gui/LookAndFeel.cpp:452-462 — the 'icon' branch: 21 px glyph, rotated by π about the centre
- e769f33:tests/state_tests.cpp:4391-4399 — the test pins 0x21BA/0x21BB 'since 2026-08-05', replacing 0x21B6/0x21B7
- e769f33:src/gui/PluginEditor.h:454 — juce::TextButton settingsButton { "Settings" } (text, no icon)
- e769f33:docs/user/USER_MANUAL.md:161 — '↶ / ↷'; :162 — 'Settings (gear)'; :276 — '### 3.5 Settings (gear)'
- e769f33:docs/DOCUMENTATION_COVERAGE.md:1760-1763 — owner directive 2026-08-05: undo/redo carry the sibling's circle arrows U+21BA/U+21BB
- e769f33:docs/HANDOVER.md:765-766 — round-55 entry still describing U+21B6/U+21B7; no later HANDOVER entry records the change
- e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:15-18 — candidate list has no glyph item
- Anamorph@fd78c3b:docs/user/USER_MANUAL.md:192 ('↺ / ↻'), :194 and :258 ('gear', same drift in the sibling)
- Runtime V12-6: session capture `rt/verify-12/00-undoredo-8x.png` — open-circle arrows with the gap and arrowheads at the BOTTOM (rotated ↺/↻), dimmed (disabled at start); session capture `rt/verify-12/00-initial-crop.png` — 'Settings' is a text button
- Phase-2 LAY-17: session capture `rt/layout/01c-topbar.png`

**Current behaviour.** The top bar shows two icon-only buttons with open-circle arrows (U+21BA/U+21BB), each rotated 180° so the arc opens at the bottom, and a text button labelled 'Settings'. The user manual's §3.1 table shows '↶ / ↷' (semicircle arrows) and calls the settings control a gear, as does the §3.5 heading. The arming-site comment in PluginEditor.cpp describes the glyphs as U+21B6/U+21B7 and the choice as an open brand question.

**Problem.** Users matching the manual to the UI look for a gear and for semicircle arrows that do not exist. Maintainers and the Level-5 brand pass are pointed at a glyph question that is already settled, with a rationale ('direction is not at risk') that the test's own record contradicts.

**Root cause.** The glyph change of 2026-08-05 (owner 'ugly icons' report) updated the code, the test and DOCUMENTATION_COVERAGE, but not the earlier arming-site comment, the manual row or HANDOVER. 'Settings (gear)' was carried over from the sibling's manual, which has the same drift.

**User impact.** Low. The controls are adjacent and readable, so a user corrects the mismatch at a glance. The maintainer-facing comment has more consequence, because it misdirects the pending brand review. *Scope:* USER_MANUAL.md §3.1 rows (:161, :162) and the §3.5 heading (:276); the PluginEditor.cpp:348-356 comment; a HANDOVER status note. DESIGN.md:898 wireframe optional (placeholder).

**Proposed improvement.** Sync the documentation to the code (authority order Code → Tests → docs). USER_MANUAL §3.1: show '↺ / ↻' or describe them as 'the two circular-arrow buttons (Undo / Redo)', and change the row to '**Settings**'. Retitle §3.5 'Settings'. Rewrite PluginEditor.cpp:348-356 to state the current facts: U+21BA/U+21BB (the sibling's pair) since 2026-08-05 by owner directive, and the 180° treatment retained. Remove the 'still holds open' claim and the contradicted direction argument, or point to the test comment. Add a dated HANDOVER line recording the glyph switch.

**Alternatives considered.**

- *Change the product to match the manual (a gear icon, semicircle glyphs)* — Rejected. Code is the authority. The owner directive chose the sibling's circle arrows, and the 'Settings' text button matches Anamorph.
- *Only fix the manual* — Insufficient. The stale code comment is what misdirects the brand pass.
- *Also correct the sibling's 'gear' wording* — Out of scope: Anamorph is read-only. Note it for the family doc owner.

**Decision: Proceed · P3.** Confirmed drift between code and docs, with a clear authority order. It is a small documentation sync required by DOCUMENTATION_LIFECYCLE_POLICY, with no behavioural change and no gate.

**Dependencies.** [UI-018](findings-ui.md#ui-018) (brand-pass candidate list; the glyph question should not appear there as open)

**Acceptance criteria.**

- USER_MANUAL.md §3.1 shows the glyphs actually rendered (↺ / ↻, or an equivalent description) and labels the settings control 'Settings', and the §3.5 heading contains no 'gear'.
- The comment block at PluginEditor.cpp above undoButton.setComponentID("icon") names U+21BA/U+21BB, cites the 2026-08-05 owner directive, and no longer claims an open checklist question.
- A grep for '21B6\|21B7' in src/ and docs/user/ returns only historical, dated records.
- HANDOVER.md carries a dated entry recording the switch to U+21BA/U+21BB.
- No code or test behaviour changes (the state tests pass unchanged).

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/PluginEditor.cpp:334-335 (0x21BA/0x21BB, 'the sibling's circle arrow') against the comment at :348-356; e769f33:src/gui/LookAndFeel.cpp:448-463 (the 180° icon treatment); e769f33:tests/state_tests.cpp:4391-4399 (the test pins 0x21BA/0x21BB and records the 2026-08-05 change); e769f33:src/gui/PluginEditor.h:454 (settingsButton {"Settings"}); e769f33:docs/user/USER_MANUAL.md:161, 162, 276; e769f33:docs/DOCUMENTATION_COVERAGE.md:1760-1763, 2349-2356; e769f33:docs/HANDOVER.md:757-767, 1352; e769f33:docs/DESIGN.md:892-898; e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:15-18. Compared Anamorph@fd78c3b:docs/user/USER_MANUAL.md:192, 194, 258. Runtime V12-6: 8× zoom of the undo/redo buttons.

**Corrections to the candidate claim.** (1) HANDOVER.md:765-766 and :1352 are dated history entries (round 55, P6) that were accurate when written. The real HANDOVER gap is that no later entry records the 2026-08-05 switch to U+21BA/U+21BB, so HANDOVER's only statement on the glyph pair is outdated. (2) DESIGN.md:898 is a wireframe whose strings are declared placeholders (:892-894) and DESIGN is superseded section by section, so it carries little weight. (3) The stale code comment is also internally contradictory. It says 'Direction is not at risk … neither glyph can come to read as the other', while the test comment (state_tests.cpp:4392-4394) says the half-arrows 'inverted their meaning when rotated'. It also claims the brand checklist 'still holds open' the glyph question, but the checklist lists no such item (:15-18 name only font and accent), and the owner directive settled the pair (DOCUMENTATION_COVERAGE.md:1760-1763). (4) The 'Settings (gear)' wording was inherited from the sibling's manual, which has the same drift (Anamorph USER_MANUAL.md:194, 258, while Anamorph's button is also the text 'Settings'). Anamorph's manual correctly shows ↺ / ↻.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 2 · severity 1 · discoverability 2 · efficiency 1 · coherence 3 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DOC-006

**USER_MANUAL says re-engaged parameters 'glide back … smooth, not a jump', but the mapper lands each target in one write, de-clicked only by the stages' 20 ms smoothers**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | partially-confirmed | Documentation | The macro layer overwrites hand edits and automation with almost no notice | Phase 4 |

**Evidence**

- e769f33:src/MacroEngine.cpp:234-242 — one setParam per target per pass; e769f33:src/MacroEngine.cpp:246-257 — single setValueNotifyingHost, no ramp
- e769f33:src/dsp/ClipSat.h:89, e769f33:src/dsp/MasteringComp.h:66, e769f33:src/dsp/MasteringEQ.h:48, e769f33:src/dsp/AnabasisEngine.cpp:114-117 — 20 ms SmoothedValue resets
- e769f33:src/gui/PluginEditor.cpp:2932-2944 — vpos eases the knob pointer on jumps (display only)
- e769f33:docs/architecture/design-decisions/ADR-0005-macro-layer-architecture.md:97-100 — 'rate-limited to control rate; the engine's 20 ms parameter smoothing makes the glide click-free'
- e769f33:docs/user/USER_MANUAL.md:330-332 — 're-engage and glide back under macro control … it is smooth, not a jump'
- e769f33:docs/policies/MODE_AND_ADAPTATION_POLICY.md:82-83 — 'through the normal rate-limited glide' (consistent with ADR-0005)
- Runtime verify-14 dumps: Threshold -14.3→-10.0 dB and Limiter Gain 5.9→7.8 dB fully landed at the first dump after a Tone wheel notch — session capture `rt/verify-14/04-05-wheel-sheet.png`

**Current behaviour.** On re-engage, each off-curve managed parameter is written to its curve value in a single write. The DSP de-clicks it with a 20 ms ramp, and the Advanced knob pointers ease visually.

**Problem.** 'Glide back … smooth, not a jump' reads as an audible, gradual transition. What actually happens is a near-immediate, de-clicked step, for example +1.9 dB of limiter push within ~20 ms. The manual sets an expectation the mechanism does not meet.

**Root cause.** The manual paraphrases the ADR/policy term 'glide', which there means 'click-free via the 20 ms smoother', in everyday language that implies a perceptible fade.

**User impact.** This is minor on its own. A user expecting a gentle return hears an abrupt change, which reduces trust in the manual. It compounds [MODEL-001](findings-state-model.md#model-001), because an audible jump nobody warned about looks like a fault. *Scope:* One paragraph of USER_MANUAL §5. Policy and ADR text are unaffected.

**Proposed improvement.** Reword USER_MANUAL §5 to state the mechanism. Detached parameters re-engage immediately at the macro's current curve values. The change is de-clicked (about 20 ms) but is not a slow fade, and Undo takes it back. Fold this into the [MODEL-001](findings-state-model.md#model-001) rewrite of the same paragraph.

**Alternatives considered.**

- *Implement a real macro-level glide (e.g. 200-500 ms ramp on re-engage)* — Rejected. It adds time-varying hidden state to a mapping ADR-0005 requires to be a pure function, changes decision 2's control-rate contract (a macro-layer gate), and there is no evidence users want it.
- *Also reword MODE policy invariant 3* — Not needed: its term is defined by ADR-0005 decision 2 and is accurate.
- *Leave as-is* — Rejected: a small, cheap correction of a user-facing overstatement.

**Decision: Modify · P3.** Only the manual overstates, and the policy and ADR are accurate. The fix is a wording correction to the manual, not a change to the mechanism.

**Dependencies.** [MODEL-001](findings-state-model.md#model-001) (same USER_MANUAL §5 paragraph)

**Acceptance criteria.**

- USER_MANUAL §5 no longer says 'glide back' or 'smooth, not a jump'. It states that re-engage is immediate and de-clicked (~20 ms) and that Undo reverts it.
- MODE_AND_ADAPTATION_POLICY invariant 3 and ADR-0005 are unchanged.

<details><summary>Verification record</summary>

**Method.** Code read at e769f33. The mapper writes each target once with no ramp (e769f33:src/MacroEngine.cpp:234-242,246-257). The smoothers covering all nine managed targets are 20 ms: ClipSat.h:89 (drive, shape, depth, tone, tame), MasteringComp.h:66 (threshold, ratio), MasteringEQ.h:48 (tilt) and AnabasisEngine.cpp:114-117 (push gain). The pointer easing in PluginEditor.cpp:2932-2944 is on-screen only. ADR-0005 decision 2 (:97-100) and MODE_AND_ADAPTATION_POLICY.md:82-83 were read. Runtime: the verify-14 and G-14 dumps taken ≤1 s after a re-engage show the targets already reached. Glide duration and audibility were not measured.

**Corrections to the candidate claim.** (1) The MODE policy is NOT in drift. 'Through the normal rate-limited glide' is defined by ADR-0005 decision 2 as exactly this mechanism: control-rate writes plus 'the engine's 20 ms parameter smoothing makes the glide click-free'. Only the manual's user-facing 'glide back … smooth, not a jump' (USER_MANUAL.md:330-332) overstates. (2) '~20 ms smoothed step on nine parameters' holds at the parameter level. For compThreshold and compRatio the audible transition is additionally shaped by the compressor's own attack and release (defaults 30 ms / 200 ms), so the perceived change is not uniformly a 20 ms step. This is not measured.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 2 · severity 1 · discoverability 2 · efficiency 1 · coherence 2 · change risk 1 · complexity 1 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DOC-007

**The product brief (DEVELOPMENT_BRIEF Part I) still states requirements that owner decisions later replaced (80-200 % scaling, streaming target lines with a penalty estimate, a TP meter toggle, SC HPF on both detectors, variable fonts), and nothing links these clauses to their deviation records**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | partially-confirmed | Documentation | User and design documents contradict the shipped product | Phase 5 |

**Evidence**

- e769f33:docs/DEVELOPMENT_BRIEF.md:153 — streaming target lines plus a loudness-penalty estimate
- e769f33:docs/DEVELOPMENT_BRIEF.md:160 — 'UI scaling (80–200%); metering options (target-line selection, true-peak toggle)'
- e769f33:docs/DEVELOPMENT_BRIEF.md:169 — 'resizable, HiDPI-aware, with window size persistence … variable-font support'
- e769f33:docs/DEVELOPMENT_BRIEF.md:76 — SC HPF 'on the compressor and limiter detectors'
- e769f33:docs/DEVELOPMENT_BRIEF.md:630 — §23 Metering row still lists 'streaming targets', while :625-626 in the same table were annotated for ADR-0030 and ADR-0028
- e769f33:docs/SOURCE_OF_TRUTH.md:35-42 — a deviation from the brief needs an ADR and owner sign-off, and the brief is not silently corrected; Part II summaries are corrected when they conflict
- e769f33:src/InternalState.h:60-67 — steps 75/85/100/125/150, named XS..XL (owner directive 2026-08-05). LAY-09 confirms this at runtime
- e769f33:src/gui/LoudnessMeterView.h:33-39 — streaming targets removed (owner directive 2026-08-05)
- e769f33:src/dsp/LookaheadLimiter.h:149-155 — the limiter detector is unfiltered since ADR-0023
- e769f33:docs/FUTURE_RISKS.md:163-169 — RISK-009: no embedded typeface, platform default sans
- e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:15-18 and :24-25 — the typography and accent provisional deviations are recorded; :112 reads 'Deviations approved by ADR: (none yet)'
- grep setResizable|setResizeLimits over e769f33:src → 0 hits
- *Merged at triage from another verifier's note, quoted as written:* Add e769f33:docs/HANDOVER.md:1326 (P5 phase summary: 'whole-window UI scale (80-200 % composed with host DPI, persisted)') as another stale 80-200 % statement against the 75-150 % ladder (InternalState.h:66, ADR-0017). HANDOVER is the status of record, so annotate it with the ADR-0017 pointer in the same sync.

**Current behaviour.** Part I of the brief, which CLAUDE.md names 'the product specification', still asks for 80-200 % scaling, streaming target lines and a penalty estimate, a TP meter toggle, SC HPF on the limiter detector and variable fonts. The code implements owner-directed replacements, each recorded in an ADR (except the font fallback), but nothing in or near the brief points from a clause to its record. The §23 summary's Metering row is stale.

**Problem.** Anyone reviewing the product against its specification must rediscover, clause by clause, which differences are deliberate and where they were decided. The one place in the brief that lists deltas is itself stale for metering.

**Root cause.** SOURCE_OF_TRUTH.md:35-37 correctly forbids silently correcting the owner-supplied brief, and deviations were recorded in ADRs, the CHANGELOG and code comments. However, no forward-pointer or register convention was defined for Part I, as the ADR_INDEX 'amended by' registry does for ADRs and the per-section banners do for DESIGN. The §23 table was maintained only for build and toolchain deltas.

**User impact.** End users are not affected. The reviewers doing the pending post-v0.1.0 fine review, and agents told the brief is the spec, can flag accepted owner decisions as defects, or waste effort tracing them to ADRs. Recovery is cheap once the ADR is found. *Scope:* docs/DEVELOPMENT_BRIEF.md: the Part I §3, §6, §7 and §8 clauses, plus the §23 Metering row. It also affects how the fine review is navigated.

**Proposed improvement.**

1. Do NOT rewrite the Part I clauses.
2. Add one dated 'Deviations from Part I' register, owner-approved because the brief is owner-supplied. Put it as an annex at the end of the brief, or in HANDOVER with a one-line pointer from the brief's header. Each row maps a brief clause to the decision and its record:
   - §6 target lines and penalty → ADR-0015
   - §7 80–200 % → ADR-0017 (XS..XL, the sibling's ladder)
   - §7 TP toggle → ADR-0020
   - §3 SC HPF on the limiter → ADR-0023 (compressor only)
   - §8 variable font → RISK-009 / DESIGN §6.1, provisional
   - §8 resize → DESIGN §6.1 (discrete steps that persist)
3. Correct the §23 Metering row, which is a Part II summary and so correctable per SOURCE_OF_TRUTH.md:40-42.
4. Flag to the owner that the variable-font fallback is the one listed deviation without an ADR, which SOURCE_OF_TRUTH.md:35-36 asks for.

**Alternatives considered.**

- *Edit the Part I text to match the code* — Rejected. SOURCE_OF_TRUTH.md:35-37: the brief is owner input and is not silently corrected, and rewriting it would erase what was asked.
- *Leave as-is and rely on ADR_INDEX* — Weak. ADR_INDEX is organised by decision, not by brief clause, so a reviewer starting from the spec has no entry point.
- *Record them as brand deviations in BRAND_CONSISTENCY_CHECKLIST* — Rejected. The checklist measures Anamorph consistency, and most of these are not brand deviations; the XS..XL ladder actually restores sibling consistency.

**Decision: Modify · P3.** The drift is real, but the obvious fix, updating the brief, is forbidden by SOURCE_OF_TRUTH. A clause-to-record register plus a correction to the §23 summary gives the fine review its list of deliberate deviations without rewriting owner input. The brand-checklist part of the claim does not hold.

**Architecture gates.**

- No hard-stop category. The brief is owner-supplied, so adding a register to it needs owner approval per SOURCE_OF_TRUTH.md:35-37
- The variable-font fallback has no ADR, which SOURCE_OF_TRUTH.md:35-36 requires for a deviation from the brief; this is for the owner to decide

**Dependencies.** [DOC-008](findings-doc-test.md#doc-008); [DOC-009](findings-doc-test.md#doc-009)

**Acceptance criteria.**

- A dated register exists that maps each changed Part I clause (target lines/penalty, the 80-200 % scaling, the TP toggle, SC HPF on the limiter, variable fonts, free resize) to its deciding record (ADR, DESIGN section or RISK)
- The Part I clause text is unchanged
- The §23 Metering row no longer lists streaming targets as part of the product
- The brief's header, or CLAUDE.md's pointer to it, names where the register lives

<details><summary>Verification record</summary>

**Method.** I read DEVELOPMENT_BRIEF.md:43, :76, :153, :160, :169 and the §23 Deltas table (:622-634). I read SOURCE_OF_TRUTH.md:30-42 (the brief's rank and correction rule), InternalState.h:55-67, LoudnessMeterView.h:33-39, LookaheadLimiter.h:149-155, FUTURE_RISKS.md:163-169, DESIGN.md:860-870, BRAND_CONSISTENCY_CHECKLIST.md:10-30, :57-58 and :103-112, and ADR-0015, 0017, 0020 and 0023. I grepped src for setResizable and setResizeLimits (0 hits). I checked the LAY-09 runtime observation (XS..XL ladder, 705x540 to 1410x1080).

**Corrections to the candidate claim.** (1) Line numbers: the streaming targets and penalty are at :153. :160 carries the 80-200 %, target-line selection and TP-toggle Settings items.
(2) Two of the six items are not deviations in substance. 'Window size persistence' is met, because the scale step persists in int_uiScale (DESIGN.md:867-870). 'Resizable' is met by discrete scale steps, as DESIGN §6.1 decided (free host resize is off).
(3) The brand-checklist sub-claim is misframed. That checklist measures consistency with Anamorph, not with the brief. The XS..XL ladder IS the sibling's ladder (InternalState.h:60-63), so it is not a brand deviation. The typography deviation IS recorded, as a provisional deviation at BRAND_CONSISTENCY_CHECKLIST.md:15-18 and :24-25. Target lines, the TP toggle and the limiter SC HPF are deviations from the brief, not from the brand. (:112 does still read 'Deviations approved by ADR: (none yet)'.)
(4) Four of the remaining deviations have ADRs: targets and penalty → ADR-0015; 80-200 % → ADR-0017; TP toggle → ADR-0020; limiter detector HPF → ADR-0023. The variable-font fallback has DESIGN §6.1, RISK-009 and the provisional checklist acceptance, but no ADR.
What is actually missing is navigation. Part I of the brief carries no annotations at all. The §23 Deltas table (Part II) was annotated for ADR-0028 and ADR-0030, but its Metering row (:630) still lists '+ streaming targets'.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 2 · severity 2 · discoverability 3 · efficiency 2 · coherence 3 · change risk 1 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DOC-008

**The UI sections of DESIGN (§6.2-6.4) carry no supersession banners although most of their wireframes and Settings list were replaced, and the manual's Settings table orders the rows differently from the panel**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | partially-confirmed | Documentation | User and design documents contradict the shipped product | Phase 5 |

**Evidence**

- e769f33:docs/DESIGN.md:936-939 — §6.4: 'UI scaling 80…200%', 'target-line selection + true-peak meter toggle'
- e769f33:src/gui/PluginEditor.cpp:752-757 and :1466-1473 — the actual rows and their layout order; :860-877 are the removal comments for the TP toggle, the Spectrum toggle and the target checkboxes
- Runtime: session capture `rt/layout/12c-settings.png` shows the Settings order Oversampling/Phase/Offline Render/Integrated/RMS Reference/UI Scale/UI Animations/Tooltips
- e769f33:docs/user/USER_MANUAL.md:280-289 — the table orders Oversampling, Phase, Offline Render, UI Scale, UI Animations, Tooltips, Integrated, RMS Reference
- e769f33:docs/DESIGN.md:916-931 — §6.3 940x900, with a 'macro (read-only, with detach badges)' row and an 'adaptive Δ overlay'
- e769f33:src/gui/PluginEditor.h:619-638 — kAdvancedH = 822; the macro row was removed in 0.1.2 (ADR-0023 item 11); e769f33:src/gui/PluginEditor.cpp:595-600
- e769f33:docs/DESIGN.md:881-915 — §6.2 GR-only strip with Sp/Ap/YT target lines, against e769f33:src/gui/PluginEditor.cpp:1746-1761 (switchable GR/SPEC well; the comment records the supersession)
- e769f33:docs/DESIGN.md:23-28 — document-wide 'superseded section by section' banner; :214, :535 and :575-597 are per-section banners, and §6 has none
- e769f33:docs/architecture/design-decisions/ADR-0005-macro-layer-architecture.md:157-160 — the Accepted decision to show a display-only delta overlay (not implemented; see [VIS-011](findings-visualisation.md#vis-011))
- e769f33:docs/architecture/design-decisions/ADR-0009-code-reuse-from-anamorph.md:186 — 'advancedMode … in the view tier'; no forward pointer to ADR-0018. e769f33:docs/architecture/design-decisions/ADR_INDEX.md:96-106 has no 0009←0018 row

**Current behaviour.** DESIGN §6.2-6.4 still show the superseded UI with no per-section banner: the GR-only Simple strip with target lines, the 940x900 Advanced with a read-only macro row, and a Settings list with 80-200 % and the target and TP rows. The supersessions are recorded only in code comments and in other ADRs. The manual's Settings table lists Integrated and RMS Reference last, but the panel shows them between Offline Render and UI Scale.

**Problem.** A maintainer or agent working on the UI who opens §6 finds superseded and still-owed items mixed, with nothing to tell them apart. This invites both 'restoring' removed features (macro row, target lines) and dismissing the still-owed adaptive overlay as obsolete. Users matching the manual to the panel find a different order.

**Root cause.** Supersession banners in DESIGN were added section by section when a reviewer noticed. The ADRs that changed §6 (0015, 0016, 0017, 0020, 0023) banner'd §4.3's schema rows but not the UI wireframes. The lifecycle trigger map has no row that sends a GUI/Settings layout change to USER_MANUAL.md; only 'Brand/UI change touching a shared element' → BRAND_CONSISTENCY_CHECKLIST exists (DOCUMENTATION_LIFECYCLE_POLICY.md:16-37 and the documentation-only rows).

**User impact.** End users are affected only slightly: the manual's table order differs from the panel, and every row is present. Maintainers and agents risk regressions toward removed UI, or treating the ADR-0005 overlay as dropped. *Scope:* docs/DESIGN.md §6.2-§6.4 (plus the same banner gap in §2.9, see [DOC-009](findings-doc-test.md#doc-009)), USER_MANUAL.md §3.5, and ADR-0009 with its ADR_INDEX registry row.

**Proposed improvement.**

1. Add a 'Superseded in part' banner to §6.2, §6.3 and §6.4, in the §4.3 form: a table mapping each item to its record.
   - 80…200 % → ADR-0017 (XS..XL)
   - target lines, penalty and their Settings rows → ADR-0015
   - TP-meter toggle → ADR-0020 (replaced by the Integrated and RMS Reference selectors)
   - read-only macro row, and 900 → 822 height → ADR-0023 item 11
   - GR-only strip, and the Spectrum toggle in Settings → ADR-0016 plus the 2026-08-05 combined-well directive (the pill on the graph well)
   The banner must state explicitly that the 'adaptive Δ overlay' is NOT superseded and is still owed by ADR-0005 item 10 ([VIS-011](findings-visualisation.md#vis-011)).
2. Reorder USER_MANUAL §3.5's table to the panel order: engine rows, then the two metering-standard rows, then the view rows.
3. Add a forward banner to ADR-0009:186 and an ADR_INDEX registry row 0009←0018.

**Alternatives considered.**

- *Rewrite §6 to match the shipped UI* — Rejected. SOURCE_OF_TRUTH and DESIGN.md:23-28 treat DESIGN as a signed-off historical record, superseded section by section and not rewritten.
- *Rely on the document-wide banner only* — Insufficient. Code comments point readers into §6, and §6.3 mixes superseded items with a still-binding ADR-0005 item, which a blanket banner cannot distinguish.
- *Reorder the Settings panel to match the manual* — Rejected. The panel's grouping (engine, then metering standards, then view) is coherent. The manual is the copy that should follow, and moving panel rows would also touch the brand checklist's Settings-ordering item.

**Decision: Modify · P3.** The drift is confirmed, but the fix must follow the repository's own convention: per-section banners, not a rewrite. The fix must also be narrower than the claim implies, because one §6.3 element (the adaptive overlay) is a live Accepted-ADR obligation, not a superseded wireframe. The manual table reorder is a trivial user-facing correction.

**Architecture gates.**

- Conflict with Accepted ADR-0005 item 10 if a banner marked the adaptive Δ overlay as superseded without a superseding ADR. The proposal avoids this by keeping it marked 'still owed'
- Adding forward banners and registry rows records existing decisions and changes none, so no gate applies

**Dependencies.** [VIS-011](findings-visualisation.md#vis-011); [DOC-005](findings-doc-test.md#doc-005); [DOC-009](findings-doc-test.md#doc-009)

**Acceptance criteria.**

- §6.2, §6.3 and §6.4 each open with a dated 'Superseded in part' banner that maps each changed item to its ADR or directive
- The §6.3 banner states that the adaptive Δ overlay remains an ADR-0005 obligation
- USER_MANUAL §3.5's Settings table lists the rows in the same order as the rendered Settings panel
- ADR-0009 carries a forward pointer to ADR-0018 for the advancedMode undo half, and ADR_INDEX's amendment registry has a 0009←0018 row

<details><summary>Verification record</summary>

**Method.** DESIGN versus code: I read DESIGN.md:18-28 (the document-wide banner), :214, :535 and :575-597 (the existing per-section banners), and :842-940. I read PluginEditor.cpp:595-600, :752-757, :763, :860-877, :1456-1474 and :1742-1761, PluginEditor.h:619-638, and ADR-0005:84 and :155-160. I grepped src/gui for any trim or adaptive-overlay display (none).
ADR cross-links: I read ADR-0009:186 and the ADR_INDEX.md:88-106 amendment registry.
Manual order: I read USER_MANUAL.md:274-289 and viewed the runtime screenshot session capture `rt/layout/12c-settings.png`. It shows the order Oversampling, Phase, Offline Render, Integrated, RMS Reference, UI Scale, UI Animations, Tooltips.

**Corrections to the candidate claim.** (1) DESIGN.md:23-28 already states that DESIGN 'is not maintained as a living spec' and that drift is expected and resolved in favour of the ADRs. So §6's drift is sanctioned. What is missing is the per-section forward pointer that §2.5 (:214), the tier section (:535) and §4.3 (:575-597) carry.
(2) The §6.3 'adaptive Δ overlay' is NOT superseded. It is ADR-0005's Accepted decision, item 10 (ADR-0005:157-160, 'the Advanced view shows them as a display-only delta overlay'), and it is unimplemented: src/gui has no trim display, which is [VIS-011](findings-visualisation.md#vis-011). A banner must not mark it superseded.
(3) The 940x900 size was flagged as provisional in DESIGN itself (⊕, :883-890).
(4) Code still cites §6.4 as its authority for the phase-latency tooltip (PluginEditor.cpp:763), and that part is still true.
(5) ADR_INDEX has the registry row 0010←0018 but no 0009←0018 row, and ADR-0009:186 has no forward banner.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 2 · severity 1 · discoverability 2 · efficiency 1 · coherence 3 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DOC-009

**Some Accepted-ADR and code-comment text in the metering area describes behaviour the code no longer has: ADR-0016's spectrumOn default (true) and RmsMeter's '24 Hz display'. The ADR-0003, ADR-0038 and FrameClock items do not hold up as misleading drift**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | partially-confirmed | Documentation | User and design documents contradict the shipped product | Phase 5 |

**Evidence**

- e769f33:docs/architecture/design-decisions/ADR-0016-spectrumon-becomes-the-graph-well-mode.md:76 — 'Type (`bool`) and default (`true`) are unchanged'
- e769f33:docs/architecture/design-decisions/ADR-0023-012-field-fix-contracts.md:240-244 — item 7: '`int_spectrumOn` default flips to `false`'
- e769f33:src/InternalState.h:34 and :112 — default false ('0.1.2 item 4')
- e769f33:docs/architecture/design-decisions/ADR_INDEX.md:88-106 — amendment registry with no 0016←0023 row
- e769f33:src/dsp/RmsMeter.h:44-47 — 'the display refreshes at 24 Hz — ~42 ms'; e769f33:src/gui/LoudnessMeterView.h:66-67 — 'the ~24 Hz meter tick'
- e769f33:src/gui/LoudnessMeterView.cpp:62 — the view runs its own FrameClock; e769f33:src/gui/FrameClock.h:69-71 — kNominalDt 1/60, kCapHz 126; e769f33:src/gui/LoudnessMeterView.h:121 — kRmsReadoutHoldSecs = 1/3
- e769f33:docs/architecture/design-decisions/ADR-0038-gr-history-display-scalars-cross-the-painting-boundary.md:61-63 — '4096' appears only in option B's rejection rationale; e769f33:docs/architecture/design-decisions/ADR-0040-gr-history-ring-capacity-is-a-duration.md:129 — kSize 1<<18; e769f33:src/dsp/GrHistoryBuffer.h:174
- e769f33:docs/DESIGN.md:288-290 — 'per-block GR + waveform minima/maxima'; e769f33:src/dsp/GrHistoryBuffer.h:104-108 — Entry {grDb, peak}
- e769f33:docs/architecture/design-decisions/ADR-0003-oversampling-scope-and-true-peak-measurement-tap.md:164-171 against e769f33:src/dsp/TruePeak.h:8-13 and :43-44 (12 taps × 4 phases) and e769f33:docs/FUTURE_RISKS.md:129-139 (measured 6-sample lag)
- e769f33:src/gui/FrameClock.h:1 — 'copied from Anamorph … (namespace only)'; :13-14 name Vectorscope/StereoMeter/SpectrumImager

**Current behaviour.** ADR-0016's Decision still states a default of true, although the product default has been false (GR view) since ADR-0023 item 7, with no forward pointer. Two meter comments give a 24 Hz display cadence that neither the view (vblank-paced FrameClock) nor the RMS readout (~3 Hz) has. The other cited items are historical rationale, a correctly-sized estimator, or provenance-preserving copy text.

**Problem.** Accepted-ADR text is authoritative under SOURCE_OF_TRUTH. A maintainer reading ADR-0016 alone would conclude that the spectrum view is the default, and might 'restore' it: that is a change to a serialized default plus a visible change of the default view. Stale cadence comments mislead anyone tuning meter refresh or cost.

**Root cause.** ADR-0023 recorded the default flip inside its own item list but did not apply the forward-banner and registry-row convention that ADR_INDEX.md:88-94 defines for partial amendments. The dsp and meter-view header comments were not re-read when the view moved onto its own FrameClock and the RMS readout hold was added in 0.1.3.

**User impact.** No direct end-user impact. The risk is maintainer regressions: flipping int_spectrumOn back to true would change the first thing every user sees in the graph well. *Scope:* docs/architecture/design-decisions/ADR-0016 and ADR_INDEX.md; comments in src/dsp/RmsMeter.h and src/gui/LoudnessMeterView.h; optionally DESIGN §2.9.

**Proposed improvement.**

1. Add a dated amendment banner under ADR-0016's Status: 'Default amended by ADR-0023 item 7 (0.1.2): false — GR is the default view; the meaning is unchanged.'
2. Add the registry row 0016←0023 to ADR_INDEX, and correct the '0.1.2 item 4' reference in InternalState.h to match ADR-0023's numbering.
3. Rewrite the two cadence comments: the meter view ticks at display rate (FrameClock, ≤~125 Hz, 60 Hz fallback), and the RMS numeric row adopts at ~3 Hz. The 10 ms recompute argument then reads 'finer than the readout adopts'.
4. Add a §4.3-style 'Superseded in part' banner to DESIGN §2.9: the GR ring stores the peak only; the spectrum is the graph-well mode (ADR-0016); the targets were removed (ADR-0015).
5. Leave ADR-0038's Options text, ADR-0003 D7 and the FrameClock copy as they are. Optionally add a one-line pointer from D7 to RISK-008's measured lag, and an Anabasis note below FrameClock's provenance line.

**Alternatives considered.**

- *Amend the ADR-0016 Decision text in place* — Rejected. ADR_INDEX.md:90-94: ADRs keep their original text and carry forward banners.
- *Fix every cited item including ADR-0038, ADR-0003 and FrameClock* — Rejected in part. Those texts are historical rationale, already consistent with the code, or provenance-bound, so editing them adds churn and could falsify the provenance claim.
- *Leave as-is* — Rejected for ADR-0016. It is the one item where a stale Accepted Decision could drive a visible default regression.

**Decision: Modify · P3.** Two items are real and cheap to fix: the ADR-0016 default, which also exposes a gap in the repository's own amendment registry, and the cadence comments. Three of the six sub-claims do not hold up as misleading drift, so the change is narrower than the finding proposes.

**Architecture gates.**

- A forward banner on Accepted ADR-0016 records an existing decision (ADR-0023 item 7) and triggers no gate
- Hazard if the stale text were acted on: returning int_spectrumOn's default to true is a serialized-default change that touches the Serialization Registry row of ARCHITECTURE_REVIEW_GATE and would conflict with Accepted ADR-0023. Not proposed

**Dependencies.** [DOC-008](findings-doc-test.md#doc-008)

**Acceptance criteria.**

- ADR-0016 carries a dated forward banner naming ADR-0023 item 7 and the false default; ADR_INDEX's amendment registry lists 0016←0023
- No comment in src/ describes the meter display or RMS readout cadence as 24 Hz
- DESIGN §2.9 carries a 'Superseded in part' banner that points to ADR-0016 and ADR-0015 and to the ring's actual Entry fields
- ADR-0038's option text, ADR-0003 D7 and FrameClock.h's provenance line are unchanged, or changed only by an added pointer

<details><summary>Verification record</summary>

**Method.** I read ADR-0016:68-84 (the Decision), ADR-0023:240-244 (item 7), InternalState.h:34 and :112, and the ADR_INDEX.md:88-106 amendment registry. For the 24 Hz item I read RmsMeter.h:44-50, LoudnessMeterView.h:19-20, :66-67 and :121, LoudnessMeterView.cpp:62, FrameClock.h:1-30 and :69-72, and PluginEditor.cpp:1000. For the ring size I read ADR-0038:58-66 and ADR-0040:3, :20 and :129, plus GrHistoryBuffer.h:98-112 and :174. I also read DESIGN.md:280-296 for the minima, and for the true-peak estimator ADR-0003:160-171, TruePeak.h:1-60 and FUTURE_RISKS.md:129-139.

**Corrections to the candidate claim.** Confirmed:
- ADR-0016:76 says the type and default (true) are unchanged. ADR-0023 item 7 (:240-244) flipped the default to false, and the code agrees (InternalState.h:34, :112). ADR-0016 has no amendment banner, ADR-0023 never cross-links ADR-0016, and ADR_INDEX's 'ADRs amended by a later ADR' registry has no 0016←0023 row. Minor: InternalState's comments call the flip '0.1.2 item 4', while ADR-0023 numbers it item 7.
- RmsMeter.h:44-47 '24 Hz display' is stale. LoudnessMeterView ticks on its own FrameClock (LoudnessMeterView.cpp:62; FrameClock caps at ~125 Hz and falls back to 60 Hz), and the RMS row adopts a new value at ~3 Hz (LoudnessMeterView.h:121). A second stale instance: LoudnessMeterView.h:66-67 says '~24 Hz meter tick'. The likely source is the editor's own 24 Hz timer (PluginEditor.cpp:1000). The comment's cost argument is unaffected.
- DESIGN.md:288-290 'waveform minima/maxima' holds against GrHistoryBuffer::Entry {grDb, peak} (:104-108), but DESIGN is a historical record (:23-28). §2.9 also still says the spectrum is 'dismissible' and lists target lines (:291-296).

Overstated or refuted:
- ADR-0038:63 '4096 entries' is a parenthetical in the Options section explaining why option B was rejected. It describes the ring at authoring time, the argument does not depend on size, and ring capacity is decided by ADR-0040 (clause 6, 1<<18), not by ADR-0038. This is not binding text.
- ADR-0003 D7 is REFUTED as stated. '48-coefficient, 4-phase' matches the code: TruePeak.h:43-44 is 12 taps per phase × 4 phases = 48 coefficients. The real differences are that the code designs a windowed sinc at prepare() instead of using the Annex-2 table (documented at TruePeak.h:8-13), and that the lag is 5.5-6 samples versus 5.9. D7 itself says such a difference triggers no amendment (:169-171), and RISK-008 records the measured lag.
- FrameClock.h:13-14 does name Anamorph's visualisers, but the file is a provenance-tagged copy that says 'namespace only' (FrameClock.h:1, ADR-0009). Editing the comment without updating the provenance line would make that line false.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 1 · severity 2 · discoverability 2 · efficiency 1 · coherence 3 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DOC-010

**Stale comments and docs misdescribe the state contracts future UI work will build on: BASELINE, '49 wide', detach 'at P4', the Learn undo bracket, 'captured and re-asserted', registry line anchors, and §7.1's 'one undo step'**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P3** | high | confirmed | Documentation | User and design documents contradict the shipped product | Phase 5 |

**Evidence**

- e769f33:src/PluginProcessor.h:466 — 'BASELINE (absent until a macro gesture, §5.3/P4)', against e769f33:docs/architecture/SERIALIZATION_REGISTRY.md:194-206 ('No code path in this build originates one')
- e769f33:src/PluginProcessor.h:449-450 and e769f33:src/PluginProcessor.cpp:82-83 — '49 wide' / 'freezes the surface at 49', against e769f33:docs/architecture/design-decisions/ADR_INDEX.md:24 (the suites assert 50)
- e769f33:src/MacroEngine.h:17-21 — 'P1 scope … detach/re-engage latch and gesture bracketing land at P4'
- e769f33:src/PluginProcessor.h:652-653 — 'the P5 UI adds the duck-routed engage + undo bracketing', against e769f33:src/gui/PluginEditor.cpp:622-639 (no bracket) and e769f33:docs/policies/MODE_AND_ADAPTATION_POLICY.md:161-163 (the Learn commit is deliberately outside undo)
- e769f33:docs/DESIGN.md:961 — 'per-slot adaptive/Learn memory', against e769f33:docs/architecture/design-decisions/ADR-0007-state-schema-v1.md:83-86 (ADAPTIVE is global)
- e769f33:docs/architecture/design-decisions/ADR-0010-parameter-surface.md:191-193 and e769f33:src/PresetManager.h:16-17 — 'captured and re-asserted', against e769f33:src/PresetManager.cpp:67-68 and :112-119 ('Ceiling lock is a SKIP, not a write-then-revert')
- e769f33:docs/architecture/SERIALIZATION_REGISTRY.md:291 — ':1693' and ':1712', where the code is at e769f33:src/PluginProcessor.cpp:1919 and :1938
- e769f33:docs/architecture/SERIALIZATION_REGISTRY.md:178 — '`saveSlotFromLive`, e769f33:src/PluginProcessor.cpp:1212-1215'; the lines are in engineFrozenTrimsIfLive (e769f33:src/PluginProcessor.cpp:1202-1217)
- e769f33:docs/user/USER_MANUAL.md:376 — 'form **one undo step**', against the retraction of a no-op apply (e769f33:src/PluginProcessor.cpp:459-546)
- *Merged at triage from another verifier's note, quoted as written:* Add a third location of the stale 'captured and re-asserted' wording. [DOC-010](findings-doc-test.md#doc-010) cites ADR-0010:191-193 and PresetManager.h:16-17; DESIGN §4.2 (e769f33:docs/DESIGN.md:568-570) also says preset apply 'captures and re-asserts ceiling exactly like a view param'. The code skips the write (e769f33:src/PresetManager.cpp:113-119). Sync all three in the same doc pass.
- *Merged at triage from another verifier's note, quoted as written:* Add one stale test comment to [DOC-010](findings-doc-test.md#doc-010)'s list. e769f33:tests/state_tests.cpp:2503-2504 says reset-to-macro 'is the sanctioned clear and pushes no step of its own'. Re-checked: resetToMacro calls pushUndoStep(saveSlotFromLive()) before the change (e769f33:src/PluginProcessor.h:225-236), and USER_MANUAL.md:333-334 calls it undoable. Reword the comment to 'pushes its own pre-state undo step'. The test's premise (mask cleared) is unaffected.

**Current behaviour.** Header comments and contract documents describe:
- a BASELINE child produced by macro gestures (nothing produces one);
- a 49-parameter surface (it is 50);
- detach and bracketing as future P4 work (it is shipped);
- a Learn undo bracket the UI is supposed to add (policy forbids it);
- a lock implemented as capture-and-reassert (it is a skip);
- registry line anchors that point at the wrong code.
The manual promises exactly one undo step per load, although a no-op load leaves none.

**Problem.** These are the contracts anyone changing presets, A/B, undo, Learn or the macro UI reads first. Several invite wrong implementations: adding an undo bracket around Learn, which the policy forbids; producing a BASELINE from gestures, which contradicts the registry's 'a writer must not invent one'; or re-implementing the lock as write-then-revert, which the .cpp explains lets an audio block glide toward the preset ceiling.

**Root cause.** Forward-looking status comments ('P1 scope', 'P5 UI adds', 'lands at P4') were written as plans and never revisited once the plans landed or were reversed. Line-number anchors in SERIALIZATION_REGISTRY were not re-derived after code moved. The lifecycle trigger map covers schema changes but not in-code contract comments.

**User impact.** No direct end-user impact; the manual §7.1 wording is harmless to users. The maintainer risk is concrete for the Learn-undo and lock comments, because both contradict a binding policy or design reason. *Scope:* Comments in src/PluginProcessor.h/.cpp, src/MacroEngine.h and src/PresetManager.h; docs/architecture/SERIALIZATION_REGISTRY.md :178 and :291; ADR-0010 (by amendment banner); DESIGN §7 (by banner); USER_MANUAL §7.1.

**Proposed improvement.**

1. BASELINE comment → 'schema-reserved: adopted, carried and dropped only; no producer (SERIALIZATION_REGISTRY §1.4)'.
2. '49' → drop the number: 'the surface fits in one 64-bit word; the constructor asserts it'.
3. MacroEngine header → describe the shipped detach, re-engage and bracketing in the present tense.
4. Learn comment → 'Learn is deliberately outside undo (MODE_AND_ADAPTATION_POLICY)'.
5. PresetManager.h → 'a locked ceiling is skipped, never written'. Add an amendment note on ADR-0010 recording that the mechanism is a skip with the same guarantee.
6. SERIALIZATION_REGISTRY → cite function names instead of bare line numbers, or re-derive :178 and :291.
7. DESIGN §7 banner → learned targets are global (ADR-0007) and frozen trims are per-slot.
8. Manual §7.1 → 'a load forms at most one undo step — one that changes nothing leaves none'.

**Alternatives considered.**

- *Leave as-is* — Rejected. The Learn and lock comments contradict binding policy or the design reason and invite regressions.
- *Change the code to match the comments (bracket Learn into undo; capture and re-assert the lock)* — Rejected. Bracketing Learn contradicts MODE_AND_ADAPTATION_POLICY.md:161-163. Write-then-revert reintroduces the glide window that PresetManager.cpp:112-117 documents.
- *Replace line anchors with symbol anchors throughout the registries* — Worth doing where cheap. It removes the recurring stale-anchor class.

**Decision: Proceed · P3.** All items are confirmed against the code, and the fixes are comment and text edits with no behaviour change. Two of the stale texts contradict binding policy, so correcting them removes a real maintainer trap before any state-model UI work.

**Architecture gates.**

- No hard-stop category. ADR-0010's lock sentence must be corrected by an amendment banner or registry row, not rewritten (ADR_INDEX.md:90-94)
- Any change that made the code follow these comments (adding Learn undo, producing BASELINE, write-then-revert lock) would be a state-model or Simple/Advanced macro-layer contract change, and is not proposed

**Dependencies.** [STATE-009](findings-state-model.md#state-009); [DOC-004](findings-doc-test.md#doc-004)

**Acceptance criteria.**

- No comment in src/ says BASELINE is produced by a macro gesture, the parameter surface is 49, detach or bracketing 'land at P4', or the UI adds undo bracketing around Learn
- PresetManager.h describes the ceiling lock as a skip; ADR-0010 carries an amendment note to the same effect
- Every line anchor in SERIALIZATION_REGISTRY.md §1.3 and §2 resolves to the named function or guard at HEAD, or is replaced by a symbol reference
- USER_MANUAL §7.1 says a load forms at most one undo step

<details><summary>Verification record</summary>

**Method.** Against the code: I read PluginProcessor.h:444-470 and :645-658, PluginProcessor.cpp:78-88, :1198-1245, :1875 and :1915-1940, and MacroEngine.h:10-26. I read PresetManager.h:8-24 and PresetManager.cpp:61-68 and :112-126, and PluginEditor.cpp:614-639 (the Learn button).
Against the docs: I read SERIALIZATION_REGISTRY.md:170-206 and :280-291, ADR-0010:186-195, ADR-0007:83-86, MODE_AND_ADAPTATION_POLICY.md:159-163, DESIGN.md:951-963, USER_MANUAL.md:370-376 and ADR_INDEX.md:24.
I grepped src for BASELINE producers (none; only adopt, carry and drop sites) and for 'retract' in PluginProcessor.cpp.

**Corrections to the candidate claim.** Every item is confirmed; three need precision. (1) The SERIALIZATION_REGISTRY anchors: :280's range '1822-2019' is correct at e769f33. The stale anchors are in the row at :291 (':1693' stored-slot guard and ':1712' active-slot gate are now :1919 and :1938; ':1875' is still right) and at :178, where '1212-1215' is attributed to saveSlotFromLive but lies in engineFrozenTrimsIfLive (:1202-1217). (2) DESIGN §7's 'per-slot adaptive/Learn memory' (:961) is ambiguous rather than flatly wrong. The frozen trims are per-slot, while the learned targets are global (ADR-0007:83-86). (3) The Learn comment is worse than stale: it says 'the P5 UI adds … undo bracketing', but the P5 UI calls startLearn()/stopLearn() directly (PluginEditor.cpp:622-639), and MODE_AND_ADAPTATION_POLICY.md:161-163 says the Learn commit stays OUTSIDE undo deliberately. The comment contradicts policy. The '49' also appears at PluginProcessor.cpp:82-83; ADR_INDEX.md:24 records that the suites assert 50.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 1 · severity 2 · discoverability 2 · efficiency 1 · coherence 3 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### DOC-011

**Engineering status banners are stale: THREAD_MODEL's 'Status: P4' header, REALTIME_SAFETY_AUDIT saying RTSan has never run, and TESTING.md's 'P1 skeleton' banner above an empty headless-tests section**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P3** | high | partially-confirmed | Documentation | User and design documents contradict the shipped product | Phase 5 |

**Evidence**

- e769f33:docs/architecture/THREAD_MODEL.md:8-11 — 'Status: P4 … Still-planned rows (the P5 spectrum rings …)', against :239 'Spectrum capture rings — IMPLEMENTED (P5, 2026-08-02)'
- e769f33:docs/architecture/THREAD_MODEL.md:87-90 — 'at P1 the only such read is getLatencySamples()', followed by :93-114 and :186-224 listing later reads; no mention of the detach mask. e769f33:src/gui/PluginEditor.cpp:1359-1363 reads proc.detachMask() in paint() ([TECH-002](findings-dsp-tech.md#tech-002))
- e769f33:docs/architecture/REALTIME_SAFETY_AUDIT.md:86-90 — 'RealtimeSanitizer has not yet run in this repository … the first green `realtime` run is **owed**'
- e769f33:docs/architecture/design-decisions/ADR-0029-realtime-enforcement-strategy.md:190-199 — 'The DSP suite runs violation-free under RealtimeSanitizer: 296 checks, 0 failures'
- e769f33:docs/procedures/TESTING.md:6-9 — 'P1 skeleton, 2026-07-31 … Rows … still marked TODO (P2+)'; :11-14 '## Headless self-tests' is followed directly by the next heading
- e769f33:docs/HANDOVER.md:29 — '1866 checks green on Linux'
- e769f33:docs/HANDOVER.md:28 — 'C++20, warning-free' and 'The §2.1 C++23 canary is wired'
- *Merged at triage from another verifier's note, quoted as written:* Add to [DOC-011](findings-doc-test.md#doc-011)'s THREAD_MODEL refresh: THREAD_MODEL.md:216-217 cites 'juce_OpenGLContext.cpp:372, scopedLock.emplace (mmLock)'. In the pinned 9.0.1 tree, :373 declares the std::optional and the emplace is at :405 (re-checked in build/_deps/juce-src). The substance is correct; only the anchor is stale.

**Current behaviour.** THREAD_MODEL's header says the tree is at P4 with the spectrum rings still planned, while its own body documents them as implemented. The realtime audit lists 'RTSan has never run' as an open gap, while ADR-0029 records a clean RTSan run. TESTING.md opens with a P1-skeleton status that points to TODO rows which no longer exist, above an empty section.

**Problem.** Anyone planning UI or threading work, and so reading these files for 'what is verified', meets a first paragraph that understates the verification state. The detach-mask paint read is missing from the thread model, but that is a separate defect ([TECH-002](findings-dsp-tech.md#tech-002)).

**Root cause.** The DOCUMENTATION_LIFECYCLE_POLICY trigger map (:16-37) routes threading changes to THREAD_MODEL and test changes to TESTING.md. Contributors updated the bodies, but the phase-dated status banners at the top of each file are in no row's checklist and were not revisited. The RTSan gap line was written in the same round that ADR-0029 recorded the run, and it was never struck through.

**User impact.** No end-user impact. For maintainers there is a misleading first impression of the verification state. The body text is correct, so recovery requires reading past the banner. *Scope:* docs/architecture/THREAD_MODEL.md header, docs/architecture/REALTIME_SAFETY_AUDIT.md Gaps list, docs/procedures/TESTING.md banner and empty section, and (related) docs/HANDOVER.md Build Status.

**Proposed improvement.**

1. Replace each phase-dated status banner with a current, dated one-liner, or with 'status of record: HANDOVER.md', so there is one place to keep current.
2. Strike the audit's RTSan gap and point to ADR-0029 §Evidence and the CI realtime job.
3. Fill the 'Headless self-tests' section (how to run both suites, what the counts mean), or delete the heading.
4. Once [TECH-002](findings-dsp-tech.md#tech-002) is decided, add the detach-mask paint read to THREAD_MODEL's 'Which context paints' list, or record its removal.

**Alternatives considered.**

- *Leave as-is because the bodies are correct* — Rejected. The banner is the part everyone reads, and the RTSan line contradicts an Accepted ADR's evidence.
- *Remove all status banners from descriptive docs* — Viable. HANDOVER.md already is the status of record (CLAUDE.md), and duplicated status lines are the recurring stale class.

**Decision: Proceed · P3.** The stale banners and the RTSan contradiction are confirmed, and the fix is text-only. The [TECH-002](findings-dsp-tech.md#tech-002) linkage in the claim is overstated: the missing paint-read entry is [TECH-002](findings-dsp-tech.md#tech-002)'s own defect and is handled there.

**Architecture gates.**

- No hard-stop category for the documentation edits. Resolving [TECH-002](findings-dsp-tech.md#tech-002) by changing how paint() reads the detach mask would be a threading-model change (hard stop) and is out of scope here

**Dependencies.** [TECH-002](findings-dsp-tech.md#tech-002)

**Acceptance criteria.**

- THREAD_MODEL.md's opening status names no still-planned spectrum rings, and matches the Planned-edges section
- REALTIME_SAFETY_AUDIT.md no longer says RTSan has never run; it cites ADR-0029's recorded run
- TESTING.md's banner describes the current suites, or defers to HANDOVER, and no empty section remains
- THREAD_MODEL's painting-thread list names every GUI read made from paint(), including the detach mask while it exists

<details><summary>Verification record</summary>

**Method.** I read THREAD_MODEL.md:1-20, :84-114, :186-224 and :237-263, and grepped it for the detach mask. I read PluginEditor.cpp:1355-1363, REALTIME_SAFETY_AUDIT.md:78-95, ADR-0029:12 and :186-205, CHANGELOG.md:582 and :611, TESTING.md:1-14 (including a cat -A of the empty section) and HANDOVER.md:26-30. I counted 'TODO (P2+)' occurrences in TESTING.md and DSP_POLICY.md.

**Corrections to the candidate claim.** (1) THREAD_MODEL: only the header banner (:8-11) is stale. The body is current: Planned edges marks the spectrum rings 'IMPLEMENTED (P5, 2026-08-02)' at :239, and :104-114 and :186-224 cover ADR-0038 and ADR-0039.
(2) The ':90 lists getLatencySamples() as the only paint read' claim is overstated. The sentence is scoped 'at P1', and the paragraphs that follow add the 0.1.4 popup query and the ADR-0038 and ADR-0039 reads. The detach-mask read in paint() (PluginEditor.cpp:1359-1363) is indeed absent from THREAD_MODEL, but that omission is [TECH-002](findings-dsp-tech.md#tech-002) itself, not a consequence of the stale banner.
(3) REALTIME_SAFETY_AUDIT:86-90 contradicts ADR-0029's own Evidence. The DSP suite ran violation-free under RTSan (296 checks, ADR-0029:197-199), and the CHANGELOG later reports RTSan clearing (:582, :611).
(4) TESTING.md's banner points to 'Rows … still marked TODO (P2+)', but the only occurrence of that string in the file is the banner itself. '## Headless self-tests' (:11) is an empty section.
(5) Additional stale status in the same family: HANDOVER.md:28 'Build Status' still reads 'C++20' and describes a C++23 canary, while ADR-0030 moved the baseline to C++23 and removed the canary (this record's evidence already cites that row).

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 1 · severity 2 · discoverability 2 · efficiency 2 · coherence 3 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

## TEST — Test infrastructure

### TEST-001

**The editor's 24 Hz tick (bypass dim, out LUFS, GR lanes, Learn state, undo enablement, graph-mode flip, edited badge) and the tooltip gate have no test. The tick's work is headlessly reachable and sits outside the ADR-0025 exception; only the modal and pointer surface needs a driven-input fixture**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | partially-confirmed | Test infrastructure | Verification stops at the headless boundary, and gesture/restore threading can crash the host | Phase 1 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:1972-2131 — timerCallback. The sub-steps are :1986 (combo 'hov' flag), :2031-2032 (settings boxes, preset display), :2033-2035 (bypass dim), :2038-2044 (out LUFS text), :2046-2075 (Learn countdown, accent and warn colours), :2078 (undo/redo enablement), :2083-2100 (GR lanes), :2103 (ceiling unit), :2106-2117 (spectrumView/grView visibility), :2119-2130 (editedDot)
- e769f33:src/gui/PluginEditor.h:273 — timerCallback is a private override of a privately inherited juce::Timer, so tests cannot call it
- e769f33:src/gui/PluginEditor.h:59-76 — the two tick directions made public 'so the headless suite can drive it', after regressions in rounds 26 and 32 and the ceiling-unit regression
- e769f33:tests/state_tests.cpp:4642 and :10913 — the only tick directions driven, called directly ('the 24 Hz tick's edge, driven directly')
- grep at e769f33: tests/state_tests.cpp contains no timerCallback, dimOverlay, outLufsValue, learnButton, refreshUndoRedoEnablement, refreshPresetDisplay, compGrMeter, spectrumView or editedDot
- e769f33:tests/state_tests.cpp:5965-6055 — tooltip sweep, asserting only getTooltip().isEmpty() / isNotEmpty()
- e769f33:src/gui/PluginEditor.h:435-447 — GatedTooltipWindow::getTipFor gate (a plain virtual call)
- e769f33:docs/DOCUMENTATION_COVERAGE.md:1313-1323 — the ADR-0025 disclosure, listing the shield, lifetime, focus, inline-edit and tooltip gate but not the tick directions
- e769f33:docs/procedures/TESTING.md:199-211 (:207 'nothing here runs a message loop')
- e769f33:docs/architecture/design-decisions/ADR-0025-regression-test-exception.md — Decision (class 1: pointer or keyboard inside a modal loop) and Option 4 (GUI automation harness, deferred)
- e769f33:worklogs/2026-08-10-linux-editor-input-probe.md:14-27 — 'Three temporary programs, none of them in the build'
- screenshot session capture `rt/gestures/34e-learn-10s.png` (G-18): orange LEARN, driven by the tick; session capture `rt/visuals/08b-silence-3s-editor.png`: out LUFS -34.5 — both working today

**Current behaviour.** The 24 Hz tick drives about 11 state-to-widget directions. The suite drives two of them directly through public helpers. The other nine run only when a message loop runs (pluginval's editor open/close, and real hosts), and nothing asserts on them. Tooltips are checked only for non-empty strings. The Settings 'Tooltips' gate predicate and all pointer, modal and focus behaviour are untested; the latter is disclosed under ADR-0025.

**Problem.** A change to the feedback layer can break any of these displays while the suite stays green: the bypass dim, the out-LUFS readout, the Learn grammar (countdown, accent while learning, warn flash on an empty pass), undo/redo enablement, the GR|SPEC visibility flip or the edited badge. The repository has already seen this class regress three times (the Settings combos in round 26, the target checkboxes in round 32, the ceiling unit), and each time the fix was to expose that one direction. The remaining directions were never given the same treatment.

**Root cause.** timerCallback is private, and no message loop runs in AnabasisStateTests, so the tick never fires. Coverage was added one direction at a time, after each regression, rather than by making the tick body callable. The modal and pointer half needs a driven-input fixture, which ADR-0025 Option 4 deferred. The XTEST probe of 2026-08-10 stayed a set of throwaway programs.

**User impact.** Nothing is wrong today; the observers saw every direction working. The exposure is to future edits: a regression in bypass dim, the Learn state or undo enablement would reach users as a misleading status display, for example Learn looking finished while it is still running, or undo appearing unavailable. This audit's own proposals (Learn clarity, stale and bypass indicators, readout labels) edit exactly this code. *Scope:* src/gui/PluginEditor.{h,cpp} timerCallback and the helpers it calls, and tests/state_tests.cpp. The modal, pointer and tooltip-display half concerns the pop-up shield, focus and inline-edit code, and is out of scope for the headless fix.

**Proposed improvement.** Split the gap along ADR-0025's own line. (A) Headless half, now: expose the tick body through one public, message-thread-only entry, for example refreshFromModel() that timerCallback calls, or tickForTest(). This follows the PluginEditor.h:59-76 precedent. Then pin each direction on a constructed, never-shown editor: bypass parameter to dimOverlay visibility; Simple-view out-LUFS text ('-' at the sentinel, one decimal otherwise); undo/redo enabled state before and after an undoable gesture and after proc.undo(); int_spectrumOn to spectrumView and grView visibility; detachMask to editedDot in Simple view; Learn states (digit 1..5 with accent while learning, warn colour on the tick after an empty-pass stop), with the millisecond clock injected or the checks bounded so they do not depend on wall time. Also expose the tooltip gate predicate and assert that getTipFor returns empty while tooltipsOn is false. Mutation-verify each test by deleting its tick branch. (B) Pointer and modal half: stays under ADR-0025 until a driven-input fixture exists (Option 4). The disclosure keeps covering the shield, focus, inline-edit and tooltip display timing.

**Alternatives considered.**

- *Leave as is* — Rejected. It contradicts the repository's own stated rule ('a direction nothing can call is a direction nothing can guard'), and this class has regressed three times.
- *Build the XTEST/driven-input fixture first and cover everything through real input* — Deferred, per ADR-0025 Option 4. It is a three-platform project and is only needed for the modal and pointer class. It is not needed for the tick directions, which are reachable today.
- *Add one public helper per direction (the existing pattern)* — Workable, but it widens the public surface nine more times, and a future tick direction starts out unguarded again. A single tick-body entry covers new directions automatically.
- *Whole-editor pixel snapshots* — Not recommended. Font and platform variance makes them brittle (see [TEST-008](findings-doc-test.md#test-008)'s measured 26 px vs 31 px spread). Widget-state assertions are exact and cheaper.

**Decision: Modify · P2.** The claim is substantively right but overbroad. The high-value part, the tick's state-to-widget directions, needs no new infrastructure and is not covered by the ADR-0025 exception, so it should be closed with the established pattern. The pointer and modal part is legitimately deferred under ADR-0025 and should stay there rather than drive an expensive harness project now.

**Dependencies.** [TEST-004](findings-doc-test.md#test-004) (same headless-reach problem for the STATISTICS panel; same approach); ADR-0025 Option 4 driven-input fixture (only for the pointer/modal/tooltip-display half)

**Acceptance criteria.**

- AnabasisStateTests runs the editor's tick body with DISPLAY unset, through a public entry that timerCallback also calls, so the two cannot diverge
- Bypass parameter 1 → dimOverlay.isVisible() is true after one tick; 0 → false. Deleting the branch at PluginEditor.cpp:2033-2035 fails a named check
- Simple view with the meter at the sentinel → the out-LUFS label reads '-'; after a tone it reads proc.meterLufsS() to one decimal
- The undo button is disabled on a fresh editor, enabled after an undoable gesture, and redo is enabled after proc.undo()
- int_spectrumOn true → spectrumView visible and grView hidden after one tick; false → the reverse
- Learn: while learning, the button text is an integer in 1..5 and textColourOffId == colours::accent; after an empty-pass stop, the next tick shows colours::warn (clock injected or bounded)
- editedDot is visible in Simple view iff proc.detachMask() is non-empty
- GatedTooltipWindow::getTipFor returns an empty string while the Settings Tooltips switch is off
- Each new check fails when its tick branch is removed (mutation-verified, per ADR-0025's anti-vacuity clause)
- DOCUMENTATION_COVERAGE's ADR-0025 disclosure still lists the shield, focus, inline-edit and tooltip-display surface as untested, with no change to its lapse condition

<details><summary>Verification record</summary>

**Method.** Read timerCallback at e769f33:src/gui/PluginEditor.cpp:1972-2131 and each of its sub-steps. Grepped tests/state_tests.cpp for timerCallback, dimOverlay, outLufsValue, learnButton, refreshUndoRedoEnablement, refreshPresetDisplay, compGrMeter, spectrumView and editedDot: none occur. Also grepped for the two public tick helpers, which are driven directly (:4642, :10913). Read the public-hook rationale in PluginEditor.h:59-76, GatedTooltipWindow (PluginEditor.h:435-447), the tooltip sweep (state_tests.cpp:5965-6055), the ADR-0025 text, the DOCUMENTATION_COVERAGE disclosure (:1313-1323), TESTING.md:199-211 and the 2026-08-10 input-probe worklog. Viewed screenshots session capture `rt/gestures/34e-learn-10s.png` (orange LEARN, which is drawn by the tick) and session capture `rt/visuals/08b-silence-3s-editor.png` (out LUFS -34.5). Both show the behaviour working today. Runtime reproduction was not needed.

**Corrections to the candidate claim.** (1) 'No automated driver' is only partly true. Two tick directions, refreshInternalSettingsBoxes and refreshCeilingUnit, were made PUBLIC so the headless suite can drive them (PluginEditor.h:59-76: 'A direction nothing can call is a direction nothing can guard'), and tests call them directly (state_tests.cpp:4642, :10913). (2) The finding merges two classes. The ADR-0025 disclosure (DOCUMENTATION_COVERAGE.md:1313-1323) covers the pop-up shield, pop-up lifetime, focus release, inline-edit abandonment and the tooltip gate. It does NOT cover the tick's state-to-widget directions. Those need no pointer and no modal loop, so they are reachable headlessly and simply uncovered. ADR-0025 does not excuse them. (3) The scope is wider than the title says: refreshPresetDisplay (:2032), the combo hover flag (:1986) and the edited-badge flip (:2119-2130) are also untested. (4) The tooltip gate's predicate (GatedTooltipWindow::getTipFor returns empty while tooltipsOn is false) is a plain call and could be tested if exposed. Only hover timing, placement and stale text need a real pointer.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 3 · severity 3 · discoverability 2 · efficiency 2 · coherence 3 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### TEST-002

**No real-host or real-device Level-5 validation is on record for the behaviours the audit could only emulate: host bypass routing, transport stop, bursty delivery, offline render with MATCH/DELTA, host DPI, automation recording, post-pop-up click routing on real pointers, and composited-desktop repaint**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P1** | high | confirmed | Test infrastructure | Verification stops at the headless boundary, and gesture/restore threading can crash the host | Phase 0 |

**Evidence**

- e769f33:docs/policies/TESTING_POLICY.md:16: Level 5 is manual validation in a DAW; :131-132: 'Level 5 is required for final sign-off but cannot gate CI'
- e769f33:docs/procedures/TESTING.md:761-770: GUI appearance, HiDPI, automation recording, offline render, rescan and session restoration are not verifiable headlessly and are a 'required release precondition'
- e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:104-110: Result table A-E all TODO; e769f33:docs/HANDOVER.md:186-187: the real Level-5 pass is deferred to the fine review
- e769f33:docs/OPEN_QUESTIONS.md:544-568: OQ-017, bursty host delivery, open and modelled rather than measured in a host
- JUCE 9.0.1 (fetched) juce_audio_plugin_client_VST3.cpp:3727-3732 and e769f33:src/PluginProcessor.cpp:722-725: host bypass reaches processBlock through the bypass parameter, not processBlockBypassed
- Runtime E07 (edges, harness-emulated host bypass): [capture](captures/09-host-bypass-frozen.png)
- Runtime ST-01 and E20 (warp+click misroutes): session capture `rt/state/05b-repro-after.png` and session capture `rt/edges/fall-r3-strip.png`
- Runtime ST-10 and E21 (lingering pop-up pixels): session capture `rt/edges/23-sel-sequence-strip.png`
- Runtime E22: the harness is a Release build, so jassert could not fire
- Runtime V13-6, real Standalone window: session capture `rt/verify-13/trials/strip.png`. Rows 1-5 are stepped (0/5 misroutes); rows 6-10 are warp+click (3/5: MATCH not toggled, Loudness 60% to 0%). Action log: rt/verify-13/actions.log

**Current behaviour.** All GUI and host-interaction evidence so far comes from headless suites, pluginval under xvfb, a minimal JUCE VST3 host (KI-012) and this audit's Xvfb harness driving the processor. No DAW session is recorded for host bypass, transport stop and start, anticipative or bursty processing, offline bounce with MATCH/DELTA, host DPI scaling, automation write and read, real pointer or touch input after a pop-up, or a composited desktop. The brand checklist's Level-5 result table is entirely TODO.

**Problem.** Several UI decisions in this audit rest on emulations that diverge from real hosts. Examples: whether a DAW bypass freezes the display or flips the plugin's BYPASS toggle; whether the GR history jumps under REAPER's anticipative FX; whether the post-menu click hijack can happen with a trackpad tap or touch. Fixing the emulated path could target the wrong behaviour.

**Root cause.** Level 5 is policy-required but structurally cannot gate CI (TESTING_POLICY:131-132). It was deferred to the post-v0.1.0 fine review, and no scripted real-host smoke procedure exists to make the pass repeatable. The audit harness is a Release build that calls processBlockBypassed directly and keeps audio flowing on 'transport 0'.

**User impact.** Some user-facing behaviour is unknown in exactly the places a mastering user meets every session: DAW bypass comparisons, stop and play, offline bounce, and HiDPI laptops. Undetected mismatches could mislead monitoring decisions (for example a frozen display read as 'no gain reduction'), and a real-input click misroute would silently change Loudness. *Scope:* All formats and hosts. Linux, macOS and Windows desktops. It covers the specific open items E07, ST-01/E20, ST-10/E21, OQ-017, the offline DELTA/MATCH path, automation and DPI.

**Proposed improvement.** Write a scripted Level-5 real-host smoke procedure in docs/procedures/ and run it before the fine review closes. Each step has an expected observation and a result row (host, version, OS, build).

Per host (REAPER plus one Linux host such as Bitwig or Ardour, Logic for AU, one Windows host), the steps are:
1. Engage DAW bypass: does the BYPASS toggle follow, and do the meters and GR freeze or keep running?
2. Transport stop and play: do I, LRA and TP reset, and does the GR history continue?
3. REAPER anticipative FX, and Cubase ASIO-Guard if available: GR scroll snaps (OQ-017).
4. Offline bounce with MATCH and with DELTA engaged, compared with realtime.
5. Record and play back Loudness and Ceiling automation.
6. Windows and macOS display scaling at 150% and 200%.
7. Preset menu followed by an immediate top-bar click, using a physical mouse, a trackpad tap, touch or pen where available, and a VNC/RDP session.
8. Combo selection repaint on a composited desktop (GNOME or KDE, macOS, Windows).
9. One pass with a Debug build to surface jasserts.

Fill in the brand checklist's Level-5 result table from the same session.

**Alternatives considered.**

- *Keep relying on harness emulation* — Rejected. The VST3 bypass path demonstrably differs from the emulation, and the misroute depends on input style.
- *Automate a real-DAW pass in CI* — Deferred. DAWs cannot run in CI (the policy says so), and ADR-0025 option 4 (a GUI harness) is a separate project.
- *Ad-hoc manual audition without a script* — Rejected. It is not repeatable, and it leaves the specific audit questions unanswered.

**Decision: Modify · P1.** Policy already requires Level 5 before sign-off. A scripted checklist costs no code risk, answers questions that decide several UI fixes (E07, ST-01, OQ-017), and verification here showed at least one emulation (host bypass) models a path the VST3 wrapper does not take.

*Calibration:* the verifier judged Proceed / P1; the final judgement is Modify / P1. Challenge agreed P1, resting on 'no DAW host has ever been observed loading Anabasis' (e769f33:docs/architecture/COMPATIBILITY_MATRIX.md:68) at 0.2.12 despite RELEASE_POLICY precondition 8, with the harness bypass path confirmed not to be what VST3/AU hosts take; confidence raised to high. Changed Proceed->Modify: extend RELEASE_COMPATIBILITY_CHECKLIST lines and record results in the COMPATIBILITY_MATRIX A-rows (and the BRAND result table), not a parallel procedure and table; set code-derived expected observations for bypass (a mapped DAW bypass flips BYPASS and processBlock keeps running); input-modality/VNC checks optional; a Debug-build session kept separate. This pass collects the evidence for [TECH-001](findings-dsp-tech.md#tech-001), [STATE-018](findings-state-model.md#state-018) (save prompt), [VIS-019](findings-visualisation.md#vis-019) (stepping and render-ahead lead), [MODEL-006](findings-state-model.md#model-006), [UX-010](findings-ux.md#ux-010), [STATE-004](findings-state-model.md#state-004) and [TECH-003](findings-dsp-tech.md#tech-003).

*Adversarial challenge:* evidence holds: yes; priority justified: yes (suggested P1); decision justified: no (suggested Modify). The gap is real and stronger than the judge argued: the repo itself records that no host has ever been observed loading the plugin, with pluginval as its only proxy (e769f33:docs/architecture/COMPATIBILITY_MATRIX.md:68-70), at version 0.2.12, despite RELEASE_POLICY precondition 8. The emulation divergence is now confirmed for both VST3 and AU, so at least one audit finding (E07) rests on a path real JUCE-wrapped hosts do not take. P1 holds under the rubric. The unknowns sit on behaviours a mastering user meets every session (DAW bypass comparison, stop/play, bounce), and a wrong UI fix built on the harness path is a realistic cost. The P1 case should rest on 'never observed in any host', not on the claim that the procedure costs no code risk, which is ease and cannot raise priority. The decision should be Modify rather than Proceed: extend the existing single-owner checklist and result table instead of creating a parallel procedure and table, set code-derived expected observations for the bypass step, and treat input-modality and VNC checks as optional depth. *Proposal risks:* (1) A new docs/procedures file with its own dated result table would fork a single-owner arrangement the repo states explicitly. COMPATIBILITY_MATRIX.md:72-77 says that what each audition pass must exercise 'is owned by docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md', and :85-86 says results belong in the A1-A3 table with host version, OS, what was run and what was observed. A second table would drift from those, the same staleness the repo guards against for the pluginval strictness. The smaller change is to add the missing items as lines in RELEASE_COMPATIBILITY_CHECKLIST.md, record results in the COMPATIBILITY_MATRIX A-rows, and record appearance and DPI results in the BRAND checklist Result table. (2) Step 1's expected observation should come from the code, not be left open. With VST3 kIsBypass and the AU bypassParam, a mapped DAW bypass should flip the plugin's own BYPASS toggle and keep processBlock, and so the meters, running. The case to record is a host that stops calling process, such as a deactivated device. E07-driven UI changes should wait for this result rather than be built against the harness path. (3) The scope grows beyond the brief's minimum (A1 REAPER/Windows, A2 Logic/AU). The Linux host is justified only through KI-012, so that session should also collect [TECH-001](findings-dsp-tech.md#tech-001)'s data. VNC/RDP, touch and pen for ST-01 are optional depth, not acceptance-blocking. (4) A Debug-build pass inside a DAW can stop or crash the host on the first jassert. It needs its own session, separate from the audition runs, so an assert does not abort the Level-5 pass. No hard-stop gate applies to the procedure itself. Any fix that follows (for example changing how host bypass is presented) would be judged at that time, and one touching bypass or the latency path would need gate review.

**Architecture gates.**

- None. It is a procedure and documentation only; no code, parameter, serialization, threading or latency change

**Dependencies.** [TECH-001](findings-dsp-tech.md#tech-001); E07 (host-bypass display); ST-01/E20 (post-pop-up click misroute); ST-10/E21 (pop-up repaint); OQ-017 (bursty delivery)

**Acceptance criteria.**

- A docs/procedures Level-5 real-host smoke procedure exists, with numbered steps and an expected observation for each of the items listed
- A dated result table names host, version, OS and build for each run
- Recorded outcomes exist for: DAW bypass (does BYPASS follow; do meters freeze) in at least one VST3 host and in Logic AU
- Recorded outcomes exist for: transport stop and play, the REAPER anticipative FX GR-scroll check, and offline bounce with MATCH and with DELTA
- Recorded outcomes exist for: automation write and read, 150%/200% host scaling, the post-pop-up click with a physical mouse, a trackpad tap and a VNC/RDP session, combo repaint on a composited desktop, and one Debug-build pass
- The BRAND_CONSISTENCY_CHECKLIST Result table is filled from that session

<details><summary>Verification record</summary>

**Method.** Opened every cited anchor at e769f33: TESTING_POLICY.md:16 and 131-132, TESTING.md:761-770, BRAND_CONSISTENCY_CHECKLIST.md:104-110 (all sections TODO) and :13-30 (Level-5 boxes deliberately unchecked), HANDOVER.md:186-187 ('the real Level-5 pass leads the fine review'), OPEN_QUESTIONS.md:544-568 (OQ-017 open; REAPER and ASIO-Guard behaviour reasoned about, not measured). Grepped worklogs/ and HANDOVER for REAPER, Bitwig, Ardour, Live, Logic, Cubase and Studio One: they appear only as analysis, with no recorded DAW session. JUCE 9.0.1 VST3 wrapper juce_audio_plugin_client_VST3.cpp:3727-3732 calls processBlockBypassed only when getBypassParameter()==nullptr, while Anabasis returns its 'bypass' parameter (e769f33:src/PluginProcessor.cpp:722-725). New runtime on :143, in the real JUCE Standalone window rather than the audit harness: preset menu → Loud Pop (Loudness 60%) → MATCH, 5 trials each way. Stepped motion: 0/5 misroutes (MATCH toggled, Loudness stayed 60%). Instant xdotool warp+click: 3/5 misroutes (MATCH unchanged, Loudness 60% to 0%).

**Corrections to the candidate claim.** The harness's 'hostbypass' emulates a path the JUCE VST3 wrapper does not take for this plugin. A VST3 host's bypass arrives as the 'bypass' parameter through processBlock, and hosts that stop calling process entirely are a third behaviour, so E07's frozen-display result may not describe any real host. The AU wrapper was not checked. The post-pop-up misroute (ST-01/E20) is not specific to the harness: it reproduces in the shipping JUCE Standalone window, but only with warp-then-press input. Whether touch, pen or VNC/RDP input produces that sequence is still unverified. I did not re-test ST-10/E21 (lingering pop-up pixels).

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 4 · severity 3 · discoverability 2 · efficiency 2 · coherence 2 · change risk 1 · complexity 3 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### TEST-003

**The owner has reported the GR history's newest edge three times (0.2.8, 0.2.11, 0.2.12), each fix recorded as verified. The frame-sequence validation behind those fixes lived outside the repository, and neither the real frame clock nor the real display is ever exercised**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | partially-confirmed | Test infrastructure | Verification stops at the headless boundary, and gesture/restore threading can crash the host | Phase 2 |

**Evidence**

- e769f33:CHANGELOG.md:429-433 — report 1: 'the newly drawn portion of the GR history … is jittery'. The fix's bullets are tagged [Verified] at :465-512.
- e769f33:CHANGELOG.md:335-347 — report 2. '0.2.8's own claim that its part B had halved the tip's movement was not reproduced on the real limiter'.
- e769f33:CHANGELOG.md:363-365 — the 0.2.11 fix delays the newest value by 21 ms (Simple) or 32 ms (Advanced). [Verified]
- e769f33:CHANGELOG.md:48-66 — report 3: '1–2 pixels of newly generated content extending to the right as a short horizontal line'.
- e769f33:worklogs/2026-09-05-gr-history-tip.md:30-40 — the frame-by-frame harness was built 'in the session scratch directory (not in the tree)'.
- e769f33:worklogs/2026-09-05-gr-history-tip.md:172-174 — presentation-pipeline hitches on the GL platforms are 'unmeasured in the field'.
- e769f33:src/gui/FrameClock.h:73-141 — onVBlank's EMA, snap confirmation, ≤126 Hz divider and 60 Hz fallback. It is private and has no test (state_tests.cpp:1433,1457 are comments only).
- e769f33:docs/procedures/TESTING.md:581-585 — a mutant that deletes repaint() survives, because the headless suite has no repaint region.
- e769f33:docs/procedures/TESTING.md:761-770 — GUI appearance and animation smoothness are Level 5 only.
- e769f33:docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md:64-66 — the host matrix (REAPER Windows plus Logic) has no GR-history motion item.
- e769f33:tests/state_tests.cpp:10484-10579 (grPaint), :6793-6809 (grPhase), :10154-10198 (grPair) — per-frame and pure-function pins exist, but no sequence-level motion assertion.

**Current behaviour.** The in-tree suites pin single-frame paint properties and pure scroll functions, driving tick(dt) by hand. The frame-sequence metrics that each of the three rounds measured, and that justified each [Verified] tag, came from a scratch harness that is no longer present: rigid-translation residual, revisions of drawn vertices, and lead-out changes. The real FrameClock pacing, the GL presentation and real host delivery are unexercised. The Level-5 checklist does not name the GR-history motion check.

**Problem.** On the display the owner has complained about most, a regression of any of the three fixed defects, or a FrameClock pacing change, cannot be caught by CI. The owner is the only detector. It happened three times in five days, and one [Verified] claim later proved not to hold on the real limiter.

**Root cause.** The validation instrument was built as disposable session tooling rather than as a test. FrameClock's pacing logic is welded to VBlankAttachment with no injectable timestamp seam. The Level-5 procedure lists 'animation smoothness' only generically.

**User impact.** This is indirect. Future work on the GR history, including [VIS-019](findings-visualisation.md#vis-019) and [VIS-020](findings-visualisation.md#vis-020), risks re-introducing visible jitter that ships green, and each owner-detected regression costs a full report-reproduce-fix round. *Scope:* GrHistoryView (tick, paintHistory, smoothedHead, visibleRight), FrameClock (shared with SpectrumView and LoudnessMeterView), the state suite, and the Level-5 release checklist.

**Proposed improvement.** (1) Move the frame-sequence harness into AnabasisStateTests as ordinary test cases. It drives the real processor and GrHistoryView through scripted delivery (steady, jittered σ 1-3 ms, short and long blocks, bursts) and a scripted frame clock (60/120/144 Hz with jitter), renders with the software renderer, and asserts the three owner-reported invariants as numbers: completed-trace translation residual = 0 px on a steady host, 0 revisions of drawn vertices, and nothing at or right of visibleRight changing except by scroll. It also pins the burst behaviour to OQ-017's current answer. Each historical defect must be re-introducible as a mutant that fails. (2) Give FrameClock a pure, timestamp-driven pacing function (the same code, called by onVBlank) and test the EMA, the confirmed fast-attack snap, the divider at 144/240 Hz, the 60 Hz fallback and the 50 ms clamp. (3) Add a named Level-5 item: GR-history motion checked on a real display at 60 Hz and ≥120 Hz in REAPER (anticipative FX on/off) and Logic, with a screen recording attached.

**Alternatives considered.**

- *Status quo (the owner as detector)* — This has already failed three times, and one [Verified] claim proved wrong.
- *Golden-image pixel tests* — Brittle across JUCE and renderer versions and font rasterisation. Invariant metrics (residuals, revision counts) are more robust and are what the rounds actually measured.
- *GL capture under xvfb in CI* — xvfb has no real vblank or GPU presentation, so it is unrepresentative. That belongs in Level 5, not CI.
- *Only the Level-5 checklist line* — Cheap, but it still leaves the owner as the only detector between releases.

**Decision: Proceed · P2.** The core claim holds: the evidence behind three [Verified] fixes cannot be re-run, and the real pacing and presentation path is unexercised. The instrument already existed, so landing it as tests is a bounded, low-risk, test-only change that directly protects the most-reported display and is a prerequisite for safely doing [VIS-019](findings-visualisation.md#vis-019) or [VIS-020](findings-visualisation.md#vis-020).

**Architecture gates.**

- Build System change (ARCHITECTURE_REVIEW_GATE: CMake structure) if the harness lands as a new CMake target. Adding cases to the existing AnabasisStateTests avoids it.
- ADR-0009 provenance: FrameClock is a verbatim Anamorph copy. Extracting its pacing into a testable function diverges from the sibling, which must be recorded in its provenance header (Anamorph stays read-only).
- ADR-0025: whatever remains untestable (GL presentation, real vblank, the deletion of repaint()) should carry the ADR's four disclosures rather than be left implicit.

**Dependencies.** [VIS-019](findings-visualisation.md#vis-019) (the burst assertion must encode OQ-017's eventual answer); [VIS-020](findings-visualisation.md#vis-020) (the same harness would verify that markers scroll rigidly)

**Acceptance criteria.**

- AnabasisStateTests contains a frame-sequence GR-history test that runs ≥ 8 s per configuration (Simple and Advanced wells, 44.1/48 kHz, 512-2048 blocks) and asserts: steady-host completed-trace residual = 0 px, 0 drawn-vertex revisions, and no change at or right of visibleRight other than translation.
- Re-introducing each historical defect as a mutant makes that test fail: the 0.2.7 bucket-rate stepping, the 0.2.10 live newest vertex, and the 0.2.11 visible lead-out.
- FrameClock's pacing is reachable through a pure function with tests covering the EMA settle, the confirmed-snap versus single-outlier behaviour, the divider at 144 and 240 Hz, the 60 Hz fallback and the 50 ms clamp. A mutant removing the snap confirmation fails.
- RELEASE_COMPATIBILITY_CHECKLIST.md names a GR-history motion check on a real display (60 Hz and ≥120 Hz) in REAPER (anticipative FX on/off) and Logic, with a recording attached to the release record.
- TESTING.md's 'what the suite cannot see' paragraph is updated to list exactly what remains Level-5-only.

<details><summary>Verification record</summary>

**Method.** I read the CHANGELOG entries at e769f33:CHANGELOG.md:429-433 (0.2.8, 'jittery'), :335-347 (0.2.11, including 'not reproduced on the real limiter'), :363-365 (the 21-32 ms delay) and :48-66 (0.2.12, '1–2 pixels'). I checked their [Verified] tags at :365, :369, :465-512 and :88-143. I read FrameClock::onVBlank (e769f33:src/gui/FrameClock.h:73-141): it is private and reached only through VBlankAttachment. I grepped the tests: FrameClock appears only in comments (e769f33:tests/state_tests.cpp:1433,1457), and onVBlank, emaDelta and kCapHz get 0 hits. The GR paint and phase pins exist (grPaint at state_tests.cpp:10484-10579, grPhase at :6793-6809, grPair at :10154-10198). I read TESTING.md :581-585 and :761-771, the worklog's harness description (worklogs/2026-09-05-gr-history-tip.md:26-40) and its residual list (:160-174), and the release host checklist (RELEASE_COMPATIBILITY_CHECKLIST.md:64-66).

**Corrections to the candidate claim.** (1) The claim that verification was 'headless only (paint pins and mutants)' understates it. Each round used a frame-by-frame harness that drove the REAL processor and the REAL GrHistoryView::tick/paintHistory under scripted host delivery (ideal, jittered and bursty) and a simulated 60 Hz clock. Frames were rendered with JUCE's software renderer and compared pixel-for-pixel against the previous painter (worklog :30-40). That harness, however, was built 'in the session scratch directory (not in the tree)' (:30). CI cannot run it, the next contributor cannot run it, and it never exercises the real FrameClock/VBlankAttachment, the GL renderer or real host delivery. The worklog itself lists presentation-pipeline hitches as 'unmeasured in the field' (:172-174). (2) The 21-32 ms delay is at CHANGELOG.md:363-365, not :349-351. (3) The finding is supported further by the fact that the 0.2.8 fix's own claim was later found 'not reproduced on the real limiter' (:345-346): its model tracked a completed bucket on an ideal host (worklog :28-29). That shows verification that was idealised rather than merely headless.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 3 · severity 3 · discoverability 2 · efficiency 3 · coherence 3 · change risk 1 · complexity 3 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### TEST-004

**The STATISTICS panel's display rules (TP and SP warn predicates, the '-' sentinel formatting with a unit suffix, bar mapping) live inline in LoudnessMeterView::paint and are pinned by no test. The SP tolerance fix shipped without one**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Test infrastructure | Verification stops at the headless boundary, and gesture/restore threading can crash the host | Phase 0 |

**Evidence**

- e769f33:tests/state_tests.cpp:1428-1491 — the only LoudnessMeterView assertions, on three pure statics; the comments at :1430-1433 and :1455-1457 say the tick is deliberately not driven
- e769f33:src/gui/LoudnessMeterView.cpp:158 — fmt returns '-' for v <= -99; :266 and :268 then append ' dBFS', giving '- dBFS' ([VIS-012](findings-visualisation.md#vis-012))
- e769f33:src/gui/LoudnessMeterView.cpp:243-244 — TP warn is an exact 'shownTp > shownCeiling' ([DSP-001](findings-dsp-tech.md#dsp-001))
- e769f33:src/gui/LoudnessMeterView.cpp:265-267 — SP warn with kCeilingWarnSlackDb = 0.005, inline in paint
- e769f33:src/gui/LoudnessMeterView.cpp:166-196 — M/S/I bar mapping (-36..0 LUFS) and fill, untested
- e769f33:src/gui/LoudnessMeterView.h:89-97 — paint is public; tick and FrameClock are private, so the shown* members cannot be set from a test
- e769f33:src/gui/FrameClock.h:73-140 — onVBlank EMA, snap and fallback; private, never fires headlessly
- e769f33:docs/architecture/design-decisions/ADR-0020-waveform-statistics-panel.md:241-243 — 'no headless test drives the panel's tick … The SP tolerance is likewise a paint() threshold with no headless driver'
- e769f33:docs/architecture/design-decisions/ADR-0015-pre-ship-contract-refreeze.md:222-229 — the TP warn meaning is an open fine-review question
- e769f33:tests/state_tests.cpp:9905-9925 (GrMiniMeter snapshot) and :10484-10571 (GR paint snapshots) — a render-and-inspect precedent exists
- screenshot session capture `rt/visuals/08b-silence-3s-editor.png` — 'RMS - dBFS' and TP in the warn colour on screen

**Current behaviour.** Every rule that decides what the STATISTICS panel prints and colours is an inline expression in paint(): the TP warn (exact), the SP warn (0.005 dB slack), the sentinel '-' with the unit still appended, the LRA and PLR blanks, and the bar fill mapping. The shown* snapshot can only be set through a private FrameClock tick, which never fires in the suite. Three helper rules (PLR derivation, RMS cadence, RMS reference) are pinned as statics.

**Problem.** The panel's warn colour is the product's over-ceiling signal, visible in Simple view in every session. A refactor that dropped the SP slack, flipped a comparison or broke the sentinel test would pass the suite. The audit's own display fixes ([DSP-001](findings-dsp-tech.md#dsp-001) TP comparand or tolerance, [VIS-012](findings-visualisation.md#vis-012) '- dBFS') have nowhere to put the regression test that TESTING_POLICY rule 1 requires.

**Root cause.** The statics pattern (plrFromShown) was applied only to the rules that came with a correctness argument at the time. The warn predicates and formatter stayed as paint() locals, and the tick is private, so neither a rule test nor a snapshot test can reach them.

**User impact.** A regression here could make the panel mislead a mastering decision. A silent SP warn would hide a genuine clamp exceedance; a spurious one would make the user chase a non-existent over. Nothing is wrong today beyond the separately reported [DSP-001](findings-dsp-tech.md#dsp-001) and [VIS-012](findings-visualisation.md#vis-012). *Scope:* src/gui/LoudnessMeterView.{h,cpp} and tests/state_tests.cpp. FrameClock.h is optional and separate.

**Proposed improvement.** Extract the panel's decisions into pure statics that paint() calls as its single source, as plrFromShown already is: for example tpWarns(tp, ceiling), spWarns(peak, ceiling), formatReading(v, dp, unit) (which owns the blank form, so [VIS-012](findings-visualisation.md#vis-012)'s fix lands in one place) and barFraction(lufs). Pin each one with boundary tests: SP at ceiling+0.004, ceiling+0.006, and -0.09999997 against -0.1; TP per whatever comparand the ADR-0015 fine review settles; the sentinel and LRA/PLR blanks; the -36 and 0 LUFS bar ends and clamping. Land these tests with [DSP-001](findings-dsp-tech.md#dsp-001) and [VIS-012](findings-visualisation.md#vis-012) so each fix has a test that fails before it. Optionally add one colour snapshot through a test-only tick hook. Leave FrameClock pacing tests as a separate P3 item: feed synthetic timestamps to an extracted pacing function.

**Alternatives considered.**

- *Pixel snapshot of the whole panel through a test tick hook* — Feasible (the GrMiniMeter precedent), but the text rendering is font-dependent. Colour sampling of the warn rows works as a complement, not as the primary pin.
- *Extract statics and pin the rules (chosen)* — Exact, platform-independent, matches the existing plrFromShown and shouldAdoptRms pattern, and leaves one definition of each rule.
- *Also test FrameClock EMA and fallback now* — Lower value, since it affects cadence rather than values. Defer to P3 unless a pacing change is proposed.
- *Leave as is (disclosed in ADR-0020)* — Rejected. The disclosure predates ADR-0025, which excludes headlessly reachable code, and pending fixes to these exact rules need a failing-before test.

**Decision: Modify · P2.** Confirmed and material: the panel is the primary readout, and its warn semantics are about to change. The constrained change (statics plus rule tests, landed with [DSP-001](findings-dsp-tech.md#dsp-001) and [VIS-012](findings-visualisation.md#vis-012)) gets most of the protection. It avoids brittle full-panel snapshots and does not spend effort on FrameClock pacing, which does not affect what the user reads.

**Dependencies.** [DSP-001](findings-dsp-tech.md#dsp-001) (TP warn comparand/tolerance; its test lands here); [VIS-012](findings-visualisation.md#vis-012) ('- dBFS' sentinel formatting; its test lands here); ADR-0015 §Consequences open fine-review question on the TP warn meaning (decides what tpWarns asserts); [TEST-001](findings-doc-test.md#test-001) (shared headless-reach approach if a tick hook is added)

**Acceptance criteria.**

- LoudnessMeterView exposes the static warn predicates and the reading formatter, and paint() computes its colours and text only through them (no inline copies)
- A test asserts spWarns is false at ceiling+0.004 and for -0.09999997 against a -0.1 ceiling, and true at ceiling+0.006
- A test asserts tpWarns matches the comparand recorded by the ADR-0015 fine review (exact tp > ceiling until that decision changes it)
- A test asserts the sentinel reading (<= -99) prints the agreed blank form for the TP, SP and RMS rows (no stray unit if [VIS-012](findings-visualisation.md#vis-012) is adopted), LRA < 0 prints '-', and PLR prints '-' when integrated <= -99
- A test asserts barFraction(-36) == 0, barFraction(0) == 1, and clamping outside that range
- Dropping the SP slack, or flipping either warn comparison, fails a named check (mutation-verified)
- ADR-0020:241-243's 'no headless driver' sentence is updated to cite the new tests

<details><summary>Verification record</summary>

**Method.** Read e769f33:tests/state_tests.cpp:1428-1491: only plrFromShown, shouldAdoptRms and rmsWithReference are asserted, and no test references kCeilingWarnSlackDb or FrameClock. Read LoudnessMeterView.cpp:95-281 (tick and paint) and LoudnessMeterView.h:42-125 (tick is private and fed by a private FrameClock; the shown* members are private). Read FrameClock.h:60-150 (onVBlank is private). Checked the snapshot precedent: GrMiniMeter at state_tests.cpp:9905-9925 and the GR paint at :10484-10571. Read the ADR-0020 2026-08-07 amendment and the ADR-0015 open question. Viewed session capture `rt/visuals/08b-silence-3s-editor.png`: 'RMS  - dBFS' and TP 1.35 dBTP in the warn colour, which confirms the untested rules are the ones on screen.

**Corrections to the candidate claim.** No factual errors in the claim. Additions: ADR-0020:241-243 already discloses that 'the SP tolerance is likewise a paint() threshold with no headless driver', so the SP warn tolerance, a bug fix, shipped without a test that fails on the old code. That predates ADR-0025, and it is not an ADR-0025 class, since paint is headlessly reachable (the GrMiniMeter precedent). The observers' code references for the '- dBFS' slip (LoudnessMeterView.cpp:284-285, :395 in the visuals observer's V-10 notes) do not exist at e769f33, where the file is 281 lines. The correct anchors are :158 (fmt) and :266-268 (the SP and RMS rows append ' dBFS'). The FrameClock part is accurate but lower value: it governs display cadence, not what is read.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 3 · severity 3 · discoverability 2 · efficiency 2 · coherence 3 · change risk 1 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### TEST-005

**No ThreadSanitizer lane in CI: a TSAN build finds the KI-008 cycle today, and none of the ~20 two-thread tests reproduces the KI-003 restore race**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | confirmed | Test infrastructure | Verification stops at the headless boundary, and gesture/restore threading can crash the host | Phase 3 |

**Evidence**

- e769f33:.github/workflows/build.yml:1048-1136 — the sanitizers job runs ASan+UBSan (-fsanitize=address,undefined,...) and valgrind memcheck only
- e769f33:.github/workflows/build.yml:1459-1463 — RTSan cannot share a binary with `address,undefined` or `thread` (so a TSAN lane needs its own job)
- e769f33:docs/architecture/design-decisions/ADR-0039-spectrum-frame-publication.md:436-439 — 'RACE-FREEDOM IS ARGUED, NOT DETECTED ... the repository runs no ThreadSanitizer, helgrind or DRD lane'
- e769f33:docs/KNOWN_ISSUES.md:680-684 — KI-008: 'the finding is the ThreadSanitizer lock-order-inversion report, not a suite failure — the suite passes. Reproduce with a -fsanitize=thread build'
- e769f33:docs/policies/TESTING_POLICY.md (rule 1 via ADR-0025: every fix ships a failing-then-passing test; ADR-0025 excludes state and threading from its exception)
- e769f33:tests/state_tests.cpp — 20 std::thread occurrences, none of which call setStateInformation off-thread against editor-tick reads (KI-003 tests model the restore single-threaded, per KNOWN_ISSUES.md:189-195)
- runtime V24-TSAN-1: rt/verify-24/tsan-e769.log / tsan-e769.err (PASS 1423, 23 lock-order-inversion, 0 data races, exit 66, 141 s)
- runtime V24-RACE-1: .../rt/verify-24/probe/r1.err..r3.err — with a two-thread restore stimulus, TSAN reports 15-17 data races (livePresetName, liveSelection, liveDetachMask)
- *Merged at triage from another verifier's note, quoted as written:* Correct the detection credit, which also affects how the lane is designed. KNOWN_ISSUES.md:683 and DOCUMENTATION_COVERAGE.md:2929 credit testTheFrozenLatchNeedsNoThreadCrossing as the KI-008 stimulus. At e769f33, all 23 TSAN lock-order-inversion reports in rt/verify-24/tsan-e769.err come from single-threaded tests. Re-checked: the stacks name testTeardownAndReengageInvariants, testANoOpPresetApplyDoesNotEatTheOldestUndoStep, testUndoRequestsDuck and at least 10 others, and none names the credited test. TSAN's deadlock detector needs only the lock-acquisition order, so a TSAN lane over the existing state suite catches KI-008 without any new stimulus. KI-003's data races still need the two-thread restore stimulus [TEST-005](findings-doc-test.md#test-005) proposes. Fix both doc credits in the same change.

**Current behaviour.** CI's sanitizers are ASan, UBSan and valgrind memcheck (plus RTSan on the DSP suite). None of them detects data races or lock-order cycles. The only race detection the project has used is ad-hoc local TSAN builds recorded in KI-008 and HANDOVER.

**Problem.** The project's open threading defects (KI-008, KI-003) and the spectrum/GR seqlock contracts (ADR-0039) can only be detected by a race detector, and none runs in CI. A regression in gesture or restore ordering, or a new editor poll of a restore-written member, passes CI. TESTING_POLICY rule 1 cannot be satisfied for a KI-008 fix without such a lane.

**Root cause.** The sanitizer matrix (ADR-0034) was built around ASan/UBSan/valgrind. TSAN is used only manually. The suites' threaded tests were written as functional stimuli, and KI-003 is modelled single-threaded.

**User impact.** Indirect. It is the missing safety net for exactly the defects that can crash a host ([TECH-003](findings-dsp-tech.md#tech-003)). UI or state work near gestures, presets and restore gets no automated race feedback. *Scope:* CI (Linux) for both suites, and every future change touching gestures, restore, presets or editor polls.

**Proposed improvement.** Add a `tsan` job on the pinned Clang with its own build directory (as for RTSan) that runs AnabasisStateTests (and AnabasisTests if cheap) with -fsanitize=thread and fails on any report. Per TESTING_POLICY rule 5, a liveness canary with a deliberate race must produce 'WARNING: ThreadSanitizer' before the lane's silence is trusted. Add two-thread stimuli for the premises the KIs describe: an off-thread setStateInformation against the editor-tick reads, and a gesture begin/end against an off-thread restore. Sequencing: either land after the KI-008 edge is removed ([TECH-003](findings-dsp-tech.md#tech-003) step 1), or land with the deadlock detector disabled (TSAN_OPTIONS=detect_deadlocks=0). The second option must be recorded as a visible scoped-out category in KNOWN_ISSUES KI-008 and TESTING_POLICY rule 4, and re-enabled when KI-008 closes.

**Alternatives considered.**

- *Keep TSAN as a documented local procedure only* — Cheap, but it does not gate. The two-thread defects stay invisible to CI, which is how KI-008 stayed open.
- *helgrind/DRD instead of TSAN* — Rejected. ADR-0039 measured helgrind false positives on textbook std::atomic seqlocks; it does not model the C++ memory model.
- *TSAN with a suppression entry for the KI-008 cycle* — Works, but runs against the repository's stated preference not to use sanitizer suppressions (ADR-0029, on RTSan). The scoped detect_deadlocks=0 option disclosed via KI and TESTING_POLICY is the more transparent form of the same compromise.

**Decision: Proceed · P2.** The evidence is complete and measured: no lane exists, a TSAN build at e769f33 is clean of data races but shows the KI-008 cycle, and a two-thread stimulus immediately exposes KI-003. The lane is the only regression instrument for [TECH-003](findings-dsp-tech.md#tech-003). Adding it is low risk and moderate cost (~2.5 min of test time on 4 cores plus the build), provided the sequencing with KI-008 is handled openly.

**Architecture gates.**

- Possibly 'Build System change' (ARCHITECTURE_REVIEW_GATE lists CMake structure, pins and the dependency set; a new CI job on the already-pinned Clang with no CMake change is arguable). Record it as an ADR-0034 sanitizer-set amendment either way
- No conflict with an Accepted ADR: ADR-0029 option D only says RTSan cannot share a binary with `thread`

**Dependencies.** [TECH-003](findings-dsp-tech.md#tech-003)

**Acceptance criteria.**

- build.yml has a TSAN job that builds and runs AnabasisStateTests under -fsanitize=thread and fails the build on any ThreadSanitizer report (exit 66 not masked, no continue-on-error).
- A liveness canary with a deliberate data race runs first in the same job, and the step fails unless it reports 'WARNING: ThreadSanitizer'.
- The state suite contains a two-thread off-message-thread setStateInformation stimulus against the editor-tick reads. Under the lane it fails at e769f33's behaviour and passes once KI-003's fix lands (or is disclosed as a known-failing category per TESTING_POLICY rule 4).
- KI-008 is either fixed (0 lock-order-inversion reports) or the deadlock detector is explicitly scoped out, with the scope recorded in KNOWN_ISSUES and TESTING_POLICY.
- Reinstating the documented round-40 prepareToPlay write (the mutant cited in DOCUMENTATION_COVERAGE:2929-2934) turns the lane red.

<details><summary>Verification record</summary>

**Method.** I read the sanitizers job (e769f33:.github/workflows/build.yml:1048-1136) and the realtime job (:1453-1615), and grepped every workflow and script for thread sanitizer use: none. I read ADR-0039:436-439, ADR-0029 option D, KI-008's evidence line and DOCUMENTATION_COVERAGE:2921-2934. I counted std::thread in the suites (state 20, DSP 0). I built and ran the state suite under -fsanitize=thread at e769f33 and wrote a two-thread restore stimulus (see [TECH-003](findings-dsp-tech.md#tech-003)).

**Corrections to the candidate claim.** (a) The realtime job comment (:1459-1463) does not exclude TSAN as a policy choice. It records that the Clang driver rejects combining realtime with `thread`, so a TSAN lane, like RTSan, needs its own build and job. (b) Measured at e769f33, a TSAN lane would be red today: 1423 checks pass and 0 data races are reported, but there are 23 lock-order-inversion reports and exit code 66, taking 141 s with gcc 13 on 4 cores (the clang-22 cost is unmeasured). (c) The cycle is reported from 15 single-threaded tests (e.g. testUndoRequestsDuck, testTeardownAndReengageInvariants). TSAN's deadlock detector does not need the two-thread stimulus that KI-008 and DOCUMENTATION_COVERAGE credit. (d) The existing ~20 std::thread stimuli do not model an off-message-thread setStateInformation against the editor reads. A TSAN lane alone would therefore NOT see the KI-003 races, which TSAN flags 15-17 times as soon as such a stimulus exists.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 2 · severity 3 · discoverability 3 · efficiency 2 · coherence 3 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### TEST-006

**The wrapper's own per-block code (meter publish, lazy loudness histogram walks, snapshot build, mono copy) sits outside RTSan, the allocation guard and the static lint's reach**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P3** | high | partially-confirmed | Test infrastructure | Verification stops at the headless boundary, and gesture/restore threading can crash the host | Phase 5 |

**Evidence**

- e769f33:src/PluginProcessor.cpp:917-1052 — processBlock: meter-reset consume :925-930, cached.toEngine :934, buffer.clear/copyFrom :940-952, engine.process :963, meter publish :975-1021 incl. om.integratedLufs() :996 and om.lraLu() :1014
- e769f33:src/dsp/LoudnessMeter.h:213-248 — integratedLufs()/lraLu() recompute the histogram lazily (computeIntegratedLufs/computeLraLu) in the caller's context
- e769f33:src/dsp/AnabasisEngine.cpp:332 — ANABASIS_NONBLOCKING only on AnabasisEngine::process; e769f33:src/dsp/AnabasisEngine.h:461-475 spectrum taps pushed inside the engine; e769f33:src/PluginProcessor.cpp:44 GR sink attached to the engine
- e769f33:tests/dsp_tests.cpp:6150-6312 — the allocation guard is armed around engine.process only, with no GR sink; e769f33:tests/dsp_tests.cpp:4244 — DSP tests that attach the GR sink run under RTSan
- e769f33:.github/workflows/build.yml:1476-1480,1589-1610 and e769f33:docs/architecture/design-decisions/ADR-0029-realtime-enforcement-strategy.md:110-112 — 'Only AnabasisTests runs under RTSan'
- e769f33:scripts/check-realtime.py:63-106,684-700 — name-scoped, same-file transitive walk; measured reach: PluginProcessor.cpp -> {processBlock, publishSilentMeters}; LoudnessMeter.h -> {processFrame..., reset..., resetIntegrated}, not integratedLufs/lraLu (rt/verify-24/lintscope.py)
- runtime V24-ALLOC-1: .../rt/verify-24/probe/wrapper_alloc_probe.cpp, alloc.log, gdb-alloc1..4.log — 0/0 across 6 configurations x 400 armed processBlock calls; the first-window malloc is attributed to start-up threads (it vanishes with SETTLE=400 in 5 of 5 runs)
- e769f33:docs/architecture/REALTIME_SAFETY_AUDIT.md:87-90 — still says the first green RTSan run is 'owed'; the realtime job passed at e769f33 (run 36039432935, job 107767666158)

**Current behaviour.** The engine's process() is under all three ADR-0029 tiers. The wrapper code around it, which runs every block in every host and feeds every meter and statistic, is checked only by the text lint. The heaviest wrapper work (the integrated-loudness and LRA histogram walks) is outside even that. It is allocation-free today, as measured.

**Problem.** A future edit to the wrapper's per-block code passes all three tiers even if it violates the Priority-1 realtime policy: a juce::String in a publish, a lock or allocation in CachedParams::toEngine, or an allocating change in LoudnessMeter's lazy recompute. The failure would surface only as host dropouts. ADR-0029's 'three complementary tiers' claim reads as covering the audio path, but it covers the engine only.

**Root cause.** The runtime tiers were deliberately scoped to the JUCE-free engine entry point (ADR-0029 §2-3). The 0.2.12 move of the GR push into the engine left the meter publish and the lazy loudness readings behind in the wrapper. The lint's same-file walk cannot follow calls into other translation units or headers.

**User impact.** None today. The risk is future: a regression would show up as clicks or dropouts in the meter-driven path in users' hosts, not in CI. *Scope:* AnabasisAudioProcessor::processBlock and its cross-file callees, on every host and platform.

**Proposed improvement.** Add a state-suite test that includes tests/AllocationGuard.h and arms it around AnabasisAudioProcessor::processBlock across the three supported bus layouts, a pending meter reset, and an open editor. It must let JUCE's background threads settle first, or give the guard a per-thread filter, because the guard counts process-wide. This is test-only and reaches MSVC too. Extend check-realtime.py's scope, by name or by a cross-file walk, to CachedParams::toEngine and LoudnessMeter::integratedLufs/lraLu/computeIntegratedLufs/computeLraLu, with self-test cases. Optionally wrap the call in __rtsan_realtime_enter/exit in an RTSan-built state-suite subset. Update the REALTIME_SAFETY_AUDIT 'owed' line.

**Alternatives considered.**

- *Leave as-is (the lint scans processBlock)* — Rejected. The lint demonstrably does not reach the cross-file callees that do the real work, and no runtime tier sees the wrapper.
- *Run the whole state suite under RTSan in CI* — Insufficient on its own, because processBlock is not in a nonblocking scope. It also amends ADR-0029 §3's explicit decision. Useful only together with a scoped realtime-enter in the test.
- *Annotate processBlock with ANABASIS_NONBLOCKING* — Would place the wrapper under RTSan in any suite that runs it. Its interaction with JUCE's virtual base, and whether -Wfunction-effects then warns on JUCE callees, is unverified. It needs a measurement before it is proposed as the fix.

**Decision: Proceed · P3.** The gap is real and code-confirmed, although narrower than claimed: the spectrum and GR pushes are covered. There is no present violation, so this is regression prevention, not a defect. The allocation-guard test is test-only, cheap and cross-platform. It is worth doing, at low priority.

**Architecture gates.**

- None for the allocation-guard test or the lint-scope extension (test/lint only; no CMake or CI structure change)
- ADR-0029 §3 ('Only AnabasisTests runs under RTSan'): amendment needed only if RTSan is extended to the state suite in CI

**Dependencies.** None.

**Acceptance criteria.**

- A state-suite test arms the allocation guard around AnabasisAudioProcessor::processBlock for stereo->stereo, mono->stereo and mono->mono, with a pending meter reset and with an editor alive, and asserts new==0 and malloc==0 (after a settle, or with thread-scoped counting). It is disclosed and skipped where the guard is compiled out.
- A mutant that constructs a juce::String (or calls juce::String::formatted) in processBlock's meter publish makes that test fail.
- check-realtime.py's reachable set includes CachedParams::toEngine and LoudnessMeter::integratedLufs/lraLu (and their compute* helpers), proven by a self-test case that fails without the extension.
- REALTIME_SAFETY_AUDIT.md no longer calls the first green RTSan run 'owed', and states that the wrapper's processBlock is covered by the new test.

<details><summary>Verification record</summary>

**Method.** I read processBlock (e769f33:src/PluginProcessor.cpp:917-1052), the engine sink and spectrum-tap sites, testTheAudioPathAllocatesNothing (dsp_tests.cpp:6130-6312), ADR-0029 §2-3, and the realtime CI job. I ran check-realtime.py's own reachable_bodies() over PluginProcessor.cpp and LoudnessMeter.h to see exactly what the lint scans. I built a scratch probe that arms tests/AllocationGuard.h around the REAL AnabasisAudioProcessor::processBlock (6 configurations x 400 calls), attributed stray counts with a conditional gdb breakpoint, and repeated with a settle delay.

**Corrections to the candidate claim.** (a) 'Spectrum and GR pushes' are NOT wrapper code at e769f33. The spectrum taps are pushed inside AnabasisEngine::process (AnabasisEngine.h:461-475), and since 0.2.12 so is the GR history (setGrHistorySink, PluginProcessor.cpp:44). Both are therefore under RTSan via the DSP tests that attach a sink (dsp_tests.cpp:4244). The GR sink is not under the allocation guard, whose matrix attaches none. (b) The wrapper-only code outside every runtime detector is: the meter-reset consume (:925-930), CachedParams::toEngine (PluginParameters.cpp:489), the InternalState reads, buffer.clear/copyFrom (:940-952), and the meter publish including LoudnessMeter::integratedLufs()/lraLu(). Their lazy histogram walks (LoudnessMeter.h:213-248) have processBlock (:996, :1014) as their only production caller. The static lint scans only processBlock and publishSilentMeters in that file, and does not reach toEngine, integratedLufs/computeIntegratedLufs or lraLu/computeLraLu. (c) There is no present violation. With the guard armed around the real processBlock I measured 0 new and 0 malloc over 2400 calls: stereo, mono->stereo, mono->mono, meter-reset requests, editor alive, 64-sample blocks. A single malloc appeared only in the very first armed window of a fresh process and vanished with a 400 ms settle in 5 of 5 runs. It comes from JUCE start-up threads, because the guard counts process-wide. (d) Adding RTSan to the state suite would not by itself cover processBlock, because RTSan checks only inside the ANABASIS_NONBLOCKING scope on AnabasisEngine::process.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 1 · severity 3 · discoverability 3 · efficiency 1 · coherence 3 · change risk 1 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### TEST-007

**Stale Windows editor-hosting comments in run-pluginval.ps1, channel_probe.cpp and TESTING_POLICY rule 4 contradict CI, which opens the editor on windows-latest**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | partially-confirmed | Documentation | Verification stops at the headless boundary, and gesture/restore threading can crash the host | Phase 5 |

**Evidence**

- e769f33:scripts/run-pluginval.ps1:109-114 — 'It is deliberately NOT inherited now: Anabasis has no editor ... If P5 reproduces the failure, add the flag THEN'
- e769f33:tools/channel_probe.cpp:305-310 — 'The Windows CI runner cannot host a plugin editor at all (KI-007)'
- e769f33:docs/KNOWN_ISSUES.md:452 — '### KI-007 — Preset/Freeze bookkeeping edges the fine review must settle together' (not a runner limitation)
- e769f33:docs/policies/TESTING_POLICY.md:208-214 — rule 4: 'the likeliest is the GPU-less windows-latest runner being unable to host the editor once P5 exists'
- CI run 36039432935 (e769f33) windows job 107767666061 log, saved at rt/verify-24/win_job.log: line 37 'macro: Loudness 85 + editor' [ok]; lines 82-86, 148-149 (and each later pass) 'Editor', 'Open editor whilst processing', 'Editor Automation' completed; the step concluded success
- pluginval develop Source/tests/BasicTests.cpp (saved at .../rt/verify-24/pv_basic.cpp:121-134) — the Editor test does ut.expect (editor != nullptr, 'Unable to create editor')

**Current behaviour.** Windows CI runs the full pluginval set with the editor, and it passes. Three repository texts still describe a pre-P5 or sibling world: 'Anabasis has no editor', 'the Windows CI runner cannot host a plugin editor at all (KI-007)', and a policy example predicting that the runner will fail once P5 exists.

**Problem.** Contradictory maintainer-facing statements about what the Windows gate verifies. One of them cites the wrong KI number. A reader auditing coverage could conclude that Windows editor coverage is missing, or add a skip flag that is not needed.

**Root cause.** The comments were written before P5 or carried over from Anamorph (whose KI-007 is the runner limitation), and were not reconciled once the editor landed and the Windows job went green with it.

**User impact.** None on users. There is minor maintainer confusion about CI coverage on the platform most users run. *Scope:* Three text sites (a script comment, a tool comment, and one policy example); no behaviour.

**Proposed improvement.** Rewrite the run-pluginval.ps1 note to state that the editor exists and that windows-latest hosts it under the full test set (cite the CI run). Keep the 'no skip without a KI' rule. In channel_probe.cpp, drop the 'cannot host ... (KI-007)' claim and say the editor is best-effort for display-less environments. Update the TESTING_POLICY rule 4 example so it no longer predicts a failure that did not occur. Optionally, have the probe print an explicit 'editor hosted' line so the log is checkable at a glance.

**Alternatives considered.**

- *Add --skip-gui-tests on Windows as the sibling does* — Rejected. CI proves the editor tests run and pass on windows-latest; skipping would remove real coverage.
- *Leave the comments* — Low harm, but they mis-cite a KI and contradict the policy's own evidence rule. The fix is trivial.

**Decision: Modify · P3.** The CI evidence shows the Windows editor is exercised, so no CI change is warranted. Only the three stale texts need reconciling. This is a smaller change than the finding's framing implies.

**Dependencies.** None.

**Acceptance criteria.**

- The repository no longer contains 'Anabasis has no editor' in scripts/run-pluginval.ps1, nor 'cannot host a plugin editor at all (KI-007)' in tools/channel_probe.cpp.
- TESTING_POLICY rule 4 keeps its skip-requires-a-KI rule, and its example no longer predicts a Windows editor failure after P5.
- The next Windows CI log still shows 'Editor', 'Open editor whilst processing' and 'Editor Automation' completing in both pluginval modes, and the probe's macro scenario hosts an editor.

<details><summary>Verification record</summary>

**Method.** I read e769f33:scripts/run-pluginval.ps1:96-114, e769f33:tools/channel_probe.cpp:295-320 and e769f33:docs/policies/TESTING_POLICY.md:208-214, and listed the KNOWN_ISSUES headings. I then checked the actual Windows CI job at e769f33 (run 36039432935, job 107767666061) via the GitHub API log, and pluginval's Editor-test source (develop) for its failure condition.

**Corrections to the candidate claim.** The comment contradictions are confirmed. The impact hypothesis ('editor tests may pass vacuously on Windows') is REFUTED by the CI log. At e769f33 the Windows channel probe printed '[ ok ] 48000 Hz / 512 macro: Loudness 85 + editor', and the '(no editor in this environment ...)' fallback line is absent. pluginval at strictness 10, with no skip flags, ran 'Editor' (~0.59 s; it expects a non-null editor), 'Open editor whilst processing' and 'Editor Automation' (~11 s) in every pass of both modes, and the job passed. The Windows runner therefore does host Anabasis's editor. What remains is comment drift. channel_probe also cites 'KI-007' for a Windows runner limit, but Anabasis's KI-007 is 'Preset/Freeze bookkeeping edges'; the citation is carried over from Anamorph's numbering.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 1 · severity 1 · discoverability 2 · efficiency 1 · coherence 3 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### TEST-008

**The combo drop-down fit test prints its pixel margin instead of asserting a floor, but the user-protecting 'fits' requirement (>= 0) is still asserted. The downgrade is deliberate and measured, and only its 'restore' TODO is stale**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Preserve** | **none** | high | partially-confirmed | Test infrastructure | Verification stops at the headless boundary, and gesture/restore threading can crash the host | — |

**Evidence**

- e769f33:tests/state_tests.cpp:4204-4205 — check (tightest >= 0, 'every combo's widest row still fits inside the control that opens it')
- e769f33:tests/state_tests.cpp:4206-4226 — deliberate downgrade from >= 24, the cross-platform font rationale, the advisory printf and 'Restore the assertion the moment the figure is confirmed on all three runners'
- e769f33:tests/state_tests.cpp:4228-4275 — font-independent identity (ideal width = text + chrome), mutation-verified
- e769f33:tests/state_tests.cpp:4245-4251 — disclosed surviving mutant: drawing that spends more than chrome is invisible to both width tests
- e769f33:src/gui/LookAndFeel.cpp:381-390 — withMinimumWidth(box.getWidth()); :392-397 and :432-445 — the measurement and the drawing read the same menuMetrics
- e769f33:src/gui/PluginEditor.cpp:1453-1473 — Settings combos laid out unconditionally (integratedBox, rmsRefBox at :1469-1470)
- local run of AnabasisStateTests (rt/verify-30/state.log): 'comboFit: tightest combo margin 31 px'
- saved CI logs job_abd209e3.log, job_ee32738d.log, job_f7fea2a7.log (Image: macos-26-arm64): 'comboFit: tightest combo margin 26 px' for both slices
- *Merged at triage from another verifier's note, quoted as written:* [TEST-008](findings-doc-test.md#test-008) already names this disclosed surviving mutant (e769f33:tests/state_tests.cpp:4245-4251, re-checked) and asks that it be treated separately. Record it as an optional P3 sub-item of [TEST-008](findings-doc-test.md#test-008): a render-and-measure test that draws one popup row into an Image and measures the text extent against the row. A drawPopupMenuItem that spends more than menuMetrics::chrome would then fail, which today's constant-reconstructing width tests cannot catch.

**Current behaviour.** The test walks every laid-out combo in Advanced view at 940x900 (at least 10 found). It asserts that each combo's widest menu row is no wider than the control, and it asserts the width arithmetic identity. It prints the tightest margin: 31 px on Linux and 26 px on macOS arm64. The Windows figure was not read.

**Problem.** The only concrete issue is hygiene. The 'restore when confirmed on all three runners' TODO has not been acted on, even though every run prints the figure. No user-facing risk follows from the downgrade.

**Root cause.** A fixed pixel floor was a snapshot of one machine's fonts. The authors replaced it with the platform-independent requirement plus an advisory print, and never came back to harvest the three runners' figures.

**User impact.** None found. The requirement that protects users (no menu row wider than its control) is still gated. *Scope:* tests/state_tests.cpp testEveryComboMenuFitsItsControl only.

**Proposed improvement.** Keep the >= 0 gate and the arithmetic identity as they are. Optional housekeeping (P3): read the Windows job's printed figure, then either restore a floor no higher than the minimum measured on the three runners (currently at most 26 px, before Windows), or rewrite the TODO to record the measured figures and say that the floor is intentionally advisory. Treat the disclosed drawPopupMenuItem-versus-chrome gap (a rendered-row measurement) as a separate item.

**Alternatives considered.**

- *Restore '>= 24' as originally written* — Rejected. macOS arm64 already reads 26 px, so a 2 px change in a font would redden the suite with no defect, which is the failure the downgrade was designed to avoid.
- *Per-platform floors from harvested figures* — Possible, but this asserts typography rather than the user requirement. Low value.
- *Render-a-row test to kill the surviving drawing mutant* — A real gap, disclosed at :4245-4251, but a different finding from this one. Worth a separate P3 item.
- *Keep as is (chosen)* — The user-protecting assertion is intact and deliberately platform-independent.

**Decision: Preserve · none.** The claim that the test was weakened is technically true, but the weakening removed only an early-warning margin, not the fit requirement. The removal is justified by measured cross-platform variance (31 px against 26 px). Reinstating a fixed floor would make the gate fail without a defect.

**Dependencies.** [VIS-014](findings-visualisation.md#vis-014) (only if it changes Settings combo widths or item text; the >= 0 gate would then be the relevant guard)

**Acceptance criteria.**

- testEveryComboMenuFitsItsControl still asserts tightest >= 0 and the 'ideal width = text + chrome' identity
- The advisory margin print remains on every run
- If a pixel floor is ever restored, it is no higher than the minimum of the Linux, macOS and Windows printed figures (Linux 31 px and macOS arm64 26 px measured), and the TODO comment records those figures

<details><summary>Verification record</summary>

**Method.** Read e769f33:tests/state_tests.cpp:4147-4275: the header, the >= 0 check at :4204-4205, the rationale and advisory print at :4206-4226, and the font-independent identity at :4228-4275. Read LookAndFeel.cpp:381-390 (menu minimum width = box width) and :392-445 (measurement and drawing share menuMetrics::chrome). Read PluginEditor.cpp:1453-1473: the Settings combos, including the Integrated and RMS Reference boxes, are laid out unconditionally, so the walk measures them. Ran the local AnabasisStateTests binary (built 2026-09-08, after the last src/tests change; 1423 checks, 0 failures): 'comboFit: tightest combo margin 31 px'. Read saved macOS-26-arm64 CI job logs (abd209e3, ee32738d, f7fea2a7): 26 px on both slices.

**Corrections to the candidate claim.** The claimed behaviour ('the test asserts that every combo menu fits its control') is still true: check(tightest >= 0) at :4204. Only the >= 24 tripwire margin was removed. The user-impact hypothesis is refuted. A metric change that eats the margin but leaves it >= 0 causes no visible defect, and one that makes it negative fails the test. Measurement and drawing share one constant, so a menu row wider than its control makes the menu wider than the control; it does not truncate text. The Settings meter-standard combos are inside the walk. The measured spread (Linux 31 px against macOS 26 px) supports the test's stated reason for not hard-coding a floor. The real residual gap is a different one, already disclosed in the code: a drawPopupMenuItem that spends more than chrome survives both width tests (:4245-4251).

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 1 · severity 1 · discoverability 1 · efficiency 1 · coherence 2 · change risk 2 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### TEST-009

**A line shift in PluginEditor.cpp or LookAndFeel turns source-lint red until docs are re-anchored, but the gate covers 11 Markdown anchors (not 33), ignores SpectrumView, GrHistoryView and FrameClock, and is cleared by one automated --fix**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Preserve** | **none** | high | partially-confirmed | Test infrastructure | Verification stops at the headless boundary, and gesture/restore threading can crash the host | — |

**Evidence**

- e769f33:scripts/check-citations.py:59-69 — TRACKED includes only src/gui/PluginEditor.{cpp,h} and src/gui/LookAndFeel.{cpp,h} among the GUI sources
- e769f33:.github/workflows/build.yml:474-490 — the citation gate step; :389-391 source-lint (no needs:)
- e769f33:.github/workflows/build.yml:3-7 — push on all branches plus pull_request; :25-27 — cancel-in-progress per ref
- e769f33:.github/workflows/build.yml:2068-2083 and :2717-2721 — macOS jobs with 60-minute timeouts; :2490-2555 and :2881-2902 — 4 pluginval invocations x 3 passes each
- e769f33:docs/procedures/TESTING.md:110-153 — the re-anchor procedure (--fix in the same change set; check against both bases; DELIBERATE_REAIMS for repairs)
- Probe (scratch clone of e769f33, deleted): +3 lines in PluginEditor.cpp → exit 1, 5 DRIFTED (CHANGELOG.md x2 entries, ADR-0027 x3); --fix → 're-anchored 5 citation(s) … 0 need a human', exit 0; +3 lines in SpectrumView.cpp and GrHistoryView.cpp → clean, exit 0
- Citation count using the script's regex: 38 src/gui anchors in Markdown, 11 gated

**Current behaviour.** When a GUI edit shifts lines in PluginEditor.cpp/.h or LookAndFeel.cpp/.h, source-lint goes red on the few anchors below the edit until 'check-citations.py --fix' is run and its output committed. Edits to the meter and graph views never trip it. The full matrix (12 jobs, including 2 macOS jobs with 12 pluginval passes each) runs on every push, and superseded runs on the same ref are cancelled.

**Problem.** There is a small, real friction: one extra command and a few line-number edits in CHANGELOG and ADR-0027 for PluginEditor.cpp changes, plus slow macOS feedback. The finding overstated both the anchor count and the file scope. No evidence was found that this has blocked or discouraged UI fixes.

**Root cause.** This is a deliberate policy. Line anchors in documents of record are kept honest by a content-identity gate (DOCUMENTATION_LIFECYCLE_POLICY, TESTING_POLICY level 1b), and the full cross-platform gate runs on every push because a green Linux build says nothing about the macOS AU or the Windows build.

**User impact.** No end-user impact. The developer cost is about one command per PluginEditor.cpp edit, plus wall-clock CI time. *Scope:* Docs that cite PluginEditor.{cpp,h} or LookAndFeel (ADR-0027, CHANGELOG, THREAD_MODEL) and the CI trigger configuration.

**Proposed improvement.** None required. Keep the gate and the matrix. Optional, zero-risk convenience: a documented local pre-push step running 'check-citations.py --fix --base @{u}' (already the TESTING.md procedure). If the anchors in ADR-0027 and CHANGELOG become a nuisance, respell them with the function name beside the line, as the script's header already recommends. Do not narrow the CI matrix for UI pushes.

**Alternatives considered.**

- *Path filters or a trimmed matrix for UI-only pushes* — Rejected. UI pushes are exactly when the macOS editor-open pluginval and the AU validation matter, and changing the trigger or matrix is a build-system change (ARCHITECTURE_REVIEW_GATE).
- *Drop line anchors in favour of symbol anchors* — Not justified by the measured cost (5 anchors, auto-fixed). It would weaken the documentation-evidence contract.
- *Pre-push hook running --fix* — Acceptable as developer tooling. Not a product or UX change.
- *Keep as is (chosen)* — The cost is small and automated, and the gate protects documentation correctness.

**Decision: Preserve · none.** The reproduced cost is one automated command touching at most a handful of anchors, and only for PluginEditor and LookAndFeel edits. The meter and graph views are not gated at all. The CI breadth is required by TESTING_POLICY's hard release gate. Changing either would weaken documented safeguards to save minutes.

**Architecture gates.**

- build-system change (CI trigger/matrix edits) — applies only to the rejected alternatives, not to the preserved state

**Dependencies.** None.

**Acceptance criteria.**

- check-citations.py stays in source-lint, preceded by its --self-test
- A line-shifting edit to src/gui/PluginEditor.cpp is cleared by a single 'check-citations.py --fix' reporting '0 need a human'
- Line-shifting edits to src/gui/SpectrumView.cpp or src/gui/GrHistoryView.cpp leave the gate clean (they are not in TRACKED)

<details><summary>Verification record</summary>

**Method.** Read check-citations.py (TRACKED at :59-69, doc_files at :517-553), build.yml (:3-7 triggers, :25-27 concurrency, :389-391 source-lint with no needs, :474-490 gate, the job list and timeouts, macOS pluginval invocations at :2490-2555 and :2881-2902) and TESTING.md:110-153. Counted citations with the script's own CITATION regex and TRACKED set: 38 src/gui anchors in tracked Markdown, of which 11 are gated. Probed a scratch clone of e769f33. Inserting 3 lines at the top of src/gui/PluginEditor.cpp made 'check-citations.py --check --base HEAD' exit 1 with 5 drifted anchors (CHANGELOG x2, ADR-0027 x3). '--fix' reported 're-anchored 5 … 0 need a human' and exited 0. Inserting 3 lines at the top of SpectrumView.cpp and GrHistoryView.cpp left the gate clean ('64 anchor(s) still point at the same text'). The clone was deleted afterwards; the real repository was not touched.

**Corrections to the candidate claim.** (1) Anchor count: 11 Markdown anchors are gated (ADR-0027 x6, CHANGELOG x3, THREAD_MODEL x2; targets PluginEditor.cpp x6, PluginEditor.h x4, LookAndFeel.h x1), plus 2 self-citations inside the LookAndFeel sources. The other 27 src/gui anchors (the ADR-0039, ADR-0009, ADR-0011, DESIGN, ADR-0040 and ADR-0016 ones) point at untracked files (FrameClock.h, SpectrumView, GrHistoryView, CurveView, the sibling's LevelMeter) or carry a revision prefix, and are not checked. (2) Edits to SpectrumView.cpp and GrHistoryView.cpp do NOT trip the gate (reproduced). (3) The fix is one automated command (--fix), with no manual re-anchoring. (4) build.yml has 12 jobs, not 9. source-lint has no needs:, and no build job depends on it, so a drift reddens the run without blocking build or test feedback. Concurrency cancels superseded runs on the same ref.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 3 · severity 1 · discoverability 2 · efficiency 2 · coherence 1 · change risk 4 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### TEST-010

**Rosetta-slice float miscompare: the flake is rare, its cause unknown, and the no-retry, no-skip stance is deliberate and documented. It is not specific to UI pushes**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Preserve** | **none** | medium | confirmed | Test infrastructure | Verification stops at the headless boundary, and gesture/restore threading can crash the host | — |

**Evidence**

- e769f33:docs/KNOWN_ISSUES.md:1377 — KI-019 header; :1385-1391 — the CI record of the Rosetta failures
- e769f33:docs/KNOWN_ISSUES.md:1432-1440 — 'not a state this program can be in'; the environment fault remains
- e769f33:docs/KNOWN_ISSUES.md:1442-1467 — the 2026-09-08 amendment: specSpan (single-threaded) failed once at 4a5b71c; attempt 2 passed; '117 exactlyEqual call sites'; 'no code workaround, no weakened assertion, no added retry, no skipped lane'
- e769f33:docs/policies/TESTING_POLICY.md:186-205 (rule 3: pluginval crash retry only) and :208-214 (rule 4: skips must be documented)
- e769f33:.github/workflows/build.yml:2295-2300 — 'Self-tests, x86_64 slice under Rosetta'
- GitHub Actions build.yml runs (read-only listing, 60 most recent completed): after the KI-019 redesign, 1 red first attempt in 32 non-cancelled runs (34220696244 at 4a5b71c, green on attempt 2); 0 since 2026-09-08
- grep at e769f33: exactlyEqual on 117 lines (124 occurrences) in tests/state_tests.cpp and 67 lines in tests/dsp_tests.cpp

**Current behaviour.** The macOS job executes both suites on the x86_64 slice under Rosetta. A bit-exact float comparison there has miscompared at a very low rate in ways the program cannot produce. When it happens the run is red, someone investigates, and the occurrence is recorded under KI-019. No automated retry or skip exists, by explicit decision.

**Problem.** There is an occasional red CI run that costs an investigation and a manual re-run. It is rare (one red first attempt in 32 recent runs, none in the last 11) and not linked to UI work.

**Root cause.** An unknown fault in x86_64 execution under Rosetta 2, documented in KI-019 as an environment fact. The suite's bit-exact assertions are deliberate.

**User impact.** No end-user impact. The developer cost is occasional triage. *Scope:* The macOS job's Rosetta self-test step. It applies to every push regardless of content.

**Proposed improvement.** None. Keep the policy: every occurrence is investigated and recorded in KI-019 with its run id, the check name and the result of re-running identical bytes. Re-open the question only if occurrences accumulate after 2026-09-08.

**Alternatives considered.**

- *Automatic retry of the Rosetta self-test step* — Rejected. KI-019 and TESTING_POLICY explicitly refuse it, and it would hide a genuine miscompare. Editing CI is also a build-system change.
- *Tolerance-based float comparisons* — Rejected. The bit-exact assertions are load-bearing (the lesson of INC-004), and loosening them weakens every affected test.
- *Skip executing the Rosetta slice* — Rejected. It removes execution coverage of the shipped universal binary's x86_64 slice as run on Apple Silicon (ADR-0036 makes the Rosetta result gate the macOS uploads), and TESTING_POLICY rule 4 would require a KNOWN_ISSUES entry.
- *Keep as is (chosen)* — Proportionate to a rare, recorded environment fault.

**Decision: Preserve · none.** The exposure is real but rare and measured, and it is not UI-specific. The repository has already weighed and recorded the trade-off, and every alternative weakens a gate to save an occasional re-run.

**Architecture gates.**

- build-system change / TESTING_POLICY rules 3-4 — applies only to the rejected retry/skip alternatives

**Dependencies.** KI-019 (open environment fault)

**Acceptance criteria.**

- The Rosetta self-test step (build.yml:2295-2300) has no retry, continue-on-error or skip added
- Each future Rosetta-slice miscompare is appended to KI-019 with its run id, the failing check and whether an identical-bytes re-run passed
- A second post-2026-09-08 occurrence triggers a recorded re-evaluation in KI-019 rather than a silent re-run

<details><summary>Verification record</summary>

**Method.** Read KI-019 (e769f33:docs/KNOWN_ISSUES.md:1377-1467), including the 2026-09-08 amendment, TESTING_POLICY rules 1, 3 and 4 (:174-214) and the Rosetta self-test step (build.yml:2295-2300). Counted exactlyEqual at e769f33: 117 lines (124 occurrences) in state_tests.cpp and 67 lines in dsp_tests.cpp. Listed the 60 most recent completed build.yml runs through the GitHub Actions API (read-only; 736 runs in total).

**Corrections to the candidate claim.** The description is accurate. Framing corrections: (1) Nothing ties it to UI: any push, docs-only included, is equally exposed; 4a5b71c followed documentation-only commits. (2) 'No retries' means no AUTOMATED retry. The 4a5b71c occurrence was investigated and the identical bytes re-run manually (attempt 2 green), and TESTING_POLICY rule 3 separately allows pluginval crash retries. (3) Observed rate: in the 60 most recent completed runs, 4 reds on 2026-09-07 came from the pre-redesign test design (since fixed). After the KI-019 redesign there were 32 completed non-cancelled runs with exactly one red first attempt (run 34220696244), and 0 reds in the 11 runs since the 2026-09-08 amendment.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 1 · severity 1 · discoverability 2 · efficiency 2 · coherence 1 · change risk 4 · complexity 2 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

