# 2026-09-26 — Product / UX audit: runtime observation register

Session-local evidence trail for [`docs/reports/2026-09-26-anabasis-product-ux-audit.md`](../docs/reports/2026-09-26-anabasis-product-ux-audit.md). Everything here was observed on the real `AnabasisAudioProcessor` and its editor, compiled from commit `e769f33` (== `main` on the audit date), driven under Xvfb by synthetic input with a synthetic stereo signal. This file is raw evidence, not policy or decision: the report carries the verified findings, their decisions and the roadmap, and cites the observation ids below (`LAY-`, `G-`, `ST-`, `V-`, `E`).

## How the observations were made

- **Harness.** A scratchpad-only JUCE GUI application (not committed, not part of the product) that compiles the plugin target's own source list, hosts the processor and `createEditor()` in a window, calls `processBlock` from a simulated audio thread at real-time pace, and exposes a control file (`signal`, `level`, `transport`, `hostbypass` → `processBlockBypassed`, `sr` → `releaseResources` + `prepareToPlay`, `param`/`paramtext` → gestured host automation, `dump`, `save`/`load`/`savexml`, `quit`). Signal programs: silence, sine, pink, a 120 BPM kick + pad "music" loop with a louder section every other four bars, a 1 s loud / 2 s quiet pink burst, square, and a 20 Hz – 20 kHz sweep.
- **Displays and input.** One Xvfb display per observer (1600x1100x24, no window manager, no compositor); `xdotool` for pointer and keys; ImageMagick `import` for captures, cropped and inspected at 2-4x. The plain Standalone was also run on its own display (this machine has no audio device). It is a Release binary from the repository's existing build tree, whose sources differ from `e769f33` at most by the comment-only commit `e467e1d`.
- **Input fidelity.** `xdotool` warps the pointer instantly and clicks with zero-length presses. Where a behaviour appeared only under warp+click the observer says so; the verification phase of the audit re-ran those with stepped pointer motion before any finding was accepted.
- **Anchors.** Code references are pinned to `e769f33`; this directory is outside the citation gate and the anchors are not maintained.
- **Screenshots.** The audit left 2,652 PNG files in the session scratchpad, captures plus derived crops and sheets (1,597 before Phase 3; the number of distinct capture events was not established); they lived in the session scratchpad and are **not** retained except for the curated set committed beside the report. Paths below are kept as the observers recorded them, relative to the session scratchpad, so each observation still says which capture it was read from.

## Layout, hierarchy, scale and resize (18 observations)

**Observer summary.** Layout is disciplined and scales cleanly, but several core visuals are unlabelled or misleading. The orange knob arcs are a unipolar fill from the range minimum, so bipolar Tone/Tilt/gains at 0 read as half-on and Ceiling at -0.10 dB reads as maxed (LookAndFeel.cpp:79-81). Advanced mode drops every macro control with no on-screen trace, relocates Ceiling/TP/MATCH/DELTA/FREEZE, and jumps the window 720→822 px instantly. Each Advanced panel opens with a caption-less combo (RMS/Tape/Transparent/Pre); the COMP/LIMITER foot bars are per-stage GR meters and the CLIP/EQ lines are transfer/response curves, none with a label, scale, legend or tooltip, and their feet sit at four different heights. The graph well's GR and SPEC plots have no axes. The Statistics panel silently resets integrated/LRA/peak holds on a single click. UI Scale offers XS/S/M/L/XL (75–150 %) via a transform with fixed logical size — proportional and clip-free inside the editor (XS captions ~7 px are at the legibility floor; XL Advanced is 1410x1233 px) — but window-following is host-dependent and did not happen in the harness. There is no resize corner; a harness-border resize revealed a width-fluid, height-rigid layout. Minor: three labelling conventions and two baselines in the utility row, mixed dB precisions, EQ knobs 12 % smaller, tooltip text lagging one control behind on pointer jumps, Escape not closing Settings/About, a clipped minus sign in the Ceiling edit box, and a jump-to-click nudge on the Input Gain slider. 45+ captures under /tmp/claude-0/-home-user/52dd522c-a346-5a74-bfc9-56317277213a/scratchpad/rt/layout/.

### LAY-01 — Simple view hierarchy and dead space

