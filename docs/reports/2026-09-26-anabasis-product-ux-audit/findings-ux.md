# Anabasis product / UX audit — findings: User experience and operation logic

Part of [`2026-09-26-anabasis-product-ux-audit.md`](../2026-09-26-anabasis-product-ux-audit.md) (audited revision `e769f33`, 2026-09-26). This file holds the complete record of each finding in these categories; the report carries the index, the systemic themes, the roadmap and the decision record. Code anchors are pinned to `e769f33`; runtime observation ids refer to [`worklogs/2026-09-26-product-ux-audit.md`](../../../worklogs/2026-09-26-product-ux-audit.md).

Each record: decision, priority and confidence after calibration; evidence; current behaviour; problem; root cause; user impact and scope; proposed improvement; alternatives considered; decision rationale (with any calibration or challenge outcome); architecture gates; dependencies; acceptance criteria; and the verification record.

## UX — User experience and operation logic

### UX-001

**LOCK reads as 'this knob cannot move' but only filters preset loads: drags, typing, reset, automation, A/B and undo all move a locked Ceiling; neither Ceiling knob shows the lock, and the Advanced view has no LOCK control at all**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | partially-confirmed | Interaction model | Save, load, browse and restore change or lose state without saying so | Phase 5 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:609-612 — LOCK toggle bound to int_ceilingLock; tooltip 'Keep the Ceiling where it is while you browse presets'
- e769f33:src/InternalState.h:110 — tooltipsOn defaults false, so the only in-UI scope statement is hidden by default
- e769f33:src/PresetManager.cpp:67, :119, :310-315 — the only reads of the lock (file-preset skip, factory-apply skip)
- e769f33:src/PluginProcessor.cpp:1451-1529 — applySlotToLive has no lock check; called by undo :619, redo :646, A/B :1578
- e769f33:src/gui/PluginEditor.cpp:1779-1781, :1797-1800 — Advanced list has ceilingK + tpToggle but no lock; ceilingLockToggle is Simple-only
- e769f33:docs/architecture/design-decisions/ADR-0010-parameter-surface.md:191-195 — lockable set {ceiling}, consulted by preset apply, lock 'never in A/B, undo or presets'
- e769f33:docs/DESIGN.md:603 — the field is named 'Ceiling preset-lock' in the design inventory
- e769f33:docs/user/USER_MANUAL.md:171, :293-294, :402-403, :526-528 — manual scopes LOCK to preset browsing
- G-16: session capture `rt/gestures/29-sheet.png`, session capture `rt/gestures/32-sheet.png` — drag, typing, double-click and automation moved the Ceiling with LOCK on; only the pill changed
- VER0-2: session capture `rt/verify-0/06-crop.png` — A/B switch to B with LOCK lit: Ceiling -1.00 dB -> -0.10 dB (app.log dump)
- VER0-4: session capture `rt/verify-0/08-crop.png`, session capture `rt/verify-0/09-crop.png` — Advanced view with int_ceilingLock=1: no LOCK control or cue; '>' to Transparent Master holds Ceiling -1.00 with no visible reason

**Current behaviour.** A Simple-only pill labelled 'LOCK' sits under TP beside the Simple Ceiling knob. When engaged, it makes preset applies (factory and file) skip the ceiling parameter. Nothing else is affected: knob drags, typed values, double-click reset, host automation, A/B switches, Copy and undo/redo all change the ceiling while the pill stays lit. Neither Ceiling knob renders the lock. The Advanced view has no LOCK control, although the session-persistent lock still governs preset browsing there. The scope is stated only in an off-by-default tooltip and in the manual.

**Problem.** The generic word 'LOCK' next to the delivery ceiling suggests the value is frozen, but it is a preset-apply filter. The mismatch surfaces when a lit LOCK coexists with a ceiling that changes on A/B, undo or a stray drag. In Advanced, a lock engaged earlier in Simple silently stops presets from moving the ceiling, and nothing on screen explains why.

**Root cause.** The lock is designed as a preset-apply filter (DESIGN §4.2 :568-571; ADR-0010 :191-195). The UI exposes it under an unscoped label, and only in the Simple layout. No paint path reads iid::ceilingLock for simpleCeilingK or ceilingK, and updateModeVisibility hides the only control in Advanced.

