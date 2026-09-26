# Anabasis product / UX audit — findings: Visual interface and layout

Part of [`2026-09-26-anabasis-product-ux-audit.md`](../2026-09-26-anabasis-product-ux-audit.md) (audited revision `e769f33`, 2026-09-26). This file holds the complete record of each finding in these categories; the report carries the index, the systemic themes, the roadmap and the decision record. Code anchors are pinned to `e769f33`; runtime observation ids refer to [`worklogs/2026-09-26-product-ux-audit.md`](../../../worklogs/2026-09-26-product-ux-audit.md).

Each record: decision, priority and confidence after calibration; evidence; current behaviour; problem; root cause; user impact and scope; proposed improvement; alternatives considered; decision rationale (with any calibration or challenge outcome); architecture gates; dependencies; acceptance criteria; and the verification record. Terms in the records: the *candidate claim* is the claim as it entered verification; *the judge* is the verifier's decision pass (Phase 3, step 3), done per *batch* of 3–6 related findings; *Adversarial challenge* is the step-4 review and *Calibration* the Phase-4 pass that set the final decision and priority (see the report's *Evidence and method*). A paragraph marked *Merged at triage from another verifier's note* is evidence from another batch's verifier, kept in its words: 'add to X' there means it has been added to this record. 'Recorded at triage' marks a finding written from such a note. `rt/…` paths and ids such as `VER0-2` or `V24-TSAN-1` name uncommitted session captures, logs and probes; `PF-…` ids are potential findings from the uncommitted Phase-1 evidence maps.

## UI — Visual interface and layout

### UI-001

