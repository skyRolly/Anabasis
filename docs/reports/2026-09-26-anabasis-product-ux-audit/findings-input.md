# Anabasis product / UX audit — findings: Input, keyboard and accessibility

Part of [`2026-09-26-anabasis-product-ux-audit.md`](../2026-09-26-anabasis-product-ux-audit.md) (audited revision `e769f33`, 2026-09-26). This file holds the complete record of each finding in these categories; the report carries the index, the systemic themes, the roadmap and the decision record. Code anchors are pinned to `e769f33`; runtime observation ids refer to [`worklogs/2026-09-26-product-ux-audit.md`](../../../worklogs/2026-09-26-product-ux-audit.md).

Each record: decision, priority and confidence after calibration; evidence; current behaviour; problem; root cause; user impact and scope; proposed improvement; alternatives considered; decision rationale (with any calibration or challenge outcome); architecture gates; dependencies; acceptance criteria; and the verification record. Terms in the records: the *candidate claim* is the claim as it entered verification; *the judge* is the verifier's decision pass (Phase 3, step 3), done per *batch* of 3–6 related findings; *Adversarial challenge* is the step-4 review and *Calibration* the Phase-4 pass that set the final decision and priority (see the report's *Evidence and method*). A paragraph marked *Merged at triage from another verifier's note* is evidence from another batch's verifier, kept in its words: 'add to X' there means it has been added to this record. 'Recorded at triage' marks a finding written from such a note. `rt/…` paths and ids such as `VER0-2` or `V24-TSAN-1` name uncommitted session captures, logs and probes; `PF-…` ids are potential findings from the uncommitted Phase-1 evidence maps.

## INPUT — Input, keyboard and accessibility

### INPUT-001

**Typed value entry commits a value for unparseable text: garbage, an empty field or a typographic minus give 0, which drives the Ceiling to 0.00 dB with no error, and a comma decimal is cut at the comma ('-2,5' gives -2.00)**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | confirmed | Accessibility/input | Pointer and text entry run on untuned JUCE defaults, so ordinary slips rewrite values | Phase 3 |

**Evidence**

- e769f33:src/PluginParameters.cpp:117 — dbFrom = t.removeCharacters("dB ").getFloatValue(), with no validation
- e769f33:src/PluginParameters.cpp:142-189 (pctFrom, hzFrom, khzFrom, hzKhzFrom) and :283-288 (Character/Tone) — every parser is getFloatValue with no validation
- JUCE 9.0.1 juce_Slider.cpp:445-455 — textChanged commits snapValue(getValueFromText(text)) inside a ScopedDragNotification. juce_Label.cpp:493-511 — Return (and focus loss) commits whenever the editor text differs from the label, including an empty field
- JUCE 9.0.1 juce_CharacterFunctions.h:223-262 — sign handling only for ASCII '-'/'+'. A leading non-digit gives 0.0. 'nan' gives quiet_NaN
- e769f33:src/gui/PluginEditor.h:206 — every slider in the editor is a Knob (members at :459-496), and juce_Slider.h:760 getValueFromText is virtual, so there is a single editor-side interception point
- e769f33:tests/state_tests.cpp:5861-5963 — parser tests cover valid shorthand only. No invalid, empty or comma case is tested
- G-06: session capture `rt/gestures/15-sheet.png`, session capture `rt/gestures/15e-loud-typed-abc-crop.png`. E05: session capture `rt/edges/33-abc-typed-crop.png`, session capture `rt/edges/33-abc-return-crop.png`, [capture](captures/06-ceiling-typed-entry.png), session capture `rt/edges/33-grid-b.png`
- verify-26/ceiling-garbage (display :156, stepped motion): 'abc'+Return gives 'Ceiling = 0.00 dB [1.0000]' (session capture `rt/verify-26/02-abc-return-crop.png`). The Undo click restores '-0.10 dB [0.9950]'
- verify-26/ceiling-inputs, from -1.00 dB: empty gives 0.00, '-2,5' gives -2.00, '-0,5' gives 0.00, '-3 dBTP' gives -3.00, '- 3' gives -3.00 (rt/verify-26/app.log dumps)
- *Merged at triage from another verifier's note, quoted as written:* A sharper worst case for [INPUT-001](findings-input.md#input-001). That record already notes 'nan' → quiet_NaN and requires finite parser output. verify-26 shows every value box accepts 'nan', and the result is worse than 0.
  - Ceiling 'nan' gives 'ceiling = nan dB [nan]'. decibelsToGain(NaN) at e769f33:src/dsp/AnabasisEngine.cpp:560 (re-checked) mutes the output: M, S, RMS and out LUFS go to '-' and the GR trace drops to the floor (session capture `rt/verify-26/23-after-ceil-nan.png`).
  - The session is saved with `<PARAM id="ceiling" value="nan" raw="nan"/>` (rt/verify-26/nan-state.xml, re-checked).
  - Undo restores the audio, but the knob's eased display never recovers, even after a later valid value. ease() at e769f33:src/gui/PluginEditor.cpp:2913-2923 (re-checked) computes v + (target - v)*a, which stays NaN once v is NaN.
  - The existing NaN guard (e769f33:tests/state_tests.cpp:205-230) covers state restore only.

  Add acceptance items: typed 'nan' or 'inf' never reaches a parameter; getStateInformation never serialises a non-finite value; the micro-animation ease resets a non-finite stored value to its target.

**Current behaviour.** The value-box editor, and host text entry through getValueForText, parse the leading number with juce::String::getFloatValue. Unparseable text, an empty field, a comma decimal (read up to the comma) and a Unicode minus all produce 0 or a truncated value, which is clamped into range and committed as a gesture. There is no error cue. On the Ceiling, 0 is 0.00 dB, its least-safe end. On Comp Threshold, 0 dB is 'no compression'. Most other controls land on their default or minimum.

**Problem.** An ordinary typing slip, or a European decimal comma (including the numpad decimal key on comma-decimal layouts), silently replaces the intended value. The worst case lands on the one control whose value is the product's core delivery promise. The product already advertises forgiving shorthand ('2k', '-1 dBTP', '0.5' as 50 %), which makes the silent failure more surprising.

**Root cause.** The parsers assume well-formed numeric text: getFloatValue returns 0 (or NaN for 'nan') on failure. Neither the parameter parsers nor the Slider/Label commit path has a validity check or a revert branch.

**User impact.** A mastering engineer who types '-0,3' or mistypes into the Ceiling ends up with a 0.00 dBFS sample-peak ceiling. With TP off (the default) that allows inter-sample overs past delivery spec, and nothing warns except the changed readout and possibly the TP row's red colour. Recovery is a single Undo, but only if the user notices. *Scope:* Every value box in both views (about 40 knobs plus 2 faders) and host generic-UI text entry (same parsers, not runtime-tested). Harm is concentrated on the Ceiling (0 = maximum) and Comp Threshold (0 = compressor effectively off). A comma decimal truncates on every parameter.

**Proposed improvement.** Target experience: invalid text never changes the parameter. Pressing Return (or clicking away) on empty or unparseable text restores the previous value, writes nothing, records no undo step, and briefly shows the field in the warn colour. A comma decimal and a typographic minus are read as the user meant them. Mechanism: (1) Editor side, override Knob::getValueFromText. If the unit-stripped text contains no parseable finite number, return getValue(). JUCE's textChanged then sees no change and updateText() restores the readout. Trigger a short warn flash on the ValueBox. (2) Parser side, shared by host and editor: before getFloatValue, map U+2212 and U+2013 to '-' and ',' to '.' when the text has no '.'. Never return a non-finite value: return a finite fallback such as the parameter default for NaN; ±inf already clamps. (3) Add state tests through the real parameters for 'abc', '', '-0,5', '−3', 'nan' and 'inf'.

**Alternatives considered.**

- *Leave as-is* — Rejected. It silently moves the delivery-critical control to its least-safe value, and comma-decimal users hit it on every decimal entry.
- *Editor-side validation only (Knob::getValueFromText)* — Minimal, UI-only, with no host-visible change. It leaves host-typed garbage and comma decimals wrong.
- *Parser-side hardening only* — Fixes comma, minus and NaN on both paths, but a parser cannot reject, so garbage still lands on some value.
- *Both editor rejection and parser normalisation* — Chosen. Neither part touches IDs, ranges, defaults, schema or the DSP.
- *Modal error dialog* — Rejected. It is heavy-handed for a typo and off the family's interaction grammar.

**Decision: Proceed · P2.** Reproduced and code-confirmed with no mitigating design rationale in the code or docs. The manual promises forgiving entry, and RISK-003 frames the Ceiling as the core promise. The verifier placed it at P1 rather than P0, because the committed value is visible in the readout and one Undo recovers it while it is a frequent trap for comma-decimal users on the most important control of every mastering pass; calibration lowered it to P2 (below). The ceiling guarantee (ADR-0006) is not touched, since the clamp still holds whatever value is set.

*Calibration:* the verifier judged Proceed / P1; the final judgement is Proceed / P2. Challenge accepted: the committed value shows in the box just edited and one Undo restores it, and the every-entry exposure (comma-decimal numpad users) is unevidenced, so P2; Proceed stays (UI-local, no gate). Merge note sharpens the worst case: typing 'nan' in the Ceiling mutes the output (decibelsToGain(NaN), e769f33:src/dsp/AnabasisEngine.cpp:560), serialises value='nan' into the session (rt/verify-26/nan-state.xml), and the eased knob never recovers because ease() propagates NaN (e769f33:src/gui/PluginEditor.cpp:2913-2923). Add acceptance: typed nan/inf never reaches a parameter; getStateInformation never serialises a non-finite value; ease() resets a non-finite stored value. Keep ADR-0024 snapping after normalisation; reject only on no ASCII digit or a non-finite result, so '0' and '.5' still commit; the warn flash is message-thread state.