*Severity (observer's estimate): low · confidence: high*

*Superseded in part by the audit's verification:* Verification measured the empty STATISTICS area as about 318 px — see UX-002.

- **Steps.** Launched harness (HARNESS_SIGNAL=music, -18 dB); import -window root after 7 s; cropped top bar, big knob, small knobs, stats, graph well.
- **Observed.** Editor 940x720 at (44,44). Primary: one 224 px Loudness knob centred at (360,240) with '0 %' / 'Loudness' caption below (360,380/396) — no tick ring, no scale. Secondary: three 76–78 px knobs Character (165,465), Tone (360,465), Ceiling (517,465), with TP (597,449) and LOCK (597,508) toggles beside Ceiling. Tertiary: one row y=558 of MATCH/DELTA/FREEZE toggle pills, a LEARN push button (345–421) and the 'out LUFS -22.1' readout (600,558). Right column: STATISTICS panel 684–975 x 100–628 whose lower ~310 px (y≈315–628) is empty. A ~70 px empty band separates the toggle row from the graph well (52–975 x 650–758, 108 px tall). About 80 px of unused width right of the knob block.
- **Interpretation.** The intended primary/secondary/tertiary reading works: the eye lands on Loudness, then the three macro knobs, then the toggle row. The cost is a half-empty Statistics panel and a slack band above the graph well; the right column reads as a placeholder rather than a designed panel. Neutral-to-positive on hierarchy, negative on space use.
- **Code.** e769f33:src/gui/PluginEditor.cpp:1701-1750 (layoutSimple: knobArea 250x300, macroRow 118 px, meterView 292 px wide, well 120 px)
- **Captures.** `rt/layout/01-simple-initial.png`, `rt/layout/01c-bigknob.png`, `rt/layout/01c-stats.png`, `rt/layout/46-simple-lower-left-1x.png`

### LAY-02 — Orange arc means 'distance above range minimum', so bipolar knobs at 0 look half-on

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Compared Loudness (0 %), Character (0.00), Tone (0.00), Ceiling (-0.10 dB) arcs in the initial Simple capture; checked Advanced Odd/Even, Color Tone, Tilt, EQ gains at 0.0 dB; read the draw routine.
- **Observed.** Character and Loudness (both at their minimum) show no arc; Tone at its centre value 0.00 shows an arc from 7 o'clock to 12 o'clock (half filled); Ceiling at -0.10 dB (range -20..0) shows a ~97 % arc. In Advanced every 0 dB / 0.00 bipolar knob (Odd/Even, Color Tone, Tilt, LS/HS/Bell gains) shows the same half arc, while unipolar knobs at 0 (Clip Drive, Color Depth, Dynamic Tame) show none. Hover adds only a faint glow (02c-hover-tone.png).
- **Interpretation.** Orange is neither 'non-default' nor 'active' nor 'bipolar centre' — it is a unipolar fill from the minimum for every rotary. A default preset therefore reads as 'Tone half on, Ceiling nearly maxed, Character off', which misrepresents neutral settings; bipolar controls have no centre-origin arc and no centre detent marker.
- **Code.** e769f33:src/gui/LookAndFeel.cpp:79-81 (value arc always startAngle→angle); e769f33:src/PluginParameters.cpp:282-303 (loudness 0..100, character 0..1, tone -1..1 default 0, ceiling -20..0 default -0.1)
- **Captures.** `rt/layout/01c-smallknobs.png`, `rt/layout/04c-eq.png`, `rt/layout/04c-clip.png`, `rt/layout/02c-hover-tone.png`

### LAY-03 — ADV toggle: macro layer vanishes entirely; Ceiling/TP/MATCH/DELTA/FREEZE relocate; window jumps 720→822 with no animation

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** xdotool click ADV toggle (838,67); captured at 150 ms, 400 ms, 1.4 s; read editor-bounds from app.log; repeated toggle 6 times with 80 ms frame bursts.
- **Observed.** Log: 940x720 → 940x822 (X window 948x728 → 948x830) on every toggle, instantaneous — the 150 ms frame already shows the final Advanced layout; 80 ms frame bursts show no intermediate sizes. In Advanced the Loudness, Character, Tone knobs, LEARN, 'out LUFS' readout and LOCK toggle all disappear; Ceiling reappears inside LIMITER at (682,176) with the TP toggle at (645,413); MATCH/DELTA/FREEZE move to the utility row right end (688/779/872, y=568); Statistics moves into the graph row (684–965 x 615–815). No macro control remains visible, so the macros cannot be operated from the Advanced view (host automation is also excluded: loudness/character/tone are not automatable per dump).
- **Interpretation.** A user who tweaks Loudness and opens Advanced loses sight of the macro state with no indicator that macros are still driving the stage parameters; returning to Simple shows them again. The 102 px window jump is abrupt but predictable; the relocation of Ceiling and the three toggles forces re-learning positions between views.
- **Code.** e769f33:src/gui/PluginEditor.cpp:1746-1780 (updateModeVisibility advOnly list), :1923-1931 (applyUiScale → setSize kSimpleH/kAdvancedH), e769f33:src/gui/PluginEditor.h:632-638 (kSimpleH 720, kAdvancedH 46+446+64+266)
- **Captures.** `rt/layout/04-advanced-initial.png`, `rt/layout/04a-adv-transition-150ms-c.png`, `rt/layout/07c-simple-after.png`

### LAY-04 — Graph well keeps its content and tab across ADV toggles; brief unpainted band once

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Selected SPEC in Advanced, toggled to Simple; selected GR in Advanced, waited 3.5 s, toggled to Simple and back capturing 0.35 s after each click.
- **Observed.** SPEC selection persisted into Simple (07c). GR history trace was continuous across Advanced→Simple→Advanced (34c-adv-gr-before / simple-after / adv-after), rescaled to the shorter 108 px Simple well. In one re-toggle capture the bottom ~100 px of the grown window was still black (unpainted) at 0.35 s (34c-adv-gr-after.png); earlier 80 ms bursts were fully painted.
- **Interpretation.** Positive: history and view choice survive the mode switch. The occasional black band is a sub-second paint lag on the newly exposed area, cosmetic.
- **Captures.** `rt/layout/34c-adv-gr-before.png`, `rt/layout/34c-simple-gr-after.png`, `rt/layout/34c-adv-gr-after.png`, `rt/layout/07c-simple-after.png`

### LAY-05 — Every Advanced panel opens with an unlabelled mode combo (RMS / Tape / Transparent / Pre)

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Cropped the four panels at 3x; enabled Tooltips in Settings and hovered each combo from rest for 1–4 s.
- **Observed.** Row one of each panel is a full-width combo with no caption: 'RMS' (COMP), 'Tape' (CLIP / COLOR), 'Transparent' (LIMITER), 'Pre' (EQ). With Tooltips ON (default OFF) hovering reveals: 'RMS averages the level, Peak follows every transient', 'The saturation voicing - Clean, Tape, Tube or Transistor', '…Transparent, Punchy or LoudTube…', 'EQ before the compressor or after the limiter - the Ceiling holds either way'. Right-click on a combo does nothing.
- **Interpretation.** 'Pre' and 'Tape' are meaningless without a caption (Detector / Model / Style / Position); with tooltips off by default the only cue is opening the combo. Consistent slot placement is good; the missing captions are not.
- **Code.** e769f33:src/gui/PluginEditor.cpp:527-562 (setupCombo detectorBox/modelBox/styleBox/eqPosBox with tipFor only), :1548-1560 (combo takes the first body row, no label)
- **Captures.** `rt/layout/04c-comp.png`, `rt/layout/04c-limiter.png`, `rt/layout/40c-tip-timing-a.png`, `rt/layout/39c-tips-sheet-a.png`

### LAY-06 — GR mini-meters and curves have no label, scale, legend or tooltip; feet of the four panels sit at different heights

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Cropped panel feet at 1x and 4x; hovered COMP bar (165,515), clip curve (398,500), EQ line (862,494) with Tooltips ON for 4 s each; read CurveView.h.
- **Observed.** COMP foot: a 204x10 px dark bar at y≈515 (two L/R lanes, right-anchored gold fill, 24 dB span per code) — unlabelled. LIMITER: same bar at y≈441, then ~80 px of empty panel below it. CLIP / COLOR: a thin gold diagonal (live transfer curve) 295–495 x 488–522 with no axes. EQ: a flat gold line at y=494 (magnitude response at 0 dB) with no 0 dB tick, frequency or dB scale. None of the four showed a tooltip after 4 s hover; right/left clicks do nothing.
- **Interpretation.** The bars are per-stage GR meters (code), but nothing on screen says 'GR', which direction fills, or how many dB; the curves have no reference frame. The header comment claims each meter is 'labelled by its own stage', which on screen means only the panel caption. The uneven foot line (COMP 515 / LIMITER 441 / CLIP 505 / EQ 494) breaks the four-panel rhythm.
- **Code.** e769f33:src/gui/CurveView.h:20-23, 74-119 (GrMiniMeter, two lanes, grSpanDb); e769f33:src/gui/CurveView.h:27-31 (Mode clipTransfer/eqResponse)
- **Captures.** `rt/layout/46-adv-panel-feet-1x.png`, `rt/layout/04c-comp-bottom-bar.png`, `rt/layout/04c-lim-bottom-bar.png`, `rt/layout/04c-clip-curve.png`, `rt/layout/04c-eq-bottom-line.png`, `rt/layout/40c-tip-timing-a.png`

### LAY-07 — Utility row mixes three labelling conventions and two baselines

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Cropped utility row 52–975 x 538–600 at 1x and 2x.
- **Observed.** Input Gain: slider 75–155 (y 562), value '0.0 dB' to the right at x≈204, caption 'Input Gain' below-left at (147,590). SC HPF: same pattern (value '20 Hz' at 388, caption at 333,590). Dither: combo 430–530 with caption 'Dither' below (529,590); SHAPE toggle (555,562) labelled inline. MATCH/DELTA/FREEZE pills sit at y=568, 6 px below the slider/combo centre line, with inline labels. Slider tracks are 80 px, thumbs 16 px.
- **Interpretation.** Captions-below for sliders/combo, inline for toggles, and values floating between slider and next caption make the row harder to scan than the knob panels; the toggles' lower baseline reads as misalignment.
- **Code.** e769f33:src/gui/PluginEditor.cpp:577 (TextBoxRight 62x14 for utility sliders) vs :1136 (TextBoxBelow 72x14 for knobs)
- **Captures.** `rt/layout/04c-utility.png`, `rt/layout/32-utility-1x.png`

### LAY-08 — Value formatting precision is inconsistent within and across panels

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Read value boxes in 01/04 captures.
- **Observed.** Simple: Character '0.00' (unitless), Tone '0.00', Loudness '0 %', Ceiling '-0.10 dB'. Advanced LIMITER: Gain '0.0 dB' beside Ceiling '-0.10 dB'; COMP Threshold '0.0 dB', Knee '6.0 dB'; Ratio '1.50:1'; Q '1.00'; Clip Shape '0.50'; Odd/Even '0.00'; Color Depth '0 %'; Clip Mix '100 %'; Attack '30.0 ms'; Bell 2 Freq '3.00 kHz' vs Bell 1 Freq '300 Hz'.
- **Interpretation.** Two dB precisions side by side (Gain 0.0 / Ceiling -0.10) and unitless 0.00 for Character/Tone/Odd-Even make the numbers look like different systems; the ceiling's two decimals are deliberate per code, the rest is unharmonised.
- **Code.** e769f33:src/PluginParameters.cpp:95-110, 297-303 (twoDecimalRange only for ceiling)
- **Captures.** `rt/layout/04c-limiter.png`, `rt/layout/01c-smallknobs.png`, `rt/layout/04c-eq.png`

### LAY-09 — UI Scale XS/S/M/L/XL scales by transform; editor bounds follow, harness window does not; XL Advanced exceeds 1080p

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Settings (777,67) → UI Scale combo (579,470) → picked XS, S, M, L, XL (menu items read from captured popup; helper pickscale.sh); read editor-bounds after each; restored M by `load state-M.bin`; also XS + ADV.
- **Observed.** Combo lists XS, S, M, L, XL (current marked). Logged editor bounds: XS 705x540, S 800x613, M 940x720, L 1175x900, XL 1410x1080 (Simple); XS Advanced 705x617. Origin shifts 44→43/45/46 with scale. The harness X window stayed 948x728 in every case: at XS/S the UI sits top-left with black to the right/below; at L the right cluster is cut at 'Settings' and the lower rows are off-window; at XL only the top-left quarter is visible. Inside the editor nothing clips or overlaps at any step; popup menus and the Settings panel scale with it (menu 158x91 at XS, 210x121 at M, 262x151 at L). At XS captions are ~7 px (EQ 'Bell 1 Gain', utility 'SC HPF') — readable in a crop, marginal on screen; value text (~8 px) legible.
- **Interpretation.** Scaling is proportional and clean inside the editor (positive). Whether the plug-in window follows is host-dependent because the editor keeps a fixed logical 940x720/822 and only changes its transform — the harness DocumentWindow does not follow, so in this environment the user lands on a clipped or letter-boxed window. XL Advanced would be 1410x1233 px, taller than a 1080-line display. XS captions are at the legibility floor.
- **Code.** e769f33:src/InternalState.h:66-67 (steps 75/85/100/125/150, names XS..XL); e769f33:src/gui/PluginEditor.cpp:1923-1931 (setSize logical + setTransform scale)
- **Captures.** `rt/layout/13c-uiscale-menu.png`, `rt/layout/14c-XS.png`, `rt/layout/15c-XS.png`, `rt/layout/15c-XS-stats.png`, `rt/layout/19c-M.png`, `rt/layout/29c-L.png`, `rt/layout/36c-XL.png`, `rt/layout/36c-XL-lower-1x.png`, `rt/layout/45c-XS-advanced.png`, `rt/layout/45c-XS-eq-labels-1x.png`, `rt/layout/45c-XS-utility-1x.png`

### LAY-10 — No resize corner on the editor; harness-border resize shows a half-fluid layout

*Severity (observer's estimate): low · confidence: high*

- **Steps.** At M: mousedown at editor bottom-right inside (1196,896 with editor at 262,182) dragged to (1400,1050); then dragged the harness window's own border corner +150,+120; then windowsize back to 948x728.
- **Observed.** Editor-corner drag: no cursor change captured, bounds unchanged 940x720 (24c.png). Harness border drag: window 1068x824, editor re-laid out at 1060x816 (25c.png): top bar and Statistics/graph well stretch, the left column re-centres, but the extra 96 px of height becomes an empty band under the graph well. An earlier drag started in the harness's blank margin (at S scale) moved the window instead. No aspect lock, no min/max limits beyond the window's own. No setResizable/constrainer in the editor.
- **Interpretation.** The plug-in is fixed-size by design (scale via Settings only); a real host will not offer a corner. The window-border path is a harness artefact, but it reveals that resized() is width-fluid and height-rigid — worth knowing if free resize is ever added.
- **Code.** e769f33:src/gui/PluginEditor.cpp:1377-1470 (resized: right-anchored bar, fixed kSimpleH-based rows)
- **Captures.** `rt/layout/24c.png`, `rt/layout/25c.png`, `rt/layout/23-hover-editor-corner.png`

### LAY-11 — GR|SPEC switch is a 36x14 px chip pair inside the graph well

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Clicked GR (78,745) and SPEC (115,745) in Simple, (78,847)/(115,847) in Advanced; cropped at 1x and 4x.
- **Observed.** Pill at the well's bottom-left: GR chip ≈62–96 x 738–752, SPEC chip ≈98–134 x 738–752 (Simple; y 840–854 in Advanced), 8 px caps text; selected chip gold with dark text, unselected grey on dark. The pill overlaps the drawing area (the grey level history runs behind it). Selection persisted across ADV toggles, scale changes and state load.
- **Interpretation.** Small but findable; the gold/grey states are clear. Target height of 14 px is under common touch/precision guidance and it sits over live graph content.
- **Captures.** `rt/layout/01c-tabs.png`, `rt/layout/05c-adv-spec-tabs.png`, `rt/layout/08c-simple-gr.png`, `rt/layout/09c-simple-spec.png`, `rt/layout/05c-adv-spec.png`, `rt/layout/32-tabs-1x.png`

### LAY-12 — Graph well plots carry no axes, units or legend

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Viewed GR and SPEC in both modes at 1x.
- **Observed.** GR view: a flat orange line along the top edge (0 dB reduction with the -18 dB music) plus a grey filled level history hugging the bottom — no dB scale, no time scale, no legend saying which is which. SPEC view: orange curve with a fainter grey trace behind it — no frequency or dB axis.
- **Interpretation.** Readable as 'something is happening' only; the user cannot tell 3 dB from 12 dB of reduction or where 1 kHz is.
- **Captures.** `rt/layout/01c-graphwell.png`, `rt/layout/34c-adv-gr-before.png`, `rt/layout/05c-adv-spec.png`

### LAY-13 — Hidden affordances: click on Statistics resets integrated/LRA/peak holds with no cue; wordmark opens About; preset name opens on right-click too; double-click edits values

*Severity (observer's estimate): medium · confidence: high*

*Superseded in part by the audit's verification:* Verification found item (e) wrong: a right-click on A/B toggles the slot and a right-click on STATISTICS resets it — see INPUT-013 and UX-002.

- **Steps.** Left/right-clicked wordmark (125,67), subtitle, preset name (481,67), graph well, Statistics (830,300/400), big knob, A/B, value labels; double-clicked Ceiling value (517,518), Ratio value (112,212), Input Gain value (204,562); pressed Escape / clicked outside after each.
- **Observed.** (a) Wordmark left OR right click opens an in-editor About panel (440x290, centred); Escape leaves it open, a click outside closes it. (b) Preset name left OR right click opens an in-editor FACTORY list (13 presets + Save Preset… / Load Preset…), 227x288 under the name. (c) A single left click anywhere on the Statistics panel instantly reset I, LRA, PLR to '-' and TP/SP holds to the current value — no button, no confirmation, no hover cue (Tooltips OFF by default). (d) Double-click on any value readout opens an inline text editor; for Ceiling the 72x14 box showed '0.10' with the leading '-' scrolled out of view. (e) Right-click on knobs, graph well, Statistics, panel headers, combos, GR bars, curves, A/B: nothing. (f) A single click on the Input Gain slider thumb moved it 0.0 → 0.3 dB (jump-to-click); a click on its value box did not.
- **Interpretation.** (c) is the one that bites: a stray click on a display-looking panel silently discards a long integrated measurement. (a)/(b)/(d) are useful but undiscoverable without tooltips. (d)'s clipped minus sign can mislead an edit. (f) is default JUCE behaviour but nudges a mastering input gain on a grab.
- **Code.** e769f33:src/gui/LoudnessMeterView.cpp:67-70 (mouseDown → requestMeterReset); e769f33:src/gui/PluginEditor.cpp:315 (titleButton.onClick → showAbout); :1136 (TextBoxBelow 72x14)
- **Captures.** `rt/layout/26c-about.png`, `rt/layout/27-logo-after-escape-c.png`, `rt/layout/27-logo-after-outside-click-c.png`, `rt/layout/26c-preset-menu.png`, `rt/layout/28-stats-before-c.png`, `rt/layout/28-stats-after-c.png`, `rt/layout/26c-dblclick-ceilval.png`, `rt/layout/35c-ratio-dblclick.png`, `rt/layout/35c-inputgain-dblclick.png`, `rt/layout/35c-final.png`

### LAY-14 — Tooltip text lags one control behind after an instantaneous pointer jump

*Severity (observer's estimate): low · confidence: medium*

- **Steps.** Tooltips ON; rested pointer 3 s off-UI; xdotool jump to Knee, capture at 1/2/3 s; jump to Mix, capture 1/2/3 s; jump to Attack, 1/2/3 s; then a stepped 5-point move to Release, 1/2/3 s. Repeated the pattern earlier across 17 controls.
- **Observed.** From rest the correct tip appears within 1 s (Knee: 'Width of the soft knee…'; RMS combo: 'RMS averages the level…'). After a jump to Mix the capsule relocated beside Mix but still read the Knee text for all 3 s; after a jump to Attack it read Mix's text ('Parallel compression…'). After the stepped move to Release it read the correct 'How quickly the compressor recovers'. The earlier 17-control sweep showed the same one-behind pattern throughout.
- **Interpretation.** With a synthetic teleport the visible tooltip describes the wrong control for as long as the pointer rests. A real mouse generates intermediate motion, which produced the right text, so real-world exposure is uncertain (fast flicks between adjacent knobs may still trigger it). Flagged for the interaction observer.
- **Code.** e769f33:src/gui/PluginEditor.h:435-447 (GatedTooltipWindow), e769f33:src/gui/PluginEditor.cpp:2840-2848 (600 ms delay)
- **Captures.** `rt/layout/41c-tip-lag-a.png`, `rt/layout/41c-tip-lag-b.png`, `rt/layout/39c-tips-sheet-a.png`, `rt/layout/39c-tips-sheet-b.png`

### LAY-15 — Advanced grid: consistent pitch but EQ knobs are 12 % smaller and panels are fixed-height with dead space

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Measured knob diameters by pixel scan on the 04 capture; cropped panels at 3x.
- **Observed.** Four panels 218 px wide, 8 px gutters, y 98–528 (fixed 446-row). Knob diameters: COMP/CLIP/LIMITER 46–48 px (two columns), EQ 41–42 px (three columns); captions ~10 px grey, values ~11 px white, 13 px caption band. LIMITER has three knob rows + toggles + GR bar then ~80 px empty; EQ has an intentional empty cell (row 4 col 1); CLIP's curve fills its foot; COMP's AUTO toggle sits alone at the foot, far from the Release knob it governs.
- **Interpretation.** Column pitch and caption style are uniform (positive); the smaller EQ knobs and the LIMITER's empty foot read as uneven density; AUTO's distance from Release weakens grouping. Labels are legible at 1x.
- **Code.** e769f33:src/gui/PluginEditor.cpp:1474-1520 (layoutAdvanced panelW, placeRow cellH 84, grid spacer)
- **Captures.** `rt/layout/04-advanced-initial.png`, `rt/layout/04c-comp.png`, `rt/layout/04c-eq.png`, `rt/layout/32-eq-labels-1x.png`

### LAY-16 — Single accent hue carries every meaning

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Reviewed colour use across both views.
- **Observed.** The amber→gold gradient is used for: knob value arcs, toggles ON (ADV, AUTO, UI Animations), the selected GR/SPEC chip, M/S/I meter bars, the GR line, the spectrum trace, both curves, the wordmark subtitle, the active A/B letter, the Input Gain slider fill. Everything else is grey on near-black. No colour distinguishes measurement from control, or warning from normal.
- **Interpretation.** Visually coherent and calm; but 'orange' cannot signal anything (over-ceiling, clipping, learn active) because it already means 'value'. Neutral finding for brand, a constraint for status design.
- **Captures.** `rt/layout/04-advanced-initial.png`, `rt/layout/01c-topbar.png`, `rt/layout/01c-stats.png`

### LAY-17 — Component inventory as observed (screen coordinates, editor at 44,44, scale M)

*Severity (observer's estimate): none · confidence: high*

- **Steps.** Read from the 01 (Simple) and 04 (Advanced) captures and crops.
- **Observed.** SIMPLE 940x720 — Top bar y=67: wordmark 60–190 (About button), subtitle 205–360, ‹ 408, preset 'Default' 452–510, › 553, A/B 570–608, Copy 620–662, undo 675–700, redo 706–732 (icon only), Settings 742–812, ADV pill 825–852 + label, BYPASS pill 897–923 + label. Loudness knob 248–471 x 128–352, value (360,380), caption (360,396). Character (165,465) 76 px, Tone (360,465) 77 px, Ceiling (517,465) 78 px, values y=518, captions y=531. TP pill 580–608 y 449 label 617–630; LOCK pill y 508 label 617–645. Row y=558: MATCH pill 70–100 label 110–150; DELTA 166–196/207–241; FREEZE 254–284/293–336; LEARN button 345–421 x 545–571; 'out LUFS' 531–574, value 585–616. STATISTICS 684–975 x 100–628: title (697,115); M/S/I rows y 138/164/190 (label x697, value x730–760, bar 767–963); TP/SP/RMS/LRA/PLR y 220/240/260/280/300. Graph well 52–975 x 650–758; GR chip 62–96 x 738–752, SPEC 98–134. ADVANCED 940x822 — panels COMP 52–276, CLIP/COLOR 284–510, LIMITER 518–742, EQ 750–975 (y 98–528), captions y=106, combos y=136 centred 165/398/630/862. COMP: Ratio (112,176) Threshold (216,176) Attack (112,260) Release (216,260) Knee (112,343) Mix (216,343) Stereo Link (165,425) AUTO pill (79,487) GR bar 62–266 x 510–520. CLIP: Clip Shape (345,176) Clip Drive (448,176) Clip Mix (345,260) Color Depth (448,260) Odd/Even (345,343) Color Tone (448,343) Dynamic Tame (398,425) curve 295–495 x 488–522. LIMITER: Gain (578,176) Ceiling (682,176) Lookahead (578,260) Release (682,260) Stereo Link (578,343) Transients (682,343) AUTO pill (545,413) TP pill (645,413) GR bar 528–733 x 436–446. EQ: Tilt (792,172) LS Freq (862,172) LS Gain (931,172); Bell 1 Q/Freq/Gain (792/862/931,250); Bell 2 Q/Freq/Gain (792/862/931,328); HS Freq (862,406) HS Gain (931,406); response line 765–962 y 494. Utility 52–975 x 540–596: Input Gain slider 75–155 y562 value x204 caption (147,590); SC HPF slider 258–338 value x388 caption (333,590); Dither combo 430–530 caption (529,590); SHAPE pill 545–573 label 583–620; MATCH pill 674–702/712–750, DELTA 766–794/803–838, FREEZE 858–886/896–940 (y 568). Graph row 52–975 x 606–862: plot 60–665; STATISTICS 684–965 x 615–815 (rows y 646/672/698; 728/748/768/788/808); GR chip 62–96 x 840–854, SPEC 98–134.
- **Interpretation.** Reference map for the other observers; neutral.
- **Code.** e769f33:src/gui/PluginEditor.cpp:1377-1470 (bar), 1474-1700 (layoutAdvanced), 1701-1750 (layoutSimple)
- **Captures.** `rt/layout/01-simple-initial.png`, `rt/layout/04-advanced-initial.png`, `rt/layout/01c-topbar.png`, `rt/layout/04c-utility.png`, `rt/layout/04c-graph-stats.png`

### LAY-18 — Settings popup: Escape does not dismiss; the dismissing click is consumed (re-checked)

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Opened Settings, pressed Escape (in About test and implicitly during scale runs), dismissed by clicking on the Knee knob position (104,344) in Advanced several times.
- **Observed.** Escape left the About panel and Settings panel open; the click used to dismiss Settings landed on the Knee knob's position but Knee stayed 6.0 dB every time (35c-final, 43c-final). The popup itself (380x350, centred, 6 combo rows + 2 toggles) is well aligned at every UI scale.
- **Interpretation.** Consistent with the recipe's prior note; the consumed click is the safe choice, the missing Escape is a small keyboard gap.
- **Code.** e769f33:src/gui/PluginEditor.cpp:1436-1445 (popupShield covers whole editor)
- **Captures.** `rt/layout/12c-settings.png`, `rt/layout/27-logo-after-escape-c.png`, `rt/layout/42c-settings-tooltips-off.png`

**Positives recorded.**

- Primary/secondary/tertiary hierarchy in Simple reads correctly at a glance: one dominant Loudness knob, three macro knobs, one tertiary toggle row.
- No text truncation, overlap or clipping found inside the editor at M in either view, nor at any UI-scale step (XS–XL); popup menus and the Settings panel scale with the editor.
- Advanced panels share one grid pitch, one caption style and one combo slot per panel; full-width combos avoid the earlier 'Transpar…' truncation.
- GR|SPEC selection, GR history and settings survive ADV toggles, UI-scale changes and a state load; the mode switch is instantaneous and deterministic (720/822 px).
- Every value readout is editable by double-click and Escape cancels; the About and preset list are in-editor overlays that work under xvfb.
- UI scale implementation clamps illegal persisted percents and keeps label and transform in one ladder (InternalState.h).
- Panel gutters (8 px), 9 px letter-spaced captions and the single amber→gold accent give a calm, coherent look with good contrast at 1x.

**Limitations recorded.**

- Whether the plug-in window follows the UI-scale transform could not be judged for real hosts: the harness DocumentWindow sizes from the editor's logical bounds and never followed, so L/XL captures are clipped by the 948x728 harness window; JUCE VST3/standalone wrappers size from getLocalArea and should follow, but this was not observed.
- No host resize corner exists to test; the window-border resize (LAY-10) is a harness-only path, so 'limits' and 'aspect lock' under a host could not be observed.
- Host-driven setScaleFactor / HiDPI path not exercised (hostScale stayed 1.0).
- Tooltip lag (LAY-14) was reproduced only with xdotool teleports; real pointer motion may not trigger it.
- Xvfb has no compositor: tooltip corner fill, font hinting and anti-aliasing may differ from macOS/Windows rendering.
- Bypass dimming, LEARN, MATCH/DELTA/FREEZE behaviour and preset actions were not exercised (other observers' areas).

## Direct manipulation: knobs, faders, toggles, value entry, keyboard, tooltips (22 observations)

**Observer summary.** Gesture grammar is JUCE-stock and consistent across the knobs: vertical drag at ~0.36 % of range per px (100 px = 36 % of range, i.e. ~278 px full travel), ctrl-drag = JUCE velocity mode (~20:1 finer, and the readout gains a decimal), shift-drag does NOTHING, alt-click and double-click reset to default (alt-press-and-drag is inert), wheel = ~2.9 % of range per notch and produces untidy values (52.9 %, -9.41 dB), Up/Down/Left/Right = 1 % of range (Ceiling: 0.01 dB = 2000 steps), no PageUp/Home/End, no right-click menu anywhere, no visible keyboard-focus indicator, and value entry only via DOUBLE-click on the number (single click is inert and nothing hints at it). Text entry is permissive (units, 'k', fraction rule 0.5 -> 50 % and 1 -> 100 % on percent boxes) but any unparsable, empty or comma-decimal string silently commits 0, which on the Ceiling means 0.00 dB, its loudest end. The Input Gain and SC HPF faders are ~75 px wide for 36 dB / 20-300 Hz and jump to the click position (an accidental click = up to +17 dB). Tooltips (once enabled in Settings) are terse, well written and identical on twins, but never mention gestures. The Loudness macro moves Comp Ratio/Threshold, Clip Shape/Drive, Dynamic Tame and Limiter Gain; hand-edited managed knobs get a 5-px corner dot in Advanced and a clickable dot beside the big knob in Simple, and they SNAP BACK to the macro curve on the next Loudness gesture - nothing stays detached. LOCK is a ceiling-only preset lock (no other control is locked, no visual change), TP re-labels the Ceiling unit dB -> dBTP in both views, LEARN is latching (5-s countdown, then orange LEARN until clicked again) and runs while FREEZE is on with no warning. Readout formats are mixed (0 % vs 60.8 % vs 0.00 vs -0.10 dB vs 0.0 dB vs -0.0 dB vs unit-less out LUFS).

### G-01 — Tooltips are OFF by default; Escape does not close Settings, the dismissing click is consumed

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Click Settings (777,67); click Tooltips toggle (362,536); press Escape; click empty area (150,300).
- **Observed.** Settings popup opened with UI Animations ON and Tooltips OFF. After the click the Tooltips switch turned orange. Escape left the popup open (03-after-escape). The outside click closed it and did nothing else.
- **Interpretation.** A first-time user gets no hover help unless they find the Settings switch; Escape not closing a modal popup is a small friction confirmed twice (recipe + here).
- **Code.** e769f33:src/gui/PluginEditor.cpp:861-863,972 (tooltipsOn default false)
- **Captures.** `rt/gestures/01-settings-open.png`, `rt/gestures/02-tooltips-on-crop.png`, `rt/gestures/03-after-escape-small.png`, `rt/gestures/04-settings-closed-small.png`

### G-02 — Tooltip texts (verbatim) for the audited controls

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Hover each control 1.8 s with stepped pointer moves: Loudness (360,240), Character (165,465), Tone (360,465), Ceiling (517,465), TP (597,449), LOCK (597,508), LEARN (383,558), FREEZE (270,558), Advanced Comp Threshold (215,175), Input Gain track (130,562) and value (204,562), SC HPF (300,562), Dither combo (481,562), Detector combo (165,136), SHAPE (557,562), Limiter AUTO (545,413).
- **Observed.** Loudness: 'How hard the adaptive chain pushes - 0 adds no push, but the Ceiling still holds'. Character: 'Clean to Color - how much of the push comes from saturation rather than clean limiting'. Tone: 'Dark to bright tilt of the overall result'. Ceiling: 'The output limit - nothing leaves the plugin above it. Sample peak by default; engage TP to hold it in dBTP'. TP: 'Catch inter-sample peaks - the Ceiling then holds in dBTP instead of sample peak'. LOCK: no tooltip captured on two hovers (source has 'Keep the Ceiling where it is while you browse presets'). LEARN: 'Play the loudest section and Learn measures it as the adaptive reference - click again to stop'. FREEZE: 'Hold the adaptive trims exactly where they are now'. Comp Threshold: 'Where the glue compressor starts to work'. Input Gain (track and value box): 'Level into the chain, before any processing'. SC HPF: 'Sidechain high-pass for the compressor's detector - keeps low end from pumping the compressor. The limiter always hears the true peak'. Dither: 'Bit-depth dither for the final export - Off, 16-bit or 24-bit TPDF'. Detector: 'RMS averages the level, Peak follows every transient'. SHAPE: 'Shape the dither noise away from where the ear is most sensitive'. Lim AUTO: 'Programme-dependent release - two stages follow the material'. Tooltips appear ~0.6 s after hover, below-right of the pointer, and routinely cover the value readout under the hovered knob or the neighbouring knob's readout (09-tip-tone covers the Ceiling value; 08c covers '-0.10 dB').
- **Interpretation.** Mostly informative for a first-time user (what the control does, in plain words); Comp Threshold's is thin (no reference/unit hint). None of the tips mention any gesture (double-click to type, alt-click to reset, ctrl-drag for fine), so the entry and reset gestures remain undiscoverable. The tip covering the readout it describes is a minor placement issue.
- **Code.** e769f33:src/gui/PluginEditor.cpp:32-89 (tipFor table), :90-98 (badge legend deliberately removed from tips), :609-612 (LOCK tip), :641-642 (LEARN tip), :2847 (600 ms delay)
- **Captures.** `rt/gestures/06-tip-loudness-repeat-crop.png`, `rt/gestures/08a-tip-character-crop.png`, `rt/gestures/09-tip-tone-crop.png`, `rt/gestures/08c-tip-smooth-to-ceiling-crop.png`, `rt/gestures/21-sheet.png`, `rt/gestures/21d-tip-schpf-crop.png`, `rt/gestures/33-sheet.png`, `rt/gestures/34a-learn-tip.png`, `rt/gestures/35c-freeze-tip.png`

### G-03 — Drag sensitivity: ~278 px for full travel on every knob; shift = no effect; ctrl = 20:1 velocity mode; alt = reset + inert drag

*Severity (observer's estimate): medium · confidence: high*

*Superseded in part by the audit's verification:* Verification showed the 20:1 figure is an artefact of this observation's 10-px-per-event motion: Ctrl switches to JUCE's velocity mode, whose movement depends on pointer speed — see INPUT-007 in the report.

- **Steps.** Loudness knob (360,240): set 50 % via ctl, drag down 100 px in 10 steps with no modifier, then repeat from 50 % with shift, ctrl, alt, super held; dump after each. Same 100-px drags on Ceiling (517,465), Character (165,465), Tone (360,465), Comp Threshold (215,175).
- **Observed.** Plain 100 px: Loudness 50 -> 14 % (36 % of range); Ceiling -10.00 -> -2.80 dB (7.2 dB); Character 0.50 -> 0.86; Tone 0.00 -> 0.72; Comp Threshold -20.0 -> -5.6 dB (14.4 dB). Shift 100 px: identical to plain (50 -> 14 %). Ctrl 100 px: 50 -> 48.2 % (1.8 %), Ceiling -10.00 -> -9.64 (0.36 dB), Comp Thr -20.0 -> -19.3 - ratio about 20:1 and the readout switches to one decimal ('48.2 %'). Alt: value jumped to the default (0 %) on press and the 100-px drag changed nothing. Super: same as plain. A 30-px Loudness drag gave '60.8 %' (fractional percent readout).
- **Interpretation.** Sensitivity 0.36 % of range per px is uniform (JUCE default 250-px extent, measured ~278 with the step pattern). Shift, the modifier most plugin users try first for fine control, does nothing; the working fine modifier (ctrl) is JUCE's velocity mode and is not hinted anywhere. Alt-drag being inert after the reset is deliberate per source but surprising.
- **Code.** e769f33:src/gui/PluginEditor.cpp:1129 (RotaryVerticalDrag, no setMouseDragSensitivity/setVelocityModeParameters); e769f33:src/gui/PluginEditor.h:213-236 (alt-click reset without Slider::mouseDown); JUCE juce_Slider.cpp:1184,1326,1337 (velocity mode on ctrl/alt/cmd, 250 px extent)
- **Captures.** `rt/gestures/10a-loud-drag-during-crop.png`, `rt/gestures/11-loud-after-moddrags.png`, `rt/gestures/27i-simple-after-loudness-drag.png`

### G-04 — Mouse wheel: ~2.9 % of range per notch, up = increase, modifiers ignored, leaves untidy values

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Set 50 % via ctl; hover Loudness knob; wheel up x1, x2, down x3, ctrl+wheel up, shift+wheel up; same on Ceiling, Character, Tone, Comp Threshold, Input Gain (thumb and value text), SC HPF.
- **Observed.** Loudness 50 -> 52.9 -> 55.9 -> 47.1 % (2.93 % per notch); ctrl+wheel and shift+wheel identical steps. Ceiling -10.00 -> -9.41 dB (0.59 dB/notch). Character 0.50 -> 0.53; Tone 0.00 -> -0.06 per down-notch; Comp Threshold -20.0 -> -18.8 dB; Input Gain 0.0 -> 1.1 dB (also works over the value text); SC HPF 77 -> 84 Hz. Dither combo: wheel does nothing.
- **Interpretation.** Wheel works but steps are arbitrary fractions of range so a percent knob reads 52.9 % after one notch; there is no fine wheel modifier. Neutral-to-low; a musically meaningful step (1 %, 0.1 dB, 1 dB) would read better.
- **Captures.** `rt/gestures/18-tone-wheel.png`

### G-05 — Double-click resets to default; right-click does nothing; single-click on the value text does nothing

*Severity (observer's estimate): medium · confidence: high*

*Superseded in part by the audit's verification:* Verification found this true only on the knob body and thumb: a right-click on a fader track jumps the value, and a right-click without movement on a macro knob re-engages detached parameters — see INPUT-013.

- **Steps.** Set 50 %; double-click the Loudness knob (360,240) with 80 ms gap; right-click it; single-click the '50 %' text (360,380), type 75, Return. Repeat double-click/right-click on Ceiling, Character, Tone, Comp Threshold, Input Gain thumb, SC HPF thumb, Dither combo.
- **Observed.** Double-click: Loudness -> 0 %, Ceiling -> -0.10 dB, Character -> 0.00, Tone (from 0.50) -> 0.00, Comp Threshold -> 0.0 dB, Input Gain -> 0.0 dB, SC HPF -> 20 Hz. Right-click: no menu, no value change on any control (13-loud-rightclick, 22a, 23d). Single click on the value text: no editor, no caret, typed '75'+Return changed nothing (14a-14d).
- **Interpretation.** Reset gesture is solid. Value entry is only reachable by double-clicking the number (G-06); a single click, the gesture most users try, is silent, so typing values is undiscoverable. No context menu means no MIDI-learn/automation/reset affordance for right-clickers.
- **Code.** e769f33:src/gui/LookAndFeel.cpp:951 (setEditable(false, editable, false) = double-click only); e769f33:src/gui/PluginEditor.h:237-271 (double-click reset)
- **Captures.** `rt/gestures/12-loud-after-dblclick-crop.png`, `rt/gestures/13-loud-rightclick-crop.png`, `rt/gestures/14c-loud-valuebox-typed75-crop.png`, `rt/gestures/22a-compthr-rightclick.png`

### G-06 — Value entry (double-click the number): permissive parsing, but garbage/empty/comma silently commits 0 - on the Ceiling that is 0.00 dB

*Severity (observer's estimate): high · confidence: high*

- **Steps.** Double-click each value text, type, Return: Loudness (360,380): 75 / 0.5 / '0.5 %' / abc / 150 / -20 / 12.34 / 60% / 1 / 1.5 / 33+Escape / 42+click elsewhere / open+Tab. Ceiling (517,518): '-6 dB' / -6 / -3.456 / 1k / 95 / abc / -25 / '-1 dBTP' / '-0,5' / '' (empty). Character (165,518): 0.75 / 75 / '75 %' / abc. Tone (360,518): -0.5 / 2 / bright. Comp Threshold (215,212): '-6 dB' / -12.34 / abc / -50 / 5. Input Gain (204,562): '-6 dB' / 1k / abc / 50 / -20 / +3. SC HPF (388,562): 1k / 95 / abc / 50 / 0.1k / '300 Hz' / 0.5 / 150.
- **Observed.** Editor opens as a small outlined box with the raw number (no unit) selected, left-aligned, blue caret. Loudness: 75 -> 75 %; 0.5 -> 50 %; '0.5 %' -> 0.5 %; abc -> 0 %; 150 -> 100 %; -20 -> 0 %; 12.34 -> 12.3 %; 60% -> 60 %; 1 -> 100 %; 1.5 -> 1.5 %; 33+Escape -> unchanged (100 %); 42 then click elsewhere -> 42 % (focus loss commits); Tab -> commits and closes. Ceiling: '-6 dB' and -6 -> -6.00 dB; -3.456 -> -3.46 dB; 1k -> 0.00 dB; 95 -> 0.00 dB; abc -> 0.00 dB; -25 -> -20.00 dB; '-1 dBTP' -> -1.00 dB; '-0,5' -> 0.00 dB; empty -> 0.00 dB. Character: 0.75 -> 0.75; 75 -> 1.00; '75 %' -> 1.00; abc -> 0.00. Tone: -0.5 -> -0.50; 2 -> 1.00; bright -> 0.00. Comp Threshold: '-6 dB' -> -6.0; -12.34 -> -12.3; abc -> 0.0 dB; -50 -> -40.0; 5 -> 0.0. Input Gain: -6 dB -> -6.0; 1k -> 1.0 dB; abc -> 0.0; 50 -> 24.0; -20 -> -12.0; +3 -> 3.0. SC HPF: 1k -> 300 Hz; 95 -> 95 Hz; abc -> 20 Hz; 50 -> 50 Hz; 0.1k -> 100 Hz; 300 Hz -> 300 Hz; 0.5 -> 20 Hz; 150 -> 150 Hz. Repeated on five different controls with the same 'garbage -> 0' result.
- **Interpretation.** An unparsable, empty or European-decimal entry ('-0,3') is not rejected but committed as 0. For Loudness/Tone/Input Gain 0 is the harmless default, but for the Ceiling 0 is 0.00 dB (the hottest allowed output) and for Comp Threshold 0 dB, i.e. a typo drives the mastering limiter to full scale with no error feedback. The '1 -> 100 %' fraction rule is deliberate but a surprise next to '1.5 -> 1.5 %'. Character (0-1 scale) rejects '75' by clamping to 1.00 while its neighbour Loudness accepts '75'. The editor box is tiny (72x14), left-aligned against a centred readout, with a blue (off-brand) caret.
- **Code.** e769f33:src/PluginParameters.cpp:117 (dbFrom = removeCharacters + getFloatValue, 0 on failure), :108-113 (pctText), :142-147 (pctFrom fraction rule), :148-153 (hzFrom); e769f33:src/gui/LookAndFeel.cpp:927-934 (raw number in editor)
- **Captures.** `rt/gestures/15-sheet.png`, `rt/gestures/15a-loud-dblclick-valuebox-crop.png`, `rt/gestures/15e-loud-typed-abc-crop.png`, `rt/gestures/22c-inputgain-valuebox-editor.png`, `rt/gestures/33d-tp-ceiling-editor.png`

### G-07 — Dragging on the value TEXT drags the value with a different (1.5x faster) sensitivity than the knob

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Set Loudness 50 %; press on '50 %' text (360,380) and drag up 100 px; same on Input Gain value text (204,562) 50 px.
- **Observed.** Loudness value-text drag 100 px: 50 -> 100 % (clamped; source uses 180 px for full travel vs ~278 px on the knob). Input Gain value-text drag 50 px: 3.0 -> 12.0 dB. Knob shows press feedback while dragging the text.
- **Interpretation.** Two sensitivities for one control: a user who grabs the number instead of the knob gets ~55 % of range per 100 px versus 36 % on the knob. Minor inconsistency.
- **Code.** e769f33:src/gui/LookAndFeel.cpp:904-906 (downProp + dy/180.0)
- **Captures.** `rt/gestures/16a-loud-valuetext-drag.png`, `rt/gestures/16-sheet.png`

### G-08 — Keyboard: arrows step 1 % of range (Ceiling 0.01 dB), PageUp/Down/Home/End/modifiers do nothing, focus is invisible

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Click Loudness knob (no drag), press Up, Up, Up, Down, Right, Left, PageUp, PageDown, Home, End, Shift+Up, Ctrl+Up, Return, Delete, BackSpace, Tab, Tab, Up. Click Ceiling knob then Up, Down, Down; click Character then Up; click Comp Threshold then Down x2; click Input Gain thumb then Up, Right; click SC HPF thumb then Up x2.
- **Observed.** Loudness 50 -> 51 -> 53 -> 52 -> 53 -> 52 % (1 % per arrow); PageUp/PageDown/Home/End/Shift+Up/Ctrl+Up/Return/Delete/BackSpace: no change. After two Tabs, Up changed Tone 0.00 -> 0.02 (focus moved Loudness -> Character -> Tone). Ceiling Up from 0.00 stayed 0.00, Down x2 -> -0.02 dB (0.01 dB per press = 2000 presses for the range). Character Up -> 0.01. Comp Threshold Down -> -0.4, -0.8 dB. Input Gain Up -> +0.36 dB. SC HPF Up 66 -> 69 Hz. In no screenshot is there any focus ring, outline or colour change on the focused knob (16b, 16d, 16e).
- **Interpretation.** Keyboard operability exists (arrows, Tab order) but is unusable in practice: nothing shows which control has focus, and the Ceiling - the one knob a mastering user would nudge by keyboard - moves 0.01 dB per press because its range interval doubles as the key step. No page/home/end steps.
- **Code.** e769f33:src/gui/PluginEditor.cpp:1152-1169 (setWantsKeyboardFocus true, no focus paint); e769f33:src/PluginParameters.cpp:230-233 (twoDecimalRange interval 0.01 used as key step); JUCE juce_Slider.cpp:1034-1049 (arrow step = interval or range/100, no PageUp/Home)
- **Captures.** `rt/gestures/16-sheet.png`, `rt/gestures/16d-after-tab.png`, `rt/gestures/16e-after-tab2.png`

### G-09 — Drag past the window edge keeps tracking and releases cleanly

*Severity (observer's estimate): none · confidence: high*

- **Steps.** Press on Loudness knob, drag up to y=5 and y=0 (outside the window), dump, release, move back; press Ceiling knob, drag down to y=1099 (screen bottom), dump, release.
- **Observed.** Loudness reached 100 % while the pointer was outside, stayed 100 % after release outside, knob showed normal hover state on re-entry (no stuck drag). Ceiling reached -20.00 dB and stayed after outside release.
- **Interpretation.** Positive: mouse capture works as expected.
- **Captures.** `rt/gestures/19-sheet.png`, `rt/gestures/19a-loud-drag-past-top.png`

### G-10 — Input Gain and SC HPF faders are ~75 px wide and jump to the click position: one click can add +17 dB

*Severity (observer's estimate): high · confidence: high*

- **Steps.** Advanced view. Input Gain thumb (104,562), track x 78-152: drag thumb right 40 px; ctrl-drag 40 px; single click on the track at x=140; single click on the thumb centre at x=122 (value 9.6 dB); single click at x=80 and x=152; vertical 50-px drag. SC HPF thumb: drag right 40 px from 77 Hz.
- **Observed.** Input Gain 40-px drag: 0.0 -> 17.6 dB (0.44 dB/px). Ctrl-drag 40 px: 0.1 dB. Single click at x=140: jumped 0.0 -> 17.6 dB. Single click on the thumb itself at x=122: 9.6 -> 8.7 dB (snapped to pointer). Click at x=80 -> -12.0 dB, at x=152 -> 23.5 dB. Vertical drag: -0.2 dB (pointer-snap only). SC HPF 40 px: 77 -> 240 Hz.
- **Interpretation.** A 36-dB input trim on a 75-px track is far too coarse to set by hand (0.44 dB per px) and, worse, JUCE's snap-to-mouse means a stray click near the track instantly applies up to +17 dB into a limiter. The value box is the only precise route, and it is double-click-only (G-05).
- **Code.** e769f33:src/gui/PluginEditor.cpp:574-578 (LinearHorizontal, TextBoxRight 62x14; no setSliderSnapsToMousePosition(false))
- **Captures.** `rt/gestures/22-sheet.png`, `rt/gestures/22b-inputgain-after-drag.png`

### G-11 — Input Gain shows '-0.0 dB' after host automation to its default

*Severity (observer's estimate): low · confidence: high*

- **Steps.** ctl 'param inputGain 0.3333' (the default normalised value), capture; then 'paramtext inputGain 0.0 dB', capture.
- **Observed.** Readout '-0.0 dB' beside the fader after the automation write; '0.0 dB' after the text write. Dump confirms '-0.0 dB'.
- **Interpretation.** Negative zero leaks into the dB formatter; a host recalling the default via a 0.3333 normalised value shows a minus sign on a zero.
- **Code.** e769f33:src/PluginParameters.cpp:85 (dbText = String(v,1) with no -0 guard)
- **Captures.** `rt/gestures/36-sheet.png`

### G-12 — Dither combo: click opens a 3-item menu; first Down only highlights the current item; wheel and right-click inert

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Click combo (481,562); Down, Return; wheel down/up over the combo; right-click; open + Escape; open + click outside; open + Down Down + Return; Tab, Up.
- **Observed.** Menu lists Off (bullet), 16-bit, 24-bit below the combo. Down then Return kept 'Off' (Down highlighted the already-selected first row). Wheel: no change. Right-click: nothing. Escape and an outside click both close the menu. Down, Down, Return -> 16-bit. While open the combo shows an orange outline; the hovered row is highlighted.
- **Interpretation.** Standard JUCE combo behaviour; no wheel-to-cycle, so a mouse-only user needs two clicks per change. Neutral.
- **Captures.** `rt/gestures/23-sheet.png`, `rt/gestures/23g-dither-menu-down2.png`

### G-13 — Loudness macro sweep 0-100 %: which Advanced controls move (mapping as observed)

*Severity (observer's estimate): medium · confidence: high*

*Superseded in part by the audit's verification:* The Threshold curve reaches its -12 dB plateau at 60 % Loudness (`compThresholdDb = -12 · min(1, l / 0.6)`, e769f33:src/MacroEngine.h:52), between this observation's 50 % and 75 % samples, not at 75 %.

- **Steps.** All params reset to defaults; Simple view; music at -6 dB. Type 25, 50, 75, 100 into the Loudness value box, wait 2.5 s, capture Simple, dump, toggle ADV (838,67), capture, toggle back.
- **Observed.** out LUFS (short-term) readouts: 0 % -11.9, 25 % -13.5, 50 % -7.9, 75 % -7.7, 100 % -5.7 (fluctuating with the music; I stays -11.6). Character/Tone/Ceiling readouts unchanged. Parameter diffs vs defaults - 25 %: Comp Ratio 1.62:1, Comp Threshold -5.0 dB, Limiter Gain 3.4 dB. 50 %: Ratio 1.75:1, Thr -10.0 dB, Clip Shape 0.46, Clip Drive 2.6 dB, Lim Gain 7.8 dB. 75 %: Ratio 1.88:1, Thr -12.0 dB, Clip Shape 0.40, Clip Drive 5.8 dB, Dynamic Tame 0.8 dB, Lim Gain 12.7 dB. 100 %: Ratio 2.00:1, Thr -12.0 dB (plateau from 75 %), Clip Shape 0.35, Clip Drive 9.0 dB, Dynamic Tame 1.5 dB, Lim Gain 18.0 dB. Nothing else moved (attack/release/knee/mix/links/colour/EQ/limiter release untouched). In the Advanced panels the six moved knobs look exactly like every other knob - no badge, colour or marker distinguishes macro-managed controls from free ones.
- **Interpretation.** The mapping is coherent (compression + clip drive + limiter gain rise together, threshold plateaus at 75 %), but an Advanced user has no way to see which six knobs belong to the macro until they hand-edit one. The 'out LUFS' readout is short-term, so it does not read as a stable consequence of the macro.
- **Code.** e769f33:src/gui/PluginEditor.cpp:2038-2043 (outLufs = meterLufsS short-term)
- **Captures.** `rt/gestures/26-simple-sheet.png`, `rt/gestures/26-adv-sheet.png`, `rt/gestures/26-sweep-50-adv.png`, `rt/gestures/26-sweep-100-adv.png`

### G-14 — Detach: hand-edited managed knob gets a 5-px corner dot, but snaps back to the macro on the next Loudness gesture (never stays)

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Advanced, Loudness 100 %: drag Comp Threshold down 30 px by hand (-12.0 -> -16.3 dB); capture; hover the knob and the dot; click the dot. Back to Simple; type Loudness 50; dump. Repeat the hand edit (-15.8 dB) and this time DRAG the Loudness knob 30 px; dump.
- **Observed.** After the hand edit a small orange dot appeared at the top-right of the Threshold knob cell (27a-badge-crop); knob, caption and value colours unchanged; hovering the dot shows only the Threshold tooltip; clicking the dot changed nothing. In Simple an orange dot appeared beside the big Loudness knob with tooltip 'Advanced edits took knobs off the macros - click to return to the macro sound' and the preset read 'Default *' even with all values at defaults; clicking it removed the dot and the asterisk. Typing Loudness 50: Comp Threshold went -16.3 -> -10.0 dB (the macro value), dot gone. Dragging Loudness to 60.8 %: Threshold -15.8 -> -12.0 dB, dot gone. Confirmed twice.
- **Interpretation.** The detach indication exists but is faint (5 px, no legend in Advanced, no label/knob colour change) and the model is one-way: any later Loudness touch silently overwrites the hand edit. A user who tuned the threshold and then nudges Loudness loses the edit with no confirmation; the Simple dot's tooltip explains it only after the fact.
- **Code.** e769f33:src/gui/PluginEditor.cpp:656-660,1710,2126-2130 (editedDot); e769f33:src/gui/LookAndFeel.cpp:897-906 (macro gesture clears the whole detach mask); e769f33:src/gui/PluginEditor.cpp:90-98 (badge legend removed from tooltips)
- **Captures.** `rt/gestures/27-adv-sheet.png`, `rt/gestures/27a-badge-crop.png`, `rt/gestures/27-simple-sheet.png`, `rt/gestures/24-dot-crop.png`, `rt/gestures/25a-dot-hover-crop.png`, `rt/gestures/25b-dot-clicked-small.png`

### G-15 — Ceiling in Simple and in the Limiter panel are the same parameter; readouts always agree and the macro never moves it

*Severity (observer's estimate): none · confidence: high*

- **Steps.** Type -3 into Simple Ceiling; switch to Advanced; drag Limiter Ceiling up 50 px; back to Simple; drag Loudness.
- **Observed.** Advanced Limiter panel read '-3.00 dB' after the Simple entry; after the Advanced drag both read '0.00 dB'; Loudness drag to 68 % left Ceiling at 0.00 dB. No detach dot on Ceiling (it is not macro-managed).
- **Interpretation.** Positive: one parameter, two consistent views.
- **Captures.** `rt/gestures/28-sheet.png`

### G-16 — LOCK locks nothing on screen: every control still moves; it only shields the Ceiling from preset loads, with no visual change

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** LOCK on (597,508). Drag Loudness 50 px, Ceiling 50 px, Character 50 px, Tone 50 px; type -4 into Ceiling and 80 into Loudness; double-click Ceiling; ctl 'param ceiling 0.5'. Set Ceiling -1.00, open preset menu (481,67), click Loud Pop (474,175); dump; LOCK off; Ceiling -1.00 again; load Loud Pop; diff.
- **Observed.** With LOCK on: Loudness 50 -> 68 %, Ceiling -1.00 -> 0.00 dB, Character 0.00 -> 0.18, Tone 0.00 -> 0.36, typed -4 -> -4.00 dB, typed 80 -> 80 %, double-click -> -0.10 dB, automation -> -10.00 dB. No control changed appearance when LOCK engaged (only the LOCK switch turned orange). Loud Pop with LOCK on: Loudness 60 %, Character 0.25, Tone 0.15, Lim Gain 9.8 dB loaded, Ceiling stayed -1.00 dB. LOCK off: identical except Ceiling -0.10 dB (preset value). Preset menu: FACTORY list of 13 (Default ... Lo-Fi Crush) + Save Preset... / Load Preset...; the click that dismisses the menu is consumed (my LOCK click while the menu was open did not toggle LOCK).
- **Interpretation.** The word LOCK next to the Ceiling knob reads as 'lock this knob'; it does not - drags, typing, reset and automation all pass. It is a preset-browsing ceiling lock and only the (unshown on hover) tooltip says so. Nothing on the locked knob indicates the state.
- **Code.** e769f33:src/gui/PluginEditor.cpp:609-612 (ceilingLockToggle bound to int_ceilingLock); e769f33:src/PresetManager.cpp:62-67,119,310 (skip pid::ceiling on load when locked)
- **Captures.** `rt/gestures/29-sheet.png`, `rt/gestures/32-sheet.png`, `rt/gestures/29d-lock-preset-menu.png`

### G-17 — TP toggle re-labels the Ceiling unit dB -> dBTP in both views; the Statistics TP row does not visibly change

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Hover TP (597,449), click; capture Simple and Advanced; double-click Ceiling value and type -1; TP off.
- **Observed.** Ceiling '-0.10 dB' -> '-0.10 dBTP' (Simple) and Limiter panel '-0.10 dBTP' with its TP switch on; dump 'ceiling = -0.10 dBTP'. Statistics row still 'TP 1.18 dBTP' in red (a peak hold from earlier) and 'SP 0.00 dBFS' in red - no label, unit or value change on toggling. The value editor shows raw '-0.10' with no unit. TP off -> '-1.00 dB'.
- **Interpretation.** Unit follows mode as designed; the Statistics TP row is always in dBTP regardless of the mode, so the toggle gives no feedback in the meters, and the stale red 1.18 dBTP hold sits above a ceiling that now promises dBTP compliance.
- **Code.** e769f33:src/PluginParameters.cpp:302-308 (unit follows truePeakEngaged)
- **Captures.** `rt/gestures/33-sheet.png`, `rt/gestures/33b-tp-on-simple.png`, `rt/gestures/33c-tp-on-adv.png`

### G-18 — LEARN is latching: 5-s countdown, then an orange 'LEARN' that stays until clicked again; no parameter changes; runs while FREEZE is on

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Loudness 50 %, music -6 dB. Click LEARN (383,558); capture at 0.3, 2, 5, 10, 15, 20, 30 s with dumps. Click again. FREEZE on (270,558) then LEARN; capture at 0.5 and 7.5 s; click LEARN again; FREEZE off.
- **Observed.** Label '5' at 0.3 s, '3' at 2 s, '1' at 5 s, then 'LEARN' in orange text from 10 s through 30 s. No parameter (loudness/character/tone/ceiling/managed knobs) changed at any point. Second click: label back to white 'LEARN' (no warn flash). With FREEZE on, LEARN ran identically ('5' then orange LEARN); FREEZE stayed on, no message. Learn tooltip is the only place that says 'click again to stop'.
- **Interpretation.** After the countdown ends the only cue that learning is STILL running is the text colour; a first-time user will read '5..1 then LEARN' as a finished 5-second pass and leave it running indefinitely. No interlock or hint that Learn and Freeze conflict (Freeze holds the trims Learn is measuring for).
- **Code.** e769f33:src/gui/PluginEditor.cpp:621-640 (explicit start/stop), :2046-2075 (countdown then accent text while learning); e769f33:src/gui/PluginEditor.h:630 (kLearnMinPassMs 5000)
- **Captures.** `rt/gestures/34-sheet.png`, `rt/gestures/34e-learn-10s.png`, `rt/gestures/35-sheet.png`

### G-19 — Readout formatting is inconsistent across controls and switches format with the value

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Collected from every dump and screenshot during the session.
- **Observed.** Loudness '0 %' / '50 %' but '60.8 %', '48.2 %', '1.5 %', '0.0 %' (sub-0.05 % values) after drags, ctrl-drags or typing. Character/Tone '0.00' (no unit). Ceiling '-0.10 dB' (2 dp); all other dB knobs '0.0 dB' (1 dp); Input Gain '-0.0 dB'. Ratio '1.50:1'; times '30.0 ms'; '100 Hz' but '8.00 kHz'; percent knobs '100 %'. Meters: M/S/I '-11.8' (no unit), 'out LUFS -12.0' (caption carries the unit), 'TP 0.58 dBTP', 'SP -0.10 dBFS', 'RMS -8.8 dBFS', 'LRA 4.2 LU', 'PLR 15.0' (no unit). Percent readouts have a space before '%'; dB readouts a space before 'dB'.
- **Interpretation.** Spacing and unit placement are consistent, but precision jumps within one control (0 % vs 60.8 %) and between siblings (Ceiling 2 dp vs Threshold 1 dp; Character unit-less 0-1 next to Loudness 0-100 %). Continuous knob drags on Loudness routinely land on fractional percents, so the neat integer look only survives typed/default values.
- **Code.** e769f33:src/PluginParameters.cpp:85-116 (dbText/msText/pctText/hzText), :282-286 (Character/Tone String(v,2))
- **Captures.** `rt/gestures/00-initial.png`, `rt/gestures/27i-simple-after-loudness-drag.png`, `rt/gestures/15i-loud-after-12.34-crop.png`, `rt/gestures/20b-advanced-after-macro-reset.png`

### G-20 — Editor value box: editing look is small, left-aligned and off-brand

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Double-click '50 %' under Loudness and '0.0 dB' beside Input Gain; capture before typing.
- **Observed.** A 72x14 (62x14 for faders) rectangle with a light grey outline appears in place of the centred readout; the raw number is left-aligned, selected with a dim accent highlight, blue caret; the '%'/'dB' unit disappears while editing.
- **Interpretation.** Functional but cramped and visually foreign (default JUCE caret colour, left justification against a centred readout, hard outline).
- **Code.** e769f33:src/gui/PluginEditor.cpp:1136 (TextBoxBelow 72x14), :577 (TextBoxRight 62x14); e769f33:src/gui/LookAndFeel.cpp:938-953
- **Captures.** `rt/gestures/15a-loud-dblclick-valuebox-crop.png`, `rt/gestures/22c-inputgain-valuebox-editor.png`

### G-21 — Stale tooltip carry-over on fast pointer jumps (environment-sensitive)

*Severity (observer's estimate): none · confidence: low*

- **Steps.** Hover Statistics panel 1.8 s (tip shown), single-warp the pointer to the Loudness knob, capture at 1.8 s and 3.8 s; repeat Character -> Tone.
- **Observed.** The previous component's tooltip remained (same text, old position) for 3.8 s until the pointer moved again; with stepped pointer moves the correct tooltip appears every time. In 05 the Statistics text was shown at the Loudness knob position.
- **Interpretation.** Almost certainly an XTest warp not producing a motion event JUCE consumes (a real mouse always does), so this is an audit-environment note rather than a user defect; listed so nobody re-reports the stale screenshots.
- **Code.** JUCE juce_TooltipWindow.cpp:206-259 (tip only re-evaluated from getComponentUnderMouse)
- **Captures.** `rt/gestures/05-tip-loudness-crop.png`, `rt/gestures/07b-tip-after-jump-crop.png`, `rt/gestures/08b-tip-warp-to-tone-crop.png`

### G-22 — Preset menu outside-click behaviour and consumed click

*Severity (observer's estimate): low · confidence: medium*

- **Steps.** Open preset menu; click on the root window below the plugin (474,778); click LOCK; click preset button; press Escape; later open and click inside the editor (150,300).
- **Observed.** A click outside the plugin window left the menu open; the next click on LOCK closed the menu but did not toggle LOCK (consumed); Escape closes the menu; a click inside the editor closes it (and is consumed).
- **Interpretation.** Consistent with the recipe's note; the consumed dismissal click means a user who sees the menu and clicks LOCK/TP/ADV must click twice. The root-window case mirrors clicking in the host window on Linux and could not be judged against a real host.
- **Code.** e769f33:src/gui/PluginEditor.h:170-190 (PopupShield consumes the dismissing click)
- **Captures.** `rt/gestures/30-sheet.png`, `rt/gestures/31-sheet.png`, `rt/gestures/32a-menu-click-outside.png`

**Positives recorded.**

- Tooltip copy is one descriptive line per control, terse, no trailing period, and the Ceiling/TP twins in Simple and Advanced show the identical text (single table, e769f33:src/gui/PluginEditor.cpp:32-89).
- Ceiling readouts agree exactly between the Simple knob and the Limiter-panel knob at all times (-3.00 dB / 0.00 dB / -0.10 dBTP), same parameter, same unit switch.
- Every knob has clear press feedback (ring brightens, pointer turns white) and a hover state; dragging past the window edge keeps working and releasing outside ends the gesture cleanly with no stuck drag.
- Double-click and alt-click reset to the parameter default on every knob and fader tested (Loudness -> 0 %, Ceiling -> -0.10 dB, Comp Threshold -> 0.0 dB, Input Gain -> 0.0 dB, SC HPF -> 20 Hz).
- Text entry is forgiving in the right ways: '-6 dB', '-6', '-1 dBTP', '+3', '60%', '1k', '0.1k', '300 Hz' all parse; percent boxes read a bare 0.5 as 50 % and '0.5 %' literally; out-of-range values clamp to the range ends; Escape cancels an edit, Return/Tab/focus-loss commit.
- The detach state is visible in both views (corner dot on the hand-edited knob in Advanced, a dot beside the big Loudness knob in Simple with tooltip 'Advanced edits took knobs off the macros - click to return to the macro sound'), clicking the Simple dot re-lands the curve and clears the preset dirty mark.
- LOCK does exactly what its tooltip says ('Keep the Ceiling where it is while you browse presets'): loading Loud Pop kept -1.00 dB with LOCK on and reset it to -0.10 dB with LOCK off, everything else loaded normally.
- TP toggle immediately re-labels the Ceiling readout '-0.10 dB' -> '-0.10 dBTP' in Simple and Advanced, and the Advanced TP toggle mirrors the Simple one.
- LEARN shows a wordless countdown (5,4,3,2,1) for the minimum pass and turns its label orange while a pass is running, then back to white when stopped.
- Popup menus (preset, dither) highlight the hovered item, close on Escape and on a click inside the editor, and combos show an accent outline while open.

**Limitations recorded.**

- A single xdotool pointer WARP between two components inside the window is often not seen by JUCE (the tooltip/hover stays on the previous component until the pointer moves again); every hover here was therefore done with 2-3 stepped moves. This is an XTest/Xvfb artifact, not user-facing, but it means 'stale tooltip' screenshots 05/07b/07c/08b/27a must not be read as plugin defects.
- Tooltip window stacking under Xvfb with no window manager is unreliable (25c showed a tooltip clipped by the Statistics panel); tooltip position/clipping relative to the editor was not judged.
- Clicking on the bare root window (outside the plugin window) does not dismiss a JUCE popup menu; a real host window would behave the same on Linux but this could not be verified against a host.
- Fine-drag modifiers were tested with X11 modifier names (shift, ctrl, alt, super); macOS Command behaviour was not testable.
- LEARN's effect on the adaptive trims is internal state, not a parameter, so only the button state could be observed, not what was learned; FREEZE+LEARN semantics could not be verified beyond 'both run at once with no warning'.
- Host-side automation-lane behaviour (begin/end gesture pairing, undo grouping) was not observed; only the harness ctl channel (param/paramtext) was used for host-style writes.
- The 'out LUFS' readout is the short-term value (e769f33:src/gui/PluginEditor.cpp:2038-2043), so the per-step values in the Loudness sweep fluctuate with the music's verse/chorus and are not a clean A/B of the macro.

## State workflows: presets, A/B, Copy, undo, LOCK, bypass, Settings, persistence, sample rate (22 observations)

**Observer summary.** State workflows are largely solid: presets, stepping, A/B (per-slot identity and undo), Copy (undoable), LOCK, session persistence (view/scale/tooltips/both slots) and cross-instance preset discovery all work. The serious findings are: (1) after picking a preset from the menu the next click is either delivered to the Loudness knob as a phantom drag (knob silently jumps to 0 %/100 %, preset marked '*') or swallowed — reproduced 12/14 times with warp+click; (2) the undo model has traps — the ADV view toggle pushes an undo step and wipes redo, while Settings rows (Oversampling, UI Scale) are not undoable so pressing undo after changing one silently reverts an unrelated earlier preset/parameter change; (3) host bypass shows no indication and freezes every meter and the GR history at stale values, whereas plugin BYPASS shows a red pill plus dim and keeps metering (and keeps drawing GR activity). Medium items: Copy has no stated direction or feedback and overwrites the other slot; saving over an existing user preset name overwrites silently; MATCH and DELTA can both be on with nothing marking MATCH as monitoring-only; Settings combo lists remain painted with a stale tick after a selection until the mouse moves; no Settings row explains its effect or shows the latency it costs (Min: 480–486; Linear: 480→529–547 samples); Integrated/RMS Reference switches give no acknowledgement in the Statistics panel. Low: the '*' modified marker is not persisted (a reloaded session claims the clean preset name); Load Preset uses the unbranded JUCE file chooser; sample-rate changes silently wipe the GR history and S/out-LUFS/TP holds while FREEZE survives; transport stop changes nothing on screen.

### ST-01 — First click after choosing a preset from the menu is hijacked by the Loudness knob (silent knob jump) or swallowed

*Severity (observer's estimate): high · confidence: medium*

- **Steps.** Click preset name (481,67); click 'Loud Pop' (474,176) [loudness becomes 60 %]; wait 1 s (also tried 3 s); `xdotool mousemove 838 67 click 1` (ADV toggle). Variants: BYPASS (909,67), TP (597,449), '>' (553,67), empty area (838,300), Statistics panel (800,400); split press/release; move-then-wait-1s-then-click; 10-step smooth motion then click. `dump` after each.
- **Observed.** ADV click: Loudness 60 %→100 %, ADV stays Off, preset name becomes 'Loud Pop *' — reproduced 7/7 (immediate warp+click). TP click → Loudness 0 %, TP unchanged. (838,300) press → Loudness 10.4 % (=60 − 124 px/250 px, i.e. a RotaryVerticalDrag anchored at the menu item's y=176). BYPASS 2/3 hijacked, 1/3 normal; '>' 3/5 hijacked (→100 %), 2/5 stepped normally. With a 1 s pause between the pointer move and the click, the click was swallowed instead (no control reacted, knob unchanged: BYPASS stayed Off, '>' did not step, (800,400) no-op). With continuous 10-step motion before the click everything was normal (ADV toggled, knob unchanged). The value jump is silent — no gesture is visible, only the knob arc and the '*' marker afterwards.
- **Interpretation.** After the parented preset PopupMenu closes, the editor's mouse routing is in a bad state for the next press: a press is either delivered to the Loudness knob as a drag whose origin is the menu item's position (knob jumps by (item_y − click_y)/250 of full range) or is consumed outright. For a user this means choosing a preset and then clicking a top-bar control can silently rewrite Loudness (to 0 % or 100 %) and dirty the preset. The magnitude of real-mouse exposure is uncertain because continuous motion before the click behaved normally in the one trial tried; but trackpad taps and fast flick-clicks produce few motion events. Other observers using warp+click after menus will hit this too.
- **Code.** e769f33:src/gui/PluginEditor.cpp:1129 (knobs are Slider::RotaryVerticalDrag, 250 px extent); e769f33:src/gui/PluginEditor.cpp:2172-2330 (showPresetMenu: withParentComponent(this) + PopupShield raise/lower in the async completion); e769f33:src/gui/PluginEditor.h:166-199 (PopupShield swallows mouse events while raised)
- **Captures.** `rt/state/05a-repro-before.png`, `rt/state/05b-repro-after.png`, `rt/state/10b-s2-selected.png`, `rt/state/10c-s3-down.png`, `rt/state/12b-smooth-click.png`

### ST-02 — Preset menu contents, tick, modified marker, Escape

*Severity (observer's estimate): none · confidence: high*

- **Steps.** Click preset name (481,67); capture; after loading Loud Pop drag Tone knob up 25 px; capture; reopen menu; press Escape.
- **Observed.** Menu: header FACTORY; 13 items in order Default (dot tick), Transparent Master, Loud Pop, EDM Club, Vocal Forward, Tape Glue, Rock Punch, Hip-Hop Low End, Acoustic Warmth, Classical Dynamics, Podcast Voice, Cinematic Wide, Lo-Fi Crush; separator; 'Save Preset…', 'Load Preset…'. No USER section until a user preset exists. Selecting Loud Pop set Loudness 60 %, Character 0.25, Tone 0.15, Ceiling −0.10 dB, TP Off; Advanced view showed Ratio 1.80:1, Threshold −12.0 dB, Clip Shape 0.44, Clip Drive 3.9 dB, Color Depth 19 %, Color Tone 0.08, Dynamic Tame 0.3 dB, Limiter Punchy, Gain 9.8 dB, Tilt 0.3 dB (dump agrees). Moving a knob renders 'Loud Pop *'; the menu dot stays on Loud Pop while modified. Escape closes the preset menu.
- **Interpretation.** Positive/neutral: the menu is clear, the tick tracks the base preset and the '*' marker communicates 'modified'. Nothing tells the user what a preset changes beyond its name.
- **Captures.** `rt/state/01-preset-menu.png`, `rt/state/01-preset-menu-crop.png`, `rt/state/02-loudpop-simple.png`, `rt/state/39-loudpop-advanced-clean.png`, `rt/state/13a-topbar.png`, `rt/state/13b-crop.png`, `rt/state/13c-crop.png`

### ST-03 — ‹ › stepping wraps and includes user presets (FACTORY ring then USER)

*Severity (observer's estimate): none · confidence: high*

- **Steps.** From 'Loud Pop *' click '>' (553,67) 13×, then '<' 2×; later from user preset 'AuditTest' click '>' then '<' ×2.
- **Observed.** Sequence EDM Club → Vocal Forward → … → Lo-Fi Crush → Default (wrap) → Transparent Master → Loud Pop; '<' reverses. From AuditTest '>' gives Default (wrap); from Default '<' gives AuditTest then Lo-Fi Crush. Stepping from a modified preset goes to the neighbour of the base preset.
- **Interpretation.** Positive: one ring, wraps, user presets not skipped, consistent with the menu order.
- **Code.** e769f33:src/gui/PluginEditor.cpp:379-393 (stepPreset builds FACTORY + sorted user files, walks as a ring)
- **Captures.** `rt/state/14-step-strip.png`, `rt/state/17-step-user.png`

### ST-04 — Save Preset… is a branded in-editor overlay; typing works on Linux; file lands in ~/.config/RollyTech/Anabasis/Presets

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Menu → 'Save Preset…'; type 'AuditTest' (`xdotool type`); Return; `find $HOME`; reopen menu.
- **Observed.** Overlay 'SAVE PRESET' with one text field pre-filled with the current preset's raw name (selected), Save / Cancel buttons, dimmed backdrop. Typing replaced the selection correctly; Return saved and closed; file created at $HOME/.config/RollyTech/Anabasis/Presets/AuditTest.anabasis (2017 bytes, `<AnabasisPreset schemaVersion="1">` with 45 PARAM + empty DETACH_MASK; no internal settings). Preset name shows 'AuditTest'; menu now has a USER section with the tick on AuditTest. ctrl+a then typing also worked (second save).
- **Interpretation.** Positive: no keyboard problem on Linux (KI-014 not reproduced here; macOS untested). The dialog gives no hint where the file is stored and offers no folder/rename/delete management.
- **Code.** e769f33:src/gui/PluginEditor.cpp:2361-2382 (showSavePreset pre-fills raw stored name, grabs focus)
- **Captures.** `rt/state/15a-save-dialog.png`, `rt/state/15b-crop.png`, `rt/state/15c-crop.png`, `rt/state/15d-crop.png`

### ST-05 — Saving over an existing user preset name overwrites silently — no confirmation

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** With EDM Club active: menu → Save Preset… (486,517); ctrl+a; type 'AuditTest'; Return; capture 0.3 s and 1 s later; compare file mtime and contents.
- **Observed.** No dialog or toast at 0.3 s or 1 s; the top bar immediately reads 'AuditTest'; AuditTest.anabasis mtime changed (1790407931 → 1790408821) and now contains loudness=80.0 (EDM Club's value) instead of 0.0.
- **Interpretation.** A destructive action with no guard: a typo or the pre-filled name silently destroys a saved preset.
- **Captures.** `rt/state/38c-crop.png`, `rt/state/38d-topbar.png`

### ST-06 — Load Preset… opens JUCE's stock file chooser in a separate, unbranded window

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Menu → 'Load Preset…'; capture; click AuditTest.anabasis; click Open.
- **Observed.** A separate X window titled 'Load Preset' with JUCE default grey/teal styling: full absolute path in a combo (truncated with …), file list, 'file:' field, Cancel / Open (Open disabled until a row is selected). Selecting and Open loads the preset; the top bar reads 'AuditTest'.
- **Interpretation.** Functional, but visually foreign next to the branded Save overlay; exposes a long temp/config path. On this Linux box the non-native JUCE browser is used.
- **Code.** e769f33:src/gui/PluginEditor.cpp:2334-2356 (juce::FileChooser launchAsync)
- **Captures.** `rt/state/16a-load-dialog.png`, `rt/state/16b-crop.png`, `rt/state/16c-topbar.png`

### ST-07 — A/B: per-slot preset identity and per-slot undo history; switching is not an undo step; nothing else changes

*Severity (observer's estimate): none · confidence: high*

- **Steps.** In slot A set Loudness 60 % (`param loudness 0.6`); click A/B (589,67); dump; capture; click A/B again; later: in slot B press undo.
- **Observed.** Press 1: B letter highlighted, top bar 'Default', Loudness 0 % (B's own state), undo AND redo dimmed; meters/GR history/FREEZE/LEARN/view/MATCH untouched, history not cleared. Press 2: back to 'AuditTest *', 60 %, undo enabled again. Undo pressed in slot B with empty history: nothing happens (button dimmed). Host-automation change (`param`) marks the preset '*' and is undoable.
- **Interpretation.** Positive: A/B is instant and self-contained; undo/redo buttons show clear enabled/disabled states. The A/B switch itself cannot be undone (per-slot histories), which is defensible but undocumented in the UI.
- **Captures.** `rt/state/18a-slotA-loud60.png`, `rt/state/18b-after-ab-1.png`, `rt/state/18-ab-topbars.png`, `rt/state/20-undo-topbars.png`

### ST-08 — Copy: active→other, direction unstated, no feedback, undoable only from the destination slot

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** A active (AuditTest *, 60 %), B = Default 0 %. Click Copy (640,67); capture at 0.25 s and 1 s; A/B; dump; undo in B; A/B; undo in A.
- **Observed.** No visible confirmation or change at 0.25 s/1 s (button unchanged, no toast). After A/B, B reads 'AuditTest *' with Loudness 60 % (A was copied into B). Undo in B reverted B to 'Default' 0 % (Copy is an undo step in the destination slot; B's undo button lit after the copy). A unaffected; undo in A reverted its own 60 %→0 %.
- **Interpretation.** The button says only 'Copy': the user cannot tell whether it copies A→B or B→A, and gets no acknowledgement that anything happened; the other slot is overwritten silently. Recovery exists but only after switching slots and pressing undo there.
- **Captures.** `rt/state/19a-copy-immediate.png`, `rt/state/19-copy-topbars.png`

### ST-09 — Undo-step classification: ADV toggle consumes an undo step and clears redo; Settings rows are not undoable and undo then reverts an unrelated earlier change

*Severity (observer's estimate): high · confidence: high*

- **Steps.** After each action press undo (688,67) and capture/dump: (a) Character drag +30 px; (b) TP toggle; (c) preset Loud Pop via menu; (d) A/B; (e) ADV toggle; (f) Settings → Oversampling 2x, dismiss, undo; (g) Settings → UI Scale L, dismiss, undo.
- **Observed.** (a) 0.10→undo 0.00 ✓; (b) TP On→undo Off ✓; (c) Loud Pop→undo 'AuditTest' with previous values ✓ (identity restored); (d) A/B → undo does nothing (per-slot); (e) ADV On → redo dimmed (new transaction pushed, redo stack cleared) → undo turns ADV Off (editor 822→720) — ADV is an undo step; (f) Oversampling 2x: latency 480→484, undo leaves 484 but the preset name flips 'AuditTest'→'Lo-Fi Crush' (an older preset step was undone instead); (g) UI Scale L: editor 1175x900; undo leaves scale L and again reverts the preset to 'Lo-Fi Crush'.
- **Interpretation.** Two traps: (1) toggling the view — advertised as never changing the sound — pushes an undo transaction and destroys the redo stack; (2) after changing a Settings row, pressing undo does not revert it but silently undoes the last parameter/preset change, which the user is not looking at.
- **Code.** e769f33:src/gui/PluginEditor.cpp:1943-1948 (comment: advancedMode written by undo restore since ADR-0018); e769f33:src/gui/PluginEditor.cpp:361-362 (undo/redo onClick → proc.undo()/redo())
- **Captures.** `rt/state/20-undo-topbars.png`, `rt/state/20e1-adv.png`, `rt/state/21e-topbar.png`, `rt/state/27c-topbar.png`

### ST-10 — Settings combo dropdown stays painted after a selection, showing a stale tick, until the mouse moves

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Settings → Oversampling combo (579,290) → click '2x' (504,341); capture at 0.3 s and 1.8 s; then move the mouse; capture. Repeated for 4x/8x/16x/Off and Phase.
- **Observed.** Combo text updates ('2x', '4x'…) and latency changes immediately, but the list stays on screen with the dot still on the PREVIOUS value (e.g. combo '4x', dot on '2x') for ≥2 s; it disappears as soon as the pointer moves. Escape closes a dropdown cleanly with a proper repaint.
- **Interpretation.** Looks like the selection did not take (stale tick) and that the menu is still open; a second click on the ghost list can mis-select. Either a missing repaint under the dismissed child menu or a ghost menu window.
- **Code.** e769f33:src/gui/PluginEditor.cpp:2451-2495 (healGhostTrackedPopupMenus) and 2496+ (refreshPopupShield) — the code already anticipates stranded pop-ups
- **Captures.** `rt/state/21c-crop.png`, `rt/state/22-os-4x-03s-crop.png`, `rt/state/22-os-4x-18s-crop.png`, `rt/state/23b-phase-linear-03s-crop.png`, `rt/state/23d-phase-after-move-crop.png`

### ST-11 — Settings rows: options and measured latency; no row explains its effect; no latency readout

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** For each Oversampling value with Phase Minimum then Linear: `dump` and read `latency=`. Open Phase / Offline Render / Integrated / RMS Reference / UI Scale combos and capture.
- **Observed.** Rows: Oversampling (Off/2x/4x/8x/16x), Phase (Minimum/Linear), Offline Render (Follow Online/Force Max), Integrated (BS.1770-2+/BS.1770-1), RMS Reference (AES-17/Mathematical), UI Scale (XS/S/M/L/XL), UI Animations toggle (on), Tooltips toggle (off). Latency in samples at 48 kHz — Minimum: Off 480, 2x 484, 4x 486, 8x 486, 16x 486; Linear: Off 480, 2x 529, 4x 541, 8x 545, 16x 547. No text, tooltip or latency figure on any row; no close button (click outside closes); UI Scale dropdown extends below the panel.
- **Interpretation.** Users choosing 16x or Linear cannot see that latency rises (to ~11.4 ms) or what 'Follow Online' means; the settings read like a spec sheet without consequences.
- **Captures.** `rt/state/21a-settings.png`, `rt/state/21b-crop.png`, `rt/state/23a-crop.png`, `rt/state/24a-crop.png`, `rt/state/24b-crop.png`, `rt/state/25c-crop.png`, `rt/state/26b-crop.png`

### ST-12 — Integrated standard and RMS Reference switches give no visible acknowledgement in the Statistics panel

*Severity (observer's estimate): low · confidence: medium*

- **Steps.** With Settings open: Integrated → BS.1770-1 (crop stats before/after, 1.5 s); RMS Reference → Mathematical (crop stats).
- **Observed.** Integrated: 'I −9.2' before and after; the row label stays 'I', nothing resets or re-labels. RMS Reference: reading moved −12.4 → −14.9 dBFS (~−2.5 to −3 dB on running music) but the label stays 'RMS'; no unit/standard shown anywhere.
- **Interpretation.** A user cannot tell from the meter which standard/reference is active, and the Integrated switch appears to do nothing (no reset of the running integration, so it is unclear when the new gating applies).
- **Captures.** `rt/state/25a-crop.png`, `rt/state/25b-crop.png`, `rt/state/26a-crop.png`

### ST-13 — Plugin BYPASS: red pill + subtle dim; meters keep running on the unprocessed signal; GR history keeps drawing; marks preset modified

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Click BYPASS (909,67); capture at 1 s and 4 s; dump; click again; capture.
- **Observed.** BYPASS pill turns red; whole editor dims slightly; knobs unchanged (60 %); top bar 'AuditTest *'; Statistics keep updating and show the raw input: TP 4.03 dBTP and SP 3.83 dBFS in red, out LUFS −13.8; GR history continues — flat for the first second, then GR dips reappear while still bypassed; latency unchanged 480. Un-bypass restores normal display; SP/TP holds keep the red over-0 values.
- **Interpretation.** From across the room only the small red pill signals bypass; the dim is faint. GR activity reappearing in the history while bypassed contradicts 'bypassed'. Bypass dirtying the preset ('*') is questionable since bypass is stored as a parameter.
- **Code.** e769f33:src/gui/PluginEditor.cpp:1968 (dimOverlay visible when bypass ≥ 0.5)
- **Captures.** `rt/state/30a-before-bypass.png`, `rt/state/30b-bypass-1s.png`, `rt/state/30c-bypass-4s.png`, `rt/state/30-bypass-composite.png`

### ST-14 — Host bypass (processBlockBypassed): no UI indication and all meters/GR history/out LUFS freeze at stale values

*Severity (observer's estimate): high · confidence: high*

- **Steps.** `hostbypass 1`; capture at 1 s and 4 s; dump; `hostbypass 0`; capture.
- **Observed.** BYPASS pill stays off, no dim, no text. The 1 s and 4 s captures are pixel-identical in the Statistics panel (M −6.9, S −8.1, RMS −6.3), GR well and out LUFS −8.1 — the display froze. Latency stays 480. After `hostbypass 0` meters resume (M −9.8).
- **Interpretation.** The two bypass paths look and behave completely differently: host bypass leaves a frozen, misleading meter panel with no hint the plugin is bypassed.
- **Captures.** `rt/state/30e-hostbypass-1s.png`, `rt/state/30f-hostbypass-4s.png`, `rt/state/30g-hostbypass-off.png`, `rt/state/30-bypass-composite.png`

### ST-15 — MATCH and DELTA are not mutually exclusive and nothing marks MATCH as monitoring-only

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Click MATCH (87,558); capture; click DELTA (182,558); capture; MATCH off; DELTA off; dump each time.
- **Observed.** loudnessComp=On then deltaMonitor=On simultaneously (both toggles orange). No badge, colour change or text in the meters/graph; out LUFS reads −9.8 / −8.8 / −7.1 across the states. Both are also present in the Advanced utility row.
- **Interpretation.** MATCH+DELTA together is a meaningless combination for the user, and nothing says MATCH will not be in the render.
- **Captures.** `rt/state/31-match-delta-composite.png`, `rt/state/31b-match-then-delta.png`

### ST-16 — Persistence round-trip: view, scale, tooltips, active slot and both slots restore; undo history, meter holds and the '*' marker do not

*Severity (observer's estimate): low · confidence: high*

- **Steps.** `save state1.bin` + `savexml state1.xml` (Simple, M, tooltips off, slot A 'AuditTest *' 60 %, B Default). Then `param advancedMode 1`, A/B → B, `param loudness 0.3`, `param character 0.8`, Settings: Tooltips on, UI Scale L. `load state1.bin`; capture; dump; A/B; dump.
- **Observed.** After load: Simple view (720 high), UI Scale M, Tooltips Off, slot A active, A = 60 %/0.00 but top bar reads 'AuditTest' WITHOUT '*'; B back to Default 0 % (both slots overwritten from the file); undo and redo both dimmed (history cleared); TP hold reset to 0.01 dBTP, LRA shows '-', I keeps −9.7; the Settings popup that was open stayed open across the load. XML: AnabasisRoot schemaVersion=1 → ANABASIS (50 PARAM) → ANABASIS_INTERNAL (int_oversample, int_osPhase, int_offlineQuality, int_ceilingLock, int_uiScale=100, int_tooltipsOn, int_uiAnimations, int_spectrumOn, int_integratedStd, int_rmsRef) → AB active=0 with two SLOT (presetName, presetSource, presetFactoryId, presetUserFile, 50 PARAM, DETACH_MASK). No 'modified' flag stored.
- **Interpretation.** Mostly positive (everything a session needs is stored, including UI scale and tooltips). Loss of the '*' marker means a reopened session claims to be exactly 'AuditTest' while its values differ from the file.
- **Captures.** `rt/state/32a-persist-baseline.png`, `rt/state/32c-before-load.png`, `rt/state/32d-after-load.png`, `rt/state/32d-topbar.png`, `rt/state/32e-topbar.png`

### ST-17 — Sample-rate change with FREEZE on: FREEZE and frozen trims survive, but GR history is wiped and S/out LUFS/TP holds reset

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Click FREEZE (270,558); wait 10 s; capture; `sr 96000 512`; capture at 1 s/3 s; dump; `sr 48000 64`; `sr 192000 2048`; captures at 1 s/4 s.
- **Observed.** FREEZE toggle stays On through all three changes; limGain 9.8 dB (frozen trim) unchanged; latency 480→960→480→1920 (constant 10 ms). Each change clears the GR history to a flat line that refills from the right, sets S and out LUFS to '-', resets the TP hold (0.46→0.01 dBTP); I shifted −8.4→−9.2 rather than restarting from '-'. History fill rate identical at 48k/64, 96k/512, 192k/2048 (~20 % width in 4 s) — no speed/scale glitch. No message about the reset.
- **Interpretation.** KI-006: the frozen state is visibly preserved (toggle + trims). The meter reset on every prepareToPlay is expected but silent; the partial I reset is inconsistent with S/LRA showing '-'.
- **Captures.** `rt/state/33a-freeze-10s.png`, `rt/state/33b-sr96k-1s.png`, `rt/state/33e-sr48k-64-4s.png`, `rt/state/33g-sr192k-4s.png`, `rt/state/33-freeze-sr-composite.png`

### ST-18 — transport 0: no display reacts to the playhead stopping

*Severity (observer's estimate): low · confidence: medium*

- **Steps.** Capture; `transport 0`; wait 2.5 s; capture; `transport 1`.
- **Observed.** Meters keep integrating (I −7.6 → −7.5), GR history keeps scrolling, no indicator changes.
- **Interpretation.** Neutral in the harness (audio keeps flowing); a host stop does not pause integrated measurement, which some users expect.
- **Captures.** `rt/state/34-transport-composite.png`

### ST-19 — Second instance sees a user preset saved by the first without restart

*Severity (observer's estimate): none · confidence: high*

- **Steps.** Launch instance 2 (HARNESS_POS=1000,40, same HOME); open its preset menu (1441,67), capture; in instance 1 save 'SecondInst'; reopen instance 2's menu.
- **Observed.** Instance 2 initially lists USER: AuditTest; after the save in instance 1 it lists AuditTest and SecondInst immediately.
- **Interpretation.** Positive: the menu rescans the folder on open.
- **Code.** e769f33:src/gui/PluginEditor.cpp:2174-2177 (showPresetMenu re-reads the directory each time)
- **Captures.** `rt/state/35a-crop.png`, `rt/state/36e-crop.png`

### ST-20 — LOCK keeps the ceiling across preset loads but the preset shows no modified marker

*Severity (observer's estimate): low · confidence: high*

- **Steps.** `paramtext ceiling -1.0`; click LOCK (597,508); menu → Loud Pop; dump; LOCK off; menu → EDM Club; dump.
- **Observed.** With LOCK on: Loud Pop applied, Ceiling stays −1.00 dB, top bar 'Loud Pop' (no '*'). With LOCK off: EDM Club resets Ceiling to −0.10 dB. int_ceilingLock is stored in the session XML.
- **Interpretation.** Works as specified; the clean name with a non-preset ceiling is a small honesty gap (LOCK's orange pill is the only cue).
- **Code.** e769f33:src/PresetManager.cpp:62-68, 306-316 (locked ceiling never written)
- **Captures.** `rt/state/37-lock-composite.png`, `rt/state/37b-lock-loudpop.png`

### ST-21 — A click on the preset name right after a keyboard-dismissed menu did not open the menu (button highlighted only)

*Severity (observer's estimate): low · confidence: low*

- **Steps.** Menu open → Return (stray) → Escape → click preset name (481,67); capture.
- **Observed.** Preset button shows hover/pressed shading but no menu; a later identical click opened it.
- **Interpretation.** Same family as the recipe's Settings-dismiss note and ST-01: one click after a pop-up closes is intermittently lost. Seen once here, not deliberately re-confirmed.
- **Captures.** `rt/state/35e-crop.png`, `rt/state/36b-crop.png`

### ST-22 — UI Scale L: editor grew to 1175x900 but the harness window stayed 948x728 (editor clipped)

*Severity (observer's estimate): none · confidence: medium*

- **Steps.** Settings → UI Scale → L; read editor-bounds; capture.
- **Observed.** Log: editor-bounds w=1175 h=900 (1028 in Advanced); window bounds unchanged 40 40 948 728; right/bottom of the editor cut off (ADV/BYPASS toggles and graph well not visible); the Settings overlay re-centres at the new size. Back to M restores 940x720.
- **Interpretation.** Almost certainly the audit harness not honouring the editor's resize; in a real host the window should follow. Recorded so nobody reads the clipped screenshots as a plugin bug; the scaled editor itself laid out correctly within the visible region.
- **Captures.** `rt/state/27a-uiscale-L.png`, `rt/state/28b-back-to-M.png`

**Positives recorded.**

- Preset menu is clear: FACTORY/USER sections, a dot tick that tracks the base preset, a '*' modified marker, Escape closes it, and it re-reads the folder on every open (a preset saved by another instance appears at once).
- ‹ › walk one ring (FACTORY then USER), wrap at both ends, never skip user presets, and step from the base of a modified preset.
- Save Preset is a branded in-editor overlay with the name pre-selected; typing/ctrl+a/Return work on Linux; files are plain XML (schemaVersion=1) in ~/.config/RollyTech/Anabasis/Presets.
- A/B is instant, carries its own preset identity and undo history per slot, and leaves meters, history, FREEZE/LEARN and the view alone; the active letter is clearly highlighted.
- Undo/redo buttons render distinct enabled/disabled states; knob drags, toggles, preset loads, host-automation writes and Copy are all undoable with the preset identity restored.
- Session state round-trips view, UI scale, tooltips, active slot, both slots with their preset identities and the internal settings.
- LOCK reliably protects the ceiling through preset loads and persists in the session.
- FREEZE and its frozen trims survive sample-rate changes; GR-history scroll speed is time-based and identical at 48k/64, 96k/512 and 192k/2048; latency scales cleanly (480/960/1920 = 10 ms).
- Plugin BYPASS gives an unmistakable red pill and keeps the meters honest about the raw signal (over-0 dBTP shown in red).

**Limitations recorded.**

- All input is synthetic xdotool (instant pointer warps, zero-length clicks). ST-01 (post-menu click hijack/swallow) is fully reproducible with warp+click but behaved normally in the single trial with continuous motion; real-mouse/trackpad exposure needs a human check.
- The harness window does not resize with the editor, so UI Scale L/XL layouts could only be judged in the clipped region (ST-22); XS/S were not tried.
- No real host: host bypass, transport and sample-rate changes are harness emulations (processBlockBypassed, isPlaying flag, releaseResources+prepareToPlay); latency numbers are getLatencySamples read via `dump`, at 48 kHz except the SR test.
- Nothing was auditioned by ear, so whether MATCH is truly monitoring-only or how Copy/bypass sound could not be verified — only parameter/UI state was observed.
- KI-014 is macOS-only; only the Linux name field was tested (no issue found).
- Offline Render (Follow Online/Force Max) has no observable effect in this harness (no offline render path).
- Menu geometry shifts as user presets are added (Save Preset… moved from y≈437 to 494 to 517); two of my clicks landed on the USER header before I re-measured — those runs were discarded and repeated.
- Stats-panel comparisons for Integrated/RMS Reference were made on running music, so the ~3 dB RMS shift is approximate.

## Metering and visualisation under signal (19 observations)

**Observer summary.** Metering was exercised with nine signal programs plus MATCH/DELTA/BYPASS/host-bypass, A/B, preset, state-load, sample-rate and settings changes (about 110 captures, ~28 reviewed sets). The GR well draws limiter-only GR (gold, 24 dB full height) over a per-block output peak fill (grey) across a 20 s window with no dB scale, time axis, legend, hover readout or numeric GR anywhere in Simple view; compressor GR is invisible there even when the Loudness macro drives 5+ dB of it. The Statistics panel is consistent and precise but its session holds are silently corrupted by plugin BYPASS (TP/SP jump to the dry input's +5.5 dBFS and stay), fully frozen with no stale cue under host bypass (three captures byte-identical), reset by any click on the panel without feedback (and by host state load but not by the preset browser), and never distinguish held/no-signal/live. The TP row is red in both true-peak modes at defaults (exact compare, -0.06 dBTP vs -0.10). MATCH and DELTA leave all numbers unchanged (render tap) without saying so; 'out LUFS' is an exact duplicate of the S row. SPEC is an unlabelled log-f/-90..0 dB in/out plot with fast decay, a 0 dB top that clips hot fundamentals, and no lingering after re-prepare in these tests. The GR|SPEC pill is a single toggle despite its two-segment look, and the tooltips (the only hint that the panel is clickable) show the previous hover's text and get clipped at the window edge. Missing for a mastering engineer: any numeric GR/peak-GR readout, input vs output loudness (delta), a reset control, targets or reference ticks on the bars, an event/bypass marker on the history, and a stale/bypassed indicator.

### V-01 — GR history well: what is drawn, no scale/axis/legend/readout, 20 s window, 24 dB span

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Programs music -18 / music -6 / burst -6 / sine 1k -3 with 'paramtext limGain 12' then 'paramtext limGain 24' (clamped to 18 dB). Captured full UI and 2x crops of the well (x 50-980, y 640-762). Measured orange-line y at x=700/800/900/950 with a pixel scan.
- **Observed.** Orange line hanging from the top = limiter gain reduction (deepest per prepared block); grey area filled from the baseline = post-chain sample peak per block (linear: -3 dBFS sine fills ~71 % of height, -0.5 dB square ~94 %, loud master fills 100 %). Newest data at the right edge, scrolls right-to-left. With 'burst' (3 s cycle) 6.8 cycles fit across the 930 px well = ~20.5 s window. Zero-GR line at screen y=658; steady GR from a -3 dBFS sine with +12 dB lim gain (~8.9 dB GR) drew at y=692, with +18 dB (~14.9 dB GR) at y=715: ratio 57/34=1.68 vs 14.9/8.9=1.67, i.e. linear in dB over a 24 dB full-height span. There is NO dB scale, NO time axis, NO legend, NO hover readout (mouse over the trace shows nothing, clicks pass through), NO numeric GR value or GR peak anywhere in the Simple view.
- **Interpretation.** The well is a qualitative 'shape' display only. A mastering engineer cannot read how many dB of limiting are applied, when (no time ticks), or what the peak reduction was; the 24 dB span and 20 s window are undocumented on screen. The orange/grey semantics are also unlabelled (nothing says 'GR' or 'peak').
- **Code.** e769f33:src/gui/GrHistoryView.h:132 (kWindowSeconds = 20); e769f33:src/gui/LookAndFeel.h:376 (grSpanDb = 24); e769f33:src/gui/GrHistoryView.cpp:221-470 (paintHistory, no axis drawing); e769f33:src/dsp/AnabasisEngine.cpp:775 (push grDb, peak)
- **Captures.** `rt/visuals/01-music-18-well.png`, `rt/visuals/02-music-6-well.png`, `rt/visuals/04-burst-6-well.png`, `rt/visuals/27c-sine-limgain12-well.png`, `rt/visuals/27d-sine-limgain24-editor.png`

### V-02 — Well shows limiter GR only; compressor GR is invisible in Simple view

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Advanced view, Loudness 0: 'paramtext compThreshold -30' (ratio 1.5:1 auto-release), wait 9 s, capture; crop COMP panel and well. Compare with Loudness 70 ('paramtext loudness 70') where the macro sets compThreshold -12 dB / ratio 1.85:1.
- **Observed.** With the comp working (COMP mini-bar under the panel filled ~22 % from the right, i.e. ~5 dB) the well's orange trace stayed flat at the zero line for the whole 20 s; only the grey peak fill got lower. In the Simple view there is no compressor meter at all, so at Loudness 70 % (macro-driven comp threshold -12 dB, ratio 1.85:1, clip drive 5.1 dB) the user sees only the limiter trace.
- **Interpretation.** The one history display in the product describes only one of three gain-affecting stages. In Simple view a user can have 5+ dB of compression and see a flat 'no reduction' line — misleading for the main audience of the Simple view.
- **Code.** e769f33:src/dsp/AnabasisEngine.cpp:1056 (grMinChunk folds limiter gains only) and :769-775 (histMinGain -> push); e769f33:src/dsp/AnabasisEngine.h:344-355 (per-stage GR only for the Advanced panel meters); e769f33:src/gui/PluginEditor.cpp:668-669, 1576, 1609 (mini meters are Advanced-only child components)
- **Captures.** `rt/visuals/12b-adv-comp-only-editor.png`, `rt/visuals/12b-adv-comp-only-comp-panel.png`, `rt/visuals/12b2-adv-comp-only-wellstats.png`, `rt/visuals/12a-adv-loud70-editor.png`

### V-03 — GR trace overlaps the peak fill when the master is loud (legibility)

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Music -6 with the big Loudness knob dragged to 69.2 % (three vertical drags from (360,240)); also 'paramtext loudness 70' later. Captured well crops.
- **Observed.** At high Loudness the grey peak fill reaches the top of the well on every block, and the 1.4 px orange GR trace (sitting ~5-10 dB down) runs through the grey fill. Both are mid-grey/orange on a dark background; the trace is readable but the fill no longer conveys anything (solid block), and the two overlap for the whole window.
- **Interpretation.** The well's two layers were designed for the quiet case; in the case the product is for (pushed master) the waveform layer saturates and the display degrades to 'a line inside a grey block'.
- **Code.** e769f33:src/gui/GrHistoryView.cpp:440-447 (wave height = area.height * peak, clamped to 1)
- **Captures.** `rt/visuals/03-music-6-loud70-well.png`, `rt/visuals/03-music-6-loud70-editor.png`, `rt/visuals/14b-loud70-match-off-editor.png`

### V-04 — GR history reset matrix: cleared only by a re-prepare; no event markers on the timeline

*Severity (observer's estimate): low · confidence: high*

- **Steps.** With music -6 running: click A/B (589,67) [1.2 s and 3 s later], '>' preset (553,67) -> 'Transparent Master' then '<' back, BYPASS (909,67) on/off, GR->SPEC->GR pill clicks, ADV toggle (838,67), 'sr 96000 512', 'load state.bin' (setStateInformation). Captured the well after each.
- **Observed.** A/B: trace continues, no clear, no marker. Preset load: continues (Loudness jumped to 25 %, trace just changes character). BYPASS on: continues; the dim overlay covers the well; fill goes to full height (dry input peaks above ceiling); after bypass off no gap or marker. Pill switch GR->SPEC->GR: history preserved. ADV toggle: preserved (re-laid out into the taller 612x250 well). 'sr 96000 512': cleared — honest empty zero region on the left, new data entering at the right. State load: NOT cleared (only the statistics reset).
- **Interpretation.** Mostly positive (history survives view/tab/AB/preset changes and clears honestly on re-prepare). What is missing: any tick on the timeline showing where an A/B, preset or bypass event happened, so a 20 s trace can mix two different settings with no boundary.
- **Code.** e769f33:src/dsp/GrHistoryBuffer.h:217-223 (prepare clears only on (rate,block) change); e769f33:src/PluginProcessor.cpp:1855-1871 (state load requests meter reset only)
- **Captures.** `rt/visuals/10b2-after-AB-3s-well.png`, `rt/visuals/10c-after-preset-next-editor.png`, `rt/visuals/10d-bypass-on-editor.png`, `rt/visuals/09c-pink-6-back-to-gr-well.png`, `rt/visuals/11a-adv-on-editor.png`, `rt/visuals/12c-sr96k-1.5s-wellstats.png`, `rt/visuals/20b-gr-after-state-load-0.8s-well.png`

### V-05 — Plugin BYPASS keeps metering the bypassed (dry) signal into the same session holds

*Severity (observer's estimate): high · confidence: high*

- **Steps.** Music -6, Loudness 0: click BYPASS (909,67); captures at 1.2 s and 5 s; click BYPASS off; capture 1.2 s later.
- **Observed.** Under BYPASS the editor dims but the Statistics panel keeps running on the dry signal: TP/SP rose from 1.35 dBTP / -0.10 dBFS to 4.55 dBTP / 4.28 dBFS (both red) during bypass and to 5.96 dBTP / 5.53 dBFS after un-bypass; the I row kept integrating the bypassed passage (-4.0 -> -4.2); out LUFS followed the dry level. After un-bypass the panel showed sample peak 5.53 dBFS for a master whose limiter is clamping at -0.10 dBFS.
- **Interpretation.** A/B-by-bypass is the most common mastering gesture; after one bypass the TP/SP session maxima (and PLR) describe the unprocessed input, not the master, and stay wrong until the user discovers the hidden click-to-reset. The dim overlay is the only cue and it disappears the moment bypass is released.
- **Code.** e769f33:src/PluginProcessor.cpp:963-1005 (publishes TP/SP/I holds from the engine render tap every block regardless of the bypass parameter)
- **Captures.** `rt/visuals/10d-bypass-on-editor.png`, `rt/visuals/10d2-bypass-on-5s-editor.png`, `rt/visuals/10d3-bypass-off-editor.png`

### V-06 — Host bypass freezes every meter and the history with no stale indication

*Severity (observer's estimate): high · confidence: high*

- **Steps.** 'hostbypass 1' (processBlockBypassed path) with music -6, Loudness 70; captures at 4 s and 7 s, then repeated: captures at 2 s, 3 s, 4 s and md5 of the editor region; 'hostbypass 0', capture 1.5 s later.
- **Observed.** All three captures during host bypass were byte-identical (md5 a20d5fd0... for the whole 940x720 editor): M/S/I (-8.9/-8.8/-9.1), out LUFS -8.8, TP/SP, LRA, PLR and the GR history did not move at all. Nothing on screen says bypassed or stale — the bars stay orange, numbers look live. On 'hostbypass 0' everything resumed and the history continued with no gap.
- **Interpretation.** A frozen meter that looks live is worse than a blank one: a user host-bypassing the plugin to compare will read the last processed values as current. The same will apply whenever a host stops calling processBlock (transport stopped in most DAWs).
- **Code.** e769f33:src/PluginProcessor.h:140-141 / e769f33:src/PluginProcessor.cpp:722 (getBypassParameter only; no processBlockBypassed override, so JUCE's default passthrough runs and nothing is published); meters published only inside processBlock (e769f33:src/PluginProcessor.cpp:963ff)
- **Captures.** `rt/visuals/15a-hostbypass-repeat-2s-editor.png`, `rt/visuals/15c-hostbypass-repeat-4s-editor.png`, `rt/visuals/15d-hostbypass-off-1.5s-editor.png`, `rt/visuals/14e-hostbypass-on-editor.png`, `rt/visuals/14e2-hostbypass-on-7s-editor.png`

### V-07 — Statistics reset: any press anywhere on the panel resets, with no feedback and no discoverable control

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Simple view: click TP row (720,220); click M row (720,138); double-click I row (720,190); click empty panel area (800,500); right-click (800,400); click the graph well (500,700). Captured 0.6 s and 3 s after each.
- **Observed.** Every press (any row, empty area, double-click, right-click) reset I, LRA (prints '-' then '0.0 LU'), TP and SP holds within one frame; no flash/highlight/confirmation, no context menu on right-click, no button anywhere, nothing in Settings. Clicking the well does nothing. The only hint is a tooltip ('Click to reset the integrated measurement...') and Tooltips are OFF by default.
- **Interpretation.** The reset works and is one click, but it is invisible and too easy to trigger by accident (a stray click while reaching for the ADV toggle wipes a 4-minute integrated measurement). Right-click could carry a menu; the panel offers none.
- **Code.** e769f33:src/gui/LoudnessMeterView.cpp:67-70 (mouseDown -> requestMeterReset, any button); e769f33:src/PluginProcessor.cpp:926-931 (reset consumed at block top)
- **Captures.** `rt/visuals/13a-stats-before-click-stats.png`, `rt/visuals/13b-after-click-TP-row-stats.png`, `rt/visuals/13e-after-dblclick-I-row-stats.png`, `rt/visuals/13g-after-rightclick-panel-editor.png`, `rt/visuals/13h-after-click-well-editor.png`

### V-08 — Statistics reset matrix is inconsistent: host state load resets, preset-browser load does not; transport stop, bypass, A/B and standard change do not

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** 'transport 0' 3 s; A/B click; '>' preset to 'Transparent Master'; BYPASS on/off; 'save state.bin' + 'load state.bin'; Settings > Integrated BS.1770-2+ -> BS.1770-1; 'sr 96000 512'. Captured the panel each time.
- **Observed.** transport 0: everything keeps running (audio still flows). A/B: no reset (I -3.8 unchanged). Preset '>' load: no reset (I -3.9, TP/SP/LRA holds kept). BYPASS: no reset (see V-05). Host state load: full reset (LRA '-', I restarted, TP/SP fresh). Integrated standard change: value switches instantly (-13.4 -> -13.3), no reset, no label change. 'sr 96000': everything reset including S ('-'), LRA ('-').
- **Interpretation.** Two paths that both read as 'loading a preset' behave differently, and the session-cumulative I/TP/SP/LRA/PLR silently span A/B slots, preset changes and bypassed passages. There is no 'measuring since' / elapsed indicator to make the scope of the integration visible.
- **Code.** e769f33:src/PluginProcessor.cpp:1855-1871 (setStateInformation -> requestMeterReset); preset browser path does not call it (no other caller: grep requestMeterReset -> LoudnessMeterView.cpp:69, PluginProcessor.cpp:1871 only)
- **Captures.** `rt/visuals/14a-transport0-3s-stats.png`, `rt/visuals/10b-after-AB-editor.png`, `rt/visuals/10c2-after-preset-next-3s-editor.png`, `rt/visuals/17a-before-state-load-stats.png`, `rt/visuals/17b-after-state-load-0.8s-editor.png`, `rt/visuals/22c-integrated-1770-1-3s-stats.png`, `rt/visuals/12c-sr96k-1.5s-wellstats.png`

### V-09 — 'out LUFS' readout duplicates the S row exactly

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Compared the 'out LUFS' value (Simple view, (600,558)) with the S row in every capture under music -6, Loudness 70, burst, sine.
- **Observed.** Identical in every frame: -11.9/-11.9, -7.1/-7.1, -13.1/-13.1, -3.0/-3.0, -34.5/-34.5 (silence 3 s). Both are the render-tap short-term (3 s) LUFS. The readout disappears in Advanced view; there is no input LUFS, no delta LUFS and no integrated readout next to the knob.
- **Interpretation.** A redundant number occupying the prime spot next to the macro; the one figure a maximiser user wants there (how much louder than the input, or the integrated value) is absent.
- **Code.** e769f33:src/gui/PluginEditor.cpp:2038-2043 (outLufsValue <- meterLufsS()); e769f33:src/gui/LoudnessMeterView.cpp:83 (S row <- meterLufsS())
- **Captures.** `rt/visuals/02-music-6-outlufs.png`, `rt/visuals/02-music-6-stats.png`, `rt/visuals/04-burst-6-editor.png`

### V-10 — No 'no signal' vs 'holding' vs 'stale' indication; silence and reset look identical in the well

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Music -6 for 21 s then 'signal silence'; captured at 3 s, 15 s and 27 s of silence.
- **Observed.** 3 s: M '-', S -34.5, RMS '- dBFS' (the unit stays printed beside the dash), out LUFS -34.5. 15 s and 27 s: M/S/out LUFS '-', I -3.2 / TP 1.35 (red) / SP / LRA 24.2 / PLR 4.6 all hold with unchanged bar colour and no 'held' styling. The GR well scrolls the history out over 20 s and ends as an empty well with the flat orange zero line — exactly the post-reset picture.
- **Interpretation.** Held values are not distinguished from live ones; a user returning to the screen cannot tell whether -3.2 LUFS integrated is still being measured, is a finished measurement, or is polluted by earlier test tones (it was: it included sine/square/sweep programs). '- dBFS' is a formatting slip.
- **Code.** e769f33:src/gui/LoudnessMeterView.cpp:157-158 (fmt prints '-'), :268 (RMS row appends ' dBFS' unconditionally)
- **Captures.** `rt/visuals/08b-silence-3s-editor.png`, `rt/visuals/08c-silence-15s-editor.png`, `rt/visuals/08d-silence-27s-stats.png`

### V-11 — M/S/I bars: fixed -36..0 LUFS range, no target marker, silent saturation above 0 LUFS

*Severity (observer's estimate): low · confidence: high*

- **Steps.** square 200 Hz at -0.5 dB; crop of the panel. Also pink -6, sine -3.
- **Observed.** Square: M/S read +2.0 LUFS with the bar 100 % full (no overflow cue), I -5.4; RMS reads +2.5 dBFS (AES-17 +3.01 offset on a near-full-scale square); TP 1.35 dBTP red. Sine -3: bars ~92 %. No target line (-14, -9 etc.), no ceiling/reference tick anywhere on the bars; a gradient fill only.
- **Interpretation.** The bars carry no reference the eye can use; the numbers do the work. Streaming targets were removed deliberately (owner directive in the source), but not even the user's own Loudness goal or a 'you are here vs input' mark is shown. A positive dBFS RMS is technically right under AES-17 yet reads as an error to most users because the reference is not named on the row.
- **Code.** e769f33:src/gui/LoudnessMeterView.cpp:166-196 (lo=-36, hi=0, jlimit); e769f33:src/gui/LoudnessMeterView.h:33-39 (targets removed); e769f33:src/gui/LoudnessMeterView.cpp:38-51 (AES-17 +3.0103)
- **Captures.** `rt/visuals/06-square200-stats.png`, `rt/visuals/06-square200-editor.png`, `rt/visuals/05-sine1k-3-stats.png`

### V-12 — TP row is red in BOTH true-peak modes at the default ceiling

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Music -6, Loudness 0. TP toggle (597,449) ON (ceiling label shows '-0.10 dBTP'), click panel to reset, wait 20 s, capture. TP OFF, reset, wait 20 s, capture. Earlier 6 s pair as a repeat.
- **Observed.** TP off: 0.49 dBTP in red (genuine inter-sample over; SP -0.10 white). TP on: -0.06 dBTP in red (0.04 dB above the -0.10 ceiling; exact comparison, no tolerance) — repeat run 6 s after reset: -0.07 dBTP red. SP -0.10 dBFS white in both.
- **Interpretation.** The warn colour never turns off at the shipped defaults, so it carries no information: with TP engaged the residual 0.04-0.06 dB (the limiter's/meter's own TP tolerance) still paints red, and a user who just enabled TP to fix the red sees it stay red. The SP row has a 0.005 dB slack; the TP row has none.
- **Code.** e769f33:src/gui/LoudnessMeterView.cpp:243-244 (shownTp > shownCeiling, exact) vs :265-267 (SP row uses kCeilingWarnSlackDb); ADR-0015 open question referenced in the comment at :217-242
- **Captures.** `rt/visuals/28a-tp-on-20s-editor.png`, `rt/visuals/28b-tp-off-20s-editor.png`, `rt/visuals/27a-tp-on-after-reset-stats.png`

### V-13 — MATCH and DELTA do not change any meter (render tap), and nothing says so

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Loudness 70, music -6: capture; MATCH (87,558) on, capture at 4 s; off; DELTA (182,558) on, capture at 4 s.
- **Observed.** MATCH on: M -6.4 / S -7.6 / out LUFS -7.6 / RMS -4.8 — the same range as MATCH off (-8.7/-8.9/-8.9) although the monitored signal is attenuated to match the input. DELTA on: M -6.4 / S -6.5 / out LUFS -6.5 / RMS -1.0 while the user hears only the difference signal. GR well unchanged in both. No badge or label on the panel or the readout indicating 'render (pre-monitor)'.
- **Interpretation.** Metering the programme output regardless of monitor functions is the defensible choice (documented in the source), but the UI gives no hint, so with DELTA on the panel reports -6.5 LUFS for a barely audible difference signal. A one-word tag ('OUT' / 'pre-monitor') on the panel, or dimming under DELTA, would remove the ambiguity.
- **Code.** e769f33:src/PluginProcessor.cpp:963-972 (meters from engine render tap, not the listening buffer)
- **Captures.** `rt/visuals/14b-loud70-match-off-editor.png`, `rt/visuals/14c-loud70-match-on-editor.png`, `rt/visuals/14d-loud70-delta-on-editor.png`

### V-14 — SPEC analyser: unlabelled log-f/-90..0 dB plot, two traces not latency-aligned, fast decay, top clamps at 0 dB

*Severity (observer's estimate): low · confidence: high*

- **Steps.** SPEC tab under sine 1k -3, square 200 -0.5, sweep -6 (3 frames 0.4 s apart), pink -6, silence (0.5/2/8 s), and 'sr 44100 512' during a sine (0.25/0.75/2.75 s).
- **Observed.** Two traces: input dim grey (1 px, 55 % alpha) under the output in gold (1.3 px). No frequency labels, no dB labels, no grid, no legend. Sine 1 kHz peak at 56 % of the width (log 20 Hz-20 kHz), reaching the top. Square -0.5 dB: fundamental drawn as a flat-topped plateau (clipped at the 0 dB ceiling of the plot), harmonics visible. Sweep: a single moving bump with no peak-hold or trail; the grey input trace sits slightly to the right of the gold output (input leads output by the plugin latency). Silence: trace collapses to the bottom line within 0.5 s and the floor draws as a gold line across the bottom, over the pill. Re-prepare during a sine: single trace at 0.25 s, no visible ghost of the old trace (KI-018 linger not provoked).
- **Interpretation.** Usable for 'is there a peak / where roughly', not for reading levels or frequencies; the in/out pair is the useful idea but the input is nearly invisible and the 0 dB top clips a hot master's fundamental into a plateau. A -3 dBFS sine already touches the top, so headroom above 0 dB or a dBFS-per-bin normalisation note is needed.
- **Code.** e769f33:src/gui/SpectrumView.cpp:829-830 (fLo/fHi, dbLo=-90, dbHi=0), :913 (jlimit to dbHi), :922-927 (colours/widths)
- **Captures.** `rt/visuals/05b-sine1k-3-spec-well.png`, `rt/visuals/06b-square200-spec-well.png`, `rt/visuals/07b-sweep-6-spec1-well.png`, `rt/visuals/07d-sweep-6-spec3-well.png`, `rt/visuals/09b-pink-6-spec-well.png`, `rt/visuals/19b-spec-0.25s-after-reprepare-well.png`, `rt/visuals/19e-spec-silence-0.5s-well.png`

### V-15 — GR|SPEC pill is a single toggle: clicking the label of the current mode switches away from it

*Severity (observer's estimate): low · confidence: high*

- **Steps.** On SPEC: click the 'SPEC' half (115,745) -> capture; on GR: click the 'GR' half (78,745) -> capture.
- **Observed.** Clicking 'SPEC' while SPEC was active switched the well to GR; clicking 'GR' while GR was active switched it to SPEC. The pill is drawn as a two-segment control with the active segment highlighted.
- **Interpretation.** Deliberate (0.1.2 item 5) but the look promises 'select', the behaviour is 'flip'. A user who clicks the labelled segment they want will get the other view half the time.
- **Code.** e769f33:src/gui/GrHistoryView.cpp:42-46; e769f33:src/gui/SpectrumView.cpp:154-158; e769f33:src/gui/LookAndFeel.h:296-348 (graph_switch draws two halves)
- **Captures.** `rt/visuals/18a-spec-now-well.png`, `rt/visuals/18b-after-click-SPEC-half-well.png`, `rt/visuals/18c-after-click-GR-half-well.png`

### V-16 — Tooltips (the only reset hint) lag one hover behind, and the Statistics tooltip is clipped at the window edge

*Severity (observer's estimate): medium · confidence: medium*

- **Steps.** Settings > Tooltips ON, popup dismissed. Hover stats (800,300) 2 s; hover M row (720,138) 2 s; hover pill (95,745) 2 s; hover out LUFS (600,558); hover well body. Repeat: hover (300,300) then pill 2 s, then (300,300) 2 s.
- **Observed.** Over the M row the panel tooltip appeared but cut off at the editor's right edge ('Click to reset t|', 'the loudness range an|'). Over the pill the tooltip shown read 'How hard the adaptive chain pushes...' (the Loudness knob's text, the previously hovered component) — and in the first run the stale stats tooltip stayed on screen while a second tooltip appeared elsewhere. Moving back to the knob then showed 'Switch the graph between the spectrum and the GR history' (the pill's text). One tooltip was drawn partly behind the Statistics panel. Reproduced twice.
- **Interpretation.** The meter panel's affordance lives only in a tooltip, and the tooltip system shows the wrong text (previous hover) and truncates the right text. Whether the lag is a JUCE TooltipWindow/xdotool timing artefact or an editor-level shield issue was not determined; the visible outcome was consistent across two runs.
- **Code.** e769f33:src/gui/LoudnessMeterView.cpp:6-10 (tooltip text); tooltip window creation in src/gui/PluginEditor.cpp (not traced)
- **Captures.** `rt/visuals/25b-hover-M-row-editor.png`, `rt/visuals/25c-hover-pill-editor.png`, `rt/visuals/25d-hover-outlufs-editor.png`, `rt/visuals/26a-hover-pill-repeat-editor.png`, `rt/visuals/26b-after-leaving-pill-editor.png`

### V-17 — Simple vs Advanced metering: same Statistics panel; Advanced adds unlabeled per-stage GR bars and a taller well, loses 'out LUFS'

*Severity (observer's estimate): low · confidence: high*

- **Steps.** ADV toggle (838,67) with music -6 and Loudness 70; crops of COMP and LIMITER panels.
- **Observed.** Advanced: identical Statistics rows moved to the right of the well; the well becomes ~612x250 px (same 20 s, same 24 dB, so steeper); COMP and LIMITER each get a 14 px mini-bar filling from the right (limiter ~27 % at Loudness 70 = ~6.5 dB) with no number, no scale, no peak hold; the CLIP/COLOR curve view; the 'out LUFS' readout is gone. Under the COMP panel the compressor GR appears only here.
- **Interpretation.** Neutral-to-positive: the same numbers in both views is consistent (ADR-0020). Missing in both: any numeric GR (current or peak) for any stage.
- **Code.** e769f33:src/gui/PluginEditor.cpp:2084-2097 (mini meters fed per channel); e769f33:src/gui/LoudnessMeterView.h:16-17
- **Captures.** `rt/visuals/11a-adv-on-editor.png`, `rt/visuals/12a-adv-loud70-editor.png`, `rt/visuals/12a-adv-loud70-lim-panel.png`

### V-18 — Standard/reference choices are not labelled on the panel rows

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Settings > Integrated -> BS.1770-1; Settings > RMS Reference -> Mathematical; captures 0.5 s and 3 s after each; then restored.
- **Observed.** Integrated flip: the I number changed instantly (-13.4 -> -13.3 here; the two standards can differ by more), row still reads 'I'. RMS flip: the RMS number dropped by ~3 dB instantly (AES-17 -> Mathematical), row still reads 'RMS ... dBFS'. Nothing in the panel says which standard/reference is active.
- **Interpretation.** Instant switching with no audio involvement is good engineering; but a 3 dB RMS difference between two sessions with no on-panel label will be read as a metering bug.
- **Code.** e769f33:src/gui/LoudnessMeterView.cpp:79-84, 132 (choice resolved in the view; row tags are constant strings at :174 and :266-268)
- **Captures.** `rt/visuals/22b-integrated-1770-1-0.5s-stats.png`, `rt/visuals/23a-rms-mathematical-0.5s-stats.png`, `rt/visuals/26g-settings-closed-editor.png`

### V-19 — Settings combo menus leave a ghost list after a selection (observed while changing metering settings)

*Severity (observer's estimate): low · confidence: low*

- **Steps.** Settings > Integrated combo, click 'BS.1770-2+'; capture 0.8 s later; same for RMS Reference 'AES-17'.
- **Observed.** The combo box text updated (BS.1770-2+ / AES-17) but the dropdown list stayed drawn under it with the bullet still on the previous item (BS.1770-1 / Mathematical) 0.8 s after the click; it was gone in the next capture after clicking elsewhere. Also: the first attempt to reselect BS.1770-2+ (click combo then item 0.8 s apart) did not take.
- **Interpretation.** Outside this area's core; noted because the metering standards live in this popup. Could be a fade-out animation caught mid-way (UI Animations on) or a lingering menu window.
- **Captures.** `rt/visuals/26d-after-select-2plus-editor.png`, `rt/visuals/26f-after-select-aes17-editor.png`

**Positives recorded.**

- Statistics panel is identical in Simple and Advanced views (rows, decimals, colours), so nothing is learned twice; numbers use sensible precision (LUFS 1 dp, TP/SP 2 dp, LRA/PLR 1 dp).
- Meter values are internally consistent: 'out LUFS' == S row every frame; PLR == TP - I on screen; SP -0.10 dBFS exactly at the default ceiling under limiting; RMS -3.0 dBFS for a -3 dBFS sine under AES-17.
- The TP row measures true peak even with true-peak mode off, and the SP row beside it shows the ceiling honoured, so the inter-sample over is visible as a real number (0.49-1.35 dBTP on the test programmes), not hidden.
- GR history survives A/B, preset load, BYPASS, GR/SPEC switching and the ADV re-layout, and clears honestly on a re-prepare with an explicit 'unmeasured' empty region on the left instead of stretched data.
- GR trace scale is linear in dB and shared with the per-stage mini meters (24 dB), so what the LIMITER bar shows 'now' and what the well shows 'over 20 s' agree by construction.
- Peak fill and GR trace scroll continuously with no visible bucket re-phasing between frames 0.5 s apart; the 20 s window measured with the burst ruler matches the documented value.
- Spectrum shows both input and output on a log axis with a fast, jitter-free response to a sweep; the in/out latency offset is even visible.
- Statistics reset is one click and takes effect within one frame; the tooltip text (when enabled) explains exactly what it resets.
- Re-prepare (sample-rate change) resets everything coherently: history, S/M, integrated, LRA, TP/SP holds.
- Integrated standard and RMS reference switch instantly in the view with no audio-thread involvement and no reset side effects.
- BYPASS dims the whole editor (overlay), giving at least a strong visual state while bypassed.
- Ceiling label switches to 'dBTP' when true-peak mode is engaged.

**Limitations recorded.**

- Harness, not a DAW: 'transport 0' only flips isPlaying while audio keeps flowing, so 'reset on transport stop' could not be tested for real; in a host that stops calling processBlock the display will freeze as observed under 'hostbypass 1' (V-06).
- Scrolling smoothness / frame pacing of the GR history cannot be judged from still captures under Xvfb software rendering; only position deltas between frames 0.5 s apart were checked (consistent with ~46 px/s). No OpenGL, no HiDPI, no real display.
- KI-018 (previous spectrum trace lingering after a reset) could not be provoked: the ctl channel serialises commands ~100 ms apart, so a signal change exactly coincident with the re-prepare was not achievable; the sine->re-prepare test showed a single trace at 0.25 s.
- Tooltip lag (V-16) was reproduced twice via xdotool moves; whether a real mouse shows the same one-hover-behind text was not verifiable here, and the root cause was not traced in source.
- One pill click (first click of the 16-series, at (115,745) immediately after a ctl command) did not register and was not reproducible; not reported as a finding.
- Preset-browser load was exercised only through the '>' button ('Transparent Master'); the preset menu itself was not opened in this area.
- The harness 'music' generator peaks above 0 dBFS at level -6 (dry SP +5.5 dBFS under bypass); the absolute TP/SP numbers under bypass are a property of the test signal, the finding (holds polluted by bypass) is not.
- Colour/contrast judgements are from PNGs of a 24-bit Xvfb; no colour-blind simulation performed.

## Edge cases, empty/unavailable states, popups, focus, robustness (22 observations)

**Observer summary.** I covered all 14 items from my own run on :115, plus item (1) on :99; every step is in actions.log. Most dismissal and robustness behaviour is sound: the popup shield consumes the dismissing click, corrupt state blobs are rejected cleanly, the time base is correct across sample rates, and undo/redo holds up. Nine user-facing gaps stand out:
- **Garbage or empty text sets 0 dB:** typing garbage or nothing into the Ceiling field commits 0.00 dB, the least safe ceiling (PluginParameters.cpp:117).
- **Escape does not close Settings or About:** it does close the other popups.
- **Statistics resets on any click:** a click anywhere on the panel, including its empty lower two-thirds, silently resets integrated loudness, LRA and the peak holds.
- **Host bypass freezes the display:** it goes pixel-identical with no bypass indication, while the controls stay live.
- **Keyboard focus is invisible:** Tab and the arrow keys work, but no control shows a focus ring.
- **The preset label is wrong after any state load:** it reads 'Default' without '*' and never marks later edits (by design, since the baseline is not serialized).
- **The preset flow fails silently:** corrupt user presets, empty names and overwrites all fail or proceed without any message.
- **GR displays saturate with no scale:** at heavy reduction the GR trace and meter bars pin at full scale with no dB labels.
- **Small true-peak overshoot in TP mode:** the TP row reads 0.03 dB above the true-peak ceiling, in red.

The no-device Standalone gives no in-plugin explanation for its '-' meters or its blank GR well. Two findings are probably environment artefacts: a warp+click drag-through and lingering popup pixels (E20, E21). app.log has no errors, but it is a Release build, so assertions could not fire.

### E01 — No-device Standalone: the plugin never explains its '-' meters, and the GR well draws nothing

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Own run, DISPLAY=:99. Captured the root window. Clicked Options (527,225), then 'Audio/MIDI Settings...' (584,248). Clicked inside the dialog (960,740) and pressed Escape. The :99 editor was already in Advanced view when I arrived, and I left it that way.
- **Observed.** The JUCE wrapper shows a yellow banner, 'Audio input is muted to avoid feedback loop', with its own 'Settings...' button next to the plugin's 'Settings' button. Statistics reads M/S/I '-', TP '- dBTP', SP/RMS '- dBFS', LRA '-', PLR '-'. The bars are empty. The GR well is completely blank: no 0 dB reference line and no text, only the GR|SPEC chip. The Options menu has Audio/MIDI Settings..., Save current state..., Load a saved state..., Reset to default state. The Audio/MIDI dialog shows 'Feedback Loop: [x] Mute audio input', 'Output: << none >>' with a Test button, 'Input: << none >>', an empty 'Active MIDI inputs' box, and no sample-rate or buffer rows. Escape closed the dialog. The partial run's 01-standalone-nodevice.png, which I viewed, shows the same state.
- **Interpretation.** A user with no output device sees dashes and an empty graph that look broken, not idle. The only hint is the wrapper banner, and it talks about muted input, not the missing device. Two different 'Settings' buttons sit one above the other with different meanings (audio device vs plugin preferences). The Statistics dashes are intended. The blank GR well comes from an early return when the history is empty, which also skips the reference line.
- **Code.** e769f33:src/gui/GrHistoryView.cpp:264 (if (head <= 0) return;), e769f33:src/gui/LoudnessMeterView.cpp:158 ('-' formatting)
- **Captures.** `rt/edges/01-standalone-nodevice.png`, `rt/edges/01-standalone-nodevice-crop.png`, `rt/edges/02-standalone-options-menu.png`, `rt/edges/03-standalone-audio-settings.png`, `rt/edges/04-standalone-after-dialog-escape.png`, `rt/edges.partial-stopped/01-standalone-nodevice.png`

### E02 — Escape does not close the Settings or About overlays, while it does close the other popups

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Own run. Clicked Settings (777,67) and pressed Escape twice. Clicked the logo (125,67) to open About and pressed Escape. For comparison: opened the Settings combo (579,290) + Escape; preset menu (481,67) + Escape; Advanced Limiter Style combo (630,136) + Escape; Save Preset panel + Escape; inline value editor + Escape.
- **Observed.** The Settings panel stayed open after the 1st and 2nd Escape (06, 06b). About stayed open after Escape (57 strip, middle frame) and closed on a click inside the panel. Escape closed each of these: the combo list inside Settings (Settings itself stayed open, and a 2nd Escape did nothing; 10, 11), the preset menu (14), the Advanced combo (21), the Save Preset panel (58b), and the inline value editor, where it cancelled the edit (34a).
- **Interpretation.** Keyboard dismissal is inconsistent. The two modal overlays that users open most often cannot be closed from the keyboard, and a keyboard-only user has no way out of Settings or About. The Backdrop component handles only mouseDown, and no keyPressed override exists anywhere in the editor.
- **Code.** e769f33:src/gui/PluginEditor.h:85-96 (Backdrop::mouseDown only); e769f33:src/gui/PluginEditor.cpp:937 (only the save field has onEscapeKey)
- **Captures.** `rt/edges/05-settings-open.png`, `rt/edges/06-settings-after-escape.png`, `rt/edges/06b-settings-after-2nd-escape.png`, `rt/edges/09-settings-combo-open.png`, `rt/edges/10-settings-combo-after-escape.png`, `rt/edges/11-settings-combo-after-2nd-escape.png`, `rt/edges/57-strip.png`, `rt/edges/58b-save-panel-after-escape-crop.png`

### E03 — Every outside-click dismissal is consumed and never acts on the control under it; the preset-button 'highlight' is ordinary hover

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Own run. First pass used 'xdotool mousemove X Y click 1'. Re-verified in 47 with move, 250 ms hover, then click. Cases: Settings open, click preset name (481,67) or A/B (589,67). Preset menu open, click MATCH (87,558), '>' (553,67), or the preset button itself. Advanced Limiter Style combo open, click limiter TP (649,413). Settings combo open, click outside at (200,620) twice.
- **Observed.** Settings closed, the preset menu did not open, and the preset button showed a lighter pill (07b). Moving away removed it (07c). A hover-only approach produced the identical pill (07e), and the partial run's 08-hover-preset-2x.png / 07b-topbar-2x.png, which I viewed, show the same. Settings dismissed via A/B: A/B stayed on its slot (08). Preset menu: MATCH stayed Off (16); the preset stayed 'Default' after '>' (17b); clicking the own button closed the menu without reopening it (18). Combo + TP click: TP stayed Off in both screenshot and dump (22b). Combo inside Settings: the 1st outside click closed only the list (12b) and the 2nd closed Settings (12c). The hover-then-click re-test (47 strip) gave the same results.
- **Interpretation.** This is the documented PopupShield design: one click dismisses and never acts. It is safe and consistent, but reaching the target always takes two clicks. The earlier worry about a 'button highlighted without opening' is only the hover state. Neutral to positive.
- **Code.** e769f33:src/gui/PluginEditor.h:105-190 (PopupShield rationale)
- **Captures.** `rt/edges/07b-settings-click-preset-t10-crop.png`, `rt/edges/07c-mouse-moved-away-crop.png`, `rt/edges/07e-hover-preset-stepped-crop.png`, `rt/edges/08-settings-dismiss-by-AB-click-crop.png`, `rt/edges/12b-combo-outside-click1.png`, `rt/edges/12c-combo-outside-click2.png`, `rt/edges/13-preset-menu-open.png`, `rt/edges/16-preset-menu-click-on-MATCH.png`, `rt/edges/17b-preset-menu-click-next-arrow-crop.png`, `rt/edges/18-preset-menu-click-own-button-crop.png`, `rt/edges/20-adv-limstyle-combo-open-crop.png`, `rt/edges/22b-adv-combo-outside-click-on-TP-crop.png`, `rt/edges/47-strip.png`, `rt/edges.partial-stopped/08-hover-preset-2x.png`

### E04 — A click or drag anywhere on the Statistics panel silently resets integrated loudness, LRA, PLR and both peak holds

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Own run. (a) Dragged from (830,400) up to (830,150). (b) Waited 6 s, then single-clicked the empty lower area (830,520). (c) With Tooltips on, hovered (830,460).
- **Observed.** (a) At mouse-down, I went from -24.2 to a restarted value, TP from -5.56 to -13.99, and LRA from 6.1 LU to '-' (30 strip). (b) I -24.9 → '-', LRA → '-', PLR → '-', and TP/SP holds reset (31 strip). The empty bottom two-thirds of the panel acts as a reset button. There is no confirmation, no hover change and no undo. The only hint is a tooltip, 'Waveform statistics off the output. Click to reset the integrated measurement, the loudness range and both peak holds.' (64d), and tooltips are off by default.
- **Interpretation.** A stray click, for example to focus the plugin window during a long pass, discards an integrated measurement that may have taken minutes. The whole panel is the hit area, and the behaviour cannot be discovered with default settings. The tooltip wording 'Waveform statistics' is also odd.
- **Code.** e769f33:src/gui/LoudnessMeterView.cpp:67-70 (mouseDown -> requestMeterReset), :8-9 (tooltip text)
- **Captures.** `rt/edges/30-stats-drag-strip.png`, `rt/edges/31-stats-click-strip.png`, `rt/edges/64d-stats-tooltip-2.png`

### E05 — Invalid or empty text in a dB value field commits 0 dB, which on Ceiling is the maximum and least safe value

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Own run, script te.sh. For each input: 'paramtext ceiling -1.0'; double-click the Ceiling readout (517,518); type the text (LC_ALL=C.UTF-8 xdotool type); capture; press Return; capture; dump. Also typed -5 + Escape, and -7 + click away. A single click on the readout does not open the editor (32a).
- **Observed.** Starting from -1.00 dB: 'abc' → 0.00 dB. Empty → 0.00 dB. '1e9' → 0.00 (clamped). '-1e9' → -20.00 (clamped). '-∞': only '-' reached the field → 0.00 dB. '−3' (U+2212): only '3' reached the field → 0.00 dB. '日本語': nothing reached the field → -1.00, unchanged. '-6 dB' → -6.00. '  -3  ' → -3.00. '-2,5' → -2.00 (comma decimal truncated). '0x10' → 0.00. '+5' → 0.00. Escape cancelled, leaving -1.00. Clicking away committed -7.00. The partial run's app.log shows the same pattern (dumps after 10 'paramtext ceiling -1.0' resets read 0.00, 0.00, -20.00, 0.00, 0.00, -3.00, -2.50, 0.00, -4.00, 0.00), but it does not record which input produced which result, so I re-ran every case myself.
- **Interpretation.** A typo or an accidental Return on an empty field moves the ceiling to 0 dBFS, the least safe value, instead of reverting. A European user typing '-2,5' gets -2. Text pasted with a typographic minus becomes a positive number, clamped to 0 dB. The parser feeds unvalidated text to getFloatValue(), which returns 0 on garbage.
- **Code.** e769f33:src/PluginParameters.cpp:117 (dbFrom = removeCharacters("dB ").getFloatValue())
- **Captures.** `rt/edges/33-abc-typed-crop.png`, `rt/edges/33-abc-return-crop.png`, `rt/edges/33-grid-a.png`, `rt/edges/33-grid-b.png`, `rt/edges/33-grid-c.png`, `rt/edges/34a-editor-type-5-escape-crop.png`, `rt/edges/34b-editor-type-7-clickaway-crop.png`

### E06 — The inline value editor clips its text vertically, drops the unit, and uses JUCE-default styling

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Own run: double-clicked the Ceiling readout (517,518) and zoomed the editor 8x. Partial-run evidence: 40-ceiling-dblclick-4x.png, which I viewed. Its filename and the partial app.log's 'paramtext ceiling' / dump pattern show it is the same double-click on Ceiling.
- **Observed.** The editor is a roughly 72x14 box with a grey outline. It shows '-0.10' without 'dB', left-aligned where the readout is centred. The glyph bottoms are cut off by the box (8x zoom). The partial capture also shows a blue JUCE-default caret, and the same blue caret appears in the Save Preset field (60a).
- **Interpretation.** Text entry works but looks unfinished and off-brand. Clipped digits make it harder to confirm what you typed.
- **Code.** e769f33:src/gui/PluginEditor.cpp:1136 (setTextBoxStyle TextBoxBelow, 72x14)
- **Captures.** `rt/edges/32b-ceiling-value-doubleclick-crop.png`, `rt/edges/32b-ceiling-editor-8x.png`, `rt/edges.partial-stopped/40-ceiling-dblclick-4x.png`, `rt/edges/60a-save-panel-prefilled-crop.png`

### E07 — Under host bypass the whole display freezes with no bypass indication, while controls stay live

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Own run: 'hostbypass 1'; captured at t=2, 6 and 9 s. Dragged Tone up (360,465 → 415), clicked TP (597,449), dragged Loudness down. Clicked Statistics. Then 'hostbypass 0'.
- **Observed.** 49a, 49b and 49c are pixel-identical (ImageChops bbox None): the GR history stops scrolling, M/S/I, TP/SP/RMS and out LUFS are frozen, the BYPASS toggle stays off, and there is no dim layer and no text. The controls still respond: Tone 0.32, TP on with the Ceiling unit now 'dBTP', Loudness 48% (49d, confirmed by dump). Clicking Statistics under bypass sets every row to '-', M and S included, like the no-device state (49ef, left). On un-bypass the display resumes (49ef, right).
- **Interpretation.** When a host bypasses the plugin (DAW bypass button, not the plugin's own), the UI looks alive but shows stale numbers. The user can turn knobs and see no metering reaction, with no hint why. A frozen GR trace can also be mistaken for 'no gain reduction'.
- **Captures.** `rt/edges/49a-hostbypass-on.png`, `rt/edges/49c-hostbypass-on-t9.png`, `rt/edges/49d-hostbypass-after-interaction.png`, `rt/edges/49ef-strip.png`

### E08 — Keyboard focus works but is invisible: no focus ring anywhere

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Own run. Clicked the Character knob (165,465) and typed 'abcxyz'. Pressed Up x3. Pressed Tab x16 with the pointer parked outside and diffed each capture with the animated regions masked. Ran a focus probe: click Character, Tab k times, press Up, dump, and revert with Down when something changed. Also Shift+Tab + Up.
- **Observed.** Typed letters changed nothing (dump unchanged, 51b). Up x3 set Character to 0.03 (step 0.01), and the knob looked identical to its unfocused state (51-focus-compare). Across 16 Tab presses the only pixel changes were hover-glow fades on the first presses (tab-knobrow strip). The probe showed the traversal Tab x1 → Tone (Up also moved the macro-driven colourTone and eqTilt), Tab x2 → Ceiling, Tab x3-12 → no slider responds to Up. Shift+Tab from Character → Loudness (Up gave 1%).
- **Interpretation.** The 0.1.1 keyboard-operability work functions (Tab order and arrow keys), but a keyboard user cannot see where focus is, which defeats the purpose. Arrow steps of 0.01 mean about 100 presses for the full range. Letters are harmlessly ignored.
- **Code.** e769f33:src/gui/PluginEditor.cpp:1155-1169 (setWantsKeyboardFocus(true)); src/gui/LookAndFeel.cpp: focus drawing exists only for TextEditor (:723)
- **Captures.** `rt/edges/51b-character-typed-letters.png`, `rt/edges/51c-character-arrow-up3-crop.png`, `rt/edges/51-focus-compare.png`, `rt/edges/tab-knobrow-00-04.png`

### E09 — After a state load the preset label reads 'Default' without '*' and never shows '*' again, even after edits

*Severity (observer's estimate): medium · confidence: high*

- **Steps.** Own run. Set extremes (38e shows 'Default *'), ran 'save state-extremes.bin', reapplied the Default preset, then 'load state-extremes.bin'. Afterwards ran 'paramtext eqTilt 3', and separately dragged the LS Freq knob in the UI (862,175 → 155).
- **Observed.** After the load, the label reads 'Default' with no '*' while Loudness is 100%, Ceiling -20 dB and Input Gain +24 dB (53d). It stayed 'Default' after the eqTilt automation (53e) and after the UI drag that moved LS Freq from 100 to 121 Hz (53f). In a fresh session the same kind of edit does show '*' (38a).
- **Interpretation.** On every project reopen, the preset label claims the untouched factory 'Default' for a heavily modified state, and the modified marker is dead for the rest of the session. This is documented as deliberate (the baseline is not serialized), but the visible label is wrong.
- **Code.** e769f33:src/PluginProcessor.cpp:508-512 (baseline invalidated on setStateInformation; presetDirty() false while absent); e769f33:src/gui/PluginEditor.cpp:2160-2165
- **Captures.** `rt/edges/38e-advanced-all-extremes.png`, `rt/edges/53d-after-valid-state-load.png`, `rt/edges/53e-after-load-then-edit-crop.png`, `rt/edges/53f-after-load-ui-drag-crop.png`

### E10 — Corrupt state blobs are silently ignored: the UI survives, nothing changes, no error is shown

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Own run: set Loudness to 40% as a marker, then 'load /etc/hostname' (3 bytes), a truncated real state (100 and 4000 bytes), and 8 KB of /dev/urandom. Then loaded the valid state-extremes.bin.
- **Observed.** app.log shows 'loaded 3 bytes', 'loaded 100 bytes', 'loaded 4000 bytes' and 'loaded 8192 bytes'. After each, the dump is unchanged (Loudness 40%, Ceiling -0.10, Input Gain 0.0). The UI renders normally (53b) with no message. The valid blob restored everything, including Advanced view (53d), and reset the statistics.
- **Interpretation.** The processor is robust: no crash and no partial application. The flip side is that a project whose state failed to restore opens with whatever settings were current, with no warning to the user.
- **Captures.** `rt/edges/53b-after-load-etc-hostname.png`, `rt/edges/53d-after-valid-state-load.png`

### E11 — Preset file edge cases fail silently: corrupt user presets, empty names, stripped names, silent overwrite

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Own run, HOME=$E/home. Save Preset with the name cleared, then clicked Save. Saved the name 'Edge/Test:1'. Saved 'EdgeTest1' again after an edit and watched the file stat. Wrote Corrupt.anabasis (plain text) and WrongRoot.anabasis (XML with the wrong root) into the preset dir and picked each from the menu. Opened Load Preset… and pressed Escape.
- **Observed.** Empty name: Save does nothing, the panel stays open, the field loses its focus outline, and the Save button is never disabled (59a). 'Edge/Test:1' was saved as EdgeTest1.anabasis with no notice (59c). Re-saving overwrote the file (mtime 1790412022 → 1790412038, size 2017 → 2060) with no confirmation (60a/60b). The Save field is prefilled with the current name, including the factory name 'Default' (58a). Corrupt and WrongRoot appear under USER (62a). Picking either does nothing: the label stays 'EdgeTest1 *' and no error is shown (62bc). Load Preset… opens JUCE's generic non-native chooser in default grey/teal styling (61a); Escape closed it.
- **Interpretation.** No error surfaces anywhere in the preset flow. A user whose preset file is damaged clicks it and nothing happens. Overwriting a preset has no guard. The generic chooser only matters on Linux.
- **Code.** e769f33:src/gui/PluginEditor.cpp:941-962 (createLegalFileName; empty name → silent return; no overwrite check)
- **Captures.** `rt/edges/58a-save-preset-panel.png`, `rt/edges/59a-save-empty-name-clicked-save-crop.png`, `rt/edges/59c-preset-menu-with-user.png`, `rt/edges/60a-save-panel-prefilled-crop.png`, `rt/edges/61a-load-preset-chooser.png`, `rt/edges/62a-menu-with-corrupt-user.png`, `rt/edges/62bc-strip.png`

### E12 — Extreme values: no readout overflow or arc overshoot, but GR displays pin at full scale with no scale or over marker

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Own run: 'param loudness 1', 'paramtext ceiling -20', 'paramtext inputGain 24', 'param compRatio 1', 'paramtext lookahead 20'. Captured Simple and Advanced. The partial app.log shows the same five commands (lines 606-614); I did not rely on its screenshots.
- **Observed.** The readouts '100 %', '-20.00 dB', '24.0 dB', '4.00:1' and '10.0 ms' all fit, with no truncation. The Loudness arc ends exactly at the end stop, and the Ceiling arc is empty at -20. Lookahead 20 was silently clamped to 10.0 ms. In the GR well, the orange trace pins to the bottom edge; the well has no dB labels, so the amount of GR cannot be read. The trace runs through the translucent GR|SPEC chip, over the 'GR' and 'SPEC' letters (38d-chip-4x). The limiter GR bar is pinned full-width and the comp bar is right-anchored, with no number and no over-range cue (38e crops). The macro-detach dot for Ratio sits in the gap between Ratio and Threshold, closer to Threshold (38e crop). After the ceiling drop, the SP/TP rows turn red from holds recorded before the change (38b) until a manual reset.
- **Interpretation.** Layout survives the extremes. At heavy GR, though, every GR display saturates silently, so the user cannot tell 12 dB from 30 dB of reduction. The badge placement makes it ambiguous which knob is detached.
- **Code.** e769f33:src/gui/PluginEditor.cpp:1362 (badge at k->getRight()-10 of the slider bounds)
- **Captures.** `rt/edges/38a-loudness-max-simple.png`, `rt/edges/38b-ceiling-min-simple.png`, `rt/edges/38c-ceiling-min-later-crop.png`, `rt/edges/38d-simple-all-extremes.png`, `rt/edges/38d-chip-4x.png`, `rt/edges/38e-advanced-all-extremes.png`, `rt/edges/38e-advanced-all-extremes-crop.png`, `rt/edges/38e-lim-meter-3x.png`, `rt/edges/38e-comp-meter-3x.png`

### E13 — 50 Hz square at +6 dBFS: no input-over indicator; only the output TP row turns red

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Own run: 'signal square 50', 'level 6', clicked Statistics to reset, then captured Simple and, after re-entering the square, Advanced.
- **Observed.** Statistics: M/S/I -0.6 LUFS, TP 1.75 dBTP in red (TP mode off), SP -0.10 dBFS not red, RMS 2.9 dBFS, i.e. above SP because of the AES-17 +3 dB reference. The GR trace sits flat at about -6 dB and the limiter bar shows about 6 dB. Nothing indicates that the input itself is 6 dB over full scale; there is no input meter.
- **Interpretation.** The output is correctly limited, and inter-sample overs are flagged. A hot or clipping input is invisible, and RMS reading above the sample peak can puzzle users who do not know the AES-17 convention.
- **Code.** e769f33:src/gui/LoudnessMeterView.cpp:243-244, :265-267 (TP/SP warn rules)
- **Captures.** `rt/edges/40a-square50-plus6-simple.png`, `rt/edges/50a-square-plus6-advanced.png`

### E14 — TP mode on: the TP row repeatedly reads 0.03 dB above the true-peak ceiling (red)

*Severity (observer's estimate): medium · confidence: medium*

- **Steps.** Own run. TP mode on (clicked in 49d), Ceiling -0.10 dBTP, Loudness 48%, music at -6 dB. Clicked Statistics to reset and waited 12 s; repeated with a 15 s wait.
- **Observed.** The TP row showed -0.07 dBTP in red both times (49g, 49h), against a ceiling of -0.10 dBTP. SP was -0.10 dBFS.
- **Interpretation.** With the TP limiter engaged, the plugin's own meter reports a small true-peak overshoot and colours it as a violation. Either the limiter overshoots or its TP estimator disagrees with the meter's. Either way the user sees a red warning in the mode that is supposed to guarantee the ceiling. Handing this to the DSP audit.
- **Captures.** `rt/edges/49g-tpmode-12s-after-reset-crop.png`, `rt/edges/49h-tpmode-repeat-crop.png`

### E15 — Sample-rate and block changes: history clears, scroll speed holds, 384 kHz spectrum loses low-frequency detail

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Own run: music at -6 dB, Loudness 60%. 'sr 384000 32', captures 2 s apart, SPEC view. Then 'sr 44100 4096' with GR and SPEC captures. Then 'sr 48000 512'.
- **Observed.** app.log: 'reprepared: sr=384000 block=32 latency=3840', then 'sr=44100 block=4096 latency=441', then 'sr=48000 block=512 latency=480'. The GR history cleared on reprepare (empty left region). Scroll speed was 96 px per 2 s at both 48k and 384k and 97 px per 2 s at 44.1k/4096 (45 and 46 strips). At 44.1k/4096 the GR trace and waveform are visibly stepped, about 93 ms per step (48a). At 384k the spectrum's left ~40% is a smooth flat line, while 48k and 44.1k show structure there (48 strip). Statistics restarted after each reprepare. No hang and no visual corruption.
- **Interpretation.** The time base is correct across rates. The 384 kHz spectrum degrades because a fixed 4096-point FFT gives about 94 Hz bins, so the log 20 Hz-20 kHz axis interpolates the low end. Large host blocks make the GR history coarse. Both are cosmetic.
- **Code.** e769f33:src/gui/SpectrumView.h:60 (kOrder = 12, 4096-point FFT); e769f33:src/gui/SpectrumView.cpp:829 (fLo 20 / fHi 20000)
- **Captures.** `rt/edges/45-gr-well-48k-vs-384k.png`, `rt/edges/45d-sr384k-t2.png`, `rt/edges/46-gr-well-44k-4096.png`, `rt/edges/48a-sr44k4096-loud60-gr-crop.png`, `rt/edges/48-spec-384k-48k-44k.png`

### E16 — Host automation during a knob drag: the knob jumps twice and the drag wins

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Own run: 'paramtext ceiling -6'. Mouse-down on the Ceiling knob (517,465) and moved up 10 px. Appended 'param ceiling 0.2' while still holding. Continued up 10 px, then released. Dumped after each step.
- **Observed.** The dumps read -5.36 dB, then -16.00 dB (automation; the knob visibly jumped while held), then -4.56 dB after the drag continued (the drag resumes from its own anchor and overwrites the automation), and -4.56 dB after release (52 strip).
- **Interpretation.** This is standard JUCE behaviour. The user sees the knob snap away and back, and automation written during a touch is lost. Worth knowing for touch/latch automation; not a defect in itself.
- **Captures.** `rt/edges/52-strip.png`

### E17 — Smallest UI scale (XS): everything fits, but the smallest captions are marginal at 1x

*Severity (observer's estimate): low · confidence: medium*

- **Steps.** Own run: Settings → UI Scale → XS. Captured Simple and Advanced, made 2x crops and a native 1x crop, then set it back to M.
- **Observed.** editor-bounds became w=705 h=540 (Simple) and 617 (Advanced), with the origin shifting to (43,43). No label is truncated or overlapping. At 2x every caption, stat tag and top-bar label is readable. At 1x the dim-grey captions (Character/Tone/Ceiling, 'out LUFS', LOCK, stat tags M/S/I/TP) are about 8-9 px and hard to read (36b native crop). The Settings panel and its dropdown scale with the editor (36a, 37a).
- **Interpretation.** XS is usable for layout. Legibility of secondary captions at 1x on a normal-DPI screen is borderline, mostly because the text is dim grey on near-black.
- **Captures.** `rt/edges/36a-xs-with-settings.png`, `rt/edges/36b-xs-simple.png`, `rt/edges/36b-xs-topbar-2x.png`, `rt/edges/36b-xs-controls-2x.png`, `rt/edges/36b-xs-stats-2x.png`, `rt/edges/36b-xs-well-2x.png`, `rt/edges/36b-xs-native-1x-small-labels.png`, `rt/edges/36c-xs-advanced.png`, `rt/edges/36c-xs-adv-left-2x.png`, `rt/edges/36c-xs-adv-right-2x.png`, `rt/edges/36c-xs-adv-bottom-2x.png`

### E18 — Rapid and double clicks: every click counts, with no glitches; undo/redo hammering is robust

*Severity (observer's estimate): none · confidence: high*

- **Steps.** Own run. Triple-click A/B (589,67; 100 ms apart). Triple-click and double-click ADV (838,67), counting editor-bounds log lines. Double-click then single-click the Advanced TP toggle (649,413). 25 rapid undo clicks (688,67), then 25 redo clicks (719,67).
- **Observed.** A/B went from A to B, i.e. 3 toggles (24). ADV triple-click ended in Advanced with one logged resize (coalesced); ADV double-click logged 720 then 822 and stayed Advanced. TP double-click was a net no-change (Off); a single click turned it On and the Ceiling unit changed to 'dBTP' (26, 26b). 25 undos walked all the way back through a preset apply and a state load to the loaded extremes state, including switching the view back to Advanced; undo then greyed out (63a). 25 redos returned exactly to 'EdgeTest1 *' with Tilt 1.0 dB (63b).
- **Interpretation.** This is positive. Note that A/B toggles on every mouseDown, so a double-click is a no-op, which is expected. The undo history reaching back through a state load is intended, but a user hammering undo can land on very different settings; here the label read 'Default' while showing +24 dB input gain.
- **Code.** e769f33:src/gui/PluginEditor.h:198 (ABControl toggles on mouseDown)
- **Captures.** `rt/edges/24-ab-tripleclick-crop.png`, `rt/edges/25-adv-tripleclick.png`, `rt/edges/25b-adv-doubleclick-crop.png`, `rt/edges/26-tp-doubleclick-crop.png`, `rt/edges/26b-tp-singleclick-crop.png`, `rt/edges/63a-undo-x25.png`, `rt/edges/63b-redo-x25-crop.png`

### E19 — Drag gestures: knob capture holds, horizontal drags do nothing, the graph well ignores drags

*Severity (observer's estimate): low · confidence: high*

- **Steps.** Own run. Dragged horizontally from Tone (360,465) to Ceiling (517,465). Dragged vertically from Tone up onto the Loudness knob (360,240). Dragged inside the graph well from (500,700) to (300,690).
- **Observed.** The horizontal drag changed nothing (dump: Tone 0.00, Ceiling -0.10). The vertical drag set Tone to 1.00 while Loudness stayed at 0% even with the pointer over it; the label gained '*' (28a/28b). The graph-well drag did nothing: no pan, zoom or selection, and it did not switch GR/SPEC (29a/29b).
- **Interpretation.** Capture behaviour is correct. Knobs are vertical-only (RotaryVerticalDrag), so users who drag sideways or circularly get no response and no feedback. That is a minor expectation mismatch; many mastering plugins accept both directions.
- **Code.** e769f33:src/gui/PluginEditor.cpp:1129 (RotaryVerticalDrag)
- **Captures.** `rt/edges/27a-drag-tone-to-ceiling-held-crop.png`, `rt/edges/28a-drag-tone-up-onto-loudness-held.png`, `rt/edges/28b-drag-tone-up-onto-loudness-released.png`, `rt/edges/29a-drag-graph-held-crop.png`, `rt/edges/29b-drag-graph-released-crop.png`

### E20 — Harness artefact: an instant pointer warp plus press can drag the control previously under the pointer, not reproducible with hover

*Severity (observer's estimate): low · confidence: low*

- **Steps.** Own run. fall.sh: preset button, then 'xdotool mousemove 474 176 click 1' on 'Loud Pop' (Loudness 60%), then 'mousemove 87 558 click 1' on MATCH, 7 trials. fall2.sh: the same with a 150 ms hover before each press, 16 trials. Also 'mousemove 115 745 click 1' on the SPEC chip from a pointer resting at (467,130), and an ADV warp-click from (467,130).
- **Observed.** With warp-click, trials r3 and r5 of 7: MATCH did not toggle and Loudness fell 60% → 0%, the label becoming 'Loud Pop *' (fall-r3/r5 strips). The SPEC warp-click did not switch the view and Loudness fell 60% → 0% (loudness-value-trace). The ADV warp-click did not toggle ADV and Loudness rose 0 → 25.2%, which equals the 63 px vertical offset divided by 250 px. GR/SPEC chip warp-clicks failed 1 of 5. With a hover pause first: 0/16 preset trials and 0/6 chip trials failed. Separately, once (54a) the Color-model combo was found open after a preset pick and a logo click. The 56 strip shows how a sequence can land an item-position click on that combo when an undismissed About swallows the preceding click. I could not reproduce 54a exactly.
- **Interpretation.** This almost certainly comes from synthetic X11 input: no motion is processed at the new position before the press. It should not affect physical mice. It may be worth a spot-check on Linux with touchscreens, pen tablets or VNC/RDP, where pointer warps precede clicks; there, an innocent click could move the Loudness macro by hundreds of percent-equivalent. Other observers using 'mousemove X Y click 1' should treat unexpected knob jumps with suspicion.
- **Captures.** `rt/edges/fall-r3-strip.png`, `rt/edges/fall-r5-strip.png`, `rt/edges/loudness-value-trace.png`, `rt/edges/41-strip.png`, `rt/edges/54a-about-open.png`, `rt/edges/56-strip.png`

### E21 — Popup pixels linger after the popup window is gone (Xvfb repaint)

*Severity (observer's estimate): low · confidence: low*

- **Steps.** Own run on :115: opened the Comp Detector combo (164,136), clicked 'Peak' (100,185), captured at 0.15, 0.5, 1.5 and 3 s without moving, then moved 1 px. On :99: Options → Audio/MIDI Settings. Partial-run evidence (viewed): 33-sel-before-move-crop.png and 33-sel-after-1px-move-crop.png, the same experiment on the same combo.
- **Observed.** The value changed to Peak immediately and the combo's X window was unmapped (xdotool listed only 2 windows at 3 s), yet the list image stayed on screen until the 1 px move (23 strip). On :99 the Options-menu image stayed drawn behind the Audio/MIDI dialog until the dialog closed (03, 03b, 04).
- **Interpretation.** This is most likely an Xvfb artefact: there is no compositor, and the uncovered region is not repainted until the next pointer event. I would not expect it on macOS, Windows or a composited Linux desktop. Recorded so it is not re-reported as a plugin bug.
- **Captures.** `rt/edges/23-sel-sequence-strip.png`, `rt/edges/03b-standalone-audio-settings-later.png`, `rt/edges.partial-stopped/33-sel-before-move-crop.png`, `rt/edges.partial-stopped/33-sel-after-1px-move-crop.png`

### E22 — app.log scan: no assertions, errors or warnings

*Severity (observer's estimate): none · confidence: high*

- **Steps.** Read the whole app.log (8987 lines) and grouped message types. Grepped for assert, jassert, error, warn, fail, exception, abort, nan and inf. Did the same for the partial run's app.log and for xvfb.log.
- **Observed.** Every line has the [harness] prefix. The message types are only cmd echoes, dumps, 'param X -> ...', 'reprepared', 'loaded N bytes', 'saved 8401 bytes' and editor-bounds. There is no assertion or error line, and the partial log is the same. xvfb.log has only xkbcomp 'Could not resolve keysym' warnings, which come from the environment.
- **Interpretation.** Clean, but weak evidence: the harness is a Release build, so jassert is compiled out and could not fire.

**Positives recorded.**

- The PopupShield model is consistent: every outside click dismisses exactly one layer and never acts on the control underneath (combo inside Settings: 1st click closes the list, 2nd closes Settings). This was re-verified with realistic hover-then-click input.
- Escape correctly closes combo lists, the preset menu, the Save Preset panel and the inline value editor, where it cancels without committing.
- Knob drags keep mouse capture: dragging from one knob across another changes only the originating knob.
- Rapid clicking on A/B, ADV and the toggles is handled cleanly. 25 rapid undos followed by 25 redos return exactly to the starting state, including view mode.
- Corrupt, truncated and random state blobs (3 B to 8 KB) are rejected without crashing, without partial application and without UI damage. A valid blob restores everything, including Advanced view.
- The GR history time base is correct at 44.1k/4096, 48k/512 and 384k/32 (same px/s). Reprepare clears the history cleanly and re-reports latency (3840/441/480 samples).
- The inline value editor accepts unit suffixes and whitespace ('-6 dB', '  -3  ') and clamps out-of-range values ('1e9', '-1e9') to the range ends.
- The TP toggle immediately relabels the ceiling readout as dBTP, in both Simple and Advanced views.
- Keyboard operability exists: clicking a knob gives it focus, arrow keys adjust it, and Tab/Shift+Tab traverse Loudness → Character → Tone → Ceiling.
- At XS scale no label is truncated or overlapping, and the Settings panel and dropdown lists scale with the editor.
- Illegal filename characters in preset names are sanitised (createLegalFileName), so no path escapes the preset folder.
- The Statistics tooltip clearly documents the click-to-reset behaviour, but only when Tooltips is enabled.

**Limitations recorded.**

- Unicode text entry is inconclusive. '∞', U+2212 and CJK characters never reached the JUCE TextEditor through xdotool on Xvfb, even with a UTF-8 locale. I cannot tell whether the editor filters them or the synthetic X11 key mapping lost them. IME and paste were not tested.
- Several click-routing anomalies (E20) occur only with instantaneous xdotool warp+press and vanish with a hover pause, so their real-world relevance (touch, pen, remote desktop) is unverified. The popup-pixel lingering (E21) is probably an Xvfb/no-compositor artefact. Neither can be confirmed without a real desktop or host.
- The harness is a Release build, so jassert/DBG output cannot appear. 'No assertions' in app.log does not mean no assertion would fire in Debug.
- The audio output is not audible here: glitches during rapid A/B toggling, sample-rate changes or bypass could not be heard. Only meters and dumps were used.
- Host bypass was simulated with the harness's processBlockBypassed path. How real DAWs (Pro Tools, Logic, Reaper) show their own bypass, and whether they keep calling the editor, was not tested.
- Linux X11 only. Native file choosers, macOS/Windows focus rings and HiDPI scaling were not tested.
- Keyboard focus traversal was probed only with Up (sliders). Where Tab lands among buttons and combos after Ceiling was not identified, because nothing on screen indicates focus.
- The partial run's app.log records ctl commands but not xdotool actions, so I re-ran every numbered item myself. I cite partial screenshots only where I viewed them and their filenames make the steps unambiguous (01-standalone-nodevice, 08-hover-preset-2x, 07b-topbar-2x, 40-ceiling-dblclick-4x, 33-sel-before/after-1px-move).

## Candidates the consolidation dropped

The merge step of the audit's third phase de-duplicated the three domain consolidators' candidates into the finding list. These candidates were not carried; the reason recorded at the merge step follows each:

- `A-46` — Environment artefact. The post-pop-up click misroute (ST-01, E20, ST-21) appears only when xdotool warps the pointer and presses without first delivering motion, the same mechanism as the known warp artefact. E20 labels it a harness artefact: 0/16 failures with a 150 ms hover. ST-01 was normal with continuous motion. The touch, pen and VNC spot check it still needs is carried as a residual in TEST-002.
- `A-47` — Environment artefact. E21 found the combo's X window already unmapped while its pixels stayed on screen until a 1 px move, which is Xvfb with no compositor. The composited-desktop check is carried as a residual in TEST-002.
- `A-04#host-bypass-indication` — Harness artefact. The harness 'hostbypass' calls processBlockBypassed directly, but the JUCE 9.0.1 VST3 wrapper calls it only when getBypassParameter()==nullptr (build/_deps/juce-src/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp:3728-3731, checked). Anabasis returns pid::bypass (e769f33:src/PluginProcessor.cpp:722-725). A-04's plugin-BYPASS half is kept as UX-008, and its frozen-display half is merged into VIS-005.
- `A-59` — Not a finding. Automation arriving mid-drag makes the knob jump and the drag wins, which is standard JUCE touch-override behaviour. The observer (E16) judged it not a defect, and the harness 'param' write is a bracketed touch-style gesture, not host playback.
- `B-47` — Duplicate of A-59, and not a finding for the same reason.
- `A-60` — Not a finding. The combo boxes ignoring the wheel, and Down highlighting before selecting, are JUCE ComboBox defaults. The observer (G-12) rated them neutral, and a wheel-disabled combo avoids accidental mode changes.
- `ST-13#bypass-dirties-preset` — Unsupported. The name was already 'AuditTest *' before bypass (ST-07/ST-08), and view-tier toggles are excluded from the dirty projection (e769f33:tests/state_tests.cpp:2404-2465).
- `PF-anamorph-reference-3` — Refuted. Typed value-box entry is bracketed in the pinned JUCE: Slider::Pimpl::textChanged opens a ScopedDragNotification (juce_Slider.cpp:445-456), and G-14 showed a typed Loudness re-engaging. The real gesture-less path is the arrow keys (STATE-007).
- `PF-ui-architecture-19` — Not observed. LAY-17 measured the wordmark at 60-190 px and the subtitle at 205-360 px on Linux, with no collision. macOS and Windows font metrics are untested.
- `V-16#tooltip-clipped-behind-panel` — Environment artefact: tooltip stacking and clipping under Xvfb with no window manager. It was not independently confirmed.
- `G-21/LAY-14#stale-tooltip` — Known environment artefact: xdotool warp without motion. It is used only as non-confirming context in UI-008, which rests on Anamorph's documented reproduction of the shared JUCE mechanism.
- `ST-22/LAY-09#window-not-following` — Known environment artefact: the harness window does not follow UI-scale resizes.
- `LAY-10#harness-border-resize` — Environment artefact: the resize path through the harness border is harness-only.
- `G-02#no-LOCK-tooltip-captured` — Environment artefact. The source has a LOCK tooltip (e769f33:src/gui/PluginEditor.cpp:609-612).
- `ST-17#frozen-trim-unchanged` — Observer misread. limGain is a macro-managed parameter, not an adaptive trim, and the trims have no readout (VIS-011). The KI-006 audio half is therefore unverified (STATE-004).
- `B-43#undo-cleared-on-load` — Not a separate finding. Clearing plugin undo on a host state load is standard behaviour, so it is kept only as context in VIS-009.
- `E22` — Not a finding. It is an audit limitation (a Release-build harness in which jassert cannot fire), recorded in TEST-002.
- `KI-018-corner/KI-011` — Not assessed. The spectrum's one-tick stale trace after a reset could not be provoked, and no observer evaluated the KI-011 restore notifications, so there is no evidence to report.

## Verifier notes set aside at triage

Verifiers recorded material they noticed outside their batch. Each note was disposed of at triage: *duplicate* (already covered by the named finding), *merge-into* (added as evidence to the named finding, where the report shows it), *new* (written up as its own finding) or *drop*. Duplicates and drops are listed here; merge-into notes appear in the finding records.

- **duplicate → UI-013.** Same defect: the dirty marker is ellipsised away on 'Transparent Master *'. UI-013 already has the verify-12 captures, the 106 px geometry and the reserved-marker fix. *(note: The top-bar preset name truncates with an ellipsis and drops the ' *' edited marker for long names. After detaching Limiter Gain on 'Transparent Master', the button read 'Transparent Mast…' with no '*' visible. Evidence: /tmp/claude-0/-home-user/52dd522c-a346-…)*
- **drop.** Audit-evidence hygiene: the observer anchors in obs/visuals.md are wrong. This is not a product problem. UX-002 and VIS-002 already carry the corrected anchors (mouseDown :67-70, TP warn :243-244). Synthesis must cite finding-level anchors only. *(note: Evidence hygiene: the code cross-references in the observer file obs/visuals.md do not match e769f33. V-07 cites LoudnessMeterView.cpp:194-197, V-10 cites :284-285 and :395, and V-11 cites :293-323 and :165-178, but the file has 281 lines. The real anchors are…)*
- **duplicate → VIS-012.** VIS-012's evidence and corrections already include the LRA warm-up: '0.1 LU at +4 s' (rt/verify-1/33-sameprep-strip.png), LoudnessMeter.h:290-291, and the manual's 'steady master reads near 0' (USER_MANUAL.md:232). *(note: LRA warm-up reads as a final value. About 3-4 s after any reset or prepare, the LRA row prints a near-zero figure (0.1 LU at +4 s after a same-pair re-prepare; rt/verify-1/33-sameprep-strip.png), because computeLraLu returns a range once two short-term values…)*
- **duplicate → UX-002.** UX-002's corrections record the missing keyboard path and accessible name (LoudnessMeterView.cpp:53-57, HANDOVER.md:1337). Its proposal adds a focusable 'Reset statistics' control. The wider naming gap for other mouse-only controls is new record INPUT-016. *(note: The STATISTICS reset has no keyboard path and no accessible name. LoudnessMeterView sets no title or description and is not focusable (e769f33:src/gui/LoudnessMeterView.cpp:53-57; e769f33:src/gui/LoudnessMeterView.h:42-43), while HANDOVER claims 'Accessibility…)*
- **duplicate → VIS-014.** Same claim and anchors (LoudnessMeterView.cpp:79, 84, 174): the I and PLR rows do not show the active Integrated standard. *(note: The I row does not say which integrated standard it shows. The Settings 'Integrated' flip (BS.1770-2+ gated vs BS.1770-1 ungated) changes I and the derived PLR instantly, with no row label or tag change (e769f33:src/gui/LoudnessMeterView.cpp:79, 84, 174-184; r…)*
- **duplicate → DOC-002.** DOC-002's current_behavior and corrections already record that the reset also blanks out LUFS and the COMP/LIMITER GR mini-meters when no audio flows, using the same verify-1/11 and 22 captures and the state_tests.cpp:1343-1357 pin. *(note: The Statistics click reaches beyond the panel when no audio flows: it blanks the Simple 'out LUFS' readout (e769f33:src/gui/PluginEditor.cpp:2040-2043) and the Advanced COMP/LIMITER GR mini-meters (e769f33:src/PluginProcessor.cpp:883; e769f33:src/dsp/AnabasisE…)*
- **duplicate → DSP-002.** DSP-002 correction 5 and STATE-006's corrections and proposal already record the contradiction between ADR-0018:93-95 'inaudible-by-design' and KI-010. *(note: ADR-0018's premise that the ADV-only undo duck is 'inaudible-by-design' (e769f33:docs/architecture/design-decisions/ADR-0018-copy-and-advanced-join-the-undo-history.md:93-95) contradicts KI-010 (e769f33:docs/KNOWN_ISSUES.md:690-712: every undo step dips the pr…)*
- **duplicate → VIS-016.** Same duplication: out LUFS and the S row both read meterLufsS(). *(note: Simple view shows the same short-term loudness twice: 'out LUFS' (e769f33:src/gui/PluginEditor.cpp:2038-2043, proc.meterLufsS()) and the Statistics 'S' row (e769f33:src/gui/LoudnessMeterView.cpp:83, processor.meterLufsS()). Both read -27.3 in /tmp/claude-0/-ho…)*
- **duplicate → DOC-008.** DOC-008 already covers DESIGN §6.3's 940x900 frame, the read-only macro row and the missing per-section supersession banner (DESIGN.md:916-931). *(note: DESIGN §6.3 (e769f33:docs/DESIGN.md:916-931) still draws a 940×900 Advanced frame with the read-only macro row and a ceiling lock (🔒) in the LIMITER zone. It carries no 'Superseded in part' banner, unlike the §4.3/§5 sections (e769f33:docs/DESIGN.md:214,535,57…)*
- **duplicate → UX-004.** UX-004's evidence cites USER_MANUAL.md:293-294 'The Ceiling LOCK lives next to the Ceiling knob itself (true only in Simple)'. *(note: USER_MANUAL §3.5 says 'The Ceiling LOCK lives next to the Ceiling knob itself' (e769f33:docs/user/USER_MANUAL.md:293-294). This is true only in Simple: the Advanced LIMITER Ceiling (e769f33:src/gui/PluginEditor.cpp:1602-1607) has no LOCK beside it.)*
- **duplicate → UX-006.** The tooltips-off default and its effect on tooltip-only explanations are the subject of UX-006. Other findings already note it as a constraint on remedies. *(note: Tooltips default to OFF (e769f33:src/gui/PluginEditor.cpp:972: tooltipsOn = ist.getProperty (iid::tooltipsOn, false)). Any remedy anywhere in the audit that relies only on a tooltip to explain a state will be invisible to a default user.)*
- **duplicate → UX-005.** UX-005 correction 5 already records that learnStartedMs is editor-local (PluginEditor.h:557), so a reopened editor has no countdown and no minimum-pass guard. The lost empty-pass flash is part of the same editor-local state. *(note: Editor-local Learn bookkeeping does not survive closing and reopening the editor. learnStartedMs defaults to 0.0 and learnStopPending to false (e769f33:src/gui/PluginEditor.h:557-558). A reopened editor therefore shows no countdown and does not enforce the 5 s…)*
- **duplicate → TEST-001.** TEST-001 covers the untested 24 Hz tick, including the Learn state it projects. The Learn click grammar (5 s refusal, countdown, flash) sits in the same untested editor code and belongs in TEST-001's test list. *(note: Learn has no editor-level test coverage. The Learn tests in tests/dsp_tests.cpp (around :2739-2767 and :3629-3690) cover the engine command, the commit and the restore ordering. Nothing pins the UI grammar: the 5 s refusal, the countdown, the empty-pass flash,…)*
- **duplicate → INPUT-009.** INPUT-009's title and corrections already cover the Settings-panel (Backdrop::mouseDown, PluginEditor.h:92-96) case and KI-013's too-narrow attribution. *(note: KI-013 scope drift: KNOWN_ISSUES KI-013 (e769f33:docs/KNOWN_ISSUES.md:913-941) blames the absorbed press on the pop-up shield only. The same double-click reset follows a Settings-panel dismissal, where the absorbing component is Backdrop::mouseDown (e769f33:sr…)*
- **duplicate → UI-008.** The verify-5 LOCK runs (8-step approach shows the Ceiling tip 3/3) are UI-008's own evidence for the stale-cache tooltip. *(note: Tooltip stale-cache with realistic stepped input likely explains G-02's 'LOCK: no tooltip captured on two hovers'. An 8-step/30 ms approach to LOCK showed the Ceiling tip 3/3, and a different approach showed no tip at all. Evidence: rt/verify-5/32-sheet.png, 3…)*
- **drop.** A single-frame anomaly that did not reproduce. The next attempt showed no tooltip window mapped, which fits the known Xvfb repaint artefact (E21). There is no product evidence. *(note: Unconfirmed anomaly: in one run a 'Presets' tooltip (e769f33:src/gui/PluginEditor.cpp:469) was visible while the preset menu was open. This contradicts the stated shield suppression (e769f33:src/gui/PluginEditor.h:155-160). It was not reproduced on the next at…)*
- **duplicate → UI-014.** UI-014's verification already states that a parentless tip uses the display's user bounds (juce_TooltipWindow.cpp:125-135), so the flip direction depends on where the plugin window sits on the monitor. *(note: Tooltip flip direction is decided by the pointer's position relative to the MONITOR's centre, not the editor's. The TooltipWindow is parentless (e769f33:src/gui/PluginEditor.h:447), so parentArea is the display's user bounds (JUCE e18f7f5 juce_TooltipWindow.cp…)*
- **duplicate → VIS-002.** VIS-002 covers the red TP row in TP mode at the ceiling and the missing tolerance, compared with SP's half-print tolerance. The 0.04 dB genuine overshoot at OS Off is DSP-001's mechanism. The -1.00 dBTP / -0.96 hold data point (rt/verify-6/03) can be added to DSP-001's measurement table. *(note: TP row red on a TP-mode master that holds its ceiling within estimator tolerance. Setup: TP on, Ceiling -1.00 dBTP, OS Off, Loudness 60 %, harness music at -12 dB. The STATISTICS TP hold reads -0.96 dBTP in warn red while SP reads -1.00 dBFS. The TP warn compa…)*
- **duplicate → UX-009.** UX-009's correction (1), root cause and proposal (a) already carry the post-mix MATCH gain defect with the same abjump.log numbers (4.45/4.49 LU, 6.38/6.50 LU) and the Anamorph ordering precedent. Per the UX-009 challenge, the title must be rewritten, and the engine fix (a) can be split from the P2 indicator half at synthesis. No separate record is needed. *(note: MATCH+BYPASS is not loudness-matched (judged inside UX-009 because its title asserts the opposite; listed here so synthesis can split it out as its own correctness item). The compensation gain is applied post-bypass-mix, so the dry leg is scaled by the same at…)*
- **duplicate → UX-010.** UX-010's current_behavior, corrections and evidence already include the realtime-print capture (-17.51 vs -8.84 LUFS, rt/verify-7/offline.log) and the DSP_POLICY.md:94-97 scope issue. *(note: Realtime prints capture the monitor path: the MATCH/DELTA guard is only `! p.nonRealtime` (e769f33:src/dsp/AnabasisEngine.cpp:668-677). So a realtime bounce or record-to-track print with MATCH on captures an attenuated master (probe: -17.51 vs render -8.84 LUF…)*
- **drop.** A note on probe infrastructure (rt/verify-7/probe.cpp, eng_measureonly.cpp), not a finding. It stays usable as UX-009 and DSP-005 verification tooling. *(note: Probe infrastructure for later verification: rt/verify-7/probe.cpp links the unmodified e769f33 processor objects from the audit harness build (no GUI) and measures the listening output with the product's LoudnessMeter. rt/verify-7/eng_measureonly.cpp is a scr…)*
- **duplicate → UX-013.** UX-013's proposal (item 7) deliberately excludes the continuous-zero cases (Clip Shape at Drive 0, colour knobs at Depth 0), because dimming them would flicker during macro moves. The verifier's default-patch observation does not change that reasoning. *(note: In the default Advanced view, three knobs are fully drawn but inert because another knob is at zero, not because of a switch:
- Clip Shape does nothing while Clip Drive is 0 dB (e769f33:src/dsp/ClipSat.h:195, clipOn = drive != 0).
- Odd/Even and Color Tone do…)*
- **duplicate → UX-013.** UX-013's scope already lists 'Offline Render = Force Max is inert when Oversampling is already 16x'. *(note: Offline Render = Force Max has no effect when Oversampling is already 16x, because effectiveFactor returns 16x in both cases (e769f33:src/dsp/Latency.h:96-99). The Settings row gives no indication of this.)*
- **duplicate → DSP-002.** DSP-002 correction 1 already records the dependence on host buffer size (34.2/38.3/48.8/70.3 ms) from the same probe.out. *(note: The forced-duck length depends on the host buffer size, because the silent bottom is held until the next host process() call (e769f33:src/dsp/AnabasisEngine.cpp:332-357, 486-529). An engine probe at e769f33 measured dropouts of 34.2 ms (48k/64), 38.3 ms (512),…)*
- **duplicate → DSP-002.** DSP-002 correction 5 already records the ADR-0018 vs USER_MANUAL/KI-010 contradiction, and STATE-006 proposes reconciling it. *(note: Doc drift: ADR-0018 calls the undo duck 'inaudible-by-design' (e769f33:docs/architecture/design-decisions/ADR-0018-copy-and-advanced-join-the-undo-history.md:93-95). That contradicts USER_MANUAL ('The short dip is the mechanism working', e769f33:docs/user/USER…)*
- **duplicate → DSP-002.** DSP-002 proposal part 2 already records that dry-fill becomes a dip to dry level on loud material and must be compared in the KI-010 listening pass. *(note: KI-010's option (a), dry-fill, assumes that blending to the delay-aligned dry signal masks the swap better than a dip to silence. For a maximizer at non-zero Loudness the dry signal is typically several dB quieter than the processed output, so dry-fill becomes…)*
- **duplicate → UX-020.** UX-020's evidence cites PluginEditor.cpp:795-801 and 840 (bare XS..XL items, tooltip), and its proposal annotates each step with its size and fit. *(note: The UI Scale menu items are bare 'XS / S / M / L / XL', with no percentage or resulting window size, and the tooltip says only 'M is the original; everything scales in proportion'. The user cannot tell before choosing how big a step is or whether it fits the s…)*
- **duplicate → UX-020.** UX-020's corrections and dependencies already name the unexercised setScaleFactor compose path and require a real-host check on a Windows scaled display. TEST-002 also lists host DPI. *(note: The host-DPI compose path (setScaleFactor → transform = hostScale × user step, e769f33:src/gui/PluginEditor.cpp:1937-1941) was never exercised at runtime by any observer. On an OS-scaled laptop (e.g. 1920x1080 at 150 % = 1280x720 logical), arithmetic suggests…)*
- **duplicate → UX-021.** UX-021's corrections already record 'No OPEN_QUESTIONS entry records the resize question (grep: 0 hits)', and its proposal adds the entry. *(note: No OPEN_QUESTIONS entry records the conflict between brief §7/§8 ('resizable', 80–200 %) and DESIGN.md:867-870 / ADR-0017 (stepped, free resize off). The question has no tracked owner decision (grep of docs/OPEN_QUESTIONS.md for resiz/scale: 0 hits).)*
- **duplicate → MODEL-001.** MODEL-001 correction 4 already records that USER_MANUAL.md:330-331 says 'next time you move a macro knob', while a press that moves nothing also re-engages. *(note: Doc drift: USER_MANUAL says detached parameters re-engage 'the next time you move a macro knob' (e769f33:docs/user/USER_MANUAL.md:330-331). In fact any macro gesture re-engages them, including a press with no movement (left or right button), a wheel notch, or…)*
- **duplicate → INPUT-013.** INPUT-013's verification already states that LAY-13(e) was wrong: right-click toggles A/B and resets STATISTICS. *(note: Phase-2 observation LAY-13(e) is partly incorrect: a right-click on A/B does toggle the active slot (rt/verify-10/04-sheet.png), and a right-click on STATISTICS does reset I/LRA/PLR and the peak holds (rt/verify-10/05-sheet.png). This matches e769f33:src/gui/P…)*
- **duplicate → INPUT-013.** INPUT-013 already covers juce::Button firing onClick for any mouse button (copyButton :330, titleButton :315, presetName :472) and proposes gating on the modifiers. The Settings right-click runtime capture is one more instance. *(note: Every juce::TextButton in the editor activates on right-click as well as left-click: JUCE Button::mouseUp has no mouse-button filter (build/_deps/juce-src/modules/juce_gui_basics/buttons/juce_Button.cpp:484-497). Verified for Settings: a right click opened the…)*
- **drop.** Not confirmed as a product defect. The absolute-move hover failure is the known XTest warp artefact. Hover under overlays clears through isMouseOver on the covering component. The sibling's live hit-test divergence is already the subject of UI-008. *(note: Anamorph's hover driver has diverged from Anabasis's. Anamorph hit-tests the live cursor and removes hover under open pop-ups and overlays (Anamorph@fd78c3b:src/PluginEditor.cpp:1676-1692, KI-024), while Anabasis uses isMouseOver(true) (e769f33:src/gui/PluginE…)*
- **drop.** Out of scope: Anamorph is read-only here. The Anabasis instance of the 'Settings (gear)' drift is DOC-005; the sibling's copy is a note for the family doc owner. *(note: The sibling's manual has the same 'Settings (gear)' drift for a text 'Settings' button (Anamorph@fd78c3b:docs/user/USER_MANUAL.md:194, 258; Anamorph@fd78c3b:src/PluginEditor.h:372). Anamorph is read-only here; this is for the family doc owner.)*
- **duplicate → UI-013.** UI-013's correction 3 covers the platform-font dependency (Verdana, Helvetica, Liberation Sans), and its acceptance criteria already require a headless fit test in the presetname font. *(note: Preset-name fit is platform-font dependent and untested. JUCE's default sans is Verdana on Windows (juce_DirectWriteTypeface_windows.cpp:499), Helvetica on macOS and Liberation Sans here, and no state test measures the top-bar preset label, unlike testEveryCom…)*
- **duplicate → UX-023.** UX-023's correction 1 and current_behavior already state that JUCE mutes input by default at first launch (StandaloneFilterWindow.h:344, 453, 563-567), so a working device processes silence. The same ALSA null-device reproduction is used. *(note: The Standalone processes silence on first launch even with a working audio device. JUCE turns 'Mute audio input' on by default for a processor with inputs and outputs (JUCE 9.0.1 juce_StandaloneFilterWindow.h:344, :453, :563-567). Anabasis has no file player (…)*
- **drop.** The anchor error is in the phase-1 map, not in the product. TECH-001 already cites the correct CHANGELOG.md:1094-1103. *(note: CHANGELOG anchor drift: the KI-012 'does not close the outstanding report' bullet is at e769f33:CHANGELOG.md:1094-1103, not 1097-1106 as cited in the phase-1 map.)*
- **duplicate → TECH-001.** TECH-001's verification already reports the private-namespace experiment with libXi.so hidden (the Standalone accepted a stepped drag and click) and the correct CHANGELOG anchor. *(note: The 9.0.1-built Standalone accepts XTEST pointer input with the unversioned libXi.so hidden (private mount namespace; /tmp/claude-0/-home-user/52dd522c-a346-5a74-bfc9-56317277213a/scratchpad/rt/verify-13/ns/dlprobe.txt, rt/verify-13/15-crop.png). This is the e…)*
- **duplicate → STATE-007.** PF-anamorph-reference-3 was already dropped as refuted at the merge step (see 'Candidates the consolidation dropped' above), and STATE-007's corrections record that typed entry is bracketed (juce_Slider.cpp:451) and detaches or re-engages like a drag. *(note: PF-anamorph-reference-3 ('typed value-box entry is gesture-less, so typed managed values neither detach nor undo') looks refuted at the JUCE pin. Slider's textChanged commits inside ScopedDragNotification (build/_deps/juce-src/modules/juce_gui_basics/widgets/j…)*
- **duplicate → MODEL-006.** MODEL-006's verification already records ADR-0005 Decision 2 (:97-100) 'gesture-bracketed per knob drag' against the unbracketed mapper writes, and the unverified host Touch/Latch behaviour. *(note: ADR-0005 decision 2 (e769f33:docs/architecture/design-decisions/ADR-0005-macro-layer-architecture.md:97-100) says mapper writes are 'gesture-bracketed per knob drag', but applyMapping/setParam write the nine managed parameters with bare setValueNotifyingHost a…)*
- **duplicate → UI-013.** Same truncated-marker defect: 'Transparent Mast…' after a Character edit. *(note: The preset label's modified marker is lost to truncation on longer preset names. refreshPresetDisplay sets the button text to name + " *" (e769f33:src/gui/PluginEditor.cpp:2165-2169), and the combined string is ellipsised. After loading 'Transparent Master' an…)*
- **duplicate → DSP-004.** DSP-004's verification and proposal already say that KI-005 blames a 'Character-macro move' while Loudness drives clipDrive, and they require correcting KI-005 to 'Loudness'. *(note: KNOWN_ISSUES KI-005 says a 'Character-macro move' carries clipDrive across 0 dB (e769f33:docs/KNOWN_ISSUES.md:296-297). clipDrive is driven only by Loudness (e769f33:src/MacroEngine.h:54); Character drives only colourDepth (e769f33:src/MacroEngine.h:57).)*
- **duplicate → UI-013.** The same defect on the five-preset strip. That it contradicts obs/layout.md:130 is an audit-internal note: UI-013's corrections already establish that truncation happens only with the marker. *(note: The '*' edited marker is ellipsised away on long preset names. At M in the Simple view, 'Transparent Master *' renders as 'Transparent Mast…' and 'Classical Dynamics *' as 'Classical Dynami…', so the edited and clean states differ only by an ellipsis. Shorter…)*
- **duplicate → STATE-005.** STATE-005 correction 1 already records that SESSION_COMPATIBILITY_POLICY rule 7 (:49-50) 'falls back to defaults' contradicts the code and ADR-0007:69-71 'keep current state'. *(note: Doc drift: SESSION_COMPATIBILITY_POLICY rule 7 (e769f33:docs/policies/SESSION_COMPATIBILITY_POLICY.md:49-50) says 'An unreadable state falls back to defaults'. The code (e769f33:src/PluginProcessor.cpp:1824-1829, 'keep current state'), ADR-0007 (e769f33:docs/a…)*
- **duplicate → DOC-004.** DOC-004 already carries every anchor: PluginProcessor.h:652-653, DESIGN.md:794-799, MODE policy :161-166 and PluginEditor.cpp:621-640. The verify-17 LEARN→Undo runtime evidence (10-undo-after-learn-crop.png, state-after-learn-undo.xml) is already in STATE-003. STATE-009 proposal (b) already sends the undoable-Learn product decision to OPEN_QUESTIONS. *(note: Learn-undo doc drift: e769f33:src/PluginProcessor.h:652-653 says 'the P5 UI adds the duck-routed engage + undo bracketing' around startLearn/stopLearn, and e769f33:docs/DESIGN.md:797 says the Learn 'commit is gesture/undo-bracketed as one step'. However, e769f…)*
- **duplicate → DOC-004.** The tooltip ('loudest', PluginEditor.cpp:641-642) versus manual ('representative', USER_MANUAL.md:312-313, :450) contradiction is DOC-004's first clause and proposal item 1. *(note: The Learn instructions contradict each other. The tooltip says 'Play the loudest section' (e769f33:src/gui/PluginEditor.cpp:641-642), while USER_MANUAL §4 says 'play a representative section' (e769f33:docs/user/USER_MANUAL.md:312-313) and the podcast workflow…)*
- **duplicate → STATE-004.** STATE-004's title and evidence already say 'any prepareToPlay, including one at the same rate and block size', and cite PluginProcessor.cpp:779-781 and AnabasisEngine.cpp:428-434. Closing KI-006's audio half under STATE-004 should include correcting KI-006's heading and 'Affects' line (KNOWN_ISSUES.md:336, :342-343). *(note: KI-006's heading and 'Affects' line name a sample-rate or block-size change, but the audio drop occurs on every prepareToPlay, including same-rate and same-block re-prepares (probe variant 0, bit-exact with a never-adapted engine). The codebase itself assumes…)*
- **duplicate → DSP-007.** DSP-007's interim step already qualifies USER_MANUAL.md:282, :442-443 and :500-501 (Oversampling 'cleaner', Force Max 'maximum quality'). DSP-001's interim step removes 'true-peak accuracy' from :282. *(note: USER_MANUAL presents Oversampling as improving true-peak accuracy and output cleanliness (e769f33:docs/user/USER_MANUAL.md:282, :442-443, :500-501). Measurements with the real engine show the opposite at the output stage: TP-mode output true peak and clamp har…)*
- **duplicate → DSP-001.** DSP-001's current_behavior and probe evidence already record Force Max +1.399 against realtime OS Off -0.074, and the milder case -0.083 against +0.155 (forcemax.txt). Its acceptance test covers Force Max offline. The general point that nothing shows a Force Max render differs from what is monitored belongs to UX-014's Settings-consequence gap. *(note: Force Max offline renders diverge materially from the realtime monitor, and nothing indicates it. At the same settings the realtime OS-Off TP reading is -0.074 dBTP while the Force Max bounce reads +1.399 dBTP. With milder drive the figures are -0.083 vs +0.15…)*
- **duplicate → DSP-001.** DSP-001's proposal already corrects ADR_INDEX, the ADR-0006 banner (:9) and Related code (:222), and ADR-0015 D7 (:174-179). The ADR-0006 :231-232 '[Unverified] — no src/' evidence line belongs in the same ADR-amendment pass. *(note: Stale ADR text: ADR-0006 'Related code: None yet — P1 onward' (e769f33:docs/architecture/design-decisions/ADR-0006-ceiling-guarantee.md:222) and 'Evidence [Unverified] — Anabasis has no src/' (:231-232). The ADR-0006 amendment banner asserts the clamp is 'stil…)*
- **duplicate → DSP-001.** DSP-001's evidence cites PresetManager.cpp:157-160 and :180-188, and its title covers 'Punchy or high Transients'. The challenge's caveat 1 still applies: punchy.txt measures Punchy style and Transients settings, not the presets loaded through their macro mappings, so the preset-specific figures remain extrapolated. *(note: Factory presets Loud Pop and Hip-Hop Low End (Punchy) and Rock Punch (Punchy, Transients 75) configure limiter voicings under which TP mode exceeds the 0.1 dBTP tolerance even at OS Off. Probe: +0.10…+0.24 and +0.47 dBTP against a -0.10 ceiling. Evidence: e769…)*
- **duplicate → VIS-008.** VIS-008's title already includes the SP row ('lowering the ceiling paints earlier legal holds red (SP included)'). ADR-0020's 'genuinely exceeded' definition (:113-116) belongs in that record's rationale. *(note: After lowering the ceiling, the SP row turns red from a stale hold. ADR-0020 defines SP red as 'the clamp was genuinely exceeded' (e769f33:docs/architecture/design-decisions/ADR-0020-waveform-statistics-panel.md:113-116), so the stale hold makes the violation-…)*
- **drop.** Evidence hygiene, not a product problem. For the report: LoudnessMeterView.cpp has 281 lines at e769f33, so the V-12/E13 anchors need re-pinning to e769f33:src/gui/LoudnessMeterView.cpp:243-244 (TP warn), :265-267 (SP warn) and :229-242 (the comment). *(note: The phase-2 observation files cite LoudnessMeterView.cpp lines that do not exist at e769f33: visuals.md V-12 cites :370-371, :392-394, :344-369 and edges.md E13 cites :245-252, but the file has 281 lines. The correct anchors are :243-244 (TP warn), :265-267 (S…)*
- **duplicate → VIS-003.** VIS-003's evidence already lists the contradiction between CurveView.h:20-23 and AnabasisEngine.cpp:1040-1042. Its corrections cite AnabasisEngine.h:346-349 and note that the lanes are Advanced-only. *(note: Code-comment contradiction on GR attribution: e769f33:src/dsp/AnabasisEngine.cpp:1040-1042 says the meter legend ('which reduction it is showing') is still an open P5 item, while e769f33:src/gui/CurveView.h:20-23 and e769f33:src/dsp/AnabasisEngine.h:346-349 ca…)*
- **duplicate → VIS-013.** VIS-013's evidence cites USER_MANUAL.md:242-244, ADR-0023:99-102 and GrHistoryView.cpp:412-433, and its proposal and acceptance criteria correct 'stays empty'. *(note: Doc drift: e769f33:docs/user/USER_MANUAL.md:242-244 says the unmeasured left region of the GR history 'stays empty', but ADR-0023 decision 6 (e769f33:docs/architecture/design-decisions/ADR-0023-012-field-fix-contracts.md:99-102) and e769f33:src/gui/GrHistoryVi…)*
- **duplicate → VIS-006.** VIS-006 already cites worklogs/2026-08-21-gr-scale-and-percent-entry.md:57-61 and AI_AGENT_POLICY C8. VIS-003 and VIS-007 already route wording to the maintainer. The cross-cutting point, that annotation findings need a maintainer copy spec, belongs in the synthesis theme rather than a record. *(note: Process constraint affecting every annotation proposal: e769f33:worklogs/2026-08-21-gr-scale-and-percent-entry.md:61 records that dB gridlines and an axis label for the GR history were rejected only under AI_AGENT_POLICY C8 (e769f33:docs/policies/AI_AGENT_POLI…)*
- **duplicate → VIS-007.** VIS-007 says the per-block figure 'is computed and never read' (meterGrDb grep, PluginProcessor.cpp:1016-1021, PluginEditor.cpp:2084) and proposes a history-based source. Retiring pubGrDb together with its THREAD_MODEL row is a one-line alternative within that record. *(note: Dead publication: pubGrDb is still stored every block (e769f33:src/PluginProcessor.cpp:1020-1021) and cleared by publishSilentMeters, and THREAD_MODEL's 'Meters -> GUI' row lists it, but no GUI code reads meterGrDb() (only a comment at e769f33:src/gui/PluginEd…)*
- **duplicate → VIS-003.** The verify-20 clip-drive magnitude (median 5.1→8.6 dB, p90 6.4→12.5 dB, 01-adv-loud70-music6.png vs clip0-full.png) is already in VIS-003's evidence and corrections. *(note: Magnitude of the hidden clipper reduction (runtime, synthetic programme): Advanced, music -6 dB, Loudness 70. Detaching the macro's 5.1 dB clip drive to 0 raised the limiter GR trace from median 5.1 to 8.6 dB and from p90 6.4 to 12.5 dB (verify-20 01-adv-loud7…)*
- **duplicate → VIS-013.** VIS-013 already cites GrHistoryView.cpp:264 ('if (head <= 0) return;') and verify-20 10-hostbypass-reprepare-well.png, which shows the state is reachable in the plug-in. *(note: The blank-GR-well state (e769f33:src/gui/GrHistoryView.cpp:264) is reachable in the plug-in, not only in the no-device Standalone: a re-prepare to a new rate or block while the host is not calling processBlock leaves the well blank with '-' statistics (verify-…)*
- **duplicate → VIS-014.** Already folded into VIS-014's proposal: PluginEditor.cpp:779 says 'INTEGRATED row' while LoudnessMeterView.cpp:174 labels the row 'I'. *(note: The Settings 'Integrated' combo tooltip refers to 'the INTEGRATED row' (e769f33:src/gui/PluginEditor.cpp:779), but the panel tags that row 'I' (e769f33:src/gui/LoudnessMeterView.cpp:174). This is a minor naming inconsistency, folded into VIS-014's proposal.)*
- **duplicate → VIS-021.** VIS-021's evidence cites SpectrumView.cpp:829-830 and the :912 jlimit clamp. Its proposal item 2 adds headroom above 0 dB or marks clamped columns. *(note: The spectrum's input and output traces are clamped to dbHi = 0 dB (e769f33:src/gui/SpectrumView.cpp:830, jlimit at :912), so input content above full scale flat-lines at the top edge and cannot be read. This is relevant to VIS-015 and to any spectrum finding.)*
- **duplicate → UI-010.** UI-010 already measures about 318 px of blank glass in the Simple STATISTICS panel, and so does UX-002. *(note: The STATISTICS panel in Simple has about 300 px of unused height below the PLR row (panel spans y≈100-628 and the last row ends near y≈310 at scale M) — /tmp/claude-0/-home-user/52dd522c-a346-5a74-bfc9-56317277213a/scratchpad/rt/verify-21/02-lvl0-6.png. It is…)*
- **duplicate → VIS-023.** VIS-023's evidence already records one GR entry per PREPARED block, i.e. the host's declared maximum (PluginProcessor.cpp:806, :1022-1031). Which hosts over-declare is a TEST-002 host-matrix question. *(note: The GR history's temporal resolution is set by the host's declared MAXIMUM block (prepareToPlay's samplesPerBlock), not by the delivered buffer size (e769f33:src/PluginProcessor.cpp:806, :1022-1031). A host that prepares with a large maximum but delivers small…)*
- **duplicate → VIS-023.** VIS-023's evidence already records SpectrumView.cpp:194-199 and the roughly one-third of audio never analysed at 384 kHz (inferred, not measured). *(note: SpectrumView::analyse transforms only the newest 4096 frames per tick (e769f33:src/gui/SpectrumView.cpp:194-199). At 384 kHz that window is 10.7 ms, against 16.7 ms between 60 Hz ticks, so roughly a third of the audio is never analysed and short transients bet…)*
- **drop.** Evidence hygiene. rt/visuals/20b-gr-after-state-load-0.8s-well.png shows SPEC because setStateInformation restored int_spectrumOn (ADR-0016), which is intended. The report must not cite that image for GR-trace claims. *(note: Evidence hygiene: V-04's screenshot rt/visuals/20b-gr-after-state-load-0.8s-well.png shows the SPEC view. setStateInformation restored the saved graph-well mode (int_spectrumOn, ADR-0016), so the image cannot evidence the GR trace after a state load. A host st…)*
- **duplicate → VIS-019.** VIS-019 already cites USER_MANUAL.md:258-259 as unqualified, notes there is no KNOWN_ISSUES entry, and proposes qualifying the text pending OQ-017. *(note: USER_MANUAL §3.4 (e769f33:docs/user/USER_MANUAL.md:258) states unconditionally that 'The trace scrolls continuously', while OQ-017 (e769f33:docs/OPEN_QUESTIONS.md:544-610) records that bursty hosts step and stall it, and KNOWN_ISSUES has no entry for this. Thi…)*
- **drop.** Anamorph is read-only and outside this audit's remit, and this was a code read only. That the sibling shares the spectrum column reducer is noted in VIS-024 for the owner. The -6 dB clip-glow assumption is the sibling's own issue. *(note: Anamorph (read-only, not changed): its clip-glow comment assumes a -6 dB Hann loss ('a 0 dBFS tone only reads ~-6 dB', fd78c3b:src/gui/SpectrumImager.cpp:818-819, compensated +6 dB at :833). Its window is JUCE's default-normalised Hann (SpectrumImager.h:175) a…)*
- **duplicate → VIS-021.** VIS-021's evidence (SpectrumView.cpp:202-203, state_tests.cpp:8503-8505, ADR-0039 line 112) and acceptance criterion 5 already require correcting both texts. *(note: Code/doc drift tied to VIS-021: the e769f33:src/gui/SpectrumView.cpp:202 comment ('normalise so a full-scale sine reads ~0 dB') and the measurement text at e769f33:tests/state_tests.cpp:8503-8505 / ADR-0039 line 112 ('−0.00 dB paired correctly' for a 0.5-ampli…)*
- **duplicate → TECH-004.** TECH-004's verification already records that the micro-animation driver is a raw VBlankAttachment (PluginEditor.cpp:1046-1052). It is uncapped on 144/240 Hz displays and costs a negligible amount. The DESIGN §6.5 FrameClock wording can be fixed in DOC-008's DESIGN banner pass. *(note: DESIGN §6.5 presents FrameClock (vblank-paced, ~125 Hz cap) as the animation and rendering mechanism, but the editor's micro-animation driver is a raw juce::VBlankAttachment with no cap (e769f33:src/gui/PluginEditor.cpp:1046-1052 vs e769f33:docs/DESIGN.md:941-…)*
- **drop.** Mechanism note only. The per-vblank isShowing() → XGetWindowProperty round-trip did not appear in 450 stack samples, so it is not material. TECH-004's proposed Linux editor budget measurement would surface it if it ever mattered. *(note: On Linux, stepMicroAnims calls isShowing() on every vblank. Through LinuxComponentPeer::isMinimised, that becomes XWindowSystem::isHidden, a synchronous XGetWindowProperty round-trip per frame (pinned JUCE build/_deps/juce-src/modules/juce_gui_basics/native/ju…)*
- **duplicate → TECH-003.** TECH-003's evidence already has the EDEADLK abort ('Fatal glibc error: pthread_mutex_lock.c:445', gdb-bt.log) and the recursive PTHREAD_PRIO_INHERIT CriticalSection anchor. It already states the result is a crash, not a hang. *(note: KI-008 under-describes its failure mode on Linux. JUCE's POSIX CriticalSection is recursive with PTHREAD_PRIO_INHERIT (juce_SharedCode_posix.h:42-44), so the kernel detects the M0/M1 cycle and glibc aborts ('pthread_mutex_lock.c:445 ... EDEADLK'). The result i…)*
- **duplicate → TECH-003.** TECH-003's evidence already lists gesture-end at PluginProcessor.cpp:353 as 'a second M0->M1 site that KI-008 does not list'. Its Step 1 covers both begin (:275) and end (:353). *(note: KI-008's table omits a second M0->M1 site. audioProcessorParameterChangeGestureEnd calls saveSlotFromLive() at e769f33:src/PluginProcessor.cpp:353 while endChangeGesture holds the parameter's listenerLock (JUCE juce_AudioProcessorParameter.cpp:101).)*
- **duplicate → TECH-003.** TECH-003's evidence V24-PV-1 already records pluginval's 'Background thread state' test running 6 times on the AU in macOS CI job 107767666230. That contradicts KI-003's claim that no case has been observed. *(note: KI-003's statement that 'no case of an off-message-thread restore has been observed' is contradicted by CI's own macOS job. pluginval's 'Background thread state' test calls setStateInformation from a background thread with the editor open for non-VST3 formats…)*
- **duplicate → TECH-003.** TECH-003's evidence V24-RACE-1 already records 8 of 8 heap-corruption aborts in the plain build against a 5 of 5 clean control, and its title calls the result a crash. Re-rating KI-003 above Low follows from that record. *(note: The KI-003 residual race is a heap-corrupting crash, not a torn label, under a sustained stimulus: 8 of 8 plain-build aborts, against a 5 of 5 clean control with no concurrent reads. KI-003 is rated Low severity. Evidence: .../rt/verify-24/probe/p1-5.out, q1-3…)*
- **duplicate → DOC-011.** DOC-011 already cites REALTIME_SAFETY_AUDIT.md:86-90 ('owed') against ADR-0029's clean RTSan run. CI run 36039432935 / job 107767666158 can be added as the evidence pointer. *(note: e769f33:docs/architecture/REALTIME_SAFETY_AUDIT.md:87-90 still says the first green RealtimeSanitizer run is 'owed'. The realtime job (RTSan canary + DSP suite under RTSan) passed at e769f33 (run 36039432935, job 107767666158).)*
- **duplicate → TEST-006.** TEST-006's proposal and acceptance criterion 1 already require a settle delay or thread-scoped counting, because tests/AllocationGuard.h counts allocations process-wide. *(note: tests/AllocationGuard.h counts allocations process-wide. A guard armed in the first milliseconds of a process picked up 1 malloc from JUCE start-up threads, which vanishes with a 400 ms settle in 5 of 5 runs. Any future wrapper-level guard test must settle or…)*
- **duplicate → UI-011.** UI-011's evidence already identifies the '324 of 398' comment at PluginEditor.cpp:1567-1570 as LIMITER's pre-ADR-0019 figure. *(note: Stale layout comment. e769f33:src/gui/PluginEditor.cpp:1567-1570 says COMP's budget '(24 combo + 3×84 rows + 26 toggles + 8 + 14 meter = 324 of 398) closes exactly' with the 74 px link row. The terms listed sum to 324, which is LIMITER's (pre-ADR-0019) spend.…)*
- **duplicate → UI-004.** UI-004's verification already notes COMP Threshold's full arc at the 0 dB default and deliberately excludes it pending the fine review. *(note: COMP Threshold at its 0 dB default, where the compressor computes no reduction (USER_MANUAL.md:193-194), draws a FULL orange value arc (e769f33:src/PluginParameters.cpp:321 with e769f33:src/gui/LookAndFeel.cpp:79-81; rt/layout/04-advanced-initial.png). The arc…)*
- **duplicate → UI-004.** UI-004's title already includes the Input Gain fader's one-third fill at unity (LookAndFeel.cpp:186-188, 04c-utility.png). *(note: The Input Gain fader fills from -12 dB, so unity gain (0.0 dB) shows a one-third accent fill (e769f33:src/gui/LookAndFeel.cpp:186-188; e769f33:src/PluginParameters.cpp:314; rt/layout/04c-utility.png). This is not in UI-004's original claim; it has been folded…)*
- **drop.** Evidence hygiene, caused by UI-008's stale tooltip. rt/layout/39c-tips-sheet-a.png must not be cited for combo tooltip wording; cite e769f33:src/gui/PluginEditor.cpp:58, 64, 73 and 88 instead. *(note: The LAY-05 tooltip capture sheet rt/layout/39c-tips-sheet-a.png is contaminated by the stale-tooltip artefact (UI-008). Hovering the COMP combo shows the Knee tooltip, and the LIMITER combo shows the Color-model tooltip. Combo tooltip wording should be cited f…)*
- **duplicate → DOC-011.** DOC-011's evidence already cites HANDOVER.md:28 ('C++20, warning-free', 'C++23 canary is wired'). The same row's 'P1 skeleton, 2026-07-31' belongs in that fix. *(note: HANDOVER Build Status is stale: e769f33:docs/HANDOVER.md:28 says 'C++20, warning-free' and 'The §2.1 C++23 canary is wired as of 2026-08-05 (cxx23-canary.yml …)', while ADR-0030 moved the baseline to C++23 and removed the canary workflow (e769f33:docs/DEVELOPM…)*
- **duplicate → DOC-001.** DOC-001's evidence cites DOCUMENTATION_LIFECYCLE_POLICY.md:16-37, and its proposal item 5 adds trigger-map rows (packaging, I/O layout, user-visible control name). Extending those rows to GUI-layout and Settings-row changes is the same fix. *(note: Lifecycle trigger-map gap (the root cause of the manual and DESIGN UI drift): e769f33:docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md:16-47 has no row that routes a GUI layout, Settings-row, control-label or I/O-layout change to USER_MANUAL.md or README.md. 'P…)*
- **duplicate → INPUT-004.** INPUT-004's current_behavior and evidence already include the Settings Tab/Down probe (settings-tab-probe.txt, 21c.png), and its proposal makes the Backdrop a keyboard focus container. *(note: Modal overlays do not contain keyboard focus. With Settings open, Tab continues to the editor controls behind the backdrop, and Down changed Tone (−0.40, with the macro moving Color Tone and Tilt) and the Ceiling while the panel covered them; the preset label…)*
- **drop.** CI cost and concurrency configuration, with no user-facing behaviour, so outside this product/UX/UI audit. It is still worth a separate engineering ticket: build.yml:3-7 and :25-27 run the full matrix twice per push to a PR branch (for example, 4a5b71c ran as 34220696244 and 34220698731). *(note: Each push to a branch that has an open PR runs the full build.yml matrix TWICE: the push and pull_request events land in different concurrency groups because github.ref differs (`refs/heads/<b>` vs `refs/pull/<n>/merge`). Evidence: e769f33:.github/workflows/build.…)*
- **drop.** Evidence hygiene. The V-10/VIS-012 observer anchors LoudnessMeterView.cpp:284-285 and :395 do not exist at e769f33 (the file has 281 lines). In the report, re-pin them to e769f33:src/gui/LoudnessMeterView.cpp:158 (the '-' formatter) and :266-268 (the ' dBFS' rows). *(note: The observers' code references for V-10 / VIS-012 cite e769f33:src/gui/LoudnessMeterView.cpp:284-285 and :395, which do not exist at e769f33 (the file has 281 lines). The '-' formatter is at e769f33:src/gui/LoudnessMeterView.cpp:158, and the rows that append ' dBFS' a…)*