**Detach indication: the Advanced badge is a 7 px inert dot anchored to the slider cell (left-column badges float between knobs); the Simple edited dot is an unlabelled press-to-act target that re-lands the macro curve**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | confirmed | Feedback/observability | The macro layer overwrites hand edits and automation with almost no notice | Phase 4 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:1352-1363 — badge: `g.fillEllipse (k->getRight() - 10, k->getY() + 2, 7, 7)` from editor paint(), keyed to the slider bounds
- e769f33:src/gui/PluginEditor.h:509-521 — EditedDot: paint + `mouseDown → onClick()`, with no mouseEnter/Exit, cursor or mouseUp handling
- e769f33:src/gui/PluginEditor.cpp:1710 — 14×14 at bigLoudnessK.getRight()-6, getY()+16 (visible circle ~10 px after reduced(2))
- e769f33:src/gui/PluginEditor.cpp:656-659 — tooltip text and `onClick = proc.resetToMacro()`
- e769f33:src/PluginProcessor.h:223-239 — resetToMacro: pushUndoStep, replaceDetachMask({}), relandMacroCurve
- e769f33:src/InternalState.h:110 — tooltipsOn default false
- e769f33:src/gui/PluginEditor.cpp:90-98 and e769f33:CHANGELOG.md:1425-1428 — corner-dot legend removed from knob tooltips by owner directive (0.1.3)
- e769f33:src/gui/PluginEditor.cpp:2120-2131 — Simple shows only presence (non-empty mask), not which or how many
- e769f33:docs/user/USER_MANUAL.md:326-343 — the only full explanation
- Runtime G-14 — session capture `rt/gestures/27a-badge-crop.png` (badge at the Threshold cell's far right), session capture `rt/gestures/25a-dot-hover-crop.png` (tooltip only with tooltips on), session capture `rt/gestures/25b-dot-clicked-small.png` (click clears the dot and '*')
- Runtime E12 — session capture `rt/edges/38e-advanced-all-extremes-crop.png` (Ratio badge in the Ratio/Threshold gap)
- Runtime verify-14 — session capture `rt/verify-14/03-crop.png` (Limiter Gain badge in the Gain/Ceiling gap)

**Current behaviour.** Advanced: a detached managed knob gets a 7 px accent dot at the top-right of its slider cell. The knob face, arc, value and caption are unchanged. The dot has no hover, tooltip or click of its own, and for left-column knobs it sits between two knobs. Simple: any detached parameter shows a 10 px accent dot at the big knob's top-right. There is no hover state and no cursor change, and it shows no count or names. With tooltips off (the default) there is no explanation. Pressing it re-lands the macro curve on all managed parameters immediately on mouse-down, as one undoable step.

**Problem.** Three problems. (1) The detached state is hard to notice and can be attributed to the wrong knob, because the state is carried by a small dot in the gap rather than by the knob. (2) The same accent dot is a passive label in Advanced and an unlabelled, fire-on-press destructive button in Simple, which is inconsistent. (3) Nothing tells the Simple user what is detached or what the dot will do, since the legend was removed and tooltips are off.

**Root cause.** The badge geometry uses slider-cell bounds rather than the rotary face the LookAndFeel draws. The owner directive (0.1.3) removed the in-product legend and nothing replaced it. EditedDot is a bare Component that acts in mouseDown. The editor tick computes only a presence fingerprint, not a summary. Tooltips default to off.

**User impact.** Advanced users miss or misread which knobs are off-macro, for example reading Ratio's badge as Threshold's. Simple users either ignore the dot or click it to find out what it is, and so re-land all of their hand edits (recoverable via Undo only if they understand what happened). This feeds directly into the [MODEL-001](findings-state-model.md#model-001) trap. *Scope:* Every session in which a managed parameter is detached: nine knobs across the COMP, CLIP/COLOR, LIMITER and EQ panels in Advanced, plus the Simple edited dot.

**Proposed improvement.** Advanced: anchor the badge to the rotary face, at the top-right of the arc's bounding square as the LookAndFeel computes it, so that its nearest knob is always its own. Also carry the state on the knob itself with a non-text cue, such as the value readout or arc drawn in a distinct 'manual' tint. Simple: give the edited dot a hover state and pointing-hand cursor. Fire on mouseUp-inside so that press-and-drag-off cancels. Show the number of detached parameters on or beside the dot. Any hover text is owner-supplied (C8) and must respect the 0.1.3 directive, which removed the legend from knob tooltips and did not rule on the dot. Keep reset-to-macro one undo step.

**Alternatives considered.**

- *Re-add the corner-dot legend to the nine knob tooltips* — This contradicts an explicit owner directive (CHANGELOG.md:1425-1428), and tooltips are off by default anyway. Owner decision only.
- *Turn tooltips on by default* — Broader change to a family convention inherited from Anamorph (PF-anamorph-reference-12). It helps the Simple dot but not the Advanced geometry. Owner decision; out of this finding's scope.
- *A detach summary/list popover on the Simple dot* — Best clarity, but it needs owner wording and more UI. It is a good follow-up if the count alone proves insufficient.
- *Leave as-is (manual-only explanation)* — Rejected: runtime shows ambiguous placement and an unlabelled destructive target in the default configuration.

**Decision: Proceed · P2.** The changes are geometry and interaction only: badge anchor, knob-borne cue, hover, cursor and mouseUp semantics. They touch no ADR contract, parameter or state, and they address confirmed ambiguity and an inconsistent press-to-act pattern. The text elements are separated out as owner decisions.

**Architecture gates.**

- None for geometry and interaction
- Conditional: if the badge or tint reads detach state from any paint() path, it must use [TECH-002](findings-dsp-tech.md#tech-002)'s published scalar. Otherwise it widens the unlisted Message→Painting path (THREADING_POLICY.md:26-28; ADR-0027/0038), which is a threading-model gate
- Any new visible text is owner-supplied (C8) and must respect the 0.1.3 owner directive (CHANGELOG.md:1425-1428)

**Dependencies.** [MODEL-001](findings-state-model.md#model-001) (the re-engage notice is part of the same detach-feedback design); [TECH-002](findings-dsp-tech.md#tech-002) (a redesigned badge or knob tint read from paint must use the published bitmask)

**Acceptance criteria.**

- At every UI-scale step and for each of the nine managed knobs, the badge's nearest rotary face (by centre distance) is its own knob's. This is checked by a geometry test over the laid-out editor.
- A detached knob is distinguishable from an attached one with the badge region cropped out (knob-borne cue).
- Simple edited dot: hovering changes its appearance and cursor. A press followed by dragging off before release leaves the mask and all values unchanged. Press and release inside re-lands the curve and adds exactly one undo step.
- The Simple view conveys how many managed parameters are detached (e.g. '2' for Threshold + Limiter Gain).

<details><summary>Verification record</summary>

**Method.** Code read at e769f33: the badge paint (e769f33:src/gui/PluginEditor.cpp:1352-1363: 7×7 at k->getRight()-10, k->getY()+2, painted by the editor, not a component); EditedDot (e769f33:src/gui/PluginEditor.h:509-521: paint + mouseDown only, with no hover, cursor or mouseUp); its bounds (PluginEditor.cpp:1710); onClick → resetToMacro (:656-659; e769f33:src/PluginProcessor.h:223-239: undo step, clear mask, re-land); visibility (PluginEditor.cpp:2120-2131, 1811); tooltips default off (e769f33:src/InternalState.h:110); owner directive removing the legend (PluginEditor.cpp:90-98; CHANGELOG.md:1425-1428). Screenshots viewed: 27a-badge-crop, 27-adv-sheet, 27-simple-sheet, 25a-dot-hover-crop, 25b-dot-clicked-small, 38e-advanced-all-extremes-crop. Own run on :144 (session capture `rt/verify-14/03-crop.png`): the Limiter Gain badge also floats in the gap between the Gain and Ceiling knobs, the same pattern as Ratio in 38e.

**Corrections to the candidate claim.** (1) The badge is 7×7 px at 100 % scale. G-14's '5 px' is its anti-aliased appearance. (2) The ambiguity is systematic, not specific to Ratio: every managed knob in a left grid column (Ratio, Limiter Gain) puts its badge in the inter-column gap, because the anchor is the slider-cell edge, not the rotary face. Right-column badges (Threshold) sit at the panel edge, far from their knob. (3) 'Resets nine parameters': resetToMacro re-lands the curve on all nine, but only off-curve values change, which in practice means the detached ones plus any automation-written ones ([MODEL-003](findings-state-model.md#model-003)). It is one undo step. (4) The Advanced badge lies inside the slider's bounds, so pressing it presses the managed knob, which starts a gesture on that parameter; with no movement nothing changes. 'Inert' is accurate for the badge itself. (5) A miss beside the Simple dot lands on the Loudness knob bounds, which is also a re-engaging macro gesture ([MODEL-001](findings-state-model.md#model-001)). In that corner, hit or miss, a press re-engages.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 3 · severity 2 · discoverability 4 · efficiency 2 · coherence 3 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-002

**The inline value editor is a stock JUCE TextEditor in a 14 px readout box. It clips glyph bottoms, is left-aligned against a centred readout, hides the leading minus under a blue caret and drops the unit**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | partially-confirmed | Visual hierarchy | Pointer and text entry run on untuned JUCE defaults, so ordinary slips rewrite values | Phase 3 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:1136 — setupRotary gives every knob setTextBoxStyle(TextBoxBelow, false, 72, 14)
- e769f33:src/gui/PluginEditor.cpp:577 — the Input Gain and SC HPF faders use TextBoxRight 62x14
- e769f33:src/gui/LookAndFeel.cpp:938-953 — createSliderTextBox makes a centred Label with a 13 pt font and sets its colours. It sets no CaretComponent::caretColourId, no editor justification and no indents
- e769f33:src/gui/LookAndFeel.cpp:744-747 — getLabelFont returns 13 pt, and juce::Label::createEditorComponent applies it to the editor (JUCE juce_Label.cpp:326-337)
- e769f33:src/gui/LookAndFeel.cpp:927-934 — editorShown sets rawEditText (unit stripped, #36) and then calls selectAll()
- JUCE 9.0.1 juce_TextEditor.cpp:1878-1884 — selectAll moves the caret to the end and then back to 0, so the caret sits on the first glyph
- JUCE 9.0.1 juce_TextEditor.h:808,832 — the stock border is {1,1,1,3} and the indents are 4/4. juce_LookAndFeel_V4.cpp:709-710 draws a 2 px focused outline. juce_LookAndFeel_V4.cpp:1364 sets caretColourId to defaultFill (blue)
- e769f33:src/gui/LookAndFeel.cpp:711-716 and e769f33:src/gui/PluginEditor.cpp:912-916 — the value-box editors are deliberately left on the JUCE default outline
- e769f33:src/gui/PluginEditor.cpp:923-927 — the Save Preset field has palette colours but no caret colour either (blue caret in session capture `rt/edges/60a-save-panel-prefilled-crop.png`)
- verify-26/ceiling-editor (display :156, stepped motion): session capture `rt/verify-26/01-sheet.png` shows six frames 230 ms apart of the Ceiling editor at 8x. The bottoms of '-0.10' are cut in every frame, and the '-' is hidden under the blue caret in 4 of 6 frames
- verify-26/loudness-editor: session capture `rt/verify-26/10-12-sheet.png` — the editor shows '0.5' left-aligned in the outlined box, the '%' is gone and the glyph bottoms are cut
- G-20 / E06 / LAY-13: session capture `rt/gestures/15a-loud-dblclick-valuebox-crop.png`, session capture `rt/gestures/22c-inputgain-valuebox-editor.png`, session capture `rt/edges/32b-ceiling-editor-8x.png`, session capture `rt/edges.partial-stopped/40-ceiling-dblclick-4x.png`, session capture `rt/layout/26c-dblclick-ceilval.png` (all viewed; they agree with the above)
- *Merged at triage from another verifier's note, quoted as written:* A stale comment in the function [UI-002](findings-ui.md#ui-002) changes. e769f33:src/gui/LookAndFeel.cpp:942 says the 13 pt value-box font is an 'explicit default; Simple mode enlarges it (#A)'. Re-checked: no code under src/gui changes a value-box font; only outLufsValue sets its own (PluginEditor.cpp:652). The comment was inherited from Anamorph's applyWidenFonts. Correct it when [UI-002](findings-ui.md#ui-002) reworks createSliderTextBox/ValueBox.

**Current behaviour.** Double-clicking any value readout opens a stock JUCE TextEditor filling the 72x14 box (62x14 on the two faders). It has a 2 px grey outline and shows the number without its unit, left-aligned, with all text selected and the blue JUCE caret at index 0. The 13 px glyphs start 5 px down, so their lower rows are cut off by the box. The caret draws over the first glyph, which is the minus sign of every Ceiling value except 0.00.

**Problem.** The number the user is about to confirm is only partly legible. On the Ceiling, the most delivery-critical value, the sign blinks in and out under the caret. The editor also moves the number from the readout's centred axis to the left edge, drops the unit that tells the user which scale they are typing in, and uses a colour that is not in the product palette.

**Root cause.** ValueBox inherits juce::Label::createEditorComponent unchanged. That produces a TextEditor with JUCE's default justification (top-left), indents (4/4), border ({1,1,1,3}), caret colour (V4 defaultFill blue) and focused outline (2 px), sized into a readout box only 1 px taller than its font. The LookAndFeel styles the idle Label but never the editor it spawns. The unit is removed on purpose by rawEditText (#36).

**User impact.** Anyone typing a value, most often the Ceiling during a mastering pass, sees clipped digits, a sign that flickers under the caret, and no unit. That makes it harder to confirm the entry before pressing Return, and it looks unfinished next to the rest of the UI. Nothing is written wrongly by the styling itself. *Scope:* Every value-box editor: about 40 rotary knobs in both views, plus the Input Gain and SC HPF faders. The caret colour issue also affects the Save Preset name field. The Anamorph sibling has the same stock editor (80x15 box, same LookAndFeel code), so this is inherited family behaviour.

**Proposed improvement.** Target experience: double-clicking a readout turns it into an entry field on the same centred axis, in the same font size, with every glyph fully inside the field. The text is selected with the caret at the END (after the digits, never over the sign), the caret and focus rim use palette colours (text or accent), and a dim non-editable unit suffix stays visible while typing. Mechanism: override Label::createEditorComponent in ValueBox (JUCE juce_Label.h:304 is virtual). There, set justification to centred, set indents and border so the text is vertically centred in the box, set CaretComponent::caretColourId and the outline colours from the palette, and place the caret at the end of the selection instead of calling selectAll(). Raise the text-box height from 14 to about 16 px at PluginEditor.cpp:577 and :1136, or shrink the editor font to fit. Set the caret colour once in the LookAndFeel so the Save field matches. The unit suffix must not change what the parser receives, or it must be paired with the no-change guard in the new finding (open-and-confirm multiplies sub-1 % values by 100).

**Alternatives considered.**

- *Leave as-is (family parity with Anamorph's identical stock editor)* — Rejected. The clipping and caret-over-sign problems affect the legibility of the delivery-critical value, and parity with a sibling that has the same defect is not a reason to keep it.
- *LookAndFeel colour-only fix (caretColourId plus outline colours)* — Cheap and removes the off-palette caret, but leaves the clipping, the left alignment and the caret sitting on the sign.
- *ValueBox::createEditorComponent override (justification, indents, border, palette caret/outline, caret at the end) plus a 16 px box* — Chosen. It is display-only and local to one class, and needs a layout check of the knob cells.
- *Replace inline entry with an overlay field like Save Preset* — Not justified. It adds a second entry idiom and a larger departure from the family grammar.

**Decision: Proceed · P2.** The defect is confirmed at code level and by my own runtime reproduction. The corrected mechanism (caret overdraw rather than scrolling) is no less harmful, because the sign is still unreadable half the time. The recorded intent covers only the outline and the unit drop. The outline can stay neutral as long as it uses the palette. The unit drop's original purpose, letting the user type a bare number, is kept by showing the unit as a non-editable suffix. The change touches no parameter, no state and no DSP.

**Dependencies.** [INPUT-017](findings-input.md#input-017) (percent open-and-confirm multiplies a value of 1 % or less by 100) — any change to the editor's text or unit display must keep 'open + Return without typing = no change'; [INPUT-001](findings-input.md#input-001) (the rejection cue for invalid entry lives in this editor); [INPUT-008](findings-input.md#input-008)

**Acceptance criteria.**

- With the Ceiling at -0.10 dB, double-clicking its readout shows '-0.10' with no glyph row cut off by the field edge, both at 1x and in an 8x crop
- The '-' of a negative Ceiling value is visible in both caret-blink phases (captures at least 250 ms apart), and the caret sits after the last character when the editor opens
- The edit text's horizontal centre is within 1 px of the idle readout's centre, for a knob box (72 px) and a fader box (62 px)
- No pixel of JUCE's default caret blue (0xff42a2c8) appears in the value editor or the Save Preset field. Caret and focus rim use colours from the colours:: palette
- The unit (dB, dBTP, %, Hz, kHz, ms) stays visible while editing, and opening an editor then pressing Return without typing leaves the parameter bit-identical for every parameter (including percent values in (0, 1])
- Knob captions and neighbouring controls do not overlap the taller box in either view at every UI scale

<details><summary>Verification record</summary>

**Method.** Opened every cited anchor at e769f33 and the vendored JUCE 9.0.1 sources: juce_Label.cpp:326-337 and 493-511, juce_TextEditor.h:808,832, juce_TextEditor.cpp:1878-1884, juce_LookAndFeel_V4.cpp:709-710,1364. Viewed the screenshots 15a, 22c, 32b-crop, 32b-8x, 40-4x (partial), 26c and 60a. Reproduced on display :156 with stepped motion (4 intermediate moves before each double-click). With the Ceiling at -0.10 dB I captured six 8x frames of the open editor, 230 ms apart (verify-26/01-sheet.png). I also opened the Loudness editor at 0.5 % (verify-26/10-12-sheet.png).

**Corrections to the candidate claim.** (1) LAY-13(d) says the Ceiling's leading '-' is 'scrolled out of view'. That is refuted. The '-' is present: it is visible in caret-off frames 1 and 4. In frames 2, 3, 5 and 6 the blinking caret draws over it, because JUCE's TextEditor::selectAll() leaves the caret at index 0 (juce_TextEditor.cpp:1878-1884), exactly on the sign. (2) Dropping the unit is deliberate: see the '#36 raw number only' comment and rawEditText (LookAndFeel.cpp:796-817, 931). The default grey outline is also a recorded choice ('value boxes unchanged', LookAndFeel.cpp:716; 'the LookAndFeel deliberately leaves [ValueBox fields] at the default outline', PluginEditor.cpp:912-916). Neither is an oversight. Clipping, left alignment and the caret colour have no recorded rationale. (3) The editorShown anchor is :927-934, not :924-931. (4) The precise clipping mechanism: the stock editor has border-top 1 plus topIndent 4 (juce_TextEditor.h:808,832), and draws a 2 px focused outline (juce_LookAndFeel_V4.cpp:709-710). That leaves about 10 px for a 13 px font from getLabelFont (LookAndFeel.cpp:744-747), so the text starts at y=5 and runs past the bottom edge.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 3 · severity 2 · discoverability 2 · efficiency 2 · coherence 3 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-003

**Readout precision and units vary: '0 %' vs '60.8 %', unitless Character/Tone/Odd-Even, a 2-dp Ceiling beside 1-dp dB knobs, '300 Hz' vs '3.00 kHz', and M/S/I and PLR meter rows with no unit**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | partially-confirmed | Visual hierarchy | The inherited frame, renderer and palette were not re-derived for this product's content or the user's display | Phase 5 |

**Evidence**

- e769f33:src/PluginParameters.cpp:85 — dbText prints 1 dp. :87-113 — pctText prints an integer for whole percents and 1 dp otherwise, with the stated owner rationale. :114-115 — hzText prints integer Hz below 1 kHz and 2-dp kHz above
- e769f33:src/PluginParameters.cpp:283-288 — Character and Tone use String(v,2) with no unit. e769f33:src/PluginParameters.cpp:303-308 — the Ceiling uses String(v,2) plus ' dB'/' dBTP' (ADR-0024 / ADR-0015)
- e769f33:src/gui/LoudnessMeterView.cpp:184 — the M/S/I numbers are drawn without a unit. :243-275 — TP ' dBTP', SP/RMS ' dBFS', LRA ' LU', PLR with no unit
- e769f33:src/gui/PluginEditor.cpp:646 and :2041 — the 'out LUFS' caption carries the unit and the value is bare
- e769f33:docs/user/USER_MANUAL.md:172-173 (Character 0…1, Tone −1…+1) and :226-233 (Statistics rows)
- LAY-08: session capture `rt/layout/04c-limiter.png`, [capture](captures/03-knob-arcs-at-defaults.png). G-19: session capture `rt/gestures/00-initial.png`, session capture `rt/gestures/27i-simple-after-loudness-drag.png`. Viewed session capture `rt/gestures/22c-inputgain-valuebox-editor.png`, which shows Gain '0.0 dB' beside Ceiling '-0.10 dB', '300 Hz' vs '3.00 kHz', Odd/Even '0.00', M/S/I '-12.3/-11.0/-11.2' and PLR '12.4' unitless
- verify-26/drags (display :156, 12-step stepped drags): Loudness readouts '72 %', '80.6 %', '36.4 %', '42.8 %' (rt/verify-26/app.log dumps)

**Current behaviour.** Parameter readouts use per-formatter precision: dB 1 dp, Ceiling 2 dp, percent integer or 1 dp depending on the value, unitless 2 dp for Character, Tone and other 0…1 controls, integer Hz below 1 kHz and 2-dp kHz above. Continuous drags land on fractional percents. In the Statistics panel the M/S/I and PLR values have no unit, while TP, SP, RMS and LRA do.

**Problem.** The parameter-readout variation is mostly deliberate and functional: the Ceiling is dialled to a spec, and a percent readout must not hide a typed tenth. Its cost is cosmetic. The meter rows are different: four of the eight Statistics numbers carry no unit while their neighbours do, and PLR in particular has no self-evident unit.

**Root cause.** There is no display-format specification. Formatters were changed one at a time, each with a local rationale (0.1.6 pctText, ADR-0024 ceiling). The Statistics panel was specified by rows (ADR-0020 §6) without a unit rule for the loudness rows.

**User impact.** Low. A user can read every parameter value correctly, since the precision differences follow function. A newcomer has to know from convention or the manual that M/S/I are LUFS and PLR is LU/dB. *Scope:* All parameter readouts (display only) and the Statistics panel in both views.

**Proposed improvement.** Keep the deliberate precisions: percent follows the value, Ceiling 2 dp, unitless 0…1/−1…+1 macros, the Hz/kHz switch. Make two targeted changes. (1) Give the unitless meter numbers a unit: a small dim 'LUFS' header or suffix for the M/S/I column (the number column is 48 px at LoudnessMeterView.cpp:184, so a header is the lower-risk layout), and a unit for PLR ('LU' or 'dB'; the word is owner-supplied C8 wording). (2) Record one display-format table (unit, decimal places, spacing, precision rule, owning decision) beside the formatters or in the user manual. Future formatter changes are then checked against it instead of being fixed one parameter at a time.

**Alternatives considered.**

- *Harmonise all dB readouts to 2 dp and all percents to 1 dp* — Rejected. It reverses the owner's 0.1.6 percent decision and adds false precision to continuous controls. ADR-0024's reasoning is specific to the Ceiling.
- *Quantise percent ranges so drags land on integers* — Rejected. It is a range change (PARAMETER_COMPATIBILITY_POLICY rule 3 / ADR-0010), as the pctText comment itself notes.
- *Add units to the meter rows and write a display-format spec* — Chosen. The change is small and display-only and closes the one undecided gap.
- *Leave everything as-is* — Acceptable for parameter readouts. It leaves the meter-unit gap.

**Decision: Modify · P3.** The broad claim that the numbers look like they come from different systems is mostly contradicted by recorded, owner-approved or ADR-backed decisions, which should be preserved. The narrower, evidenced gap is the unitless M/S/I and PLR rows, together with the lack of a format spec that let the variation build up. Adding units does not conflict with ADR-0020, which fixes the rows and their meaning, not their unit text.

**Dependencies.** [UI-022](findings-ui.md#ui-022) (dB formatter sign handling, rejected — no change needed there)

**Acceptance criteria.**

- The Statistics panel shows a unit for M, S and I (column header or suffix) and for PLR, in both Simple and Advanced, with no truncation at UI scale XS
- No parameter readout string changes: existing tests at e769f33:tests/state_tests.cpp:5947-5952 and the ADR-0024 ceiling tests pass unchanged
- A single display-format table exists that lists every formatter in src/PluginParameters.cpp (dbText, msText, pctText, hzText, the ceiling, Character/Tone and the others) and every Statistics row, with unit, decimal places and the owning decision

<details><summary>Verification record</summary>

**Method.** Read the formatters at e769f33:src/PluginParameters.cpp:85-189 and 283-308, the Statistics painter at e769f33:src/gui/LoudnessMeterView.cpp:153-275, the out-LUFS readout at e769f33:src/gui/PluginEditor.cpp:646 and 2038-2043, USER_MANUAL §3.2 and the Statistics table, and ADR-0020 §6 and ADR-0024. Viewed rt/gestures/22c (full Advanced view), 00-initial, and my own verify-26 captures. Stepped knob drags on :156 landed on '72 %', '80.6 %', '36.4 %' and '42.8 %'.

**Corrections to the candidate claim.** The anchors shift slightly: hzText is at :114-115, Character at :283-285, Tone at :286-288, and the Ceiling formatter at :303-308. Most of the listed differences are deliberate and documented, not unharmonised. Percent precision follows the value by owner decision (0.1.6, rationale at PluginParameters.cpp:87-107, pinned by e769f33:tests/state_tests.cpp:5947-5952). The Ceiling's 2 dp is ADR-0024. Character (0…1) and Tone (−1…+1) are documented as unitless scales (USER_MANUAL.md:172-173). Hz→kHz at 1 kHz is the conventional audio display. The only items with no recorded rationale are the missing units on the M/S/I rows (LoudnessMeterView.cpp:184) and the PLR row (:274-275). Every other Statistics row carries one (TP dBTP, SP/RMS dBFS, LRA LU).

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 4 · severity 1 · discoverability 2 · efficiency 1 · coherence 2 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-004

**Value arcs and fader fills always start at the range minimum, so the eight zero-centred controls (including the Simple Tone macro) and the Input Gain fader look partly engaged at their neutral 0; there is no zero origin or detent marker**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Visualization | The inherited frame, renderer and palette were not re-derived for this product's content or the user's display | Phase 5 |

**Evidence**

- e769f33:src/gui/LookAndFeel.cpp:55 — angle = startAngle + pos * (endAngle - startAngle)
- e769f33:src/gui/LookAndFeel.cpp:79-81 — value path addCentredArc(..., startAngle, angle): always from the range minimum
- e769f33:src/gui/LookAndFeel.cpp:186-188 — linear fill = track.withWidth(pos - bounds.getX()): always from the left end
- e769f33:src/PluginParameters.cpp:286 — Tone -1..1 default 0; :350 Odd/Even -1..1 default 0; :353 Color Tone -1..1 default 0; :380 Tilt -3..3 default 0; :382/:384/:386/:391 LS/HS/Bell1/Bell2 Gain -12..12 default 0
- e769f33:src/PluginParameters.cpp:314 — Input Gain -12..24 default 0 (the fader fills to one third at unity)
- e769f33:src/PluginParameters.cpp:303 — Ceiling -20..0 default -0.1; :321 Comp Threshold -40..0 default 0 (full arc = compressor inactive)
- Anamorph fd78c3b:src/gui/LookAndFeel.cpp:65-67 — identical unipolar arc (the inherited renderer; e769f33:src/gui/LookAndFeel.cpp:1 provenance ADR-0009)
- LAY-02 — Tone 0.00 half arc; Odd/Even, Color Tone, Tilt and the EQ gains half arcs; unipolar knobs at 0 none; [capture](captures/03-knob-arcs-at-defaults.png), session capture `rt/layout/04c-eq.png`
- LAY-07/LAY-16 — Input Gain fader fill at 0.0 dB; session capture `rt/layout/04c-utility.png`
- [capture](captures/02-advanced-view.png) — Threshold 0.0 dB full arc; Ceiling -0.10 dB near-full arc

**Current behaviour.** Every rotary draws its orange value arc from the 7 o'clock range minimum to the pointer, and the Input Gain and SC HPF faders fill from the left end. At the factory default, the Simple Tone macro (0.00) shows an arc from 7 to 12 o'clock. In Advanced, Odd/Even, Color Tone, Tilt and all four EQ gains at 0 show the same half arc, and Input Gain at 0.0 dB shows a one-third fill. A negative bipolar value shows a shorter arc from the minimum. Unipolar knobs at 0 (Loudness, Character, Drive, Depth, Dynamic Tame) show no arc. The track has no origin tick or centre marker.

**Problem.** On zero-centred controls the arc encodes 'distance above minimum' rather than 'deviation from neutral'. A neutral 0 is drawn identically to a unipolar knob at 50 %, the sign of a cut or boost is not visible in the arc, and the product renders 'neutral' two ways: no arc on unipolar knobs, half arc on bipolar ones.

**Root cause.** A single renderer is used for every parameter shape: AnabasisLookAndFeel::drawRotarySlider and drawLinearSlider, inherited from Anamorph under ADR-0009. It always strokes the arc or fill from the range start. No per-control origin exists, and none is derived from the slider's range, which SliderAttachment already provides in real units.

**User impact.** In every Simple session the Tone macro (one of the four primary controls) looks half-engaged at its neutral default. An Advanced EQ at flat looks like 'every gain half up', and a cut looks like a smaller boost. Users must read the number under each knob to learn the state or the sign. The number is always present, so no wrong mastering decision is forced, but glanceability of the EQ and colour panels is lost. *Scope:* Eight rotaries (Tone in Simple; Odd/Even, Color Tone, Tilt and four EQ gains in Advanced) and the Input Gain fader. The Ceiling and Threshold 'arc grows as the stage does less' cases are noted but excluded. The change is local to LookAndFeel.cpp, with no parameter or state involvement.

**Proposed improvement.** When a slider's range spans zero (s.getMinimum() < 0 < s.getMaximum()), the arc or fill should start at the ZERO point (originPos = s.valueToProportionOfLength(0.0)). That is 12 o'clock for the eight symmetric controls and the 0 dB point for Input Gain. The arc would run clockwise for positive values and counter-clockwise for negative ones. At exactly 0 no value arc is drawn; instead a small origin tick marks the track, as a visual detent marker. The origin must be computed in the same 0..1 'pos' space the eased 'vpos' sweep uses, so preset and A/B sweeps animate from the origin. Unipolar controls, Ceiling and Threshold are unchanged. An optional follow-up is a UI-side drag snap within about 1 % of 0; it must not change the parameter range. The change should be recorded in the ADR-0009 provenance header as a deliberate delta from the Anamorph renderer, and in the brand checklist B 'Control drawing' note.

**Alternatives considered.**

- *Leave as-is* — Keeps neutral settings reading as half-engaged on eight controls, including a primary Simple macro, and hides the sign of EQ gains. Not justified.
- *Origin at the parameter DEFAULT for every knob (arc = deviation from default)* — More general but changes unipolar knobs whose defaults are not the minimum (Mix 100 %, Link 100 %, Transients 50 %, Ratio 1.5, Attack 30 ms): a default Mix would lose its arc. A larger visual-language change with no evidence of harm for those controls. Rejected as scope creep.
- *Invert the arc for Ceiling/Threshold (arc = amount of processing)* — A semantic re-definition with no harm evidence; the current arc reads position correctly. Leave it for the fine review, not this change.
- *Centre tick only, arc unchanged* — Cheap, but the half arc still reads as 'half on'. Insufficient.

**Decision: Modify · P2.** The defect is real, frequent and cheap to localise. The constrained change is 'zero origin for zero-spanning ranges' rather than the finding's broader framing, which also bundled the Ceiling. That bundling rests on an overstated interpretation, and inverting Ceiling/Threshold has no harm evidence. The change is visual only and touches no gate category, provided any detent is implemented UI-side.

**Architecture gates.**

- Parameter Registry change — ONLY if a detent were implemented by altering a parameter's NormalisableRange/interval/default; implement any snap UI-side to stay ungated

**Dependencies.** [UI-001](findings-ui.md#ui-001); [UX-007](findings-ux.md#ux-007); [UI-006](findings-ui.md#ui-006); [UI-017](findings-ui.md#ui-017); [UI-003](findings-ui.md#ui-003)

**Acceptance criteria.**

- With the factory Default patch in Simple view, the Tone knob shows no value arc (at most an origin tick at 12 o'clock), while Loudness and Character at 0 remain arc-less as today.
- Setting Tone, Odd/Even, Color Tone, Tilt or any EQ gain to a positive value draws the arc clockwise from 12 o'clock to the pointer; a negative value draws it counter-clockwise from 12 o'clock.
- The Input Gain fader shows no fill at 0.0 dB, fills right of the 0 dB point for +6 dB and left of it for -6 dB.
- A pixel diff of the default Simple and Advanced views against e769f33 shows changes only on the eight zero-centred knobs and the Input Gain fader; Ceiling, Threshold and all unipolar knobs are unchanged.
- A preset or A/B switch that moves a bipolar knob animates its arc from the zero origin, with no frame drawing an arc from the range minimum.
- The parameter registry snapshot test is unchanged (no range, interval, default or ID edits).
- The LookAndFeel.cpp provenance header records the divergence from the Anamorph renderer.

<details><summary>Verification record</summary>

**Method.** Read the rotary renderer at e769f33:src/gui/LookAndFeel.cpp:23-138 and the linear renderer at :140-215. Read every zero-spanning range in e769f33:src/PluginParameters.cpp:286-391. Searched src/gui for any per-slider 'bipolar'/origin property and found none; the only properties set are 'glow' and 'hov', at PluginEditor.cpp:917 and :1986. Compared the Anamorph renderer, which is read-only (Anamorph fd78c3b e769f33:src/gui/LookAndFeel.cpp:63-67). Viewed session capture `rt/layout/01-simple-initial.png`, 01c-smallknobs.png, 04-advanced-initial.png, 04c-eq.png, 04c-limiter.png and 04c-utility.png. I did not re-run the display, because the rendering follows deterministically from the code and the screenshots are unambiguous.

**Corrections to the candidate claim.** Facts confirmed. Ceiling at -0.10 dB sits at pos 0.995 of -20..0, so the arc is about 99.5 % rather than 97 %. That near-full arc is a CORRECT position reading, not a misrepresentation, so the 'Ceiling maxed misrepresents the setting' interpretation is overstated. Ceiling belongs to a different case: 'the arc grows as the stage does less', and COMP Threshold at its 0 dB default (compressor inactive) draws a full arc the same way. There is no evidence of harm there, so I exclude it from the change. Two instances are missing from the claim. The Input Gain fader (-12..+24 dB) shows a one-third accent fill at unity (LookAndFeel.cpp:186-188; 04c-utility.png). And the bipolar set is exactly eight rotaries: Tone, Odd/Even, Color Tone, Tilt, and the LS, HS, Bell 1 and Bell 2 gains. A negative value draws a shorter arc from 7 o'clock, so the sign of the value is not visible in the arc at all.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 5 · severity 2 · discoverability 2 · efficiency 1 · coherence 3 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-005

**Every Advanced panel opens with a full-width mode combo that has no visible caption ('RMS', 'Tape', 'Transparent', 'Pre'); they are the only uncaptioned controls in the editor**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | confirmed | Information architecture | Controls do not show whether they are live or what they select, and the explanation sits in tooltips that ship off | Phase 5 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:527, :537, :548, :562 — setupCombo(detectorBox/modelBox/styleBox/eqPosBox, id, tipFor(id)): no caption label
- e769f33:src/gui/PluginEditor.cpp:1191-1214 — setupCombo adds items, a tooltip and a title only; :1207 sets the title to the registry name
- e769f33:src/gui/PluginEditor.cpp:1561-1563, 1579-1581, 1599-1601, 1612-1614 — each combo takes the first 24 px body row with no label
- e769f33:src/gui/PluginEditor.cpp:1534-1558 — the combos were unified to this slot (R2) and made full width at 0.1.1 to stop 'Transpar…' truncation
- e769f33:src/gui/PluginEditor.cpp:1567-1570 with 1560-1577 — the COMP body budget is spent 398/398, so a new caption row is not free
- e769f33:src/gui/LookAndFeel.cpp:637-693 — drawComboBox/positionComboBoxText render the value and arrow only
- e769f33:src/gui/PluginEditor.cpp:579-583 — the Dither combo is the one combo with a caption
- e769f33:src/gui/PluginEditor.cpp:58, 64, 73, 88 — tooltip strings; e769f33:src/InternalState.h:110 — tooltips default off
- e769f33:docs/user/USER_MANUAL.md:194, 201, 204, 213 — the manual names 'RMS/Peak detector', 'Color model', 'Style', 'Pre / Post position switch'
- LAY-05 — session capture `rt/layout/04c-comp.png`, session capture `rt/layout/04c-limiter.png`, [capture](captures/02-advanced-view.png) (captions absent); session capture `rt/layout/39c-tips-sheet-a.png` (tooltip text affected by the stale-tooltip artefact)

**Current behaviour.** Directly under each Advanced panel header sits a full-width combo showing only the current choice: 'RMS' (COMP), 'Tape' (CLIP / COLOR), 'Transparent' (LIMITER), 'Pre' (EQ). No caption names what is being chosen. The popup lists only the values (for example 'Pre / Post'). The noun is available only in a tooltip, which ships off, in the screen-reader title, or in the manual.

**Problem.** Every other control in the product carries a visible name: knobs have captions, toggles inline labels, and Dither its caption. These four mode selectors are bare values. 'Pre' (pre what? It means before the compressor) and 'Tape' (a colour model, not a tape effect toggle) are ambiguous without a noun.

**Root cause.** When R2 and 0.1.1 moved the combos into a shared full-width first-row slot to fix truncation, no caption slot was added. setupCombo has no caption parameter, unlike setupRotary, which takes a Label. The COMP body budget closes exactly, which discourages adding a row.

**User impact.** A user opening Advanced must hover with tooltips enabled, open the list, or consult the manual to learn what each combo selects. The ambiguity is worst for EQ 'Pre/Post', whose reference points (before the comp / after the limiter) appear nowhere on screen. The cost is low per occurrence but recurs in every Advanced session, and it breaks the product's own labelling grammar. *Scope:* Four combos in layoutAdvanced (detectorBox, modelBox, styleBox, eqPosBox), visual only.

**Proposed improvement.** Show a dim caption INSIDE each combo, left-aligned in textDim at the knob-caption size: 'Detector', 'Model', 'Style', 'Position'. The value follows it, right of the caption. Implement this with a 'caption' component property that drawComboBox paints and positionComboBoxText uses to offset the value label. The row budget and every knob position stay unchanged. The box is about 205 px wide; caption about 55 px + value about 75 px + arrow 20 px fits, so 'Transparent' stays untruncated at every UI scale. Caption wording is to be aligned with the naming decision in [UI-009](findings-ui.md#ui-009) and the manual §3.3 terms. The popup and the accessible title stay as they are.

**Alternatives considered.**

- *Separate caption row above each combo* — Clear, but costs about 12 px in every zone. COMP's 398 px budget is fully spent, so knobs or the GR lane would have to move.
- *Caption right-aligned in the zone header band (e.g. 'COMP ……… Detector')* — No layout cost, but separates the name from its control, and the header already carries the zone title.
- *Half-width caption + half-width combo* — Re-introduces the 'Transpar…' truncation the 0.1.1 change fixed, unless the widths are tuned per zone.
- *Status quo (tooltips/manual)* — Tooltips ship off ([UX-006](findings-ux.md#ux-006)); leaves the only uncaptioned controls in the product.

**Decision: Proceed · P2.** The claim is fully confirmed, the fix is small and low-risk, and it restores the product's own labelling grammar. The in-box caption respects the exhausted COMP row budget and the 0.1.1 anti-truncation directive. No gate is touched: no parameter name or ID changes.

**Dependencies.** [UI-009](findings-ui.md#ui-009); [UX-006](findings-ux.md#ux-006)

**Acceptance criteria.**

- With tooltips off, each of the four Advanced mode combos visibly names its choice (e.g. 'Detector RMS', 'Model Tape', 'Style Transparent', 'Position Pre') at UI scale M.
- 'Transparent', 'Transistor' and 'Post' render untruncated at every UI-scale step (XS–XL).
- No knob, toggle, GR lane or curve well in the four panels moves (bounds identical to e769f33).
- Each combo's popup items and order, and its accessible title (the registry name), are unchanged.
- The caption words match the terms used in USER_MANUAL §3.3.

<details><summary>Verification record</summary>

**Method.** Read the setupCombo calls at e769f33:src/gui/PluginEditor.cpp:527, 537, 548 and 562 and the helper at :1191-1214. Read the bounds at :1561-1563, :1579-1581, :1599-1601 and :1612-1614, and the R2 and 0.1.1 comment at :1534-1558. Read drawComboBox and positionComboBoxText at e769f33:src/gui/LookAndFeel.cpp:637-693, which draw no caption; the popup is addItemList only, with no section header. Read the tooltip strings at PluginEditor.cpp:58, 64, 73 and 88, the tooltip default at e769f33:src/InternalState.h:110, and the dither caption at PluginEditor.cpp:580. Viewed [capture](captures/02-advanced-view.png), 04c-limiter.png and 04c-eq.png.

**Corrections to the candidate claim.** Confirmed. The LAY-05 tooltip capture sheet (session capture `rt/layout/39c-tips-sheet-a.png`) is contaminated by the known stale-tooltip artefact. Hovering the COMP combo shows the Knee tooltip, and the LIMITER hover shows the Color-model text. The tooltip wording was therefore verified in code, not from those captures. The combos DO carry an accessible title: the registry name via setTitle at :1207 ('Comp Detector', 'Color', 'Style', 'EQ Position'), so screen readers get the noun and only sighted users lack it. The utility-row Dither combo HAS a caption (:580), so these four are the editor's only uncaptioned controls.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 3 · severity 2 · discoverability 3 · efficiency 2 · coherence 4 · change risk 1 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-006

**One amber-gold accent carries value, data and every mode/state cue (Learn running, toggles on including DELTA/MATCH/FREEZE, detach and edited dots, active A/B, selected chip); only over-ceiling/empty-Learn (warn) and BYPASS (red pill) have their own colours**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | medium | partially-confirmed | Visual hierarchy | The inherited frame, renderer and palette were not re-derived for this product's content or the user's display | Phase 5 |

**Evidence**

- e769f33:src/gui/LookAndFeel.h:47-63 — neutral family roles plus accent gold 0xfff0b432, accent2 amber 0xffe07830, warn 0xffd96a5a; the comment says the swatch awaits owner ratification
- e769f33:src/gui/LookAndFeel.cpp:77-78, :193 — arc and fader fill amber→gold; :285-286 — toggle ON = accent, except the 'bypass' red 0xffd0584e
- e769f33:src/gui/PluginEditor.cpp:2056 — Learn running = accent text colour; :2072 — empty pass = warn flash
- e769f33:src/gui/PluginEditor.cpp:1356-1362 — detach badges: accent 7 px dots at the knob's top-right, over the accent arc region; PluginEditor.h editedDot paint: accent
- e769f33:src/gui/PluginEditor.cpp:268-270 — active A/B letter accent; e769f33:src/gui/LoudnessMeterView.cpp:192-193 — M/S/I bars accent2→accent; :212 — TP/SP over-ceiling in warn
- e769f33:src/gui/CurveView.cpp:126, :178; GrHistoryView.cpp:549; SpectrumView.cpp:926 — curves and traces in accent
- e769f33:docs/DESIGN.md:852-857 (§6.1 accent family, P5 colour-blind check, owner ratifies); e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:17-18, 25 (swatch provisional), :89 (colour-blind-safe box unchecked)
- LAY-16 — [capture](captures/02-advanced-view.png), session capture `rt/layout/01c-topbar.png`, session capture `rt/layout/01c-stats.png`

**Current behaviour.** Amber→gold marks knob values, fader fills, meter bars, the GR line, the spectrum output, the transfer and response curves, the sub-brand text, and every ON toggle (ADV, AUTO, TP, LOCK, SHAPE, MATCH, DELTA, FREEZE). It also marks the active A/B letter, the selected GR/SPEC chip, the Learn-running text and the detach and edited dots. Desaturated red marks only over-ceiling TP/SP readings and a 1.5 s empty-Learn flash. A controlled red pill marks BYPASS.

**Problem.** No colour role exists for 'a mode is active that changes what you hear or what the engine is doing', such as Learn running, DELTA or MATCH monitoring, FREEZE latched or a knob detached. Those cues are drawn in the same accent that means 'value' and 'data', so they do not stand out from the dozens of accent arcs around them. The warn colour's credibility is also weakened by the TP row being red at the defaults ([VIS-002](findings-visualisation.md#vis-002)).

**Root cause.** The palette was specified as neutral family roles plus one accent family (DESIGN §6.1). The status and colour-blind palette pass that §6.1 deferred to P5 was provisionally waived under the v0.1.0 blanket approval, and its Level-5 check is still owed. Each state cue therefore defaulted to the accent.

**User impact.** State cues that matter, such as whether Learn is still running, whether DELTA is making you hear the difference signal, or which knobs are detached, are camouflaged among the value arcs. This is the colour-level contributor to [UX-005](findings-ux.md#ux-005), [UI-001](findings-ui.md#ui-001), [UX-007](findings-ux.md#ux-007) and [UX-009](findings-ux.md#ux-009). On its own it causes no wrong output, but it raises the attention cost of every mode check in every session. *Scope:* The palette-role design (LookAndFeel.h colours namespace) and the toggle, Learn, detach and edited-dot rendering. Value arcs, meters and traces are out of scope; they keep the brand accent.

**Proposed improvement.** Do not redesign the palette. Add ONE reserved state role, to be ratified together with the owner's pending accent-swatch decision and the colour-blind pass. Use it only for transient or abnormal modes: Learn running, DELTA/MATCH active, FREEZE latched, detach and edited indicators. Candidate treatments are a non-amber hue or a high-contrast neutral outline treatment. Pair every state cue with a non-colour channel (text such as 'LEARNING…', a shape, or an outline or pulse) so it survives deuteranopia and protanopia. Keep warn for over-ceiling and failure only, and BYPASS red as is. Record the role table in DESIGN §6.1 or its successor, and in the brand checklist.

**Alternatives considered.**

- *Status quo (single accent)* — Calm and on-brand, but mode cues stay indistinguishable from values. The downstream findings would each invent their own fix.
- *Full palette redesign with multiple hues* — Out of proportion. It changes product identity (the accent awaits owner ratification) and the brand checklist C 'distinct accent' item.
- *Non-colour cues only (text, shape, motion), no new hue* — A viable first step that needs no swatch decision and helps colour-blind users. It may be enough for Learn and DELTA; recommended as part of the Modify.

**Decision: Modify · P2.** The systemic observation is valid for state cues but overstated for warnings. The appropriate change is a narrowly scoped state role plus mandatory non-colour cues, not a palette overhaul. It should be decided in the same owner pass that ratifies the swatch and runs the colour-blind check, because the swatch is product identity (C8-adjacent). No architecture gate category is touched.

**Dependencies.** [UX-005](findings-ux.md#ux-005); [UI-001](findings-ui.md#ui-001); [UX-007](findings-ux.md#ux-007); [UX-009](findings-ux.md#ux-009); [UX-008](findings-ux.md#ux-008); [VIS-002](findings-visualisation.md#vis-002)

**Acceptance criteria.**

- A palette-role table (value/data, mode-active state, warn, bypass) is recorded in DESIGN §6.1 or a successor record and ratified by the owner alongside the accent swatch.
- Learn running, DELTA active, MATCH active, FREEZE latched and detached-knob indicators each render in the state treatment and also carry a non-colour cue (text or shape); a screenshot under a deuteranopia simulation still distinguishes each from an ordinary accent value arc.
- Value arcs, fader fills, M/S/I bars, the GR trace and the spectrum output keep the amber→gold accent.
- The warn colour appears only for over-ceiling readings and failure states, in coordination with the [VIS-002](findings-visualisation.md#vis-002) outcome.
- The BRAND_CONSISTENCY_CHECKLIST colour-blind item records the pass result.

<details><summary>Verification record</summary>

**Method.** Inventoried every colours::accent/accent2/warn use and the hard-coded arc and fill colours across src/gui: LookAndFeel.cpp:77-78, :193, :285-286, :357; PluginEditor.cpp:268-270, :1308, :1358, :2056, :2072; LoudnessMeterView.cpp:192-193, :212; CurveView.cpp:126, :178; GrHistoryView.cpp:549; SpectrumView.cpp:926; and editedDot in PluginEditor.h. Read the palette at e769f33:src/gui/LookAndFeel.h:47-63, DESIGN §6.1 (e769f33:docs/DESIGN.md:844-857) and BRAND_CONSISTENCY_CHECKLIST.md:17-18, 25 and 89. Viewed [capture](captures/02-advanced-view.png) and 01-simple-initial.png.

**Corrections to the candidate claim.** The claim 'leaving no colour for status or warning' is wrong in its warning half. colours::warn (0xffd96a5a) marks the TP and SP rows over the ceiling (LoudnessMeterView.cpp:212) and the empty-Learn flash (PluginEditor.cpp:2072). BYPASS uses a dedicated red pill (LookAndFeel.cpp:285, 0xffd0584e). The confirmed part is that no MODE/STATE tier exists. Learn running (accent text), every toggle-on including the audible-monitor modes DELTA and MATCH, FREEZE, the detach badges and the Simple edited dot (accent dots next to accent arcs), the active A/B letter and the selected GR/SPEC chip all share the value/data accent. The claim's DESIGN reference is loose: §6.1 specifies an amber/gold accent FAMILY (two colours, like Anamorph's blue/teal pair) and leaves the status palette to the P5 colour-blind check, which is still unchecked.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 4 · severity 2 · discoverability 3 · efficiency 1 · coherence 3 · change risk 3 · complexity 3 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-007

**The GR|SPEC pill (78x18) sits over live plot content, so the GR trace at heavy reduction, the spectrum floor and the LF skirt draw through its labels. Its target size is adequate.**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | partially-confirmed | Visual hierarchy | The inherited frame, renderer and palette were not re-derived for this product's content or the user's display | Phase 2 |

**Evidence**

- e769f33:src/gui/LookAndFeel.h:298 — `kW = 78, kH = 18, kInsetX = 6, kInsetY = 4`; :300-304 — bounds {6, h-22, 78, 18}
- e769f33:src/gui/LookAndFeel.h:318-321 — 'Translucent body: present enough to read, never a solid mask over the trace beneath (the owner's explicit requirement)' — bg alpha 0.55
- e769f33:src/gui/LookAndFeel.h:282-288 — placement rationale: bottom-left is 'the least informative corner in both modes'
- e769f33:src/gui/SpectrumView.cpp:784 — plot area reduced(10, 8), so the floor is at h-8, inside the pill band h-22..h-4; :929-931 — pill painted last over the traces
- e769f33:src/gui/GrHistoryView.cpp:25-28 and e769f33:src/gui/SpectrumView.cpp:101-104 — hit area = pill expanded by 2 px (82x22)
- e769f33:docs/architecture/design-decisions/ADR-0023-012-field-fix-contracts.md:240-244 — clause 7 fixes the bottom-left placement and the whole-pill toggle (Accepted, gate cleared 2026-08-09)
- Observation E12 + session capture `rt/edges/38d-chip-4x.png` — GR trace through the 'GR'/'SPEC' letters at extreme GR
- Observation V-14 + session capture `rt/visuals/19e-spec-silence-0.5s-well.png` (floor along the pill bottom), session capture `rt/visuals/06b-square200-spec-well.png` (LF skirt through 'GR')
- Observation LAY-11 + session capture `rt/layout/32-tabs-1x.png`
- Runtime (verify-23): session capture `rt/verify-23/pill-seq.png` and p0/p1-*-pill4x.png — 78x18 pill, labels clear under Loudness-70 music; label contrast 4.33-4.53:1

**Current behaviour.** A 78x18 two-label pill with a translucent body (bg at 55 % alpha) is drawn last over the bottom-left of both plots. Anything plotted there passes visibly through it: the SPEC floor line at h-8, LF skirts below about -76 dB at 20-38 Hz, and the GR trace when reduction exceeds about 19 dB.

**Problem.** When data crosses the labels, the strokes merge with the glyphs ('GR' reads as a squiggle in 38d), so the control's own state text becomes harder to read in exactly the extreme situation where the user looks at the graph most.

**Root cause.** The switch was placed inside the plot, with no reserved band, as a deliberate owner choice (ADR-0023 §7). Translucency was an owner requirement. The labels are drawn with no knock-out or halo, so they have no local contrast protection.

**User impact.** Minor legibility loss of the mode labels at heavy GR or with a low-floor spectrum. The control stays findable by position and by its lit segment, and it stays clickable. There is no workflow cost beyond a moment's squint. *Scope:* Both graph-well views, Simple and Advanced (same geometry), at all UI scales.

**Proposed improvement.** Keep the placement, size and translucent body (ADR-0023 §7 and the owner requirement). Protect only the glyphs: draw each label over a small, near-opaque bg-coloured rounded backing the size of its text, or with a 1 px bg-coloured halo. A trace crossing the pill then stays visible through the body but never through the letters. Optionally, stop the spectrum's floor stroke at the pill's right edge so the silence floor does not run along the label baseline.

**Alternatives considered.**

- *Reserve a band below the plot for the pill* — Removes all overlap but shrinks the plot height of both views (the GR history's fixed 24 dB span would get fewer px/dB). A larger layout change for a minor legibility issue.
- *Make the pill body opaque* — Conflicts with the owner's explicit 'never a solid mask' requirement recorded at LookAndFeel.h:318-319.
- *Enlarge the pill* — Not justified: the 82x22 hit area is adequate for a mouse-driven desktop plugin, and enlarging it would cover more data.
- *Leave as is* — Acceptable. The issue appears only at extremes, but the glyph-halo fix is cheap and respects every recorded constraint.

**Decision: Modify · P3.** Occlusion of the labels is confirmed, but only at extreme GR or on a low floor. The 'small target' part is not supported. A constrained, glyph-only protection addresses the confirmed part without touching the owner-mandated placement, translucency or toggle model.

**Architecture gates.**

- ADR-0023 clause 7 (bottom-left whole-pill toggle): respected by the proposal; moving or re-modelling the pill would conflict (hard stop)

**Dependencies.** [INPUT-012](findings-input.md#input-012) (same control; do any restyle together); E12 (GR display saturation at extremes); [VIS-021](findings-visualisation.md#vis-021) (the spectrum floor/LF corner the pill covers)

**Acceptance criteria.**

- With Loudness 100, Ceiling -20 and Input +24 (the E12 recipe), the 'GR' and 'SPEC' glyphs remain fully legible in a 4x crop, with no trace stroke crossing a glyph.
- With silence in SPEC mode, the floor line does not run through or along the label glyphs.
- The pill's position, 78x18 size, translucent body and whole-pill toggle behaviour are unchanged.
- Label contrast against whatever lies beneath stays ≥ 4.5:1.

<details><summary>Verification record</summary>

**Method.** Read graph_switch at e769f33:src/gui/LookAndFeel.h:296-348: kW=78, kH=18, inset (6, h-22), a translucent body at bg alpha 0.55 flagged as 'the owner's explicit requirement', and no hover input. Read both chipHitArea()s (expanded 2 px), SpectrumView.cpp:929-931 (the pill drawn last, over the traces) and SpectrumView.cpp:784 (the plot area is reduced by 10x8, so the -90 dB floor lies at h-8, inside the pill's h-22..h-4 band). Viewed session capture `rt/edges/38d-chip-4x.png`: at extreme settings the orange GR trace squiggles through 'GR' and 'SPEC'. Viewed rt/visuals/06b (the LF skirt strikes through 'GR') and 19e (the floor line runs along the pill's lower edge, under the letters). Runtime on :153 at 1x: the pill measures about 78x18 (each segment 39x18; hit area 82x22). With music at Loudness 70 the pill stayed clear of the GR trace (session capture `rt/verify-23/p1-after-SPEC-click-pill4x.png`). Label contrast of the brightest anti-aliased label pixel against the pill body: 4.33:1 (GR on the spectrum background) and 4.53:1 (SPEC over the GR level-history fill).

**Corrections to the candidate claim.** The observer's '36x14 per chip' is the visible label cell. The code and the 1x capture give 39x18 per segment and an 82x22 hit area for the whole pill, since any press flips (see [INPUT-012](findings-input.md#input-012)). That is comparable in height to the editor's MATCH/DELTA/FREEZE switches, so 'small target' is not supported as a defect. Occlusion of the letters happens (a) in GR mode only when reduction exceeds about 19 dB, i.e. the trace reaches the pill's top at h-22 on the fixed 24 dB span; the E12 capture was taken at Loudness 100 with Ceiling -20 and Input +24; and (b) in SPEC mode where 20-~38 Hz content sits in the bottom ~14 dB, plus the silence floor line. In ordinary use the labels are clear. Label contrast is about 4.3-4.5:1, so 'low-contrast' is marginal.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 2 · severity 1 · discoverability 2 · efficiency 1 · coherence 2 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-008

**A tooltip can show a neighbouring control's text: Anabasis lacks the sibling's 0.9.4 live re-hit-test, so JUCE's cached component-under-mouse labels a box placed at the live pointer**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P3** | medium | partially-confirmed | Feedback/observability | Controls do not show whether they are live or what they select, and the explanation sits in tooltips that ship off | Phase 5 |

**Evidence**

- e769f33:src/gui/PluginEditor.h:435-447 — GatedTooltipWindow::getTipFor: on/off gate then juce::TooltipWindow::getTipFor (c) on the CACHED component; no live re-hit-test
- JUCE e18f7f5 (9.0.1) modules/juce_gui_basics/windows/juce_TooltipWindow.cpp:209 (newComp = mouseSource.getComponentUnderMouse(), cached) vs :223 (mousePos = mouseSource.getScreenPosition(), live); :92-95 updatePosition → setBounds + setVisible
- Anamorph@fd78c3b:src/PluginEditor.h:15-76 (TooltipSource::choose, pure function), :282-313 (GatedTooltipWindow with componentAt live hit test), Anamorph@fd78c3b:src/PluginEditor.cpp:267 (componentAt wiring); Anamorph@fd78c3b:CHANGELOG.md:45-58 (0.9.4 fix, symptom and mechanism)
- runtime verify-5: session capture `rt/verify-5/32-sheet.png` (Ceiling text at LOCK, then correct after 2-px nudge), 33-sheet.png (rep1, rep2 wrong; smooth30 correct)
- runtime verify-5: .../rt/verify-5/10-sheet.png (3-step moves across Comp knobs: correct), 34-35-sheet.png (onto-tip and Tone→Ceiling: correct), 06-07-sheet.png (Settings rows: correct, one no-show)
- observers (teleport-only): LAY-14 session capture `rt/layout/41c-tip-lag-a.png`, session capture `rt/layout/41c-tip-lag-b.png`; G-21 session capture `rt/gestures/07b-tip-after-jump-crop.png`

**Current behaviour.** The tip text comes from JUCE's cached component-under-mouse, and the capsule is placed at the live pointer. When the two disagree, the capsule sits at the hovered control but carries a neighbour's text, or no tip appears. Examples: after a teleport, or when a stepped move's last step is the first one inside a small control. The wrong text persists until the pointer moves again.

**Problem.** The help layer can give the wrong explanation for the control under the pointer. The reproduced case shows the Ceiling's 'The output limit - nothing leaves the plugin above it' on the LOCK toggle. That reinforces the misreading G-16 documents, that LOCK locks the output ceiling knob.

**Root cause.** JUCE 9.0.1's TooltipWindow::timerCallback reads two sources of truth (cached component vs live position). The sibling fixed this at the editor level in 0.9.4 with a live re-hit-test in getTipFor. That fix post-dates the Anabasis adaptation and was not ported; the pinned JUCE is identical.

**User impact.** Tooltips are the only in-product explanation for several controls ([UX-006](findings-ux.md#ux-006)), so an occasional wrong tip misinforms exactly the user who enabled them to learn. The frequency with a real mouse is unmeasured and probably low on Linux. The sibling's reporter confirmed it on macOS. *Scope:* Every tooltip-bearing control in both views and the Settings panel, all platforms (shared JUCE code). It only matters when Tooltips are ON (default OFF).

**Proposed improvement.** Port Anamorph's TooltipSource under ADR-0009 copy-and-adapt with provenance. Add a componentAt(livePointer) callback to GatedTooltipWindow, wired to the editor's live hit test. In getTipFor, keep the cached component only if its screen bounds contain Desktop::getMousePosition(); otherwise use the component really under the live pointer, and give no tip if there is none. Keep the on/off gate first, and let the base getTipFor answer for the chosen component so modal/button-down suppression still applies. This interacts with the PopupShield: while the shield is raised the live hit test resolves to the shield, which has no tip, so the documented suppression is unchanged. Add the pure-function headless test the sibling carries, and list the JUCE-internals reliance in DEPENDENCY_POLICY.md's rule-7 register.

**Alternatives considered.**

- *Leave as-is and document as a known issue* — The mechanism is understood, and the sibling already paid for a tested fix. Leaving wrong-text tips in place is not justified.
- *Hide the tip whenever the cached and live positions disagree (no re-hit-test)* — Safe, but it drops correct tips too: the user would see no hint until the next motion. The sibling's choose() already degrades to 'no tip' when nothing is under the live pointer.
- *Patch JUCE TooltipWindow* — Out of bounds: a Build System change under ARCHITECTURE_REVIEW_GATE / DEPENDENCY_POLICY for a problem solvable at the override.

**Decision: Proceed · P3.** The code gap is certain. The wrong-text symptom is reproduced with stepped input (3/3) and was field-confirmed on macOS in the sibling on the same JUCE pin. The port is small, sibling-tested and headless-testable. Real-mouse frequency remains unmeasured, so priority stays low while tips are opt-in.

**Dependencies.** [UX-006](findings-ux.md#ux-006) (if tooltips become default-ON or more prominent, exposure rises and this becomes P2); G-16 (the reproduced wrong tip lands on LOCK)

**Acceptance criteria.**

- With Tooltips ON, the verify-5 path (8 steps × 30 ms from (360,620) to the LOCK toggle, 2 s hover) shows 'Keep the Ceiling where it is while you browse presets' in 3/3 runs, never the Ceiling text
- After an instantaneous pointer jump from one knob to another (LAY-14 protocol), the tip shown is the destination control's text or no tip — never the source control's
- A headless state-suite test exercises the choose(cached, livePointer, underLive) decision: cached kept when it contains the live pointer, replaced when it does not, null yields no tip
- With the preset menu or a combo list open, no tip for an underlying control appears (PopupShield suppression unchanged)
- Real-mouse check on macOS (Settings panel, UI Scale row → onto the tip box, the sibling's 0.9.4 protocol) shows the correct or no text in 5/5

<details><summary>Verification record</summary>

**Method.** Code: e769f33:src/gui/PluginEditor.h:435-447. GatedTooltipWindow::getTipFor only gates on and off; grep TooltipSource/componentAt in src = 0. Anamorph@fd78c3b:src/PluginEditor.h:15-76 (TooltipSource::choose), :282-313 (live hit test), Anamorph@fd78c3b:src/PluginEditor.cpp:267 (wiring), CHANGELOG.md:45-58 (0.9.4 symptom, confirmed on macOS by the reporter). Both products pin JUCE e18f7f5 (9.0.1). In the pinned juce_TooltipWindow.cpp:209 and :223, the text comes from the cached getComponentUnderMouse() and the box position from the live getScreenPosition(). Runtime on :135 (verify-5), Tooltips ON, XTest stepped moves:
(a) An 8-step, 30 ms/step move from (360,620) to LOCK (597,508), then a 2 s hover, showed the CEILING tip at the LOCK position in 3 of 3 runs (32a, 33-rep1, 33-rep2). A 2-px nudge then showed the correct LOCK tip (32b).
(b) A 30-step, 10 ms/step approach to LOCK showed the correct tip (33-smooth30).
(c) Knee→Mix→Attack→Release in 3 steps each and Tone→Ceiling in 5 steps showed correct tips (10-sheet, 35b).
(d) The Anamorph trigger, moving the pointer onto the tip box (Settings UI Scale / RMS rows, Release knob, Ceiling knob), did not produce wrong text under Xvfb: the tip hid and re-appeared with the correct text (05-07, 10g/h, 34b/c).
(e) Twice, a 2 s hover produced no tip at all after a stepped approach (06a, 31a). This fits the cache resolving to a tipless component.

**Corrections to the candidate claim.** The observers' statement that stepped moves always showed the correct text is not accurate. A realistic-speed stepped path whose final step enters a small control from a neighbour (Ceiling → LOCK) reproduced the wrong-control text 3/3. This also likely explains G-02's 'LOCK: no tooltip captured'. The input is still XTest, so this is not independent real-mouse confirmation. The observed pattern is 'the cache is one pointer event behind', and dense real-mouse streams would rarely expose it on Linux. The Anamorph 'move onto the tip box' trigger did not reproduce on X11. Whether the macOS/Windows exposure matches the sibling's field report is unverified here.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 2 · severity 3 · discoverability 4 · efficiency 1 · coherence 3 · change risk 1 · complexity 2 · evidence 3</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-009

**MATCH toggle is named 'Loudness Comp' in the host lane and to screen readers, bringing back the 'Comp = compressor' reading; other caption/name differences are prefix drops or abbreviations**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | partially-confirmed | Information architecture | Keyboard and assistive-technology operation is half-implemented | Phase 3 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:586-590 — the caption reads "MATCH" and the comment says 'Display only; the parameter and its registry name are untouched'
- e769f33:src/PluginParameters.cpp:310 — boolParam (pid::loudnessComp, "Loudness Comp")
- e769f33:src/gui/PluginEditor.cpp:1226 — t.setTitle (p->getName (24)). JUCE@e18f7f5:modules/juce_gui_basics/detail/juce_ButtonAccessibilityHandler.h:67-72 falls back to the button text only when the title is empty, so 'Loudness Comp' is announced and 'MATCH' is not
- e769f33:src/gui/PluginEditor.cpp:487-507 — the caption override table and the rule that lanes keep the stage prefix; setupRotary titles each knob with paramName (id) at :516 and :1150-1152
- e769f33:src/PluginParameters.cpp:349-350, 367-369, 376-377, 380, 400 — lane names 'Color', 'Odd/Even', 'Lim Release', 'Lim Auto Rel', 'Style', 'Transients', 'True Peak', 'Tilt', 'Noise Shaping'
- e769f33:CHANGELOG.md:1327-1329 — 0.1.3: 'the old caption read as a compressor switch… automation name ("Loudness Comp") [is] unchanged'
- e769f33:docs/user/USER_MANUAL.md:400-401 — 'your host's automation lane still calls this parameter "Loudness Comp"'
- Runtime (verify-28 dump, rt/verify-28/app.log): 'Loudness Comp' sits in the lane list beside 'Comp Ratio', 'Comp Threshold', 'Comp Attack', 'Comp Release', 'Comp Auto Rel', 'Comp Knee', 'Comp Detector', 'Comp Mix' and 'Comp Stereo Link'
- Screenshot: session capture `rt/verify-28/01-adv-crop.png` (captions); LAY-17; G-02

**Current behaviour.** The loudness-compensation toggle is captioned MATCH in both views. Its host automation lane and its accessibility title are both 'Loudness Comp'. Advanced knob captions drop the stage prefix ('Ratio', 'Release', 'Gain', 'Stereo Link'), while lanes and accessibility titles keep it ('Comp Ratio', 'Lim Release', 'Limiter Gain', 'Limiter Stereo Link'). Toggle captions abbreviate their lane names (SHAPE/'Noise Shaping', TP/'True Peak', ADV/'Advanced'). Within the lane list the limiter prefix is inconsistent ('Lim …' vs 'Limiter …'), and several lanes have no stage prefix ('Style', 'Transients', 'Tilt', 'Odd/Even', 'Color').

**Problem.** MATCH ↔ 'Loudness Comp' is the one pair with no shared word. The lane and screen-reader name bring back the compressor-switch reading that 0.1.3 removed from the caption, in a host list with nine 'Comp …' lanes. A screen-reader user hears 'Loudness Comp' for a control labelled MATCH. A speech-input user cannot target it by its visible label. The manual needs a parenthetical to connect the two names. The other differences are prefix drops or abbreviations where the caption is (nearly) contained in the name, so they are low harm. They are mainly an internal consistency issue in the lane list.

**Root cause.** The 0.1.2 (items 8-10) and 0.1.3 (item 2) renames changed editor captions only. setupToggle and setupRotary deliberately title every control with the registry name (cpp:1144-1152, 1226), and JUCE announces a non-empty title in place of the button text. PARAMETER_COMPATIBILITY_POLICY rule 2 allows changing a registry display name, and 0.1.2/0.1.3 used it for 'Limiter Stereo Link' and 'Color', but it was not used for loudnessComp.

**User impact.** Users automating MATCH in a host, screen-reader users and speech-input users meet a name that shares no word with the visible label and suggests a compressor parameter. The other mismatches cost little. *Scope:* Materially one parameter (loudnessComp, a toggle shared by both views). Secondarily, the naming consistency of about 8 lane names (limiter abbreviation and missing stage prefixes).

**Proposed improvement.** Change loudnessComp's display name (ID unchanged) to a name containing the caption word, e.g. 'Loudness Match', so caption, host lane and screen-reader title agree and the lane stops reading as a compressor control. Keep the code's rule that the accessibility title equals the lane name. Following PARAMETER_COMPATIBILITY_POLICY rule 2: update PARAMETER_REGISTRY.md, add a CHANGELOG 'Changed' entry, re-freeze tests/fixtures/parameter_registry.snapshot, and replace the USER_MANUAL.md:400-401 parenthetical. Record the broader lane normalisation as a separate owner naming decision: 'Lim Release'/'Lim Auto Rel' → 'Limiter …', 'Style' → 'Limiter Style', 'Color' → 'Color Model', prefixes for 'Odd/Even', 'Transients' and 'Tilt'.

**Alternatives considered.**

- *Leave as-is (the manual parenthetical is the bridge)* — This keeps the ambiguity 0.1.3 removed from the editor, in the lane list and the screen-reader name. It is cheap but inconsistent with the product's own stated reason for the caption.
- *Display-only: give the MATCH toggle the accessibility title 'MATCH (Loudness Comp)'* — This helps screen-reader users but breaks the code's single rule (accessibility title = lane name) and leaves the host lane ambiguous.
- *Rename every non-conforming lane in one pass* — This gives the most consistency, but changes many host-visible names in existing sessions and is the owner's naming call. It is better sequenced after the one harmful pair.
- *Revert the caption to COMP* — Rejected. It reintroduces the compressor-switch misreading the owner fixed in 0.1.3.

**Decision: Modify · P3.** The deliberate caption/registry design is sound for in-panel prefix drops and abbreviations, so it should be preserved. Only the MATCH ↔ 'Loudness Comp' pair shows harm: it has no lexical overlap and it contradicts the 0.1.3 rationale. The change is limited to that one display name. Lane-list normalisation is left to an owner naming decision. The rename is a policy-permitted display-name change (rule 2), not an ID change.

**Dependencies.** [DOC-001](findings-doc-test.md#doc-001)

**Acceptance criteria.**

- A host automation list and the harness 'dump' show loudnessComp's name containing 'Match' (e.g. 'Loudness Match'), and its ID is still 'loudnessComp'.
- The MATCH toggle's accessibility title (getTitle) equals the new registry name, and the state suite's name/title checks pass.
- tests/fixtures/parameter_registry.snapshot is re-frozen, and PARAMETER_REGISTRY.md and CHANGELOG ('Changed') carry the rename.
- USER_MANUAL.md no longer needs the 'still calls this parameter "Loudness Comp"' parenthetical.
- A saved session and automation written before the rename recall and play back identically, since both are keyed by ID.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/PluginEditor.cpp:487-507, 586-590, 1125-1153, 1207, 1226 and e769f33:src/PluginParameters.cpp:303-400. Read JUCE@e18f7f5 detail/juce_ButtonAccessibilityHandler.h:67-72: a non-empty title replaces the button text. Checked CHANGELOG.md:1327-1329 and USER_MANUAL.md:396-401. Ran 'dump' on the real processor (display :158, rt/verify-28/app.log) to list every registry/lane name, and viewed session capture `rt/verify-28/01-adv-crop.png` for the captions.

**Corrections to the candidate claim.** (a) The caption/registry split and 'accessibility title = registry name' are deliberate and documented in code (cpp:487-497, 1144-1149). The manual bridges MATCH to 'Loudness Comp' once (USER_MANUAL.md:400-401). (b) Limiter 'Release' vs 'Lim Release' is the documented in-panel prefix drop. The caption appears inside the lane and accessibility name, so it does little harm. (c) 'Odd/Even' is identical as caption, lane and accessibility name. The 'Color' / 'CLIP / COLOR' / 'Odd/Even' item is a lane-naming inconsistency, not a caption/lane split: the 'Color' lane selects the model, and 'Odd/Even', 'Style', 'Transients' and 'Tilt' carry no stage prefix, although the code's own rule (cpp:487-493) says lanes need one. There is also a 'Lim'/'Limiter' abbreviation mix. (d) Only MATCH ↔ 'Loudness Comp' shares no word. SHAPE/'Noise Shaping', TP/'True Peak' and ADV/'Advanced' are abbreviations. (e) Brief §8 (DEVELOPMENT_BRIEF.md:171) asks only for 'complete parameter and automation names'. 'The same wording the automation lane shows' is the code comment's reading, not the brief's text.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 2 · severity 2 · discoverability 3 · efficiency 1 · coherence 3 · change risk 2 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-010

**Simple view: the STATISTICS panel is 292x530 for about 212 px of content (about 318 px of blank glass), while the GR/spectrum well, the view's maximizer visual, is squeezed to a 108 px plot; a ~76 px empty band sits above the well**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | partially-confirmed | Information architecture | The inherited frame, renderer and palette were not re-derived for this product's content or the user's display | Phase 2 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:1756-1757 — meterView bounds (640, 54, 292, 720-46-128-16 = 530)
- e769f33:src/gui/PluginEditor.cpp:1703-1744 — left column: knob area 330, macro row 118, toggle row 40 (ends at y 534, toggles 498–530)
- e769f33:src/gui/PluginEditor.cpp:1758-1760 — well = (0, 600, 940, 120).reduced(8, 6), a 108 px-tall plot
- e769f33:src/gui/LoudnessMeterView.cpp:155-275 — paint consumes about 212 px; :276-281 — 'Nothing follows' (the penalty rows left with ADR-0015)
- e769f33:src/gui/PluginEditor.h:619-638 — Simple keeps the 940x720 family frame; the Advanced height was derived (822)
- e769f33:docs/DESIGN.md:883-891 — §6.2: the frame sizes are Anamorph's, unexamined; P5 expected to re-derive them
- e769f33:docs/architecture/design-decisions/ADR-0015-pre-ship-contract-refreeze.md:126-127 — int_meterTargets and the streaming-target display removed
- e769f33:docs/architecture/design-decisions/ADR-0020-waveform-statistics-panel.md:111-113 — 8 rows, 'neither view relayouts'
- docs/BRAND_CONSISTENCY_CHECKLIST.md (A. 'Overall frame layout — window proportions' must match Anamorph)
- LAY-01 — session capture `rt/layout/01-simple-initial.png`, session capture `rt/layout/01c-stats.png`, session capture `rt/layout/46-simple-lower-left-1x.png`

**Current behaviour.** At 940x720 the Simple right column is a 292x530 glass STATISTICS panel whose readings occupy the top ~212 px; the lower ~318 px are empty but still clickable, and a click resets the measurement ([UX-002](findings-ux.md#ux-002)). The left column (big knob, macro row, toggle row) ends at y 530. A ~76 px empty band follows, then a full-width graph well whose plot is 108 px tall. The GR history hangs its 24 dB span in that height, about 4.5 px per dB.

**Problem.** The primary view allocates about 60 % of its right column (about 318 of 530 px) to blank glass, while its most informative maximizer visual (the GR history, which the manual calls 'the fastest way to see how hard and how often the limiter is working') gets the least height of any panel. The right column reads as a placeholder.

**Root cause.** The Simple geometry is Anamorph's 940x720 frame with a fixed-height meter panel. That height was sized for the 0.1.0 content (M/S/I, TP, PLR and the streaming-target/penalty rows). ADR-0015 removed the target rows, ADR-0020 re-filled part of the space with five numeric rows, and neither re-derived the Simple layout. DESIGN §6.2's re-derivation request was never executed.

**User impact.** In every session the first view looks unfinished, and the GR/spectrum well is cramped: small GR excursions are hard to read at about 4.5 px/dB, and the spectrum has little vertical resolution. The blank area also enlarges the accidental meter-reset target (tracked as [UX-002](findings-ux.md#ux-002)). *Scope:* layoutSimple and the Simple branch of paint(), which draws the right-panel glass (PluginEditor.cpp:1364-1370). The graph well's bounds in Simple are affected. No DSP, parameter or state impact.

**Proposed improvement.** Keep the 940x720 frame and re-derive its interior. Recommended target (option A): Simple adopts the Advanced bottom-strip grammar. The graph well sits on the left and STATISTICS (300 px wide, hugging its ~212–222 px content) on the right, in one strip of about 230 px along the bottom. The knob block spans the upper area, reduced by about 40–50 px of the big-knob box so the strip fits. The big knob remains the single dominant control. The GR/spectrum plot grows from 108 px to about 200+ px tall, the blank glass disappears, and STATISTICS sits in the same place in both views, which helps [UX-004](findings-ux.md#ux-004)'s relocation problem. Minimal fallback (option B): shrink the STATISTICS panel to its content and extend the well upward into the ~76 px band.

**Alternatives considered.**

- *A. Bottom strip (well + STATISTICS) as in Advanced; knob block above* — Removes the dead glass, roughly doubles the well height, and puts Statistics in one place across views. It costs a smaller big-knob box and a narrower well (about 620 px, like Advanced), so the GR history shows more seconds per pixel. Needs owner layout sign-off.
- *B. Shrink STATISTICS to content; move/extend the well into the 76 px band* — Smallest change: the well grows to about 180 px. But the right column below Statistics stays empty unless something is placed there.
- *C. Fill the blank with new readouts (numeric GR [VIS-007](findings-visualisation.md#vis-007), input level [VIS-015](findings-visualisation.md#vis-015))* — Uses the space for real information but adds features. Depends on those findings' decisions; it can complement A or B.
- *D. Re-derive kSimpleH smaller* — Brand checklist A requires the frame to match Anamorph unless a deliberate-deviation ADR and owner sign-off exist. It also enlarges the ADV resize jump ([UX-015](findings-ux.md#ux-015)). Not recommended.
- *Leave as-is* — Keeps the placeholder look and the cramped well in the primary view.

**Decision: Proceed · P2.** The space misallocation is confirmed and has a functional cost beyond aesthetics: the cramped GR/spectrum well in the view whose brief is 'one large knob plus metering visualisation'. It can be fixed inside the family frame with no gate involvement. The choice between A and B should be taken together with [UX-004](findings-ux.md#ux-004)'s view-partition decision and [UX-002](findings-ux.md#ux-002)'s reset affordance. The root-cause attribution is corrected to ADR-0015 (removal) and ADR-0020 (explicit no-relayout).

**Dependencies.** [UX-004](findings-ux.md#ux-004); [UX-002](findings-ux.md#ux-002); [UX-015](findings-ux.md#ux-015); [VIS-006](findings-visualisation.md#vis-006); [VIS-007](findings-visualisation.md#vis-007); [VIS-015](findings-visualisation.md#vis-015); [VIS-016](findings-visualisation.md#vis-016); [DOC-008](findings-doc-test.md#doc-008); [TEST-009](findings-doc-test.md#test-009)

**Acceptance criteria.**

- In Simple at UI scale M the frame remains 940x720 (or an ADR plus owner sign-off records a new frame, per BRAND_CONSISTENCY_CHECKLIST A).
- The STATISTICS panel's interior below its last row is at most about 24 px, or the remaining space carries designed, labelled content.
- The Simple graph-well plot is at least 180 px tall (up from 108 px).
- No empty band taller than about 24 px separates the Simple control block from the graph well.
- The Loudness knob remains the largest control in the view.
- The clickable STATISTICS area shrinks accordingly (verified together with the [UX-002](findings-ux.md#ux-002) fix).

<details><summary>Verification record</summary>

**Method.** Computed the layout from e769f33:src/gui/PluginEditor.cpp:1700-1761 (layoutSimple) and the paint of LoudnessMeterView at e769f33:src/gui/LoudnessMeterView.cpp:153-281. The content is 10 pad + 16 header + 2 + 3×26 bar rows + 6 + 5×20 stat rows, so it ends 212 px into a 530 px panel. The left column ends at y 530 (toggle row) and the well starts at y 606. Read the header constants at e769f33:src/gui/PluginEditor.h:619-638, DESIGN §6.2 (e769f33:docs/DESIGN.md:881-891), ADR-0015 §Decision 3 and ADR-0020 §Decision 6. Viewed session capture `rt/layout/01-simple-initial.png`.

**Corrections to the candidate claim.** Empty interior confirmed at about 318 px (screen y ~310–628), and the band above the well at about 76 px. The '~80 px unused to the right of the knob block' is overstated. The macro and toggle block ends at editor x 608 and the panel starts at 640, a 32 px gutter. Root-cause attribution is off. The streaming-target and penalty rows were removed by ADR-0015 §Decision 3, not ADR-0020. ADR-0020 §Decision 6 GREW the panel from 5 to 8 rows and explicitly chose not to relayout ('202 px of the 234 the Advanced strip allows, so neither view relayouts'), so the Simple height was never re-derived in either direction. The 'reset target' harm is owned by [UX-002](findings-ux.md#ux-002) and disappears if [UX-002](findings-ux.md#ux-002) is fixed, independently of panel size.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 5 · severity 2 · discoverability 1 · efficiency 2 · coherence 3 · change risk 2 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-011

**The LIMITER panel's toggle and GR foot sits 74 px above the other three panels' feet (its GR lane is not level with COMP's) because ADR-0019 added a row to COMP only; the EQ's smaller knobs are a documented density trade-off**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | partially-confirmed | Visual hierarchy | The inherited frame, renderer and palette were not re-derived for this product's content or the user's display | Phase 5 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:1495-1500 — panel(): body = (…, 52, 225, 434).reduced(8).withTrimmedTop(20) → y 80–478
- e769f33:src/gui/PluginEditor.cpp:1560-1577 — COMP: combo, 3×84 rows, 74 px link row (:1571), toggles, GR lane at :1576 (y 464–478)
- e769f33:src/gui/PluginEditor.cpp:1567-1570 — the comment's '324 of 398' is the pre-ADR-0019 figure LIMITER still spends
- e769f33:src/gui/PluginEditor.cpp:1598-1610 — LIMITER: combo, 3×84 rows, toggles, GR lane at :1609 (y 390–404); nothing below
- e769f33:src/gui/PluginEditor.cpp:1583-1596 — CLIP curve well fills to the body bottom; :1634-1639 — EQ 78 px rows, curve well = remainder
- e769f33:docs/architecture/design-decisions/ADR-0019-comp-stereo-link.md:3-5, :60-63 — owner asked to 'keep the layout balanced'; the 74 px row was added to COMP only
- e769f33:docs/user/USER_MANUAL.md:266-269 — the '24 dB … read against each other' sentence is about the GR history versus the panel meters
- LAY-06/LAY-15 — session capture `rt/layout/46-adv-panel-feet-1x.png`, session capture `rt/layout/04c-limiter.png` (dead band), [capture](captures/02-advanced-view.png), session capture `rt/layout/04c-eq.png`

**Current behaviour.** In Advanced, COMP's AUTO row and two-lane GR meter sit at the bottom of its body (editor y 430–478), and the CLIP and EQ curve wells also reach the body bottom. LIMITER's AUTO/TP row and GR meter end at y 404, leaving a 74 px empty band before its panel outline. The two GR lanes are 74 px apart vertically. EQ knobs are about 43 px against about 49 px elsewhere.

**Problem.** The four-panel row has one visibly unfinished zone. LIMITER, the stage that carries the Ceiling, ends early with a blank foot, and its GR lane does not share a baseline with the only other GR lane.

**Root cause.** Each zone is laid out top-down on its own budget with no shared foot anchor. Before ADR-0019 (0.1.1), COMP and LIMITER both spent 324 of 398 px and their feet matched. ADR-0019 §Decision 4 inserted a 74 px Stereo Link row into COMP only, which closed COMP's budget exactly and moved its foot down, while LIMITER was not re-anchored. The COMP comment still quotes the old 324 figure.

**User impact.** This is a cosmetic rhythm defect: the Advanced view looks less finished and the eye does not find the two GR lanes on one line. No workflow or output harm has been demonstrated, and comparing COMP and LIMITER lane lengths is neither a documented nor, per [VIS-018](findings-visualisation.md#vis-018), a meaningful workflow. *Scope:* The LIMITER block of layoutAdvanced (PluginEditor.cpp:1598-1610), plus the stale comment at :1567-1570. EQ and CLIP are unaffected.

**Proposed improvement.** Anchor the LIMITER foot to the body bottom, as COMP's is: take the GR lane via removeFromBottom(14), then the 8 px gap, then the AUTO/TP row. Both GR lanes then share editor y 464–478 and both toggle rows share one baseline. The 74 px freed band then sits between the Transients row and the toggle row. If [UX-004](findings-ux.md#ux-004) decides LOCK should exist in Advanced, that band is the natural home for a LOCK toggle beside TP (the Ceiling lives in this panel), or for a numeric GR readout if [VIS-007](findings-visualisation.md#vis-007) proceeds. Keep the EQ's 78 px cells, and correct the stale '324 of 398' comment.

**Alternatives considered.**

- *Re-anchor the LIMITER foot to the body bottom (chosen)* — A few lines, no knob moves, restores a shared baseline for the GR lanes and toggle rows.
- *Add a fourth LIMITER row with a real control (e.g. LOCK, per [UX-004](findings-ux.md#ux-004))* — Uses the space meaningfully, but depends on [UX-004](findings-ux.md#ux-004)'s decision. It is compatible with the re-anchor.
- *Enlarge the EQ cells to 84 px* — The response well would fall below its 40 px floor. The current size is a documented trade-off, so preserve it.
- *Leave as-is* — Keeps a visibly unbalanced zone that the ADR-0019 directive ('keep the layout balanced') did not intend.

**Decision: Modify · P3.** The substantive defect is narrower than claimed: LIMITER alone, not four uneven feet, and not a GR-comparison workflow. The right change is a constrained re-anchor of one zone, preserving the EQ density choice. The harm is visual polish only, so it is prioritised as such. No gate is touched.

**Dependencies.** [UX-004](findings-ux.md#ux-004); [VIS-007](findings-visualisation.md#vis-007); [VIS-018](findings-visualisation.md#vis-018); [VIS-006](findings-visualisation.md#vis-006)

**Acceptance criteria.**

- In Advanced at UI scale M, the COMP and LIMITER GR lanes occupy the same vertical range (editor y 464–478; screen about 508–522 with the editor at 44,44), and their AUTO rows share one baseline.
- No zone's last element ends more than about 10 px above its body bottom unless the gap is deliberately used by a labelled control.
- EQ cell height (78 px), CLIP/EQ well bounds and every knob's bounds are unchanged from e769f33.
- The COMP budget comment states the actual per-zone spends.

<details><summary>Verification record</summary>

**Method.** Computed every zone's vertical budget from e769f33:src/gui/PluginEditor.cpp:1493-1640. panel() gives a body from editor y 80 to 478 (398 px). COMP: 24 + 3×84 + 74 + 26 + 8 + 14 = 398, so its GR lane is at 464–478. LIMITER: 24 + 3×84 + 26 + 8 + 14 = 324, so its GR lane is at 390–404 and 404–478 is empty. CLIP: 24 + 3×84 + 76 + 6 → curve well 438–478. EQ: 24 + 4×78 + 4 → curve well 420–478. Estimated knob diameters from the cell geometry: about 49 px for 84 px cells and about 43 px for 78 px cells. Read ADR-0019 §Decision 4 and USER_MANUAL §3.4 (e769f33:docs/user/USER_MANUAL.md:266-269). Viewed [capture](captures/02-advanced-view.png), 04c-limiter.png and 46-adv-panel-feet-1x.png.

**Corrections to the candidate claim.** Only the LIMITER foot is off. The COMP GR lane and the CLIP and EQ curve wells all end at the body bottom (editor y 478). The quoted 505 and 494 px are the data-dependent positions of the curve LINES inside their wells (the EQ line at 0 dB mid-well), not layout feet. The 'four feet at four heights' framing is therefore overstated. The USER_MANUAL quote is misattributed. 'The two read against each other directly' (USER_MANUAL.md:268-269) refers to the GR-history trace versus the panel GR meters sharing a 24 dB span, not COMP versus LIMITER. [VIS-018](findings-visualisation.md#vis-018) separately finds that the COMP and LIMITER lanes measure non-comparable quantities. The EQ knobs being about 12 % smaller is confirmed, but it is a documented choice: 78 px cells so that four rows plus the response well fit (PluginEditor.cpp:1616-1628; PluginEditor.h:619-626). The LIMITER dead band is 74 px in the body, about 82 px to the panel outline.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 3 · severity 1 · discoverability 1 · efficiency 1 · coherence 2 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-012

**Utility row: the MATCH/DELTA/FREEZE pills sit 6 px below the faders, Dither combo and SHAPE because they centre in the full band including the caption strip; fader captions centre under track plus value, not under the track**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | confirmed | Visual hierarchy | The inherited frame, renderer and palette were not re-derived for this product's content or the user's display | Phase 5 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:1644-1645 — util = (0, kBarH + kPanelRowH, 940, kUtilityH).reduced(16, 4)
- e769f33:src/gui/PluginEditor.cpp:1651-1657 — faders: caption removeFromBottom(12), slider = the remaining 44 px (centre y 518)
- e769f33:src/gui/PluginEditor.cpp:1659-1664 — MATCH/DELTA/FREEZE .reduced(2, 14) over the full 56 px (centre y 524)
- e769f33:src/gui/PluginEditor.cpp:1672-1679 — dither caption removeFromBottom(12); combo row withSizeKeepingCentre(clusterW, 26) (centre y 518)
- e769f33:src/gui/LookAndFeel.cpp:278-283 — the toggle pill is centred on bounds.getCentreY()
- e769f33:src/gui/PluginEditor.cpp:566-578 — documented reason for TextBoxRight 62x14 on the utility faders; :1136 knobs use TextBoxBelow
- LAY-07 — session capture `rt/layout/04c-utility.png`, session capture `rt/layout/32-utility-1x.png` (pills at screen y 568 against 562)

**Current behaviour.** In the Advanced utility strip, the Input Gain and SC HPF tracks, the Dither combo and the SHAPE toggle share a centre line (screen y 562). The MATCH, DELTA and FREEZE pills sit 6 px lower (y 568). The 'Input Gain' and 'SC HPF' captions are centred under track plus value, so they sit under the gap between the 80 px track and its floating value.

**Problem.** Two baselines in one short strip read as a misalignment, and captions offset from their tracks weaken the label-to-control association.

**Root cause.** The toggles are sized with reduced(2, 14) over the full 56 px band, which includes the 12 px caption strip the other controls reserve. Their centre therefore lands 6 px lower. The fader captions take the full cell width rather than the track width. The strip was assembled incrementally (R2 faders, the 0.1.2 caption-baseline fix) without a shared row centre.

**User impact.** A minor scan and polish cost in the Advanced view. No functional effect. *Scope:* The utility block of layoutAdvanced (PluginEditor.cpp:1644-1680). The same toggle components use Simple's own layout, which is unaffected.

**Proposed improvement.** Centre MATCH/DELTA/FREEZE in the same 44 px band the faders and dither cluster use: take removeFromBottom(12) from the toggle area as well, or position them with withSizeKeepingCentre on y 518. All utility controls then share one centre line. Optionally, centre each fader caption under its track rather than under track plus value. Keep TextBoxRight and the inline toggle labels, which are deliberate grammar.

**Alternatives considered.**

- *Fix the shared centre line (and optionally caption alignment)* — A few lines, no behaviour change. Resolves the visible misalignment.
- *Move the fader values below the tracks* — Rejected. It eats strip height, which is the documented R2 reason for TextBoxRight.
- *Add below-captions to the toggles for uniformity* — Duplicates the inline labels and breaks the product-wide toggle grammar.
- *Leave as-is* — Tolerable, but the 6 px offset is a plain layout slip with a one-line fix.

**Decision: Modify · P3.** The baseline slip is real and trivially fixable. The 'three conventions' part is mostly deliberate grammar that should be preserved. The change is therefore constrained to alignment, not a re-grammar of the row. It is visual polish, so P3. No gate is touched.

**Dependencies.** [INPUT-002](findings-input.md#input-002); [UI-004](findings-ui.md#ui-004)

**Acceptance criteria.**

- In Advanced, the MATCH, DELTA, FREEZE and SHAPE pills, the Dither combo and both fader tracks share one vertical centre (±1 px) at every UI-scale step.
- Each fader caption is horizontally centred under its track (±2 px), or the chosen caption alignment is applied identically to all captioned utility controls.
- The fader value boxes remain to the right of their tracks, and toggle labels remain inline; Simple-view toggle positions are unchanged.

<details><summary>Verification record</summary>

**Method.** Computed the bounds from e769f33:src/gui/PluginEditor.cpp:1644-1680. The util band is (0, 492, 940, 64).reduced(16, 4), so y 496–552. Faders sit at 496–540 (centre 518) above 12 px captions. The dither row withSizeKeepingCentre(196, 26) sits in 496–540 (centre 518). MATCH/DELTA/FREEZE are reduced(2, 14) within 496–552, giving 510–538 (centre 524). Confirmed that drawToggleButton centres the pill vertically (e769f33:src/gui/LookAndFeel.cpp:278-283). Viewed session capture `rt/layout/04c-utility.png` and 32-utility-1x.png: in the 2x crop, SHAPE and the combo centre at 48 px and MATCH at 60 px, so 6 px at 1x.

**Corrections to the candidate claim.** The 6 px baseline offset is confirmed, and its root cause is exact. Two of the 'three conventions' are deliberate product grammar rather than inconsistency. Inline toggle labels are used by every toggle in the product (Simple toggle row, AUTO/TP, LOCK). The fader value box to the right is a documented R2 choice (PluginEditor.cpp:566-578: a below-track box would eat the strip height). The residual labelling issue is that each fader caption is centred under the whole 175 px cell (track plus value), about 32 px right of the track centre.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 3 · severity 1 · discoverability 1 · efficiency 1 · coherence 2 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-013

**The preset-name slot has 106 px of text width at 13 pt and ellipsises the whole 'name *' string, so the dirty marker is the first thing cut: 'Transparent Master *' and 'Classical Dynamics *' show no '*'**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | confirmed | Visual hierarchy | Save, load, browse and restore change or lose state without saying so | Phase 4 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:1407-1425 — the right cluster is laid out from the right, then r.removeFromLeft(352 - r.getX()) and ‹/› take 24 px each; presetName = r.reduced(2,0) → 118 px
- e769f33:src/gui/LookAndFeel.cpp:369 — presetname font 13.0
- e769f33:src/gui/LookAndFeel.cpp:465-471 — drawText(text, bounds.reduced(6,0), centred, useEllipsesIfTooBig=true) → 106 px text width, ellipsis applied to the whole string
- e769f33:src/gui/PluginEditor.cpp:2165 — shown = shownDirty ? name + " *" : name (marker concatenated, no reserved width)
- e769f33:src/gui/PluginEditor.cpp:469 — presetName tooltip is the static 'Presets' (and tooltips default OFF, e769f33:src/InternalState.h:110), so the full name cannot be read on hover
- e769f33:docs/user/USER_MANUAL.md:157 — 'An * after the name means the sound no longer matches the loaded preset'
- e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:53-54 — 'Preset system — … the dirty marker' is a must-match item
- Anamorph@fd78c3b:src/PluginEditor.cpp:1874 — avail = width − 12 − textWidth(marker): the sibling reserves the marker's width; :1836-1851, 1888-1893 abbreviate the NAME only, then hard-clip
- JUCE 9.0.1 build/_deps/juce-src/modules/juce_graphics/native/juce_DirectWriteTypeface_windows.cpp:499 — Windows default sans = Verdana; juce_Fonts_linux.cpp:159-164 picks Liberation Sans here; juce_Fonts_mac.mm:399 Helvetica
- Runtime V12-4: session capture `rt/verify-12/32-preset-strip.png` — all 13 factory names fit clean at M (Transparent Master and Classical Dynamics nearly fill the slot)
- Runtime V12-5: session capture `rt/verify-12/37-dirty-strip.png` — dirty: 'Transparent Mast…', clean 'Classical Dynamics' vs dirty 'Classical Dynami…', 'Hip-Hop Low End *' intact
- Phase-2 LAY-17: session capture `rt/layout/01c-topbar.png` ('Default' only)

**Current behaviour.** The top bar shows the preset name and, when the sound differs from the loaded preset, appends ' *'. The combined string is centred in a 106 px text rect at 13 pt and ellipsised from the right when too wide. For 'Transparent Master' and 'Classical Dynamics' (Linux measurement), and for any user preset over roughly 16-17 characters, an edit replaces the tail with '…', so no '*' is ever shown. A clean name and an edited one both end in '…', which makes the edited state indistinguishable from a merely long name.

**Problem.** The dirty marker is the product's only top-bar indication that the sound no longer matches the preset. It is dropped exactly for long names, and on wider platform fonts (Windows Verdana, estimated) even clean factory names are expected to truncate.

**Root cause.** refreshPresetDisplay builds one string, name + " *" (PluginEditor.cpp:2165), and the LookAndFeel ellipsises that whole string (LookAndFeel.cpp:469-470), so the trailing marker is always the first casualty. There is no width measurement, no reserved marker slot and no abbreviation. The sibling's fitting logic (Anamorph:1874-1893) was never ported.

**User impact.** After tweaking a long-named preset, a user sees no edited indicator. They may conclude that A/B slots or presets match, skip saving, or not notice that a preset re-apply or load will replace their edits. Recovery is possible (undo, the menu tick), but the cue the manual promises is missing. User presets with descriptive names are the most exposed. *Scope:* The top-bar preset display in both views and at every UI scale (the transform scales everything, so the geometry ratio is constant). It affects 2 of 13 factory presets on Linux, more on wider platform fonts (estimated), and arbitrary long user preset names. Display only: the stored name (currentPresetName, serialized) is not involved.

**Proposed improvement.** Render the marker in a reserved, never-truncated slot. Measure the marker's width in the presetname font (13 pt) and fit the NAME alone into (text width − marker width): if it is too wide, apply the sibling's consonant-skeleton abbreviation, then clip or ellipsise the name only. Always draw ' *' after the fitted name. Cache on (name, dirty, width) as the sibling does. Expose the full, untruncated name plus an 'edited' state through presetName's accessible title (and tooltip text), without touching the stored name.

**Alternatives considered.**

- *Port Anamorph's refreshPresetDisplay fitting verbatim (reserve marker, abbreviate, hard-clip)* — Preferred. It is a family-consistent, proven, display-only change and satisfies checklist A 'the dirty marker'.
- *Widen the slot (e.g. replace the 'Settings' text with an icon to free about 44 px)* — Helps but does not guarantee the marker for user names, and changes top-bar geometry and family chrome (brand item). Not sufficient alone.
- *Show the dirty state some other way (tint the name or a dot left of it)* — Robust to width, but diverges from the sibling's ' *' and the manual. Could be considered in the brand pass.
- *Smaller font for long names* — Rejected: it reduces legibility of the primary state readout and still fails for long user names.

**Decision: Proceed · P2.** Reproduced at runtime and explained at code level. The failure removes a documented state cue in an ordinary workflow (edit a preset). The fix is display-only, local, has a proven sibling implementation, and converges with a checklist-A must-match item.

**Architecture gates.**

- No ARCHITECTURE_REVIEW_GATE category touched if the change stays display-only. Altering currentPresetName() or the stored presetName instead would touch the serialized SLOT field (serialization-schema gate, ADR-0022 preset identity), so it must not.
- BRAND_CONSISTENCY_CHECKLIST A 'Preset system — the dirty marker' (the fix converges with the sibling).

**Dependencies.** [UI-018](findings-ui.md#ui-018) (checklist bookkeeping for the preset-system item)

**Acceptance criteria.**

- For each of the 13 factory presets loaded and then edited (any parameter), the top-bar capture at M shows a visible '*' after the (possibly abbreviated) name.
- A headless state test measures, with the LookAndFeel's presetname font, that fitted-name width + marker width ≤ the presetName text rect for every factory name and for a 40-character synthetic user name, both dirty and clean.
- A user preset saved as a 40-character name shows an abbreviated or clipped name followed by ' *' when edited, never '…' in place of the marker.
- presetName's accessible title (and its tooltip text) contains the full untruncated name, plus 'edited' when dirty.
- getStateInformation XML is byte-identical before and after the change for the same session (the stored name is unchanged).

<details><summary>Verification record</summary>

**Method.** Recomputed the geometry from e769f33:src/gui/PluginEditor.cpp:1407-1425 at width 940: r = 352..522, prev 352..376, next 498..522, presetName 378..496 = 118 px, and drawButtonText reduces 6 px per side (LookAndFeel.cpp:469), leaving 106 px. Read the concatenation at PluginEditor.cpp:2165 and the ellipsis draw at LookAndFeel.cpp:465-471. Runtime on :142 (V12-4/V12-5): stepped the › arrow through all 13 factory presets and captured each name, then made Transparent Master, Classical Dynamics and Hip-Hop Low End dirty via 'param compRatio' and captured again. Width model: PIL advance widths at JUCE height 13 in Liberation Sans, the font JUCE picked here, reproduce the runtime exactly (107.6 and 107.4 px vs 106 available; Hip-Hop 97.8 fits). Searched the git history for any abbreviation code in Anabasis.

**Corrections to the candidate claim.** (1) The usable text width is 106 px, not ~118 px (the button is 118, the text rect 106). (2) Long names were NOT truncated when clean. All 13 factory names fit on Linux; the truncation happens only with the dirty marker, and it removes the marker entirely ('Transparent Mast…', 'Classical Dynami…'). That is worse than cosmetic, because the edited state becomes invisible. (3) Platform-dependent: JUCE's Windows default sans is Verdana (juce_DirectWriteTypeface_windows.cpp:499), which is wider. A DejaVu Sans proxy estimate puts the clean 'Transparent Master' at 107.3 px and 'Classical Dynamics' at 106.6 px, so they would truncate even when clean. This is an estimate; Windows and macOS were not measured. (4) Root cause wording: there is no evidence Anabasis ever had the sibling's abbreviation ('git log -S abbreviate' shows no such code), so it was never ported rather than dropped.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 3 · severity 3 · discoverability 4 · efficiency 2 · coherence 3 · change risk 1 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-014

**Tooltips are anchored just below-right of the pointer and routinely cover the hovered knob's own readout or a neighbouring control**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Defer** | **P3** | high | partially-confirmed | Visual hierarchy | Controls do not show whether they are live or what they select, and the explanation sits in tooltips that ship off | — |

**Evidence**

- e769f33:src/gui/LookAndFeel.cpp:966-975 — getTooltipBounds: x = pos.x+14 (or pos.x-(w+12) past parentArea centre), y = pos.y+16 (or pos.y-(h+8)), max text width 260
- Anamorph@fd78c3b:src/gui/LookAndFeel.cpp:862-871 — identical family placement
- e769f33:src/gui/PluginEditor.h:447 — GatedTooltipWindow tooltips { nullptr, 600 } (parentless desktop window → parentArea = display bounds)
- JUCE e18f7f5 juce_TooltipWindow.cpp:161-172 — no tip while a mouse button is down (tip never covers a readout during a drag)
- runtime verify-5: session capture `rt/verify-5/34-35-sheet.png` (row 1: Ceiling tip clips its own '-1.00 dB' and covers LOCK; row 4: Tone tip covers the Ceiling dial and LOCK)
- observer: session capture `rt/gestures/09-tip-tone-crop.png` (Tone tip over Ceiling dial), session capture `rt/gestures/08c-tip-smooth-to-ceiling-crop.png` (Ceiling tip over '-0.10 dB')

**Current behaviour.** The capsule, up to 280 px wide and 1-3 lines, appears 14 px right and 16 px below the pointer. It stays there while the pointer remains on the same control. Knob readouts sit directly under the dial, so a pointer resting on the dial places the capsule over the knob's own value or the next control (LOCK/TP).

**Problem.** While hovering, the hint partly hides the value or neighbouring toggle the user is reading. The user must leave the control to see both. This is minor.

**Root cause.** Pointer-anchored placement in the family getTooltipBounds, which receives only the tip text, pointer and display area and cannot see the hovered component's bounds. Combined with the layout, where value text sits below each knob and the TP/LOCK toggles sit to the right of Ceiling.

**User impact.** Small: a hover-time occlusion that is fixed by moving the pointer. It never happens during a drag. Tooltips are off by default. *Scope:* Every knob with a text box below it, in both views, when Tooltips are ON. The direction varies with the window's position on the monitor.

**Proposed improvement.** Target: the hint never covers the hovered control's own value readout. Anchor the capsule to the hovered component rather than the pointer. The GatedTooltipWindow::getTipFor override already receives the component, so it can publish that component's screen bounds for getTooltipBounds to place the capsule beside it, falling back to the current rule for large components such as the Statistics panel. Treat this as a family presentation change decided together with Anamorph at the brand pass.

**Alternatives considered.**

- *Keep the current placement* — Acceptable while tooltips are opt-in; the occlusion is transient and hover-only.
- *Larger fixed vertical offset* — Moves the occlusion onto the next row's readout in the Advanced grid, so it does not solve it.
- *Component-anchored placement (beside the hovered control)* — Solves it but diverges from the sibling's identical presentation, so it needs a family decision.

**Decision: Defer · P3.** Real but low-impact polish, on an opt-in layer, in code shared verbatim with the sibling. It should wait for the [UX-006](findings-ux.md#ux-006) default decision (exposure) and the brand pass, where tooltip presentation is a checklist item that has to match the family. It is worth doing then, especially if tips become default-ON.

**Dependencies.** [UX-006](findings-ux.md#ux-006) (tooltip default and prominence); Brand pass item 'Tooltips — the same presentation' (e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:59)

**Acceptance criteria.**

- With Tooltips ON, hovering anywhere on the Simple Ceiling dial for 2 s shows the tip without covering any glyph of the Ceiling value readout or the LOCK/TP toggles
- Hovering the Tone dial for 2 s shows the tip without covering the Ceiling dial or its readout
- In the Advanced grid, hovering any knob shows a tip that does not cover that knob's own value/caption
- Placement still keeps the capsule on-screen when the plugin window is near any monitor edge

<details><summary>Verification record</summary>

**Method.** Code: e769f33:src/gui/LookAndFeel.cpp:966-975, AnabasisLookAndFeel::getTooltipBounds, which is byte-for-byte the same as Anamorph@fd78c3b:src/gui/LookAndFeel.cpp:862-871. JUCE e18f7f5 juce_TooltipWindow.cpp:125-135: a parentless tip gets the DISPLAY's user bounds as parentArea. juce_TooltipWindow.cpp:161-172: getTipFor returns empty while any mouse button is down. Viewed session capture `rt/gestures/09-tip-tone-crop.png` and 08c-tip-smooth-to-ceiling-crop.png. Reproduced on :135: verify-5/34a (Ceiling tip) and 35a (Tone tip).

**Corrections to the candidate claim.** The root cause is not JUCE default positioning: Anabasis overrides getTooltipBounds (+14/+16 from the pointer, flipped to left/above only when the pointer is past the DISPLAY's centre, not the editor's). The claim 'the Tone tip covers the Ceiling value' is overstated. It covers the Ceiling dial and the LOCK toggle, and the Ceiling value stays visible (09, 35a). The Ceiling tip does cover the right end of its own readout ('-0.10 d', '-1.00 d') and the LOCK toggle (08c, 34a). The tip hides on mouse-down, so it never covers a readout during a drag. The flip direction depends on where the plugin window sits on the monitor.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 2 · severity 1 · discoverability 1 · efficiency 1 · coherence 2 · change risk 2 · complexity 2 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-015

**Load Preset… opens JUCE's stock file chooser (non-native on Linux) in a separate, unbranded window, next to a branded Save overlay**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Preserve** | **none** | medium | partially-confirmed | Preset/state workflow | Save, load, browse and restore change or lose state without saying so | — |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:2347 — std::make_unique<juce::FileChooser> ("Load Preset", dir, "*.anabasis") (defaults: native dialog requested, no parent component)
- JUCE 9.0.1 juce_FileChooser_linux.cpp:318-324 (fetched per e769f33:CMakeLists.txt:82) — isPlatformDialogAvailable() = zenity || kdialog present
- git grep at e769f33: no JUCE_DISABLE_NATIVE_FILECHOOSERS anywhere in the repo
- ST-06 — JUCE fallback browser in a separate window — session capture `rt/state/16a-load-dialog.png`, session capture `rt/state/16b-crop.png`
- E11 — same fallback, Escape closes — session capture `rt/edges/61a-load-preset-chooser.png` (path shown is the harness HOME under /tmp)

**Current behaviour.** Load Preset… requests the platform's native open dialog, filtered to *.anabasis and opening in the user preset folder. On Linux systems without zenity or kdialog, JUCE falls back to its own browser in a separate top-level window with default styling. That is what the audit box showed.

**Problem.** The claimed inconsistency exists only in the Linux fallback case. Elsewhere the product uses the platform dialog users expect.

**Root cause.** JUCE's platform-dependent choice between native and fallback dialogs. The code's request for the native dialog is correct.

**User impact.** Negligible. On a minimal Linux install the chooser looks foreign but works. On standard desktops and on macOS/Windows users get their familiar OS dialog. *Scope:* Linux hosts without zenity or kdialog only.

**Proposed improvement.** Keep the native chooser. Optionally add one line to the Linux notes in the user manual: installing zenity or kdialog gives the desktop's native file dialog for Load Preset…. Do not parent or restyle the chooser.

**Alternatives considered.**

- *Pass the editor as parentComponent so the fallback browser renders inside the editor with the family LookAndFeel* — Brands the Linux fallback, but also changes how the chooser is presented on native platforms (parented presentation). Risk and test burden on three OSes to polish a rare case. Not justified.
- *Force the JUCE browser everywhere (useOSNativeDialogBox=false) and style it* — Rejected. Loses OS navigation features (favourites, recent folders, search) that users rely on for 'anywhere on disk' loads (USER_MANUAL.md:375-376).
- *Replace the chooser with an in-editor branded browser* — Large effort for no workflow gain.

**Decision: Preserve · none.** The main premise is refuted for standard desktops: JUCE picks the native dialog when zenity or kdialog is present, and macOS and Windows always use it. The long path was the harness HOME. Native file dialogs are the right, expected behaviour for loading from anywhere on disk. The only residue is cosmetic on minimal Linux installs and does not justify changing how the chooser is presented on three platforms.

**Dependencies.** None.

**Acceptance criteria.**

- Load Preset… keeps opening the OS-native open dialog on macOS and Windows, and on Linux when zenity or kdialog is installed.
- The fallback on Linux without those tools keeps working: *.anabasis filter, opens in the user preset folder, Escape cancels, and the chosen file loads.
- If the optional doc line is added, the USER_MANUAL Linux notes name zenity or kdialog as the way to get the native dialog.

<details><summary>Verification record</summary>

**Method.** Code read at e769f33:src/gui/PluginEditor.cpp:2343-2363. The juce::FileChooser is built with the default useOSNativeDialogBox=true and no parent. I read the Linux platform check in the fetched JUCE 9.0.1 (e769f33:CMakeLists.txt:82; build/_deps/juce-src/modules/juce_gui_basics/native/juce_FileChooser_linux.cpp:318-324): the native dialog is used when zenity or kdialog is on PATH, unless JUCE_DISABLE_NATIVE_FILECHOOSERS is set. git grep at e769f33 finds no such define. On the audit box `which zenity kdialog` returns nothing. Viewed session capture `rt/state/16b-crop.png` and session capture `rt/edges/61a-load-preset-chooser.png`. I did not install zenity or kdialog to observe the native path; the native-dialog claim rests on the JUCE source.

**Corrections to the candidate claim.** (1) 'Non-native on Linux' is true only on systems without zenity or kdialog, like the audit container. Standard GNOME and KDE desktops get the native dialog. (2) The 'long config path' is a harness artefact: HOME was set under rt/…. A real user sees ~/.config/RollyTech/Anabasis/Presets. (3) macOS and Windows use the OS dialog. An unbranded native file dialog next to a branded in-editor overlay is normal platform practice, not an inconsistency the product introduced. (4) The fallback works: filter *.anabasis, opens in the preset folder, Escape closes it, Open is disabled until a file is selected.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 1 · severity 1 · discoverability 1 · efficiency 1 · coherence 2 · change risk 3 · complexity 2 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-016

**At XS the secondary captions and tags render at about a 6 px cap height (4 px x-height) and are hard to read on 1x displays**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Defer** | **P3** | medium | partially-confirmed | Accessibility/input | Keyboard and assistive-technology operation is half-implemented | — |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:1138-1141 — l.setColour (textColourId, colours::textDim); l.setFont (FontOptions (11.5f))
- e769f33:src/InternalState.h:66 — steps { 75, 85, 100, 125, 150 }
- e769f33:src/gui/PluginEditor.cpp:1309 ('MASTERING MAXIMIZER' 10.0f), :1323 (panel titles 11.0f), :648 ('out LUFS' 11.5f); e769f33:src/gui/LookAndFeel.h:342 (GR/SPEC 10.0f); e769f33:src/gui/LoudnessMeterView.cpp:162,180 (STATISTICS 11.0f, M/S/I tags 11.5f)
- e769f33:src/gui/LookAndFeel.h:47-52 — bg #0e1014, bgPanel #161a21, textDim #8b94a3 (computed contrast 5.2-6.2:1)
- Runtime (verify-11, :141, stepped motion): XS 705x540/617; 1x pixel map of 'Ratio' shows a 6 px cap, 4 px x-height and peak luminance about 120/255 — session capture `rt/verify-11/04c-XS-adv.png`, session capture `rt/verify-11/05-ratio-4x.png`
- Observers LAY-09 / E17 — session capture `rt/layout/14c-XS.png`, session capture `rt/layout/45c-XS-advanced.png`, session capture `rt/layout/45c-XS-eq-labels-1x.png`, session capture `rt/edges/36b-xs-native-1x-small-labels.png`

**Current behaviour.** XS scales the whole editor uniformly by 0.75, so the dim-grey captions, stat tags, panel titles and GR/SPEC chip labels drop to 7.5-8.6 px font height (about 6 px caps). No minimum text size is applied.

**Problem.** On a 1x (about 96-110 ppi) display, a user who picks XS to save space gets captions at the lower limit of legibility. The anti-aliased strokes of the dim-grey captions render noticeably fainter than their nominal colour.

**Root cause.** A uniform whole-window transform (e769f33:src/gui/PluginEditor.cpp:1930) applied to the family type grammar, whose smallest texts are 10-11.5 px at M, with no text-size floor. This is the documented 'everything scales in proportion' contract.

**User impact.** Slower reading and possible misreading of secondary labels (knob captions, M/S/I/TP tags) at XS on standard-DPI screens. Values stay legible, and the fix is one Settings step to S or M. *Scope:* Only the opt-in XS step (and marginally S) on 1x displays. HiDPI renders the same logical size more crisply. Both modes are affected.

**Proposed improvement.** When this is taken up, as a family-level decision: at every scale, the smallest secondary text (captions, tags, chip labels) renders at a cap height of 7 px or more on a 1x display, with a rendered contrast of 4.5:1 or more. The preferred means is a modest global raise of the smallest roles (the 10.0/10.5 px roles to about 11, captions 11.5 to about 12) together with a slightly brighter caption tone, checked against the fixed 13-px caption bands. An XS-only text floor is not preferred.

**Alternatives considered.**

- *Leave as-is: XS is an opt-in compact step and proportional scaling is the documented contract* — Defensible. Text is readable in 1x crops and the remedy is one click. The cost is an accessibility gap at the smallest step.
- *XS-only minimum text size (non-uniform scaling)* — It breaks 'everything scales in proportion', and the enlarged text would overflow the fixed caption bands (13 px at M, about 10 px at XS). High layout risk.
- *Raise the smallest type roles and brighten captions at all scales* — Fixes XS without special-casing, but changes the M appearance and the family type grammar shared with Anamorph. It needs a family decision.
- *Note the trade-off in the UI Scale tooltip or USER_MANUAL §3.5 ('XS is intended for HiDPI or space-constrained setups')* — Zero-risk. It can ride along with [DOC-007](findings-doc-test.md#doc-007) now.

**Decision: Defer · P3.** The size loss is real and measured. The claimed colour problem is not: nominal contrast passes AA, and the XS text remains readable, only small. The only fix that avoids layout overflow changes the shared family type grammar (identical in Anamorph), so it should not be taken per-product ahead of a family typography decision. It also interacts with the scale model's own future ([UX-020](findings-ux.md#ux-020)/UX-021). This waits for a family-level decision on minimum text size and the caption tone, and for the [UX-021](findings-ux.md#ux-021) outcome. The tooltip or manual note can land with [DOC-007](findings-doc-test.md#doc-007) meanwhile.

**Dependencies.** [UX-020](findings-ux.md#ux-020); [UX-021](findings-ux.md#ux-021); [DOC-007](findings-doc-test.md#doc-007); Family (Anabasis/Anamorph) typography decision per BRAND_CONSISTENCY_CHECKLIST

**Acceptance criteria.**

- At XS on a 1x display, every caption, stat tag, panel title and GR/SPEC label measures at least 7 px cap height in a native screenshot
- The measured peak luminance contrast of caption strokes against their background is at least 4.5:1 at XS
- No caption or tag truncates or overlaps at any of XS..XL in either mode
- The change is applied as a documented family type-scale decision (or explicitly scoped to Anabasis with the delta recorded in the brand checklist)

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/PluginEditor.cpp:1138-1141 (rotary caption: textDim, FontOptions(11.5f)) and e769f33:src/InternalState.h:66 (XS = 75 %). Read the other small-text sites: 'MASTERING MAXIMIZER' 10.0 (PluginEditor.cpp:1309), GR/SPEC chip 10.0 (LookAndFeel.h:342), panel titles and 'STATISTICS' 11.0 (PluginEditor.cpp:1323, LoudnessMeterView.cpp:162), M/S/I tags 11.5 (LoudnessMeterView.cpp:180), 'out LUFS' 11.5 (PluginEditor.cpp:648). Reproduced XS on :141 with stepped pointer motion (Settings, then UI Scale, then XS; bounds 705x540 Simple and 705x617 Advanced). Captured native 1x frames (session capture `rt/verify-11/03c-XS-simple.png`, 04c-XS-adv.png) and dumped the pixel map of the 'Ratio' caption: cap height 6 rows, x-height 4 rows, peak rendered luminance about 120/255 on a background of 23, against 174-204 for the value text. Computed WCAG contrast for textDim #8b94a3: 6.2:1 on bg #0e1014, 5.7:1 on bgPanel #161a21, 5.2:1 on bgRaised #1d222b. Viewed session capture `rt/layout/45c-XS-eq-labels-1x.png` and session capture `rt/edges/36b-xs-native-1x-small-labels.png`.

**Corrections to the candidate claim.** 11.5 is a JUCE font HEIGHT in logical px, not points. At XS the font heights are 7.5 px (10.0 texts), 8.25 px (11.0) and 8.6 px (11.5), which is roughly a 6 px cap height. The 'low-contrast caption colour' is overstated: textDim passes WCAG AA (4.5:1) nominally on every panel background. The legibility loss comes from the tiny size, where anti-aliased 4-px-x-height strokes never reach the nominal colour. In native 1x crops every caption is still readable, only small; nothing truncates or overlaps. The caption size, palette and ladder are identical in Anamorph (fd78c3b:src/PluginEditor.cpp:751, :1794), so the behaviour is family-consistent.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 1 · severity 1 · discoverability 1 · efficiency 1 · coherence 1 · change risk 3 · complexity 2 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-017

**Knob pointers ease after every non-drag change (wheel, keyboard, host automation, macro-follow), not only after preset/A-B/reset jumps, and they ease downward about 3x slower than upward**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P3** | high | confirmed | Interaction model | The inherited frame, renderer and palette were not re-derived for this product's content or the user's display | Phase 5 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:2905-2906 (up = 1-0.0001^(3.2dt), down = 1-0.01^(2.2dt); chosen by target > v)
- e769f33:src/gui/PluginEditor.cpp:2933-2944 (vpos snaps only while this slider isMouseButtonDown; otherwise ease)
- e769f33:src/gui/PluginEditor.cpp:2935-2936 (comment: ease on jumps — reset, preset, A/B)
- e769f33:src/gui/LookAndFeel.cpp:50-54 (drawRotarySlider draws vpos whenever not dragging; the arc uses the same angle)
- e769f33:src/gui/LookAndFeel.cpp:176-183 (linear slider draws vpos too)
- e769f33:docs/architecture/PARAMETER_REGISTRY.md:49 (loudness Auto = no), :52 (ceiling Auto = yes)
- e769f33:worklogs/2026-07-30-p0-anamorph-research.md:258 (P0 research recorded the sibling's 0.45 s sweep window; wheel/automation snap)
- Anamorph@fd78c3b:src/PluginEditor.h:547-550 (knobSweepTime: wheel/automation 'snap and never mislead')
- Anamorph@fd78c3b:src/PluginEditor.cpp:1646-1650, 1752 (snapPos unless sweeping; single position tau 90 ms at :1638)
- runtime V25-BURST-DOWN: rt/verify-25/burst/ceil-down.pkl (analysed with ana.py: text +86.7 ms, 99 % travel ~+535 ms, settled ~+770 ms after the ctl write)
- runtime V25-BURST-UP: rt/verify-25/burst/ceil-up.pkl (text +51.8 ms, 99 % ~+190 ms)
- runtime V25-BURST-WHEEL: rt/verify-25/burst/wheel-down.pkl (three notches, ~300 ms glide each)

**Current behaviour.** Any knob or slider value change not made by pressing that control is drawn as an eased glide toward the new value: wheel, arrow keys, host automation, macro-follow, preset, A/B, undo and reset alike. Increases settle within about 150 ms; decreases take about 450 ms to reach 99 % and about 680 ms to settle. The numeric readout updates immediately.

**Problem.** The drawn pointer and arc disagree with the readout during direct edits (wheel, keys) and automation steps, which the code's own comment and the sibling's owner-approved design both exclude from easing. The up/down asymmetry makes preset sweeps uneven: knobs going up arrive about 3x sooner than knobs going down.

**Root cause.** The lean port of Anamorph's micro-animation driver dropped the knobSweepTime gate, so ease('vpos') runs whenever the slider is not mouse-down, with no discrimination by source. It also reused the hover in/out rates (fast in, slow out) for position instead of the sibling's single symmetric position rate.

**User impact.** Direct wheel or keyboard adjustments feel soft or laggy (about 300 ms per notch). A Ceiling automation step shows the knob arriving up to about 0.7 s after the value. Cosmetic: the readout is always correct, so no wrong decision follows. The only workaround, turning UI Animations off, also removes hover and toggle animation. *Scope:* All eased rotary knobs and linear sliders in both views. Automation applies only to automatable parameters (not the Loudness, Character or Tone macros). Macro-follow easing is visible in Advanced.

**Proposed improvement.** Snap vpos to the live value by default. Ease it only inside a short sweep window (about 0.45 s, as in the sibling) opened by the discrete jump actions: preset step or load, A/B switch, undo/redo and Knob::doReset. Wheel, keyboard, typed entry, host automation and macro-follow then track 1:1. Use one symmetric position rate (a single tau of about 90 ms) so that a sweep moves every knob together. UI Animations off keeps snapping everything.

**Alternatives considered.**

- *Leave as is* — Tolerable because the readout is exact, but it contradicts the code's stated intent and the family's owner decision (#3), and direct edits feel laggy.
- *Fix only the up/down asymmetry* — Removes the uneven sweeps but keeps wheel and automation lag. Partial.
- *Per-knob sweep flag, like the sibling's per-knob resetSweep, instead of a global window* — Finer control but more state. The global window is simpler and proven in the sibling.

**Decision: Proceed · P3.** Reproduced and code-confirmed. The fix is display-only and small, and it restores the intent the code's own comment states, with direct family precedent. Priority stays P3 because the impact is motion polish: the readout is always correct and nothing audible or decision-relevant is wrong.

**Dependencies.** [UI-019](findings-ui.md#ui-019)

**Acceptance criteria.**

- With UI Animations on, a host automation step on Ceiling (harness 'param ceiling 0' from -0.10 dB) draws the pointer at the new value on the first frame in which the readout shows it
- One mouse-wheel notch or one arrow-key step on any knob draws the pointer at the new value on the next frame, with no glide
- Preset step, A/B switch, undo/redo and double-click/alt-click reset still sweep, and knobs moving up and down complete their travel within the same ≤0.5 s window
- With UI Animations off, every change snaps (unchanged)

<details><summary>Verification record</summary>

**Method.** Opened the anchors at e769f33 and Anamorph@fd78c3b. Reproduced on :155 with a region burst capture: Python Xlib get_image of the Ceiling knob, about 400 captures per second, measuring the pointer angle from its white-pixel centroid and a hash of the value text. Host automation step from -0.10 dB to -20 dB: the value text changed first, the pointer reached 90 % of travel about 205 ms later, 99 % at about 450 ms, and settled at about 680 ms. Upward full-range step: 90 % at about 74 ms, 99 % at about 140 ms. Wheel: three downward notches 300 ms apart on the Ceiling knob; each 8.4° step glided for about 300 ms and was still 1-2° short when the next notch arrived. Pointer motion was stepped before the wheel events.

**Corrections to the candidate claim.** (1) Loudness is not host-automatable (PARAMETER_REGISTRY.md:49, ADR-0005), so the automation half applies to Ceiling and the automatable Advanced knobs, not to Loudness. The wheel and keyboard half applies to every knob. (2) The lag depends on direction. vpos reuses the hover rates: up tau ≈34 ms, down tau ≈99 ms (PluginEditor.cpp:2905-2906). A decrease therefore trails about 3x longer than an increase, and one preset sweep moves up-going and down-going knobs at different speeds. (3) Under slow automation ramps the steady-state trail is rate x tau and is small (for example a 3 dB Ceiling ramp over 2 s trails about 2°). The trail is large only for steps or fast ramps. The numeric readout always updates at once, so the display does not lead to a wrong value decision. (4) The code's own comment states the intent as 'ease on jumps (reset, preset, A/B)' (PluginEditor.cpp:2935-2936); the implementation eases every non-drag change. By the code path (not runtime-tested), macro-managed Advanced knobs that follow a Loudness drag and keyboard arrow steps are eased too.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 3 · severity 2 · discoverability 2 · efficiency 1 · coherence 3 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-018

**Family differences from Anamorph (toggle label case, About without the lens flare, Save/Cancel geometry) are not listed as deviation candidates in the brand checklist; the 'no meters icon' item is not a deviation**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | partially-confirmed | Documentation | The inherited frame, renderer and palette were not re-derived for this product's content or the user's display | Phase 5 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:372-373 — setupToggle labels "ADV", "BYPASS"; :526, 546-547, 585, 590-593, 609 — AUTO, TP, SHAPE, MATCH, DELTA, FREEZE, LOCK (ALL CAPS product-wide)
- Anamorph@fd78c3b:src/PluginEditor.cpp:412, 420 — "Adv", "Bypass"; :470-553 — Title Case toggles ('Level Match', 'Mono', 'Swap')
- e769f33:docs/DESIGN.md:898 — wireframe top bar 'ADV [BYPASS]' (strings declared placeholders at :892-894)
- e769f33:src/gui/PluginEditor.h:85-97 — Backdrop {onDismiss, panel, aboutText, dropShadow}: no lensFlare; e769f33:src/gui/PluginEditor.cpp:165 — glass::fillPanel for every overlay
- Anamorph@fd78c3b:src/PluginEditor.cpp:67-79, 572 — the About panel uses a brighter gradient, paintBrightEdges and paintFlare (aboutBackdrop.lensFlare = true)
- e769f33:docs/DESIGN.md:849-851 — About = wordmark, version+build, company, one-line description, hyperlink (no flare)
- e769f33:src/gui/PluginEditor.cpp:1478-1484 — 'Save LEFT, Cancel RIGHT (owner directive 2026-08-05)', half-width split; Anamorph@fd78c3b:src/PluginEditor.cpp:2238-2241 — Cancel 72 px rightmost, Save 72 px to its left
- e769f33:docs/DOCUMENTATION_COVERAGE.md:1781-1782 — the swap recorded as an owner directive
- e769f33:src/gui/LookAndFeel.cpp:264-275; e769f33:docs/HANDOVER.md:757-761 — 'metersicon' removed because Anabasis has no show/hide meters control
- e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:15-18 (two known candidates: font, accent), :45-52 (About, Bypass items), :112 ('none yet')
- Runtime: session capture `rt/state/15a-save-dialog.png` (Save | Cancel, half-width each); session capture `rt/layout/26c-about.png` (plain-glass About); session capture `rt/verify-12/00-initial-crop.png` (top bar: Copy/Settings Title Case, ADV/BYPASS caps)

**Current behaviour.** Anabasis differs from Anamorph in three visible family-chrome details. (1) Toggle labels are ALL CAPS product-wide (ADV/BYPASS vs Adv/Bypass). (2) The About panel uses the plain glass panel without the sibling's bright edges and lens flare. (3) The Save-Preset dialog uses two half-width buttons (same order) instead of two 72 px right-aligned buttons. None of these is listed in the brand checklist's known-candidate note, whose ADR ledger reads 'none yet'.

**Problem.** The Level-5 brand pass, the first item of the fine review, will rediscover these differences with no provenance attached. One of them (Save/Cancel) is an owner directive that could be mistaken for drift and reverted. The user-facing harm is negligible: no workflow or clarity cost is evidenced.

**Root cause.** The divergences arose separately: a product-wide caps labelling convention, an About re-implementation that ported layout and content but not the surface treatment, and an owner directive on the dialog. The checklist's known-candidates paragraph was not updated as they landed.

**User impact.** Minimal. A user of both products sees slightly different label case, dialog button widths and About styling. It does not affect understanding or workflow. *Scope:* Documentation and bookkeeping in BRAND_CONSISTENCY_CHECKLIST.md. No code change is implied until the brand pass rules.

**Proposed improvement.** Extend the checklist's 'known candidates for the deliberate deviation clause' paragraph (:15-18) with three entries, each with its code anchor and provenance: (a) ALL-CAPS toggle labels product-wide (DESIGN wireframe); (b) the About surface without the sibling's bright edges and lens flare (undecided whether the flare is family chrome or Anamorph identity); (c) the Save/Cancel half-width geometry (owner directive 2026-08-05, order unchanged). State explicitly that the absent meters toggle is not a candidate (no counterpart control). The Level-5 pass then either matches each item or records an ADR plus owner sign-off.

**Alternatives considered.**

- *Change the code to match Anamorph now (Title Case toggles, add the flare, 72 px buttons)* — Rejected. It pre-empts the owner's brand pass, reverses a recorded owner directive (Save/Cancel), and the caps convention is product-wide, so changing only ADV/BYPASS would create an internal inconsistency.
- *File an ADR per difference now* — Deferred to the brand pass. The checklist requires an ADR only once an item is ruled a deliberate deviation.
- *Do nothing* — Low cost, but the owner-directive provenance of the Save/Cancel geometry risks being lost at the Level-5 pass.

**Decision: Modify · P3.** Part of the claim holds (the undeclared family differences), part is overstated ('meters icon' is not a deviation; Save/Cancel is documented, just not in the checklist; label case is a coherent product convention). The justified action is the smaller one: record the candidates for the pending brand pass. No evidence supports a code change.

**Architecture gates.**

- No ARCHITECTURE_REVIEW_GATE category touched.
- BRAND_CONSISTENCY_CHECKLIST 'How to use this' (:34-37): any item ruled a deliberate deviation requires an ADR and owner sign-off.

**Dependencies.** [UX-022](findings-ux.md#ux-022) (a wordmark hover cue would join the same candidate list); [DOC-005](findings-doc-test.md#doc-005) (the stale code comment points the brand pass at a non-existent glyph question)

**Acceptance criteria.**

- BRAND_CONSISTENCY_CHECKLIST.md's candidate note lists the label-case, About-surface and Save/Cancel-geometry differences, each with an e769f33-style code anchor and its provenance (DESIGN wireframe / undecided / owner directive 2026-08-05).
- The note states that the absent meters toggle is not a candidate, with the reason.
- After the Level-5 pass, each listed item is either checked as matching or referenced by an ADR in the 'Deviations approved by ADR' line.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/PluginEditor.cpp:372-373, 526-609 (all setupToggle labels), 165, 1476-1484; e769f33:src/gui/PluginEditor.h:85-97 (Backdrop has no lensFlare member); e769f33:src/gui/LookAndFeel.cpp:264-275; e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:15-18, 43-60, 112; e769f33:docs/DESIGN.md:845-866, 892-898; e769f33:docs/DOCUMENTATION_COVERAGE.md:1781-1782; e769f33:docs/HANDOVER.md:757-761. Compared against Anamorph@fd78c3b:src/PluginEditor.cpp:412, 420, 470-553, 67-79, 572, 2233-2241. Viewed the ST-04 Save dialog screenshot, the LAY-17 top bar and the About captures.

**Corrections to the candidate claim.** (1) Label case is confirmed (ADV/BYPASS vs Adv/Bypass), but it is not an isolated divergence. Anabasis labels EVERY toggle in ALL CAPS (AUTO, TP, SHAPE, MATCH, DELTA, FREEZE, LOCK), matching the DESIGN wireframe 'ADV [BYPASS]' (DESIGN.md:898), while Anamorph uses Title Case throughout. It is a product-wide labelling convention; within Anabasis the only mix is Title Case 'Copy'/'Settings' buttons next to ALL-CAPS toggles. (2) The About lens flare is absent (confirmed), but the checklist's About item covers how About opens, its content and dismissal, not its surface treatment. DESIGN §6.1's About spec omits the flare, and the flare is Anamorph's 'anamorphic' product motif. Whether it is family chrome or product identity is undecided in the docs; no harm is evidenced. (3) 'No meters icon' is NOT a deviation. Anabasis has no show/hide meters control because the §6.3 strip is always present, and the unreachable 'metersicon' variant's removal is documented (LookAndFeel.cpp:264-275, HANDOVER.md:757-761). (4) Save/Cancel: the ORDER matches the sibling (Save left of Cancel). Only the geometry differs (two half-width ~142 px buttons vs two 72 px right-aligned), and it is an owner directive recorded at PluginEditor.cpp:1482 and DOCUMENTATION_COVERAGE.md:1781. It is not undeclared, though it is absent from the checklist. (5) 'Deviations approved by ADR: none yet' is accurate by design: the Level-5 pass has not run, and the checklist's known-candidates note (:15-18) lists only the font and the accent.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 2 · severity 1 · discoverability 2 · efficiency 1 · coherence 2 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-019

**While isShowing() is false, stepMicroAnims skips all easing, so drawn knob and toggle states freeze; confirmed only under a forced condition, and the realistic effect is a catch-up sweep after restore**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | medium | partially-confirmed | State management | The inherited frame, renderer and palette were not re-derived for this product's content or the user's display | Phase 5 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:2900-2903 (early return on ! isShowing())
- e769f33:src/gui/PluginEditor.cpp:2930-2944 (onA and vpos updated only past that return)
- e769f33:src/gui/LookAndFeel.cpp:50-54 (knob draws vpos when not dragging)
- e769f33:src/gui/LookAndFeel.cpp:260-261 (toggle draws eased onA)
- e769f33:src/gui/PluginEditor.cpp:372-373, 1222 (ADV/BYPASS toggles registered as animated)
- e769f33:src/gui/PluginEditor.cpp:2546-2553 (defensive claim that a wrapper can report not-showing while on screen; no host named)
- Anamorph@fd78c3b:src/PluginEditor.cpp:1582 (isShowing only feeds mouseInside), :1752 (snapPos when not sweeping)
- runtime V25-HIDDEN (forced _NET_WM_STATE_HIDDEN, atoms pre-interned): session capture `rt/verify-25/34-hidden-strip.png` (panels: before -10 dB; hidden after param -> readout -20.00 dB, knob still at -10 dB; 3 s later unchanged; after un-hide at -20 dB), frames 30-33-*.png
- runtime V25-UNHIDE catch-up sweep: rt/verify-25/burst/unhide2.pkl (pointer -1.5° -> -140° over ~0.4 s after the property was removed)
- runtime V25-HIDDEN-NULL (invalid first attempt, atoms not known to JUCE): session capture `rt/verify-25/13-hidden-strip.png`, 27-menu-compare.png

**Current behaviour.** Whenever the editor's JUCE hierarchy reports not-showing, the vblank callback returns before touching any animated property. Drawn knob positions (vpos) and toggle states (onA) keep their last eased values. The numeric readouts, which do not go through the driver, keep updating. When showing resumes, every changed control eases from its stale position to the current value.

**Problem.** If the editor is painted while isShowing() is false, knobs and toggles would show stale states beside correct readouts. That is proven possible mechanically but not observed in any real host. In ordinary use (restoring a minimised editor) the stale values produce a brief, unrequested catch-up sweep of every control that changed while hidden.

**Root cause.** The idle guard was placed ahead of the value-derived properties (vpos, onA), which carry state, instead of only ahead of hover/press work. Nothing resynchronises them on the not-showing to showing edge.

**User impact.** Hypothetical in the worst case, where a host that paints while reporting not-showing would leave knobs and the BYPASS toggle drawing wrong positions. In practice the effect is a cosmetic 0.3-0.7 s sweep after restore. No audio or state is affected. *Scope:* All registered animated knobs, sliders and toggles, in both views.

**Proposed improvement.** Keep the cheap early return, but resynchronise on the edge. On the first tick where isShowing() is true after one where it was false, call seedAnimatedFromValues() (vpos and onA snapped to the live values) and zero hovA/actA before easing resumes. Alternatively, while not showing, snap vpos and onA to their targets without repainting instead of returning outright. Either way the drawn state always matches the value once visible, with no catch-up sweep.

**Alternatives considered.**

- *Leave as is* — Acceptable while no host reports not-showing during painting. The restore-time catch-up sweep remains a small artefact.
- *Remove the isShowing() return entirely* — Removes the freeze but makes every hidden vblank do the full loop and eases while hidden. Snapping or re-seeding is cheaper and more correct.
- *Port the sibling's generation-gated loop* — More machinery than this needs, with its own history of bugs (KI-025).
- *Investigate further in real hosts first* — Useful to confirm the worst case, but the resync fix is trivial and correct regardless, and it also fixes the observable restore sweep.

**Decision: Modify · P3.** The worst case in the claim is unconfirmed in real hosts. The underlying defect, stateful easing skipped with no resync, is confirmed at runtime and has an observable restore-time effect. A small resync is lower risk than porting the sibling's gate. Cosmetic impact, so P3.

**Dependencies.** [UI-017](findings-ui.md#ui-017)

**Acceptance criteria.**

- Under the forced-hidden reproduction (Xvfb, _NET_WM_STATE/_NET_WM_STATE_HIDDEN interned before launch, _NET_WM_STATE_HIDDEN set on the harness window), 'param ceiling 0' results in the Ceiling knob being drawn at -20 dB no later than the first frame after the property is removed, with no sweep
- A host-automated BYPASS change made while not showing is drawn in its new state on the first visible frame
- Restoring the editor after values changed while hidden produces no knob sweep; preset/A-B/reset sweeps while visible are unchanged

<details><summary>Verification record</summary>

**Method.** Opened the anchors at e769f33 and the pinned JUCE sources (Component::isShowing, and isMinimised on X11, Windows and macOS). Reproduced on :155 by forcing JUCE's peer to report itself minimised while still mapped and painted. The _NET_WM_STATE and _NET_WM_STATE_HIDDEN atoms were interned by a persistent client before the harness started, then _NET_WM_STATE_HIDDEN was set on the unmanaged harness window with xprop. On a first attempt without pre-interned atoms, JUCE had cached None at init. A popup-dismissal probe then confirmed isShowing() stayed true, and the knob moved normally (13-hidden-strip.png, 27-menu-compare.png), so that run was discarded. On the valid run, 'param ceiling 0' changed the readout to -20.00 dB while the drawn Ceiling knob stayed at the -10 dB position for 3.5 s. After the property was removed, the pointer swept from -1.5° to -140° over about 0.4 s.

**Corrections to the candidate claim.** (1) The mechanism is real, but no host has been shown to report isShowing() false while the editor is painted. JUCE's isShowing requires every ancestor to be visible, and an invisible ancestor would also stop painting. It also requires !peer->isMinimised(): on X11 that reads _NET_WM_STATE_HIDDEN on the plugin's own window, which window managers do not set on embedded child windows; on Windows it reads the child HWND's showCmd; on macOS the host NSWindow's miniaturised flag. The comment at PluginEditor.cpp:2546-2553 asserts such a wrapper defensively, names no host, and the docs record none. (2) The freeze is broader than knob pointers: onA, which draws the ON/OFF state of every registered toggle including the host-automatable BYPASS, freezes too, as do hovA and actA. (3) The realistic effect: minimise, let automation, a host program change or the preset recall change values, then restore. Every changed knob then sweeps from its stale position and toggles fade, because easing resumes from the frozen values. This is cosmetic and lasts under about 0.7 s. (4) The sibling keeps vpos current while hidden: its isShowing() feeds only mouseInside, and a generation change still runs the loop with snapPos.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 1 · severity 2 · discoverability 3 · efficiency 1 · coherence 2 · change risk 1 · complexity 1 · evidence 3</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-020

**Release-outside stuck press on a value-box drag: does not reproduce on X11, and the pinned JUCE already handles capture loss on Windows**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Reject** | **none** | high | refuted | Interaction model | Pointer and text entry run on untuned JUCE defaults, so ordinary slips rewrite values | — |

**Evidence**

- e769f33:src/gui/LookAndFeel.cpp:870 (dragging=true on press), :886 (cleared on mouseUp)
- e769f33:src/gui/PluginEditor.cpp:2927 (actA from isMouseButtonDown(true))
- JUCE 9.0.1 (fetched, build/_deps/juce-src) juce_gui_basics/native/juce_Windowing_windows.cpp:2617 (SetCapture on press), :2659-2670 and :3907-3909 (capture loss -> synthesised mouseUp) — pinned JUCE in the build tree
- Anamorph@fd78c3b:worklogs/MOUSE_RELEASE_STATE_FIX_v0.8.12.md §4-§5 (validated by reasoning; Slider gesture ends via the synthesised mouseUp; recovery on re-entry)
- runtime V25-VBOX (display :155): session capture `rt/verify-25/43-vbox-strip.png` (pressed; dragged outside, still pressed; released outside, idle pointer 215/221/230 = idle reference), frames 40/41/42-*.png
- runtime G-09 (observer): knob drag released outside ends cleanly — session capture `rt/gestures/19a-loud-drag-past-top.png`

**Current behaviour.** A value-box drag, or a knob drag, released outside the editor window ends cleanly on X11. The press glow and the 'dragging' flag clear on release. Capture loss on Windows is converted to a mouseUp by the pinned JUCE.

**Problem.** No user-facing problem was found. The finding rests on a lost-release scenario that neither product has reproduced.

**Root cause.** n/a. The platform event paths in the pinned JUCE already deliver or synthesise the release.

**User impact.** None observed. In the hypothetical host that swallowed a release, the knob would look pressed until the pointer re-entered the window, and would then recover by itself. *Scope:* Value-box and knob drags that leave the editor window.

**Proposed improvement.** No change. Keep the current behaviour and reopen only on a host-specific field report of a knob staying drawn as pressed after an outside release, reproduced in that host.

**Alternatives considered.**

- *Port Anamorph's 0.8.12 reconcile (realtime OS query in the timer, plus an AND-gate in the glow)* — Small, but it adds OS polling and ordering subtleties (the sibling needed a follow-up fix) to guard a case with no reproduction on any platform. Not justified now.
- *Investigate further* — Only worthwhile with a concrete host report. Nothing in this repo or the sibling's record points to one.

**Decision: Reject · none.** The claim was actively refuted on X11 for the exact path it names. The pinned JUCE handles capture loss on Windows, macOS routes the mouseUp to the view that took the press, and any residual case is cosmetic and self-healing. Porting a speculative safety net is not justified by the evidence.

**Dependencies.** None.

**Acceptance criteria.**

- No code change for this finding
- Reopen criterion: a field report naming a host in which a knob or value box stays drawn as pressed after a release outside the plugin window, reproduced in that host with stepped pointer motion, before any port of the sibling's reconcile is considered

<details><summary>Verification record</summary>

**Method.** Confirmed the code claims: ValueBox sets and clears 'dragging' at LookAndFeel.cpp:870/886, actA reads isMouseButtonDown(true) at PluginEditor.cpp:2927, and neither isMouseButtonDownAnywhere nor getCurrentModifiersRealtime appears in src. Reproduced on :155: stepped approach to the Ceiling value box at (517,518), press, a 10-step drag down to y=950, then outside the editor to (1300,960), release outside. The pointer measured 254-255 white (pressed) while held and 215/221/230 (the idle colour, identical to the idle reference frame) 0.6 s after the outside release, so the press cleared. Read the pinned JUCE: on Windows, SetCapture is taken on press (juce_Windowing_windows.cpp:2617), and WM_CAPTURECHANGED synthesises a mouseUp while dragging (:2659-2670, :3907-3909). X11 gives the pressed window an implicit pointer grab. Read the sibling's fix record: it was validated by 'event-path reasoning' only, with no reproduction, and it states that recovery on cursor re-entry is intact (the Slider gesture ends through the mouseUp JUCE synthesises on the next event).

**Corrections to the candidate claim.** The code facts are accurate, but the claimed exposure ('a cosmetic stuck-pressed knob on Windows or Linux') is not supported. X11 delivers the outside release (reproduced for the value-box path, matching G-09 for the knob path). The pinned JUCE converts a Windows capture loss into a mouseUp. The sibling never reproduced a lost release either. Even if a host did swallow the release, the state self-heals on the next mouse event in the window.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 1 · severity 1 · discoverability 2 · efficiency 1 · coherence 1 · change risk 2 · complexity 2 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-021

**Disabled Undo/Redo buttons still show the hover wash: the background brightens exactly as on an enabled button while the glyph stays at 40 %**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P3** | high | confirmed | Visual hierarchy | The inherited frame, renderer and palette were not re-derived for this product's content or the user's display | Phase 5 |

**Evidence**

- e769f33:src/gui/LookAndFeel.cpp:325 — hovA = animOr(b, "hovA", highlighted); :343-344 — base = bgRaised.brighter(0.06f * hovA); isEnabled never read in :318-359
- e769f33:src/gui/LookAndFeel.cpp:456 — icon glyph colour multiplied to 0.4 alpha when !isEnabled()
- e769f33:src/gui/PluginEditor.cpp:2926-2928 — over = isMouseOver(true); ease("hovA", over ? 1 : 0), regardless of isEnabled
- e769f33:src/gui/PluginEditor.cpp:365-366 — undo/redo registerAnimated; :2138-2141 — the only setEnabled calls in src/gui (undo/redo follow canUndo/canRedo)
- Anamorph@fd78c3b:src/gui/LookAndFeel.cpp:366-373 and Anamorph@fd78c3b:src/PluginEditor.cpp:1687-1692 — the same pattern in the sibling (no enabled term)
- Runtime V12-1: session capture `rt/verify-12/14-hover-strip.png` — rows: rest; disabled-Undo hovered; rest; Copy hovered. The background pixel (678,60) goes (35,40,49)→(47,51,59) for DISABLED Undo, identical to Copy's (625,60) (35,40,49)→(47,51,59); max diff (12,11,10) in both boxes
- Phase-2 ST-07 (enabled/disabled glyph states distinct)

**Current behaviour.** When Undo or Redo is unavailable, its glyph is drawn at 40 % alpha but its background and outline are identical to an enabled button. On hover the background eases to the same brighter wash an enabled button gets. Clicking does nothing.

**Problem.** A disabled control that responds to hover gives a mixed signal ('interactive'). The only remaining disabled cue is the glyph alpha, which is less legible on small glyphs.

**Root cause.** The micro-animation driver computes the hover target from pointer containment alone, and the background painter reads that eased property instead of JUCE's 'highlighted' argument (which is already false for disabled buttons). Neither consults isEnabled().

**User impact.** Small. On every session start both buttons are disabled, and Redo is disabled most of the time. A user hovering them gets a 'clickable' cue, then a no-op click. There is no data risk. *Scope:* undoButton and redoButton only: the only components ever disabled. It would also affect any future disabled registered TextButton.

**Proposed improvement.** Disabled buttons do not react to hover. In drawButtonBackground, multiply hovA by (b.isEnabled() ? 1 : 0), and optionally draw the outline and fill at reduced alpha when disabled, so the whole control reads as unavailable. Alternatively set the driver's hover target to over && comp->isEnabled(), which eases out smoothly when a button becomes disabled under the pointer.

**Alternatives considered.**

- *Gate in the painter (hovA * isEnabled)* — Smallest and local. The wash snaps off if a button becomes disabled under the pointer, which is acceptable.
- *Gate in stepMicroAnims (target = over && isEnabled)* — Smooth transition and covers every registered widget. Touches the shared driver, still low risk.
- *Leave as-is (matches the sibling)* — The family behaviour is identical, but checklist B asks for a coherent treatment of disabled states, and this is inconsistent with the dimmed glyph.

**Decision: Proceed · P3.** Reproduced and code-confirmed. The fix is a one-line, gate-free visual correction that makes the disabled state coherent. It deserves P3 because the impact is cosmetic and no action is lost.

**Architecture gates.**

- No ARCHITECTURE_REVIEW_GATE category touched.
- BRAND_CONSISTENCY_CHECKLIST B 'treatment of … disabled states': the sibling shares the current behaviour, so the fix is a minor family divergence to note (or propose family-wide).

**Dependencies.** None.

**Acceptance criteria.**

- With canUndo()==false, hovering Undo for 500 ms leaves the button's background pixels unchanged vs rest (max channel diff ≤ 1).
- With canUndo()==true, hovering Undo brightens its background as Copy's does (diff ≥ 8).
- Enabled Copy/Settings hover behaviour is unchanged (same pixel delta as today).
- If the driver-level gate is chosen: an Undo that becomes disabled while hovered eases back to rest within the normal ease-out time.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/LookAndFeel.cpp:318-359 (drawButtonBackground: hovA from the component property, base = bgRaised.brighter(0.06f*hovA), no isEnabled read) and :456 (glyph alpha 0.4 when disabled). Read e769f33:src/gui/PluginEditor.cpp:2926-2928 (hovA target = isMouseOver(true), no enabled check), :365-366 (undo/redo registered), and :2138-2141, the only setEnabled sites in the GUI. Runtime on :142 (V12-1): at session start (canUndo false, glyph dim) the pointer was moved onto Undo with stepped moves plus a relative wiggle. Absolute XTest moves alone did not register hover in this harness, the known warp artefact; the enabled-Copy control test confirmed hover delivery. Captured rest vs hover for disabled Undo and for enabled Copy.

**Corrections to the candidate claim.** The effect is a background brightening ('wash'), not a 'lift'. JUCE itself passes highlighted=false and down=false for disabled buttons and ignores the click, so there is no press feedback and no action. Only the eased hovA property drives the wash. The behaviour is inherited: Anamorph's painter and driver also ignore the enabled state (Anamorph@fd78c3b:src/gui/LookAndFeel.cpp:366-373; Anamorph@fd78c3b:src/PluginEditor.cpp:1687-1692).

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 4 · severity 1 · discoverability 2 · efficiency 1 · coherence 2 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### UI-022

**Input Gain reads '-0.0 dB' after a host writes a normalised value just below its default**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Reject** | **none** | high | partially-confirmed | Visual hierarchy | The inherited frame, renderer and palette were not re-derived for this product's content or the user's display | — |

**Evidence**

- e769f33:src/PluginParameters.cpp:85 — dbText = String(v, 1) + ' dB', with no sign normalisation
- e769f33:src/PluginParameters.cpp:314 — Input Gain range {-12, 24}, default 0, so the default normalised value is 1/3 (not exactly representable in float)
- e769f33:src/gui/PluginEditor.cpp:1178 — the Knob reset value is convertFrom0to1(getDefaultValue()), which displays '0.0 dB'
- G-11: session capture `rt/gestures/36-sheet.png` (viewed) shows '-0.0 dB' after 'param inputGain 0.3333' and '0.0 dB' after 'paramtext 0.0 dB'
- verify-26/inputgain-sweep (display :156): 0.3333 gives '-0.0 dB', 0.33333334 gives '0.0 dB', 0.333333 gives '-0.0 dB', 0.33 gives '-0.1 dB', 0.3307 gives '-0.1 dB' (rt/verify-26/app.log)

**Current behaviour.** Any dB readout whose value lies in (-0.05, 0) prints '-0.0 dB'. Values in [0, 0.05) print '0.0 dB'. The true default (and reset) of every dB parameter prints '0.0 dB'.

**Problem.** Only a cosmetic asymmetry: slightly negative sub-resolution values show a sign and slightly positive ones do not. There is no evidence that the minus misleads. The value really is not the default.

**Root cause.** String(v,1) keeps the sign of a small negative value that rounds to zero. The trigger was a host-side normalised value slightly below the default, not the default itself.

**User impact.** Negligible. After a quantised automation recall the readout says '-0.0 dB' for a gain that really is 0.001 dB below unity (inaudible), and the default still reads '0.0 dB'. *Scope:* Any dB readout for values in (-0.05, 0). Input Gain is the only dB parameter whose default normalised value is not exactly representable.

**Proposed improvement.** No change. If the owner later wants symmetric zero rounding as polish, the rule belongs in the display-format table proposed under [UI-003](findings-ui.md#ui-003) (for example, print '0.0 dB' when |v| < 0.05), so it is applied to every dB formatter at once rather than patched on one.

**Alternatives considered.**

- *Normalise the sign: print '0.0 dB' when |v| < 0.05* — Cheap. It hides a genuinely non-default value, and it runs against the product's own rule that a readout must not deny the value it holds (pctText rationale, PluginParameters.cpp:87-107). There is no user harm to justify it.
- *Leave as-is* — Chosen. The display is an accurate rounding, and the default and reset already read '0.0 dB'.

**Decision: Reject · none.** The claim that a host writing the default gives '-0.0 dB' is refuted: the exact default normalised value reads '0.0 dB'. What remains is a correct rounding of a slightly negative, non-default value. There is no evidence of harm, and 'fixing' it would conflict with the product's own principle that a readout must not hide a value it holds.

**Dependencies.** None.

**Acceptance criteria.**

- (No change.) Regression guard only: 'param inputGain 0.33333334' and a double-click reset both display '0.0 dB'

<details><summary>Verification record</summary>

**Method.** Read dbText (e769f33:src/PluginParameters.cpp:85) and the inputGain definition (:314, range −12…24, default 0, so the default normalised value is 1/3). Read the Knob reset path (e769f33:src/gui/PluginEditor.cpp:1178). Viewed session capture `rt/gestures/36-sheet.png`. Reproduced on :156 with `param inputGain <norm>` for five values.

**Corrections to the candidate claim.** The premise is wrong. Writing the actual default normalised value (0.33333334f) shows '0.0 dB'. '-0.0 dB' appears only for values just below the default: 0.3333 gives -0.0012 dB and 0.333333 gives -0.000012 dB. So G-11's harness write of 0.3333 was a truncated, genuinely non-default value, and the display is an accurate rounding of it. The same '-0.0 dB' appears for any dB value in (-0.05, 0), for example an EQ gain dragged to just under zero. Double-click or Alt-click reset gives exactly '0.0 dB'.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 1 · severity 1 · discoverability 1 · efficiency 1 · coherence 1 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