**User impact.** Users who engage LOCK (which the manual's delivery workflow recommends at :426-427) can believe the ceiling is protected against every change. The readout does visibly change on a drag or A/B, so this is a clarity and trust problem rather than a silent output change. The Advanced-view invisibility produces unexplained behaviour: a preset loads but its ceiling does not. *Scope:* Simple top-right ceiling cell, Advanced Limiter panel, and every preset load in either view while int_ceilingLock is set. The lock persists with the session, so it can stay engaged across sessions unnoticed.

**Proposed improvement.** Keep the ADR-0010 semantics and make the scope and state visible. (1) Add a second toggle bound to the same int_ceilingLock Value in the Advanced Limiter panel, beside the existing TP switch. This follows the pattern TP already uses (tpToggle and tpSimpleToggle share one parameter), so the lock is never invisible in either view. (2) While locked, draw a small lock glyph or tint on both Ceiling readouts (simpleCeilingK and ceilingK). This is display only. (3) Give the control a caption that names its scope, for example 'PRESET LOCK' or a padlock with 'presets' under it. The wording is owner product text (C8); it must fit the 70 px cell (PluginEditor.cpp:1727-1733) or re-lay it out, without truncation at every UI scale. (4) Add one sentence to USER_MANUAL §3.2/§7.3: A/B, undo, Copy and direct edits still move a locked ceiling.

**Alternatives considered.**

- *Leave as-is* — Rejected. The Advanced view hides a state that changes what preset browsing does there, and the unscoped word misleads in the view that does show it.
- *Make LOCK a true freeze: block drags and typing, ignore A/B, undo and automation for the ceiling* — Rejected. It changes the ADR-0010 lockable-set semantics (Hard Stop, PARAMETER_COMPATIBILITY_POLICY rule 6) and breaks A/B as a complete-slot compare. Ignoring host automation of a host-visible parameter would also break the host contract.
- *Turn tooltips on by default* — Insufficient alone and cross-cutting (PF-anamorph-reference-12). It explains the scope on hover but leaves the lock invisible in Advanced and on the knobs.
- *Show a transient 'Ceiling held' cue when a preset load skipped a differing ceiling* — A useful optional complement, and it also answers [STATE-014](findings-state-model.md#state-014). On its own it does not fix the Advanced invisibility.

**Decision: Modify · P2.** The behaviour matches Accepted ADR-0010 and should not be widened. The fix is a constrained, display-only change: make the lock visible where it acts and name its scope. That removes the misreading without touching the gated lockable-set contract.

**Dependencies.** [STATE-002](findings-state-model.md#state-002) (a lock indicator is only truthful once the held ceiling keeps its unit/mode); [STATE-014](findings-state-model.md#state-014) (its honesty gap is closed by this indicator instead of the '*')

**Acceptance criteria.**

- With int_ceilingLock on, a lock indicator is visible on the Simple Ceiling readout and on the Advanced Limiter Ceiling readout, and disappears when the lock is off
- The Advanced view exposes a control bound to int_ceilingLock; toggling it in either view is reflected in the other within one UI tick
- The control's visible label or caption names its preset scope and renders without truncation at XS, M and XL UI scales with tooltips off
- Existing semantics unchanged: with LOCK on, drag, typed value, double-click reset, automation, A/B, Copy and undo still move the ceiling; testFactoryPresets' lock check and testALockedCeilingSurvivesAPresetThatNamesIt stay green
- USER_MANUAL §3.2 and §7.3 state that LOCK affects preset loads only

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/PluginEditor.cpp:609-612, :1723-1735, :1767-1803; e769f33:src/InternalState.h:108-110,131; e769f33:src/PresetManager.cpp:61-68,113-123,306-315; e769f33:src/PluginProcessor.cpp:391-425 (Copy), :619/:646 (undo/redo), :1451-1529 (applySlotToLive), :1556-1600 (A/B). Grepped all of src/ for ceilingLock: the only readers are the two preset-apply cores. The only GUI uses are the toggle binding (:609-612) and layout/visibility (:1733, :1800). Viewed session capture `rt/gestures/29-sheet.png` and 32-sheet.png and [capture](captures/10-lock-toggle.png). Reproduced on :130 with stepped pointer motion (VER0-2, VER0-4): with LOCK lit, the A/B switch moved Ceiling from -1.00 dB to -0.10 dB. In Advanced, no LOCK control or cue exists, and '>' kept -1.00 with no visible reason.

**Corrections to the candidate claim.** (1) The claim understates the Advanced case. ceilingLockToggle is in the simpleOnly list (PluginEditor.cpp:1797-1800), while Advanced shows ceilingK and tpToggle (:1779-1781) but no lock toggle. So in Advanced the lock state is invisible, even though it still changes what preset browsing does there. (2) 'Session load moves a locked ceiling' overstates. A session restore brings back int_ceilingLock together with the ceiling it saved, so it does not bypass the lock. (3) Copy does not move the live ceiling (copySlotToOther writes the other slot). The other slot's ceiling appears on the next A/B switch. (4) Undo moves a locked ceiling only when the undone step's pre-state held a different ceiling, such as a ceiling edit or a step taken before LOCK was engaged. Undoing a locked preset load leaves it unchanged. Undo was not exercised at runtime for this case. (5) The tooltip and the manual (USER_MANUAL.md:171, :293-294, :402-403) state the preset-only scope correctly, and ADR-0010:191-195 makes A/B and undo passing through the lock deliberate. The defect is the unscoped label and the missing on-knob and Advanced rendering, not the semantics.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 3 · severity 3 · discoverability 4 · efficiency 2 · coherence 4 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-002

**The whole STATISTICS panel, including about 310 px of empty glass in Simple, is an unmarked reset button: any click, drag or right-click discards I, LRA, PLR and both peak holds with no affordance, confirmation or undo**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P1** | high | confirmed | Interaction model | The session figures have no visible scope, liveness or tap, and a stray click wipes them | Phase 1 |

**Evidence**

- e769f33:src/gui/LoudnessMeterView.cpp:55: setInterceptsMouseClicks (true, false)
- e769f33:src/gui/LoudnessMeterView.cpp:67-70: mouseDown (any button) calls processor.requestMeterReset()
- e769f33:src/gui/LoudnessMeterView.cpp:153-281: paint has no hover, pressed or cursor state; the header (:161-163) is plain text
- e769f33:src/gui/LoudnessMeterView.h:42-43, 90: Component + SettableTooltipClient; mouseDown is the only mouse override
- e769f33:src/gui/LoudnessMeterView.cpp:6-10: the tooltip is the only explanation
- e769f33:src/InternalState.h:110: tooltipsOn defaults to false; gate at e769f33:src/gui/PluginEditor.cpp:286
- e769f33:src/gui/PluginEditor.cpp:1756-1757: Simple bounds 292 x (720-46-144) = 530 px; e769f33:src/gui/PluginEditor.h:633-634
- e769f33:docs/architecture/design-decisions/ADR-0020-waveform-statistics-panel.md Decision 6: 'Identical in both views'; the rows take 202 px
- e769f33:docs/user/USER_MANUAL.md:271-274 ('Click the STATISTICS panel to reset') and :453 (the podcast workflow depends on it)
- e769f33:docs/HANDOVER.md:1337: 'Accessibility names on every control'; the reset has none
- Runtime V-07: session capture `rt/visuals/13a-stats-before-click-stats.png`, session capture `rt/visuals/13b-after-click-TP-row-stats.png`, session capture `rt/visuals/13g-after-rightclick-panel-editor.png`
- Runtime E04: session capture `rt/edges/30-stats-drag-strip.png` (reset at drag mouse-down), session capture `rt/edges/31-stats-click-strip.png` (click in empty lower area)
- Runtime LAY-13(c): session capture `rt/layout/28-stats-before-c.png`, session capture `rt/layout/28-stats-after-c.png`
- Runtime verify-1 R1 (stepped motion, display :131): [capture](captures/08-statistics-click-reset.png). Panels: before, hover on the empty area (no cue), +0.15 s after the click (I/LRA/PLR '-', TP hold cleared), +1.2 s
- JUCE e18f7f50 juce_NSViewComponentPeer_mac.mm:2228: acceptsFirstMouse returns YES (the scratchpad copy of the pinned JUCE)

**Current behaviour.** Every mouseDown anywhere inside the STATISTICS component clears the integrated histograms (gated and ungated), LRA, and the TP and SP max-holds, so PLR clears too. This covers left, right and double clicks and the start of a drag, on a row or on the blank glass below the rows. There is no cursor change, hover or pressed state, label, confirmation, undo, context menu, keyboard path or accessible name. The explanation is a tooltip, and tooltips ship off.

**Problem.** A destructive action that cannot be undone is bound to what looks like a read-only display. The hit area is largest where there is no content: about 318 px of blank panel in Simple. A user can trigger it by accident, and a user who wants it cannot find it with the default settings.

**Root cause.** The reset is bound to LoudnessMeterView::mouseDown for the whole component (LoudnessMeterView.cpp:55, 67-70) rather than to a dedicated control. When Simple adopted the right-hand meter panel, it gave the view 530 px of height (PluginEditor.cpp:1756-1757) for about 212 px of rows. The whole blank remainder therefore inherits the reset. The one discoverability path, the tooltip, is off by default (InternalState.h:110).

**User impact.** A click to focus or activate the plugin window lands on the reset: JUCE's macOS peer delivers the activating click. So does a click in dead space or on a row, and it throws away however long the programme has been measured. Recovery means replaying the whole programme. If the stray click happens mid-pass, the final I, LRA and PLR describe only the tail of the programme but look exactly like a full-programme reading, because no 'measuring since' cue exists ([VIS-009](findings-visualisation.md#vis-009)). That is the delivery number in the manual's podcast workflow. Conversely, someone who wants a fresh measurement cannot find the gesture unless they read §3.4 or enable tooltips. Keyboard and screen-reader users cannot reset at all. *Scope:* Both views and all formats. Simple is the most exposed: a 292x530 hit area that is about 60 % blank, about 23 % of the 940x720 editor. The integrated-loudness and PLR workflows depend on it (USER_MANUAL §3.4, §8).

**Proposed improvement.** 1. Make the panel body inert. Clicks anywhere on rows or blank glass do nothing. Keep the tooltip on hover, reworded so it no longer says 'Waveform statistics off the output'.
2. Add a small labelled 'RESET' text button, right-aligned on the existing 16 px STATISTICS header line, in both views. It uses the family's quiet pill or text-button styling with hover and pressed states. It is keyboard focusable with Space/Enter and carries the accessible title 'Reset statistics'. It calls the unchanged requestMeterReset(). Because it sits in the header band, the 202 px row layout from ADR-0020 Decision 6 does not move.
3. Optionally, a right-click anywhere on the panel opens a one-item menu, 'Reset statistics'.
4. Acknowledge the reset visibly: a brief highlight on the button and, via [VIS-009](findings-visualisation.md#vis-009), a scope readout that returns to 0:00.
5. Update USER_MANUAL §3.4 and the §8 podcast step to name the control. Do not add a modal confirmation or undo: the dedicated small target removes the accidental case, and undoing would need audio-thread histogram snapshots for little benefit.

**Alternatives considered.**

- *Leave as is (documented in the manual)* — Rejected. The accidental reset and the invisible, partial-programme result remain, and discoverability depends on reading the manual.
- *Ship tooltips on by default* — Fixes discoverability for mouse users only. It does nothing for accidental resets and changes a global default for one control. Insufficient.
- *Keep the whole-panel target but require a double-click* — Cuts single stray clicks, but the reset stays invisible and a double-click on a meter is still an unusual hidden gesture. Weaker than a labelled control.
- *Confirmation dialog on reset* — Heavy for a routine metering action and interrupts the workflow. It is unnecessary once the target is small and explicit.
- *Shrink the Simple panel to its content height* — Removes the blank target but leaves the rows as a hidden reset and changes the Simple layout. It could complement the fix but does not replace it.
- *Undo for the meter reset* — Would need snapshots of the audio-thread histograms and holds. The cost is out of proportion once accidental triggering is designed out.

**Decision: Proceed · P1.** Code and runtime evidence confirm the issue, including a stepped-motion reproduction. The fix is GUI-only and reuses the existing request path, so no hard-stop gate is touched. It turns a frequent, silent, costly trap into an explicit action. Rated P1 rather than P0: the loss is a measurement that can be regenerated by replaying, not settings or audio. It is still a frequent trap with costly recovery in the delivery-loudness workflow, and it can leave a partial-programme I that looks like a full one.

*Adversarial challenge:* evidence holds: yes; priority justified: yes (suggested P1); decision justified: yes (suggested Proceed). The behaviour is real, reachable with ordinary mouse input, and confirmed in code and at runtime. Calling it 'unmarked' is accurate even though the gesture is deliberate and family-derived: here an accidental trigger costs a whole integrated measurement, not a peak hold.

P1 holds under 'frequent trap with costly recovery'. It rests mainly on the discoverability half: in every session where a user wants a clean delivery reading, the only on-screen cue is off by default, and keyboard and screen-reader users have no path at all. That breaks the brief's §8 accessibility intent, which HANDOVER.md:1337 claims is met. Plausible accidental presses on a 292x318 blank target add to it, though their frequency is inferred.

The proposal is already the minimal change: an explicit control that keeps the unchanged request path. No smaller change removes both the accidental and the hidden-gesture problems. Proceed, with the documentation and ADR-amendment scoping and the test-criterion fix above.

On gates: the judge's empty gate_flags list is correct for the hard-stop list. What it misses is that this is a recorded product-family convention change and needs an owner acknowledgement. *Proposal risks:* 1. Historical records. ADRs are historical records and are not rewritten (ADR_INDEX.md:91). The criterion 'No document says click the panel' therefore cannot apply to ADR-0020:135, THREAD_MODEL.md:313 or HANDOVER.md:1330. Scope it to USER_MANUAL §3.4 and §8, the tooltip, and the class comment at LoudnessMeterView.h:29-31. Add a dated amendment note to ADR-0020 Consequences rather than editing its text.
2. Family divergence. The change departs from Anamorph's click-a-number reset (LevelMeter.h:26-27), and the judge did not name that. Record the cost-asymmetry rationale so the brand Level-5 human pass (BRAND_CONSISTENCY_CHECKLIST) does not 'restore' the sibling behaviour.
3. Target size. A button confined to the 16 px header band is small at 100% UI scale. Extend its hit box into the 10 px top padding (26 px total). No row moves, so ADR-0020 Decision 6 is preserved.
4. Untestable criterion. Criterion 1 ('unchanged in a capture 1 s later') cannot pass with audio running, because M, S, I and TP legitimately move. Restate it as 'not reset': no row goes to '-', and the TP and SP holds are not lowered.
5. Stopped transport. A deliberate press with the transport stopped still blanks M, S and RMS through publishSilentMeters (PluginProcessor.h:648; [DOC-002](findings-doc-test.md#doc-002)). The new button will look as if it wipes everything, and the acknowledgement design must account for that.
6. Lost gesture. Users trained by the manual lose the gesture without notice. Make the right-click menu (step 3) required rather than optional, or have the hover tooltip say where the reset moved.

No hard-stop gate is touched. requestMeterReset and testMeterResetClearsSessionHolds (state_tests.cpp:3752-3810) stay unchanged; the view only gains a child button.

**Dependencies.** [VIS-009](findings-visualisation.md#vis-009) (the scope readout provides the post-reset feedback and exposes partial measurements); [DOC-002](findings-doc-test.md#doc-002) (what a deliberate reset visibly blanks, especially with no audio flowing); [VIS-012](findings-visualisation.md#vis-012)

**Acceptance criteria.**

- With audio running in Simple and Advanced: left, right, double click and drag-start on any row or on the blank area below PLR leave I, LRA, TP, SP and PLR unchanged in a capture 1 s later.
- A visible control labelled RESET (or an equivalent text label) sits on the STATISTICS header line in both views, readable with tooltips OFF, with distinct hover and pressed rendering.
- Activating the control by mouse, or by Tab focus plus Space/Enter, clears I, LRA, TP, SP and PLR exactly as requestMeterReset does today, and testMeterResetClearsSessionHolds still passes.
- The control exposes the accessible title 'Reset statistics'.
- The row geometry of the panel (ADR-0020 Decision 6) is pixel-identical before and after the change, apart from the header-line button.
- USER_MANUAL §3.4, the §8 podcast step and the panel tooltip describe the control. No document says 'click the panel'.

<details><summary>Verification record</summary>

**Method.** Read every cited anchor at e769f33: LoudnessMeterView.cpp:6-10, 53-57, 67-70, 153-281 and .h:42-43, 90; InternalState.h:110; PluginEditor.cpp:286, 1756-1757 and PluginEditor.h:633-634; USER_MANUAL.md:271-274 and 453. Viewed 13a/13b, 13g, edges/30 and 31. Reproduced on :131 with stepped motion (5 intermediate moves) in Simple while music played at -6 dB. Hovering the empty area at (830,560) gave no cursor, hover or tooltip change. One click there took I -14.2 to '-', LRA 4.4 LU to '-' and PLR 14.8 to '-', and TP 0.62 fell to -0.10 dBTP within 0.15 s; see [capture](captures/08-statistics-click-reset.png). Also checked the JUCE build the repo pins (ANABASIS_JUCE_TAG e18f7f50, CMakeLists.txt:83). Its macOS peer returns YES from acceptsFirstMouse (scratchpad build-tsan/_deps/juce-src/modules/juce_gui_basics/native/juce_NSViewComponentPeer_mac.mm:2228), so on macOS the click that activates a plugin window also reaches the component.

**Corrections to the candidate claim.** The empty region is about 318 px, not 310. The panel is 530 px tall and the rows end about 212 px down: 10 pad + 16 header + 2 + 3x26 + 6 + 5x20. Advanced also resets on any click, with about 42 px of empty space. The observer cross-reference in V-07 (LoudnessMeterView.cpp:194-197) does not match e769f33; the real mouseDown is at :67-70, and the finding's own anchors are correct. The finding also misses that the reset has no keyboard path and no accessible name. The view sets no title and is not focusable (:53-57), although HANDOVER.md:1337 claims 'Accessibility names on every control'.

</details>

<sub>Verifier scores (1-5): impact 4 · frequency 4 · severity 4 · discoverability 4 · efficiency 4 · coherence 3 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-003

**Save Preset silently overwrites an existing user preset, and because the name field comes prefilled with the current name, overwriting is the default action**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P1** | high | confirmed | Preset/state workflow | Save, load, browse and restore change or lose state without saying so | Phase 0 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:936 — saveNameEditor.onReturnKey = saveOkButton.triggerClick()
- e769f33:src/gui/PluginEditor.cpp:943-949 — createLegalFileName → empty-guard → dir.getChildFile(name + ".anabasis") → proc.savePresetFile(file); no existsAsFile() check anywhere in :941-962
- e769f33:src/PresetManager.cpp:31 — return root.writeTo (file) (replaces the target unconditionally)
- e769f33:src/gui/PluginEditor.cpp:2373 and :906/:2383 — prefill with proc.currentPresetName(), select-all on focus, focus grabbed
- e769f33:docs/user/USER_MANUAL.md:385 — 'Saving over an existing name overwrites it.'
- e769f33:docs/architecture/design-decisions/ADR-0022-preset-identity.md:26 (prefill documented), :59 (Option A 'forbid/auto-rename' rejected — for factory-name collisions)
- ST-05 — AuditTest overwritten with EDM Club values, no prompt — session capture `rt/state/38c-crop.png`, session capture `rt/state/38d-topbar.png`
- E11 — re-save overwrote EdgeTest1 (2017→2060 bytes) — session capture `rt/edges/60a-save-panel-prefilled-crop.png`
- verify-2 repro — prefill 'Alpha' while 'Alpha *' — session capture `rt/verify-2/05-save-prefill-crop.png`
- verify-2 repro — 'Al/pha'+Return silently replaced Alpha.anabasis (loudness 0.0→90.0) — [capture](captures/13-save-overwrite.png)

**Current behaviour.** Save (the button or Return) cleans the typed name (createLegalFileName), builds <user preset folder>/<name>.anabasis and writes it without checking whether it exists. The panel closes and the label and tick move to that file. The field opens prefilled with the current preset name, all selected, so Return right after opening replaces the loaded user preset. Typing a name that already exists, or one that becomes an existing name after illegal characters are stripped, replaces that other preset with no prompt and no feedback.

**Problem.** The only destructive file operation in the product has no guard. A collision, whether typed or created by stripping characters, silently destroys a different saved preset. The menu lists presets by file name only, so the loss shows up only when the user loads that preset later.

**Root cause.** saveOkButton.onClick has only an empty-name guard (e769f33:src/gui/PluginEditor.cpp:944-945) and a success branch (:949-961). The save overlay has no second (confirm) state and no message line. The manual adopted the Anamorph behaviour as the documented contract (USER_MANUAL.md:385).

**User impact.** A user can lose a tuned mastering preset in one keystroke with no warning. For a preset not loaded in the current session, there is no way back from inside the plugin. Users who keep a library of presets across projects carry the most risk. *Scope:* Every user-preset save whose cleaned name matches an existing file in the preset folder: typed collisions, Return on the prefilled name after edits, and collisions created by stripping characters (verified). The code path is platform-independent. On case-insensitive file systems (macOS, Windows) names that differ only in case also collide. This is inferred and was not reproduced at runtime. Factory presets cannot be overwritten; they are compiled in, and saving under a factory name creates a USER file per ADR-0022.

**Proposed improvement.** Add an inline two-step replace flow to the existing Save overlay. When the target file for the cleaned name already exists, the first Save or Return writes nothing. Instead the status line under the buttons reads "'Alpha' already exists — replace it?" and the Save button's label becomes "Replace". A second Save or Return writes the file, closes the panel and selects the file (ADR-0022 Decision 4, unchanged). Cancel or Escape closes the panel and leaves the file byte-identical. Any edit to the name returns the panel to its normal state. The check runs on the cleaned name, so stripped-character collisions are caught too. The preset file format, the identity rules and the factory-name case stay as they are. USER_MANUAL §7.2 (line 385) is updated to describe the prompt.

**Alternatives considered.**

- *Leave as-is (documented, matches Anamorph)* — Rejected. The destruction is silent and can be unrecoverable, in a dialog every preset-saving user opens.
- *Separate modal confirmation window (juce::AlertWindow)* — Heavier. It is unbranded unless restyled and adds a second modal surface in a plugin window. The inline state does the same job inside the overlay that already exists.
- *Auto-rename on collision ("Alpha 2")* — Rejected. It takes the naming decision away from the user, and ADR-0022 Option A rejected the same move for factory-name collisions.
- *Confirm only when the target is a different file from the current identity (no prompt for 'update the loaded preset')* — Viable lighter variant. It removes one keypress from the update flow but leaves an accidental Return after edits unguarded. Acceptable if the owner judges the extra Return too costly.
- *Keep a .bak copy of the replaced file* — Complementary silent recovery path. It adds hidden files to the user folder and needs a restore story. Defer.

**Decision: Modify · P1.** Confirmed in code and reproduced three times (ST-05, E11, verify-2), including a collision created by stripping characters that the user never typed. The cost is permanent loss of user work. The fix stays within the existing overlay on the message thread. It changes neither the preset file bytes, the identity model nor any parameter, so no hard-stop gate applies. It does not conflict with ADR-0022: confirming a replace neither forbids a name nor auto-renames one (Option A). It must not be extended to block saves under a factory name. The divergence from Anamorph is allowed: ADR-0009 says a copied file 'diverges here'.

*Calibration:* the verifier judged Proceed / P1; the final judgement is Modify / P1. Challenge: evidence holds and P1 holds, because overwriting a preset not loaded in this session cannot be undone inside the plugin and preset libraries outlive projects. Decision changed Proceed->Modify: prompt on every existing-target collision EXCEPT the unedited prefill of the currently selected user file (the documented one-keystroke update); key on 'text was edited', not identity alone; guard Return auto-repeat; share [UX-018](findings-ux.md#ux-018)'s status line. Missed gate recorded: silent overwrite is the inherited family convention (DEVELOPMENT_BRIEF §1.2 'Inherit'; BRAND_CONSISTENCY_CHECKLIST.md:53 §A must-match), so the change needs an ADR and owner sign-off, or a family-wide proposal.

*Adversarial challenge:* evidence holds: yes; priority justified: yes (suggested P1); decision justified: no (suggested Modify). (a) Evidence: code-confirmed and screenshot-confirmed. It is a real, reachable, destructive and silent file replacement with no existence check. The only parts I could not re-check are the verify-2 mtime and byte numbers. (b) Priority: P0 is not met. The action does what the user explicitly asked (save under name X), it is documented at USER_MANUAL:385, and the loss is a side effect that occurs only on a collision. P2's 'less frequent situation' clause has some pull, because the harmful case (a collision with a different preset) is occasional; frequency is closer to 2 than 3. P1 still holds under 'important workflow / trap with costly recovery': the loss cannot be undone from inside the plugin for any preset not loaded in the current session, and preset libraries persist across projects. (c) Decision: Modify rather than Proceed. The fix is warranted. It changes no parameter, serialization, threading, DSP or latency, and it does not conflict with ADR-0022 (neither Option A nor Decision 4 is touched). But (i) it is a structural deviation from the inherited family preset convention, which the brief and the brand checklist require to go through an ADR and owner sign-off, and the judge did not name this. (ii) The always-confirm variant taxes the documented one-keystroke update to guard a case that is mostly recoverable. A prompt that fires on every existing-target collision except the unedited prefill of the currently selected user file covers every unrecoverable case the evidence shows. It needs protection against auto-repeat and a status line added to the overlay. *Proposal risks:* 1. MISSED GOVERNANCE GATE. DEVELOPMENT_BRIEF.md:33 (§1.2) requires 'Inherit: … preset and A/B interaction conventions', and BRAND_CONSISTENCY_CHECKLIST.md:53 lists 'Preset system — browsing, saving …' under §A 'Structural — must match'. The checklist rule at :35-37 says a difference 'becomes a deliberate deviation with an ADR and owner sign-off — never a silent difference'. Anamorph documents silent overwrite as its convention. The judge's rationale for diverging cites only ADR-0009:113 ('diverges here'). That clause covers maintenance of copied code files, not the product-level consistency rule. gate_flags should therefore record 'family-consistency deviation: BRIEF §1.2 Inherit / BRAND checklist §A, ADR + owner sign-off', or raise it as a family-wide convention change, since Anamorph has the same defect and cannot be modified from here. This is not one of the listed hard-stop categories, but the judge's claim that no gate applies is incomplete.
2. The proposal costs the most on the most frequent and least harmful path. Always-confirm adds a second Return to every intended 'update loaded preset' save, even though that path is the one the judge's own correction calls recoverable. A narrower rule catches ST-05, typed library collisions and edited collisions: skip the prompt only when the field still holds the unmodified prefill AND the cleaned target equals proc.currentPresetSelection() as a userFile (e769f33:src/PluginProcessor.h:151); prompt in every other case where the target exists. Under the judge's alternative 4 (identity-only), the verify-2 'Al/pha' case would get no prompt, because Alpha was the current identity. The narrower rule must therefore key on 'the text was edited', not on identity alone.
3. Return auto-repeat or a double-tap can defeat the guard. The first Return arms the prompt and a held or repeated Return confirms at once. The confirm should require a fresh key press, or ignore repeats within a short window. Add an acceptance criterion for this.
4. The overlay has no status line today (a fixed 340×150 panel at PluginEditor.cpp:1454, laid out at :1476-1484). About 22 px are free below the buttons, so a line fits without resizing, but it must be added. The proposal's wording 'the status line' assumes it exists, which ties this to [UX-018](findings-ux.md#ux-018).
5. The automated-test criterion is feasible, because state_tests.cpp:3974-3975 already constructs the editor. The save lambda is private, though, so the existence/confirm decision should be factored into something a test can reach, and the test must not write into the real user preset folder.

**Dependencies.** [UX-018](findings-ux.md#ux-018) (the replace prompt shares the save-panel status line; stripped-name collisions must hit the prompt)

**Acceptance criteria.**

- With Alpha.anabasis in the preset folder: Save Preset…, type 'Alpha', Return. The file's mtime and bytes are unchanged, and the panel stays open with a prompt naming 'Alpha' and a 'Replace' button.
- A second Return, or a click on Replace, writes the file, closes the panel, and the label reads 'Alpha' with the USER row 'Alpha' ticked.
- Escape or Cancel in the confirm state closes the panel and leaves Alpha.anabasis byte-identical.
- Typing 'Al/pha' while Alpha.anabasis exists raises the same prompt, naming 'Alpha' (the cleaned name).
- Editing the name while the prompt is showing returns the button label to 'Save' and clears the prompt.
- A new name writes after a single Return with no prompt. A save under a factory name with no USER file of that name writes with no prompt and still selects the USER row (ADR-0022 Decision 4).
- USER_MANUAL §7.2 describes the replace prompt instead of 'Saving over an existing name overwrites it.'
- An automated test asserts that an existing preset file is not modified until the replace is confirmed.

<details><summary>Verification record</summary>

**Method.** Code read at e769f33: e769f33:src/gui/PluginEditor.cpp:936 (Return triggers Save), :941-962 (the Save handler), :906 and :2365-2385 (prefill and select-all). Also read e769f33:src/PluginProcessor.h:171-183, e769f33:src/PresetManager.cpp:6-32 (XmlElement::writeTo replaces the target through a temporary file), e769f33:docs/user/USER_MANUAL.md:385-395, and ADR-0022:26 and :59. Viewed session capture `rt/state/38c-crop.png` and session capture `rt/edges/60a-save-panel-prefilled-crop.png`. Reproduced on :132 with stepped pointer motion (under rt/verify-2/). (a) With 'Alpha *' loaded (edited), Save Preset… prefilled 'Alpha', fully selected (05-save-prefill-crop.png). (b) With Alpha loaded and Loudness moved to 90 %, I typed 'Al/pha' and pressed Return. Alpha.anabasis was replaced: mtime 1790416837→1790417117, size 2017→2091 bytes, loudness 0.0→90.0. The panel closed at once, with no prompt at 0.3 s (21-22-strip.png). Tried to refute by looking for any existence check or confirmation path in the handler; there is none.

**Corrections to the candidate claim.** (1) 'Nothing inside the plugin can recover it' is overstated for one case. If the overwritten preset was loaded earlier in the same slot, its values are still in the per-slot undo history: Undo back to them, then Save again. Recovery is impossible when the destroyed preset was not loaded in this session: a typed name collision (ST-05) or a collision created by name stripping ('Al/pha'→Alpha, verified). (2) The behaviour is inherited. Anamorph behaves the same way and documents it ('overwrites it silently', Anamorph@fd78c3b:docs/user/USER_MANUAL.md:383-384). (3) The prefill itself is documented in ADR-0022:26, and 'update the loaded preset' is often what the user intends. The defect is the missing confirmation, not the prefill.

</details>

<sub>Verifier scores (1-5): impact 4 · frequency 3 · severity 4 · discoverability 3 · efficiency 3 · coherence 3 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-004

**Advanced view hides LOCK and LEARN, including a Learn pass that is still running; the relocations and the out-LUFS removal are not losses**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | partially-confirmed | Information architecture | Controls do not show whether they are live or what they select, and the explanation sits in tooltips that ship off | Phase 5 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:1797-1801 — simpleOnly[] includes ceilingLockToggle, tpSimpleToggle, learnButton, outLufsCaption, outLufsValue
- e769f33:src/gui/PluginEditor.cpp:1807-1808 — only MATCH/DELTA/FREEZE are shared between views; LEARN is not
- e769f33:src/gui/PluginEditor.cpp:609-612 — LOCK = 'Keep the Ceiling where it is while you browse presets', bound to int_ceilingLock (session state)
- e769f33:src/gui/PluginEditor.cpp:2046-2074 — the Learn countdown/accent/empty-pass flash is shown only on learnButton, which is hidden in Advanced
- e769f33:src/dsp/AdaptiveEngine.h:467-473 — startLearn sets learnActive with no timeout; commitLearn only on an explicit stop
- e769f33:docs/policies/MODE_AND_ADAPTATION_POLICY.md:50-51 — Learn 'is an explicit user action with an explicit end; it never runs silently in the background'
- e769f33:docs/DESIGN.md:923 — the §6.3 Advanced wireframe shows 'gain ceiling 🔒' in the LIMITER zone
- e769f33:docs/user/USER_MANUAL.md:293-294 — 'The Ceiling LOCK lives next to the Ceiling knob itself' (true only in Simple)
- e769f33:src/gui/PluginEditor.cpp:2038-2043 and e769f33:src/gui/LoudnessMeterView.cpp:83 — out-LUFS and the Statistics S row are the same meterLufsS() value
- LAY-03 + [capture](captures/02-advanced-view.png) — no LOCK/LEARN in Advanced
- verify-3 run: session capture `rt/verify-3/l1-crop.png` (LEARN counting '5' in Simple), l2-crop.png (Advanced during the pass: no Learn indicator), l4-crop.png (back in Simple ~7 s later: accent 'LEARN', still learning)
- G-16 — LOCK shields the Ceiling only on preset loads, with no visual change on the knob

**Current behaviour.** Pressing ADV hides LOCK, LEARN and out-LUFS. Ceiling and TP reappear in the LIMITER zone, without LOCK. MATCH/DELTA/FREEZE move to the right of the utility row, and Statistics moves beside the graph well. A Learn pass started in Simple keeps running while Advanced is shown, with no indicator and no stop control. The lock stays in force during preset browsing in Advanced but cannot be seen or changed there.

**Problem.** Advanced does not offer two session-level operations that apply to it: the Ceiling lock, which governs every preset load made from the shared top bar, and Learn. The hidden Learn state is the serious part. A calibration pass the user started stays open and invisible, which contradicts MODE_AND_ADAPTATION_POLICY invariant 3 ('never runs silently in the background') in practice. The missing LOCK also contradicts the DESIGN §6.3 wireframe and the manual's 'lives next to the Ceiling knob'.

**Root cause.** updateModeVisibility partitions the widgets into disjoint advOnly/simpleOnly lists (PluginEditor.cpp:1772-1801). LOCK and LEARN were placed only in the Simple list, while FREEZE, their adaptive/session sibling, was made shared (:1807). The Learn state readout lives only on the Simple button (:2046-2074).

**User impact.** (a) Learn: a user who starts a pass and opens Advanced to watch the stage meters cannot see that the pass is still open or stop it. It keeps integrating whatever plays, possibly the whole song rather than the representative section, until they return to Simple. The learned reference then silently biases the adaptive trims. (b) LOCK: an Advanced user browsing presets cannot see why the Ceiling does not follow a preset, cannot lock it before browsing, and does not know the lock state without switching views. *Scope:* Advanced view only. Learn is an occasional calibration action. LOCK matters on every preset-browsing session done in Advanced. It is display/command wiring only: no parameter, serialization or DSP change.

**Proposed improvement.** In Advanced: (1) Make LEARN a shared control like FREEZE. Place it in the utility row's toggle cluster: re-balance the 92/92/100 px toggle cells plus the ~60 px spare in the middle band (PluginEditor.cpp:1650-1680) so MATCH/DELTA/FREEZE/LEARN fit without changing kUtilityH. It uses the same component or the same grammar (countdown during the minimum pass, accent while learning, warn flash on an empty pass), so a pass started in either view is visible and stoppable in both. (2) Place the Ceiling LOCK toggle beside the LIMITER Ceiling, as DESIGN §6.3 draws it: the AUTO/TP row at :1605-1607 can split into three, or a small lock glyph can sit at the Ceiling cell. Bind it to the same int_ceilingLock value. (3) Leave the relocations alone and do NOT re-add out-LUFS in Advanced; the Statistics S row already carries it.

**Alternatives considered.**

- *Leave as-is* — Leaves a running Learn pass invisible in Advanced, against policy invariant 3, and keeps the undocumented LOCK drift from DESIGN §6.3. Not justified.
- *Stop (or cancel) any running Learn pass when switching to Advanced* — Removes the invisible state, but makes the view switch an actor on adaptive state. Switching views should be inert (manual §3: switching views never changes the sound), and a commit on switch would learn a partial passage. Worse than showing the pass.
- *Show only a read-only 'LEARNING' indicator in Advanced* — Smaller, and it fixes the silent-background problem, but the user still has to leave the view to stop the pass or to start one. Acceptable fallback if utility-row space proves too tight.
- *Unify both views' toggle rows into one shared strip in the same position* — Would remove the relocation cost too, but it is a layout redesign touching both frames, and Simple's family 720 px frame is at stake. Disproportionate to the evidence.

**Decision: Modify · P2.** The core claim holds and is sharper than stated: an explicit-start/explicit-end Learn pass becomes invisible and unstoppable in Advanced, and LOCK's omission departs from the signed-off §6.3 wireframe. The other sub-claims do not hold up (out-LUFS is duplicated by the Statistics S row; the relocations follow the two designed layouts), so the change should be constrained to exposing LEARN and LOCK in Advanced, not to re-uniting the views.

**Architecture gates.**

- None for the proposed display-only placement (no parameter ID, schema, threading, DSP-order, latency or macro-contract change)
- ADR-0023 Decision item 9 (Advanced frame 940×822): only if a placement grew kUtilityH or kPanelRowH; the proposed placements do not

**Dependencies.** G-16 (LOCK wording/semantics finding): any LOCK relabel should land in both views at once; [UX-015](findings-ux.md#ux-015) (the Advanced height must stay 822)

**Acceptance criteria.**

- In Advanced, a LEARN control is visible and operable; clicking it starts a pass with the same countdown, accent and empty-pass warn flash as Simple
- Start Learn in Simple, switch to Advanced: the running pass is visibly indicated and can be stopped from Advanced; switching back shows the same state
- In Advanced, a Ceiling LOCK control sits adjacent to the LIMITER Ceiling, bound to int_ceilingLock; toggling it in either view is reflected in the other
- With LOCK on and Advanced shown, loading a factory preset leaves the Ceiling unchanged, exactly as in Simple
- Editor sizes remain 940×720 (Simple) and 940×822 (Advanced); no out-LUFS readout is added to Advanced
- testModeSwitchIsSoundNeutral and the registry snapshot pass unchanged; USER_MANUAL §3.3 lists LEARN and LOCK in the Advanced view

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/PluginEditor.cpp:1767-1812 (updateModeVisibility), :609-612 (LOCK setup), :621-644 (LEARN), :2038-2043 (out-LUFS is written only when !advanced, from proc.meterLufsS()), :2046-2074 (Learn state tick), :1598-1609 (LIMITER layout), :1650-1680 (utility row), e769f33:src/gui/LoudnessMeterView.cpp:83 (the Statistics S row also reads meterLufsS()), e769f33:src/dsp/AdaptiveEngine.h:467-520 (Learn has no maximum length; it commits only on an explicit stop), e769f33:docs/policies/MODE_AND_ADAPTATION_POLICY.md:50-51, e769f33:docs/DESIGN.md:909-930, e769f33:docs/user/USER_MANUAL.md:171,293-294. Viewed [capture](captures/02-advanced-view.png) and session capture `rt/layout/07c-simple-after.png`. Reproduced on :133 with stepped motion: started LEARN in Simple, switched to ADV, waited ~7 s, switched back (session capture `rt/verify-3/l1-crop.png`, l2-crop.png, l4-crop.png).

**Corrections to the candidate claim.** (1) Removing out-LUFS loses no information. The Simple out-LUFS readout and the Statistics 'S' row read the same meterLufsS() value (07c shows both at -27.3), and the Statistics panel is visible in both views. (2) Ceiling, TP, MATCH, DELTA, FREEZE and Statistics are relocated, not removed. Ceiling and TP are the same parameters in both views. Different positions follow from the two layouts (DESIGN §6.2/§6.3), and there is no evidence of harm beyond a one-time learning cost. (3) DESIGN §6.3 has no LEARN in Advanced (DESIGN.md:909-910 vs 916-931), so its absence follows the signed-off wireframe. LOCK's absence does NOT: §6.3 draws 'gain ceiling 🔒' in the LIMITER zone (DESIGN.md:923), and no ADR supersedes it. (4) Material addition found while verifying: a Learn pass started in Simple keeps running in Advanced. l2 shows no indicator anywhere, there is no way to stop the pass there, and on return to Simple the button still shows accent 'LEARN' (still learning; l4). The pass keeps accumulating until the user goes back to Simple.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 3 · severity 3 · discoverability 4 · efficiency 3 · coherence 4 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-005

**LEARN hides its state: a stop before 5 s is silently refused, the countdown has no words, afterwards only an accent text colour shows Learn is still running, and a commit gets no acknowledgement**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | confirmed | Feedback/observability | The adaptive engine changes the audio from state the user cannot see, keep or reset | Phase 5 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:621 — learnButton.setClickingTogglesState(false): a plain TextButton with no latched visual state
- e769f33:src/gui/PluginEditor.cpp:632 — else if (nowMs - learnStartedMs >= kLearnMinPassMs): a stop click inside 5 s falls through with no feedback branch
- e769f33:src/gui/PluginEditor.h:630 — kLearnMinPassMs = 5000.0
- e769f33:src/gui/PluginEditor.cpp:2050-2056 — while isLearning(): text = remaining whole seconds, then 'LEARN'; textColourOffId = accent. Text and text colour are the only running cues
- e769f33:src/gui/PluginEditor.cpp:2058-2072 — after the commit, only the unmoved-refs case gets feedback (1500 ms warn); a successful commit returns to colours::text, the same as idle
- e769f33:src/gui/PluginEditor.cpp:1797-1803 — learnButton is in simpleOnly[] and hidden in Advanced
- e769f33:src/dsp/AnabasisEngine.cpp:415-422 — Learn start/commit run only when a block consumes learnCmd, so isLearning() stays false until then
- e769f33:src/gui/PluginEditor.cpp:641-642 — the tooltip is the only in-product 'click again to stop'; e769f33:src/InternalState.h:110 tooltipsOn defaults to false
- e769f33:docs/DEVELOPMENT_BRIEF.md:131 — Learn should be 'consistent with Anamorph's Learn / Auto Gain'. The Anamorph precedent uses a latched toggle plus a live dB readout (Anamorph@fd78c3b:src/PluginEditor.cpp:470-480, 1464-1469) and commits by a visible parameter write (Anamorph@fd78c3b:src/PluginProcessor.cpp:188-212)
- Runtime G-18: session capture `rt/gestures/34-sheet.png`, session capture `rt/gestures/34e-learn-10s.png` ('5','3','1', then an orange LEARN from 10-30 s; the second click turns it white)
- Runtime verify-4 R1 (refused stop at 2.76 s): session capture `rt/verify-4/05-refused-sheet.png`
- Runtime verify-4 R2 (Advanced view mid-pass, no indicator): session capture `rt/verify-4/06-adv-while-learning.png`
- Runtime verify-4 R3 (commit looks the same as idle; ADAPTIVE refOnsetRate=3.338 refTiltDb=-4.690 written): session capture `rt/verify-4/11-commit-sheet.png`, s02-after-learn.xml
- Runtime verify-4 R4 (empty pass: warn flash only): session capture `rt/verify-4/15-empty-sheet.png`

**Current behaviour.** Click 1 requests a start. The button shows a white 'LEARN' until the next block top sets isLearning(). It then shows accent digits 5→1 and an accent 'LEARN' for the rest of the pass, with the same background, shape and label as idle. A click inside 5 s is ignored with no response. A click after 5 s requests a commit. When the engine drops isLearning(), the text returns to white (a success looks exactly like idle), or flashes warn for 1.5 s if the references did not move. Nothing shows that a learned reference is active. In Advanced view the control is absent, so a running pass there is invisible and cannot be stopped.

**Problem.** A multi-state process (start pending, running under the minimum, running and committable, stop pending, committed, empty) is shown only through text and hue. The refusal and success states have no presentation at all, and the running state is lost entirely in Advanced view.

**Root cause.** The Learn state machine lives in editor-local fields (learnStartedMs, learnStopPending, emptyFlashUntilMs; PluginEditor.h:555-561) and is projected onto one TextButton's text and textColourOffId in the 24 Hz tick (PluginEditor.cpp:2046-2075). onClick has no branch for a refused click (:632). The tick has no branch for 'requested but not yet running' or 'committed'. hasLearned() is read only for the empty-pass comparison and never displayed. The control is in simpleOnly[] (:1797-1803). The 'wordless' presentation is recorded only in code comments (:618, PluginEditor.h:556) and in MODE_AND_ADAPTATION_POLICY.md:158-160 as a description of what shipped. No owner/brand decision requiring it was found.

**User impact.** Learn is optional and its audible effect is bounded and subtle, so this is not an audio-correctness problem. A user can leave a pass running indefinitely without knowing it, in which case the calibration is never committed. They can also commit a span they did not intend (the out-of-phase click), or click at 3 s and not know the click was refused. They cannot confirm a successful Learn, or tell whether the session uses a learned or factory reference. The podcast workflow (USER_MANUAL.md:447-451) relies on Learn. *Scope:* The Simple-view toggle-row LEARN button and its tick branch, plus the lack of any Learn presence in Advanced view. Editor only; engine and command atomics unaffected.

**Proposed improvement.** Target interaction:
1. From the first click until the engine reports the pass over, LEARN shows a latched running state that differs from idle in shape or fill, not hue alone: a lit background like the FREEZE pill's on-state, plus a progress ring or bar that fills over the 5 s minimum.
2. Once the minimum is reached, the control tells the user the next action is available (e.g. the label changes to STOP/DONE, or the full ring pulses).
3. A click inside the minimum is either shown as refused (the ring flashes or shakes) or, preferably, queued as 'stop at minimum', shown as pending, and committed automatically at 5 s. That keeps an explicit, user-issued end.
4. After a start request with no block consumed yet, show a distinct 'waiting for audio' pending state, derived in the editor from a local startRequested flag while isLearning() is false.
5. A commit whose references moved gets a success flash (about 1-1.5 s, distinct from the warn flash).
6. A small persistent marker shows while hasLearned() is true.
7. Advanced view gets a running indicator and a stop affordance (see PF-ui-architecture-12).
No engine or threading change: every state is derivable from the existing isLearning() and hasLearned() readouts and the published refs.

**Alternatives considered.**

- *Leave as-is* — Rejected. The running, refused, committed and pending states stay invisible, and it contradicts brief :131 (family consistency with Anamorph's latched Level Match plus readout).
- *Turn LEARN into a FREEZE-style toggle pill bound to isLearning()* — Viable and closest to the Anamorph precedent, but a pill cannot show the minimum-pass progress or the pending/committed states without an extra readout.
- *Auto-stop after a fixed pass length* — Removes the refusal problem but drops the user's explicit end, which conflicts with MODE_AND_ADAPTATION_POLICY invariant 3 ('explicit user action with an explicit end'). Not recommended.
- *Queue an early stop so it commits at 5 s* — Recommended part of the fix. It removes the silent refusal while keeping a user-issued end.

**Decision: Proceed · P2.** The code and two independent runtime runs (G-18 and verify-4) confirm every hidden state. The fix is editor-only, reads atomics that are already published, and touches no gate. It restores the family-consistency requirement in the brief. Priority is P2, not P1: Learn is optional and not used in every session, its audible effect is bounded, and recovery is cheap (re-learn).

**Dependencies.** PF-ui-architecture-12 (LEARN and out-LUFS unreachable in Advanced view); PF-product-contract-docs-3 (tooltips off by default); [STATE-009](findings-state-model.md#state-009) (acknowledged commit, learned-state marker, reset path); [UX-019](findings-ux.md#ux-019) (the commit acknowledgement is the channel for the Freeze hint); [DOC-004](findings-doc-test.md#doc-004) (manual and tooltip must describe the final grammar)

**Acceptance criteria.**

- From the first LEARN click until isLearning() drops, a greyscale screenshot of the control differs from the idle control (not only by hue).
- A click inside the minimum pass either produces a visible response within 100 ms or is shown as a pending stop that commits automatically at 5 s; no click is silently discarded.
- With the start command issued but no processBlock yet, the control shows a pending state different from idle.
- A non-empty commit shows a success acknowledgement for at least 1 s that differs from both idle and the empty-pass warn flash; the empty pass still shows its warn flash.
- While hasLearned() is true a persistent marker is visible; after loading a session without ADAPTIVE it disappears.
- In Advanced view a running pass is visible and can be ended.
- No change to AdaptiveEngine/AnabasisEngine Learn code or learnCmd; the dsp_tests Learn cases (e.g. the learnCmd ordering test near e769f33:tests/dsp_tests.cpp:3639-3690) stay green.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/PluginEditor.cpp:614-644 (onClick, the minimum-pass guard at :632, the tooltip at :641-642), :2046-2075 (tick), :1797-1803 (simpleOnly[]); e769f33:src/gui/PluginEditor.h:555-561 and :627-630; e769f33:src/dsp/AnabasisEngine.cpp:411-422 (the command is consumed at a block top); e769f33:src/dsp/AdaptiveEngine.h:467-520 (startLearn, commitLearn, isLearning). Viewed G-18 session capture `rt/gestures/34-sheet.png` and 34e-learn-10s.png. Reproduced on :134 with stepped pointer motion and music at -6 dB. R1: a stop click at 2.76 s was ignored; the digits went on 2→1 and then an orange LEARN (verify-4/05-refused-sheet.png). R2: switching to ADV mid-pass showed no Learn control and no indicator; back in Simple the orange LEARN was still there (verify-4/06-adv-while-learning.png, 07-crop.png). R3: a stop after 5 s turned the text white, the same as idle, with no other change, although savexml showed an ADAPTIVE child had been written (verify-4/11-commit-sheet.png, s02-after-learn.xml). R4: a pass over silence gave a red/warn text flash of about 1.5 s and the previous references were kept (verify-4/15-empty-sheet.png, s03-after-empty.xml).

**Corrections to the candidate claim.** 1. 'A click with the transport stopped shows nothing' should read 'a click while the host calls no processBlock shows nothing'. The start is a command the engine consumes at a block top, and commitLearn/startLearn run whether or not the block is audible. Many hosts keep processing with the transport stopped; there the countdown starts at once and a silent pass ends as the empty-pass warn flash. The harness cannot show the no-processing case because its audio always flows, so that part rests on code only.
2. The manual does say how to stop ('press it again', USER_MANUAL.md:312-313). In the product, only the tooltip says it, and tooltips are OFF by default (e769f33:src/InternalState.h:110).
3. Additions that make the problem wider: LEARN is Simple-only, so in Advanced a running pass has no indicator and cannot be stopped (R2).
4. Clicks follow isLearning(). A user who reads the orange LEARN as idle and clicks to 'start' commits the running pass instead, and their next click (meant as 'stop') starts a new pass, so the user's model and the button fall out of phase.
5. Code only, not run: learnStartedMs is editor-local (PluginEditor.h:557). An editor reopened mid-pass has no countdown and no minimum-pass guard.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 2 · severity 3 · discoverability 4 · efficiency 2 · coherence 4 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-006

**Tooltips ship off (an inherited ⊕ family default), yet they are the only in-product explanation of LOCK, LEARN's running state, MATCH/DELTA/FREEZE, the edited dot, the Statistics click-to-reset, the uncaptioned Advanced combos and the latency cost of Settings choices**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Information architecture | Controls do not show whether they are live or what they select, and the explanation sits in tooltips that ship off | Phase 5 |

**Evidence**

- e769f33:src/InternalState.h:110 — tree.setProperty (iid::tooltipsOn, false, nullptr)
- Anamorph@fd78c3b:src/InternalState.h:51 — identical family default
- e769f33:docs/DESIGN.md:605 — `int_tooltipsOn` | tooltips | bool | ⊕ off (proposed, pending fine review)
- e769f33:docs/DEVELOPMENT_BRIEF.md:170 — 'Tooltips on every control, governed by the toggle'; :338 C8 — tooltip/label wording is maintainer-owned
- e769f33:docs/user/USER_MANUAL.md:149-150 — 'every control has one, but they are off by default'; :287 'Default off'
- e769f33:src/gui/PluginEditor.cpp:36 (BYPASS tip empty, deliberate), :313-316 (title ghost button, no tip), :693 (About link tip cleared), :90-98 (detach legend removed from knob tips by 0.1.3 owner directive)
- e769f33:src/gui/PluginEditor.cpp:609-612 (LOCK meaning lives only in its tip), :641-642 (LEARN 'click again to stop' only in tip), :656-657 (edited dot only in tip), :759-782 (Oversampling/Phase latency cost only in tips)
- e769f33:src/gui/LoudnessMeterView.cpp:6-10 (reset explained only in tooltipText) and :67-70 (mouseDown -> requestMeterReset)
- e769f33:src/PluginProcessor.cpp:1771,1880 — InternalState (incl. int_tooltipsOn) is saved/restored with each instance's session state; no global preference store exists in src
- e769f33:tests/state_tests.cpp:6011-6052 — tests assert only that sliders/combos/named toggles carry a non-empty tip
- runtime verify-5: session capture `rt/verify-5/01-default-hover-learn-crop.png`, 03-default-hover-stats-crop.png (2 s hover at defaults: no hint, no hover cue)
- observers: G-01 session capture `rt/gestures/01-settings-open.png` (Tooltips OFF), LAY-05 session capture `rt/layout/39c-tips-sheet-a.png`, E04 session capture `rt/edges/64d-stats-tooltip-2.png`

**Current behaviour.** A fresh instance opens with tooltips off. Hovering any control shows nothing. Several Simple-view satellites and the Advanced mode combos have no caption or visual cue that explains them. The only explanation is a tip that stays hidden until the user opens Settings and turns Tooltips on, and that choice is saved per instance.

**Problem.** Several of the product's non-self-describing affordances carry their meaning only in the help layer, and that layer is hidden by default. Examples: LOCK, which locks only against preset loads; the Statistics panel, where any click resets the integrated/LRA/peak holds; the clickable edited dot; and 'Pre' and 'Tape' in the Advanced combos. The brief asks for tooltips on every control. The default was carried over from a sibling whose Simple controls are self-describing.

**Root cause.** Two things combine. First, a ⊕ default copied from Anamorph (InternalState.h:110; DESIGN.md:605). Second, some semantics were placed only in tooltip strings: the detach legend was moved out of the tips by owner directive, while LOCK, the Statistics reset and the combos gained no captions or cues. Because the switch is per-instance session state (not a per-user preference), either default taxes one user group on every new instance.

**User impact.** A first-time user cannot find out from the UI what LOCK, the Statistics click, the edited dot or the combo values do. This feeds the separate traps E04 (a stray click discards an integrated measurement), G-16 (LOCK misread as a knob lock) and G-18 (LEARN left running). Experienced users are not affected. *Scope:* Simple and Advanced views, every format. It applies to every new instance. The explanations exist in USER_MANUAL.md, outside the product.

**Proposed improvement.** Target: a first-time user sees what each non-obvious control does or which state it is in, without first finding Settings → Tooltips. An expert who turns tips off is not made to do so again on every instance. Constrained change, in two parts. (1) Put the meaning that matters into the UI itself, as the separate findings propose. Examples: a caption on each Advanced mode combo (LAY-05); a hover cue or explicit reset affordance on the Statistics panel (E04); a visible lock state on the Ceiling (G-16); an explicit 'learning' state (G-18). All wording comes from the maintainer (brief C8). (2) Put the tooltip default to the owner as a named fine-review decision, not a silent ⊕. If it is to become ON, prefer a per-user remembered choice that seeds new instances, keeping int_tooltipsOn in the session state as now, over a bare flip. Align USER_MANUAL.md:150 with the deliberate tipless controls.

**Alternatives considered.**

- *Flip the default to ON (one line at InternalState.h:110, plus the DESIGN/manual rows)* — Cheapest, and it fixes first-run discoverability. But the setting is per-instance, so every expert must turn tips off in every new instance. It also diverges from the Anamorph family default without a family decision.
- *Per-user preference (global properties file) seeding new instances, default ON until the user chooses* — Serves both user groups. It adds a new persistence surface outside the session and needs an owner/architecture decision. It is a serialization-schema change only if int_tooltipsOn were removed from the session state, which is not proposed.
- *Keep OFF and fix only the specific non-self-describing affordances (captions, cues, state indication)* — This removes the harmful dependencies on the help layer even for users who never enable tips. It is the part that should happen regardless.
- *Leave as-is* — Not justified: the brief (§8) wants tooltips on every control, and the explained behaviours include a destructive click (E04).

**Decision: Modify · P2.** The harm is real but indirect. The cure is to stop hiding load-bearing meaning behind an opt-in layer, not simply to flip a per-instance default, which would move the cost onto experts on every instance. The default itself is an open ⊕ item for the owner's fine review, and the wording is maintainer-owned (C8), so the audit should frame the decision rather than make it.

**Architecture gates.**

- Owner ⊕ decision: e769f33:docs/DESIGN.md:605 int_tooltipsOn default is a proposed value pending the fine review
- DEVELOPMENT_BRIEF C8 (e769f33:docs/DEVELOPMENT_BRIEF.md:338): any new caption/cue wording must come from the maintainer
- Serialization schema change (ARCHITECTURE_REVIEW_GATE) ONLY if int_tooltipsOn were moved out of the session state; a default change alone keeps the ADR-0007 missing-field-default rule and the existing field
- Family divergence from Anamorph (ADR-0009 / BRAND_CONSISTENCY_CHECKLIST 'Tooltips — same presentation and governing toggle'): not a hard stop, but a family decision

**Dependencies.** LAY-05 (Advanced combo captions); E04 (Statistics click-to-reset affordance); G-16 (LOCK state/meaning); G-18 (LEARN running state); [UI-008](findings-ui.md#ui-008) (tip correctness matters more if tips are more visible); [UI-014](findings-ui.md#ui-014) (tip placement matters more if default ON)

**Acceptance criteria.**

- On a fresh instance at default settings, each of the four Advanced mode combos shows a visible caption naming what it selects (Detector / Model / Style / Position, or maintainer wording) without hovering
- On a fresh instance at default settings, the Statistics panel shows a visible hover cue or reset affordance before a click resets I/LRA/PLR/peak holds
- The int_tooltipsOn default is recorded as an owner decision (DESIGN.md:605 no longer marked ⊕), with the per-instance cost of the chosen default stated
- If the default becomes ON: after a user turns Tooltips OFF once, a newly inserted instance opens with Tooltips OFF (or the per-instance cost is explicitly accepted in the decision record)
- USER_MANUAL.md §3 no longer claims every control has a tooltip while BYPASS, the wordmark and the About link deliberately have none (or those gain tips)

<details><summary>Verification record</summary>

**Method.** Read every anchor at e769f33: e769f33:src/InternalState.h:110 (tooltipsOn=false), Anamorph@fd78c3b:src/InternalState.h:51 (same default), e769f33:docs/DESIGN.md:605 (int_tooltipsOn '⊕ off', i.e. pending the fine review), e769f33:docs/DEVELOPMENT_BRIEF.md:170 ('Tooltips on every control, governed by the toggle') and :338 (C8: tooltip/label wording is owned by the maintainer), e769f33:docs/user/USER_MANUAL.md:149-150 and :287, e769f33:src/gui/PluginEditor.cpp:36/90-98/313-316/609-612/641-642/656-657/693/759-782, e769f33:src/gui/LoudnessMeterView.cpp:6-10 and :67-70, e769f33:tests/state_tests.cpp:5972-6052. I also checked that the setting is stored per instance, in the session state (e769f33:src/PluginProcessor.cpp:1771,1880; there is no PropertiesFile in src). Runtime on :135 with a fresh instance and stepped pointer moves: hovering LEARN, MATCH and the Statistics panel for 2 s at defaults showed no hint and no hover cue (verify-5/01, 03). With Tooltips ON, LOCK, Ceiling, Tone and the Settings rows show their tips. Viewed the cited gestures/01, edges/64d and layout/LAY-05 evidence.

**Corrections to the candidate claim.** 'Only in-product explanation' is slightly overstated for two controls. TP gives visible feedback: the Ceiling unit changes to dBTP (G-17). LEARN shows a countdown. For LOCK, MATCH, DELTA, FREEZE, the edited dot, the Statistics reset and the four Advanced mode combos, the claim holds. The empty BYPASS tip is deliberate and matches the sibling (cpp:36, 'the red pill labels itself'). So the manual's 'every control has one' is minor doc drift, not user harm. The suspected root cause 'not re-evaluated' cannot be verified. What the evidence does show is that the default is a recorded ⊕ proposal (DESIGN.md:605) awaiting the owner's fine review. One aggravating fact was not in the claim: the setting is per-instance session state, so any default costs one population of users a toggle on every new instance.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 3 · severity 2 · discoverability 4 · efficiency 2 · coherence 3 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-007

**Advanced does not show which knobs the macros manage until one is hand-edited; the six a Loudness sweep moves look like any other knob**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Defer** | **P3** | high | confirmed | Feedback/observability | The macro layer overwrites hand edits and automation with almost no notice | — |

**Evidence**

- e769f33:src/MacroEngine.h:32-45 — the managed set: limGain, compThreshold, compRatio, clipDrive, clipShape, colourDepth, dynTilt, eqTilt, colourTone
- e769f33:src/MacroEngine.h:57 — colourDepthPct(l,c) = 100*c*(0.4+0.6l): stays 0 at Character 0
- e769f33:src/gui/PluginEditor.cpp:487-513 — caption override table; no managed marker
- e769f33:src/gui/PluginEditor.cpp:1352-1363 — the badge is drawn only for ids in proc.detachMask()
- e769f33:docs/user/USER_MANUAL.md:336-339 — 'only the nine managed parameters can detach'; the nine are not named on screen
- e769f33:CHANGELOG.md:1426-1429 — owner directive removed the corner-dot legend from the nine managed knobs' tooltips (0.1.3)
- G-13 + session capture `rt/gestures/26-sweep-100-adv.png` — six moved knobs, no marker

**Current behaviour.** In Advanced all knobs look alike. A managed knob gets an accent corner dot only after the user hand-edits it (detach). Nothing marks the nine managed knobs while they are still on their macro curves.

**Problem.** An Advanced user cannot tell in advance which edits enter the detach/re-engage grammar (edit survives only until the next macro gesture) and which are permanent (unmanaged knobs). The model is learnable only by trial or from the manual.

**Root cause.** Membership is signalled only after the fact, through the detach badge (PluginEditor.cpp:1352-1363), and the textual legend was removed by owner directive at 0.1.3.

**User impact.** Low to moderate. The consequence of editing a managed knob is shown immediately afterwards (the dot), and re-engage happens only on a later Simple-view macro gesture or edited-dot click. The cost is a surprise the first time a hand edit is replaced by the macro curve (see [MODEL-001](findings-state-model.md#model-001)), not silent loss on unseen knobs. *Scope:* Advanced view, nine knobs across COMP, CLIP/COLOR, LIMITER and EQ. Display only.

**Proposed improvement.** Target: one glyph position carries both membership and state. Draw a hollow accent ring at the exact spot where the detach dot appears, for managed knobs still on their macro curve, and fill it when the knob detaches. No text, no new row, no tooltip legend (respecting the 0.1.3 directive). The ring could be limited to Advanced and drawn at low alpha so it does not compete with the knob arcs.

**Alternatives considered.**

- *Leave as-is* — Defensible: the dot appears on first edit, and the owner has deliberately reduced per-knob annotation. The residual cost is first-time surprise.
- *Restore the tooltip legend on the nine managed knobs* — Directly reverses the owner's 0.1.3 directive, and tooltips are off by default, so it would not reach most users. Reject.
- *A per-panel caption, e.g. 'macro-managed: Ratio, Threshold'* — Adds text to already-dense panels; a worse fit for the owner's direction than a glyph.
- *A hollow/filled ring at the badge position (proposed)* — Minimal, display-only, and consistent with the existing badge grammar. Needs owner sign-off because it adds per-knob decoration.

**Decision: Defer · P3.** The claim is true but lower-impact than stated, because the detach dot shows exactly which user values are at risk as soon as they exist. Whether a pre-edit membership cue is worth the extra per-knob decoration depends on (a) how [MODEL-001](findings-state-model.md#model-001)'s re-engage behaviour is settled (if a macro touch stops overwriting hand edits, the cue matters much less) and (b) the owner's stance at the fine review, given the 0.1.3 removal of the dot legend.

**Architecture gates.**

- None for a display-only glyph (managed_params::ids and the macro contract are untouched)
- Owner directive 0.1.3 (CHANGELOG.md:1426-1429, not an ADR): per-knob annotation of the managed set was reduced; a new cue needs owner sign-off

**Dependencies.** [MODEL-001](findings-state-model.md#model-001) (re-engage overwrites hand edits on the next macro touch); [MODEL-002](findings-state-model.md#model-002)

**Acceptance criteria.**

- At defaults in Advanced, exactly the nine managed_params::ids knobs carry a membership glyph; no unmanaged knob does
- Hand-editing a managed knob changes its glyph from the 'managed' to the 'detached' state at the same position
- Moving a macro in Simple and returning to Advanced shows re-engaged knobs back in the 'managed' state
- No tooltip text is added and no layout row or frame height changes
- The glyph table is driven from managed_params::ids, with the existing static_assert on the badge-table size kept

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/MacroEngine.h:32-45 (nine managed ids) and :50-60 (curves: colourDepth = 100*c*(0.4+0.6l), so 0 at Character 0; eqTilt and colourTone follow Tone only), e769f33:src/gui/PluginEditor.cpp:487-513 (caption display names, no marker) and :1336-1363 (a badge is painted only when detachMask contains the id). Viewed session capture `rt/gestures/26-sweep-100-adv.png`: Ratio 2.00, Threshold -12, Clip Shape 0.35, Clip Drive 9.0, Dynamic Tame 1.5, Gain 18.0 with no distinguishing mark. Read e769f33:CHANGELOG.md:1426-1429 and e769f33:docs/user/USER_MANUAL.md:333-343.

**Corrections to the candidate claim.** The impact clause 'cannot predict which knobs the next macro touch will overwrite' is overstated. The only user values a macro touch overwrites belong to DETACHED knobs, and those are badged (7 px accent dot) the moment the edit lands (PluginEditor.cpp:1359-1363). Non-detached managed knobs follow the macro but hold no user edit, so nothing is lost on them. What is truly invisible is membership BEFORE the first edit. G-13 is a Loudness-only sweep with Character 0 and Tone 0, so it reveals 6 of the 9; colourDepth, eqTilt and colourTone are also managed. Context: at 0.1.3 the owner removed the per-knob corner-dot legend from these nine knobs' tooltips by directive (CHANGELOG.md:1426-1429), which signals a preference against per-knob annotation.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 3 · severity 2 · discoverability 4 · efficiency 2 · coherence 2 · change risk 1 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-008

**Plugin BYPASS is signalled only by the family red pill and a 40 % near-black dim. There is no textual state in the body, and the live displays under the dim undercut it.**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | medium | partially-confirmed | Feedback/observability | Controls do not show whether they are live or what they select, and the explanation sits in tooltips that ship off | Phase 1 |

**Evidence**

- e769f33:src/gui/PluginEditor.h:99-103: DimLayer fills 0x66090b0e and never blocks the mouse
- e769f33:src/gui/PluginEditor.cpp:1427: dim bounds = editor minus the top bar
- e769f33:src/gui/PluginEditor.cpp:2033-2035 and :1968: dim visibility is derived only from the bypass parameter
- e769f33:src/gui/PluginEditor.cpp:36: BYPASS tooltip is deliberately empty ('the red pill labels itself')
- e769f33:src/gui/LookAndFeel.cpp:285: red pill colour 0xffd0584e for componentID 'bypass'
- e769f33:docs/DESIGN.md:846-849: family spec is 'red pill when engaged, dim overlay below the top bar'
- e769f33:docs/DEVELOPMENT_BRIEF.md:27, :33, :172 and e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:51-52: bypass placement and interaction must match Anamorph
- Anamorph fd78c3b:src/PluginEditor.h:219-222: identical DimLayer 0x66090b0e (read-only reference)
- e769f33:src/PluginProcessor.cpp:722-725 plus e769f33:src/PluginProcessor.h:138-141: getBypassParameter returns pid::bypass. JUCE 9.0.1 build/_deps/juce-src/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp:3728-3731 and juce_audio_plugin_client_AU_1.mm:2196-2198 call processBlockBypassed only when getBypassParameter()==nullptr. e769f33:CMakeLists.txt:285-290: formats are VST3/AU/Standalone
- ST-13: session capture `rt/state/30b-bypass-1s.png`, session capture `rt/state/30-bypass-composite.png`
- V-04: session capture `rt/visuals/10d-bypass-on-editor.png`
- verify-6 (own, stepped clicks): session capture `rt/verify-6/08a-simple-bypass-on.png`, 08b-simple-bypass-off.png, 08-glance-off-vs-on.png: body mean L 25.8 -> 19.7, max 220 -> 137

**Current behaviour.** BYPASS on: the 84-px top-bar toggle turns into a red pill with a brighter 'BYPASS' label. A 40 % near-black layer covers everything below the 46-px top bar. The top bar (preset name, A/B, Copy, undo) stays undimmed. No text anywhere in the body says the plugin is bypassed, and the pill has no tooltip. Under the dim, the meters, GR trace and lanes keep moving. A DAW bypass (VST3/AU) drives the same parameter and so shows the same cues.

**Problem.** The bypassed state is expressed only by a colour change in the top-right corner plus a moderate darkening of an already dark UI. Meanwhile the displays under the dim keep animating as if processing were live. The body has no explicit state text for the user's eye while it is on the meters or the graph well during a comparison.

**Root cause.** This is a deliberate family design carried over from Anamorph (DESIGN §6.1, ADR-0009 provenance): a pill plus a 0x66 overlay, chosen so the meters stay readable under bypass. No body-level state label was ever specified. The coherence gap comes mostly from [VIS-004](findings-visualisation.md#vis-004) (the displays keep showing processing), not from the alpha value.

**User impact.** Low to moderate. The user normally engages BYPASS by clicking the pill, so the state is already in their attention. The residual risk is a user whose eyes are on the graph well or STATISTICS during a comparison: they read moving GR and meters with no in-place reminder that they are hearing, and looking at, the unprocessed signal. It also applies to automation or a DAW bypass the user did not trigger in the editor. *Scope:* The editor top bar and body overlay, in both views and at every UI scale. It affects only the bypassed state. It does not affect audio.

**Proposed improvement.** Keep the family pill, dim, placement and interaction unchanged. Add one in-body state caption that appears while bypass >= 0.5 and disappears within one 24 Hz tick of un-bypass: for example 'BYPASSED' drawn in the graph well (top-left, text colour, not red), and/or a STATISTICS header suffix such as 'STATISTICS - INPUT (BYPASSED)'. The caption should also state what the displays show during bypass, which is the content decided in [VIS-004](findings-visualisation.md#vis-004)/VIS-001. It must not intercept the mouse. Do not raise the dim alpha: that would cut meter legibility during bypass and diverge from Anamorph.

**Alternatives considered.**

- *Raise the dim alpha (e.g. 0x99) or desaturate the body* — Stronger signal, but it reduces legibility of the meters the product deliberately keeps readable during bypass, and it diverges from the sibling's specified treatment (brand checklist item A). It also does not explain what the displays show.
- *Give BYPASS a tooltip* — Tooltips are off by default (int_tooltipsOn=false), and a tooltip does not signal state. Negligible value.
- *Leave as-is* — Defensible: the cue matches the family spec and the user clicked the pill. However, it leaves the displays unexplained during bypass once [VIS-004](findings-visualisation.md#vis-004)/VIS-001 change what they show.

**Decision: Modify · P3.** The evidence does not support 'the dim is too weak' as a defect. The measured darkening is substantial, and the treatment is a specified, brand-constrained family design. The supported gap is the absence of a textual state where the user is looking, combined with live displays under the dim. A small caption addresses that without touching the family pill, dim or placement.

**Architecture gates.**

- Brand consistency: DEVELOPMENT_BRIEF §1.2 'Bypass placement - same position and same interaction' (e769f33:docs/DEVELOPMENT_BRIEF.md:27, :33) and BRAND_CONSISTENCY_CHECKLIST item A (:51-52). The caption must leave pill position, size, colour and dim unchanged, and it falls under the pending Level-5 fine review. No ARCHITECTURE_REVIEW_GATE hard-stop category is touched.

**Dependencies.** [VIS-004](findings-visualisation.md#vis-004); [VIS-001](findings-visualisation.md#vis-001)

**Acceptance criteria.**

- With BYPASS on, a text state ('BYPASSED' or equivalent) is visible inside the editor body (graph well and/or STATISTICS header) in the Simple and Advanced views at UI scales XS..XL.
- The caption disappears within one editor tick (<= ~42 ms) of BYPASS off, and it appears for host automation of the bypass parameter and for a DAW bypass on VST3/AU.
- The pill geometry (84 px, rightmost), the pill colour 0xffd0584e and the dim 0x66090b0e below the top bar are pixel-unchanged against the current build.
- The caption never intercepts mouse input: clicks on the well pill and the STATISTICS panel behave identically with bypass on and off.

<details><summary>Verification record</summary>

**Method.** Code read at e769f33: PluginEditor.h:99-103 (DimLayer 0x66090b0e, no mouse), PluginEditor.cpp:678-680, :1427 (bounds exclude the top bar), :1968 and :2033-2035 (visibility follows only the bypass param), :36 (empty tooltip), :373-374 plus LookAndFeel.cpp:285 (red pill 0xffd0584e). Compared with Anamorph fd78c3b:src/PluginEditor.h:219-222, which has the same alpha and colour. Read DESIGN.md:846-849, DEVELOPMENT_BRIEF.md:27, :33 and :172, and BRAND_CONSISTENCY_CHECKLIST.md:51-52. Viewed ST-13 and V-04 screenshots. Reproduced on :136 with stepped clicks: took before/after captures and measured body luminance (08a/08b), then built a 15 %-scale 'glance' comparison (08-glance-off-vs-on.png). Checked the JUCE 9.0.1 wrappers (VST3, AU, AUv3, AAX) and CMakeLists formats for the host-bypass route.

**Corrections to the candidate claim.** The dim is measurable, not faint. Body mean luminance drops 25.8 -> 19.7 (-24 %) and the brightest body pixel drops 220 -> 137 (-38 %). At glance scale the two states can be told apart side by side. 'From across the room only the pill reads' is the observer's subjective judgement: it was not measured, and no evidence was found of a user missing the state. What IS established: the body carries no text saying bypassed, and every display under the dim keeps animating ([VIS-004](findings-visualisation.md#vis-004)). The dim and pill are a specified family design (DESIGN §6.1), not an accidental inheritance. Host-bypass half confirmed as not applicable. Shipped formats are VST3/AU/Standalone, and both VST3 and AU route a DAW bypass to pid::bypass, so the pill and dim do light up for a DAW bypass.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 4 · severity 2 · discoverability 2 · efficiency 1 · coherence 2 · change risk 1 · complexity 1 · evidence 3</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-009

**Monitor-state combinations and persistence are not shown: DELTA is inaudible under BYPASS, MATCH makes BYPASS loudness-matched rather than unity, MATCH and DELTA can be on together, and a session saved with DELTA on reopens monitoring only the difference**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P1** | high | partially-confirmed | State management | Listening aids and default voicing do not do what the product says | Phase 1 |

**Evidence**

- e769f33:src/dsp/AnabasisEngine.cpp:1279-1282 — the bypass endpoint outputs delayedDry whatever wetLeg (delta) holds
- e769f33:src/dsp/AnabasisEngine.cpp:1284-1287 — the monitor gain multiplies `out` AFTER the bypass mix, so the dry leg is scaled by the same attenuation as the wet (comment claims 'loudness-matched bypass')
- e769f33:docs/architecture/design-decisions/ADR-0006-ceiling-guarantee.md:164 (D8 'dry ring scaled by the same compensation, so a bypass comparison is loudness-matched by construction'); e769f33:docs/DESIGN.md:262-263; e769f33:docs/policies/DSP_POLICY.md:133-139
- e769f33:docs/user/USER_MANUAL.md:112-113 (first push step 6) and :164 (BYPASS row) promise a loudness-matched bypass
- e769f33:src/PluginParameters.cpp:413-417 — view tier {bypass, loudnessComp, deltaMonitor}: outside A/B, undo and presets; e769f33:src/PluginParameters.cpp:310-311 — ordinary parameters, so they are saved in the session root
- e769f33:src/gui/PluginEditor.cpp:590-591, 1216-1222 — plain ButtonAttachments; no other GUI reader of loudnessComp/deltaMonitor (grep); :2033-2035 — the uniform bypass dim layer
- Runtime rt/verify-7/abjump.log (probe, L=70, BYPASS toggled after 10 s): music, MATCH off: wet M -6.76 -> BYPASS -11.21 (4.45 LU). MATCH on: wet -12.45 -> BYPASS -16.94 (4.49 LU). Pink, MATCH off: -7.68 -> -14.06 (6.38 LU). MATCH on: -14.81 -> -21.31 (6.50 LU)
- Runtime rt/verify-7/bypass.log: DELTA+BYPASS output S -16.25 == BYPASS -16.25 == dry. MATCH+BYPASS -24.95 (dry -16.25, i.e. dry attenuated by 8.7 dB). MATCH+DELTA -21.21
- Runtime rt/verify-7/persist.log: after setStateInformation of a state saved with both on: deltaMonitor=1, loudnessComp=1 (root PARAM values 1.0)
- Anamorph fd78c3b:src/dsp/AnamorphEngine.cpp:1203-1221 (match gain on the processed signal) then :1337 (bypass crossfade toward the raw dry): the precedent's bypass is unity, not scaled
- Screenshots: [capture](captures/05-match-delta-states.png) (MATCH+DELTA lit, no badge); session capture `rt/state/30b-bypass-1s.png` (BYPASS dims the whole MATCH/DELTA row uniformly)

**Current behaviour.** MATCH on + BYPASS on outputs the dry signal attenuated by the full compensation gain, so the processed-vs-bypass level jump is identical to MATCH off. DELTA has no effect under BYPASS, but its toggle stays lit (dimmed with everything else). MATCH and DELTA can be on together, and the output is then the difference signal scaled by g. Both toggles persist in the session and restore lit. No indicator anywhere summarises what is actually being monitored.

**Problem.** The product's central honesty workflow (manual first push, steps 3 and 6: judge with MATCH, A/B against BYPASS 'loudness-matched too') does not do what it says. The bypass comparison under MATCH keeps the full 4-7 LU loudness advantage of the processed signal, while the user has been told the levels are matched. The secondary problems are real but smaller: the monitor state is visible only as small lit toggles, DELTA's no-op under BYPASS is not shown, and a restored DELTA is marked only by its toggle.

**Root cause.** The design error is in ADR-0006 D8 / DESIGN §2.7: g = LUFS(dry) - LUFS(wet) is applied post-mix (AnabasisEngine.cpp:1284-1287), so it scales both legs. Scaling both legs by the same gain keeps their ratio, so 'loudness-matched by construction' is false. No test pins the bypass level under MATCH: ADR-0006 D9 lists only render/null/click tests. On the UI side there is no derived 'monitoring' state. The editor only mirrors the three raw parameters.

**User impact.** A user who follows the manual toggles BYPASS with MATCH on and hears the processed signal 4-7 LU louder than the 'matched' bypass. That is exactly the louder-sounds-better bias MATCH exists to remove, so they may push Loudness further or approve a master on a biased comparison. A reopened session with DELTA on plays only the thin difference signal, with a small lit toggle and full-programme meter readings as the only clues. *Scope:* Every MATCH+BYPASS comparison in realtime playback (Simple and Advanced; also a host bypass, since the host bypass parameter IS pid::bypass, PluginProcessor.cpp:722-725). DELTA-under-BYPASS and the persistence cue affect every session that uses the monitor toggles. The offline render is unaffected (the monitor functions are inert under nonRealtime).

**Proposed improvement.** (a) Correctness: apply the MATCH gain to the wet leg only, before the bypass mix, so BYPASS plays the delay-aligned dry at unity and the matched wet sits at the dry's loudness. With MATCH off, bypass stays a bit-exact null. Keep the ~10 ms crossfades and bit-exact endpoints. Add a test that pins |LUFS(monitor, BYPASS) - LUFS(monitor, wet)| <= 1 LU with MATCH on. Amend ADR-0006 D8, DESIGN §2.7, the DSP_POLICY inv 7/12 scope text, the engine comments and the manual. (b) Visibility: give the toggle row a derived monitor-state indicator next to 'out LUFS' (Simple) and in the Advanced utility row. It shows 'MATCH -x.x dB' while matching, 'DELTA' while soloing the difference, and 'DELTA - no effect while bypassed' (DELTA toggle hatched or greyed) under BYPASS. The indicator is visible on first paint after a session restore. Keep MATCH+DELTA allowed but labelled 'DELTA (matched)'.

**Alternatives considered.**

- *Leave the engine as is and only fix the documentation (say bypass is attenuated by the compensation)* — Rejected. It documents a comparison that is useless for its stated purpose: the bypass jump is identical to MATCH off.
- *Apply 1/g to the dry leg instead (boost bypass up to the wet level) and leave the wet unattenuated* — Mathematically matched, but it boosts the dry by up to ~9 dB. That breaks ADR-0006's 'compensation never gets louder' consequence and risks monitor overload. Wet-leg attenuation is the smaller change.
- *Make MATCH and DELTA mutually exclusive (radio behaviour)* — Not justified by the evidence. The combination is coherent (matched difference); labelling is enough.
- *Hide/disable the DELTA toggle while BYPASS is on* — Acceptable variant of (b). Disabling input would lose the user's setting on un-bypass unless it is only a visual state, so a visual-only greyed state is preferred.

**Decision: Proceed · P1.** Reproduced with the product's own engine and meter: the documented loudness-matched bypass does not exist. The fix is confined to the monitor stage (the render and the offline output are untouched), and Anamorph's precedent shows the correct ordering. Because it contradicts an Accepted ADR clause (D8), it must go through the Architecture Review Gate. It does not wait on the UI indicator. The visibility half is a lower-priority P2 item and could be split out, but it shares the indicator with [VIS-010](findings-visualisation.md#vis-010)/UX-010.

*Calibration:* the verifier judged Proceed / P0; the final judgement is Proceed / P1. Challenge accepted: evidence holds (the monitor gain is applied after the bypass mix, so BYPASS plays dry*g, e769f33:src/dsp/AnabasisEngine.cpp:1279-1287) but P0 is not met: render and delivered audio are untouched and MATCH and Loudness default off, so P1 (materially harms the manual's MATCH+BYPASS workflow). Proceed with the engine half through the gate: ADR-0006 D8 amendment plus a monitor-stage signal-order change (hard stop) and DSP_POLICY inv 7 wording. Define DELTA+MATCH explicitly (applying g before the delta substitution is a second D8 change). Split the MATCH-gain readout/indicator into [VIS-010](findings-visualisation.md#vis-010)/UX-010 (P2; new audio->GUI scalar = thread-model review). Scope the <=1 LU acceptance to steady programme or land [DSP-005](findings-dsp-tech.md#dsp-005) after. Retitle to the corrected mechanism.

*Adversarial challenge:* evidence holds: yes; priority justified: no (suggested P1); decision justified: yes (suggested Proceed). Evidence and root cause hold, and the proposed ordering follows Anamorph's. Priority is overstated under the rubric. The render and the delivered audio are untouched. MATCH is off by default and Loudness defaults to 0 (PluginParameters.cpp:282, 310), so 'default-configuration' does not apply. The failure is also not silent in effect: the bypass jump under MATCH is the same 4.5-6.5 LU as an ordinary unmatched bypass, so the user is left no worse off than with any unmatched plugin, while being told otherwise. That 'materially harms an important workflow' (manual first-push steps 3-6, every MATCH+BYPASS comparison), which is P1, rather than 'seriously compromises normal product use' (P0). Severity 4, not 5. Proceed is right for the engine half, with the ADR-0006 D8 gate named as the judge did. The indicator half should be split out as its own P2 item gated on the thread-model review. *Proposal risks:* (1) 'Apply the gain to the wet leg only' is ambiguous about DELTA. Applying g to `processed` before the delta substitution (:1239-1241) turns DELTA+MATCH into dry − g·processed, a different signal. ADR-0006 D8 defines delta as dry − wet, so that is a second D8 amendment the judge did not name. Applying g to `wetLeg` after the substitution keeps today's (dry−processed)·g, and then the proposed 'DELTA (matched)' label is inaccurate: that signal is an attenuated difference, not the residue against the matched wet. The fix must pick one. (2) The 'MATCH -x.x dB' readout in part (b) needs the applied monitor gain published from the audio thread to the GUI. monitorGain and compMeasureDb are engine-private (AnabasisEngine.h:623-624), and the THREAD_MODEL.md:35 Meters→GUI row enumerates the published scalars. A new published scalar is a new cross-thread path, which is the 'Thread Model change' item (ARCHITECTURE_REVIEW_GATE.md:13), not named in gate_flags. Split (b) from (a) so the correctness fix does not wait on that gate. (3) The '<= 1 LU bypass jump' acceptance criterion depends on [DSP-005](findings-dsp-tech.md#dsp-005). After the fix, the jump equals the residual predict bias: about 0.5-0.9 LU momentary at macro L=70 in steady state, over 1 LU at programme steps (abjump: music MATCH wet dM -1.31 at the chorus edge), and 4-5.5 LU at the comp-heavy manual setting. Scope the criterion to steady pink at macro settings, or land [DSP-005](findings-dsp-tech.md#dsp-005) first. (4) DSP_POLICY inv 7's scope paragraph (amended PR #5, 2026-08-01) explicitly defends post-mix, and the flip-test comment at e769f33:tests/dsp_tests.cpp:3483-3484 relies on 'the monitor gain is post-mix'. Both need re-wording through the policy/doc lifecycle, and the flip test's bit-identity argument must be re-checked. It should still hold, because the meters take pre-monitor frames. (5) After the fix, MATCH+BYPASS output rises 5.5-8.7 dB (to unity dry) compared with today. That is expected, but the change must be noted in release notes, since existing users' bypass level changes.

**Architecture gates.**

- Conflict with Accepted ADR-0006 D8 ('Bypass's crossfade target is the dry ring scaled by the same compensation') and its Consequences bullet 'A/B loudness-matched comparison works out of the box' — ADR amendment required
- DSP signal-order change on the MONITOR stage only (compensation gain moves from post-bypass-mix to the wet leg); render path, reported latency and ceiling guarantee untouched — name it at the gate anyway
- DSP_POLICY invariant 7 and 12 scope paragraphs (post-mix compensation wording) and invariant 10 must be re-worded; invariant 10 behaviour (inert offline) must be preserved

**Dependencies.** [DSP-005](findings-dsp-tech.md#dsp-005) (the matched level itself is biased 0.6-5 dB by the predict floor; a matched bypass is only as fair as g); [VIS-010](findings-visualisation.md#vis-010) (shared monitor-state indicator / tag); [UX-010](findings-ux.md#ux-010) (same indicator carries the render/print cue)

**Acceptance criteria.**

- With MATCH on and settled (>= 4 s of pink at -12 dBFS or the harness music at -6 dB, Loudness 70), toggling BYPASS changes the monitored momentary loudness by <= 1 LU. Today it is 6.5 LU pink and 4.5 LU music.
- With MATCH off, BYPASS output is a bit-exact delay-aligned copy of the input (invariant 7 test still passes).
- Offline (nonRealtime) output with MATCH/DELTA on is bit-identical to off (testLoudnessCompensationDoesNotAlterRender, testDeltaMonitor still pass).
- A new automated test pins the matched-bypass level relation.
- With DELTA on and BYPASS on, the DELTA control visibly reads as inactive and the indicator says DELTA has no effect while bypassed.
- After restoring a session saved with DELTA on, the first painted frame shows the monitor-state indicator ('DELTA') in the Simple and Advanced views.
- USER_MANUAL steps 3-6 and the §3.1 BYPASS row describe the shipped behaviour, and ADR-0006 D8 / DESIGN §2.7 are amended to match.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/dsp/AnabasisEngine.cpp:1236-1287 (delta substitution, render tap, bypass mix, post-mix monitor gain), :668-704, e769f33:src/PluginParameters.cpp:265,310-311,413-417, e769f33:src/gui/PluginEditor.cpp:590-591,1216-1222,2033-2035, e769f33:docs/user/USER_MANUAL.md:104-113,161,164, ADR-0006 D8 (:164), DESIGN.md:262-263, DSP_POLICY.md:133-139. Viewed [capture](captures/05-match-delta-states.png) and session capture `rt/state/30b-bypass-1s.png`. Built a scratchpad probe (rt/verify-7/probe.cpp) linking the unmodified e769f33 processor objects from the audit harness build; drove processBlock (realtime flag, 48 kHz/512) with the harness 'music' (-6 dB) and pink (-12 dB) generators and measured the LISTENING output with the product's own BS.1770 LoudnessMeter. Cases: all MATCH/DELTA/BYPASS combinations (bypass.log), BYPASS toggled mid-stream with MATCH off vs on (abjump.log), getState/setState round trip with DELTA+MATCH on (persist.log). GUI runtime not re-driven: the lit-toggle/no-badge state is already in ST-15 screenshots and no GUI code reads the monitor state besides the two attachments.

**Corrections to the candidate claim.** (1) The claim 'MATCH makes BYPASS loudness-matched rather than unity' is WRONG, and the real behaviour is worse. The monitor gain is applied after the bypass mix, so the bypass leg becomes dry x g, where g is the wet-to-dry attenuation. It is not unity and not matched. Measured at L=70, the wet-to-BYPASS momentary jump is 4.45 LU (music) / 6.38 LU (pink) with MATCH off and 4.49 LU / 6.50 LU with MATCH on. MATCH leaves the bypass comparison exactly as unmatched as before, only 5.5-7.3 dB quieter. The manual (:112-113, :164), ADR-0006 D8, DESIGN §2.7 and the engine comment (:1284-1285) all claim a matched bypass. Anamorph applies its match gain to the wet before its bypass crossfade to the raw input (Anamorph fd78c3b:src/dsp/AnamorphEngine.cpp:1203-1221, 1337), so its bypass is truly matched; the Anabasis adaptation moved the gain post-mix. (2) 'DELTA is inaudible under BYPASS': more precisely, DELTA has no effect under BYPASS and the output is the plain dry (DELTA+BYPASS == BYPASS, bypass.log). The bypass dim layer (PluginEditor.cpp:2033-2035) dims the whole toggle row, which is a weak partial cue, but it dims MATCH too, and MATCH is still acting. (3) 'Nothing on screen' after reopening a session is overstated: the DELTA toggle itself is restored lit (persist.log: delta=1, match=1 after setStateInformation). There is no stronger indicator, and the meters read programme loudness ([VIS-010](findings-visualisation.md#vis-010)). (4) MATCH+DELTA outputs the difference signal scaled by g (-21.2 LUFS vs -12.6 for DELTA alone). It is not meaningless, only unlabelled.

</details>

<sub>Verifier scores (1-5): impact 5 · frequency 4 · severity 5 · discoverability 4 · efficiency 3 · coherence 5 · change risk 3 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-010

**MATCH and DELTA stay lit but are ignored in offline renders, and their automation lanes are ignored there too, with no cue**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | medium | partially-confirmed | Feedback/observability | Listening aids and default voicing do not do what the product says | Phase 1 |

**Evidence**

- e769f33:src/dsp/AnabasisEngine.cpp:668-677 — deltaTarget = deltaMonitor && !nonRealtime; compOn = loudnessComp && !nonRealtime; offline snaps monitorGain to 1 and deltaMix to 0
- e769f33:src/PluginProcessor.cpp:886-889, 904, 938 — the only switch is the host's setNonRealtime flag
- e769f33:src/PluginParameters.cpp:265, 310-311 — boolParam default automatable=true; loudnessComp and deltaMonitor use the default
- e769f33:src/gui/PluginEditor.cpp:590-591, 1216-1222 — plain ButtonAttachments, no render/print cue
- e769f33:docs/user/USER_MANUAL.md:104-108, 432-433, 495 — MATCH/DELTA described as listening aids to 'judge with'; nothing states the offline-inert / realtime-printed behaviour (only Offline Render rows at :284, 443-444, 501)
- e769f33:docs/policies/DSP_POLICY.md:94-97 — 'Both are listening-only (inert under nonRealtime ...), so no render can exceed the ceiling': true only for host-flagged offline renders
- Runtime rt/verify-7/offline.log: realtime plain S -8.84 · realtime MATCH -17.51 · realtime DELTA -12.55 · OFFLINE plain/MATCH/DELTA all -8.84 (render meter -8.84 throughout)

**Current behaviour.** In a host-flagged offline bounce, MATCH and DELTA (and any automation on them) are ignored and the full master is rendered, while the toggles stay lit. In any capture the host runs in realtime (realtime bounce, recording the output to a track), the monitor path is printed: an attenuated master with MATCH, the difference signal with DELTA. Nothing in the UI or manual says either.

**Problem.** The toggles look and automate like processing parameters, but what they do in an export depends on the host's offline flag, which the user cannot see. The manual tells users to judge with MATCH on, so it is often still on at print time.

**Root cause.** This is by design (ADR-0006 D6, DSP_POLICY inv 10): monitoring must not reach the render, and the render is identified only by the host's nonRealtime flag. The view-tier parameters stay ordinary automatable parameters, and the monitor state has no UI or documentation layer.

**User impact.** A realtime print made with MATCH left on captures a master around 8-9 dB quieter than the one the plugin's meters report. The pass is wasted, or a wrong file is delivered if nobody checks. With DELTA on, the difference signal is printed, and it can exceed the ceiling (DSP_POLICY inv 4 scope). An offline 'print the delta' bounce silently yields the full master. An automated MATCH lane is ignored offline. *Scope:* Every export while MATCH or DELTA is on. The realtime-print variant depends on the host and workflow (realtime bounce or record-to-track print), not verified in a DAW. The offline variant affects anyone trying to bounce the delta or relying on MATCH automation.

**Proposed improvement.** Keep the DSP behaviour. Add (1) a persistent monitor-state indicator whenever MATCH or DELTA is on (shared with [UX-009](findings-ux.md#ux-009)/VIS-010), with the tooltip/manual text 'listening aid — not in offline bounces; captured by realtime prints/recordings'. (2) When the host playhead reports isRecording while MATCH or DELTA is on, switch that indicator to a warning style ('MONITOR ON — being recorded'). (3) A manual paragraph in §2.4/§3.2 and the FAQ stating both behaviours, with the correction to DSP_POLICY inv 4's 'no render can exceed the ceiling' wording scoped to host-flagged offline renders.

**Alternatives considered.**

- *Make loudnessComp/deltaMonitor non-automatable* — Removes the automation half of the confusion, but it changes the parameter surface's automatable flags (ADR-0010 territory) for a rare use. Not justified now.
- *Let MATCH/DELTA apply offline too* — Rejected: violates DSP_POLICY invariant 10 and ADR-0006 D6.
- *Auto-disable MATCH/DELTA when the host starts recording* — Silently changing a user toggle is a worse surprise, and isRecording is not reliable across hosts. A warning is sufficient.
- *Documentation only* — Necessary but not sufficient: the trap happens at print time, when nobody is reading the manual.

**Decision: Modify · P2.** The code behaviour is intentional and must be preserved, but it is invisible, and in realtime prints it reaches the delivered audio. The fix is UI plus documentation only, with no gate impact. P1 because MATCH-on is the recommended judging state and a realtime print pass is a common mastering step; the recovery is a re-print. Host-side confirmation in a DAW is still owed.

*Calibration:* the verifier judged Proceed / P1; the final judgement is Modify / P2. Challenge accepted: the dominant export path (host offline bounce) yields the correct master by design (ADR-0006 D6); realtime-capture reach is JUCE-semantics inference, not host-measured, and an 8.7 dB print error is exposed by any post-print check, so P2. Modify: ship the shared persistent monitor-state indicator (with [UX-009](findings-ux.md#ux-009)/VIS-010) plus manual and tooltip text; defer the isRecording warning (new playhead read and audio->GUI flag = thread-model gate, unreliable across hosts); leave DSP_POLICY inv 4 wording alone or take it through the ceiling-guarantee gate. Raise to P1 if [TEST-002](findings-doc-test.md#test-002)'s DAW check shows realtime printing is a common path.

*Adversarial challenge:* evidence holds: yes; priority justified: no (suggested P2); decision justified: no (suggested Modify). The mechanism is real, but the P1 rationale relies on an unverified host behaviour and an unevidenced frequency ('a realtime print pass is a common mastering step'). The dominant DAW export path, the host offline bounce, yields the correct master, which is the intended behaviour. The realtime-print trap produces a gross 8.7 dB level error that the DAW's own meter or any post-print loudness check exposes, and recovery is a re-print. That fits 'meaningful improvement ... in a less frequent or lower-cost situation' (P2) until a DAW check shows realtime capture is a common path. Raise to P1 if that check confirms it. Modify rather than Proceed: ship the shared persistent monitor-state indicator (with [UX-009](findings-ux.md#ux-009)) and the manual/tooltip text, defer the isRecording warning, and either drop the inv-4 rewording or run it through the ceiling-guarantee gate. *Proposal risks:* (1) Part (2), the isRecording warning, adds a playhead read on the audio thread and a new audio→GUI published flag. The product reads no playhead today (no getPlayHead or isRecording in src/). That is a new cross-thread path, the 'Thread Model change' gate item (ARCHITECTURE_REVIEW_GATE.md:13), and gate_flags is empty. The finding's own alternatives call isRecording 'not reliable across hosts'. A realtime bounce in many hosts does not arm transport recording, so the warning misses exactly the case it targets and gives false assurance. Defer part (2). (2) Part (3), re-scoping DSP_POLICY inv 4's 'no render can exceed the ceiling', edits the ceiling-guarantee text. ARCHITECTURE_REVIEW_GATE.md:22 ('Ceiling guarantee change — anything that weakens ...') and inv 4's own 'weakening it is an Architecture Review Gate item' make this gated even as a clarification. This is a missed gate flag. The smaller route: leave inv 4 alone (its 'render' already means nonRealtime) and state the realtime-capture caveat in USER_MANUAL/FAQ and the tooltip only. (3) The acceptance criterion 'Manual DAW check recorded' is the evidence the priority depends on, so it should come before the priority is set, not after the fix. (4) Making the two parameters non-automatable remains correctly rejected: ADR-0010 requires a kVersion bump + ADR.

**Dependencies.** [UX-009](findings-ux.md#ux-009) (shared monitor-state indicator); [VIS-010](findings-visualisation.md#vis-010) (same tag area)

**Acceptance criteria.**

- With MATCH or DELTA on, a monitor-state indicator is visible in both views without hovering.
- With MATCH or DELTA on and the host playhead reporting isRecording=true, the indicator shows a distinct warning state within one UI refresh.
- USER_MANUAL states that MATCH/DELTA are inert in offline bounces and are captured by realtime bounces/recordings.
- DSP_POLICY inv 4 scope text says the 'no render can exceed the ceiling' property holds for host-flagged offline renders.
- Offline output with MATCH/DELTA on stays bit-identical to off (existing tests pass).
- Manual DAW check recorded: a realtime bounce with MATCH on in at least one host is measured and its level matches the documented behaviour.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/dsp/AnabasisEngine.cpp:668-677, e769f33:src/PluginProcessor.cpp:886-889,904,938 (nonRealtime flag plumbing), e769f33:src/PluginParameters.cpp:265 (boolParam automatable default true), :310-311, e769f33:src/gui/PluginEditor.cpp:590-591,1216-1222, DSP_POLICY.md:88-97 and :253-257, and grepped USER_MANUAL.md for render/bounce/offline/MATCH/DELTA. Probe (rt/verify-7/offline.log): the real processor with setNonRealtime(true) vs (false) at L=70 on music -6 dB, with MATCH or DELTA on, measuring the plugin's output buffer with the product's LoudnessMeter. Not tested in a real DAW.

**Corrections to the candidate claim.** The offline claim is confirmed: OFFLINE MATCH and OFFLINE DELTA outputs equal the plain master (S -8.84 in all three). The finding misses the opposite and riskier case. The guard is only `! p.nonRealtime`, so any capture the host does not flag as offline gets the monitor path: a realtime bounce, or recording the plugin output to a track, which is the common 'print' pass in mastering. Realtime MATCH output measured -17.51 LUFS against a -8.84 master; realtime DELTA output -12.55. So MATCH or DELTA left on is printed into such a capture, while the plugin's own 'out LUFS' keeps showing -8.8. That the host flags realtime prints as realtime is inferred from JUCE setNonRealtime semantics, not tested in a DAW. Automation of the two parameters is confirmed possible (automatable=true).

</details>

<sub>Verifier scores (1-5): impact 4 · frequency 3 · severity 4 · discoverability 4 · efficiency 3 · coherence 3 · change risk 1 · complexity 2 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-011

**The inactive A/B slot is a black box: its preset name, edited state and history are invisible until switched to, and after Copy both slots carry the same label**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | confirmed | State management | The tier and undo model is principled in code but invisible at the point of action | Phase 4 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:241-272 — ABControl::paint draws only 'A / B', active letter = colours::accent
- e769f33:src/gui/PluginEditor.cpp:318-327 — onToggle = switchToSlot + repaint + refreshPresetDisplay(true); tooltip 'A/B Compare'
- e769f33:src/gui/PluginEditor.cpp:2144-2171 — refreshPresetDisplay reads only currentPresetName()/presetDirty() of the live slot
- e769f33:src/PluginProcessor.cpp:1219-1241 — the SLOT carries presetName + presetSource/presetFactoryId/presetUserFile
- e769f33:src/PluginProcessor.cpp:425-434 — Copy: storedSlot = saveSlotFromLive(); storedPresetBaseline = presetBaseline.createCopy()
- e769f33:docs/user/USER_MANUAL.md:411-418 — 'Each slot keeps its own preset name, edited state and undo history'
- VER8-2 (fresh instance, A edited then first A/B): session capture `rt/verify-8/v02-A-edited-top.png` ('Default *') and session capture `rt/verify-8/v04-after-ab1-top.png` ('Default', B accented)
- VER8-4 (after Copy, B reads the same as A): session capture `rt/verify-8/v12-top.png`
- VER8-6 (A/B tooltip is 'A/B Compare' only): session capture `rt/verify-8/v17-ab-tooltip-crop.png`
- ST-07/ST-08: session capture `rt/state/18-ab-topbars.png`, session capture `rt/state/19-copy-topbars.png`
- Sibling parity: Anamorph e769f33:src/PluginEditor.h:224-236 (identical ABControl)

**Current behaviour.** The top bar shows one preset name, the live slot's with its ' *'. The A/B pill shows two letters and accents the active one. The inactive slot's name, its edited state, and whether it equals the live slot cannot be seen without switching. Switching costs a forced duck ([DSP-002](findings-dsp-tech.md#dsp-002)). The pill's tooltip, which is off by default, says only 'A/B Compare'.

**Problem.** The manual presents A/B as two independent setups, each with its own name and edited state, but the UI only ever shows one of them. The user cannot tell what B holds, or whether B already equals A (for example after Copy), without auditioning it. On a fresh instance the two labels differ only by the star.

**Root cause.** ABControl is a stateless two-letter toggle ported verbatim from Anamorph. The editor has one name field bound to the live slot. The processor exposes no accessor for the stored slot's name, identity or dirty state, and no A=B equivalence query. storedSlot and storedPresetBaseline are private and read only by the swap and Copy paths.

**User impact.** In a compare pass the user presses A/B just to find out what the other slot is. Each press is an audible dip, and on a fresh instance it lands on an unrelated Default patch. The user may also not notice that the two slots are already identical (after Copy), and so 'compare' two identical sounds. *Scope:* Every A/B comparison in both the Simple and the Advanced view. It is display-only: no DSP, no serialization, no parameter change.

**Proposed improvement.** 1. Give the inactive slot a readout that does not depend on the tooltip switch. While the pointer is over the A/B pill, the preset-name field shows a dimmed preview of the other slot, for example 'B: Default' or 'B: Loud Pop *'. It returns to the live name on exit. No click, no switch, no duck.
2. Mark equality in the pill itself. When the two slots are equivalent, the inactive letter shows a small '=' mark or a linked glyph. The comparison is the stripped slot compare the Copy guard already uses (e769f33:src/PluginProcessor.cpp:397-424). It runs on the message thread, on change events (edit end, Copy, switch, preset apply, undo) or at the existing ~3 Hz dirty-poll cadence, never at 24 Hz, because saveSlotFromLive() takes the APVTS lock (KI-011).
3. Processor additions: read-only accessors storedPresetName(), storedPresetDirty() and slotsEquivalent().

**Alternatives considered.**

- *Leave as is (industry-common two-letter toggle; the manual tells users to Copy first)* — Rejected. The manual's own claim of per-slot names and edited state has no visible counterpart, and every probe costs a duck.
- *Dynamic tooltip on the pill ('B: Default *, differs from A')* — Cheap but weak: tooltips are off by default (e769f33:docs/user/USER_MANUAL.md:150-151). Worth doing in addition, not instead.
- *Permanent secondary label under the preset name ('B: Default')* — Always visible, but it needs top-bar height or width that the sibling geometry (kBarH, brand checklist §A 'Overall frame layout') does not have. Hover preview gets most of the value at no layout cost.
- *Split the pill into two independently clickable segments showing both names* — Too wide for the 46 px pill and changes the family interaction model (brand checklist §A).

**Decision: Proceed · P2.** The behaviour is fully confirmed in code and at runtime. The fix is display-only with no hard-stop gate, and it serves the manual's recommended A/B + Copy workflow directly. The one constraint is family parity: the brand checklist requires the same A/B interaction model as Anamorph, so the change should be proposed for both products, or recorded as a deliberate divergence.

**Architecture gates.**

- No hard-stop category touched: display-only, no parameter/serialization/threading/DSP/latency change
- Product-family item, not a hard stop: e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:55-56 'A/B compare — the same interaction model, the same slot semantics' (§A must-match). Port to Anamorph or record the divergence (ADR-0009 family rule)
- KI-011 lock-cost constraint: the equivalence query must not run at the 24 Hz tick (it reaches apvts.copyState)

**Dependencies.** [UX-012](findings-ux.md#ux-012) (Copy acknowledgement should update the same readout); [DSP-002](findings-dsp-tech.md#dsp-002) (each blind probe press costs a duck; the equality query is the same one a no-op-switch skip would use); [STATE-013](findings-state-model.md#state-013) (a fresh B = Default becomes visible instead of surprising)

**Acceptance criteria.**

- With slot A active and slot B holding preset X (edited), hovering the A/B pill shows 'B: X *' in the preset-name field within 100 ms, without Tooltips enabled; moving away restores the live name; no duck is requested (engine duck flag untouched)
- Immediately after Copy, the pill shows the equality mark; editing any sound parameter in either slot removes it within ≤ 400 ms
- On a fresh instance with A edited, hovering the pill shows 'B: Default' (no star)
- The equivalence check never runs from the 24 Hz tick (asserted by counting calls in a test or by code review)
- The same behaviour exists in the Advanced view; Anamorph parity is either ported or recorded in an ADR

<details><summary>Verification record</summary>

**Method.** Read ABControl::paint (e769f33:src/gui/PluginEditor.cpp:241-272; it draws only 'A', '/' and 'B', with the active letter accented), the onToggle wiring (:318-327) and refreshPresetDisplay (:2144-2171; its only source is proc.currentPresetName() plus presetDirty()). Read copySlotToOther (e769f33:src/PluginProcessor.cpp:391-435) and saveSlotFromLive (:1219-1241): presetName and the ADR-0022 identity trio travel in the SLOT, and the baseline is copied at :434. Reproduced on :138 with stepped motion. On a fresh instance I dragged Loudness to 34 %, so A read 'Default *'. The first A/B then read 'Default', with only the star to tell them apart (VER8-2). After Copy both slots read 'Default *' (VER8-4). With tooltips on, the pill's tooltip reads only 'A/B Compare' (VER8-6). Compared with the sibling: Anamorph's ABControl is the same struct (Anamorph e769f33:src/PluginEditor.h:224-236, 'a single click toggles (FabFilter-style)').

**Corrections to the candidate claim.** 'History invisible' is accurate but not a meaningful gap: per-slot undo shows up in the undo/redo enablement once the slot is active, and a readout of the other slot's history has no workflow use. The label is identical after Copy because the slots really are identical, so the label is not wrong. The defect is that there is no readout of the inactive slot at all. On a fresh instance the labels are already near-identical before any Copy ('Default *' vs 'Default'), which makes the problem slightly worse than the finding states.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 4 · severity 2 · discoverability 4 · efficiency 3 · coherence 3 · change risk 2 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-012

**Copy does not say which way it copies, gives no acknowledgement and silently overwrites the other slot; the only recovery is to switch slots and undo there**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | confirmed | Feedback/observability | The tier and undo model is principled in code but invisible at the point of action | Phase 4 |

**Evidence**

- e769f33:src/gui/PluginEditor.h:452 — `juce::TextButton copyButton { "Copy" };`
- e769f33:src/gui/PluginEditor.cpp:329-332 — tooltip 'Copy the current settings into the other A/B slot.'; onClick = proc.copySlotToOther()
- e769f33:src/gui/PluginEditor.cpp:1417 — Copy is 46 px wide in the top-bar layout
- e769f33:src/PluginProcessor.cpp:391-435 — live→inactive; destination undo entry only when it changes
- e769f33:docs/architecture/design-decisions/ADR-0018-copy-and-advanced-join-the-undo-history.md:60-66 — Copy is a destination-slot undo step; no duck
- e769f33:docs/architecture/design-decisions/ADR-0007-state-schema-v1.md:119 — undo/redo stacks not serialized
- VER8-3 (hover → pressed → +0.05/+0.3/+1 s → away, no acknowledgement): session capture `rt/verify-8/v05-11-copy-sequence.png`
- VER8-4 (B after Copy = A's 'Default *'/34 %): session capture `rt/verify-8/v12-top.png`, session capture `rt/verify-8/v12-knob.png`
- VER8-5 (tooltip text, Tooltips enabled): session capture `rt/verify-8/v18-copy-tooltip-crop.png`
- ST-08: session capture `rt/state/19a-copy-immediate.png`, session capture `rt/state/19-copy-topbars.png`

**Current behaviour.** A 46 px button labelled 'Copy' sits next to the A/B pill. Clicking it overwrites the inactive slot with the live slot: sound, name, identity and dirty datum. Nothing on screen changes: the active slot is unchanged, the inactive slot is invisible ([UX-011](findings-ux.md#ux-011)), and the button returns to its hover shading. The pre-copy destination can be restored only by switching to it and pressing Undo, and only within the current session.

**Problem.** The control carries no direction, so A-to-B and B-to-A look identical, and it gives no confirmation that anything happened. The destructive half of the action, overwriting a slot the user may have built by hand, happens entirely off screen.

**Root cause.** A plain family TextButton with a static label and no post-action state. The destination is not rendered anywhere ([UX-011](findings-ux.md#ux-011)), so there is nothing that could visibly change.

**User impact.** A user who built an alternative in B and presses Copy while in A, meaning to 'save' A, loses B without noticing. They find out only on the next A/B press, and can recover only if they know about the destination-slot undo and have not reloaded the session in between. *Scope:* Every Copy press, in both views. Display and label only; the ADR-0018 semantics are unchanged.

**Proposed improvement.** Keep Copy's semantics and add direction and acknowledgement.
1. Show the direction on the control. While Copy is hovered or focused, the A/B pill draws an arrow from the active letter to the inactive one. The button label becomes direction-bearing: 'A→B' or 'B→A', or 'Copy→B' if the button is widened from 46 to ~64 px at e769f33:src/gui/PluginEditor.cpp:1417, with the preset-name field absorbing the difference.
2. Acknowledge a Copy that changed the destination:
   • the destination letter in the pill pulses for ~0.5 s (a static 0.5 s accent when UI Animations are off);
   • if [UX-011](findings-ux.md#ux-011) lands, the inactive-slot readout and equality mark update at once.
   A no-op Copy gives a neutral 'already identical' cue instead.
3. Extend the tooltip to name the recovery: 'Copy slot A into slot B — Undo in B reverts it.'

**Alternatives considered.**

- *Confirmation dialog before overwriting* — Rejected: friction on a frequent action, and ADR-0018 already makes it undoable.
- *Separate A→B and B→A buttons* — Rejected: copying into the ACTIVE slot is a different operation (it replaces what you are hearing) and doubles the controls.
- *Leave as is (manual §3.1/§7.4 state direction and recovery)* — Rejected: the only on-screen statement is behind a default-off switch, and nothing on screen acknowledges a destructive action.
- *Label-only change ('A→B')* — Acceptable minimal version, but it loses the verb, so it can read as 'switch to B'. The hover arrow on the pill avoids that.

**Decision: Proceed · P2.** Confirmed at runtime and in code. The fix is presentational, keeps ADR-0018 semantics, and removes the silent-overwrite trap at small cost. Priority is P2, not P1: a recovery path exists (destination undo), the manual documents direction and recovery, and the trap needs a specific mistaken intent. The recovery path is lost after a session reload, which is why this is worth doing now.

**Architecture gates.**

- No hard-stop category touched: label/paint only; ADR-0018 Copy semantics and undo placement unchanged
- Product-family item, not a hard stop: e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:55-56 (A/B 'same interaction model') — Anamorph uses the identical 'Copy' button and tooltip (Anamorph e769f33:src/PluginEditor.cpp:344), so port or record the divergence
- Top-bar geometry is sibling-derived (e769f33:src/gui/PluginEditor.cpp:1405-1424); widening Copy touches the brand checklist §A 'Overall frame layout'

**Dependencies.** [UX-011](findings-ux.md#ux-011) (the acknowledgement is strongest when the destination slot has a visible readout)

**Acceptance criteria.**

- With A active, the Copy control shows its direction on screen (label or hover arrow) without Tooltips enabled; with B active it shows the reverse
- A Copy that changes the destination produces a visible cue on the destination letter within 100 ms, lasting ≥ 300 ms; with UI Animations off the cue is static but still present
- A Copy that changes nothing (second press) shows no overwrite cue and pushes no undo entry (existing guard preserved)
- Tooltip names the destination and the Undo-in-destination recovery
- ADR-0018 tests (testAbSlotsAndTiers ADR-0018 checks, copy undo cases) pass unchanged

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/PluginEditor.h:452 (TextButton 'Copy') and e769f33:src/gui/PluginEditor.cpp:329-332: onClick = proc.copySlotToOther(), with no repaint, pulse or state change. Read e769f33:src/PluginProcessor.cpp:391-435 (always live to inactive; the destination's pre-copy state is pushed on the destination's stack only if it differs) and ADR-0018:60-66. Reproduced on :138. Stepped approach to Copy, then mousedown and mouseup, captured at press, +0.05 s, +0.3 s and +1 s, and after moving away. The only change is the family hover and pressed shading; nothing changes after release (VER8-3). Switching to B then showed A's 'Default *' at 34 % (VER8-4). With tooltips on, the tooltip reads 'Copy the current settings into the other A/B slot' (VER8-5).

**Corrections to the candidate claim.** The direction is deterministic (current to other) and is stated in the manual (e769f33:docs/user/USER_MANUAL.md:160, 413-417) and in the tooltip. The tooltip is invisible by default because Tooltips are off by default (:150-151), so on screen the direction is still unstated. A Copy that would not change the destination pushes nothing and leaves redo intact (e769f33:src/PluginProcessor.cpp:397-424), so a repeated Copy is harmless. One aggravating detail the finding misses: undo stacks are not serialized (e769f33:docs/architecture/design-decisions/ADR-0007-state-schema-v1.md:119). A Copy that is saved with the session and reopened cannot be undone at all.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 3 · severity 3 · discoverability 3 · efficiency 2 · coherence 2 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-013

**Controls are never marked inactive when another control overrides them. In the default patch both Release knobs are inert because AUTO is on. SHAPE is inert at Dither Off, Phase at Oversampling Off with Follow Online, and Character plus the Color knobs under the Clean model. Both AUTO pills sit in the column opposite their Release knob.**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | partially-confirmed | Interaction model | Controls do not show whether they are live or what they select, and the explanation sits in tooltips that ship off | Phase 5 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:2134-2141 — the only setEnabled calls in the editor, for Undo/Redo
- e769f33:src/gui/PluginEditor.cpp:522,526 — compRelease knob and COMP AUTO; :543,546 — limRelease knob and LIMITER AUTO
- e769f33:src/gui/PluginEditor.cpp:579,585 — Dither combo and SHAPE toggle, bound directly with no cross-parameter logic
- e769f33:src/gui/PluginEditor.cpp:1565,1573-1574 — COMP Release in row 2; AUTO in the left half of the foot row
- e769f33:src/gui/PluginEditor.cpp:1603,1605-1607 — LIMITER Release in row 2 (right column); AUTO in the left half of the foot row, TP in the right half
- e769f33:src/PluginParameters.cpp:324,368 — compAutoRelease and limAutoRelease default to true
- e769f33:src/dsp/MasteringComp.h:132-133,272-282 — compReleaseMs is used only on the manual path
- e769f33:src/dsp/LookaheadLimiter.h:146-147,289-301 — aRelManual is used only when autoRelease is false
- e769f33:src/dsp/AnabasisEngine.cpp:1081-1084,1201-1214 — shaping acts only inside the ditherOn branch
- e769f33:src/dsp/AnabasisEngine.cpp:352-354,423-424 — phase is compared only when an effective factor is latched
- e769f33:src/dsp/Latency.h:96-105 — Force Max makes the offline effective factor 16x
- e769f33:src/dsp/AnabasisEngine.cpp:656 — setTruePeakMode(p.truePeakMode && osN < 4)
- e769f33:src/gui/PluginEditor.cpp:2102 — the Ceiling unit follows truePeakMode (ADR-0015)
- e769f33:src/dsp/ClipSat.h:282 — the colour residue is gated on dep > 0 && model != 0; e769f33:src/MacroEngine.h:57 — Character drives only colourDepth
- e769f33:src/PresetManager.cpp:152-155,195-198 — Transparent Master and Classical Dynamics set colourModel to Clean
- e769f33:src/gui/LookAndFeel.cpp:456,758 — only icon text buttons and labels render a disabled state; knobs, toggles and combos have none
- e769f33:src/gui/PluginEditor.cpp:55,71 — Release tooltips make no mention of AUTO
- LAY-15/LAY-17: COMP AUTO pill at (79,487), Release at (216,260) — session capture `rt/layout/04c-comp.png`, [capture](captures/02-advanced-view.png)
- verify-9 repro, AUTO on vs off with an identical Release knob: session capture `rt/verify-9/02-comp-auto-on-vs-off.png`
- verify-9 repro, SHAPE identical at Dither Off and 16-bit: session capture `rt/verify-9/04-dither-off-vs-16-shape.png`
- verify-9 repro, TP off vs on at 4x changes only the unit: session capture `rt/verify-9/11c-limiter-tp-off-vs-on-4x.png`
- verify-9 repro, Release tooltip under AUTO: session capture `rt/verify-9/12c.png`
- verify-9 repro, Phase Linear at Oversampling Off gives latency=480 (app.log), shown in session capture `rt/verify-9/07c.png`

**Current behaviour.** Every widget renders the same whether or not its parameter currently acts.

In the default patch in the Advanced view:
- COMP Release (200 ms) and LIMITER Release (100 ms) are drawn with full orange arcs, but both AUTO toggles are On, so neither knob affects the audio.
- SHAPE looks like any other off toggle while Dither is Off, where it has no effect.
- In Settings, Phase can be switched at Oversampling Off with Follow Online, and nothing happens: no latency change, no duck.

After loading Transparent Master or Classical Dynamics (Clean model), the Simple view Character knob and the Advanced Color Depth, Odd/Even and Color Tone knobs are inert with no cue.

At 4x and above, TP changes only the Ceiling's unit text.

With tooltips enabled, the Release tooltips still say 'How quickly the ... recovers'.

**Problem.** The UI misreports which controls are live.

A user who turns an inert Release knob hears nothing and gets no explanation. They either distrust the plugin or build a wrong model of the chain.

Presets and sessions then store Release values the user believes are shaping the sound.

Each AUTO governor sits in the opposite column, 1-2 rows away from the knob it overrides, so the cause is not visually adjacent to the effect.

**Root cause.** - Widgets are attached 1:1 to APVTS parameters. The editor has no derived 'is this parameter effective' state, and nothing re-renders a control when its governor changes.
- The look-and-feel has no inactive or disabled rendering for rotary sliders, toggles or combos (LookAndFeel.cpp:456,758 are the only isEnabled reads).
- On the DSP side, AUTO replaces the manual release outright; it does not scale it. The auto poles are fixed constants, scaled only by the adaptive trim (ADR-0013). Some engineers may expect base-plus-auto behaviour instead.
- The 0.1.1 layout moved all toggles into half-width foot rows (PluginEditor.cpp:1538-1543 comment). AUTO took the left half there, away from Release.

**User impact.** Every Advanced-view session starts with two inert, fully drawn Release knobs.

Engineers tuning release, a routine mastering adjustment, waste gestures and may conclude that release has no audible effect on the material. They may also save presets whose Release values mislead later readers.

The smaller traps (SHAPE, Phase, Character under Clean) cost an A/B attempt each.

Nothing produces wrong output and nothing destroys work. *Scope:* - Advanced view: COMP panel (Release/AUTO), LIMITER panel (Release/AUTO; TP at 4x and above as a special case), CLIP/COLOR panel (Color Depth, Odd/Even, Color Tone under Clean), utility row (SHAPE/Dither).
- Simple view: Character under Clean, reached via 2 of the 13 factory presets or a user choice.
- Settings: Phase, dependent on Oversampling and Offline Render. Also Offline Render = Force Max is inert when Oversampling is already 16x.
- Default patch: 3 inert Advanced controls plus Phase in Settings.

**Proposed improvement.** 1. Add one pure, message-thread function isEffective(paramId, state). Evaluate it on the existing 24 Hz tick or on parameterChanged. It covers discrete governors only:
   - compRelease: effective only when compAutoRelease is Off
   - limRelease: effective only when limAutoRelease is Off
   - ditherShaping: effective only when dither is not Off
   - osPhase: effective when oversample is not Off, or offlineQuality is Force Max
   - colourDepth, colourBalance, colourTone and the Simple-view Character: effective only when colourModel is not Clean
2. Render ineffective controls dimmed but live. Draw the arc, pointer, value text and pill at roughly 40% alpha and keep the caption legible. The control stays interactive: do not use setEnabled(false), so users can pre-set a value, double-click reset works, and automation stays visible.
3. Append a clause to the tooltip, such as ' - inactive while AUTO is on' or ' - needs a Color model other than Clean'.
4. Never dim TP. At 4x and above, extend its tooltip and the manual: 'at 4x and above detection is already true-peak; TP then sets the Ceiling unit'.
5. Move each AUTO pill into the Release knob's column, directly under the Release cell or as a compact pill beside its value, in both COMP and LIMITER.
6. Document in USER_MANUAL §3.3 that AUTO replaces the Release setting.
7. Continuous-zero cases (Clip Shape at Drive 0, Color knobs at Depth 0) are out of scope. Dimming them would flicker during macro moves.

**Alternatives considered.**

- *Leave as-is* — Rejected. The default patch shows two inert, fully drawn Release knobs, and tooltips ship off ([UX-006](findings-ux.md#ux-006)), so nothing in the product explains it.
- *setEnabled(false) on dependent controls* — Rejected. JUCE disabled components ignore the mouse, which blocks pre-setting Release before switching AUTO off and blocks double-click reset. It also needs new disabled rendering anyway.
- *Hide dependent controls* — Rejected. It causes layout jumps and hides stored values that presets carry.
- *Change DSP so Release scales the AUTO poles (base-plus-auto convention)* — This would make the knob meaningful, but it changes the sound of every existing session with AUTO on. It touches ADR-0013's definition of the AUTO poles and parameter-compatibility expectations. It is out of scope for a UI fix.
- *Tooltip text only* — Insufficient on its own because tooltips default off. It is kept as a complement to the dimming.

**Decision: Modify · P2.** The defect is real, reproduced and code-confirmed, and it affects the default patch. The obvious fixes, disabling controls or changing the DSP, carry interaction or compatibility costs.

A view-only, dim-but-live treatment limited to discrete governors, with TP excluded and the AUTO pills moved into the Release column, solves the misreport without touching parameters, serialization, DSP or latency. No Accepted ADR is contradicted. ADR-0010 option N already treats Character being inert under Clean as a known cost.

The layout change overrides an owner-directed 0.1.1 arrangement, so it needs owner review, but it is not a hard-stop gate.

Priority is P2, not P1: the situation is frequent, but each occurrence is low cost. There is no wrong output or lost work, and recovery is one click once understood.

**Dependencies.** [UX-006](findings-ux.md#ux-006); [UX-014](findings-ux.md#ux-014); [MODEL-004](findings-state-model.md#model-004)

**Acceptance criteria.**

- Default patch, Advanced view: COMP Release and LIMITER Release render in the inactive style. Switching the matching AUTO off restores full rendering within one tick, so a pixel diff of the Release cell between AUTO on and off is non-empty.
- An inactive Release knob still accepts drag, wheel, double-click reset and host automation, and its value text updates. Its tooltip ends with a clause naming AUTO.
- SHAPE renders inactive at Dither Off and active at 16-bit and 24-bit.
- The Settings Phase row renders inactive exactly when Oversampling is Off and Offline Render is Follow Online, and active in every other combination.
- With Color = Clean: Color Depth, Odd/Even and Color Tone (Advanced) and Character (Simple) render inactive. After loading 'Transparent Master', the Simple-view Character knob is visibly inactive.
- TP is never dimmed. At Oversampling 4x and above, its tooltip states that detection is already true-peak and that TP sets the Ceiling unit.
- In both COMP and LIMITER, each AUTO pill is in the same column as its Release knob and vertically adjacent to it, at every UI scale from XS to XL.
- A headless state test asserts isEffective for every governor combination listed.
- USER_MANUAL §3.3 states that AUTO replaces the Release setting.

<details><summary>Verification record</summary>

**Method.** Code: read every cited anchor at e769f33, plus the DSP consumers MasteringComp.h, LookaheadLimiter.h, ClipSat.h, Latency.h and MacroEngine.h, and LookAndFeel.cpp's isEnabled handling.

Screenshots: viewed session capture `rt/layout/04c-comp.png` and [capture](captures/02-advanced-view.png).

Runtime reproduction on :139, harness in Advanced view, stepped pointer motion before every click:
- COMP AUTO On to Off: a pixel diff of the Release knob cell (180,225)-(252,320) is empty.
- Dither Off to 16-bit: a pixel diff of the SHAPE pill is empty.
- Phase set to Linear at Oversampling Off: latency stays 480 samples.
- Oversampling 4x Linear: latency 541.
- TP On at 4x: the only change is the Ceiling text, '-0.10 dB' to '-0.10 dBTP'.
- With tooltips on and AUTO on, hovering LIMITER Release shows 'How quickly the limiter recovers'.

**Corrections to the candidate claim.** - Anchor fix: :526 and :546 set up the AUTO toggles. The Release knobs are at :522 and :543.
- Scope is larger than stated. Both AUTO toggles default to On (PluginParameters.cpp:324, 368), so in the untouched default patch COMP Release (200 ms) and LIMITER Release (100 ms) are fully drawn and do nothing.
- The layout problem affects the LIMITER as well as COMP. Both AUTO pills sit in the LEFT half of the foot row, while both Release knobs are in the RIGHT column.
- TP at 4x and above has no DSP effect but is not inert overall. It switches the Ceiling unit between dB and dBTP (ADR-0015), so it must not be dimmed.
- Phase at Oversampling Off is inert only when Offline Render is Follow Online. With Force Max, renders run at 16x using the selected phase (Latency.h:96-105; AnabasisEngine.cpp:352-354).
- Under Clean, Character (Simple view) and Color Depth, Odd/Even and Color Tone (Advanced view) are all inert. The Color model combo that explains this exists only in the Advanced view.
- 'No dedicated dimming check was run' no longer applies: the checks above were run.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 4 · severity 3 · discoverability 4 · efficiency 2 · coherence 3 · change risk 2 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-014

**Settings explain their consequences only in tooltips, which ship off: there is no latency readout, the Phase row does not show that it needs Oversampling or Force Max, and both the tooltip and the manual say Linear adds latency even where it adds none**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | partially-confirmed | Feedback/observability | Controls do not show whether they are live or what they select, and the explanation sits in tooltips that ship off | Phase 5 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:759-782 — the Settings combos and their tooltips; :763 comment citing DESIGN §6.4; :766 Phase tooltip 'Linear ... adds latency (reported to the host)'
- e769f33:src/gui/PluginEditor.cpp:1453,1456-1473 — the 380x350 panel with label and combo rows and no helper or status line
- e769f33:src/InternalState.h:106-110 — Oversampling defaults to Off, Phase to Minimum, Offline Render to Follow, tooltips to off
- e769f33:src/dsp/Latency.h:57-58,62-67,96-105 — OS latency of 4/6/6/6 (Minimum) and 49/61/65/67 (Linear) samples; the Force Max effective factor is 16x
- e769f33:src/dsp/AnabasisEngine.cpp:423-424 — phase is compared only when a factor is latched
- e769f33:docs/user/USER_MANUAL.md:283 — 'linear phase = ... more latency', with no mention that it needs Oversampling or Force Max
- e769f33:docs/DESIGN.md:936-938 — §6.4 puts the latency note in the tooltip
- ST-11: latency at 48 kHz, Minimum 480/484/486/486/486 and Linear 480/529/541/545/547 — session capture `rt/state/21a-settings.png`, session capture `rt/state/23a-crop.png`, session capture `rt/state/26b-crop.png`
- verify-9 repro: Phase Linear at Oversampling Off gives latency=480; 4x Linear gives latency=541 — session capture `rt/verify-9/07c.png`, session capture `rt/verify-9/09c.png`
- *Added from another verifier's note:* Add runtime evidence that the active oversampling factor and phase are invisible outside Settings. At 4x Linear the Advanced view looks the same as at OS Off apart from the Ceiling unit when TP is toggled (session capture `rt/verify-9/10c-tp-off-4x.png` vs 01-adv-default.png). The factor is read only to seed the Settings combos (PluginEditor.cpp:1869-1870). This supports [UX-014](findings-ux.md#ux-014)'s Settings latency footer. [UX-014](findings-ux.md#ux-014) rejected a top-bar latency readout; the decision should also say whether a compact OS tag next to TP (which is inert at 4x and above, [DSP-003](findings-dsp-tech.md#dsp-003)) is in or out.

**Current behaviour.** The Settings popup shows six label-and-combo rows and two toggles, with no inline text and no latency figure.

The explanations exist only as tooltips, which are off by default, and in USER_MANUAL §3.5.

Switching Phase at Oversampling Off with Follow Online changes nothing. Yet both the Phase tooltip and the manual say Linear adds latency.

The reported latency (for example 529 samples at 2x Linear, 48 kHz) is visible only in the host.

**Problem.** A user cannot see the latency consequence of the Settings rows inside the plugin.

A Phase A/B at Oversampling Off silently does nothing, contradicting the tooltip and manual text.

The practical cost is small: Settings changes are infrequent, the added latency is at most 1.4 ms and compensated by the host, and most DAWs display plugin latency themselves.

**Root cause.** - DESIGN §6.4 delegates the latency note to a tooltip, and tooltips default off.
- The editor never reads the processor's reported latency.
- There is no derived-state treatment for the Phase row ([UX-013](findings-ux.md#ux-013)).
- The manual and tooltip text for Phase ignore its dependency on the effective oversampling factor.

**User impact.** Low. It costs a wasted Phase comparison at Oversampling Off. Users who care about exact PDC must look in the host. *Scope:* The Settings popup (Oversampling, Phase and Offline Render rows), the Phase tooltip, and USER_MANUAL §3.5.

**Proposed improvement.** 1. Add one read-only footer line to the Settings panel: 'Latency 529 smp · 11.0 ms (oversampling +49)'. Compute it on the message thread from getLatencySamples() and getSampleRate(), refreshed on the tick. When Force Max is set and differs, append the render figure, e.g. 'render 547'.
2. Give the Phase row the [UX-013](findings-ux.md#ux-013) inactive style, with a short inline hint ('needs Oversampling or Force Max') when Oversampling is Off and Offline Render is Follow Online.
3. Correct the Phase tooltip and the USER_MANUAL §3.5 Phase note to state the dependency.
4. Leave the UI Scale menu as it is.
5. Recompute the panel height for the extra row (350 to about 372), following the sizing comment at :1447-1452.
6. Sync DESIGN §6.4 per DOCUMENTATION_LIFECYCLE_POLICY.

**Alternatives considered.**

- *Leave as-is (tooltips plus manual)* — Acceptable for the latency figure, since hosts show PDC, but it leaves the Phase trap and the incorrect tooltip and manual text.
- *Latency readout in the main top bar* — Rejected. It clutters every session for a value that changes only in Settings.
- *Inline helper text under every Settings row* — Rejected. It roughly doubles the panel height for rows that are self-explanatory (UI Scale, Animations).
- *Turn tooltips on by default* — That is [UX-006](findings-ux.md#ux-006)'s decision. It would surface the text but not fix the Phase contradiction.

**Decision: Modify · P3.** The core claims hold: the consequences are tooltip-only, there is no latency figure, and the Phase dependency is unstated and mis-described.

The UI Scale and 'Follow Online' parts are refuted or already covered, and the latency magnitude was overstated.

The constrained change (one status line, the Phase inactive state shared with [UX-013](findings-ux.md#ux-013), and corrected text) involves no reported-latency change, since it only reads the value. It touches no gate. It amends the signed-off DESIGN §6.4 wording, which is a documentation sync, not an ADR conflict.

This is P3 because Settings are changed rarely and the cost is low.

**Dependencies.** [UX-013](findings-ux.md#ux-013); [UX-006](findings-ux.md#ux-006); [DSP-006](findings-dsp-tech.md#dsp-006)

**Acceptance criteria.**

- The Settings panel shows the currently reported latency in samples and ms, equal to the host-reported value for every Oversampling x Phase combination at 44.1, 48 and 96 kHz. At 48 kHz: Off 480, 2x Minimum 484, 16x Linear 547.
- The figure updates within one tick of changing Oversampling, Phase or Offline Render, and after a prepareToPlay at a new sample rate.
- With Oversampling Off and Follow Online, the Phase row renders inactive with a dependency hint. With Force Max selected, it renders active.
- Neither the Phase tooltip nor USER_MANUAL §3.5 claims that Linear adds latency when Oversampling is Off and Offline Render is Follow Online.
- All Settings rows fit inside the panel without clipping at UI scales XS through XL.
- DESIGN §6.4 is updated to match.

<details><summary>Verification record</summary>

**Method.** Code: read e769f33:src/gui/PluginEditor.cpp:745-782 (rows and tooltips), :1447-1473 (panel geometry), InternalState.h:106-110 (defaults), Latency.h:57-105 (latency table and effectiveFactor), USER_MANUAL.md:280-289 and DESIGN.md:936-938. A grep of src/gui confirmed that no widget reads getLatencySamples.

Screenshots: viewed session capture `rt/state/21a-settings.png`, session capture `rt/state/23a-crop.png`, session capture `rt/state/26b-uiscale-menu.png` and session capture `rt/layout/12c-settings.png`.

Runtime on :139 with stepped clicks:
- Phase set to Linear at Oversampling Off: latency stays 480.
- Oversampling 4x Linear: latency 541, matching ST-11.

**Corrections to the candidate claim.** - UI Scale: the list does extend about 25 px below the panel, but this is ordinary JUCE popup-menu behaviour. All five items are visible inside the editor, so this is not a defect.
- 'Follow Online' is explained by the Offline Render tooltip (PluginEditor.cpp:770) and by USER_MANUAL.md:284. It is unexplained only while tooltips are off, which is [UX-006](findings-ux.md#ux-006).
- The latency oversampling adds is at most 67 samples (Linear 16x, about 1.4 ms at 48 kHz) and at most 6 samples at Minimum. The 10 ms lookahead allowance is constant in every setting, so ~11.4 ms is the total latency, not the cost of choosing 16x or Linear.
- Phase is not unconditionally inert at Oversampling Off. With Offline Render = Force Max, renders run at 16x with the selected phase (Latency.h:96-105). The tooltip and manual are wrong only for the Follow Online case.
- The tooltip placement of the latency note follows the signed-off DESIGN §6.4 (DESIGN.md:937, 'latency note in tooltip'). It is a design choice, not an oversight.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 2 · severity 2 · discoverability 3 · efficiency 1 · coherence 2 · change risk 1 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-015

**Switching views resizes the window 720↔822 px by design; the family's async-resize tear is not tracked for Anabasis in any host check**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | medium | partially-confirmed | Documentation | The inherited frame, renderer and palette were not re-derived for this product's content or the user's display | Phase 5 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:1929 — setSize(kWidth, advanced ? kAdvancedH : kSimpleH) on every view change
- e769f33:src/gui/PluginEditor.h:633,638 — kSimpleH = 720; kAdvancedH = 46+446+64+266 = 822
- e769f33:docs/architecture/design-decisions/ADR-0023-012-field-fix-contracts.md:250-253 — Advanced 940×822 is an Accepted decision
- Anamorph@fd78c3b:docs/KNOWN_ISSUES.md:226-258 — KI-008 one-frame Advanced-toggle tear in async-grant hosts (JUCE VST3 wrapper)
- e769f33:docs/KNOWN_ISSUES.md:1515-1523 — editor resize named as an expected host-specific category; entries must name the exact host/version
- e769f33:docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md — no row exercising the ADV resize in the host matrix
- LAY-03 — 940x720 → 940x822 on every toggle, no intermediate sizes
- verify-3 run: session capture `rt/verify-3/adv-r1-0.05.png` (Advanced fully painted 50 ms after the click; LAY-04 band not reproduced)
- LAY-04 + session capture `rt/layout/34c-adv-gr-after.png` — the single unpainted-band capture (not reproduced)

**Current behaviour.** Every ADV toggle (and every undo that adopts a different view) calls setSize, 940×720 ↔ 940×822, instantly. In the harness the window follows and repaints within 50 ms. Real-host behaviour, including async-grant hosts, has not been observed for Anabasis.

**Problem.** The resize is designed, and no harm to users is evidenced. The gap is observability: the family knows a one-to-few-frame torn frame occurs on this exact toggle in async-grant VST3 hosts, and Anabasis has neither a known-issue entry nor a compatibility-checklist step to find out whether it occurs here. A tester who sees it will report it as a new defect with no reference point.

**Root cause.** The Advanced height is derived from content (ADR-0023 item 9) while Simple keeps the family 720 frame. The JUCE VST3 ContentWrapperComponent waits for the host's onSize grant (the same code is present in Anabasis's JUCE). The sibling's host finding was not carried into Anabasis's release checks.

**User impact.** The window jumps 102 px on each view switch, which is predictable and matches the sibling. In async-grant hosts there may be a brief clipped frame, which is cosmetic. The main cost is to triage, not to mastering results. *Scope:* All plug-in formats in hosts that grant resizes asynchronously (FL/Live/Bitwig-class per the sibling). Documentation and release-check only.

**Proposed improvement.** Keep the resize behaviour. Add a step to RELEASE_COMPATIBILITY_CHECKLIST's host matrix: toggle ADV both ways (and undo across a view change) in each matrix host, and note any clipped or torn frame with host and version. Add a pointer in KNOWN_ISSUES' host-specific preamble to the sibling's KI-008 mechanism as the expected cause. Create a numbered KI only when the tear is observed in a named host/version, as KNOWN_ISSUES.md:1521-1523 requires.

**Alternatives considered.**

- *Animate the window height between 720 and 822* — Each frame would be a host resize request. That multiplies the async-grant gap into many torn frames and adds host-thread risk. Reject.
- *One frame size for both views (Simple grows to 822 or Advanced shrinks to 720)* — Removes the jump, but changes the Simple family frame or the Accepted ADR-0023 item 9 geometry, with no evidence the jump harms anyone. Not justified.
- *Patch JUCE's self-resize allowlist* — A dependency change (JUCE 9.0.1 pin, ADR-0028) that needs an ADR and an identified affected host first, as the sibling concluded. Premature.
- *Copy Anamorph KI-008 verbatim into Anabasis KNOWN_ISSUES* — Would record an unobserved, host-unnamed issue, against Anabasis's own entry rule. A checklist step plus a preamble pointer is the compliant form.

**Decision: Modify · P3.** The resize is designed, family-consistent and correct. The LAY-04 black band did not reproduce and is an environment artefact. What remains is a small documentation and release-check gap. That warrants a checklist step and a pointer, not a behaviour change and not an unverified KI.

**Architecture gates.**

- None for the checklist/doc change
- Alternatives only: single frame size conflicts with Accepted ADR-0023 Decision item 9 (940×822); a JUCE allowlist patch is a dependency/build change under ADR-0028

**Dependencies.** [STATE-006](findings-state-model.md#state-006) (undo also resizes when it adopts a different view)

**Acceptance criteria.**

- RELEASE_COMPATIBILITY_CHECKLIST contains a host-matrix step that toggles ADV in both directions and records host/version for any clipped or torn frame
- KNOWN_ISSUES' host-specific preamble references the sibling's KI-008 mechanism as the expected cause, without a numbered entry until one is observed
- No change to kSimpleH/kAdvancedH or to applyUiScale behaviour

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/PluginEditor.cpp:1923-1931 (applyUiScale → setSize(kWidth, advanced ? kAdvancedH : kSimpleH)) and :1963-1970, e769f33:src/gui/PluginEditor.h:626-638, e769f33:docs/architecture/design-decisions/ADR-0023-012-field-fix-contracts.md:250-253, e769f33:docs/DESIGN.md:883-890, Anamorph@fd78c3b:docs/KNOWN_ISSUES.md:226-258, and e769f33:docs/KNOWN_ISSUES.md:1515-1523. Grepped e769f33:docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md for editor/resize rows (none). Checked that the same JUCE VST3 wrapper grant path exists in Anabasis's fetched JUCE (untracked local build tree build-vg/_deps/juce-src/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp:2386-2401, host allowlist Wavelab/Live/Bitwig/Reaper). Reproduced on :133: 4 ADV toggles with stepped motion, captures at 50/150/350/700 ms, pixel-scanned the newly exposed band (y 770-860).

**Corrections to the candidate claim.** Resize and absence of any transition are confirmed (log 720→822→720→822→720). LAY-04's unpainted bottom band did not reproduce: both Simple→Advanced toggles were fully painted at 50 ms with 0% black pixels in the new band (session capture `rt/verify-3/adv-r1-0.05.png`, adv-r3-0.05.png). Treat it as an Xvfb repaint artefact like E21, not a product defect. The resize itself is designed: DESIGN §6.2/§6.3 specify two frame sizes, ADR-0023 item 9 fixed Advanced at 940×822, and the sibling resizes 720→900 the same way. An animated resize is not a viable target, since each intermediate size is a host resize request and would multiply the KI-008 gap. The async tear is expected by mechanism but UNVERIFIED on Anabasis. Anabasis's own KNOWN_ISSUES rules (1515-1523) require an exact host and version for a host-specific entry, so a verbatim carry-over of Anamorph KI-008 would break that rule.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 3 · severity 1 · discoverability 2 · efficiency 1 · coherence 1 · change risk 1 · complexity 1 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-016

**Nothing in the product shows how to use the controls: a single click on a value does nothing, typing needs a double-click, the pointer never changes shape, and the tooltips and manual leave out the fine-drag, wheel, keyboard and right-button behaviour**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Interaction model | Pointer and text entry run on untuned JUCE defaults, so ordinary slips rewrite values | Phase 3 |

**Evidence**

- e769f33:src/gui/LookAndFeel.cpp:951 — `l->setEditable (false, s.isTextBoxEditable(), false); // double-click to type` (single-click entry is off)
- e769f33:src/gui/LookAndFeel.cpp:873 — ValueBox::mouseDown forwards to juce::Label::mouseDown with the comment 'double-click still opens the editor'. A press followed by movement drags the value at 180 px per full range (:921).
- e769f33:src/gui/PluginEditor.cpp:32-89 — the tipFor table gives one descriptive line per parameter and never mentions a gesture. :90-98 records that the 0.1.3 owner directive removed the detach-badge legend from the knob tips.
- e769f33:src/InternalState.h — `tree.setProperty (iid::tooltipsOn, false, nullptr)`, so tooltips are off by default
- e769f33:docs/user/USER_MANUAL.md:141-151 — the Universal gestures list covers knob drag, double-click or Alt-click reset, value-box vertical drag and double-click typing, and says tooltips are off by default. It says nothing about fine-drag, the wheel, arrow keys, fader click-to-position or the right button.
- grep of src/gui for `setMouseCursor|MouseCursor::` finds nothing, so no control gives a hover-cursor affordance
- e769f33:src/gui/PluginEditor.h:237 — Knob::mouseDoubleClick resets the knob to default. The same gesture on the readout opens the editor.
- Runtime G-05 / LAY-13(d) (rt = rt): session capture `rt/gestures/14c-loud-valuebox-typed75-crop.png`, session capture `rt/layout/35c-ratio-dblclick.png`, session capture `rt/layout/35c-inputgain-dblclick.png`
- Runtime G-02 tooltip texts: session capture `rt/gestures/21-sheet.png`, session capture `rt/gestures/33-sheet.png`
- Runtime verify-10 (stepped motion, :140): a single click on '50 %' opened no editor, and 75+Return left 50 %. A double-click opened the editor (outlined box, '50' selected), and 75+Return gave 75 %. Screenshot: session capture `rt/verify-10/07-sheet.png`

**Current behaviour.** Values can be typed only by double-clicking the small readout under a knob, or the readout to the right of a fader. A single click on it does nothing visible, and the pointer never changes shape. A double-click on the knob body resets the knob. Tooltips are off by default, and when on they describe what each control does, never how to operate it. The manual's gesture list covers drag, reset, readout drag and double-click typing. It omits the fine modifier (Ctrl/Cmd velocity mode), the wheel, arrow keys, the faders' click-to-position and what the right button does.

**Problem.** The ways to operate the controls are invisible. Exact numeric entry is what delivery-spec work needs (Ceiling to -1.00, Input Gain to a set trim), and it is reachable only through a convention the product never signals, on a 72x14 target, next to a knob where the same double-click destroys the value. Fine adjustment, wheel stepping and keyboard nudging are equally hidden.

**Root cause.** No affordance or hint layer was designed for gestures. The gestures come from JUCE and the Anamorph sibling: a double-click-only Label, JUCE Slider modifier and wheel defaults. The tooltips are descriptive only and off by default, and owner direction (0.1.3) moved explanations out of the tooltips and into the manual. The manual's gesture list was carried over from Anamorph's (Anamorph:docs/user/USER_MANUAL.md:174-182, identical wording) and was never extended to Anabasis's faders, wheel or modifiers.

**User impact.** First-time and occasional users drag toward exact values. On the Ceiling that is about 0.08 dB per px; the wheel steps 0.59 dB per notch. They find typed entry only by accident or in the manual, and some double-click the knob and reset it instead. This costs time on every mastering pass that needs spec values. It does not produce wrong output. *Scope:* Every rotary knob (Simple and Advanced views, 50+ controls), the two utility faders and every value readout. Most relevant to Ceiling, Input Gain, the thresholds and the EQ frequencies, where exact values matter.

**Proposed improvement.** The user should be able to tell from the product itself how to type, reset and fine-adjust. (1) Hovering any value readout shows a text (I-beam) cursor, and hovering a knob or fader shows a drag cursor. This adds no UI copy, so C8 is not engaged. (2) A single click on a readout, released without movement, opens the text editor. JUCE Label single-click editing opens only on a mouse-up that was not dragged, so drag-on-readout keeps working. This must ship only together with the fix in new_findings #1 (a click-away with nothing typed must commit nothing). (3) USER_MANUAL §3 gets a complete gesture table for knob, fader, readout, toggle, wheel, keyboard and right button, updated in the same change as [INPUT-007](findings-input.md#input-007), [INPUT-010](findings-input.md#input-010) and [INPUT-013](findings-input.md#input-013). (4) Whether tooltips should carry gesture hints (e.g. 'double-click the value to type') is the owner's call, since UI copy is owner-supplied under C8 and the 0.1.3 directive moved explanations out of tooltips. It is offered as an option, not implemented.

**Alternatives considered.**

- *Leave as-is (manual-only discoverability)* — Keeps family parity with Anamorph, but leaves exact entry hidden. The double-click on the knob that resets the value stays unmarked.
- *Add gesture hints to every tooltip* — Direct, but it is new UI copy (C8, owner-supplied), it cuts against the 0.1.3 directive that removed legend text from the tips, and tips are off by default, so most users would never see it.
- *Show a first-run gesture overlay or coach mark* — Discoverable, but needs new copy and a new overlay state, and it is heavier than the problem warrants.
- *Right-click menu with 'Enter value…' / 'Reset'* — A good affordance for right-clickers, covered by [INPUT-013](findings-input.md#input-013). It does not help users who single-click.

**Decision: Modify · P2.** The problem is real and reproduced, and it affects a frequent workflow. The obvious fix (tooltip hints) is constrained by C8 and by a recent owner directive. The smaller change fixes most of the discoverability without new UI copy: cursors, single-click entry once the no-op-commit defect is fixed, and a complete manual table. The tooltip text is left to the owner.

**Dependencies.** new_findings #1 (no-op value-editor commit writes the rounded value and re-engages the macro): must be fixed before single-click entry; [INPUT-007](findings-input.md#input-007) (the fine modifier must be settled before it is documented); [INPUT-010](findings-input.md#input-010) (wheel behaviour to document); [INPUT-013](findings-input.md#input-013) (right-button behaviour to document; a context menu would be a second entry affordance); G-08 keyboard focus visibility (arrow-key nudging is only usable once focus is visible); AI_AGENT_POLICY C8: any tooltip wording is owner-supplied

**Acceptance criteria.**

- Hovering any slider value readout shows an I-beam cursor, and hovering a knob or fader shows a drag cursor, in both views and at every UI scale.
- A single left click, released without movement, on any readout (e.g. Loudness '50 %') opens the text editor with the raw number selected. A press-and-drag on the same readout still drags the value and opens no editor.
- After opening a readout's editor and dismissing it without typing (click-away, Tab or Return), the parameter's normalised value is bit-identical, no undo step is pushed and the detach mask is unchanged. A state_tests case covers this for a macro knob and one managed parameter.
- USER_MANUAL §3 lists every accepted gesture: knob drag direction and rate, fine modifier, double-click and Alt-click reset, readout click and drag, fader click behaviour, wheel step and modifier, arrow-key step, right-button behaviour. The list matches the code at the release commit.
- Any change to tooltip text carries the owner's recorded approval (C8).

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/LookAndFeel.cpp:938-952 (createSliderTextBox, setEditable(false, editable, false)), :796-817 and :931 (rawEditText pre-fill), the tipFor table at e769f33:src/gui/PluginEditor.cpp:32-89 with the 0.1.3 legend-removal note at :90-98, InternalState default tooltipsOn=false, and USER_MANUAL §3 at e769f33:docs/user/USER_MANUAL.md:141-151. Grepped src/gui for setMouseCursor/MouseCursor and found none. Reproduced on :140 with stepped motion: a single click on '50 %' followed by typing 75 and Return left Loudness at 50 % with no editor opened; a double-click opened the editor and 75+Return gave 75 % (session capture `rt/verify-10/07-sheet.png`). Viewed the observers' 14c, 21-sheet, 33-sheet and 35c screenshots.

**Corrections to the candidate claim.** The claim is accurate. It is incomplete in three ways. (1) No control in src/gui sets a mouse cursor, so the pointer gives no hover cue either: there is no I-beam over editable readouts and no drag cursor. (2) Double-click means different things in different places. On the knob body it resets to the default (e769f33:src/gui/PluginEditor.h:237), but on the 72x14 readout under the knob it opens text entry. A user who double-clicks the knob expecting to type loses the value; Undo recovers it. (3) Making entry easier would expose a defect found during this verification (see new_findings). A double-click on the readout followed by a click elsewhere, with nothing typed, commits the rounded display value and opens a gesture. On a macro knob that re-engaged a detached parameter.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 4 · severity 2 · discoverability 4 · efficiency 3 · coherence 3 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-017

**A corrupt or foreign user preset is listed like any other and silently does nothing when chosen, while the ‹ › ring skips it, so the menu and the arrows disagree about what is loadable**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P3** | high | confirmed | Feedback/observability | Save, load, browse and restore change or lose state without saying so | Phase 4 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:2225-2228 — USER rows added for every findChildFiles("*.anabasis") entry, always enabled (third arg true)
- e769f33:src/gui/PluginEditor.cpp:2332-2338 — menu pick: applyPresetFile result discarded; comment calls a corrupt file 'a documented no-op'
- e769f33:src/gui/PluginEditor.cpp:2355-2360 — Load Preset… pick: applyPresetFile result discarded
- e769f33:src/gui/PluginEditor.cpp:455-464 — ‹ › loop keeps walking while applyPresetFile returns false
- e769f33:src/PluginProcessor.cpp:1676-1678 — parse before bracket; unreadable file returns false with nothing changed
- e769f33:src/PresetManager.cpp:86-91 — parsePresetFile: null on unparsable XML or a root other than AnabasisPreset
- e769f33:src/gui/LookAndFeel.cpp:502-508 — product principle: inactive rows dimmed so 'unavailable reads before the click rather than after it does nothing'
- E11 — Corrupt/WrongRoot listed under USER, picking either leaves 'EdgeTest1 *' — session capture `rt/edges/62a-menu-with-corrupt-user.png`, session capture `rt/edges/62bc-strip.png`
- verify-2 repro — Corrupt row indistinguishable — session capture `rt/verify-2/14-menu-corrupt-crop.png`
- verify-2 repro — pick leaves 'Gamma' — session capture `rt/verify-2/15b-after-pick-1s-crop.png`
- verify-2 repro — ‹ from Gamma skips Corrupt to Alpha — session capture `rt/verify-2/16-prev-from-gamma-crop.png`

**Current behaviour.** Every *.anabasis file in the user folder appears as a normal USER row, whether or not it can be read. Choosing an unreadable or foreign-root file closes the menu and changes nothing: no label change, no sound change, no undo step, no message. ‹ › steps past the same file without landing on it. Load Preset… on such a file behaves the same silent way.

**Problem.** The UI cannot report a load failure. The menu offers a row the ring treats as non-existent, which breaks the product's own rule that unavailable menu rows are dimmed before the click.

**Root cause.** showPresetMenu builds rows from a directory scan with no readability test, and both editor apply sites discard the bool that applyPresetFile returns. The preset flow has no error surface; only the ring tests readability, by attempting the apply.

**User impact.** A user whose preset file is damaged clicks it and nothing happens. They cannot tell a broken file from a missed click, and they get no hint to repair or replace the file. From ‹ › the file is invisible, so the two browse paths show different libraries. *Scope:* User-folder files that fail parsePresetFile: unparsable XML, or a root tag other than AnabasisPreset. The same silent failure applies to a file picked in Load Preset…. Files with the correct root but unknown or missing parameters are not affected; they apply partially by design (ADR-0007 read rules). Frequency is low.

**Proposed improvement.** (1) When building the menu, test each user file with the same PresetManager::parsePresetFile check that the apply path uses, so the menu and the ring agree by construction. Add unreadable files as inactive rows with the suffix ' (unreadable)'. The existing LookAndFeel already dims inactive rows, and JUCE will not fire them. (2) If an apply from the menu or the Load Preset… chooser still fails (for example the file changed after the menu opened, or it was picked in the chooser), check the bool and show a short message in the preset-name slot, e.g. "Can't read 'Corrupt'" in the warning colour for about 2 s, then restore the previous label. Parameters, identity, undo and audio stay untouched, as today. Add one sentence to USER_MANUAL §7.

**Alternatives considered.**

- *Hide unreadable files from the menu* — Makes the menu match the ring exactly, but the user never learns the file exists and is broken. Worse for diagnosis than a dimmed row.
- *Feedback on failed apply only, rows stay enabled* — Smallest change and it fixes the silent failure, but the menu and the ring still disagree. Acceptable as a first step if parsing files at menu-open time is measured to be too slow.
- *Modal error dialog* — Too heavy for a harmless no-op, and it adds a modal surface in a plugin window.
- *Leave as-is* — Rejected. It is a silent failure path that contradicts the product's own inactive-row rule (LookAndFeel.cpp:502-505).

**Decision: Proceed · P3.** Confirmed in code and reproduced twice (E11, verify-2). The inconsistency is real, and the product already states the rule it breaks. The fix reuses the single readability test, parsePresetFile, that ADR-0022 and the ring depend on, so the menu cannot disagree with the apply path. Nothing is gated: the ADR-0007 rule that corrupt input is a no-op is kept (only feedback is added), and the ring semantics of ADR-0022 are unchanged. Priority is P2 rather than P3 because it is a correctness-of-feedback and consistency fix, not polish. P2 rather than P1 because broken preset files are rare and nothing is lost.

*Calibration:* the verifier judged Proceed / P2; the final judgement is Proceed / P3. Lowered P2->P3 for uniformity: choosing a corrupt or foreign preset is a true no-op (no duck, no undo step, nothing lost) and rare (fr1, sev2), the same class and the same menu/ring model as [STATE-015](findings-state-model.md#state-015) (P3), and equal in severity x frequency to [UX-019](findings-ux.md#ux-019) and [STATE-010](findings-state-model.md#state-010) (P3). Land it with [UX-018](findings-ux.md#ux-018)'s status-line mechanism.

**Dependencies.** [UX-018](findings-ux.md#ux-018) / [UX-003](findings-ux.md#ux-003) (the same idea of a short preset status message; share one mechanism); [STATE-015](findings-state-model.md#state-015) (the same menu/ring model)

**Acceptance criteria.**

- With Corrupt.anabasis (plain text) and WrongRoot.anabasis (valid XML, foreign root) in the preset folder, the menu shows both under USER as dimmed, inactive rows with an '(unreadable)' suffix, and clicking them does nothing.
- Readable USER rows, FACTORY rows and the single tick are unchanged.
- ‹ › still skips both files; testTheRingWalksPastAnUnreadablePreset passes unchanged.
- Picking an unreadable file in Load Preset… shows a visible message naming the file within 0.5 s. Label, identity, parameters, undo/redo state and audio stay unchanged, with no duck.
- Replacing the broken file with a valid preset on disk makes its row active the next time the menu opens.
- Opening the menu with 200 user presets stays within a measured, stated message-thread budget.

<details><summary>Verification record</summary>

**Method.** Code read at e769f33: e769f33:src/gui/PluginEditor.cpp:381-467 (the ring; :455-464 loops on the bool), :2173-2234 (the menu adds every *.anabasis as an enabled row, :2225-2228), :2332-2338 and :2355-2360 (the result is discarded). Also e769f33:src/PluginProcessor.cpp:1661-1678 (returns false before the undo bracket and the duck), e769f33:src/PresetManager.cpp:86-91 (parsePresetFile), and e769f33:src/gui/LookAndFeel.cpp:502-508 (inactive rows are dimmed so that 'unavailable reads before the click'). Viewed session capture `rt/edges/62a-menu-with-corrupt-user.png` and session capture `rt/edges/62bc-strip.png`. Reproduced on :132 with stepped motion: I wrote Corrupt.anabasis ('this is not xml'). It is listed under USER between Alpha and Gamma, looking like any other row (14-menu-corrupt-crop.png). Picking it with Gamma loaded left the label at 'Gamma' and Loudness at 60 %, showed no message, and left the undo/redo buttons unchanged (15b-after-pick-1s-crop.png). ‹ from Gamma skipped Corrupt and landed on Alpha (16-prev-from-gamma-crop.png).

**Corrections to the candidate claim.** The evidence lists :460 among the places that ignore applyPresetFile's bool. That is wrong: the ring uses the bool to keep walking (:457-464). Only the menu (:2337) and the Load Preset… chooser (:2359) discard it. The failure is a true no-op, with no duck and no undo step, so the harm is confusion, not sound. Frequency is low: the plugin's own writes are atomic (JUCE XmlElement::writeTo goes through TemporaryFile, juce_XmlElement.cpp:416-434 in the fetched JUCE 9.0.1), so broken files come from outside the plugin (hand edits, sync tools, other software).

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 1 · severity 2 · discoverability 4 · efficiency 2 · coherence 4 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-018

**Save Preset gives no validation feedback: an empty name is a silent no-op (Save never disables), illegal characters are stripped without notice, the field can be prefilled with the factory name 'Default', and there is no location hint**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | partially-confirmed | Feedback/observability | Save, load, browse and restore change or lose state without saying so | Phase 0 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:943-945 — createLegalFileName(trim); if (name.isEmpty()) return; (no feedback, Save never disabled)
- e769f33:src/gui/PluginEditor.cpp:949 — if (proc.savePresetFile (file)) { … } with no else branch (write failure silent)
- e769f33:src/gui/PluginEditor.cpp:1155-1158 — JUCE Button sets wantsKeyboardFocus true unconditionally (why a Save click takes focus from the field)
- e769f33:src/gui/PluginEditor.cpp:1454, :1476-1485 — 340×150 panel; title, field, button row, with ~38 px unused below the buttons
- e769f33:src/gui/PluginEditor.cpp:2373 — prefill = proc.currentPresetName() (ADR-0022:26 documents it)
- JUCE 9.0.1 juce_File.cpp:855-878 (fetched per e769f33:CMakeLists.txt:82) — strips "#@,;:<>*^|?\/ and truncates at 128 chars
- E11 — empty-name Save no-op, focus outline lost, 'Edge/Test:1'→EdgeTest1 — session capture `rt/edges/59a-save-empty-name-clicked-save-crop.png`, session capture `rt/edges/59c-preset-menu-with-user.png`, session capture `rt/edges/58a-save-preset-panel.png`
- ST-04 — no location hint — session capture `rt/state/15a-save-dialog.png`
- verify-2 repro — after the empty-name Save click, typed 'Xyz' is dropped — session capture `rt/verify-2/18-20-strip.png`
- verify-2 repro — 'Al/pha' saved as Alpha, no notice — [capture](captures/13-save-overwrite.png)
- *Added from another verifier's note:* Add a concrete trigger for [UX-018](findings-ux.md#ux-018) part (c), the silent write failure. JUCE 9.0.1 File::createLegalFileName (juce_File.cpp:855-878) strips only the characters \"#@,;:<>*^|?\\/ and caps the length. It does not reject Windows reserved base names (CON, PRN, AUX, NUL, COM1-9, LPT1-9) or trailing dots and spaces. Such a name reaches XmlElement::writeTo, and a false return falls through the Save handler with no else branch (e769f33:src/gui/PluginEditor.cpp:948-961): the panel stays open and nothing is said. Add a Windows acceptance case: 'save as CON shows the error line and changes no label or identity'. Inferred from code, not run on Windows.

**Current behaviour.** The Save panel has a title, a name field, and Save and Cancel buttons, and no message line. An empty or whitespace-only name, or one made only of stripped characters, makes Save do nothing. The Save button stays enabled, and a mouse click on it moves keyboard focus to the button, so further typing is lost. Characters that are illegal in file names are removed without notice, and the preset is saved and listed under the stripped name. A failed write leaves the panel open without explanation. Nothing in the panel says where the file goes.

**Problem.** Every case where Save does something other than what was typed is silent. The user cannot tell why Save did nothing, or that the stored name differs from what they typed. The silent stripping can also create a name collision that becomes a silent overwrite ([UX-003](findings-ux.md#ux-003)).

**Root cause.** The overlay has no validation state and no message surface. The handler's only branches are an early return and a success path (e769f33:src/gui/PluginEditor.cpp:943-961), and Save's enabled state is never tied to the field's content.

**User impact.** Mild confusion and repeated attempts in the empty or failed cases. Preset names that don't match what was typed (e.g. 'Mix #2' is stored as 'Mix 2'). Lost keystrokes after a no-op click. The worst outcome, a stripped name hitting an existing file, is data loss, which is handled under [UX-003](findings-ux.md#ux-003). *Scope:* Save Preset… on all platforms: names that are empty or contain "#@,;:<>*^|?\/, names over 128 characters, and failed writes. Not in scope: the prefill, which is ADR-sanctioned and kept.

**Proposed improvement.** Constrained change inside the existing overlay. Add one status line in the unused ~38 px under the buttons; the same line carries [UX-003](findings-ux.md#ux-003)'s replace prompt. (a) While the cleaned name is empty, show the Save button disabled, make Return inert, and have the status line read 'Enter a name'. (b) As the user types, when the cleaned name differs from the typed text, show it live: "Will save as 'EdgeTest1'". (c) If the write fails, keep the panel open with 'Couldn't save the preset — check that the preset folder is writable', and change no label or identity. (d) After any Save that does not close the panel, give keyboard focus back to the name field. The prefill with the current name stays as it is (ADR-0022). The folder location is better served by a menu affordance that reveals the folder than by static text in this panel (see new_findings), so it is not part of this change.

**Alternatives considered.**

- *Reject illegal characters as they are typed (input filter)* — Prevents surprises but silently eats keystrokes, which is another kind of unexplained behaviour. The live 'Will save as' line is clearer.
- *Replace illegal characters with '_' or '-' instead of removing them* — Changes the file-naming rule for everyone and still needs to be shown. Not needed if the cleaned name is displayed.
- *Stop prefilling factory names (empty field after loading a factory preset)* — Rejected. The prefill is documented in ADR-0022:26, and factory-name user presets are explicitly supported.
- *Show the preset folder path in the panel* — Long and platform-specific (%APPDATA%…). A 'show folder' action in the preset menu is more useful; out of scope here.
- *Leave as-is* — Rejected. The silent outcomes compound with [UX-003](findings-ux.md#ux-003) into silent data loss.

**Decision: Modify · P2.** The validation and feedback gaps are confirmed and reproduced, including a loss of focus the claim only hinted at. The 'Default' prefill sub-claim is dropped as ADR-sanctioned, and the location sub-claim is moved to a folder-reveal affordance outside this panel. The remaining fix is one status line, a Save enabled state tied to the field, and focus handling, all on the message thread with the file-naming rule unchanged. No gate applies.

**Dependencies.** [UX-003](findings-ux.md#ux-003) (shares the status line; a stripped-name collision must raise the replace prompt); [UX-017](findings-ux.md#ux-017) (one preset status/feedback mechanism)

**Acceptance criteria.**

- An empty field, whitespace only, or '///' shows the Save button as disabled, Return writes nothing, the status line reads 'Enter a name', and the name field keeps keyboard focus.
- Typing 'Edge/Test:1' shows "Will save as 'EdgeTest1'" before Save is pressed. After saving, the USER row reads 'EdgeTest1'.
- With the preset folder made unwritable, Save leaves the panel open with an error line, and the top-bar label, tick and identity are unchanged.
- After any Save press that leaves the panel open, typed characters go into the name field.
- The prefill is still the current preset name, fully selected, including factory names.

<details><summary>Verification record</summary>

**Method.** Code read at e769f33: e769f33:src/gui/PluginEditor.cpp:941-962, :2365-2385, :1454 (panel 340×150), :1476-1485 (panel layout), :1155-1158 (JUCE Buttons take keyboard focus). Also JUCE createLegalFileName in the fetched JUCE 9.0.1 (e769f33:CMakeLists.txt:82; build/_deps/juce-src/modules/juce_core/files/juce_File.cpp:855-878). Viewed session capture `rt/edges/58a-save-preset-panel.png`, session capture `rt/edges/59a-save-empty-name-clicked-save-crop.png`, session capture `rt/edges/59c-preset-menu-with-user.png` and session capture `rt/state/15a-save-dialog.png`. Reproduced on :132 with stepped motion: I cleared the field and clicked Save. The panel stayed open and the field lost its focus outline. Typing 'Xyz' afterwards was dropped and the field stayed empty, because the Save button now held keyboard focus (18-20-strip.png). 'Al/pha' was saved as Alpha with no notice (21-22-strip.png).

**Corrections to the candidate claim.** (1) The 'Default' prefill is not a validation defect. It is the documented prefill of the current name (ADR-0022:26; setText at :2373). Factory presets are compiled in and read-only; saving always writes a USER file, and ADR-0022 lets it share a factory name. That sub-claim should be kept as it is. (2) The claim understates the stripping. createLegalFileName also removes '#', '@' and ',' (common in names like 'Mix #2'), cuts names over 128 characters, and turns a name made only of such characters into an empty name, which gives the same silent no-op. (3) Not in the claim: a failed write (savePresetFile returns false at :949, e.g. an unwritable folder) also leaves the panel open with no explanation. So would an OS-reserved name that createLegalFileName does not filter, e.g. CON on Windows; this is inferred, not run. (4) Not in the claim: after a mouse-click no-op, keystrokes are lost. In a DAW, keys the plugin does not consume may reach the host; this was not verified in a host. (5) The folder location is documented in USER_MANUAL.md:387-395, but no UI surface shows or opens it.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 2 · severity 2 · discoverability 4 · efficiency 2 · coherence 3 · change risk 1 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-019

**LEARN and FREEZE run together with no interlock or hint, so a Learn can commit a reference that has no audible effect until Freeze is released**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | confirmed | Feedback/observability | The adaptive engine changes the audio from state the user cannot see, keep or reset | Phase 5 |

**Evidence**

- e769f33:src/dsp/AdaptiveEngine.h:398 — if (learnActive.load(...) && audible): Learn sums accumulate independent of freeze
- e769f33:src/dsp/AdaptiveEngine.h:418 — if (! freeze && audible): trims slew toward (ref - feature) targets only when not frozen
- e769f33:src/gui/PluginEditor.cpp:2134-2142 — setEnabled is only ever called on undoButton/redoButton
- e769f33:src/gui/PluginEditor.cpp:42 — FREEZE tip 'Hold the adaptive trims exactly where they are now'; :641-642 LEARN tip does not mention Freeze
- e769f33:docs/policies/MODE_AND_ADAPTATION_POLICY.md:47-51 — Freeze: 'while frozen, the adaptive layer contributes a constant'; Learn: explicit start/end
- e769f33:docs/architecture/design-decisions/ADR-0007-state-schema-v1.md:83-88 — ADAPTIVE global, frozen trim vector per slot
- Runtime G-18: session capture `rt/gestures/35-sheet.png` (FREEZE on, then LEARN '5', orange LEARN, white LEARN; FREEZE stays on; no message)
- Runtime verify-4 (FROZEN_TRIMS unchanged across a committed Learn; they move only after unfreeze): rt/verify-4/s04-frozen-music.xml, s05-frozen-after-learn.xml, s06-refrozen-pink.xml, 18-freeze-learn-sheet.png

**Current behaviour.** With FREEZE on, LEARN runs and commits normally and the new references are saved. The trim vector stays latched, so the sound does not change until FREEZE is released. The trims then slew over about 2 s toward targets based on the new reference. Nothing on screen links the two controls, and neither tooltip (off by default anyway) nor the manual mentions the interaction.

**Problem.** The effect of a completed Learn is deferred by another control and nothing on screen says so, so the user cannot tell 'Learn did nothing' from 'Learn is held by Freeze'.

**Root cause.** The engine behaviour is correct per the policy. There is no derived-state presentation of the adaptive controls: the editor shows neither the trim vector nor the learned-reference state (see PF-product-contract-docs-4), and there is no commit acknowledgement ([UX-005](findings-ux.md#ux-005)) that could carry the hint.

**User impact.** Low-frequency confusion: it needs Freeze on and a Learn. The output is correct. The risk is that the user thinks Learn failed and repeats it or gives up, or is surprised later when releasing Freeze moves the sound. With A/B, it can also retarget the other unfrozen slot. *Scope:* The Simple toggle row (FREEZE and LEARN), the USER_MANUAL §4 text, and the LEARN tooltip. No DSP change.

**Proposed improvement.** Keep both controls operable with no interlock. When a non-empty Learn commits while FREEZE is on, the commit acknowledgement from [UX-005](findings-ux.md#ux-005) says so without needing tooltips. For example it reads 'LEARNED · held by FREEZE', or the FREEZE pill pulses once with the acknowledgement. Extend the LEARN tooltip: 'With FREEZE on, the new reference takes effect when you release Freeze'. Add one sentence to USER_MANUAL §4 saying the same, and that the reference is shared by both A/B slots (see [STATE-009](findings-state-model.md#state-009)).

**Alternatives considered.**

- *Interlock: disable LEARN while FREEZE is on* — Rejected. It blocks a legitimate calibrate-while-held workflow and couples the two controls' semantics; MODE policy Enforcement puts Freeze semantics under the Architecture Review Gate.
- *Auto-release FREEZE on a Learn commit* — Rejected. It silently changes the sound and breaks the Freeze contract (MODE invariant 3; ARG item).
- *Documentation only (manual sentence plus tooltip)* — Acceptable minimum, but tooltips are off by default, so most users would still get no in-product signal.

**Decision: Modify · P3.** The engine behaviour must be preserved (policy invariant 3), so the obvious 'interlock' fix is wrong. A constrained hint on the existing commit-acknowledgement channel plus one manual sentence covers the real gap at near-zero risk. It is P3 because it needs two optional controls together and the output is never wrong.

**Architecture gates.**

- None for the proposed hint. The rejected interlock and auto-release alternatives would change Freeze semantics: MODE_AND_ADAPTATION_POLICY Enforcement names that an Architecture Review Gate item and an AI Agent Hard Stop.

**Dependencies.** [UX-005](findings-ux.md#ux-005) (commit acknowledgement channel); PF-product-contract-docs-4 (adaptive trims invisible; 'let it settle' has no display); [STATE-009](findings-state-model.md#state-009) (global reference shared across A/B slots)

**Acceptance criteria.**

- With FREEZE on, a completed non-empty Learn shows an on-screen indication, visible with tooltips at their default (off), that the new reference is held by Freeze.
- LEARN stays operable while FREEZE is on.
- FROZEN_TRIMS in savexml are bit-identical before and after a Learn done with FREEZE on; testFreezeLatchesTrims unchanged and green.
- USER_MANUAL §4 states that a reference learned while frozen applies when Freeze is released.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/dsp/AdaptiveEngine.h:398-413 (Learn accumulates whenever learnActive && audible, regardless of freeze) and :418 (trims slew only when !freeze && audible). Read e769f33:src/gui/PluginEditor.cpp:2134-2142 (only Undo/Redo call setEnabled), :42 (FREEZE tip) and :641-642 (LEARN tip). Read MODE_AND_ADAPTATION_POLICY.md:47-51 (invariant 3). Viewed G-18 session capture `rt/gestures/35-sheet.png`. Reproduced on :134:
1. Music at -6 dB for 12 s, then FREEZE on: savexml s04.
2. Switched to pink noise and ran LEARN for about 7 s, then stopped: savexml s05. FROZEN_TRIMS were bit-identical between s04 and s05 (releaseOctaves=-0.12085, stereoLink=0.02417, scHpfHz=1.23901, dynTiltDb=0.01996), while ADAPTIVE moved from (3.338, -4.690) to (0.214, -2.761).
3. FREEZE off for 12 s, then on again: savexml s06. The vector had moved toward the new reference (releaseOctaves -0.0177, stereoLink 0.0035, scHpfHz 1.000).
The UI showed the accent countdown and then a white LEARN, FREEZE stayed lit, and no message appeared (verify-4/18-freeze-learn-sheet.png).

**Corrections to the candidate claim.** 1. The deferred effect is REQUIRED behaviour, not a defect: MODE_AND_ADAPTATION_POLICY invariant 3 says that while frozen the adaptive layer contributes a constant. The only gap is the missing explanation.
2. Learning while frozen is a legitimate workflow (calibrate without the sound moving during the pass), so the 'no interlock' part of the title is not itself a problem.
3. Any Learn is subtle even unfrozen: the trims are bounded and re-converge over about 2 s, so 'hears no change' is partly true without Freeze too.
4. Code-derived and not run: the references are global (ADR-0007) and Freeze is per slot, so a Learn done in a frozen slot retargets the other slot straight away if that slot is unfrozen.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 1 · severity 2 · discoverability 4 · efficiency 2 · coherence 2 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-020

**At L/XL the editor can be taller than the display (XL Advanced 1410x1233 logical), and nothing checks the screen before or after a scale or ADV change**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Interaction model | The inherited frame, renderer and palette were not re-derived for this product's content or the user's display | Phase 5 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:1923-1935 — applyUiScale: setSize (kWidth, advanced ? kAdvancedH : kSimpleH); setTransform (scale (hostScale * scale)); no display query
- e769f33:src/gui/PluginEditor.h:632-638 — kWidth 940, kSimpleH 720, kAdvancedH = kBarH + kPanelRowH + kUtilityH + kMeterRowH (822)
- e769f33:src/InternalState.h:66-67 — steps {75,85,100,125,150}, names XS..XL
- e769f33:src/gui/PluginEditor.cpp:795-801,840 — the menu lists bare XS..XL, and the tooltip says only 'M is the original; everything scales in proportion', so nothing shows the size a step will produce
- grep of e769f33:src for getDisplays|userArea|setResizeLimits|ComponentBoundsConstrainer — 0 hits
- JUCE 9.0.1 juce_audio_plugin_client_VST3.cpp:2345-2348 getSizeToContainChild = getLocalArea(editor) (transform-aware), so the host view becomes 1410x1233; juce_StandaloneFilterWindow.h:709 setBoundsConstrained only at construction
- Runtime (verify-11, :141, stepped motion): editor-bounds 1410x1080 at XL Simple, then 1410x1233 after advancedMode=1, on a 1600x1100 screen — rt/verify-11/app.log, session capture `rt/verify-11/01c-scale-menu.png`
- Observer LAY-09 — session capture `rt/layout/36c-XL.png`, session capture `rt/layout/13c-uiscale-menu.png`

**Current behaviour.** The scale step (or the ADV toggle) sets a fixed logical size times a transform. The plug-in window grows to whatever that produces, whatever the display, and a step that fits in Simple can overflow after ADV grows the window by 14%. The UI Scale menu shows only XS..XL, with no size or fit information.

**Problem.** On common displays (1080-line monitors; 768/800/900-line laptops) the larger steps push the bottom of the editor off-screen: the Statistics panel, the graph well and the Advanced utility row. The user is given nothing to predict this. A session saved at L/XL on a large monitor reopens overflowing on a laptop.

**Root cause.** applyUiScale (e769f33:src/gui/PluginEditor.cpp:1923-1935) applies the fixed five-step ladder to a fixed per-mode logical size without reading the display's user area. The JUCE wrappers pass the transformed size to the host unchecked, and the Standalone constrains to the screen only when the window is first created.

**User impact.** Metering and the lower controls are hidden until the user steps the scale down. The top bar stays reachable, so recovery takes one Settings change, but the trap repeats whenever ADV is toggled at L or a large-monitor session is opened on a smaller screen. No audio or state is affected. *Scope:* Users of L/XL on displays shorter than about 1100 logical px in Advanced (about 1150 px for XL Simple), and M Advanced on 768/800-line screens. Every format, since all JUCE wrappers size from the transformed bounds. OS-scaled displays probably make it worse, but that is not verified.

**Proposed improvement.** Make the scale choice screen-aware without changing the stored preference.
(1) The UI Scale menu shows each step's resulting size for the current mode (e.g. 'L — 1175 × 1028'). Steps whose transformed bounds exceed the user area of the display the editor is on, minus a fixed host-chrome allowance, are shown disabled with a 'too large for this display' note.
(2) When the editor opens, or ADV toggles, while the stored step does not fit, the editor RENDERS at the largest step that fits. The combo shows that rendered step with a note (e.g. 'L — XL too large here'). int_uiScale is NOT rewritten, so the same session reopens at XL on a larger display.
(3) No automatic change at any other time.
The check uses Desktop::getDisplays().getDisplayForRect(getScreenBounds()), falling back to the primary display before the peer exists, and must be injectable so headless tests stay deterministic.

**Alternatives considered.**

- *Leave as-is and document the display requirement for L/XL in USER_MANUAL §3.5* — Cheapest, but it does not stop the ADV-at-L trap or the large-monitor-to-laptop reopen case. Acceptable only as an interim doc sync.
- *Clamp and write the clamped step back to int_uiScale* — Rejected. It destroys the user's preference on a laptop-to-studio round trip. It also makes a display-dependent event a writer of the InternalState tree, which is the writer pattern HANDOVER rounds 51/63 removed from the editor.
- *Annotate and disable non-fitting items only, with no render fallback* — A valid smaller first step with no invariant risk. It still leaves the reopen-on-a-smaller-screen and ADV-toggle cases overflowing.
- *Free / aspect-locked host resize ([UX-021](findings-ux.md#ux-021))* — It would let users fit exactly, but it is gated and deferred. It does not remove the need for a sensible opening size.

**Decision: Modify · P2.** The overflow is real, reproduced and code-confirmed, and it has no guard. The obvious fix ('clamp to the screen') would, if done as a silent clamp or a write-back, break the tested invariant that the displayed step is the rendered step (testAnOutOfListUiScaleClampsConsistently: 'every combo label names the step that item actually renders at'). It would also mutate a persisted field based on the display. The constrained version guards the choice, renders a fitting step without persisting it, and keeps the combo honest, so ADR-0017's ladder and its adoption rule stay untouched. Priority is P2, not P1: recovery is one Settings change with the top bar still visible, the default M fits 1080p, and no audio or state is harmed.

**Architecture gates.**

- No hard-stop category, provided the fallback is render-only: ui_scale::steps and InternalState::replaceFrom's adoption rule (ADR-0017 Decision 1-2) must not change
- A write-back of a clamped step, or a new persisted 'effective scale' field, would be a Serialization Registry change (ARCHITECTURE_REVIEW_GATE) and is excluded from this proposal
- The ADR-0017 verification test (testAnOutOfListUiScaleClampsConsistently) must keep passing: the combo must show the step actually rendered

**Dependencies.** [UX-021](findings-ux.md#ux-021) (free resize, if adopted later, would supersede the step guard); [DOC-007](findings-doc-test.md#doc-007) (USER_MANUAL §3.5 wording for the size and fit behaviour); ui-architecture#15 (ADV toggle resize 720↔822 — merged here); A real-host check of the host-DPI (setScaleFactor) compose path on a Windows scaled display

**Acceptance criteria.**

- On a display whose user area is 1920x1040 logical, the UI Scale menu shows XL as not selectable in both modes and L as not selectable in Advanced (or annotated as too large), and each item shows its resulting W×H for the current mode
- Loading a session with int_uiScale=150 and advancedMode=1 on that display gives transformed editor bounds (getLocalArea) with height ≤ the user area minus the chrome allowance, and the combo displays the rendered step with a 'too large' note
- Saving that session afterwards writes int_uiScale="150" unchanged; reopening it on a ≥1440-line display renders 1410x1233
- Toggling ADV at L on a 1080-line display never produces bounds taller than the user area
- On a display where every step fits, behaviour matches e769f33 exactly, testAnOutOfListUiScaleClampsConsistently passes unchanged, and the display query is injectable for tests
- No code path other than uiScaleBox.onChange and InternalState::replaceFrom writes iid::uiScale

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/PluginEditor.cpp:1923-1935 (applyUiScale), e769f33:src/gui/PluginEditor.h:632-638 (940 / 720 / kAdvancedH=822), e769f33:src/InternalState.h:66-67 (ladder), and e769f33:src/gui/PluginEditor.cpp:795-801 and :840 (combo items and tooltip). Grepped src/ for Desktop::getDisplays, userArea, setResizeLimits and constrainers: zero hits. Read the JUCE 9.0.1 wrappers (ADR-0028 pin). VST3 sizes the host view from the TRANSFORMED bounds (build/_deps/juce-src/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp:2090-2092, 2345-2348 getLocalArea). The Standalone does the same (juce_StandaloneFilterWindow.h:1012-1033) and constrains to the screen only once, at window creation (:709). Reproduced on :141 (Xvfb screen 1600x1100) with stepped pointer motion: Settings, then UI Scale, then XL. The log read 'editor-bounds w=1410 h=1080'. 'param advancedMode 1' then gave 'w=1410 h=1233': taller than the 1100-px screen, with no clamp and no warning (rt/verify-11/app.log, session capture `rt/verify-11/01c-scale-menu.png`). Viewed session capture `rt/layout/36c-XL.png` and 13c-uiscale-menu.png.

**Corrections to the candidate claim.** The claim is accurate, but it understates the scope. The step list is XS 705x540/617, S 799x612/699, M 940x720/822, L 1175x900/1028, XL 1410x1080/1233 (Simple/Advanced, logical px, before host chrome). On a 1920x1080 display with a taskbar and a host plugin-window title bar, XL does not fit in EITHER mode (XL Simple is exactly 1080 before chrome). L Advanced (1028 + chrome) also overflows. So an L user who clicks ADV overflows even though L fit in Simple. On 768-line laptops even M Advanced (822) overflows. The harness window not following the resize (948x830 here) is the known artefact (JUCE ResizableWindow::childBoundsChanged uses the untransformed size) and is not counted. No real host was exercised, so 'hosts honour the size' is inferred from the JUCE wrapper code, not observed. The host-DPI compose path (setScaleFactor) was also not exercised.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 2 · severity 2 · discoverability 3 · efficiency 2 · coherence 2 · change risk 3 · complexity 3 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-021

**There is no free or host resize, only five fixed scale steps (75-150 %), against the brief's 'resizable' 80-200 %; the layout is width-fluid but height-rigid**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Defer** | **P3** | high | partially-confirmed | Interaction model | The inherited frame, renderer and palette were not re-derived for this product's content or the user's display | — |

**Evidence**

- grep of e769f33:src/gui for setResizable|setResizeLimits|ComponentBoundsConstrainer — 0 hits
- JUCE 9.0.1 juce_AudioProcessorEditor.h:284 resizableByHost = false; juce_audio_plugin_client_VST3.cpp:2096-2104 canResize → kResultFalse; juce_StandaloneFilterWindow.h:713 setResizable (editor->isResizable(), false)
- e769f33:docs/DEVELOPMENT_BRIEF.md:160,169 — 'UI scaling (80–200%)', 'resizable … with window size persistence'
- e769f33:docs/DESIGN.md:867-870 — 'free host resize is still off (discrete steps, size persists via int_uiScale)'
- e769f33:docs/architecture/design-decisions/ADR-0017-uiscale-ladder-narrowing.md — the owner-directed ladder {75,85,100,125,150}; 'Forecloses: treating the ladder as a display concern'
- e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:91 — '[ ] Vector drawing throughout; resizable; HiDPI-aware' (unchecked)
- e769f33:src/gui/PluginEditor.cpp:1704,1757-1758 — row heights derived from kSimpleH/kBarH constants; widths from getWidth()
- Anamorph fd78c3b:src/PluginEditor.cpp:681 setResizable(false,false); :1794 scales {0.75,0.85,1.0,1.25,1.5} — the same family model
- Runtime LAY-10 — session capture `rt/layout/23-hover-editor-corner.png`, session capture `rt/layout/24c.png` (no corner, no resize); session capture `rt/layout/25c.png` (harness-only, excluded as a defect; it corroborates the height-rigid layout)

**Current behaviour.** The editor is fixed-size in every format. The host offers no resize edge, the Standalone window is non-resizable, and size changes only through five Settings steps (M→L is a 25 % jump).

**Problem.** Users cannot fit the editor to their screen between steps. The product spec (brief §7/§8) and the brand checklist still promise 'resizable' and 80-200 %, while DESIGN and ADR-0017 record a stepped model, and the owner decision reconciling the two is not recorded anywhere as open or closed.

**Root cause.** A deliberate design choice. The sibling's whole-window transform ladder was adopted (DESIGN.md:867-870; ADR-0017 by owner directive 2026-08-05), the editor never calls setResizable or setResizeLimits, and the brief and checklist were not updated ([DOC-007](findings-doc-test.md#doc-007)).

**User impact.** The main harm, overflow on smaller screens, is covered by [UX-020](findings-ux.md#ux-020). What remains is granularity: a user whose best fit lies between M and L, or between L and XL, has to accept a smaller UI than the screen allows. Reference products users compare against (brief §8 cites Ozone 12) resize freely. *Scope:* All formats and hosts. A set-once preference per session, not an every-pass workflow.

**Proposed improvement.** Now: record the question as an owner decision (an OPEN_QUESTIONS entry, ideally a family-level decision because Anamorph shares the model), and sync the brief and checklist wording through [DOC-007](findings-doc-test.md#doc-007).
If the owner opts for resize: offer aspect-locked host resize per mode (940:720 Simple, 940:822 Advanced) through setResizeLimits plus getConstrainer()->setFixedAspectRatio. The dragged size maps to a continuous factor on the SAME whole-window transform, so resized() and the layout are untouched. The constrainer's aspect is updated on ADV toggle while the current factor is kept. XS..XL remain as quick presets.

**Alternatives considered.**

- *Keep the stepped model and fix the docs ([DOC-007](findings-doc-test.md#doc-007)), with the [UX-020](findings-ux.md#ux-020) guard* — The recommended near-term path. It respects the owner directive and family parity, and it removes the concrete overflow harm.
- *Aspect-locked free resize over the existing transform* — Best UX for the least layout work. It needs a persisted continuous factor (an ADR-0017 domain conflict if stored in int_uiScale, or a new field under the Serialization Registry gate), and it exposes the Linux/X11 resize path that ADR-0010 and Anamorph KI-003 document as a host-crash risk.
- *Fluid (non-uniform) resize with a height-fluid layout* — Largest effort. resized() is height-rigid by construction (row heights from constants), and it would diverge from the family's whole-window-scale grammar.
- *Add intermediate ladder steps (e.g. 110 / 140 %)* — Smaller gaps, but it is an ADR-0017 ladder-domain change needing its own gate clearance and family agreement.

**Decision: Defer · P3.** The finding is factually right, but the stepped model is a recorded owner-directed decision (ADR-0017, DESIGN) that the product shares with its sibling. CLAUDE.md forbids guessing at open decisions, and a cross-product model change is a family decision. The user-visible harm (not fitting) is handled by [UX-020](findings-ux.md#ux-020). This waits for: (1) the owner's fine-review decision reconciling brief §7/§8 with ADR-0017/DESIGN, recorded as an OPEN_QUESTIONS item or an ADR, for both family products; (2) if resize is chosen, a Linux/X11 host resize soak given the documented ConfigureNotify crash path. The spec drift itself proceeds under [DOC-007](findings-doc-test.md#doc-007).

**Architecture gates.**

- Conflict with Accepted ADR-0017 (Decision 1: legal set = five steps; out-of-set values converge on load) if a continuous scale is stored in int_uiScale
- Serialization Registry change (ARCHITECTURE_REVIEW_GATE) if a new persisted window-size or scale field is added instead
- Linux/X11 editor-resize host-crash risk cited in ADR-0010's advancedMode rationale (Anamorph KI-003/KI-007): not a gate category, but it needs explicit review before enabling host resize

**Dependencies.** [UX-020](findings-ux.md#ux-020); [DOC-007](findings-doc-test.md#doc-007); Owner decision (fine review) on stepped vs free resize for the Anabasis/Anamorph family

**Acceptance criteria.**

- An owner decision on stepped vs free resize is recorded (OPEN_QUESTIONS entry or ADR) and names whether it applies to Anamorph too
- If stepped is retained: DEVELOPMENT_BRIEF §7/§8 and BRAND_CONSISTENCY_CHECKLIST D describe the XS..XL ladder instead of '80–200 %' and 'resizable' (via [DOC-007](findings-doc-test.md#doc-007))
- If resize is adopted: the VST3 view reports canResize true, the Standalone window gets a resize edge, and dragging produces a continuous aspect-locked scale between the chosen limits with no empty band in either mode
- If resize is adopted: an ADV toggle keeps the current scale factor, and the size survives a session save and reload
- If resize is adopted: a rapid open/close plus resize soak in a Linux/X11 host shows no crash, and pluginval passes at the build.yml strictness

<details><summary>Verification record</summary>

**Method.** Grepped e769f33:src/gui for setResizable, setResizeLimits and constrainers: 0 hits. The JUCE 9.0.1 default resizableByHost is false (juce_audio_processors/processors/juce_AudioProcessorEditor.h:284), VST3 canResize returns kResultFalse unless isResizable (juce_audio_plugin_client_VST3.cpp:2096-2104), and the Standalone does setResizable(editor->isResizable()) (juce_StandaloneFilterWindow.h:713). Read the brief (e769f33:docs/DEVELOPMENT_BRIEF.md:160 'UI scaling (80–200%)', :169 'resizable, HiDPI-aware, with window size persistence'), DESIGN (e769f33:docs/DESIGN.md:867-870 'free host resize is still off'), ADR-0017 (the owner-directed sibling ladder, gate cleared 2026-08-06), and e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:91 (unchecked 'resizable'). Read resized()/layoutSimple: heights come from constants (e769f33:src/gui/PluginEditor.cpp:1704, 1757-1758 use kSimpleH/kBarH) and widths from getWidth(). Viewed session capture `rt/layout/24c.png` (no corner, bounds unchanged) and 25c.png (harness-only border resize: an empty band below the well, which corroborates the height-rigid code). Checked Anamorph: setResizable(false,false) and the same {0.75..1.5} ladder (Anamorph fd78c3b:src/PluginEditor.cpp:681, 1794).

**Corrections to the candidate claim.** The facts are correct. The framing needs correcting: this is not an unintended omission. DESIGN.md records free resize as deliberately off, the ladder is an owner directive recorded in Accepted ADR-0017, and the sibling product has the identical fixed-size model. The conflict is between the brief/checklist and a later recorded decision, which is [DOC-007](findings-doc-test.md#doc-007) drift plus an owner decision not yet taken; it is not an implementation defect. 'Window size persistence' IS met, per session, through int_uiScale. The height-rigid layout matters only for a FLUID layout: aspect-locked resize on top of the existing whole-window transform would not need resized() to change. No OPEN_QUESTIONS entry records the resize question (grep: 0 hits).

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 2 · severity 1 · discoverability 2 · efficiency 2 · coherence 3 · change risk 4 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-022

**The wordmark is an invisible About button: a 330 px ghost hit area over the wordmark and subtitle, with no hover state, cursor change, tooltip or accessible name**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | partially-confirmed | Interaction model | Controls do not show whether they are live or what they select, and the explanation sits in tooltips that ship off | Phase 5 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:313-316 — titleButton.setButtonText({}); setComponentID("ghost"); onClick → showAbout(true); no setTooltip / setTitle / setMouseCursor
- e769f33:src/gui/PluginEditor.cpp:1405 — titleButton.setBounds(bar.getX()+8, 0, 330, kBarH)
- e769f33:src/gui/PluginEditor.cpp:1307-1310 — wordmark painted at x=18 (w 240), subtitle at x=162 (w 190); the ghost covers both
- e769f33:src/gui/LookAndFeel.cpp:321 — drawButtonBackground returns immediately for "ghost"; titleButton is not registerAnimated, so no hovA exists
- e769f33:src/gui/PluginEditor.h:92-95 + e769f33:src/gui/PluginEditor.cpp:682 — aboutText=true: any mouseDown on the backdrop dismisses; no keyPressed/Escape handler anywhere in PluginEditor.cpp
- e769f33:src/InternalState.h:110 — tooltipsOn defaults to false
- e769f33:docs/user/USER_MANUAL.md:3-4 and :157 — the manual tells users to click the ANABASIS title
- e769f33:docs/DESIGN.md:845 — 'wordmark + sub-brand left (ghost button → About)'
- e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:45-46 — About page: 'how it is opened' must match the sibling
- Anamorph@fd78c3b:src/PluginEditor.cpp:335-337, 2259; Anamorph@fd78c3b:src/gui/LookAndFeel.cpp:369 — the identical ghost pattern (300 px), no tooltip
- JUCE 9.0.1 build/_deps/juce-src/modules/juce_gui_basics/buttons/juce_Button.cpp:484-497 — the click fires on mouseUp with no mouse-button filter
- JUCE 9.0.1 build/_deps/juce-src/modules/juce_gui_basics/detail/juce_ButtonAccessibilityHandler.h:67-72 — the accessible title falls back to the button text, which is empty here
- Runtime V12-2: session capture `rt/verify-12/20-logo-rest.png` vs 21-logo-hover.png and 22-subtitle-hover.png — max pixel diff (0,0),(0,0),(0,1) over the bar's left 356 px
- Runtime V12-3: session capture `rt/verify-12/29-about-sheet.png` — left-click opens About; Escape leaves it open; a click closes it; a right click on the subtitle opens About; a right click on Settings opens Settings
- Phase-2 LAY-13(a): session capture `rt/layout/26c-about.png`

**Current behaviour.** A 330×46 px invisible TextButton sits over the wordmark and subtitle. Hovering it changes nothing: no wash, no cursor change, and no tooltip even with Tooltips on. Clicking it with any mouse button opens the About overlay. Any further click closes the overlay, and Escape does not. Screen readers see a button with no name.

**Problem.** The only route to version and build information, which users need for bug reports and support, has no visual, pointer or accessible affordance. It is found by reading the manual or by accident, and assistive technology cannot identify it.

**Root cause.** A family convention copied from Anamorph as-is: the componentID 'ghost' makes the LookAndFeel skip all painting, the button is not registered with the micro-animation driver, and no tooltip, title or cursor is set. Tooltips being off by default removes the one generic hint Anabasis has.

**User impact.** It is rarely needed (version checks and support), so the cost is low. A user asked for their version who has not read the manual may not find it. A screen-reader user hears an unnamed button at the start of the top bar. *Scope:* One control (titleButton) in both views. The right-click activation is editor-wide (every juce::TextButton) and is out of scope here; it is listed under new_findings.

**Proposed improvement.** Keep the wordmark as the About opener, as the family requires. Register titleButton for the hover driver and, on hover, brighten the painted wordmark (text → a brighter tone, eased by hovA). Set the mouse cursor to PointingHandCursor over the hit area. Call titleButton.setTitle("About Anabasis") for accessibility and setTooltip("About"), which stays governed by the Tooltips switch. Optionally narrow the hit area to the painted wordmark and subtitle extent. Make Escape close About, covered separately by LAY-18.

**Alternatives considered.**

- *Leave as-is (family convention)* — Acceptable visually, since it matches Anamorph and is documented in the manual, but it leaves the accessible-name gap. That gap is a Brief §8 / checklist D requirement, not a style choice.
- *Add a dedicated 'i' / About button to the right cluster* — Rejected: it adds chrome to a full top bar and changes 'how About is opened', a checklist-A must-match item.
- *Filter the About button to left-click only* — Rejected for this finding. Right-click activation is generic JUCE behaviour on every button and harmless here; any change should be editor-wide.
- *Accessible title and tooltip only, no visual hover* — Viable minimum (no visual change for the brand pass to rule on), but it does nothing for the sighted default user because tooltips are off.

**Decision: Modify · P3.** The evidence supports an affordance gap, and a real accessibility gap, on a low-frequency path. The obvious fix (a visible About button) conflicts with the family must-match on how About opens. The constrained change keeps the interaction identical and adds only a hover cue, a pointer cursor and an accessible name. The visual part should be logged for the Level-5 brand pass, because Anamorph cannot be changed from here.

**Architecture gates.**

- No ARCHITECTURE_REVIEW_GATE category touched.
- BRAND_CONSISTENCY_CHECKLIST A 'About page — how it is opened' (Level-5 brand pass): a hover cue is a visual divergence from Anamorph and must be recorded as a deliberate deviation or proposed family-wide.

**Dependencies.** LAY-18 (Escape does not dismiss overlays); [UI-018](findings-ui.md#ui-018) (family-deviation ledger in BRAND_CONSISTENCY_CHECKLIST)

**Acceptance criteria.**

- With the pointer moved onto the wordmark from outside the bar, a screenshot after 300 ms differs from the at-rest capture in the wordmark region (max channel diff > 8), and moving away restores it.
- The X cursor over the title hit area is the pointing-hand cursor, and it is the normal cursor elsewhere in the bar.
- titleButton.getTitle() == "About Anabasis" (or an owner-approved string), and the JUCE accessibility handler reports a non-empty title, asserted in a state test next to the existing arming-site tests.
- With Tooltips ON, resting on the wordmark shows 'About'. With Tooltips OFF (the default), no tooltip appears.
- A left click on the wordmark still opens the About overlay, and the version/build line is unchanged.
- The BRAND_CONSISTENCY_CHECKLIST candidate list names the hover cue.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/PluginEditor.cpp:313-316, 1405, 1305-1310 and 682; e769f33:src/gui/PluginEditor.h:85-97, 449-450; e769f33:src/gui/LookAndFeel.cpp:321; e769f33:src/InternalState.h:110; and USER_MANUAL.md:3-4, 157. Checked the Anamorph pattern (Anamorph@fd78c3b:src/PluginEditor.cpp:335-337, 2259; e769f33:src/gui/LookAndFeel.cpp:369) and the JUCE 9.0.1 Button and accessibility sources. Runtime on :142 with stepped moves and a relative wiggle (V12-2): pixel diff of the top bar at rest vs hovering the wordmark, and vs hovering the subtitle, was at most 1 level, i.e. no hover state. V12-3: a left click on the wordmark opened About. Escape left it open. A click anywhere closed it. A right click on the SUBTITLE opened About, and a right click on the Settings button opened Settings.

**Corrections to the candidate claim.** (1) Right-click activation is not specific to the wordmark. JUCE Button::mouseUp fires on any mouse button (juce_Button.cpp:484-497), so every TextButton in the editor behaves this way; right-clicking Settings opens Settings. (2) The hit area (local x 8..338) covers the wordmark (painted 18..~146) AND the 'MASTERING MAXIMIZER' subtitle (painted from x=162), so the button is larger than the logo. (3) This is not an unintended leftover. DESIGN §6.1 prescribes 'ghost button → About' (e769f33:docs/DESIGN.md:845), Anamorph ships the same ghost (300 px, no tooltip), and BRAND_CONSISTENCY_CHECKLIST A requires About to be opened the same way as the sibling. (4) The manual documents the affordance twice (header line 3-4 and §3.1), so it is discoverable for manual readers. (5) Tooltips are OFF by default (InternalState.h:110), so adding a tooltip alone would not help a default user. (6) Added: the button has no accessible name. Both its title and its text are empty, and JUCE's ButtonAccessibilityHandler falls back from the title to the button text.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 1 · severity 1 · discoverability 4 · efficiency 1 · coherence 2 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-023

**In the Standalone, the JUCE wrapper's banner 'Settings...' button (audio device and input mute) sits just above the plugin's own 'Settings' button (plugin preferences). The banner shows on every first launch, not only when there is no device**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Documentation | User and design documents contradict the shipped product | Phase 1 |

**Evidence**

- JUCE 9.0.1 (fetched) juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h:924-929: NotificationArea hard-codes 'Audio input is muted to avoid feedback loop' and a TextButton labelled "Settings..."; :984-989: that button calls showAudioSettingsDialog()
- JUCE 9.0.1 juce_StandaloneFilterWindow.h:344: shouldMuteInput defaults to true on first run; :868-876: the banner's visibility follows the mute value alone; :563-567: a muted input is replaced with an empty buffer
- e769f33:src/gui/PluginEditor.h:454: juce::TextButton settingsButton { "Settings" }; e769f33:src/gui/PluginEditor.cpp:368: onClick → showSettings(true), the plugin preferences overlay
- e769f33:src/gui/LookAndFeel.h:165-167: the Standalone window resolves the DEFAULT look-and-feel, so the wrapper chrome is unstyled JUCE
- e769f33:CMakeLists.txt:289-290: stock Standalone format; git grep at e769f33 finds no JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP
- e769f33:docs/user/USER_MANUAL.md:119-120: 'Pick your audio hardware in the standalone's audio settings on first launch'. It does not say which Settings, and does not mention 'Mute audio input'. e769f33:docs/user/INSTALLATION.md:115-116: the same
- e769f33:docs/architecture/design-decisions/ADR-0008-build-architecture-and-plugin-identity.md:163-164 and e769f33:docs/DEVELOPMENT_BRIEF.md:47: the Standalone is the debugging and manual-audition format, but it ships to users (e769f33:docs/user/INSTALLATION.md:17-18)
- Runtime E01 (edges): session capture `rt/edges/01-standalone-nodevice.png`, session capture `rt/edges/02-standalone-options-menu.png`, session capture `rt/edges/03-standalone-audio-settings.png`
- Runtime V13-1, no device, stepped clicks: session capture `rt/verify-13/01-sa-launch.png` (banner plus top-bar Settings), session capture `rt/verify-13/02-banner-settings-clicked.png` (Audio/MIDI Settings window), session capture `rt/verify-13/03-plugin-settings-clicked.png` (plugin Settings overlay)
- Runtime V13-2, working ALSA null default device, fresh HOME: session capture `rt/verify-13/12-nulldev-firstrun.png` (banner shown, all meters '-', GR 0 dB line drawn) and session capture `rt/verify-13/13-crop.png` (the audio dialog now lists channels, rate and buffer)
- *Added from another verifier's note:* Add to [UX-023](findings-ux.md#ux-023)'s §2.5 rewrite: USER_MANUAL.md:117-119 calls the Standalone 'useful for checking a file', but the stock StandaloneFilterWindow has no file player. I re-checked with git grep at e769f33 over src and CMakeLists for AudioFormatReader, AudioTransportSource, AudioFormatManager and JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP: nothing. The rewrite must drop 'checking a file' or qualify it as 'via a loopback or virtual input device'.

**Current behaviour.** On every first launch of the Standalone, and whenever input stays muted, a yellow JUCE banner reads 'Audio input is muted to avoid feedback loop' and has a 'Settings...' button that opens the Audio/MIDI dialog. Just below it, the plugin's own top-bar 'Settings' button opens oversampling, phase, metering and UI preferences, with nothing about audio devices. The input is muted by default, so even with a correctly selected device the plugin receives silence and every meter reads '-'.

**Problem.** Two buttons with almost the same label and similar styling open unrelated panels. The one the user actually needs on first run is the unstyled wrapper banner button, whose message talks about feedback rather than 'nothing is being processed'. The manual sends the user to 'the standalone's audio settings' without saying which Settings, and never mentions unticking 'Mute audio input'.

**Root cause.** The stock JUCE StandaloneFilterWindow is used unchanged (no custom Standalone app), and its NotificationArea hard-codes the 'Settings...' label (juce_StandaloneFilterWindow.h:924-929). JUCE defaults input mute on for any processor with inputs and outputs (:344, :453). The plugin independently labels its preferences button 'Settings' (PluginEditor.h:454). The product docs (USER_MANUAL §2.5, INSTALLATION) do not describe the wrapper chrome.

**User impact.** A first-time Standalone user who has selected a device still sees and hears nothing. The obvious 'Settings' button in the plugin shows oversampling and metering options with no device or input control, so the product looks broken rather than idle. Recovery is cheap once the user finds the banner button and the checkbox, but nothing points there. *Scope:* Standalone format only (Linux, Windows, macOS). Every first launch, and any later launch while input stays muted. VST3 and AU are unaffected. It combines with [VIS-012](findings-visualisation.md#vis-012) and [VIS-013](findings-visualisation.md#vis-013) (unexplained dashes and a blank GR well) to make the plugin look broken.

**Proposed improvement.** Documentation first. Rewrite USER_MANUAL §2.5 and the INSTALLATION troubleshooting line to say: 'Choose the device under Options → Audio/MIDI Settings… (or the Settings… button on the yellow banner). At first launch the input is muted to prevent feedback: untick Mute audio input to process a live or loopback input. The Settings button in the Anabasis top bar holds the plug-in's own preferences (oversampling, phase, metering, UI) and has no device settings.' A user who follows §2.5 on a machine with a working device then reaches moving meters without guessing. Relabelling the banner (for example 'Audio Settings…', with a message such as 'Input muted — nothing is being processed') needs a custom Standalone app, and is deferred.

**Alternatives considered.**

- *Leave as-is* — Rejected. The manual actively points users to an ambiguous 'audio settings', and the input-mute default is undocumented.
- *Rename or iconify the plugin's top-bar Settings button* — Rejected. It would change every format to work around a Standalone-only collision.
- *Custom Standalone app (JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP) that relabels the banner button and rewords the message* — Deferred. This is a CMake compile-definition change plus a maintained copy of the JUCE wrapper, so it is build-system gated. Worth doing only if the Standalone is promoted beyond its ADR-0008 'debugging and manual-audition' role.
- *Default input unmuted* — Rejected. It removes JUCE's feedback protection for mic-plus-speaker setups, and it also needs a custom app.

**Decision: Modify · P2.** The defect is real and reproduced, including with a working device. The smallest change that removes the trap is a documentation correction that names the wrapper controls and the mute checkbox. Changing the wrapper itself is build-system gated and disproportionate for a format the brief calls optional.

**Architecture gates.**

- None for the documentation fix
- The deferred custom-Standalone-app alternative would be a build-system change (CMakeLists.txt compile definition) that also touches the ADR-0008 format decision, and needs human review

**Dependencies.** [STATE-016](findings-state-model.md#state-016); [VIS-012](findings-visualisation.md#vis-012); [VIS-013](findings-visualisation.md#vis-013)

**Acceptance criteria.**

- USER_MANUAL §2.5 names 'Options → Audio/MIDI Settings…' and the banner's 'Settings…' as the device controls
- USER_MANUAL §2.5 states that 'Mute audio input' is ticked at first launch and must be unticked to process an input
- USER_MANUAL §2.5 states that the top-bar Settings button holds plug-in preferences, not audio devices
- INSTALLATION.md's 'Standalone needs audio' line says the same
- On a machine with a working device and a fresh settings file, a tester following only §2.5 gets moving meters

<details><summary>Verification record</summary>

**Method.** Read the JUCE 9.0.1 wrapper in the fetched tree (build/_deps/juce-src, tag 9.0.1): juce_StandaloneFilterWindow.h:344, 563-567, 868-876, 924-929, 984-989. Confirmed at e769f33 that nothing overrides the wrapper: there is no JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP (git grep), and CMakeLists.txt:289-290 adds the stock Standalone format. Read the plugin button at e769f33:src/gui/PluginEditor.h:454 and PluginEditor.cpp:368. Viewed rt/edges/01, 01-crop, 02 and 03. Reproduced on my own display :143 with stepped pointer motion (4 steps, 1 px jiggle, 120 ms hover), using the Standalone binary from build/ (built 2026-09-08; since then the only src change is src/dsp/AnabasisEngine.h, per git log). (a) No device: clicking the banner 'Settings...' opened a separate 'Audio/MIDI Settings' window (Mute audio input, Output/Input none). Clicking the top-bar 'Settings' opened the plugin overlay (Oversampling, Phase, Offline Render, Integrated, RMS Reference, UI Scale, UI Animations, Tooltips). Neither panel mentions the other. (b) Fresh HOME with an ALSA null default device, so audio really ran (GR reference line drawn): the banner still showed, and every meter read '-'.

**Corrections to the candidate claim.** 1) The banner is not a no-device indicator. It shows whenever the wrapper's 'Mute audio input' is on. JUCE turns that on by default at first launch (getBoolValue("shouldMuteInput", true), line 344), and the guard applies because Anabasis has both inputs and outputs (line 453). So even with a working device, the first launch shows the banner and processes silence (the input is cleared, lines 563-567). 2) The buttons are not side by side: the banner's 'Settings...' is at the right end of the yellow row, and the plugin's 'Settings' is in the top bar about 38 px lower and about 167 px to the left. 3) The Options menu offers the same audio dialog as a third entry point. 4) The banner disappears once input is unmuted, so the collision exists only while input is muted.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 2 · severity 2 · discoverability 4 · efficiency 2 · coherence 3 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UX-024

**Preset housekeeping happens only in a hidden OS folder the plugin cannot open: the preset menu has no rename, delete or 'show folder' action, and the manual sends users to %APPDATA%, ~/Library or a ~/.config dot-folder**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | recorded at triage | Preset/state workflow | Save, load, browse and restore change or lose state without saying so | Phase 4 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:2231-2233 — the menu's only file actions are 'Save Preset…' and 'Load Preset…'
- e769f33:src/PresetManager.h:30-34 — userPresetDirectory() = userApplicationDataDirectory/RollyTech/Anabasis/Presets
- e769f33:docs/user/USER_MANUAL.md:386-395 — per-OS folder table; 'There is no in-plugin rename/delete — manage the files in that folder (the menu picks up changes, sorted alphabetically)'
- e769f33:docs/user/USER_MANUAL.md:522-523 — FAQ 'Where are my presets stored? How do I share one?' refers back to §7.2
- [UX-018](findings-ux.md#ux-018) alternatives: 'A show folder action in the preset menu is more useful; out of scope here'
- [UI-015](findings-ui.md#ui-015) corrections — Load Preset… opens the native dialog in the preset folder (a partial, indirect route to the files)
- Anamorph@fd78c3b:src — no reveal-folder action (family-wide)

**Current behaviour.** Users can save and load presets from the top-bar menu. To rename, delete, back up or share a user preset, they have to find the folder by hand from the manual: %APPDATA%\RollyTech\Anabasis\Presets on Windows (AppData is hidden in Explorer by default), ~/Library/RollyTech/Anabasis/Presets on macOS (~/Library is hidden in Finder by default), or ~/.config/RollyTech/Anabasis/Presets on Linux (a dot-folder). The plugin offers no way to open the folder. The only indirect route is Load Preset…, whose native dialog opens there.

**Problem.** The product hands all preset management to a folder it neither shows nor opens, and every OS hides that folder by default. To remove a test preset, fix a mistyped name ([UX-018](findings-ux.md#ux-018)) or share a preset, the user has to leave the plugin and navigate hidden folders.

**Root cause.** The preset menu was ported from the sibling with save and load only. The manual documents the folder path instead of the product exposing the folder.

**User impact.** Moderate friction in an infrequent task: cleaning up after a [UX-003](findings-ux.md#ux-003) overwrite or a [UX-018](findings-ux.md#ux-018) stripped name, renaming, or sharing. Users who cannot find the folder accumulate junk presets in the USER list and the ‹ › ring. Users who rename in the OS file manager then hit [STATE-015](findings-state-model.md#state-015)'s orphaned label.

**Proposed improvement.** 1. Add one item, 'Show Preset Folder', after 'Load Preset…' in the preset menu. On the message thread, call PresetManager::userPresetDirectory().createDirectory(), then File::revealToUser() on the directory (Explorer, Finder or the xdg file manager). Fall back to startAsProcess() on the directory where reveal is unsupported.
2. Update USER_MANUAL §7.2 and the FAQ to name the item, and add one sentence on [STATE-015](findings-state-model.md#state-015): renaming the loaded preset on disk leaves its name without a tick.
3. List the menu addition as a family deviation candidate under the checklist's preset-system item.
In-plugin rename and delete stay out of scope (Defer to the owner and the family).

**Alternatives considered.**

- In-plugin rename and delete (submenus or a manage overlay). A fuller fix, but it adds destructive UI that needs confirm and undo design and a family decision. Defer.
- Show the folder path in the Save overlay. Long, platform-specific text; [UX-018](findings-ux.md#ux-018) already rejected it.
- Leave it manual-only. Hidden-folder navigation stays the only route; acceptable only if the owner rules preset management out of the product.
- Rely on the Load Preset… dialog's context menu. Works on Windows, is limited on macOS and absent in JUCE's Linux fallback browser; not discoverable.

**Decision: Proceed · P2.** Confirmed from code and the manual. The gap is one missing menu action backed by a direct JUCE call, on the message thread only, with no parameter, schema, DSP or threading change. The sibling has the same menu, so the addition is recorded on the brand checklist rather than diverging silently. The larger rename and delete question is deferred to the owner.

**Architecture gates.**

- No hard-stop category. Brand and family: the preset system is a must-match family item (BRAND_CONSISTENCY_CHECKLIST.md:53). Record the addition as a deviation candidate or propose it to the sibling.

**Dependencies.** [UX-003](findings-ux.md#ux-003) (recovering from an overwrite needs the folder); [UX-018](findings-ux.md#ux-018) (moved its folder-location sub-claim here); [STATE-015](findings-state-model.md#state-015) (renaming on disk orphans the loaded identity; the new manual line should say so); [UI-015](findings-ui.md#ui-015) (the Load Preset… dialog is today's indirect route)

**Acceptance criteria.**

- The preset menu shows 'Show Preset Folder' below 'Load Preset…' in both views.
- Choosing it opens the OS file manager at the user preset folder on Windows, macOS and a Linux desktop with a file manager, and creates the folder first if it does not exist.
- It changes no parameter, preset identity, label, tick or undo stack. State test, with the reveal call stubbed: live state and undo depth are identical before and after.
- USER_MANUAL §7.2 and the FAQ name the menu item, and BRAND_CONSISTENCY_CHECKLIST lists the deviation.

<details><summary>Verification record</summary>

What I re-read at e769f33:
- e769f33:src/gui/PluginEditor.cpp:2219-2233 (showPresetMenu): FACTORY rows, USER rows, a separator, then only 'Save Preset…' (10001) and 'Load Preset…' (10002).
- e769f33:src/PresetManager.h:30-34: userPresetDirectory = userApplicationDataDirectory/RollyTech/Anabasis/Presets.
- e769f33:docs/user/USER_MANUAL.md:384-395 (the per-OS folder table and 'There is no in-plugin rename/delete — manage the files in that folder') and :522-523 (the FAQ points back to §7.2).
- [UX-018](findings-ux.md#ux-018)'s decision text, which moved the folder-location sub-claim to a menu affordance that reveals the folder.
- [UI-015](findings-ui.md#ui-015)'s corrections: Load Preset… opens the OS dialog in the preset folder on macOS, Windows and GNOME/KDE, and JUCE's fallback browser otherwise.
A grep of Anamorph@fd78c3b:src for revealToUser, Reveal and 'Show Preset' found nothing, so the gap is family-wide. Not run on macOS or Windows. That AppData and ~/Library are hidden by default is standard OS behaviour, not measured here.

</details>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