*Adversarial challenge:* evidence holds: yes; priority justified: no (suggested P2); decision justified: yes (suggested Proceed). The evidence holds and the fix direction is sound, but P1 does not meet the rubric. P1 needs either (i) material harm to a common workflow every session or pass, or (ii) a frequent trap with costly recovery. On (ii), recovery is not costly. The judge's own rationale concedes the value shows in the box just edited and one Undo restores it. On (i), typing the Ceiling may happen every pass, but the defect fires only on invalid text. For '.'-decimal users that is an occasional slip. The every-entry case is comma-decimal numpad users, and the repo has no evidence (KNOWN_ISSUES, CHANGELOG, OPEN_QUESTIONS) of how common that path is for the target audience. The worst realistic case is also narrower than 'wrong mastering decision': with TP off, 0.00 dB is a sample-peak limit at full scale that still clamps, so the risk is inter-sample overs a user misses when '-0,1' reads back as '0.00 dB' instead of '-0.10 dB'. That is real but visible at the point of entry. This fits P2: a meaningful robustness and clarity fix in a lower-cost situation. Suggested scores: severity 3 (not 4, the output is displayed), frequency 2 (not 3, it needs invalid text), user_impact 3. evidence_confidence 5 stands. Raising this to P1 would need evidence that comma or numpad decimal entry is a routine path for the product's users. Proceed stays right: the change is small and UI-local, crosses no gate, and also closes the separately reported 'nan'-into-parameter path. A NaN guard would also match the product's existing stance, since the state-restore NaN guard was already fixed (CHANGELOG.md:380-413). *Proposal risks:* No hard-stop gate is crossed. IDs, ranges, defaults, choice order, schema, threading, signal order and latency are all untouched, and rules 1-7 of PARAMETER_COMPATIBILITY_POLICY do not cover text parsers. Constraints the judge did not name:
(a) The parser-side comma/minus mapping changes host-visible getValueForText, which ADR-0024's table (typed text 'snaps on the way in') and e769f33:tests/state_tests.cpp:11020-11045 pin. Snapping must stay after normalisation, and the existing ceiling2dp and pctText round-trip tests must stay green.
(b) Mapping ',' to '.' also turns a thousands separator into a decimal ('1,500' ms reads as 1.5 ms instead of today's 1 ms). Neither result is right and it is no regression, but the acceptance criteria must not imply thousands handling.
(c) The editor-side 'no parseable number' predicate must not become a second parser that disagrees with the per-parameter ones. Rejecting on 'the parser returned 0' would wrongly reject a real '0' (Comp Threshold 0 dB, Tone 0.00, Loudness 0). It must still accept '.5', '+3', '8k', '60%', '2:1', '-1 dBTP' and '- 3'. The safe predicate is 'contains at least one ASCII digit after minus normalisation, and the result is finite'. Add acceptance cases for '0' and '.5' through the editor path.
(d) The warn flash must be message-thread, timer-driven state on the LookAndFeel-created ValueBox. It must not become a new painting-thread read of editor bookkeeping, which ADR-0027 governs. A revert with no cue risks an 'I typed and nothing happened' confusion, so the flash (or an equivalent cue) is warranted, not optional polish.
(e) DOCUMENTATION_LIFECYCLE_POLICY sync is not listed: USER_MANUAL.md:145-149 (invalid entry is rejected, comma accepted), CHANGELOG, and a TESTING_POLICY regression test at editor level for the rejection. state_tests already builds editors (the hover-hint sweep), so this is feasible.
(f) The family divergence is permitted by ADR-0009 but should be recorded as such. Anamorph has the same getFloatValue parsers (Anamorph@fd78c3b:src/PluginParameters.cpp:155-191).

**Dependencies.** Typing 'nan' writes NaN into a parameter (merged into this record; see Evidence) — same root cause, closed by the same parser/editor validation; [UI-002](findings-ui.md#ui-002) (the editor's visual rejection state)

**Acceptance criteria.**

- With the Ceiling at -1.00 dB, double-click, type 'abc', Return: the Ceiling stays -1.00 dB, no parameter-change gesture reaches the host, the undo history gains no entry, and the field shows a visible rejection cue
- The same happens for an empty field + Return and for an empty field + click-away
- '-0,5' gives -0.50 dB and '-2,5' gives -2.50 dB. '−3' (U+2212) gives -3.00 dB, via a parser unit test through the real parameter
- getValueForText returns a finite value for every parameter for 'nan', 'inf', '-inf', '' and 'abc' (state test over all parameters)
- All existing shorthand tests pass unchanged ('-6 dB', '-1 dBTP', '+3', '8k', '0.5' as 50 %, '0.1%' as 0.1 %)

<details><summary>Verification record</summary>

**Method.** Read dbFrom (e769f33:src/PluginParameters.cpp:117), the other parsers (:142-189, :283-288) and the JUCE 9.0.1 paths: juce_Slider.cpp:445-455 (textChanged), juce_Label.cpp:493-511 (Return commits whenever the text differs) and juce_CharacterFunctions.h:223-262 (only ASCII '-'/'+' are signs, a non-digit gives 0, 'nan' gives NaN). Viewed the G-06/E05 screenshots. Reproduced on :156 with stepped motion: Ceiling 'abc'+Return gives 0.00 dB (verify-26/02-abc-return-crop.png). Starting from -1.00: empty gives 0.00, '-2,5' gives -2.00, '-0,5' gives 0.00, '-3 dBTP' gives -3.00, '- 3' gives -3.00. The Undo button restored the previous value after the 'abc' commit. Checked e769f33:tests/state_tests.cpp:5861-5963: only valid shorthand is tested.

**Corrections to the candidate claim.** Confirmed as stated, with two refinements. (1) The typographic minus (U+2212) could not be delivered through xdotool here. That '−3' parses to 0 is code-confirmed (readDoubleValue treats only ASCII '-' as a sign, juce_CharacterFunctions.h:239-250) but not runtime-reproduced. (2) The entry is not undo-less: JUCE commits it inside a ScopedDragNotification, so one Undo recovers it, and the new value is visible in the readout. Separately, while verifying I found that the text 'nan' is not turned into 0 but writes NaN into the parameter, which mutes the output when entered on the Ceiling. That shares this root cause and fix but is reported as a separate new finding because its severity class differs. The Character/Tone anchor is :283-288.

</details>

<sub>Verifier scores (1-5): impact 4 · frequency 3 · severity 4 · discoverability 3 · efficiency 2 · coherence 4 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### INPUT-002

**The Input Gain and SC HPF faders jump to wherever they are pressed, from any mouse button, across a hit area about 113x48 px, on a ~73 px track: one stray click can put up to +17.6 dB into the chain**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Interaction model | Pointer and text entry run on untuned JUCE defaults, so ordinary slips rewrite values | Phase 3 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:574-578 — `s->setSliderStyle (juce::Slider::LinearHorizontal); s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 14);` for inputGainK and scHpfK
- e769f33:src/gui/PluginEditor.cpp:1643-1658 — utility row height kUtilityH=64 reduced by 4 px; each fader cell is 175 px wide including the 62-px readout, with the caption taking the bottom 12 px, so the slider component is about 113x48 px
- e769f33:src/gui/PluginEditor.cpp:1646-1650 — design intent: 'two parameters set by ear at session start … full-width track instead'
- e769f33:src/gui/LookAndFeel.cpp:240-250 — track inset by the 8-px thumb radius so the thumb 'maps 1:1 to the cursor … never lags the cursor (#5)', an established preference for absolute tracking
- e769f33:src/PluginParameters.cpp:314-315 — inputGain range -12…+24 dB (36 dB); scHpfFreq 20–300 Hz, log
- JUCE 9.0.1 (pinned e18f7f5, e769f33:CMakeLists.txt:83) juce_Slider.cpp:862-899 — mouseDown has no button filter: menuEnabled is false (:1347), so any button takes the drag branch and calls mouseDrag(e) at once. :773-808 — the snap branch sets newPos = (mousePos − sliderRegionStart)/sliderRegionSize. :1351 — snapsToMousePos = true.
- Runtime G-10 / LAY-13(f): session capture `rt/gestures/22-sheet.png`, session capture `rt/gestures/22b-inputgain-after-drag.png`, session capture `rt/layout/04c-utility.png`, session capture `rt/layout/32-utility-1x.png`
- Runtime verify-10 (stepped motion, :140, Advanced view, Input Gain from 0.0 dB each time): left click x=140 gave 17.6 dB; x=85 gave -9.5 dB; x=150 gave 22.5 dB; on the thumb (x=104) -0.2 dB; RIGHT click x=140 gave 17.6 dB; MIDDLE click x=140 gave 17.6 dB; left click x=140 at y=543/548/552/570/575/580 gave 17.6 dB each time; SC HPF click x=320 moved 20 to 154 Hz; Undo after the jump restored 0.0 dB. Screenshots: session capture `rt/verify-10/02-sheet.png` (before, left-click and right-click results), session capture `rt/verify-10/01-adv-crop.png`

**Current behaviour.** A press anywhere in the fader's ~113x48 px area, with any mouse button, immediately moves the value to the pointer's x position. The press opens a host gesture and adds an undo step. Dragging maps 1:1 at about 0.49 dB/px (Input Gain) across a ~73-px region. The only finer routes are the hidden double-click entry and the undocumented Ctrl velocity mode, which stalls on slow drags ([INPUT-007](findings-input.md#input-007)).

**Problem.** A trim that the code comment says is set once, by ear, at session start can be changed by a large amount by one stray, misaimed or right-button click. That includes a click in the empty strip above or below the thin track. The change goes straight into the compressor, clipper and limiter. Hand-setting better than about 0.5 dB is impractical.

**Root cause.** When R2 item 2 turned the strip knobs into faders, JUCE's linear-slider default snapsToMousePos=true was kept, with no motion threshold and no button filter, and the slider component spans the whole cell height. The LookAndFeel inset (#4/#5) was designed for absolute tracking and makes it precise. It does not make a stray press safe.

**User impact.** The value jumps visibly, and the limiter slams audibly: +17.6 dB into the chain; the Ceiling still holds. It can be undone once noticed. If the user does not notice, for example with MATCH on (the level change is compensated) or mid-session, a hand-set trim is silently lost. Precise trims need the hidden double-click entry. *Scope:* Two parameters (inputGain, scHpfFreq), Advanced view only. The same JUCE defaults apply to Anamorph's linear sliders (Anamorph:src/PluginEditor.cpp:486, 611), so this is family-wide behaviour.

**Proposed improvement.** Target: pressing a fader never changes its value by itself. Only deliberate movement does. Keep the 1:1 thumb tracking the owner asked for (#5) once a drag is under way. Concretely, in a small Knob override for the linear style: (1) only the primary button starts a drag (shared with [INPUT-013](findings-input.md#input-013)). (2) A press on the thumb grabs it relatively: no jump, then 1:1 tracking. (3) A press off the thumb changes nothing until the pointer moves at least 3 px (the sibling's own click-safety threshold, Anamorph:docs/user/USER_MANUAL.md:355). Then either jump-and-track, or ignore off-thumb presses altogether; the owner decides at the fine review. (4) The fine modifier from [INPUT-007](findings-input.md#input-007) applies (≥10x finer), and double-click and Alt-click reset stay as they are. Presentation only: no parameter, range or ID change.

**Alternatives considered.**

- *setSliderSnapsToMousePosition(false) on both faders (one line)* — Removes the jump, but makes every drag relative at 250 px full extent, so the thumb moves at ~0.3x pointer speed. That brings back the 'thumb lags the cursor' behaviour recorded as feedback #5 (LookAndFeel.cpp:245). Acceptable only if the owner revisits #5.
- *Widen the track* — Improves resolution. The utility row's 360/300-px budget is committed to other controls, and a wider track does not make a stray press safe.
- *Revert to small rotary knobs* — Undoes R2 item 2, an owner-directed change. Rejected.
- *Leave as-is* — Family-consistent with Anamorph, but leaves a large-magnitude input trap on a mastering trim.

**Decision: Modify · P2.** Confirmed with realistic motion, and the right and middle buttons have the same effect. The obvious one-line fix (relative drag) conflicts with the recorded 1:1-tracking preference (#5). The constrained change (press-safety threshold or thumb-only grab, primary-button filter, fine modifier) removes the trap and keeps the owner's tracking feel. P2, not P1: the faders are Advanced-only, set rarely, and the jump is visible, audible and undoable.

**Dependencies.** [INPUT-013](findings-input.md#input-013) (a primary-button-only filter covers the right- and middle-click jumps); [INPUT-007](findings-input.md#input-007) (the fine modifier supplies precise fader setting); [UX-016](findings-ux.md#ux-016) (document fader behaviour in USER_MANUAL §3)

**Acceptance criteria.**

- From Input Gain 0.0 dB, a left press-and-release at any off-thumb point of the fader area (e.g. x=140, y=545…580 at M scale), without ≥3 px of movement, leaves the value at 0.0 dB, and no undo step is pushed.
- A right or middle press anywhere on either fader leaves the value unchanged.
- A drag that starts on the thumb moves it 1:1 with the pointer (thumb centre within 1 px of the pointer x during the drag), preserving #5.
- With the fine modifier held, a 40-px drag changes Input Gain by ≤2 dB (≥10x finer than the ~0.49 dB/px default).
- Double-click and Alt-click still reset Input Gain to 0.0 dB and SC HPF to 20 Hz as one undo step.
- A state_tests case sends a synthetic mouseDown/mouseUp at an off-thumb position and asserts the value and the undo depth are unchanged.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/PluginEditor.cpp:574-578 (LinearHorizontal, TextBoxRight 62x14), :1643-1658 (utility row and 175-px cells), e769f33:src/gui/LookAndFeel.cpp:240-250 (getSliderLayout insets the track 8 px each side 'so the thumb maps 1:1 to the cursor … never lags the cursor (#5)'), and JUCE 9.0.1 juce_Slider.cpp:852-899 (mouseDown calls mouseDrag(e) at once) and :765-808 (handleAbsoluteDrag uses the pointer position when snapsToMousePos is set; default true at :1351). Grep confirms setSliderSnapsToMousePosition is never called. Reproduced on :140 with stepped approach motion.

**Corrections to the candidate claim.** The claim holds, and the runtime evidence widens it. (a) The jump is not limited to left clicks: a right-click and a middle-click at the same x also jumped Input Gain 0.0 to +17.6 dB. G-05's 'right-click leaves the value unchanged' held only because it pressed the thumb. (b) The hit area is the whole slider component, not the drawn 4-px track. Clicks at y=543 to 580 (about 18 px above to 19 px below the track line) all snapped. (c) The mapping is about 0.49 dB/px over a ~73-px interactive region: x=85 gave -9.5 dB, x=140 gave +17.6 dB, x=150 gave +22.5 dB. The observer's 0.44 dB/px came from a drag. (d) The jump is one undoable step: Undo restored 0.0 dB.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 2 · severity 3 · discoverability 3 · efficiency 2 · coherence 3 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### INPUT-003

**Keyboard focus is accepted but never drawn, and Tab follows JUCE's screen-position order, which in Advanced zig-zags across all four panels row by row**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Accessibility/input | Keyboard and assistive-technology operation is half-implemented | Phase 3 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:1154-1169 — setWantsKeyboardFocus (true) on every rotary knob, with the comment 'Accepting focus is all that is added'
- e769f33:src/gui/PluginEditor.cpp:1213, 1227, 1275, 1289 — the same call for combos and toggles
- e769f33:src/gui/LookAndFeel.cpp:723 — the only hasKeyboardFocus read, for 'glow' TextEditors (the Save field). drawRotarySlider (:24), drawLinearSlider (:140), drawToggleButton (:252-316), drawButtonBackground (:318) and drawComboBox (:637-686) read no focus state
- JUCE@e18f7f5:modules/juce_gui_basics/detail/juce_FocusHelpers.h:62-81 — visible children are sorted by (explicit order, always-on-top, Y, X). No setExplicitFocusOrder exists in src/
- e769f33:docs/DEVELOPMENT_BRIEF.md:171 (keyboard operability); e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:88 (unchecked); e769f33:docs/HANDOVER.md:1376-1378 ('focus order audit — DONE 0.1.1')
- Runtime (verify-28, :158, rt/verify-28/taborder-adv.txt): from COMP Ratio, Tab visits Threshold → Clip Shape → Clip Drive → Limiter Gain → Ceiling → EQ Tilt → LS Freq → LS Gain → Bell 1 Q → Bell 1 Freq → Bell 1 Gain → COMP Attack → COMP Release → Clip Mix
- Runtime (verify-28): moving focus Clip Mix → Color Depth changed 0 pixels in the editor above y=540 (the ImageChops bbox is None), and Up then moved colourDepth 0 → 1 %. Screenshot session capture `rt/verify-28/04-focus-compare-clipmix-vs-depth.png`
- G-08 / E08: a plain click on a knob gives it focus and Up then changes it. The Tone macro nudge also moved colourTone and eqTilt. Screenshots session capture `rt/gestures/16d-after-tab.png`, [capture](captures/12-keyboard-focus-invisible.png), session capture `rt/edges/tab-knobrow-00-04.png`
- Runtime (verify-28): with Settings open, Tab + Down reached Tone and Ceiling behind the overlay (rt/verify-28/settings-tab-probe.txt, 21c.png). Detail is carried in [INPUT-004](findings-input.md#input-004)
- *Merged at triage from another verifier's note, quoted as written:* Add activation keys to [INPUT-003](findings-input.md#input-003)'s keyboard-operability scope. Focused buttons and toggles activate only on Return, not Space. Re-checked JUCE@e18f7f5 juce_Button.cpp:665-674: keyPressed handles returnKey only. Runtime: verify-28 41s.png vs 44-return-on-settings-button.png. Space is the platform convention for buttons and toggles. Define the activation keys (for example, a Space-aware keyPressed on the editor's buttons and toggles) and document them in the manual keyboard section [INPUT-003](findings-input.md#input-003) item 4 already adds. Severity is P3-level.

**Current behaviour.** Every knob, fader, combo and toggle accepts keyboard focus. A plain mouse click on a knob also gives it focus. Tab and Shift+Tab walk JUCE's default screen-position order. In Simple: top bar, then Loudness, Character, Tone, Ceiling, then the toggles. In Advanced the order is row-major across the four panels (COMP row 1 → CLIP row 1 → LIMITER row 1 → EQ row 1 → EQ row 2 → COMP row 2 …). No control draws any focus state: a focus move is pixel-identical. Arrow keys then change the focused control, and on a macro knob they also change the mapped Advanced parameters.

**Problem.** The brief §8 keyboard workflow cannot be used as delivered. The user cannot see which control will respond, and the traversal jumps between unrelated stages, so tabbing through one module is impossible. Because a mouse click also arms keyboard focus, arrow keys pressed later go to the last-clicked knob unseen, including the Ceiling and the macros, whenever the host delivers keys to the plugin.

**Root cause.** The 0.1.1 work added focus acceptance only (cpp:1154-1169). No LookAndFeel draw path reads focus, and no traversal order or per-panel focus container was defined. All controls are siblings under the editor, so JUCE's Y-then-X sort spans the whole window. The HANDOVER marks the audit DONE, so the gap was not tracked.

**User impact.** Keyboard-only and accessibility users cannot operate the plugin in practice. Mouse users can change a knob unknowingly with arrow keys after clicking it, in hosts that route keys to the plugin. Recovery is by undo, but only if the change is noticed. *Scope:* Every focusable control in both views (about 60 in Advanced, about 15 in Simple, plus the top bar), and the overlays ([INPUT-004](findings-input.md#input-004)).

**Proposed improvement.** (1) A restrained brand-accent focus indicator on every focusable widget type (knob ring or halo, fader thumb, combo outline, toggle pill, text button) whenever the control holds keyboard focus and the editor window is focused. It should reach at least 3:1 contrast against the panel. (2) An explicit traversal table. Simple: top bar → Loudness → Character → Tone → Ceiling → TP → LOCK → MATCH → DELTA → FREEZE → LEARN → GR/SPEC. Advanced, panel by panel: COMP (Detector, Ratio, Threshold, Attack, Release, Knee, Mix, Stereo Link, AUTO) → CLIP/COLOR → LIMITER → EQ → utility row → graph chips. Implement it with setExplicitFocusOrder from one table, or with per-panel focus-container components. (3) A state-suite test asserting the traversal order through FocusTraverser::getAllComponents for both views. (4) Doc sync: correct HANDOVER.md:1376-1378, add a keyboard section to the manual, and route checklist D through the Level-5 review.

**Alternatives considered.**

- *Leave as-is* — Rejected. Brief §8 stays unmet, and hidden arrow nudges remain possible.
- *Revert 0.1.1 focusability* — Removes the hidden-nudge trap but abandons the brief's keyboard requirement. Rejected.
- *setMouseClickGrabsKeyboardFocus(false) on knobs, so only Tab gives focus* — A good complement: a mouse click can no longer arm an invisible arrow target, while Tab traversal keeps working. It loses the click-then-arrow habit. This is an owner interaction decision.
- *Indicator only for Tab-arrived focus (focus-visible semantics)* — Cleaner for mouse users, but leaves click-armed focus invisible unless combined with the previous option.

**Decision: Modify · P2.** The claim was reproduced and code-confirmed, and extended with the Advanced cross-panel order. The fix is editor-only (painting plus focus order) and touches no hard-stop category. The new visual needs the brand checklist's Level-5 human review. The verifier rated it P1 (calibration lowered it to P2, below): for the brief's keyboard and accessibility workflow the failure occurs every session, and the invisible click-armed focus is a silent-change trap on the Ceiling and the macros.

*Calibration:* the verifier judged Proceed / P1; the final judgement is Modify / P2. Challenge accepted: mouse-user nudges are small and undoable, and keyboard-only operation is an unmet brief §8 requirement rather than an every-session path, so P2. Modify, split: (1) a distinct non-accent focus indicator shown only while the peer is focused (gold is ⊕ pending ratification and already means value/on), a KeyboardFocusTraverser-based test (not FocusTraverser), doc sync; (2) Advanced panel-wise order via plain grouping components or an explicit order table that also covers the always-on-top overlays, sequenced with [INPUT-004](findings-input.md#input-004). The Simple order already matches the brief: pin it. Merge note: focused buttons and toggles activate on Return only, not Space (juce_Button.cpp:665-674; rt/verify-28): define Space-aware activation and document it, as a P3-level sub-item. Cover the stray-arrow combo mode switch explicitly.

*Adversarial challenge:* evidence holds: yes; priority justified: no (suggested P2); decision justified: no (suggested Modify). The defect is real and code-confirmed, but the priority rests on two overstatements. The mouse-user trap produces 0.01 dB Ceiling steps and 1 % macro steps, reached only when the host routes keys to the plugin, and undoable. That is not a 'frequent trap with costly recovery'. Keyboard-only operation is an unmet brief §8 requirement (checklist D, still unchecked) but not an every-session workflow for this product's users; the judge's own frequency score of 3 contradicts 'every session'. P2 fits: a meaningful clarity and accessibility fix in a less frequent, low-cost situation. The combo single-step mode switch deserves explicit coverage and is closer to the P1 bar than the knob nudges, but it has not been runtime-verified.

Modify, not Proceed: split the work. First, a distinct, non-accent focus indicator on the JUCE widget types, shown only while the peer is focused, plus a KeyboardFocusTraverser test for visibility and hidden-control exclusion, plus the doc sync. Second, the Advanced panel-wise order, made either with plain grouping components or with an explicit order table that also covers the overlays. Keep it sequenced with [INPUT-004](findings-input.md#input-004) so overlay traversal does not regress. setMouseClickGrabsKeyboardFocus(false) on knobs and combos stays an owner interaction call. The Simple order already matches the brief, so pin it with a test and change nothing. *Proposal risks:* (1) setExplicitFocusOrder applied to the main controls from one table conflicts with the overlays. The Settings, Save and About controls are children of always-on-top Backdrops (settingsBackdrop.addAndMakeVisible at PluginEditor.cpp:743-881, savePresetBackdrop at :905-965). Components with an explicit order sort before always-on-top ones, so with Settings open the ~60 ordered main controls would come first in the cycle, and Tab from the last Settings control would exit to controls behind the overlay. That makes [INPUT-004](findings-input.md#input-004) worse. Either the overlays become KeyboardFocusContainers or they get table entries, and an acceptance criterion should cover traversal while an overlay is open. (2) Per-panel 'focus-container components': with FocusContainerType::keyboardFocusContainer, KeyboardFocusTraverser navigates only within the nearest container (juce_KeyboardFocusTraverser.cpp:45-58), so Tab would cycle inside COMP forever. Grouping needs plain components or accessibility-only focusContainer. The panels are painted, not components, so this route is also a layout refactor, not a paint change. (3) The proposed test names FocusTraverser::getAllComponents. That call returns every visible child, labels and backdrops included, with no wantsKeyboardFocus filter (juce_FocusTraverser.cpp:84-93). The test must use KeyboardFocusTraverser::getAllComponents or createKeyboardFocusTraverser (juce_KeyboardFocusTraverser.cpp:83-100), or it asserts the wrong list. (4) A 'brand-accent' ring: the gold/amber accent is ⊕ pending owner ratification (BRAND_CONSISTENCY_CHECKLIST.md:18), and the value arcs (LookAndFeel.cpp arcLo 0xffe07830 / arcHi 0xfff0b432) and ON toggles are already gold. A gold ring would read as value or on-state. 3:1 against the panel is not enough; the indicator must also be distinct from the adjacent arc, and it must not depend on the UI Animation toggle. (5) Threading: a hasKeyboardFocus or peer-focus read in the paint path is the same class as the existing isMouseOver reads. JUCE paints components on the GL thread under the MessageManager lock (juce_OpenGLContext.cpp:389-405), so no THREAD_MODEL gate applies. A focus-visible variant must keep its 'arrived by Tab' flag as a component property (as with hovA/dragging), not as editor bookkeeping read from paint (ADR-0027 clause 4). (6) Repaint needs no new code: Slider::focusOfChildComponentChanged (juce_Slider.cpp:1730) and Button/ComboBox focusGained/focusLost already repaint. The Simple table's 'GR/SPEC' entry and the top bar's A/B, however, are custom components that are not focusable (ABControl, PluginEditor.h:~190). Including them is new keyboard behaviour ([INPUT-005](findings-input.md#input-005)/006 scope), not ordering. (7) hasKeyboardFocus stays true after the host window takes OS focus, so the 'editor window focused' clause needs an explicit peer-focus check. No hard-stop gate is crossed: the change is editor-only and touches no parameter, state, latency or macro contract.

**Dependencies.** [INPUT-004](findings-input.md#input-004); [INPUT-005](findings-input.md#input-005); [INPUT-006](findings-input.md#input-006)

**Acceptance criteria.**

- After each Tab or Shift+Tab, a screenshot diff shows a focus indicator on exactly one control, at that control's bounds, with at least 3:1 contrast against the panel background.
- In Advanced, starting from the COMP mode combo, repeated Tab visits every COMP control before any CLIP/COLOR control, then LIMITER, EQ and the utility row, in the documented table order. Shift+Tab reverses it.
- In Simple, Tab order is top bar → Loudness → Character → Tone → Ceiling → TP → LOCK → MATCH → DELTA → FREEZE → LEARN.
- A state-suite test asserts the traversal order for both views and fails if a control is added without a table entry.
- Controls hidden in the current view never receive focus (a regression check for the existing behaviour).
- HANDOVER.md no longer claims a completed focus-order audit that did not happen, and USER_MANUAL documents keyboard operation.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/PluginEditor.cpp:1154-1169, 1213, 1227, 1275, 1289; grep of src/ for keyPressed, setExplicitFocusOrder, FocusTraverser and hasKeyboardFocus (only LookAndFeel.cpp:723 and the save field). Read the LookAndFeel draw overrides (LookAndFeel.cpp:24, 140, 252-316, 318, 637-686, 752): none reads focus. Read JUCE@e18f7f5 detail/juce_FocusHelpers.h:48-81. Viewed session capture `rt/gestures/16d-after-tab.png` and [capture](captures/12-keyboard-focus-invisible.png). Reproduced on :158 with stepped pointer motion: (1) in Advanced, clicked COMP Ratio, then probed Tab + Up/Down + dump 14 times (rt/verify-28/taborder-adv.txt); (2) moved focus Clip Mix → Color Depth by Tab with the pointer parked outside and diffed the editor above y=540 (session capture `rt/verify-28/04-focus-compare-clipmix-vs-depth.png`).

**Corrections to the candidate claim.** 'No defined focus order' is precise in one sense: nothing in src sets an order, so JUCE's default applies. That default orders all visible editor children by explicit order, then always-on-top, then Y, then X. Because the controls are direct editor children (panels are painted, not components), Advanced traversal interleaves the panels by screen row. E08's 'first Tab landed on Tone' started from Character, so it is consistent with this order. Invisible other-mode controls are correctly skipped (isVisible filter, juce_FocusHelpers.h:62). HANDOVER.md:1376-1378 records the 'focus order audit' as DONE, but only focusability was added. That is doc drift.

</details>

<sub>Verifier scores (1-5): impact 4 · frequency 3 · severity 3 · discoverability 5 · efficiency 4 · coherence 4 · change risk 2 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### INPUT-004

**Escape cannot close Settings or About (it closes every other pop-up), Settings has no close control, and keyboard focus stays on the controls hidden underneath**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | confirmed | Accessibility/input | Keyboard and assistive-technology operation is half-implemented | Phase 3 |

**Evidence**

- e769f33:src/gui/PluginEditor.h:83-97 — Backdrop overrides mouseDown only ('Click outside (or anywhere, for About) dismisses')
- e769f33:src/gui/PluginEditor.cpp:936-937 — the only Escape mapping is saveNameEditor.onEscapeKey. No keyPressed override exists in src/gui
- e769f33:src/gui/PluginEditor.cpp:2387-2396 — showAbout and showSettings only toggle visibility: no focus transfer, no focus restoration
- e769f33:src/gui/PluginEditor.cpp:1436-1441 — the backdrops cover the whole editor, top bar included
- JUCE@e18f7f5:modules/juce_gui_basics/detail/juce_FocusHelpers.h:62-81 — traversal includes every visible editor child (the always-on-top backdrop first, then all covered controls), because the Backdrop is not a focus container
- JUCE@e18f7f5:modules/juce_gui_basics/buttons/juce_Button.cpp:665-674 — Return triggers a focused button, which is how Settings opens from the keyboard
- Runtime (verify-28): Settings stayed open after Escape, and after Escape → Return → Escape (session capture `rt/verify-28/13-settings-strip.png`, 45-after-escape-return-escape.png). About stayed open after Escape (session capture `rt/verify-28/33-about-strip.png`)
- Runtime (verify-28): with Settings open, Tab ×4 + Down changed tone, colourTone and eqTilt, and Tab ×5 + Down changed ceiling (rt/verify-28/settings-tab-probe.txt). Down ×20 set Tone to −0.40 under the open panel (session capture `rt/verify-28/21c.png`)
- E02 / G-01 / LAY-18: Escape does close combo lists, the preset menu, Advanced combos, the Save panel and the inline editor. Screenshots session capture `rt/edges/05-settings-open.png`, session capture `rt/edges/06-settings-after-escape.png`, session capture `rt/edges/06b-settings-after-2nd-escape.png`, session capture `rt/edges/10-settings-combo-after-escape.png`
- ST-11: Settings has no close button

**Current behaviour.** About (click on the wordmark) and Settings (click, or Return on the focused Settings button) open backdrops that cover the whole editor, top bar included. Escape leaves both open, while it closes every other pop-up (combo lists, the preset menu, the Save panel, the inline value editor). Settings has no close or Done control: only a click outside the panel dismisses it (About: a click anywhere). While Settings is open, focus stays where it was. Tab walks the covered editor controls, and arrow keys change them unseen: Tone, with the Tone macro moving Color Tone and Tilt, and the Ceiling.

**Problem.** Keyboard dismissal is inconsistent across the product's pop-ups. A keyboard-only user can open Settings but cannot close it. Worse, keys pressed while the modal panel is up change sound parameters hidden behind it, including the delivery Ceiling. Arrow keys pressed with Settings open appear to act on the visible dialog but change the hidden sound parameter that has focus.

**Root cause.** The mouse-only Backdrop pattern was inherited from Anamorph, which also maps Escape only on the save field (Anamorph@fd78c3b:src/PluginEditor.cpp:383). The editor has no key handler. Opening an overlay does not move keyboard focus into it, and the Backdrop is not a keyboard focus container, so JUCE's traversal still includes every visible control underneath.

**User impact.** Keyboard-only users are trapped in Settings or About. Any keyboard user can silently alter Tone, the macros or the Ceiling while a modal panel hides them. Mouse users lose the expected Escape shortcut. *Scope:* The Settings and About overlays, both views. The Save panel already handles Escape and grabs focus, but its traversal is also not confined.

**Proposed improvement.** (1) Escape dismisses any visible Backdrop (About, Settings, Save), via Backdrop::keyPressed or an editor-level handler, and returns keyboard focus to the control that opened it (Settings button, wordmark, preset name). (2) On open, focus moves to the first control in the panel (Settings: the Oversampling combo; About: the link). The Backdrop becomes a keyboard focus container, so Tab and Shift+Tab cycle only inside the panel and no key reaches the covered controls. (3) Settings gets a visible close (×) or Done control, reachable by Tab and styled in the family grammar. (4) USER_MANUAL §3.1 documents Escape.

**Alternatives considered.**

- *Leave as-is (click-outside only)* — Rejected. It keeps the keyboard trap and the hidden-parameter changes.
- *Escape only, without focus containment* — Removes the trap but still lets Tab and arrow keys change covered controls. Insufficient.
- *Make the overlays real modal components (enterModalState)* — This would contain input, but interacts with the PopupShield click-consumption design (PluginEditor.h:105-190) and with host modal handling. A heavier change than needed.

**Decision: Proceed · P2.** The claim was reproduced and code-confirmed, and extended: focus is not contained, so arrow keys change hidden parameters while Settings is open. The fix is local to the editor's overlay handling and touches no hard-stop category or ADR. Priority is P2: keyboard use of these overlays is less frequent than the main workflow, but the trap and the hidden changes are real.

**Dependencies.** [INPUT-003](findings-input.md#input-003)

**Acceptance criteria.**

- With Settings, About or Save open, a single Escape closes it, and keyboard focus returns to the control that opened it.
- Opening Settings by mouse or by Return puts keyboard focus on the first Settings control.
- With Settings open, Tab ×N followed by Up/Down/Return (any N up to 30) changes no APVTS parameter in a harness 'dump'. Tab cycles only through the Settings controls and the close control.
- Settings shows a close or Done control that is reachable by Tab and closes the panel with Return.
- The existing outside-click dismissal and PopupShield click consumption behave exactly as before (the E03 re-test passes).
- USER_MANUAL documents Escape for the overlays.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/PluginEditor.h:83-97 (Backdrop), e769f33:src/gui/PluginEditor.cpp:315, 368, 676-703, 936-937, 1430-1441, 2387-2396; grep of src/gui for keyPressed and onEscapeKey. Read JUCE@e18f7f5 juce_Button.cpp:665-674 and juce_FocusHelpers.h:62-81. Viewed session capture `rt/edges/06b-settings-after-2nd-escape.png`. Reproduced on :158 with stepped motion: opened Settings (click), pressed Escape (session capture `rt/verify-28/13-settings-strip.png`); opened About, pressed Escape (33-about-strip.png); opened Settings with Return on the focused Settings button, then Escape/Return/Escape (44-return-on-settings-button.png, 45-after-escape-return-escape.png); with Settings open, probed Tab + Down + dump (settings-tab-probe.txt) and pressed Down ×20 after Tab ×5 (21c.png).

**Corrections to the candidate claim.** The original claim holds and understates the problem. While Settings is open, keyboard focus is neither moved into nor confined to the panel. Tab continues through the covered editor controls, and arrow keys change them: Tone went to −0.40 and, through the macro, moved Color Tone and Tilt, and the Ceiling moved, all behind the panel, with the preset label turning 'Default *'. Keyboard-only users can open Settings: Return triggers the focused Settings button (JUCE Buttons respond to Return, not Space). About is opened by the wordmark 'ghost' button.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 2 · severity 3 · discoverability 3 · efficiency 3 · coherence 4 · change risk 1 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### INPUT-005

**Keyboard delivery to the editor may vary by host (EDITOR_WANTS_KEYBOARD_FOCUS FALSE, single Save-field focus grab), but the product's own record shows macOS typing works; only the host matrix is untested**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Investigate further** | **P3** | medium | partially-confirmed | Accessibility/input | Keyboard and assistive-technology operation is half-implemented | Phase 0 |

**Evidence**

- e769f33:CMakeLists.txt:304 — EDITOR_WANTS_KEYBOARD_FOCUS FALSE. ADR-0008 lists it among the supporting juce_add_plugin flags (e769f33:docs/architecture/design-decisions/ADR-0008-build-architecture-and-plugin-identity.md:294-296)
- JUCE@e18f7f5:modules/juce_audio_plugin_client/detail/juce_VSTWindowUtilities.h:56-58 (macOS VST: windowIgnoresKeyPresses when the flag is 0); juce_audio_plugin_client_AU_1.mm:1703-1707 (AU holder refuses focus); juce_gui_basics/native/juce_NSViewComponentPeer_mac.mm:2620-2623, 1194-1196 (acceptsFirstResponder follows canBecomeKeyWindow)
- e769f33:src/gui/PluginEditor.cpp:2365-2385 — showSavePreset makes one grabKeyboardFocus, justified by menu-callback ordering
- Anamorph@fd78c3b:src/PluginEditor.cpp:2068-2103 — bounded retry focusSaveNameField(4). Anamorph e769f33:docs/KNOWN_ISSUES.md:260-310 — REAPER KI-009 persists with the retry
- e769f33:docs/KNOWN_ISSUES.md:943-952 — KI-014: macOS, all formats: 'Typing normally is unaffected, so the field is fully usable' (only auto-repeat of letters and digits is missing)
- e769f33:src/gui/PluginEditor.cpp:2179-2187 — the preset menu is parented to the editor (withParentComponent)
- ST-04 — Save field typing works in the Linux harness. verify-28 — arrow keys and Return reach knobs and buttons in the Linux harness

**Current behaviour.** The plugin never requests keyboard focus from the host. Focusable controls receive keys only when the host has given the editor window focus; in the Standalone that is always. The Save Preset field grabs focus once when opened. On Linux (harness), typing, arrow keys, Return and Escape all reach the editor. On macOS the product's own KI-014 reports that the Save field types normally in all formats. There is no recorded result for Windows hosts, for REAPER (where the sibling has a documented focus loss), or for knob arrow keys and double-click value entry in any real host.

**Problem.** The reach of keyboard operation (brief §8), and of Space inside the Save field versus the host transport, is unverified outside Linux and one macOS report. It is not shown to be broken. The only documented failure class (the sibling's REAPER KI-009) is host-specific and was not fixed by the retry this finding asks to port.

**Root cause.** There is no host-matrix testing for keyboard paths (the KI-004-class coverage gap in the sibling, and the same gap here). JUCE's key routing for this flag differs per wrapper and platform. The sibling's retry was not ported, on an ordering argument the sibling later judged incomplete.

**User impact.** Unknown beyond the evidence. If some host withholds keys, value typing, arrow stepping and Save naming fail there, and Space may start the transport. Current evidence shows typing working on Linux and on macOS (KI-014). *Scope:* All keyboard paths (value boxes, knobs, Save field, overlays) × formats (VST3, AU, Standalone) × platforms and hosts.

**Proposed improvement.** Run a documented keyboard host-matrix pass before changing code. Cover macOS Logic (AU) plus one VST3 host (Live or REAPER), Windows REAPER and Cubase or Live (VST3), and Linux REAPER (VST3). In each, record: (1) click a knob, then Up changes it; (2) double-click a value box, type '-1', Return commits; (3) Save Preset… receives letters and Space; (4) Escape cancels the inline editor. Store the results in COMPATIBILITY_MATRIX. If (3) fails in any host, port the sibling's bounded retry with provenance (ADR-0009) and update the comment at cpp:2374-2382 to the sibling's corrected rationale. If keys do not reach the editor at all in a host, record a KNOWN_ISSUES entry and raise the EDITOR_WANTS_KEYBOARD_FOCUS question at the Architecture Review Gate.

**Alternatives considered.**

- *Port the sibling's 4×50 ms retry now (defensive parity)* — Cheap and needs no gate, but there is no Anabasis evidence it is needed, and it did not fix the sibling's only documented focus failure (REAPER). Acceptable as a follow-up once the matrix shows a failure.
- *Set EDITOR_WANTS_KEYBOARD_FOCUS TRUE* — A Build System change (gated) that also alters a flag recorded in Accepted ADR-0008. It risks taking host transport keys, the reason the flag is FALSE. Not justified by current evidence.
- *Accept as-is and document 'keyboard support depends on the host'* — Possible outcome after the matrix, but premature without data.

**Decision: Investigate further · P3.** The code-level facts are true, but the user-impact hypothesis is contradicted for macOS by the product's own KI-014. The proposed remedy (the retry) did not cure the sibling's documented host failure. Deciding whether any change is needed requires host-matrix evidence (listed in proposed_improvement) that this environment cannot produce.

**Architecture gates.**

- Only if the matrix leads to flipping EDITOR_WANTS_KEYBOARD_FOCUS: Build System change (e769f33:CMakeLists.txt:304), and a change to a flag recorded in Accepted ADR-0008 (supporting juce_add_plugin flags). The investigation and the retry port touch no gate.

**Dependencies.** None.

**Acceptance criteria.**

- COMPATIBILITY_MATRIX records, for each listed host/format/platform, pass or fail for knob arrow stepping, value-box typing plus Return, Save-field letters and Space, and inline-editor Escape.
- If any Save-field failure is recorded, the ported retry makes that host pass and does not regress the Linux harness (ST-04 re-test).
- If any host withholds keys entirely, a KNOWN_ISSUES entry and a USER_MANUAL note exist, and any flag change goes through the Architecture Review Gate with an ADR-0008 amendment.

<details><summary>Verification record</summary>

**Method.** Read e769f33:CMakeLists.txt:304, e769f33:src/gui/PluginEditor.cpp:1154-1168, 2365-2385, 2173-2187. Compared with Anamorph@fd78c3b:src/PluginEditor.cpp:2053-2103, Anamorph e769f33:docs/KNOWN_ISSUES.md:260-310 (KI-009) and CHANGELOG.md:863-868, 906-915. Grepped JUCE@e18f7f5 for JucePlugin_EditorRequiresKeyboardFocus (VSTWindowUtilities.h:56-58, AU_1.mm:1703-1707, VST2.cpp:1860) and read NSViewComponentPeer_mac.mm:1583-1601, 1194-1196 and 2620-2623. Found the product's own KI-014 at e769f33:docs/KNOWN_ISSUES.md:943-952. Runtime covers only Linux (ST-04, and verify-28 key tests on :158). macOS and Windows hosts are not reachable here.

**Corrections to the candidate claim.** The code facts hold: the flag is FALSE; in the pinned JUCE it acts only in the macOS VST2/VST3 window utilities (windowIgnoresKeyPresses) and the AU editor holder, with no effect on Windows or Linux; and the Save field makes a single grab where the sibling retries 4×50 ms. However, the claim that the Save field and key input 'may all be dead' in macOS hosts is contradicted by the product's own KI-014 (2026-08-13, macOS, all formats): 'Typing normally is unaffected, so the field is fully usable'. So keys do reach the editor on macOS, at least in the reporter's host(s); the mechanism was not established here. The sibling's retry did not cure its REAPER KI-009 either, and the Anabasis preset menu is parented to the editor (cpp:2179-2187), which removes the separate-window cause the sibling's retry was written for. Any benefit of porting the retry to Anabasis is therefore unproven. The Anabasis comment's ordering argument (cpp:2374-2382) differs from the sibling's corrected account, which says what matters is whether the host has focused the plugin's window (Anamorph@fd78c3b:src/PluginEditor.cpp:2068-2090).

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 2 · severity 3 · discoverability 3 · efficiency 2 · coherence 2 · change risk 2 · complexity 2 · evidence 2</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### INPUT-006

**Arrow keys step by the value interval or 1 % of the linear value range: Ceiling needs 2000 presses end to end, and log-tapered knobs jump (Lim Release 5 → 1 → 11 ms); PageUp/PageDown, Home/End and modifiers do nothing**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | confirmed | Accessibility/input | Keyboard and assistive-technology operation is half-implemented | Phase 3 |

**Evidence**

- JUCE@e18f7f5:modules/juce_gui_basics/widgets/juce_Slider.cpp:38-44 — step = interval, or range length × 0.01 when interval is 0
- JUCE@e18f7f5:modules/juce_gui_basics/widgets/juce_Slider.cpp:1029-1059 — any modifier returns false; only the arrow keys are handled; the step comes from the accessibility value range
- JUCE@e18f7f5:modules/juce_gui_basics/widgets/juce_Slider.cpp:1814-1817 — the accessibility value range uses getStepSize, so screen-reader increments are identical
- e769f33:src/PluginParameters.cpp:230-233, 303 — Ceiling twoDecimalRange (-20, 0) has interval 0.01 (ADR-0024 grid), which becomes the key step
- e769f33:src/PluginParameters.cpp:237-240, 367 — logRange has no interval, so Lim Release 1-1000 ms steps by about 10 ms of linear value
- e769f33:src/gui/PluginEditor.h:206-236 — Knob overrides mouseDown only; there is no keyPressed override in src/gui
- Runtime (verify-28 dumps in rt/verify-28/app.log): Ceiling −0.10 → −0.13 dB after Down ×3; Lim Release 5.0 → 1.0 ms [norm 0.2330 → 0.0000] → 11.0 ms [0.3470] → 21.0 ms [0.4406]; Page/End/Shift/Ctrl + arrow: no change
- G-08: Loudness 1 %, Comp Threshold 0.4 dB, Input Gain 0.36 dB, SC HPF about 3 Hz per press. E08: Character 0.01. Screenshots session capture `rt/gestures/16-sheet.png`, session capture `rt/edges/51c-character-arrow-up3-crop.png`

**Current behaviour.** Up/Right add one step and Down/Left subtract one. The step is the parameter interval where one exists (Ceiling 0.01 dB, about 2000 presses across −20…0 dB), and otherwise 1 % of the linear value range: Loudness 1 %, Comp Threshold 0.4 dB, Input Gain 0.36 dB, and about 10 ms on the 1-1000 ms log Lim Release, where a press at the low end spans a third of the knob's travel and skips the fast-release region. PageUp/PageDown, Home/End and every modifier combination do nothing. None of this is documented.

**Problem.** Keyboard adjustment is too fine where precision is least needed (the Ceiling's range) and too coarse where it matters most (the low end of log-tapered time and frequency knobs, e.g. 1-10 ms limiter release). There is no coarse or fine modifier, so a keyboard or screen-reader user cannot reach many settings efficiently, or at all (Lim Release values between 1 and 11 ms).

**Root cause.** JUCE's default Slider key handling is left untuned. Its step is defined in value space (the interval, or 1 % of max − min), not in the knob's skewed travel. The Ceiling's ADR-0024 display-precision interval therefore doubles as its key step, and log ranges get a linear step.

**User impact.** Keyboard and assistive-technology users get impractical Ceiling adjustment and cannot reach fast limiter or compressor time constants or low EQ and HPF frequencies by keys. Mouse users are unaffected. *Scope:* Every knob and fader (about 40), especially the Ceiling and the log-range parameters (SC HPF, Comp Attack/Release, Lookahead, Lim Release, EQ frequencies and Q).

**Proposed improvement.** Add a keyPressed override on Knob, with a matching AccessibilityValueInterface step, that moves in normalised travel. Arrow = 1 % of travel, so log knobs step evenly (Lim Release from 5 ms Down lands just below 5 ms, not at 1 ms). Shift+arrow = fine (0.1 % of travel). PageUp/PageDown = 10 % of travel. Where travel-percent is not musical, use per-parameter steps: Ceiling arrow 0.1 dB, Shift 0.01 dB, Page 1 dB, all on the existing 0.01 grid so ADR-0024 is untouched. Do not bind Home/End to raw extremes on the Ceiling (0.00 dB); either leave them unbound or use them for 'reset to default', as an owner decision. Keep each nudge on the existing attachment gesture path so undo and detach semantics are unchanged. Document the steps in the manual.

**Alternatives considered.**

- *Leave the JUCE default* — Rejected. It measurably blocks keyboard access to part of several ranges.
- *Coarsen the Ceiling's range interval to 0.1* — Rejected. It changes a host-visible range, which is gated as a Parameter Registry change, and conflicts with Accepted ADR-0024's two-decimal decision.
- *Set a different interval on the editor Slider only* — The attachment copies the range interval, and an editor-only interval would also quantise drag and text entry. It is fragile and does not fix the log-taper problem.
- *Accelerating key-repeat (step grows while held)* — Possible later refinement. It does not fix the log-taper problem alone.

**Decision: Proceed · P2.** The claim was reproduced and code-confirmed, with a more serious log-taper defect added. The fix is editor-side key handling that changes no range, ID or ADR-0024 grid, so no gate applies. Priority is P2: keyboard adjustment is a less frequent path, and its value depends on [INPUT-003](findings-input.md#input-003) making focus visible, but part of several ranges is unreachable by keys.

**Dependencies.** [INPUT-003](findings-input.md#input-003); [INPUT-005](findings-input.md#input-005)

**Acceptance criteria.**

- On every log-range knob, one Down press from any value moves by about 1 % of the knob's travel (normalised delta between 0.009 and 0.011): Lim Release from 5.0 ms lands between 4.5 and 5.0 ms, not at 1.0 ms.
- Ceiling: Down moves 0.1 dB, Shift+Down 0.01 dB, PageDown 1 dB, every result on the 0.01 grid, and testCeilingIsQuantisedToTwoDecimals still passes.
- PageUp/PageDown move 10 % of travel on non-Ceiling knobs, and Shift+arrow moves 0.1 %.
- The accessibility value interface reports the same step the keys use.
- Undo and detach behaviour for keyboard nudges matches mouse nudges (a state-suite check).
- USER_MANUAL lists the keyboard steps and modifiers.

<details><summary>Verification record</summary>

**Method.** Read JUCE@e18f7f5 widgets/juce_Slider.cpp:38-44 (getStepSize), 1029-1059 (keyPressed) and 1814-1817 (accessibility range). Read e769f33:src/PluginParameters.cpp:230-240, 303, 315, 322-323, 367 and e769f33:src/gui/PluginEditor.h:206-236 (Knob overrides mouseDown only). Viewed session capture `rt/gestures/16-sheet.png` and session capture `rt/edges/51c-character-arrow-up3-crop.png`. Reproduced on :158 in Advanced with stepped clicks. Ceiling: Down ×3 → −0.13 dB; then PageDown, End, Shift+Down and Ctrl+Down left it unchanged. Lim Release set to 5 ms, then Down → 1.0 ms, Up → 11.0 ms, Up → 21.0 ms. Lookahead Up 2.0 → 2.1 ms; Comp Attack Down 30 → 29 ms (verify-28 app.log dumps).

**Corrections to the candidate claim.** The claim holds. Additional finding: on log-tapered knobs the key step is linear in value, not in knob travel. On Lim Release (1-1000 ms, log) one press is about 10 ms: it jumps from 5.0 ms straight to the 1.0 ms floor, and upward to 11.0 ms, which is 0.000 → 0.347 normalised, about a third of the knob's rotation in one press. Near the top of the range a press is about 1 % of the rotation. Screen-reader increment and decrement use the same step, via the accessibility value range.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 2 · severity 2 · discoverability 4 · efficiency 4 · coherence 3 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### INPUT-007

**No usable fine-adjust modifier: Shift does nothing, the Ctrl/Cmd/Alt velocity mode stalls on slow drags (1 px per event gives zero change), Alt resets instead, and knobs ignore sideways drags; none of this is documented**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Interaction model | Pointer and text entry run on untuned JUCE defaults, so ordinary slips rewrite values | Phase 3 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:1129 — `s.setSliderStyle (juce::Slider::RotaryVerticalDrag);`; no drag-sensitivity or velocity configuration anywhere in src/gui (grep)
- e769f33:src/gui/PluginEditor.h:215-236 — `if (e.mods.isAltDown()) { … doReset(); … return; }`, deliberately without Slider::mouseDown, so 'an alt-PRESS-AND-DRAG is inert' (the comment accepts this and cites the ADR-0009 Anamorph inheritance)
- JUCE 9.0.1 juce_Slider.cpp:815-849 — handleVelocityDrag: speed = 0.2·sens·(1+sin(π(1.5+min(0.5, offset+max(0,|d|−threshold)/maxSpeed)))), zero for |d|=1 with threshold 1. :848 enableUnboundedMouseMovement (cursor hidden).
- JUCE 9.0.1 juce_Slider.cpp:1182-1185 isAbsoluteDragMode; :1326 pixelsForFullDragExtent=250; :1321 velocityModeThreshold=1; :1337 modifierToSwapModes=ctrlAltCommandModifiers
- e769f33:docs/user/USER_MANUAL.md:141-151 — no fine modifier documented
- Anamorph:src/PluginEditor.cpp:736 and Anamorph:src/PluginEditor.h:414 — the sibling has the identical RotaryVerticalDrag and Alt-reset pattern (family-wide)
- Runtime G-03 / E19: session capture `rt/gestures/10a-loud-drag-during-crop.png`, session capture `rt/gestures/11-loud-after-moddrags.png`, session capture `rt/edges/27a-drag-tone-to-ceiling-held-crop.png`
- Runtime verify-10 (:140, Loudness from 50 %, 100 px down): plain 1 px/event 10.4 %, 10 px/event 14 %; Ctrl 1 px/event 50 % (no change), 2 px 49.9 %, 5 px 49.3 %, 10 px 48.2 %, 25 px 45.8 %; Shift 1 px 10.4 %, 10 px 14 % (same as plain); Alt press plus 100-px drag up gave 0 % (reset, drag inert); horizontal ±100 px gave 50 % (no change). Values from the harness 'dump' in rt/verify-10/app.log.

**Current behaviour.** Knobs drag vertically only, at a fixed 250 px for the full range: Ceiling ≈0.08 dB/px, Comp Threshold ≈0.16 dB/px, Loudness ≈0.4 %/px. Shift has no effect. Ctrl/Cmd switches to JUCE's velocity mode, in which movement depends on pointer speed: slow movement (1 px per event) moves nothing, medium movement is fine, fast movement is coarse, and the cursor is hidden. An Alt press resets to the default and the rest of that drag is ignored. Horizontal drags do nothing. None of this appears in the manual or the tooltips.

**Problem.** The product has no predictable fine-adjust gesture. The modifier most users try first (Shift) silently runs at full speed and overshoots. The one that works (Ctrl/Cmd) behaves like a stuck control when used slowly, as fine adjustment is. Alt, a fine modifier in some products, destroys the current value. Sideways drags get no response.

**Root cause.** JUCE Slider defaults were kept unchanged: 250-px extent, velocity mode on ctrlAltCommand with threshold 1, vertical-only rotary drag. The Anamorph Knob's Alt-reset was inherited. A fine-drag convention was never specified in the brief, DESIGN or the manual.

**User impact.** Precise moves on the Ceiling, thresholds, EQ gains and the macros take repeated careful drags, typing (hidden, [UX-016](findings-ux.md#ux-016)) or arrow keys (1 % steps, invisible focus, G-08). Shift users overshoot. Ctrl users on a slow, careful drag see nothing happen. The friction recurs every session but has no audio-correctness consequence. *Scope:* Every rotary knob in both views, and the two faders (velocity mode applies to linear sliders too). Behaviour is family-wide (Anamorph identical).

**Proposed improvement.** Target: holding Shift while dragging any knob or fader moves it exactly 10x finer, at any pointer speed, with no jump when Shift is pressed or released mid-drag (re-anchor at the modifier change). Cmd (macOS) / Ctrl (Windows/Linux) gives the same fixed-ratio fine mode instead of JUCE's velocity mode, through a Knob-level mouseDrag override or by disabling the velocity swap (setVelocityModeParameters(…, userCanPressKeyToSwapMode=false, …)) plus the custom fine path. The fine path must stay inside the gesture that Slider::mouseDown opened (one undo step, same detach semantics). Keep Alt-click reset (documented, family-consistent, and the code records the inert Alt-drag as a deliberate trade-off). Keep vertical-only drag for family consistency and document it; accepting horizontal drags as well (RotaryHorizontalVerticalDrag) is left to the owner, since it doubles the speed of diagonal drags. Document the fine modifier and the drag direction in USER_MANUAL §3 ([UX-016](findings-ux.md#ux-016)).

**Alternatives considered.**

- *Only document the Ctrl/Cmd velocity mode* — No code change, but documents a mode that stalls at slow speeds and hides the cursor. It does not solve the fine-adjust problem.
- *Tune velocity mode (setVelocityModeParameters with an offset, and Shift as the swap key)* — Cheap, but velocity mode stays event-rate and speed dependent. With an offset the movement becomes per-event, so it varies with mouse polling rate. That is not a predictable fine mode.
- *Make Alt a fine modifier (move reset to double-click only)* — Breaks the documented, family-wide Alt/Option-click reset (USER_MANUAL §3, Anamorph manual §3). Rejected.
- *Leave as-is* — Family-consistent, but no reliable fine gesture exists.

**Decision: Modify · P2.** The defect is confirmed and is more serious than reported, because the only fine modifier stalls on slow drags. A full gesture redesign is not warranted: Alt-reset and vertical drag are deliberate, documented and family-wide. The constrained change adds a fixed-ratio Shift fine mode (and maps Cmd/Ctrl to it) and documents the set. Fine adjustment happens every session, but no audio or data harm results, so P2.

**Dependencies.** [UX-016](findings-ux.md#ux-016) (document the modifiers); [INPUT-013](findings-input.md#input-013) (on macOS Ctrl-click is the popup-menu click, so a left-button/non-popup filter must leave Cmd and Shift as the fine modifiers); [INPUT-002](findings-input.md#input-002) (the fine mode is what makes precise fader setting possible); Anamorph product-family note: the sibling has the identical gesture set; record the divergence or mirror the change (ADR-0009 copy-and-adapt, no conflict)

**Acceptance criteria.**

- With Shift held, a 100-px vertical drag on Loudness from 50 % moves it by 3.6–4.0 % (1/10 of the plain rate), within ±10 %, at 1, 2, 10 and 25 px per motion event alike.
- With Cmd (macOS) / Ctrl (Windows/Linux) held, the same drag gives the same result as Shift; a 1-px-per-event drag is never a zero change.
- Pressing or releasing Shift mid-drag causes no value discontinuity: the value is continuous across the modifier change, within one fine step.
- A fine drag is one host gesture and one undo step, and detaches a managed parameter exactly as a plain drag does (state_tests).
- Alt-click and double-click still reset to the default as one undo step.
- USER_MANUAL §3 documents the fine modifier, vertical-only drag and Alt-click reset.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/PluginEditor.cpp:1129 (RotaryVerticalDrag; grep: no setVelocityModeParameters or setMouseDragSensitivity in src/gui), e769f33:src/gui/PluginEditor.h:206-236 (Knob::mouseDown consumes Alt as a reset and skips Slider::mouseDown), and JUCE 9.0.1 juce_Slider.cpp:815-849 (handleVelocityDrag), :929 and :1182-1185 (mode swap), :1320-1342 (defaults: 250 px extent, velocityModeThreshold=1, ctrlAltCommandModifiers). Reproduced on :140: 100-px vertical drags on Loudness from 50 % at 1, 2, 5, 10 and 25 px per motion event with no modifier, Ctrl and Shift; an Alt press-and-drag; ±100-px horizontal drags.

**Corrections to the candidate claim.** 'Ctrl is about 20:1' is an artefact of the observer's 10-px-per-event motion. JUCE velocity mode is speed-dependent, not a fixed ratio. Per event, Δ = 0.2·(1 + sin(π(1.5 + min(0.5, (|d|−1)/200)))) of the range, so a 1-px move contributes exactly zero. Measured over 100 px: 1 px/event gave 50.0 to 50.0 % (no change); 2 px gave 49.9 %; 5 px gave 49.3 %; 10 px gave 48.2 %; 25 px gave 45.8 %. A slow, careful drag, the kind a fine adjustment is, therefore stalls or barely moves, and a fast flick is coarse. The velocity drag also hides the cursor (enableUnboundedMouseMovement(true,false), juce_Slider.cpp:848); that is from code only, since Xvfb captures do not show the cursor. Alt is part of the swap set but is taken as a reset on press, so only Ctrl/Cmd pressed before the drag, or any of the three pressed mid-drag, reaches velocity mode. On macOS Ctrl-click is also JUCE's popup-menu click (see [INPUT-013](findings-input.md#input-013)). Plain drag rate confirmed at 0.36–0.40 % of range per px (JUCE's 250-px extent). Shift is identical to plain at both 1 and 10 px/event. Alt jumped 50 to 0 % and the 100-px drag did nothing. ±100-px horizontal drags changed nothing.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 4 · severity 2 · discoverability 4 · efficiency 3 · coherence 2 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### INPUT-008

**Percent boxes read a bare '1' as 100 % but '1.5' as 1.5 %, and Character (0…1) clamps '75' to 1.00 while Loudness beside it accepts 75**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Preserve** | **none** | high | confirmed | Accessibility/input | Pointer and text entry run on untuned JUCE defaults, so ordinary slips rewrite values | — |

**Evidence**

- e769f33:src/PluginParameters.cpp:119-147 — the fraction rule (owner directive, 0.1.6 item 2): a bare value in (0,1] is multiplied by 100 and '%' makes it literal. The discontinuity at 1 is acknowledged at :132-135
- e769f33:docs/user/USER_MANUAL.md:145-149 — the rule is documented for users. :172-173 — Character is 0…1 and Tone −1…+1
- e769f33:tests/state_tests.cpp:5906-5963 — '1' gives 100 %, '1.5' gives 1.5 %, '0.1%' gives 0.1 %, and the round-trip is pinned
- e769f33:src/PluginParameters.cpp:283-288 — Character and Tone parse with plain getFloatValue and the range clamps
- G-06: session capture `rt/gestures/15-sheet.png`, session capture `rt/gestures/15i-loud-after-12.34-crop.png`
- verify-26/percent-roundtrip (display :156): 'paramtext loudness 0.5 %', double-click, the editor shows '0.5', Return with no typing gives 'Loudness = 50 %' (session capture `rt/verify-26/10-12-sheet.png`). At 1 %, click-away gives 100 %. Escape is safe (0.3 % stays 0.3 %)

**Current behaviour.** In the seven percent boxes, a bare number in (0,1] is a fraction of full scale and anything with '%' is literal, so '1' gives 100 % while '1.5' and '1 %' give 1.5 % and 1 %. Character and Tone take their own documented scales, and out-of-range entries clamp. The result is shown in the readout immediately and is undoable.

**Problem.** There is a real discontinuity, and nothing at the entry point says which rule applies (tooltips are off by default). It is a deliberate trade-off the owner made and documented, and the result is always visible.

**Root cause.** This is a product decision (0.1.6 item 2): the fraction shorthand, with '%' as the escape hatch. The only unintended consequence is that the value-box editor removes the '%' that the rule relies on (see the new finding).

**User impact.** Low in the claimed form. A user who types '1' meaning 1 % sees '100 %' at once and can retype or Undo. The high-impact variant is the editor round-trip (a sub-1 % value confirmed unchanged becomes 100 times larger), which is outside this claim. *Scope:* The seven percent parameters (Loudness, Comp Mix, Comp Stereo Link, Clip Mix, Color Depth, Limiter Stereo Link, Transients) and the Character/Tone boxes.

**Proposed improvement.** Keep the rule and the Character/Tone scales. Close the discoverability gap through [UI-002](findings-ui.md#ui-002): a visible unit suffix while editing tells the user they are in a percent box. Fix the unit-stripping round-trip separately: opening and confirming without typing must be a no-op.

**Alternatives considered.**

- *Drop the fraction rule ('1' always 1 %)* — Rejected. It reverses an explicit owner directive that the manual documents and tests pin, and makes '0.5' land on half of one percent again.
- *Make Character/Tone accept percent-style input (>1 divided by 100)* — Rejected. It invents a second scale for controls documented as 0…1 and −1…+1, which creates a new inconsistency.
- *Show the unit and scale at the entry point (via [UI-002](findings-ui.md#ui-002))* — The only change worth making, tracked under [UI-002](findings-ui.md#ui-002).
- *Preserve* — Chosen.

**Decision: Preserve · none.** The behaviour is owner-directed (0.1.6 item 2), explained in the code, documented in USER_MANUAL §3 and pinned by tests. Its results are immediately visible and undoable. The Character clamp is consistent with how every control handles out-of-range input. There is no evidence of harm beyond an accepted trade-off. The material defect found next to this rule is a separate issue.

**Dependencies.** [INPUT-017](findings-input.md#input-017) (percent open-and-confirm multiplies a value of 1 % or less by 100); [UI-002](findings-ui.md#ui-002) (unit visible while editing)

**Acceptance criteria.**

- (Preserve.) tests/state_tests.cpp testPercentTextEntryReadsFractionsAndLiteralPercents continues to pass unchanged
- USER_MANUAL §3 continues to state the fraction rule and the '%' escape

<details><summary>Verification record</summary>

**Method.** Read pctFrom and its rationale (e769f33:src/PluginParameters.cpp:119-147), the Character/Tone parsers (:283-288), USER_MANUAL §3 (e769f33:docs/user/USER_MANUAL.md:145-149, 172-173) and the pinning test (e769f33:tests/state_tests.cpp:5906-5963). Viewed the G-06 screenshots. On :156, with stepped motion, I also tested how the rule interacts with the unit-stripped editor: Loudness at 0.5 %, open the editor, Return with no typing, and the value becomes 50 %.

**Corrections to the candidate claim.** The behaviour is exactly as described, but the finding presents as unexplained what is a documented, owner-directed and tested rule. The discontinuity at 1 is explicitly accepted in the code (PluginParameters.cpp:132-135) and the manual. Character's '75' becoming 1.00 is ordinary out-of-range clamping of a control documented as 0…1, the same as Ceiling '95' giving 0.00 or Input Gain '50' giving 24.0. It is not a second scale rule. The real defect near this rule is different and was not in the claim: the editor strips the '%' (rawEditText), so confirming an unchanged sub-1 % value multiplies it by 100. That is reported as a new finding.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 2 · severity 2 · discoverability 3 · efficiency 1 · coherence 2 · change risk 3 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### INPUT-009

**KI-013: the click absorbed by a pop-up dismissal still starts a double-click run, so a quick second click on a knob resets it. Reproduced with the preset menu, the Settings panel and an Advanced combo list**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | confirmed | Accessibility/input | Pointer and text entry run on untuned JUCE defaults, so ordinary slips rewrite values | Phase 3 |

**Evidence**

- e769f33:src/gui/PluginEditor.h:166-189 — PopupShield consumes the dismissing press (and its double-click) but cannot un-count it
- e769f33:src/gui/PluginEditor.h:92-96 — Backdrop::mouseDown dismisses Settings/About/Save Preset on an outside press (same absorption, not covered by KI-013's wording)
- e769f33:src/gui/PluginEditor.h:237-269 — Knob::mouseDoubleClick: getNumberOfClicks()==2 → gesture-bracketed reset (undo step, detaches managed params)
- e769f33:src/gui/LookAndFeel.cpp:951 — value-box label setEditable(false, editable, false): double-click opens the text editor (second double-click actor)
- e769f33:docs/KNOWN_ISSUES.md:913-941 — KI-013 Low/Confirmed, 'not reproducible headlessly'
- JUCE e18f7f5 modules/juce_gui_basics/detail/juce_MouseInputSourceImpl.h:417-439 (getNumberOfMultipleClicks, isLongPressOrDrag 300 ms), :565-572 (canBePartOfMultipleClickWith: time, position, buttons, peer — no component)
- Anamorph@fd78c3b:docs/KNOWN_ISSUES.md:579-617 — sibling KI-018, same residual, declined a per-control guard
- runtime verify-5: session capture `rt/verify-5/22-25-sheet.png` (Tone reset after preset-menu dismissal; Settings open then Character reset; Lim Release reset to 100.0 ms after combo dismissal); app.log dumps: tone 0.50→0.00 at 150/250/350 ms, 0.50 kept at 600 ms; character 0.50→0.00; limRelease 300.0→100.0 ms, Undo → 300.0 ms
- runtime verify-5: .../rt/verify-5/30-sheet.png (dismissing click on LOCK consumed; second click toggles)

**Current behaviour.** After a pop-up (preset menu, combo list) or the Settings panel is dismissed by clicking on a knob, a second quick tap on the same spot within the OS double-click interval arrives as a double-click. The knob resets to its default: Ceiling to -0.10 dB, Loudness to 0 %. On a value readout, the text editor opens instead.

**Problem.** The dismissing click is supposed to have no effect on the control beneath (the INC-011 contract). In fact it becomes the first half of a double-click. The natural recovery from [INPUT-014](findings-input.md#input-014)'s consumed click, 'it didn't respond, click again', is exactly the gesture that triggers a reset. On the Ceiling that is a silent move to the loudest default (-1.00 → -0.10 dB), which matters for delivery specs.

**Root cause.** JUCE tracks the multi-click run per MouseInputSource, keyed on time, position, buttons and peer, not on the receiving component (juce_MouseInputSourceImpl.h:565-572). Neither the shield nor the Backdrop can reset it. Knob::mouseDoubleClick trusts getNumberOfClicks()==2 without checking that the knob itself received the first press.

**User impact.** Occasional. Recovery is one Undo, but only if the user notices; the Ceiling change is the least noticeable and the most consequential. It happens during menu/Settings exits, which occur every session. *Scope:* Every knob in both views (Knob struct) and every value readout (JUCE Label double-click edit), after any dismissal: preset menu, combo drop-downs (including those inside Settings), the Settings/About/Save backdrops. All platforms (shared JUCE code).

**Proposed improvement.** Target: a click that dismissed a pop-up or panel never counts toward a double-click on the control under it. A deliberate double-click on a knob still resets it. Constrained, component-local change:
- In Knob::mouseDown, record the event time of a press whose getNumberOfClicks()==1.
- In Knob::mouseDoubleClick, act only if that recorded first press is the immediately preceding one, within MouseEvent::getDoubleClickTimeout(). A run whose first press went to the shield or a Backdrop never produced a clicks==1 press on the knob, so it is ignored.
- Apply the same guard to the value-box label's double-click edit.
- Keep the shield and Backdrop unchanged.
- Update KI-013 (close it, or narrow it) and correct its scope to include backdrop dismissals.

**Alternatives considered.**

- *Accept as residual (status quo KI-013 / sibling KI-018)* — The sibling argued that a per-control guard re-creates the approach the shield replaced. In Anabasis only two classes act on double-click (Knob and the value-box label), and the reproduction shows a reset of delivery-critical knobs after routine Settings exits, so accepting it is weak.
- *Editor-level record of the absorbed press (time + position set in PopupShield::mouseDown and Backdrop::mouseDown), consulted by Knob and the value box* — Equivalent effect with a central writer. It couples the controls to the editor.
- *Keep the shield raised for the double-click interval* — It would swallow a legitimate second click, contradicting the shield contract. Rejected.
- *MouseEvent::setDoubleClickTimeout / patch JUCE* — The timeout is process-global, so it would affect the host and other plug-ins. A JUCE patch is a Build System change. Both rejected.

**Decision: Proceed · P2.** Reproduced deterministically on three dismissal surfaces with realistic click spacing. The failure silently changes a mastering parameter (including the Ceiling) through the very recovery gesture that the consumed click invites. A small, component-local guard fixes it without touching the shield contract, JUCE, threading or the macro layer.

**Dependencies.** [INPUT-014](findings-input.md#input-014) (the consumed dismissal click is what invites the quick second click)

**Acceptance criteria.**

- Preset menu open; Tone set to 0.50; two taps on the Tone knob 150 ms apart (first dismisses): Tone remains 0.50 and the menu is closed
- Settings open; Character 0.50; two taps 200 ms apart on Character: Character remains 0.50 and Settings is closed
- Advanced Limiter Style list open; Lim Release 300 ms; two taps 200 ms apart on Release: Lim Release remains 300.0 ms
- Same sequences on a value readout do not open the text editor
- With no pop-up open, a double-click (80-350 ms) on any knob still resets it to default as one undo step, and on a macro-managed knob still detaches it
- A press-and-drag immediately after a dismissal drags the knob normally
- KNOWN_ISSUES KI-013 is closed or narrowed, with its scope covering backdrop dismissals

<details><summary>Verification record</summary>

**Method.** Code at e769f33:
- e769f33:src/gui/PluginEditor.h:166-189: PopupShield consumes down/up/drag/double-click/wheel.
- e769f33:src/gui/PluginEditor.h:92-96: Backdrop::mouseDown dismisses Settings/About/Save and consumes.
- e769f33:src/gui/PluginEditor.h:237-269: Knob::mouseDoubleClick resets when getNumberOfClicks()==2.
- e769f33:src/gui/LookAndFeel.cpp:951: the value-box label edits on double-click.
- e769f33:docs/KNOWN_ISSUES.md:913-941: KI-013.
- JUCE e18f7f5 detail/juce_MouseInputSourceImpl.h:417-439 and :565-572: the run is keyed on the input source (time <400 ms Linux default, position within tolerance, same buttons, same peer); a drag or a hold over 300 ms breaks it.
Runtime on :135 with stepped moves, then `xdotool click --repeat 2 --delay D 1`:
- Preset menu open, two clicks on the Tone knob: Tone 0.50→0.00 at D=150, 250 and 350 ms; unchanged at D=600 ms.
- Settings open, two clicks (D=200) on Character: 0.50→0.00, Settings closed.
- Advanced Limiter Style list open, two clicks (D=200) on Lim Release: 300.0→100.0 ms (default), Style unchanged.
- One Undo restored 300.0 ms.
- Control: with the preset menu open, a single dismissing click followed by a second click on LOCK: the first was consumed and the second toggled.

**Corrections to the candidate claim.** The claim understates the scope. KI-013 attributes the absorbed press to the pop-up shield, but the same reset follows a Settings-panel dismissal (Backdrop), and JUCE drop-down lists trigger it too. It also narrows exposure in one way: the second press must be a quick tap (released within 300 ms, not dragged, within the position tolerance). A press-and-drag after dismissal behaves normally. The reset is visible (the knob sweeps, the readout changes) and is one Undo step.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 2 · severity 3 · discoverability 4 · efficiency 2 · coherence 3 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### INPUT-010

**The mouse wheel moves a fixed 15 % × wheel delta per event: about 2.9 % of the range per notch on Linux (≈3.5 % on Windows, per the code), untidy readouts (52.9 %, -9.41 dB), and every modifier ignored**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P3** | high | confirmed | Interaction model | Pointer and text entry run on untuned JUCE defaults, so ordinary slips rewrite values | Phase 3 |

**Evidence**

- JUCE 9.0.1 (pinned e18f7f5, e769f33:CMakeLists.txt:83) juce_Slider.cpp:1131 — `auto proportionDelta = wheelAmount * 0.15;`
- JUCE 9.0.1 juce_Slider.cpp:1148-1165 — no modifier test; `newValue = value + jmax (normRange.interval, std::abs (delta)) …`; `ScopedDragNotification drag (owner);` per wheel event (one host gesture per notch)
- JUCE 9.0.1 juce_Slider.cpp:1350 — `bool scrollWheelEnabled = true;`; grep src/gui: no slider wheel override
- JUCE 9.0.1 juce_XWindowSystem_linux.cpp:3767-3768 (±50/256 per notch); juce_Windowing_windows.cpp:2748-2755 (amount = 0.5·120, /256); juce_NSViewComponentPeer_mac.mm:813-819 (precise deltas × 0.5/256)
- e769f33:docs/user/USER_MANUAL.md:141-151 — the wheel is not documented. Contrast Anamorph:docs/user/USER_MANUAL.md:357, which documents its imager's wheel.
- Runtime G-04: session capture `rt/gestures/18-tone-wheel.png`
- Runtime verify-10 (:140, stepped approach): Loudness 50 %, then notch up 52.9 %, notch up 55.9 %, Ctrl notch 58.8 %, Shift notch 61.7 %, Alt notch 64.6 %; Ceiling -10.00 dB, then notch -9.41 dB, Ctrl notch -8.82 dB (harness dump, rt/verify-10/app.log)

**Current behaviour.** Every wheel event over a knob, fader or readout moves the value by 15 % of the range times the platform wheel delta: 2.93 % per notch on X11, ≈3.5 % on Windows from code, and small fractional steps from macOS trackpads. The step is never snapped to a meaningful grid, so readouts land on 52.9 %, 55.9 %, -9.41 dB. No modifier changes the step. Each notch is its own host gesture (undo and re-engage consequences are in [STATE-007](findings-state-model.md#state-007)).

**Problem.** The wheel is a quick nudge tool, but its steps are arbitrary fractions of the range. It cannot produce tidy values (1 %, 0.1 dB), it cannot fine-nudge the Ceiling (0.59 dB per notch), and its behaviour varies by platform. It is undocumented, so users cannot predict it.

**Root cause.** JUCE's default Slider wheel handling was left as it is: a proportional step with no quantisation and no modifier handling.

**User impact.** Wheel users get untidy readouts and coarse steps. They must fall back to typing (hidden) or dragging for exact values. This is polish-level friction, with no audio or data harm beyond what [STATE-007](findings-state-model.md#state-007) covers. *Scope:* All rotary knobs, both faders and all readouts (the wheel also works over the value text), in both views. Combos ignore the wheel (G-12), which is consistent.

**Proposed improvement.** Target: one wheel notch moves a control by one musically meaningful step and lands on that step's grid. For example: 1 % on percent controls; 0.1 dB on Ceiling and Input Gain; 0.5 dB on thresholds and EQ gains; one semitone-like or 1/24-octave step on frequencies; one interval on stepped ratios. Shift+wheel moves 1/10 of that step, snapped to the fine grid. Implement it as a Knob::mouseWheelMove override that uses only the sign of each discrete notch, and accumulates precise or smooth deltas so that a fixed scroll distance gives a fixed number of steps on every platform. The step table lives beside the parameter formatting (presentation only; no range or ID change). Implement it in the same override as [STATE-007](findings-state-model.md#state-007)'s gesture coalescing, so wheel edits are bracketed once per burst rather than once per notch. Document the wheel in USER_MANUAL §3.

**Alternatives considered.**

- *Disable the wheel (setScrollWheelEnabled(false))* — Removes the untidy values, but also a useful nudge. Anamorph documents its imager's wheel, so dropping the wheel here would be a regression in family feel.
- *Only snap the result to the display precision* — Tidier readouts, but steps stay platform-dependent and coarse (0.59 dB on the Ceiling), with no fine modifier.
- *Leave as-is and document* — Cheapest. Predictable once documented, but the untidy values and the lack of a fine wheel remain.

**Decision: Proceed · P3.** Confirmed and code-explained. The fix is presentation-level, has no gate impact, and fits into the override [STATE-007](findings-state-model.md#state-007) already needs. Impact is low (polish), so P3.

**Dependencies.** [STATE-007](findings-state-model.md#state-007) (per-notch gesture bracketing: undo-stack flooding and macro re-engage per notch; implement in the same wheel override); [UX-016](findings-ux.md#ux-016) (document the wheel); [INPUT-007](findings-input.md#input-007) (use the same fine modifier, Shift, for the wheel)

**Acceptance criteria.**

- From Loudness 50 %, one wheel notch up gives exactly 51 %. From Ceiling -10.00 dB, one notch up gives -9.90 dB, and Shift+notch gives -9.99 dB.
- After any sequence of plain notches, every readout lands on its step grid: no 52.9 % or -9.41 dB.
- The per-notch step is identical on X11 and Windows mouse wheels. On a macOS trackpad a fixed scroll distance maps to a fixed number of steps, whatever the event granularity.
- Ctrl/Cmd+wheel either equals plain or is documented. Shift+wheel is 1/10 of the step.
- USER_MANUAL §3 documents the wheel step and modifier.
- Undo and re-engage behaviour of wheel edits matches [STATE-007](findings-state-model.md#state-007)'s accepted decision.

<details><summary>Verification record</summary>

**Method.** Read JUCE 9.0.1 juce_Slider.cpp:1126-1137 (getMouseWheelDelta: wheelAmount*0.15 of the range), :1139-1172 (mouseWheelMove: no modifier handling, a minimum of one interval, a ScopedDragNotification per event), :1350 (scrollWheelEnabled default true), and the platform delta scaling in juce_XWindowSystem_linux.cpp:3767-3768 (±50/256), juce_Windowing_windows.cpp:2748-2755 (0.5·120/256 per notch) and juce_NSViewComponentPeer_mac.mm:813-819 (precise deltas scaled by 0.5/256). Grep: no mouseWheelMove override or setScrollWheelEnabled on sliders in src/gui (the only override is the pop-up shield, PluginEditor.h:187). Reproduced on :140.

**Corrections to the candidate claim.** The claim is accurate for Linux/X11: measured 50 to 52.9 to 55.9 %, Ceiling -10.00 to -9.41 dB. Ctrl, Shift and Alt notches gave identical steps (58.8, 61.7, 64.6 %; Ceiling Ctrl notch -9.41 to -8.82 dB). The per-notch size varies by platform. From code (not measured), a Windows notch is 0.234·0.15 ≈ 3.5 % of the range, and macOS precise (trackpad) deltas produce many small events, each moving at least one parameter interval. So 'about 2.9 % per notch' is specific to X11. Momentum (inertial) scroll events are not filtered by JUCE's Slider; whether this causes post-release drift on macOS trackpads was not verifiable here.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 3 · severity 1 · discoverability 3 · efficiency 2 · coherence 2 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### INPUT-011

**Dragging the value text uses 180 px for full scale while the knob uses JUCE's 250 px, so one control has two drag sensitivities, and the text drag has no fine mode**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P3** | high | confirmed | Interaction model | Pointer and text entry run on untuned JUCE defaults, so ordinary slips rewrite values | Phase 3 |

**Evidence**

- e769f33:src/gui/LookAndFeel.cpp:921 — prop = downProp + (-e.getDistanceFromDragStartY()) / 180.0, with no modifier handling in :892-925
- e769f33:src/gui/PluginEditor.cpp:1129 — RotaryVerticalDrag. No setMouseDragSensitivity or setVelocityModeParameters anywhere in src/gui, so the JUCE default applies
- JUCE 9.0.1 juce_Slider.cpp:1326 — pixelsForFullDragExtent = 250, used at :786/:800
- Anamorph (read-only) Anamorph@fd78c3b:src/gui/LookAndFeel.cpp:817 — the same /180.0 constant, so it is inherited under ADR-0009
- G-07: session capture `rt/gestures/16a-loud-valuetext-drag.png`, session capture `rt/gestures/16-sheet.png`
- verify-26/drag-compare (display :156, 12-step stepped drags): 60 px from 50 % gives knob '72 %' and text '80.6 %'. 45 px from 20 % gives knob '36.4 %' and text '42.8 %' (rt/verify-26/app.log)

**Current behaviour.** The same vertical drag moves a parameter about 1.39 times further when started on the numeric readout than when started on the knob. Ctrl gives fine control on the knob but not on the readout.

**Problem.** One control has two drag gains, and which one applies depends on the exact pixel pressed (the readout sits directly under the knob). The documented gesture ('value boxes: drag vertically to change', USER_MANUAL §3) behaves differently from the knob gesture beside it, and the text drag has no precision mode.

**Root cause.** The ValueBox drag was copied from Anamorph with a literal 180 px extent instead of reading the owning slider's getMouseDragSensitivity(), and it does not route modifiers.

**User impact.** A user who grabs the number to make a fine adjustment overshoots more than on the knob and has no fine modifier there. This is a mild, recurring inconsistency, not a wrong result. *Scope:* Every value box (about 40 rotary readouts plus the Input Gain and SC HPF fader boxes), in both views.

**Proposed improvement.** The readout should drag exactly like its knob. Replace the 180.0 literal with (double) s->getMouseDragSensitivity() (250 today, and it follows any future sensitivity change). Honour the same fine modifier the knob uses (a ctrl-held drag scaled by the knob's velocity/fine factor), so the readout is not the only surface without precision control. Record the adaptation against the Anamorph original per ADR-0009's provenance rule.

**Alternatives considered.**

- *Use the slider's getMouseDragSensitivity() in ValueBox* — Chosen. It is a one-line, UI-only change that removes the second constant.
- *Change the knobs to 180 px* — Rejected. It changes the feel of the primary control for 40 knobs to match a secondary surface.
- *Leave as-is for family parity with Anamorph* — Acceptable but inconsistent. Parity with a sibling that shares the inconsistency is not a reason to keep it.
- *Remove dragging from the value box* — Rejected. It removes a documented gesture (USER_MANUAL §3).

**Decision: Proceed · P3.** Confirmed at code level and reproduced with realistic stepped motion. There is no recorded rationale for the 180 px constant, and the fix is local and display-layer only. Priority stays P3 because the harm is a mild inconsistency with no wrong output. Ease of implementation does not raise it.

**Dependencies.** G-03 (knob fine-drag modifier behaviour — the readout should mirror whatever the knob does)

**Acceptance criteria.**

- From Loudness 50 %, a 60 px stepped vertical drag on the knob and a 60 px stepped vertical drag on its readout produce readouts within 1 percentage point of each other
- A ctrl-held drag on a readout changes the value by the same amount as the same ctrl-held drag on its knob
- testAValueBoxClickIsNotAMacroGesture and the ValueBox gesture-bracket tests still pass (the drag still opens its bracket on first movement)

<details><summary>Verification record</summary>

**Method.** Read ValueBox::mouseDrag (e769f33:src/gui/LookAndFeel.cpp:892-925, constant at :921), setupRotary (e769f33:src/gui/PluginEditor.cpp:1129-1136), and searched src/gui for setMouseDragSensitivity and setVelocityModeParameters (none). Read JUCE juce_Slider.cpp:786,800,1326. Compared with the Anamorph working tree at Anamorph@fd78c3b:src/gui/LookAndFeel.cpp:817 (same 180.0). Reproduced on :156 with 12-step stepped vertical drags from the same start value on the Loudness knob and on its value text.

**Corrections to the candidate claim.** The ratio claimed ('about 1.4x') is confirmed: 250/180 = 1.39. Measured: a 60 px drag from 50 % gave 72 % on the knob and 80.6 % on the text. A 45 px drag from 20 % gave 36.4 % on the knob and 42.8 % on the text. Both are consistent with about 250 px and 180 px of travel after JUCE's roughly 5 px drag threshold. Additionally, ValueBox::mouseDrag ignores modifiers, so the knob's ctrl fine mode (G-03) does not exist on the text. On the two horizontal faders the value box drags vertically while the fader itself is horizontal and absolute.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 3 · severity 1 · discoverability 2 · efficiency 2 · coherence 3 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### INPUT-012

**The GR|SPEC switch looks like a two-segment selector but acts as a single flip, so clicking the label that is already active switches away from it**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Preserve** | **none** | high | confirmed | Interaction model | Pointer and text entry run on untuned JUCE defaults, so ordinary slips rewrite values | — |

**Evidence**

- e769f33:src/gui/GrHistoryView.cpp:42-46 — 'The whole pill is ONE toggle (0.1.2 item 5)… any press inside it switches the well to the spectrum'
- e769f33:src/gui/SpectrumView.cpp:154-158 — mirror: any press sets spectrumOn=false
- e769f33:src/gui/LookAndFeel.h:290-294 — 'the pill is ONE TOGGLE… The 0.1.1 side-of-divider semantics made a press on the active segment a silent no-op, which read as a stuck control… the segments remain as the state display'
- e769f33:docs/architecture/design-decisions/ADR-0023-012-field-fix-contracts.md:240-244 — clause 7: '…and the whole pill is one toggle — no press on it is a no-op' (Status: Accepted, gate cleared 2026-08-09, line 3)
- e769f33:docs/user/USER_MANUAL.md:239-241 — 'clicking the pill — anywhere on it — toggles to the other view'
- e769f33:src/gui/SpectrumView.cpp:34 / GrHistoryView.cpp:17 — tooltip 'Switch the graph between the spectrum and the GR history' (describes a flip; tooltips default OFF)
- Runtime (verify-23): session capture `rt/verify-23/pill-seq.png` (p0 SPEC → click SPEC → GR → click GR → SPEC → click divider → GR), stepped motion
- Observation V-15 + session capture `rt/visuals/18a-spec-now-well.png`, 18b-after-click-SPEC-half-well.png, 18c-after-click-GR-half-well.png
- *Merged at triage from another verifier's note, quoted as written:* Extend the scope to the A/B pill. ABControl::mouseDown (e769f33:src/gui/PluginEditor.h:192-201) toggles on any press on either letter, so clicking the already-lit letter switches slots (session capture `rt/verify-8/v22-crop.png`). Here the cost is higher than GR|SPEC: a different sound plus a forced duck ([DSP-002](findings-dsp-tech.md#dsp-002)). The pill was inherited verbatim from Anamorph ('FabFilter-style'), and the family A/B convention governs it (DEVELOPMENT_BRIEF §1.2; BRAND_CONSISTENCY_CHECKLIST.md:53), not ADR-0023 §7. Keep the toggle, and apply the optional whole-pill hover highlight to both pills. The right-click half is [INPUT-013](findings-input.md#input-013).

**Current behaviour.** Any press anywhere in the 82x22 hit area flips the well to the other view. The two labelled halves show only which view is active. There is no hover state.

**Problem.** A segmented-control look conventionally means 'click the segment to select it'. A user who clicks the already-lit segment, for example to make sure SPEC is showing, gets the other view instead.

**Root cause.** The owner deliberately changed the interaction to a flip in 0.1.2 (item 5; ADR-0023 §7) after a field report that the side-of-divider no-op read as a stuck control. The two-segment visual was kept as the state display.

**User impact.** Occasionally one unwanted view switch, recovered by one more click. No state loss: the choice is just the persisted well mode. The opposite model had a reported field failure (a no-op that read as stuck). *Scope:* The graph-well mode pill in both views and both editor modes.

**Proposed improvement.** Keep the flip behaviour. Optional, low-value polish that stays within ADR-0023 §7: give the pill a whole-pill hover highlight (outline brightening across both halves). That signals one control with one action rather than two targets, and changes no behaviour.

**Alternatives considered.**

- *Segment-select semantics (click GR shows GR, click SPEC shows SPEC; clicking the active segment does nothing)* — Reintroduces the owner-reported 'stuck' no-op and directly conflicts with Accepted ADR-0023 clause 7 ('no press on it is a no-op'): a hard stop.
- *Hybrid: an inactive segment selects, and an active segment flips* — Behaviourally identical to today (every press lands on the other view), so it changes nothing.
- *Restyle as a single-label toggle or sliding switch* — Would remove the look/behaviour mismatch, but reopens an owner-ratified visual and the brand pass for little evidenced gain.
- *Whole-pill hover highlight* — Cheap and consistent with the ADR; optional polish, since there is no evidence of users being misled.

**Decision: Preserve · none.** The mechanics are confirmed, but they are the owner's deliberate resolution of a reported field problem, and they are mandated by Accepted ADR-0023 §7. The alternative model has documented harm (a no-op read as a stuck control). The cost of the current model is one recoverable click, with no evidence of real confusion. Changing the behaviour would hit a hard stop without evidence to justify it. The hover highlight is noted as optional polish, not a required change.

**Architecture gates.**

- Accepted ADR-0023 clause 7 ('the whole pill is one toggle — no press on it is a no-op'): any segment-select change conflicts (hard stop)

**Dependencies.** [UI-007](findings-ui.md#ui-007) (same control; any visual polish to be done together)

**Acceptance criteria.**

- Any press anywhere in the pill's hit area switches the well to the other view, in both views and both editor modes (testTheGraphWellViewsOnlyClaimTheirModeChips continues to pass).
- USER_MANUAL §3.4 continues to state 'clicking the pill — anywhere on it — toggles to the other view'.
- (If the optional hover polish is adopted) hovering any point of the pill highlights the whole pill, not one segment, and changes no behaviour.

<details><summary>Verification record</summary>

**Method.** Code: GrHistoryView::mouseDown sets spectrumOn=true and SpectrumView::mouseDown sets it false for any press inside the pill, with no reference to the segment (e769f33:src/gui/GrHistoryView.cpp:35-47, e769f33:src/gui/SpectrumView.cpp:146-159). graph_switch::paint draws two halves with the active one lit, and its banner calls the segments 'the state display, not separate targets' (e769f33:src/gui/LookAndFeel.h:290-294, 306-347). Runtime on :153 using stepped pointer motion (5 intermediate moves, then 2 settle moves, before each click). SPEC active, click the SPEC half (115,745): GR. GR active, click the GR half (78,745): SPEC. Click the divider (97,745): GR. See session capture `rt/verify-23/pill-seq.png`. Hovering the pill produced no visual change in the pill region (a p4/p5 diff shows only live-trace rows above it). graph_switch::paint takes no hover argument. Viewed rt/visuals/18a-18c. Found the governing record: ADR-0023 clause 7.

**Corrections to the candidate claim.** The governing Accepted record is ADR-0023 clause 7 ('the whole pill is one toggle — no press on it is a no-op'), not ADR-0016. ADR-0016 covers only the meaning of int_spectrumOn. The observer's 'will get the other view half the time' is overstated. A user who clicks the segment of the view they want always gets it unless it is already showing, and then a second click recovers. The field report that motivated the design ('clicking SPEC does not switch back', LookAndFeel.h:290-294) is evidence that users do click the lit label expecting a switch.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 2 · severity 1 · discoverability 2 · efficiency 1 · coherence 2 · change risk 4 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### INPUT-013

**The right button behaves exactly like the left button on every control except the combo boxes, with no context menu: right-click toggles A/B, MATCH and FREEZE, runs Copy, resets STATISTICS, jumps faders by up to +17.6 dB, and re-engages detached Advanced edits; a right-drag moves and detaches knobs**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Interaction model | Pointer and text entry run on untuned JUCE defaults, so ordinary slips rewrite values | Phase 3 |

**Evidence**

- JUCE 9.0.1 (pinned e18f7f5, e769f33:CMakeLists.txt:83) juce_Slider.cpp:862-899 — `if (e.mods.isPopupMenu() && menuEnabled) showPopupMenu(); … else if (normRange.end > normRange.start) { … currentDrag = std::make_unique<ScopedDragNotification> (owner); mouseDrag (e); }`; :1347 `bool menuEnabled = false;`
- e769f33:src/gui/LookAndFeel.cpp:423-425 — 'gated on `setPopupMenuEnabled`, which this editor never calls and which JUCE leaves off'
- e769f33:src/gui/PluginEditor.h:198 — `void mouseDown (const juce::MouseEvent&) override { if (onToggle) onToggle(); }` (A/B, any button); :520 EditedDot likewise; :235 Knob forwards non-Alt presses of any button to Slider::mouseDown
- e769f33:src/gui/LoudnessMeterView.cpp:67-70 — `mouseDown (const juce::MouseEvent&) { processor.requestMeterReset(); }` (any button)
- e769f33:src/gui/SpectrumView.cpp:146-159 and e769f33:src/gui/GrHistoryView.cpp:35-47 — chip press toggles int_spectrumOn (any button)
- e769f33:src/PluginProcessor.cpp:283-300 — a gesture-begin on loudness/character/tone re-engages everything, 'a gesture that moves NOTHING — press and release … ' included
- e769f33:src/gui/LookAndFeel.cpp:898-918 — the same codebase deliberately stopped a press on the macro READOUT from re-engaging ('pressing on a numeric readout is not that notice'). A right-click on the macro knob still does.
- e769f33:src/gui/PluginEditor.cpp:330 copyButton.onClick, :315 titleButton.onClick, :472 presetName.onClick — juce::Button fires onClick for any mouse button
- Runtime G-05, LAY-13(a)(b)(e), LAY-05: session capture `rt/gestures/13-loud-rightclick-crop.png`, session capture `rt/gestures/22a-compthr-rightclick.png`, session capture `rt/layout/26c-preset-menu.png`, session capture `rt/layout/26c-about.png`
- Runtime verify-10, right-drag: Comp Threshold knob right-drag 60 px up, -20.0 to -10.7 dB, detach dot shown (session capture `rt/verify-10/03-sheet.png`); readout right-drag 40 px up, -20.0 to -11.6 dB
- Runtime verify-10, right-click toggles A/B (session capture `rt/verify-10/04-sheet.png`) and resets STATISTICS: I -16.2 became '-', LRA 17.0 LU became '-', TP 0.89 dBTP was replaced by the current value (session capture `rt/verify-10/05-sheet.png`)
- Runtime verify-10, right-click toggles: MATCH Off to On to Off, FREEZE Off to On to Off; right-click on Copy in slot A made slot B equal to A (Loudness 0 to 75 %, Comp Threshold 0.0 to -12.0 dB); right-click on the GR chip switched GR to SPEC (session capture `rt/verify-10/08-sheet.png`)
- Runtime verify-10, right-click and middle-click on the Input Gain track at x=140 each jumped 0.0 to 17.6 dB (session capture `rt/verify-10/02-sheet.png`)
- Runtime verify-10, right-click without movement on the Loudness knob (Simple) with Comp Threshold detached at -11.6 dB: Comp Threshold became 0.0 dB (the macro value) and the edited dot vanished; Undo restored -11.6 dB (session capture `rt/verify-10/06-sheet.png`)

**Current behaviour.** No control has a context menu. Every non-combo control treats the right button (and the middle button, and on macOS Ctrl-click, which JUCE reports as a popup click) exactly like the left. Knobs and readouts drag. Faders jump to the pointer. Macro knobs open a macro gesture that re-engages every detached parameter. Toggles, A/B, Copy, Undo/Redo, the wordmark and the preset name fire their click actions. The STATISTICS panel clears its integrated, LRA, PLR and peak holds. The graph chip switches the view. Combos alone ignore the right button.

**Problem.** Many plugin users right-click a control out of habit, expecting a menu (reset, type a value, host automation or MIDI learn). Here that exploratory click silently performs the primary action, and several of those actions destroy something. An integrated-loudness measurement is lost and cannot be recovered without replaying the programme. A detached Advanced edit is re-engaged, and in Simple view the only cue is a 10 px dot disappearing. The other A/B slot is overwritten. A mastering trim jumps +17.6 dB. The A/B slot or MATCH changes and the sound changes. Several of these give no cue that anything happened.

**Root cause.** JUCE defaults (Slider menuEnabled=false, which falls through to the drag branch; Button clicks on any button) combined with custom mouseDown handlers written without a button test. No context-menu or secondary-button behaviour was ever designed.

**User impact.** A user who right-clicks while exploring can lose a long integrated-loudness reading (unrecoverable), discard hand edits (recoverable by Undo only if noticed), compare the wrong slot, or overwrite the other A/B slot. Right-clickers also find no reset, type-in or automation affordance. *Scope:* Every knob, fader and readout (50+), all toggles and TextButtons in the top bar and the utility and Simple rows, A/B, the edited dot, the STATISTICS panel and the graph chip, in both views. Hosts' own parameter context menus (JUCE getHostContext) are not offered either.

**Proposed improvement.** Target: a secondary click (right button, middle button, macOS Ctrl-click) never performs a primary action and never opens a gesture. (1) Required: filter non-primary buttons in every press handler. Override Knob::mouseDown, mouseDrag and mouseUp so that a press with isPopupMenu or a non-left button does not reach Slider::mouseDown; this also stops right-drag and the right-click macro re-engage. Add the same test to ValueBox, ABControl, EditedDot, LoudnessMeterView, SpectrumView and GrHistoryView. For juce::Button instances, gate the click on the triggering modifiers (Button::clicked(const ModifierKeys&) receives the mouse-up modifiers) through a small shared subclass or helper. (2) Recommended next step: a right-click on a parameter control opens a small menu with Reset to default, Enter value… (the same editor as the readout) and, where the host supplies one, the host's parameter menu via AudioProcessorEditor::getHostContext()->getContextMenuForParameter. The menu item wording is owner-supplied (C8), so step (2) needs the owner's text before it ships. On macOS Ctrl-click is the popup click, so Cmd and Shift stay the fine-drag modifiers ([INPUT-007](findings-input.md#input-007)).

**Alternatives considered.**

- *Enable JUCE's built-in Slider popup (setPopupMenuEnabled(true))* — Gives a menu for free, but its items are 'Velocity-sensitive mode' and 'Rotary mode' sub-menus (juce_Slider.cpp:655-680), which are irrelevant or confusing for users, are unapproved UI copy (C8), and hit the sub-menu layout caveat at LookAndFeel.cpp:410-430. Rejected.
- *Filter non-primary buttons only (step 1), no menu* — Removes every destructive side effect with no new UI copy. Right-clickers still find nothing, but nothing breaks. This is the minimum acceptable change.
- *Leave as-is* — Keeps destructive side effects on a habitual gesture, including an unrecoverable measurement reset. Rejected.

**Decision: Modify · P2.** Reproduced with realistic motion and explained in code. It is broader than claimed: seven control families perform their primary action on the right button, and three results are destructive. Right-clicking is a common habit and one outcome cannot be recovered (the measurement reset), so the verifier rated it P1 ('frequent trap with costly recovery'); calibration lowered it to P2 (below). It is not P0, because right-click is not the product's ordinary action and most outcomes are visible and undoable. Step 1 needs no UI copy. Step 2 waits only for owner wording.

*Calibration:* the verifier judged Proceed / P1; the final judgement is Modify / P2. Challenge accepted: an exploratory trap with mostly cheap recovery; the unrecoverable STATISTICS reset is the same handler a left click fires ([UX-002](findings-ux.md#ux-002)) and the macro re-engage also fires on a designed left press ([MODEL-001](findings-state-model.md#model-001)), so P2. Modify: ship step 1 only. Filter non-primary buttons in mouseDown/mouseUp/mouseDrag (overriding Button::clicked cannot block the action; ToggleButtons flip in internalClickCallback), also in mouseDoubleClick and before the Alt branch, through one shared PrimaryButtonOnly mixin per the click-shield rule, plus a tree-walking regression test. Defer context menus until owner copy/design. Keep the macro-layer gate confirmation.

*Adversarial challenge:* evidence holds: yes; priority justified: no (suggested P2); decision justified: no (suggested Modify). The evidence is real, reachable and code-explained, and the judge's scope (every custom handler, every juce::Button, Slider drag) is accurate. P1 does not survive the rubric. 'Frequent trap with costly recovery' needs both conditions. The trap is exploratory: a user who right-clicks looking for a menu finds none and stops, so frequency is nearer 2 than 3. The most habitual target, a non-macro knob body, changes nothing on a static right-click (G-05). Recovery is mostly cheap. A/B, MATCH, FREEZE and the GR/SPEC chip are visible and toggle back. The fader jump and the macro re-engage are Undo-able (06c after-undo). Copy is Undo-able on the other slot. The only unrecoverable outcome, the STATISTICS reset, is the same handler a LEFT click already fires, so its cost belongs mainly to LAY-13(c). The costliest right-button case, silent macro re-engage in Simple view, also happens on a designed left press. The right button adds an unintended second route to these outcomes, not a new class of damage. That matches P2: a meaningful robustness improvement in a less frequent, mostly low-cost situation. It is not P0: nothing blocks use, and right-click is not the product's ordinary action. Modify rather than Proceed. Ship step 1 only, with the corrected Button filter, the double-click and Alt coverage, and a tree-walking regression test. Defer step 2 (context menu, host menu) until the owner supplies copy and a design decision on non-automatable parameters. Keep the macro-layer gate confirmation the judge named. *Proposal risks:* (1) The Button mechanism in the proposal does not work. Overriding Button::clicked(const ModifierKeys&) cannot block the action. sendClickMessage calls clicked(mods), then the listeners (including ButtonParameterAttachment::buttonClicked, juce_ParameterAttachments.cpp:269-274), then onClick (juce_Button.cpp:410-433) whatever clicked() does. For every juce::ToggleButton (clickTogglesState: MATCH/loudnessComp, FREEZE, Bypass, Advanced, TP, Delta, Auto, Lock, e769f33:src/gui/PluginEditor.h:455-505), internalClickCallback flips the state through setToggleState before any click hook runs (juce_Button.cpp:364-377, 167-204). The filter must sit in mouseDown/mouseUp/mouseDrag: a non-primary press is never forwarded to Button::mouseDown/mouseUp. That keeps the keyboard and accessibility triggers (triggerClick → handleCommandMessage) intact. (2) The Knob override list misses mouseDoubleClick. A right double-click still reaches Knob::mouseDoubleClick → bracketed doReset (e769f33:src/gui/PluginEditor.h:238-266), which pushes an undo step and detaches a managed parameter, so acceptance criterion 1 would still fail on a double right-click. The filter must also run before the Alt branch (e769f33:src/gui/PluginEditor.h:217), or Alt+right-click still resets. (3) Seven or more per-class predicates go against the codebase's own stated rule for pointer hazards: the click shield comment at e769f33:src/gui/PluginEditor.h:117-121 reads 'one rule in one place … covers controls added later for free — where a predicate bolted onto each control has to be remembered every time'. Use one shared mixin (e.g. a `PrimaryButtonOnly<T>` template over Slider/Button/Component). Add a state_test that walks the editor's child tree and sends synthetic right/middle presses to every control, rather than the four hand-picked cases in the acceptance criteria, so a new control cannot silently regress. (4) Step 2 is a new UI surface, not a fix. It needs owner copy (C8) and a DESIGN/brand decision. The host menu from getContextMenuForParameter would be offered for parameters the harness reports as '(not automatable)' (loudness, character, tone, freeze, advancedMode, lookahead, truePeakMode, dither). That interaction needs design, so step 2 should be Deferred rather than bundled into a Proceed. (5) The gate flag is correctly named: which pointer events count as a macro gesture touches the §5.3 macro-layer contract (e769f33:src/PluginProcessor.cpp:287). The judge did not name any other gate that the step-1 filter crosses: no parameter ID, schema, threading or latency change.

**Architecture gates.**

- Simple/Advanced macro-layer contract (DESIGN §5.3 'the next macro-knob gesture re-engages', e769f33:src/PluginProcessor.cpp:287): stopping secondary-button presses from opening a macro gesture changes which pointer events trigger re-engage. It is a UI input filter, not a change to the rule, but it should be confirmed under ARCHITECTURE_REVIEW_GATE before merge.

**Dependencies.** [INPUT-002](findings-input.md#input-002) (the fader jump on a right or middle press is removed by the same filter); [INPUT-007](findings-input.md#input-007) (macOS Ctrl-click is a popup click, so the fine modifier must not be Ctrl on macOS); [UX-016](findings-ux.md#ux-016) (document right-button behaviour and, if adopted, the menu); [UX-002](findings-ux.md#ux-002) (the STATISTICS left-click reset, LAY-13(c); this filter removes only the right-button path); AI_AGENT_POLICY C8: context-menu item wording is owner-supplied (step 2 only)

**Acceptance criteria.**

- A right-click, a middle-click and (macOS) a Ctrl-click on every knob, fader, readout, toggle, A/B, Copy, Undo/Redo, preset name, wordmark, edited dot, STATISTICS panel and GR/SPEC chip leave every parameter, both A/B slots, the undo depth, the view state and the meter holds unchanged. Verified by harness dump plus screenshot before and after, in both views.
- A right-drag of 60 px on Comp Threshold (knob and readout) changes nothing and sets no detach dot.
- With a managed parameter detached, a right-click on the Loudness, Character or Tone knob leaves it detached at its value.
- New state_tests cases: a synthetic right-button mouseDown/mouseUp on LoudnessMeterView does not call requestMeterReset; on ABControl it does not toggle the slot; on a macro Knob it opens no gesture and leaves detachMask() unchanged.
- All left-button behaviour is unchanged: the existing state_tests suite and pluginval pass.
- If step 2 ships: a right-click on a parameter control opens a menu with owner-approved items (Reset to default / Enter value…, plus host items where getHostContext() provides them), and choosing Reset is one undo step.

<details><summary>Verification record</summary>

**Method.** Code: JUCE 9.0.1 juce_Slider.cpp:862-899 (a popup-menu click goes to showPopupMenu only if menuEnabled, default false at :1347; otherwise any button takes the drag branch and opens a ScopedDragNotification); no setPopupMenuEnabled in src/gui (only a comment at e769f33:src/gui/LookAndFeel.cpp:423-425 noting it is never called). Custom mouseDown handlers without a button filter: ABControl (e769f33:src/gui/PluginEditor.h:198), EditedDot (:520), LoudnessMeterView (e769f33:src/gui/LoudnessMeterView.cpp:67-70), SpectrumView (e769f33:src/gui/SpectrumView.cpp:146-159), GrHistoryView (e769f33:src/gui/GrHistoryView.cpp:35-47), ValueBox (e769f33:src/gui/LookAndFeel.cpp:836-873). The macro gesture-begin branch re-engages on any gesture, even one with no movement (e769f33:src/PluginProcessor.cpp:283-300). Runtime on :140 with stepped approach motion before every press, as listed in the evidence.

**Corrections to the candidate claim.** The claim is confirmed and was understated. The previously untested right-drag moves knobs: Comp Threshold went -20.0 to -10.7 dB and got the detach dot. It also moves readouts: -20.0 to -11.6 dB. Several phase-2 statements were wrong because of where or how the observer clicked. LAY-13(e) said a right-click on A/B and on STATISTICS did nothing; here the right-click toggled A to B and back, and reset I, LRA, PLR and the TP/SP holds to '-' or current values. G-05 said a right-click changes no value; that was true only on the knob body and thumb. On a fader track the right-click jumped Input Gain 0.0 to +17.6 dB, and a middle click did the same. Also newly confirmed: a right-click on Copy overwrote slot B with slot A (B's Loudness went 0 to 75 %, Comp Threshold 0.0 to -12.0 dB); right-clicks on the MATCH and FREEZE toggles turned them on and off; a right-click on the GR/SPEC chip switched GR to SPEC. A right-click on the Loudness knob with no movement re-engaged a detached Comp Threshold (-11.6 to 0.0 dB, the edited dot vanished), and Undo restored it. Combos correctly ignore the right button (JUCE ComboBox filters isPopupMenu), consistent with LAY-05 and G-12.

</details>

<sub>Verifier scores (1-5): impact 4 · frequency 3 · severity 4 · discoverability 4 · efficiency 3 · coherence 4 · change risk 3 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### INPUT-014

**Pop-up modality costs one extra click: every dismissing click is consumed; tooltips and combo hover art are suppressed while a list is open; the 'click outside the plugin window leaves the menu open' case is an Xvfb-only observation**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Preserve** | **none** | high | partially-confirmed | Interaction model | Pointer and text entry run on untuned JUCE defaults, so ordinary slips rewrite values | — |

**Evidence**

- e769f33:src/gui/PluginEditor.h:105-165 — PopupShield rationale incl. the two accepted regressions (no tooltips, combo hover art eases out) while raised
- e769f33:src/gui/PluginEditor.h:166-189 — PopupShield consumes mouseDown/Up/Drag/DoubleClick/Wheel/Magnify
- e769f33:src/gui/PluginEditor.h:92-96 — Backdrop::mouseDown dismisses on an outside press
- e769f33:src/gui/PluginEditor.cpp:1434-1441 — shield and backdrops cover the whole editor; :2496-2594 refreshPopupShield; :2275-2276, :2313-2319 raise-before-menu / lower-on-callback
- e769f33:docs/policies/DEPENDENCY_POLICY.md:144 — rule-7 register row: tooltip/hover suppression under the raised shield, by design
- runtime verify-5: session capture `rt/verify-5/30-sheet.png` (dismiss click on LOCK consumed, second click toggles)
- runtime verify-5: .../rt/verify-5/36a-small.png (root-window click under Xvfb/no WM leaves the preset menu open; focus stayed on the harness window)
- runtime verify-5: .../rt/verify-5/37-sheet.png (ST-21 not reproduced 3/3)
- observers: session capture `rt/edges/07b-settings-click-preset-t10-crop.png`, session capture `rt/edges/08-settings-dismiss-by-AB-click-crop.png`, session capture `rt/edges/16-preset-menu-click-on-MATCH.png`, session capture `rt/edges/22b-adv-combo-outside-click-on-TP-crop.png`, session capture `rt/gestures/32a-menu-click-outside.png`

**Current behaviour.** While any list or panel is open, the first click anywhere in the editor only dismisses it. The control under the pointer is not operated. A list inside Settings needs one click to close the list and another to close Settings. Hover tooltips and combo hover art are suppressed while a list is open.

**Problem.** Reaching a control after a menu or Settings takes two clicks, and a user may briefly think the first click failed. Apart from the double-click side effect ([INPUT-009](findings-input.md#input-009)), no state is changed or lost.

**Root cause.** A deliberate whole-editor click sink (PopupShield) plus modal backdrops. They enforce the INC-011 lesson that the click which dismisses a list must never operate the control beneath: A/B toggling, Alt-click resets, graph-well switching, and the Save Preset backdrop discarding a typed name. The suppression side effects follow from Component::getComponentAt resolving to the shield.

**User impact.** One extra click per menu/Settings exit that is followed by an action. It is cheap, visible and consistent, and matches how OS menus, drop-downs and modal lightboxes behave. *Scope:* Preset menu, all combo drop-downs, the Save Preset field's context menu, and the Settings/About/Save Preset panels, in both views and all formats.

**Proposed improvement.** Keep the consume-on-dismiss contract. Fix its only harmful consequence separately ([INPUT-009](findings-input.md#input-009): the absorbed click must not start a double-click). Give Settings/About Escape dismissal separately (E02). Obtain real-host evidence before treating the outside-window case as a defect: with the preset menu open in a DAW, click the host's arrange window on macOS, Windows and Linux, and record whether the menu closes.

**Alternatives considered.**

- *Click-through dismissal (dismiss and also operate the control beneath)* — Reintroduces the INC-011 defect class: A/B toggles, alt-resets, Save Preset name loss. Also inconsistent with platform menu conventions. Rejected.
- *Click-through only for 'safe' controls (e.g. toggles)* — Re-creates the per-control predicate approach the shield replaced, and a toggle flip is itself a state change. Rejected.
- *Keep as-is* — Safe, consistent, conventional; the recovery cost is one click. Preferred.

**Decision: Preserve · none.** The extra click is the intended, documented cost of a safety contract that fixed real work-destroying defects. It matches OS conventions and changes nothing by itself. The genuinely harmful side effect is handled in [INPUT-009](findings-input.md#input-009). The outside-window observation lacks real-host evidence, and the lost-click report (ST-21) did not reproduce.

**Dependencies.** [INPUT-009](findings-input.md#input-009) (double-click side effect of the absorbed click); E02 (Escape does not close Settings/About)

**Acceptance criteria.**

- (Preserve) With the preset menu, any combo list or Settings open, a single click on any control closes the pop-up and leaves that control's value/state unchanged (LOCK, A/B, MATCH, TP, knobs verified by dump)
- (Preserve) A second click after dismissal operates the control normally
- (Evidence needed before any change to the outside-window case) In at least one real DAW per platform, clicking the host window with the preset menu open is recorded as closing or not closing the menu

<details><summary>Verification record</summary>

**Method.** Code at e769f33:
- e769f33:src/gui/PluginEditor.h:105-189: PopupShield rationale; the accepted regressions are stated at :150-165.
- e769f33:src/gui/PluginEditor.h:92-96: Backdrop dismissal.
- e769f33:src/gui/PluginEditor.cpp:1434-1441: the shield and backdrops are sized to the editor.
- e769f33:src/gui/PluginEditor.cpp:2496-2594: raise/lower.
- e769f33:src/gui/PluginEditor.cpp:2275-2276 and :2313-2319: raised before the preset menu, lowered on its callback.
- DEPENDENCY_POLICY.md:144: rule-7 register row for the tooltip/hover regression.
Runtime on :135 with stepped moves:
- Preset menu open, a click on LOCK closed the menu and LOCK stayed off; a second click toggled it (30-sheet).
- The dismissing clicks on knobs in the [INPUT-009](findings-input.md#input-009) runs were consumed.
- With the preset menu open, a click on the root window below the editor left the menu open (36a).
- ST-21 retried 3 times (Escape-dismissed menu, then a click on the preset name): the menu opened each time (37-sheet).
Viewed the E03 crops cited.

**Corrections to the candidate claim.** The consumed dismissal and the shield's tooltip/hover suppression are confirmed and are deliberate (INC-011). The root-window case reproduces only in Xvfb with no window manager, where keyboard focus never leaves the harness window. JUCE's PopupMenu dismisses on focus loss, so real-host behaviour is unverified and it should not be counted as a product defect. ST-21's lost click did not reproduce (0/3) and is unconfirmed. One anomaly was not in the claim: in one run (37-rep3) a 'Presets' tooltip was visible while the preset menu was open, contradicting the stated suppression. It was not reproduced (38b showed no tooltip window) and may be an Xvfb repaint artefact.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 3 · severity 1 · discoverability 2 · efficiency 2 · coherence 1 · change risk 4 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### INPUT-015

**Each Settings toggle (UI Animations, Tooltips) spans the full 340 px row, so a click on the empty glass right of the label flips the preference**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P3** | high | recorded at triage | Interaction model | Pointer and text entry run on untuned JUCE defaults, so ordinary slips rewrite values | Phase 3 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:1453 — settingsBackdrop.panel = withSizeKeepingCentre (380, 350)
- e769f33:src/gui/PluginEditor.cpp:1457-1473 — sp = panel.reduced (20, 16) → 340 px rows; combo rows use splitRow (130 px label + control); animToggle.setBounds (row (26)); tooltipsToggle.setBounds (row (26))
- e769f33:src/gui/LookAndFeel.cpp:252-262 — the toggle draws a left-side switch and label; hover brightens only the switch and text
- e769f33:src/gui/PluginEditor.h:92-96 — Backdrop::mouseDown ignores clicks inside the panel
- Runtime (verify-9, display :139, stepped click): session capture `rt/verify-9/07c.png` (Tooltips off) → click at (645,585) → session capture `rt/verify-9/10-tp-off-4x.png` (Tooltips on)
- Anamorph@fd78c3b:src/PluginEditor.cpp:2223-2225 — the sibling's Settings toggles also span the row (family pattern)
- [INPUT-004](findings-input.md#input-004) — Settings has no close control, and only a click outside the panel dismisses it

**Current behaviour.** The two Settings toggles draw a small switch and a label at the left of their row, but each accepts clicks across the full 340x26 px row. With the pointer over the empty right part, the switch at the far left brightens. A click there flips Tooltips or UI Animations.

**Problem.** The hit area is about four times the painted control and has no visible extent. Settings has no close control ([INPUT-004](findings-input.md#input-004)), and only a click outside the panel dismisses it. A user who clicks in the panel's empty right half to click away can flip a preference instead.

**Root cause.** The toggles are laid out at the full row width from the row helper, while the combo rows split label and control. The sibling uses the same full-row layout, so the pattern was inherited.

**User impact.** Low. The result is visible (tips appear or disappear, animations change) and one click reverses it; no audio is affected. It is an avoidable surprise in the one overlay users often dismiss by clicking.

**Proposed improvement.** Constrain each toggle's bounds to its painted extent: switch width, gap, label text width measured in the LookAndFeel font, plus about 8 px of padding, left-aligned in the existing row. Row height (26 px), row positions and the 380x350 panel stay unchanged. Optionally land it together with [INPUT-004](findings-input.md#input-004)'s close control, so users have an explicit exit.

**Alternatives considered.**

- Leave as is for family parity. Harmless, but keeps an invisible target in the panel users click to leave.
- Keep the full-row target and draw a full-row hover wash so the target is visible. A common settings-list idiom, but a heavier visual change to the family panel.
- Move the switch to the right edge with the label on the left (settings-list style) and keep the full row. A layout change for two rows that diverges from the sibling.

**Decision: Proceed · P3.** Confirmed by code and a stepped runtime click. The fix is a bounds change on two message-thread components with no parameter, state or DSP impact, and it follows the split the combo rows already use. It diverges from the sibling's geometry, so it is recorded as a brand-checklist deviation candidate.

**Architecture gates.**

- No hard-stop category. Brand: Settings geometry is family chrome, so list the change as a deviation candidate.

**Dependencies.** [INPUT-004](findings-input.md#input-004) (with no close control, stray clicks inside the panel are more likely)

**Acceptance criteria.**

- A click inside the panel 40 px or more to the right of the 'Tooltips' or 'UI Animations' label text changes neither toggle and does not close Settings.
- Clicks on the switch or on the label text still toggle, and the hover highlight appears only over that extent.
- The panel size (380x350) and every row position are unchanged at XS through XL.

<details><summary>Verification record</summary>

What I re-read at e769f33:
- e769f33:src/gui/PluginEditor.cpp:1453: the Settings panel is 380x350.
- :1457-1473: the panel content is reduced by (20,16), so rows are 340 px wide. The combo rows split into a 130 px label and a control (splitRow :1461-1465), but animToggle and tooltipsToggle take the whole row (setBounds (row (26)), :1472-1473).
- e769f33:src/gui/LookAndFeel.cpp:252-262: drawToggleButton paints the switch and label at the left; hover brightens only the switch and text, but the clickable area is the whole bounds.
- e769f33:src/gui/PluginEditor.h:92-96: Backdrop::mouseDown dismisses only for clicks outside the panel, so a narrowed toggle would leave the empty area inert.
I viewed the discovering verifier's captures myself. session capture `rt/verify-9/07c.png` shows Tooltips off. After a stepped click at screen (645,585), about 260 px right of the 'Tooltips' label inside the panel, my crop of session capture `rt/verify-9/10-tp-off-4x.png` shows Tooltips on. The sibling uses the same full-row layout (Anamorph@fd78c3b:src/PluginEditor.cpp:2223-2225).

</details>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### INPUT-016

**Mouse-only controls have no accessible name, role or keyboard path: a screen reader or the keyboard cannot find or press the A/B pill, the edited dot or the GR|SPEC pill, and Undo/Redo announce '↺'/'↻'. This contradicts brief §8 and HANDOVER's 'names on every control'.**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | recorded at triage | Accessibility/input | Keyboard and assistive-technology operation is half-implemented | Phase 3 |

**Evidence**

- e769f33:src/gui/PluginEditor.h:192-201 — struct ABControl : juce::Component, SettableTooltipClient; mouseDown → onToggle; no title, focus or accessibility handler
- e769f33:src/gui/PluginEditor.h:512-521 — struct EditedDot : juce::Component, SettableTooltipClient; mouseDown → onClick (resetToMacro, PluginEditor.cpp:658)
- JUCE 9.0.1 build/_deps/juce-src/modules/juce_gui_basics/components/juce_Component.cpp:3307-3310 — the default AccessibilityHandler has role unspecified and no press action
- e769f33:src/gui/SpectrumView.cpp:146-159 and e769f33:src/gui/GrHistoryView.cpp:35 — the GR|SPEC pill is a hit rectangle handled in the view's mouseDown
- e769f33:src/gui/PluginEditor.cpp:334-335, :357-360 — Undo/Redo text is '↺'/'↻' with tooltips 'Undo'/'Redo' and no setTitle; JUCE juce_ButtonAccessibilityHandler.h:67-80: title = button text when empty, help = tooltip
- e769f33:src/gui/PluginEditor.cpp:313-316 — titleButton has empty text and no title (covered by [UX-022](findings-ux.md#ux-022))
- e769f33:src/gui/LoudnessMeterView.cpp:53-57, :67-70 — the STATISTICS reset has no name, no focus and no press action (covered by [UX-002](findings-ux.md#ux-002))
- e769f33:docs/DEVELOPMENT_BRIEF.md:171 — 'Accessibility: complete parameter and automation names, keyboard operability …'
- e769f33:docs/architecture/design-decisions/ADR-0009-code-reuse-from-anamorph.md:188-189 — 'Zero accessibility. Anamorph has none; … the copied GUI carries a gap to be filled at P5'
- e769f33:docs/HANDOVER.md:1337 — 'Accessibility names on every control'
- e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:85-88, :109 — the §D keyboard and name items are unchecked; the D Accessibility result is TODO

**Current behaviour.** Knobs, faders, combos and parameter toggles carry registry titles and accept focus. Several non-parameter controls do not:
- The A/B pill and the Simple edited dot are role-less components with no title, no focus and no press action. Assistive technology finds nothing to activate, and Tab skips them.
- The GR|SPEC pill is a region inside the graph view with the same limits.
- The wordmark/About button has an empty title ([UX-022](findings-ux.md#ux-022)).
- The STATISTICS reset is the whole panel, with no name ([UX-002](findings-ux.md#ux-002)).
- Undo and Redo are real buttons, but their accessible title is the arrow glyph; 'Undo' and 'Redo' exist only as help text.

**Problem.** A/B compare and reset-to-macro are core workflow actions, and neither can be reached without a mouse. Several controls are unnamed or named only by a glyph. The brief requires keyboard operability and complete names, ADR-0009 recorded that the copied GUI's accessibility gap would be closed at P5, and HANDOVER reports it closed.

**Root cause.** The P5 accessibility pass added titles through the setup helpers for parameter-bound widgets (knobs, toggles, combos). Controls built outside those helpers were never covered: the ported family ABControl, EditedDot, the in-view graph pill, the icon TextButtons and titleButton. No test checks that every interactive component has a name.

**User impact.** Screen-reader and keyboard-only users cannot A/B, re-engage the macros from Simple, or switch the graph well, and they hear '↺' for Undo. Mouse users are unaffected.

**Proposed improvement.**

1. Undo/Redo: setTitle ('Undo') and setTitle ('Redo'); keep the glyph text.
2. ABControl: override createAccessibilityHandler with role button, title 'A/B compare', value text 'A active' or 'B active', and a press action that calls onToggle. setWantsKeyboardFocus (true) and handle Space/Return in keyPressed.
3. EditedDot: role button, title 'Reset to macro', press action resetToMacro; focusable only while visible.
4. GR|SPEC pill: expose it as a button, either a small transparent child component over the existing hit rectangle or a handler on the view with a press action. Title 'Graph: GR history' or 'Graph: spectrum'.
5. titleButton and the STATISTICS reset: as proposed in [UX-022](findings-ux.md#ux-022) and [UX-002](findings-ux.md#ux-002).
6. Add a state test that walks every visible interactive component in both views and asserts a non-empty, non-glyph accessibility title, plus a press action for buttons.
7. Correct HANDOVER.md:1337 until that test passes, and fill checklist §D. Add the new stops to [INPUT-003](findings-input.md#input-003)'s traversal table and focus indicator.

**Alternatives considered.**

- Titles only, without press actions or focus. Cheap, but A/B and reset-to-macro stay unreachable without a mouse; not enough for brief §8.
- Replace ABControl and EditedDot with juce::Button subclasses. Focus, the press action and ButtonAccessibilityHandler come for free; a larger diff to family code copied under ADR-0009, though ADR-0009:188 already sanctions accessibility deltas.
- Defer until a Level-5 screen-reader pass. The code gap is already certain; that pass should verify the fix, not decide whether the controls are reachable.

**Decision: Proceed · P2.** Confirmed from code at every site. The brief (§8) and ADR-0009:188 make this the closing of a recorded gap, not a new feature. The changes are message-thread UI only: no parameter IDs, serialization, DSP or threading change. ABControl is family code, but ADR-0009 explicitly expects the accessibility delta, and nothing changes visually.

**Architecture gates.**

- No hard-stop category. Brand: ABControl is an ADR-0009 family copy; the accessibility delta is sanctioned by ADR-0009:188 and changes nothing visually.

**Dependencies.** [INPUT-003](findings-input.md#input-003) (focus indicator and explicit traversal; add the new stops); [UX-022](findings-ux.md#ux-022) (the wordmark's accessible title); [UX-002](findings-ux.md#ux-002) (the STATISTICS reset control and its name); [INPUT-013](findings-input.md#input-013) (mouse-button filtering in the same custom mouseDown handlers); [TEST-002](findings-doc-test.md#test-002) (a Level-5 pass with a real screen reader)

**Acceptance criteria.**

- undoButton's accessibility title is 'Undo' and redoButton's is 'Redo', and the glyphs still render.
- The A/B pill, the edited dot (while visible) and the GR|SPEC pill each expose AccessibilityRole::button, a non-empty title and a press action. Invoking the press action has the same effect as a left click: slot switch with duck, resetToMacro, graph flip.
- Tab reaches the A/B pill in both views, and the edited dot in Simple while it is shown; with focus on the A/B pill, Space switches the slot.
- A new state test enumerating every visible interactive component in Simple and Advanced finds none with an empty or single-glyph title.
- HANDOVER.md:1337 and BRAND_CONSISTENCY_CHECKLIST §D reflect the tested state.

<details><summary>Verification record</summary>

What I re-read at e769f33:
- e769f33:src/gui/PluginEditor.h:192-201: ABControl is a juce::Component with SettableTooltipClient, and its only interaction is mouseDown → onToggle. No title, no keyboard focus, no custom accessibility handler.
- e769f33:src/gui/PluginEditor.h:512-521: EditedDot has the same shape, with onClick → resetToMacro (wired at PluginEditor.cpp:656-658).
- JUCE 9.0.1 juce_Component.cpp:3307-3310: the default handler has role 'unspecified' and no press action.
- e769f33:src/gui/SpectrumView.cpp:146-159 and e769f33:src/gui/GrHistoryView.cpp:35: the GR|SPEC pill is a hit rectangle inside each view's mouseDown, not a child component.
- e769f33:src/gui/PluginEditor.cpp:334-335 and :357-360: Undo and Redo show the glyphs U+21BA/U+21BB, with tooltips 'Undo'/'Redo' and no setTitle. juce_ButtonAccessibilityHandler.h:67-80: the title falls back to the button text, and help comes from the tooltip.
- A grep of PluginEditor.cpp for setTitle finds calls only for sliders, combos, toggles and the UI Scale box (:834, :1151, :1207, :1226, :1268, :1288). tests/state_tests.cpp looks components up by title (:3903, :3948, :5977-6006) but asserts no coverage.
- Contract texts: DEVELOPMENT_BRIEF.md:171, ADR-0009:188-189, HANDOVER.md:1337, and BRAND_CONSISTENCY_CHECKLIST.md:85-88 and :109.
Not exercised with VoiceOver, NVDA or Orca. The announcements are inferred from JUCE's handler code.

</details>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### INPUT-017

**Opening a percent value editor and confirming without typing (Return, Tab or click-away) multiplies any value in (0, 1] % by 100: Loudness 0.5 % becomes 50 % and 1 % becomes 100 %, committed as a gesture**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | recorded at triage | Accessibility/input | Pointer and text entry run on untuned JUCE defaults, so ordinary slips rewrite values | Phase 3 |

**Evidence**

- e769f33:src/gui/LookAndFeel.cpp:796-817 — rawEditText: 'strip a trailing unit word', so '0.5 %' → '0.5'
- e769f33:src/gui/LookAndFeel.cpp:927-934 — ValueBox::editorShown sets the editor text to rawEditText(*s) and calls selectAll()
- JUCE 9.0.1 juce_Label.cpp:493-511 (textEditorReturnKeyPressed commits when updateFromTextEditorContents reports a change) and :524-527 (textEditorFocusLost → the same commit path)
- JUCE 9.0.1 juce_Slider.cpp:445-455 — textChanged: if the parsed value differs, ScopedDragNotification + setValue, i.e. a host gesture and an undo step
- e769f33:src/PluginParameters.cpp:142-147 — pctFrom: (! literal && v > 0 && v <= 1) ? v * 100 : v
- e769f33:src/PluginParameters.cpp:108-112 and :136-141 — pctText always appends ' %'; 'A formatter that dropped the suffix would multiply small values by 100 on every host round-trip, which is why the suffix is a contract'
- e769f33:tests/state_tests.cpp:5954-5962 — round-trip test covers getValueForText(getText(v)) only, not the editor's stripped text
- e769f33:src/PluginParameters.cpp:282, 327, 332, 342, 357, 375, 376 — the seven percent parameters: Loudness, Comp Mix, Comp Stereo Link, Clip Mix, Color Depth, Limiter Stereo Link, Transients
- e769f33:src/MacroEngine.h:51-54 — moving Loudness from 0.5 % to 50 % moves limGain from ≈0.03 to ≈7.8 dB, compThreshold from -0.1 to -10 dB, and clip drive from 0 to ≈2.6 dB
- Runtime verify-26 (:156): rt/verify-26/app.log l.977 → l.1029 (0.5 % → 50 % after double-click + Return), l.1083 → l.1135 (1 % → 100 % after click-away), l.1189 (0.3 % unchanged after Escape); screenshot session capture `rt/verify-26/10-12-sheet.png`
- [UI-002](findings-ui.md#ui-002) acceptance criterion 5 and [INPUT-008](findings-input.md#input-008)'s proposal both name this as a separate fix ('opening and confirming without typing must be a no-op')

**Current behaviour.** Double-clicking a percent readout opens the editor showing only the number ('0.5' for '0.5 %'). Return, Tab or a click elsewhere commits that text.
- Above 1 %, nothing changes, because the bare number parses literally.
- In (0, 1] %, the fraction rule reads the bare number as a fraction of full scale. The value is multiplied by 100 and committed as a host gesture with an undo step.
- On Loudness the commit is also a macro gesture: it re-lands the curve and re-engages every detached Advanced parameter ([MODEL-001](findings-state-model.md#model-001)).

Escape is safe.

**Problem.** An action that should be a no-op, opening the editor and leaving without changing anything, rewrites the value by a factor of 100. The product's own round-trip guarantee depends on the '%' suffix, and the editor removes it. The trigger includes an accidental double-click on a readout followed by any click elsewhere. No user expectation connects looking at a value with changing it.

**Root cause.** Two deliberate rules collide. The editor shows the raw number without its unit (#36, LookAndFeel.cpp:927-934). The 0.1.6 fraction rule (owner directive) reads a bare number in (0,1] as a fraction. The rule's safety argument covers only host round-trips (getText → getValueForText). JUCE's Label commits whenever the edited text differs from the label text, and the stripped text always differs. No test exercises the editor path.

**User impact.** It happens rarely, because values in (0, 1] % are uncommon, but the change is large when it does.
- Loudness 0.5 % → 50 % adds about 8 dB of limiter gain plus compression and clipping at once, and discards Advanced hand edits through the macro re-engage.
- Transients or Color Depth 1 % → 100 % changes the sound drastically.

The readout shows the new value and one Undo restores it, so recovery is cheap once the user notices.

**Proposed improvement.** Make opening and confirming the editor without an edit a no-op for every parameter.

Smallest route: ValueBox remembers the exact text it placed in the editor at editorShown. On Return, Tab or focus loss, if the editor text still equals that string, it restores the label and sends no change. This can be done by overriding Label::textEditorReturnKeyPressed / textEditorFocusLost, or by comparing in Knob::getValueFromText and returning getValue().

Alternative that also serves [UI-002](findings-ui.md#ui-002): keep the unit in what the parser receives, for example a visible non-editable suffix appended before parsing.

Keep the fraction rule ([INPUT-008](findings-input.md#input-008) Preserve): a user who actually types '0.5' still gets 50 %.

Add a state test that drives a real ValueBox/Slider. It opens the editor and confirms without typing for every parameter at representative values, including 0.1, 0.5 and 1 % on each percent parameter.

**Alternatives considered.**

- Drop the fraction rule. Rejected: [INPUT-008](findings-input.md#input-008) preserves it, and typed '0.5' → 50 % is the owner's example (0.1.6 item 2).
- Show '0.5%' with the unit in the editor text so the literal branch applies. This works, but reverses #36's raw-number design; acceptable if [UI-002](findings-ui.md#ui-002) adds a visible unit anyway.
- Make focus loss discard changes (Label lossOfFocusDiscardsChanges). This fixes only one of the three triggers and changes the family's commit-on-blur grammar.

**Decision: Proceed · P2.** This is a confirmed, reproducible, silent value change on a no-op action. The editor-side fix is small and has no parameter-ID, serialization, DSP or ADR impact, and [UI-002](findings-ui.md#ui-002) and [INPUT-008](findings-input.md#input-008) already call for it as a separate item. It is P2 rather than P1 because the precondition is uncommon and the result is visible and undoable. It is still a correctness defect in the value-entry path, not polish.

**Dependencies.** [UI-002](findings-ui.md#ui-002) (unit suffix in the editor; choose one mechanism for both); [INPUT-001](findings-input.md#input-001) (same editor/parser interception point, Knob::getValueFromText); [INPUT-008](findings-input.md#input-008) (the fraction rule stays as it is); [MODEL-001](findings-state-model.md#model-001) (a Loudness commit re-engages detached parameters)

**Acceptance criteria.**

- For each of the seven percent parameters at 0.1 %, 0.5 % and 1 %: double-click the readout, then press Return, press Tab, or click elsewhere. The value stays bit-identical, no gesture reaches the host, and the undo history gains no entry.
- Typing '0.5' + Return still gives 50 %, '0.5 %' gives 0.5 %, and testPercentTextEntryReadsFractionsAndLiteralPercents passes unchanged.
- With a detached Advanced parameter, opening and confirming the Loudness editor at 0.5 % leaves the detach mask and every managed value unchanged.
- A state test drives the real editor path (ValueBox showEditor, then confirm) for every parameter, not only getValueForText(getText(v)).
- Escape behaves as today.

<details><summary>Verification record</summary>

Re-read the code path.
- LookAndFeel.cpp:796-817: rawEditText strips a trailing unit word, so '0.5 %' becomes '0.5'.
- :927-934: editorShown puts that text in the editor and selects it.
- JUCE 9.0.1 juce_Label.cpp:493-511: Return commits when updateFromTextEditorContents reports a difference, and '0.5' always differs from the label's '0.5 %'. :524-527: focus loss takes the same path.
- juce_Slider.cpp:445-455: textChanged sets the parsed value inside a ScopedDragNotification whenever it differs from the current value.
- PluginParameters.cpp:142-147: pctFrom multiplies a bare value in (0,1] by 100.
- :108-112 and :136-141: pctText always appends ' %', and the round-trip guarantee is argued on that suffix.
- e769f33:tests/state_tests.cpp:5954-5962: the test checks only getValueForText(getText(v)).

Re-read the verify-26 app.log dumps.
- 'paramtext loudness 0.5 %' gives 'Loudness = 0.5 % [0.0050]' (l.977). After a double-click and Return it reads 'Loudness = 50 % [0.5000]' (l.1029).
- 1 % becomes 100 % [1.0000] after a click elsewhere (l.1083 → l.1135).
- 0.3 % with Escape stays 0.3 % (l.1189).

Tab as a commit trigger is the verifier's runtime observation. The detach and re-engage side effects on managed parameters follow from [MODEL-001](findings-state-model.md#model-001)'s evidence but were not run for this trigger.

</details>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

