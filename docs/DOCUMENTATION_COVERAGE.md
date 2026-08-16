# DOCUMENTATION_COVERAGE.md

Permanent documentation-coverage audit. **Future contributors/AI must update this on every
documentation-affecting change** (`docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md`).

Coverage = how well the module/topic is documented. Confidence = strength of the evidence behind
that documentation (Verified / Partially Verified / Unverified / Not Supported).

**Last updated:** for the **0.1.5 framework bump (2026-08-16)** — JUCE 9.0.0 → 9.0.1.

**Scope of the 0.1.5 round.** One dependency pin moved and nothing else was implemented. The
documentation surface it touched is exactly the one `DOCUMENTATION_LIFECYCLE_POLICY.md`'s
"Build / CMake / JUCE pin / C++ baseline" row names — `BUILD.md`, `CI_CD.md` (no edit needed: it
describes wiring, not the pin), `DEPENDENCY_POLICY.md`, and an **ADR** — plus the "Ship a version"
row (`CHANGELOG.md`, `HANDOVER.md`, `README.md`) and the live records of the pin itself
(`THIRD_PARTY_LICENSES.md`, `NOTICE`, `COMPATIBILITY_MATRIX.md`, `src/dsp/Latency.h`'s provenance
comment). **New record: [ADR-0028](architecture/design-decisions/ADR-0028-juce-901-pin.md)**, with
an amendment banner on ADR-0008 and a forward-pointer on the resolved OQ-001, both per the
`ADR_INDEX.md` convention that an earlier record keeps its text.

Two things this round deliberately did **not** rewrite, for the same reason it wrote them down:
`TEST_REPORT.md`'s environment line still says the measurements were taken at 9.0.0, because they
were, and re-stamping an environment nobody re-measured in would turn a record of an experiment
into a claim about today (the ADR-0026 lesson, applied to a different file); and the historical
records of the ORIGINAL pin decision — `CHANGELOG.md`'s P1 entry, `DESIGN.md`, ADR-0008's decision
text, OQ-001's resolution — say 9.0.0 and keep saying it. What changed is the set of documents that
describe the pin **now**.

**`DEVELOPMENT_BRIEF.md` was on that list for one round and did not belong there.** Review caught
it: the brief's only two pin sites are **§23**, the live "where Anabasis deliberately differs from
Anamorph" table, and **§23.2**, which lists the pin among the resolved decisions — both
current-state statements about the product family, not specification history. §2 asks for "the
newest stable 9.x" and quotes no revision at all, so nothing in the brief's *spec* half was
affected either way. That §23 tracks the present rather than the drafting date is visible in the
table itself: its Plugin-identity row already records "Anamorph moved to it in its 0.9.1", an event
later than the brief. Both sites now name 9.0.1 and the divergence, which is the same correction
ADR-0028 made for `README.md` and refused to skip. The misclassification is left recorded rather
than quietly dropped, because "which documents are historical?" was the judgement call of this
round and getting it wrong in the safe-looking direction — declaring a live document historical —
is how a delta table stops enumerating deltas.

**The one C6 drift found and reported rather than corrected.** `AI_AGENT_POLICY.md` §Hard Stop
conditions closes with "These map one-to-one to the `ARCHITECTURE_REVIEW_GATE.md` items". They do
not: the gate lists ten changes, the hard-stop list seven. **Build System change** — the item this
round is — has no hard-stop counterpart, and neither does *DSP Graph change*, *Ceiling guarantee
change* or *Plugin Format change*, while the hard-stop list's *Accepted-ADR conflict* has no gate
counterpart. The reading matters here rather than academically: under the accurate mapping a JUCE
pin bump is a gated change needing human review **and an ADR**, which is what happened, not an
agent hard stop that must halt before touching anything. Reported, not silently repaired — C6, and
because rewording a hard-stop list is not a change this task authorises.

Scope of the previous round (0.1.4), stated precisely because the round before it made a branch-snapshot claim in the
present tense with no expiry ("display/naming fixes only") that its own CHANGELOG then retracted:
this round changes **packaging, pop-up/menu interaction, and two stored-state behaviours inside the
unchanged schema**. It contains **no audible DSP change** and **no parameter-surface change**, so
the registry snapshot is untouched and PARAMETER_COMPATIBILITY_POLICY is not engaged. It **does**
change layout-adjacent code — pop-up menu measurement (`getIdealPopupMenuItemSize` now derives its
allowance from the constants `drawPopupMenuItem` spends, so long rows stop being clipped) and the
preset menu's column cap — which widens some menus by up to 8 px; the closed-state geometry of
every control is unchanged. That distinction is the point: "no layout change" would have been the
convenient sentence and it would have been false.

**New tooling:** `scripts/check-citations.py`, which verifies every `file:line` evidence anchor in
`docs/` and the root Markdown still points at the text it pointed at in a base revision, and
re-anchors the ones an edit above them moved. It exists because this round proved the rule cannot be
held by hand: the anchors were re-anchored once, then TWO later review commits moved code again and
the docs were not re-run, leaving 42 of 71 stale. Its header states what it cannot do — a citation
aimed at the wrong code from the start is preserved faithfully, so a clean run means none MOVED, not
that all are correct. That limit is not theoretical: THREE anchors here were already wrong before the
tool existed, and it dutifully carried each one along. Two were mis-aimed — `SERIALIZATION_REGISTRY.md`
§1.4's `BASELINE` carriage and drop sites, and `LATENCY_MODEL.md`'s redundant `updateLatency()` — and
are corrected, both now carrying the FUNCTION beside the line number, which is the half of a citation
a reader can check without running anything. The third was a category error rather than an aim error:
`POSTMORTEMS.md` INC-003 cites the line that BROKE the macOS build, a line the fix deleted, and its
own Evidence row says "(before the fix)" — but it was spelled as a bare anchor, so it named whatever
`main` holds today and the tool re-aimed it through three rounds of code movement. It is now pinned to
the pre-fix revision (`bcebfaf:tests/state_tests.cpp:5694`), which is the same spelling ADR-0016 uses
and which the ownership test declines by construction. A historical claim needs a historical anchor;
a bare one is a claim about the present. `worklogs/` is out
of scope: those records cite the sibling product, including by bare file name, and an early version
of the script rewrote those upstream anchors against this tree's line numbers before the scoping was
added.

The tool went through five corrections of its own before it was trustworthy, and they are worth
recording because each was a way of being confidently wrong — a tool that rewrites line numbers
turns every misjudgement into a corrupted document that reads as maintained.

It matched bare file names, so it rewrote the P0 worklog's UPSTREAM anchors against this tree
(caught in the diff, reverted, `worklogs/` now excluded). It read only the first anchor of a
compound citation (`…cpp:708-709, 851, 1208`), so it moved the head and left the tail — producing
`:1040, 1039, 1053`, out of order, in the very file it was meant to keep true. It paired citations
by their base SPELLING, so once one had been re-anchored the tool could no longer see it drift
again — the exact failure it exists to prevent; it now pairs the Nth reference to a path with the
Nth in the base.

The fourth was the worst, because it was silent and it shipped. A citation qualified by another
checkout or a pinned revision — `<checkout>:src/PluginProcessor.cpp:485-491`,
`7686204:src/gui/PluginEditor.cpp:1171-1174` — was matched from the path onwards, so the qualifier
never reached the ownership test and **27 anchors across `DESIGN.md`, `OPEN_QUESTIONS.md` and five
ADRs were shifted by THIS tree's code movement onto unrelated lines of the sibling product**,
including the ADR-0016 table whose own heading says it was read from the pre-change tree. Those
anchors are ADR-0009's mandated attribution; re-aiming them defeats the thing they are for. All 27
are restored, and the ownership test is now narrow by construction: a citation is checked only when
it names its path from this repository's root, unqualified, and that path is `TRACKED` verbatim —
so a bare file name, a sibling path and a `<rev>:` anchor are all left alone. Under-checking costs
coverage; misclassifying costs the truth of the document.

The fifth was in the rewrite itself: substitution was by string, so re-anchoring
`some/file.cpp:107` to `:130` also turned an untouched `some/file.cpp:1076` into `:1306`. It now
substitutes by the match's own span, right to left.

Those example anchors name `some/file.cpp` for a reason worth keeping: an ILLUSTRATIVE citation
spelled with a tracked path is indistinguishable from a real one, and this paragraph's own example
was silently re-anchored — `:1076` became `:1092`, which is not the shift the sentence describes.
A tool that rewrites line numbers cannot tell prose about citations from citations. Prose examples
therefore use a path the tool does not track.

Both remaining fixes are mutation-verified against the exact reported shapes, and the tool runs in
the `source-lint` CI job — against the fork point with the base branch, computed rather than
assumed, with a force-push or a first push falling back to `HEAD~1` instead of failing on a base
revision that no longer exists.

**New records:** `POSTMORTEMS.md` **INC-006** (a tidiness change put a privileged write path in a
world-writable directory — a local root exploit introduced and removed inside this PR, recorded for
the reasoning rather than the fix) and **INC-005** (the macOS package could report success with nothing
at the destination); `KNOWN_ISSUES.md` **KI-013** (the shield-absorbed click still counts toward
the multi-click run) and **KI-014** (macOS press-and-hold suppresses key repeat in the Save Preset
field — platform behaviour, filed rather than fixed); **ADR-0025** (the bounded,
disclosure-bound exception to `TESTING_POLICY.md` rule 1) and its rule-1 amendment.

**Untested surface of this round, in one place — the ADR-0025 disclosure.** The pop-up shield, the
pop-up lifetime cancellations, the keyboard-focus release, the inline-edit abandonment and the
tooltip gate ship with **no suite regression test**: the suites construct the editor but synthesise
no pointer device and run no modal loop, so the event sequence that constitutes each defect cannot
be produced at all. What is verified instead is the reasoning — each carries the framework
mechanism it depends on, read out of the pinned source, at the site that depends on it. What is
consequently unprotected: a future edit to the shield's z-order, its interception toggle, the
menu-tracking hooks or the focus ordering can regress silently. INC-005 is defended by the build's
own fail-closed assertions plus the A/B probe that proves those assertions can fire. The exception
lapses for any of these the day the suites gain a driven-input fixture (the closest prior art is
the X11/XTEST probe recorded under `worklogs/` for KI-012).

**Suites: `AnabasisTests` 296 + `AnabasisStateTests` 845 = 1141 checks green**, up 102 on 0.1.3's
1039 — ten new state tests (`testANoOpPresetApplyIsNotAUserAction`,
`testANoOpPresetApplyDoesNotEatTheOldestUndoStep`,
`testAMalformedStoredSlotCannotSplitSoundFromMetadata`,
`testThePopupShieldActuallyCoversTheEditor`,
`testAPopupRowKeepsItsLabelOutOfTheShortcutStrip`,
`testTheResizableFrameOverrideDiscriminatesItsCallers`,
`testEveryComboMenuFitsItsControl`,
`testARootlessSurfaceDropsTheActiveSlotsMetadataToo`,
`testAShortcutRowIsMeasuredWideEnoughForItsOwnLabel`,
`testANoOpPresetApplyIsNotAUserActionAfterASessionRestore`), each mutation-verified against its own
deliberately reverted fix and against no other. `testPresetIdentitySharedName` also gained the leg
that closes the last uncovered corner of the undo compare: with no ADR-0022 trio on either slot —
a pre-ADR-0022 session — `presetName` is the SOLE discriminator, and nothing asserted that
direction, so the name could have been stripped from `strippedForUndoCompare` with every test
still green.

**Five** of those ten exist because a review round found defects the first cut shipped, and all
five are worth recording as coverage lessons rather than quietly fixed. The shield was **added,
sized nowhere, and therefore inert**: every part of the mechanism — z-order, interception toggle,
menu bookkeeping — was correct while the component had empty bounds, so no click could ever reach
it. Geometry is the one property of that component a headless test CAN read, and there was no test
reading it. The second was a **vacuous first attempt**: the undo-depth test originally re-applied
the preset after editing, which is a real restore, so it never entered the retraction path it
claimed to cover and passed against the reverted fix. It is now a control/subject pair, and the
mutant separates them by exactly one step (127 vs 128). The third is the sharpest of the five:
a pop-up row's two right-hand furniture cases were covered by **`jassert`s declaring them
unreachable**, which is not coverage at all — `jassert` compiles out of every shipped build, so the
declaration bound only the developer, and the look-and-feel also serves menus this editor does not
build. The row now renders what it is handed, and the test that replaced the assertions fails on
the old drawing. An assertion that a case cannot happen is a claim about callers; where the callers
 include a framework, only rendering it is a claim about the code. The fourth is the same lesson
in the neighbouring override: `drawResizableFrame` is reached by two unrelated JUCE callers and
told them apart by a MAGIC NUMBER — a border of `getPopupMenuBorderSize()` on all four sides — so
a future `ResizableBorderComponent` with a 3 px drag zone would have silently lost the frame the
override exists to protect. It now also asks the editor whether a menu PARENTED TO THIS EDITOR is
on screen, and the test pins both halves: each is killed by its own mutant, on different
assertions. (A later round narrowed that second half again: it was first wired to `shieldRaised`,
which is true for any tracked pop-up including every combo drop-down — and a drop-down is a desktop
window that never reaches this override. Two tests that are both true whenever one of them is are
not two tests; it now reads `presetMenusOpen > 0`.)

**The fifth is the one that cost the most and is the most worth reading**, because the test and the
fix were both correct and the feature was still dead in the field. The no-op preset re-apply shipped
with two tests, both mutation-verified, both building a FRESH `AnabasisAudioProcessor` — whose
constructor seeds a valid `presetBaseline`. `setStateInformation` deliberately INVALIDATES that
datum (it is not serialized, so a load cannot honestly restore it) and nothing rebuilds it until the
next apply or save, so in every project opened from a host the retraction's dirty-datum comparison
was invalid-against-valid: never equivalent, always refused. The feature worked in exactly the
session shape the tests constructed and in no other. The lesson is not "add a restore case" but the
sharper one: **a fixture that a processor can only be in for the first few seconds of its life is
not the state the code runs in.** `setStateInformation` is the entry point for nearly every real
session, and a test suite that never passes through it is testing a constructor. The comparison now
tests what the datum MEANS (did the slot read clean either side?) rather than tree identity, and
`testANoOpPresetApplyIsNotAUserActionAfterASessionRestore` fails on the old expression while the
fresh-processor legs still pass — which is what shows the fix is a widening and not a loosening.

**The round-7 finding is the one that reframes this whole file's citations.** An audit of every
tracked `file:line` anchor in the governed documents found that MOST had been aimed at the wrong
code since before `check-citations.py` existed, and every re-anchoring since had carried each one
faithfully onto the same wrong text: `LATENCY_MODEL.md` cited the undo stack for `updateLatency()`,
`SERIALIZATION_REGISTRY.md` cited the latency predictor for `saveSlotFromLive()`, `THREAD_MODEL.md`
cited a comment for the OpenGL context. Fourteen were re-derived from the SYMBOL each prose claim
names, verified by reading the line, and declared in `DELIBERATE_REAIMS` so the correction is a
reviewable act rather than a silent rewrite. Two lessons are worth more than the fix: a tool that
preserves content identity makes a mis-aimed anchor look MAINTAINED, which is worse than leaving it
obviously stale; and the earlier entry above calling this "three citations" was itself an
under-count from inspecting three rather than auditing all of them — the same shape of error, one
level up.

**Superseded header (0.1.3, kept rather than deleted):** the **0.1.3 polish round (2026-08-09)** — seven owner items, display and
naming only, no DSP change, no new ADR (nothing decided at contract level: the three
Colour → Color NAME renames and the MATCH caption ride PARAMETER_COMPATIBILITY_POLICY rule 2
with the snapshot re-frozen, the same lane as 0.1.2's "Limiter Stereo Link"). **Amended:**
`CHANGELOG.md` `[0.1.3]` and its header (entry count); `KNOWN_ISSUES.md` KI-009 — since closed
and moved to `POSTMORTEMS.md` INC-004, see below — (the 0.1.3
addendum: the owner's per-channel GR-lane observation localises the left-channel kill to the
comp-output → wet-ring span, the span's audit against the new constraint, the permanent
field-configuration battery pair at OS Off/4×, and the OS-toggle field experiment it hands
back); `PARAMETER_REGISTRY.md` (three NAME cells); `docs/user/USER_MANUAL.md` (MATCH, Color,
the panel caption); `HANDOVER.md` (version/test rows); `README.md` (the suite count). In code
comments, the judgement calls are recorded where they act: the RMS readout hold in
`LoudnessMeterView` (the volatility was a display-cadence property, not a measurement defect —
the 50 ms ADR-0020 window is untouched; the cadence being divided is the PANEL's, which the
vblank-paced `FrameClock` runs at up to ~125 Hz), the GR-history left-edge fix in `GrHistoryView.cpp`
(the unmeasured-region zero line was also drawn for a full scrolling window, dropping
vertically into the trace at bucket-expiry phase), and the EQ band-per-row grid in
`PluginEditor.cpp`. **A review follow-up the same day** then landed the round's one code fix
and five corrections. The FIX is the first mechanism in KI-009's lineage that reproduces the
field fingerprint exactly — though a second follow-up round then established, from the owner's
re-test, that it is NOT the field trigger: the colour sub-block's
fifth-power polynomial can overflow to non-finite from a FINITE input once `clipDrive == 0`
exact-skips the clipper's own bound, and the engine's wet-ring boundary then substitutes
`0.0f` **per channel** — one channel permanently silent, the other normal, gated on Clip Mix
because the mix is what lets the stage reach the ring. Bounded at the argument
(`ClipSat::kArithmeticLimit`, +120 dBFS — inert and bit-exact for every reachable signal);
KI-009's 0.1.3 addendum ran to a FOLLOW-UP and a ROUND-2 addendum separating Class A (the
overflow, fixed and pinned) from Class B (the field report), each listing what the rounds had
excluded. **Both are superseded, and KI-009 is CLOSED (2026-08-11):** Class B was undefined
behaviour in `AnabasisEngine::processChunk`'s channel bound that Clang acted on at `-flto`, and
`POSTMORTEMS.md` **INC-004** now carries the mechanism, the excluded list and the guards, while
`KNOWN_ISSUES.md` keeps only the fixed-issue pointer its own convention prescribes — so the
KI-009 entry this ledger amended no longer exists to be amended. The corrections:
the `scHpfFreq` tooltip (it said "both detectors" a round after ADR-0023 made the limiter's
unfiltered), the RMS reference applied OUTSIDE the readout hold so a Settings flip is never
delayed (the hold belongs to the measurement), the bell rows' Q | Freq | Gain column order, the
`CLIP / COLOUR` heading the manual's own rename pass missed, macOS/Windows installer
Plug-in / Application capitalisation, and an English gloss for a quoted non-English phrase in a
source comment. Round 2 added: the new test registered in `DSP_POLICY.md`'s invariant→test map (invariant 9, extended with the PER-CHANNEL half and a rule for future stages — a stage whose failure mode is input-magnitude-driven must bound its own arithmetic, being in the sanitise list is necessary and not sufficient); the GR zero-region rule extracted as a pure pinnable predicate (`GrHistoryView::drawsZeroRegion`, five assertions, mutation-verified); and four wording drifts corrected — the CHANGELOG preamble's "no DSP behaviour changes" (the entry ships one audio-path edit, so it now reads "no audible DSP change"), the manual's claim that the knob hints carry the corner-dot legend (removed from the UI this round), and the manual's "the well opens on the spectrum" recipe (it opens on the GR history since 0.1.2). Suites 291 + 682; pluginval ×3 both modes at the `build.yml` strictness. **Round 3** closed the review's remaining honesty gaps: the ClipSat guarantee is now TRUE rather than nearly — the drive product `dry · g` could overflow the same way ~400 dB above the colour polynomial, so both sites take one bound (`kArithmeticLimit`, renamed from `kColourArgLimit` because it now answers one question for the whole stage) and the fuzz gained the two stimuli that reach them, each mutation-verified separately; both new tests now run under `juce::ScopedNoDenormals`, matching `processBlock`, after the review found the sustained case's two hottest rows passing on a SUBNORMAL limiter gain the shipped binary would flush — the sweep now stops below where FTZ, not this stage, decides the outcome, and says so; the RMS cadence prose corrected from "~24 Hz" to the panel's frame rate (`FrameClock` paces on vblank up to ~125 Hz) in all three copies; the manual's two remaining "Loudness Comp" captions rewritten to name MATCH while keeping the parameter's own name visible; and the `placeRow` spacer convention asserted rather than widened. KI-009 gained a ROUND-3 addendum recording the owner's bypass result (left returns ⇒ the loss is inside the processed leg, host routing and the format layer excluded, AU and VST3 identical on macOS), the five-step implication chain the evidence and the code now CONTRADICT, and two free field experiments that break the chain rather than a sixth hypothesis. Suites 291 + 682 = 973, re-counted from the suites' own output. **Round 4** corrected the last citations the round-3 rename left behind (`kColourArgLimit` → `ClipSat::kArithmeticLimit` in this file and in KI-009, and KI-009's "six assertions" → the eight the two non-zero-mix rows actually carry), re-worded the CHANGELOG's investigation sub-heading (it said "no code change to the audio path" inside an entry that ships one), made `placeRow`'s spacer skip release-safe as well as debug-asserted, and — the substantive part — PROVED step 4 of KI-009's implication chain instead of arguing it: `testClipSatCannotLoseAChannel` gained a CROSS-CHANNEL INDEPENDENCE section (bit-exact independence with the tame idle; ≤1 dB level movement at its 2 dB maximum), and mutation testing established the stronger structural result that the shared tame is a SHELF whose lowpass leg always survives, so it cannot zero a channel at ANY gain — the mutant that does break it is a shared envelope applied as a broadband gain (−48.5 dB), which is the real failure class. Suites 293 + 682 = 975. **Round 5** acted on the owner's new datum — the failure reproduces at the DEFAULT preset — and it retires the Clip Mix lead: at the Default preset the clip sub-block is exact-skipped, the colour residue is not evaluated and the tame takes its idle branch even with the §5.4 trim at its bound, so ClipSat's wet value IS its input and every mix branch lands on the same float. `testClipMixCannotChangeTheDefaultPresetsSound` pins that through the real wrapper — bit-identical renders at six Clip Mix values with both channels alive, then every factory preset at both endpoints — and is mutation-verified (a mix-dependent contribution at the null settings fails it at 23 027 samples). KI-009's ROUND-5 addendum records the proof, states that the correlation every prior round was steered by cannot be causal where the owner also observes the failure, and replaces it with three field experiments that discriminate "the value matters" from "touching the control matters". Suites 293 + 722 = 1015. **Round 6** answered the demand for runtime evidence: thirteen per-channel energy taps were temporarily instrumented from the raw input to the final write and run under the owner's exact reproduction configuration. No divergence — but reading the engaged values back exposed that `MacroEngine` maps on a 30 ms `juce::Timer` that never fires in a headless console app, so setting the Loudness parameter moved the knob and nothing downstream: every "loudness N" case in the channel battery had been re-running the DEFAULTS case with the clipper exact-skipped, for its whole life. That is the one configuration in which `clipMix` is not inert, so the coverage hole sat exactly where KI-009 needed it least. The battery now engages `clipDrive`/`colourDepth`/`dynTilt`/`limGain` directly beside the knob, with the timer reason recorded at the call site; the configuration still shows both channels matched within 1 %. KI-009's ROUND-6 addendum carries the trace, the hole and what it does and does not change about the report. **Round 7** (2026-08-10) acted on two owner reports and added no code. (a) The owner re-tested KI-009 on **Linux** and cannot reproduce it there, which narrows the report to the **macOS build**: KI-009's heading and a ROUND-7 addendum now say so, and the addendum reorders the hypotheses — the macOS-only divergence, previously an also-ran, is the only live one of the three that PREDICTS a platform split, so it leads; the state-restore and gesture-versus-value directions are demoted, not dropped. The operative consequence is stated in the entry: no Linux-side experiment can move KI-009 any further, so none should be run in its name, and the round therefore adds no test. (b) A NEW report — the Linux editor accepting no mouse input — became **KI-012**, carrying a runtime harness rather than an argument: the built VST3 loaded into a purpose-built minimal JUCE VST3 host on a real X server, driven by **XTEST** pointer events and read back with `XGetImage`, run both bare and under a window manager. Clicks land (the ADV toggle resizes the window), a rotary drag moves `Loudness` and the three parameters its macro map drives, and hover repaints 26 861 pixels against a 0-pixel idle control — so the symptom does not reproduce, and the entry records what that excludes (component tree, hit-testing, overlay z-order, the GL gate — the plug-in owns exactly one X11 window with no GL child) together with the sibling comparison the owner asked for, in which every interaction-relevant construct is identical and the one divergence found is off the input path. The evidence trail is `worklogs/2026-08-10-linux-editor-input-probe.md`; nothing entered the suites, deliberately, because a passing probe of a fault that never appears pins nothing. **Round 8** (2026-08-10) is the cross-platform round, and it starts with a correction to the ground the last three rounds stood on: the macOS CI job had failed to COMPILE `tests/state_tests.cpp` on every push since round 5's commit, and the broken line sits INSIDE `testClipMixCannotChangeTheDefaultPresetsSound` — so every KI-009 regression written for a macOS-only fault had never executed on macOS. `docs/POSTMORTEMS.md` **INC-003** carries the mechanism (an explicit template argument on `juce::jmax` instantiates JUCE's `dsp::SIMDRegister` overload; `size_t` is `uint64_t` on Linux and is not on macOS, so the same line is well-formed here and ill-formed there) and, as the expensive half of the lesson, what a red job on another platform costs an investigation. Amended for the round: `KNOWN_ISSUES.md` KI-009 gains a ROUND-8 addendum (macOS now green end to end — build, both suites, pluginval ×3 in both modes, packaging — so "does not reproduce" is finally a measurement on the reproducing platform; plus the exclusion table for the uninitialised-memory, wrapper/bus and warning-noise hypotheses, each killed by evidence rather than argument); `POSTMORTEMS.md` (INC-003, new); `docs/policies/TESTING_POLICY.md` (the Level-4 row now covers AU as well as VST3, a new Level-1b row, and the hard-gate section gains the AU requirement and the three cross-platform gates with an explicit statement of what each does NOT catch); `CHANGELOG.md` `[0.1.3]` (Added/Fixed/Investigation); `HANDOVER.md` (test rows). In code, the round adds `scripts/check-portability.py` (lint + compile canary, mutation-verified), the `source-lint`/`linux-clang`/`sanitizers` CI jobs, AU support in `scripts/run-pluginval.sh`, and two tests: the pushed-chain × non-zero-Clip-Mix cross-product the field report describes and nothing previously ran, and `testClipSatCannotHideANonFiniteFromTheBoundary`, which pins the one real defect class the five-lens audit found — at Clip Mix 0 the stage's output is the untouched dry sample, so a poisoned internal state is invisible to the boundary invariant 9's repair is keyed on. Unreachable in the shipped build thanks to 0.1.3's colour bound; the test fails with 32 000 non-finite samples on ORDINARY audio when that bound is removed. Suites 295 + 743 = 1038.
Previous: for the **0.1.2 field-fix round (2026-08-09, ADR-0023)**. The round's
documentation moved with its contracts. **New:** `ADR-0023` (the round's decision record: the
knee-above static curve, the unfiltered limiter detector with the comp-side clamp, the three
bus layouts, the fixed-scale GR history, the bottom-left toggle pill with GR the default view,
the caption/name split with the one deliberate NAME rename, the 940×822 Advanced layout, and
the per-channel GR publication). **Amended:** `ADR_INDEX.md` (the row); `PARAMETER_REGISTRY.md`
(footnote ²³ — the `stereoLink` name, `scHpfFreq`'s comp-only scope — and ADR-0019's footnote
cross-reference); `SERIALIZATION_REGISTRY.md` §1.6 (the `int_spectrumOn` default flip, recorded
as a default-only change under ADR-0016's unchanged semantics); `THREAD_MODEL.md` (the
per-channel `compGrDbCh`/`limGrDbCh` meter atomics on the existing relaxed row);
`MODE_AND_ADAPTATION_POLICY.md` (the scHpf trim reaches the comp's detector only);
`docs/user/USER_MANUAL.md` (layouts incl. mono→mono, the GR-default well and its pill, the
zone captions, the SC HPF scope, the fixed-scale history); `CHANGELOG.md` `[0.1.2]` (which
also releases the PR #12 `[Unreleased]` entries); `HANDOVER.md` (version/status/test rows and
the round summary — including the correction of the "Pre-0.1.0" Release Status drift);
`README.md` (the suite count, under its re-count rule). **Corrected:** `KNOWN_ISSUES.md`
KI-009 — the 0.1.2 addendum records the Delta observation's algebraic narrowing (the input pin
is live; the loss is in the processed leg; the fingerprint is not expressible in the current
source with intact state), the shipped instrumentation, and what the field must still supply.
Coverage: **Full** for the round's contracts (ADR + registries + policy + manual + tests);
confidence **Verified** — the tests in ADR-0023's evidence block, with eight negative controls
across two mutant builds, each killed by its own assertions.
**Three review rounds followed**, all folded into this entry rather than given their own: the
first reconciled the release state (the CHANGELOG header now carries the no-tag fact, since
`release.yml` publishes a version's section verbatim and a sentence about the tag not existing
would ship inside the notes that disprove it) and asserted the null test's adaptation premise.
The second cleared the **detector-path drift ADR-0023 left behind** — four live copies still
described the removed limiter high-pass: the detector-tap paragraph in `AnabasisEngine.cpp`
(which argued about a recursive filter on a path that now holds no filter state at all),
`AdaptiveEngine.h`'s null argument (the scHpf trim reaches the COMP's detector only, and its
"can only lower a level" claim is now enforced by the raw-magnitude ceiling rather than reasoned
from the RBJ passband the overshoot did not honour), `DSP_POLICY.md`'s invisible-failure class
(three stages → **two**, the limiter's high-pass recorded as having left with the filter rather
than deleted, since the failure mode still applies to any detector filter added later) and
`USER_MANUAL.md`'s adaptation paragraph. `DESIGN.md` §2.5 got a supersession banner in the
§4.3 shape — the signed-off P0 record is superseded section by section, never rewritten. That
round also seeded the editor's cached graph-well flag from the live tree instead of a
hard-coded default (the `shownTpMode` lesson, recorded below, repeating itself once the default
moved) and brought both per-stage GR lanes back under `publishSilentMeters`' one clear list,
which the limiter lane had left when it moved onto the engine's per-channel atomics and the
comp lane had never joined — mutation-verified. The third replaced the overshoot guard's POINTWISE form with a raw-magnitude peak ENVELOPE: `min(|filtered|, |raw|)` compared two samples the filter had put out of phase, so a bass-dominated `|raw|` crossing zero twice per cycle gated the detector at the bass rate — re-coupling the compressor to the very envelope `scHpfFreq` exists to make it deaf to. The CONTRACT did not move ("may only deafen" is about levels; the defect was enforcing it against an instantaneous sample), so ADR-0023's Decision item 3 is amended in place with a dated note and the measurements — 1.295 dB unfiltered · 0.291 dB pointwise · 0.0026 dB with the ceiling — while its supporting copies (`AdaptiveEngine.h`, `AnabasisEngine.cpp`, `DESIGN.md`'s §2.5 banner, `PARAMETER_REGISTRY.md` footnote ²³, `ADR_INDEX.md`, `CHANGELOG.md`, `HANDOVER.md`) were re-worded off "clamped to the raw one". That round also recorded the accepted mono→mono METERING consequence in ADR-0023 item 5 (BS.1770 sums channel energies, so a one-channel instance reads ~3 LU quieter — the standard's reading, deliberately un-normalised) and re-measured the compressor's budget row, which the extra per-sample envelope moved from 0.10 % to 0.24 % against its ≤0.3 % allocation. Suites 264 + 660.
Previous: the **ADR-0022 preset-identity port and its three review rounds
(2026-08-08)**. The port gave factory presets immutable internal ids and identified a user preset
by its file, so a shared name is a shared *label*; the documentation set moved with it. **New:**
`ADR-0022`, with the owner's serialization sign-off quoted in its Status banner and its scope
bounded to those fields. **Amended:** `ADR_INDEX.md` (the ADR-0022 row); `SERIALIZATION_REGISTRY.md`
§1.2 (the three additive `SLOT` properties, their defaults-on-absence, both encoder conditions, and
the inactive-slot absence-survives-re-save exception); `SESSION_COMPATIBILITY_POLICY.md` rule 4
(the round-trip list gains the indicator identity); `CHANGELOG.md`; `HANDOVER.md`. **Corrected:**
`KNOWN_ISSUES.md` KI-007 item 2, which recorded as OPEN the name-based ring defect this change
removes, and KI-003, whose restore-writer enumeration gained `liveSelection` — a set the
`historyEpoch` comment and `MODE_AND_ADAPTATION_POLICY.md` §KI-003 residual state in parallel, and
which the third round brought back into agreement (a member missing from one copy reads as one the
restore does not write). **Superseded in place**, per this file's own convention: the three
round-61/63 records of the deleted `rememberPresetSource` hint, whose ungated-RING half survives
the deletion as a requirement rather than as history — it is why `stepPreset` walks on past an
entry it cannot apply. Coverage: **Full** for the identity model, its wire form and its fallbacks
(ADR + registry + policy + tests all carry it); confidence **Verified** — `testPresetIdentitySharedName`,
`testFactoryPresetIdIntegrity`, `testPresetIdentityAcrossRestore` and
`testTheRingWalksPastAnUnreadablePreset`, with fourteen negative controls each killed by its own
assertions. **A known gap, recorded rather than filled:** this audit has no entries for the 0.1.1
release round or for review rounds 4–6; `HANDOVER.md` and `CHANGELOG.md` carry their records, and
reconstructing them here after the fact would be invention rather than audit (constraint C7).
Suites 259 + 634.
Previous: the **`shownTpMode` construction seed (2026-08-06)**. The editor's cached
TP-display flag was hard-coded `false`, but it caches *what the Ceiling boxes are showing*, not
the product default — and those two differ for an editor opened on a TP-ON session, where the
attachment renders " dBTP" at construction. If the mode then went OFF before the first 24 Hz
tick (~42 ms: a host write, a state load, the other TP toggle), the edge gate saw
`tp == shownTpMode == false`, skipped the refresh, and left " dBTP" standing over a sample-peak
ceiling until something else forced a recompute. Seeded in the constructor instead — from
`CeilingUnitSource::truePeakEngaged()`, the predicate the value-text lambda itself consults, so
"the cache equals the mode the visible suffix reflects" holds by construction rather than by
coincidence (a raw parameter read agrees only while the holder is wired; with it unwired the
lambda falls back to " dB" while a raw `true` would claim otherwise). `refreshCeilingUnit`
now reads that same predicate, so the gate and the suffix cannot disagree. Placed BEFORE
`startTimerHz` so the tick — the member's one reader — cannot observe the placeholder, rather
than relying on the message loop not running mid-constructor. It is an assignment and not a
`refreshCeilingUnit()` call because nothing needs refreshing at that point: only the cache has
to catch up with what is already on screen. The guard gained the reported scenario
(editor created TP-on, mode off before any tick) and is mutation-verified — restoring the
hard-coded seed turns it red. Suites 233 + 436.
Previous: the **DESIGN §4.3 attribution repair and the locked-override test (2026-08-06)**. The §4.3 supersession banner credited all of its rows to **ADR-0015**, including
`int_uiScale`'s ladder (ADR-0017's), and did not mention **ADR-0016** at all while the
`int_spectrumOn` row below it still read "spectrum overlay … dismissible". That is the widening
the three-record split exists to prevent, committed in the one document whose job is to point at
the right record. The banner is now a per-row table — one forward pointer per superseded row, to
the record that actually supersedes it — with §4.2's `ceiling`/`truePeakMode` defaults attributed
to ADR-0015 alongside, and a note recording the earlier misattribution rather than hiding it.
ADR-0016 and ADR-0017 gained the matching doc-sync bullet (each names `DESIGN.md` §4.3, which
neither did), and ADR-0016's clearance banner no longer says ADR-0017's gate is open — it was
cleared later the same day.
**Locked-ceiling coverage:** `testFactoryPresets`' lock check reached only the DEFAULTS half of
the rule once EDM Club's −0.5 override went with the ADR-0015 default change — no factory table
names `ceiling` any more, so the stronger "an override aimed at a locked parameter is skipped"
path had no coverage. Rather than add a preset to the shipped bank to have something to test
with, the new `testALockedCeilingSurvivesAPresetThatNamesIt` drives the FILE path, where a
document naming `ceiling` is exactly that collision, through `applyPreset (const XmlElement&, …)`
— the overload the wrapper's own preset ring uses, so it is the shipped path with the filesystem
left out. An unlocked pass first proves the document really does move the ceiling, so the locked
check cannot pass vacuously; mutation-verified (removing the skip in `applyOnePresetValue` turns
it red). The factory check keeps its assertion and gains a comment saying which half it reaches.
Suites 233 + 432.
Previous: the **third gate clearance and two rendering/comment fixes (2026-08-06)**.
**ADR-0017 CLEARED**, separately again — the owner's confirmation naming the reduced ladder, the
acceptance that out-of-set stored values normalise on adoption, and that this is a pre-1.0 decision
with no released-session migration obligation. Recorded in the four places the previous two
clearances were: the ADR's Status banner, `ADR_INDEX.md`, `HANDOVER.md` (item (i) struck plus a
round-log entry) and the two ledger rows. **That closes the round-2 batch: ADR-0015, ADR-0016 and
ADR-0017 are each cleared on their own terms** — the sentence the three-record split existed to
make true, since one record would have made the second and third clearances read as extensions of
the first.
Two code fixes alongside. `GrHistoryView` drew its "SPEC" chip FIRST so it would survive the reader
contract's three early returns, then stroked the trace and filled the waveform over the whole
`area` — which contains the chip's footprint, so at zero reduction the trace crosses the glyph
while `SpectrumView` (chip last) does not, contradicting the comment claiming the two chips share a
face. The traces move into a private `paintHistory` that keeps all three early returns verbatim and
the chip is drawn after it: same order as the sibling, same hit-area, no geometry change, and the
chip still survives an empty ring or an in-flight clear. And `iid::spectrumOn`'s identifier comment
still read "bool (dismissible, brief §6)" — the meaning ADR-0016 supersedes — in the very file that
ADR names as the field's home; every other site had been updated. It now states the mode semantics
and cites the ADR.
Previous: the **second gate clearance, ADR-0017, and DSP-test isolation (2026-08-06)**.
**ADR-0016 CLEARED** — separately from ADR-0015 and on its own terms, the owner's confirmation
naming the semantic change, the pre-1.0 migration decision, and the acceptance that stored values
keep loading with no migration path (an acceptance of the read delta the ADR tabulates, not a
claim that none exists). Recorded in the same four places the first clearance was: the ADR's
Status banner, `ADR_INDEX.md`, `HANDOVER.md` (item (h) struck plus a round-log entry) and the two
ledger rows.
**ADR-0017 opened** for a third member of the same class that had gone unrecorded anywhere:
`int_uiScale`'s ladder narrowed from seven steps (80/90/100/125/150/175/200) to five
(75/85/100/125/150). Type, unit and default are untouched — it is still a percent, 100 is still
the default — so this is a **domain** change rather than a semantic one, and a reader could argue
it sits below the gate's bar. It is recorded at the bar anyway, because
`SERIALIZATION_REGISTRY.md` §1.6 and §2 both name the ladder as part of this field's read
contract, and because over-recording a pre-ship change costs a paragraph while under-recording one
costs a user's window size. A stored 80 → 75, 90 → 85, 175/200 → 150, and the correction persists
at adoption; 100/125/150 are common to both ladders, so the ordinary session is untouched. Its
gate is **open** — neither 2026-08-06 sign-off names `int_uiScale` — and it is its own ADR rather
than a fourth item inside ADR-0016, which had just been signed off naming three.
**DSP-test isolation:** three invariant guards derived their bound from the POD default instead of
stating it, so a product default move silently re-calibrated them. `testOutputNeverExceedsCeiling`
and `testCeilingUnderOs` now pin `ceilingDbTp = −1` **and** `truePeakMode = false` (the harder case
for the sample-level backstop — with TP on, the TP-driven gain keeps the signal further from the
clamp the test exists to prove), and `testLimiterAlignment` pins the ceiling its two-sided
`>= ceilingLin * 0.995` check is calibrated against. `testNullWithDefaults` deliberately keeps
inheriting — "all-defaults is a bit-exact null" is its whole claim — and its comment now says so;
three stale "0.891 default" numbers in test comments corrected to 0.989.
**TP-default rationale recorded** (no behaviour changed): ADR-0015 §Consequences now states why
the meter row being off is deliberate — round-2 item 4 said "all TP toggles (**analyzer +
processor**) default OFF", so the diagnostic default was specified, not inherited — and carries
the two questions left for the fine review: whether the row should be the exception to the
family's opt-in pattern (it is the only one of the three defaults that removes a *diagnostic*
rather than a guarantee), and what the `warn` colour should compare against. The meter's own
comment now records the same trade at the comparison.
Previous: **ADR-0016 — `int_spectrumOn`'s semantic migration recorded (2026-08-06)**.
Review asked whether repurposing the field needed a record. It does, and by the repository's own
terms it needs an **ADR**, not a registry line: `ARCHITECTURE_REVIEW_GATE.md` lists "any field
add/remove/**semantic change**" and `SESSION_COMPATIBILITY_POLICY.md` rule 1 covers a field
"removed **or have its meaning changed**" — the same class as the `int_meterTargets` removal,
which got a full ADR. **ADR-0016** is deliberately its own record rather than a fourth item inside
ADR-0015: that one was signed off naming three changes, and widening a signed-off record after the
fact is the failure the last four rounds were about. Its gate is therefore **NOT cleared** and is
tracked as `HANDOVER.md` item (h) — nothing urgent, the pre-ship window is open.
The before/after was read from the commit that held it rather than recalled, which sharpened the
story: the old meaning was *"does the spectrum take half of the Advanced strip?"* — Advanced split
the strip and showed **both** views, and Simple did not read the field at all — so `false`
sessions display exactly what they always did and only `true` ones lose the GR trace that used to
sit beside the spectrum. Recorded at the two ledger rows (`SERIALIZATION_REGISTRY.md` §1.6 gains a
meaning-change banner; `PARAMETER_REGISTRY.md`'s inventory line says it is a semantic change and
not just a UI move) and in `ADR_INDEX.md`.
Alongside: `SpectrumView`'s comments still described the corner as a dismiss ×, quoted the removed
`setTooltip ("Spectrum")` identifier, and called the wording an open C8 owner TODO that the R2
item-11 directive had already discharged — the header banner, the hit-area comment and both
`hitTest` consequences now describe the mode chip that is actually there, including why the
narrowed tooltip scope is now the *right* scope rather than a cost.
Previous: the **Architecture Review Gate clearance and two cleanups (2026-08-06)**.
The gate is **CLEARED** — the first clearance in this repository. Two of ADR-0015's three contract
changes are `ARCHITECTURE_REVIEW_GATE.md` items in their own right (a Serialization Registry
change and a Parameter Registry change) and both sit on `CLAUDE.md`'s Hard Stop list, which a
green build explicitly does not clear; they had landed on the round-2 directive plus a
self-authored ADR, which the review flagged twice — correctly, since the gate's Procedure puts the
human review *before* the merge, so an ADR written in the same PR is a record and not a clearance.
The owner has now reviewed and signed off all three by name. Recorded where the repository's
process puts it: **ADR-0015's Status banner** (an ADR's Status is where its authority lives, so
the sign-off is quoted there verbatim), **`ADR_INDEX.md`** (so "was the gate cleared?" is
answerable without opening the file), **`HANDOVER.md`** (item (g) of the fine-review list struck,
plus a dated entry in the round log — the chronological record of record), and a line at each of
the two ledger rows the change touches, **`PARAMETER_REGISTRY.md`** ¹⁵ and
**`SERIALIZATION_REGISTRY.md`**'s header. Those three decisions are now **settled, not ⊕**;
everything else under the v0.1.0 blanket approval keeps its ⊕, including the ⊕ on the mode-aware
unit's *wording*. The ordering is written down as the part not to repeat.
Two cleanups alongside: `LoudnessMeterView::paint`'s trailing `area.removeFromTop (6)` — the gap
that used to sit above the §6.4 penalty rows, reserving nothing since they left with the
streaming-target display — is gone (the 6 px gap *above* the TP row is a different, live one), and
the manual's graph-well bullet said "the choice is session state" twice.
Previous: the **third PR #8 review round — one false lifetime invariant withdrawn
(2026-08-06)**. `CeilingUnitSource`'s member comment and ADR-0015 §5 argued that declaring the
holder before `apvts` made it *outlive* the value-text lambda that captured its address. It does
not: the APVTS constructor hands every layout parameter to `AudioProcessor::addParameter`, so the
parameters — and their lambdas — belong to the **base** class and `~AudioProcessor` destroys them
after every derived member; `apvts` also dies before the holder, leaving `truePeakRaw` dangling
for the rest of the derived teardown. Declaration order buys the **construction** half and nothing
more, and that half is real. What makes the arrangement safe is a runtime fact — `getText` is
called while the processor is live and nothing in JUCE queries parameter text from a destructor —
so the hazard is latent, not live. The ownership is deliberately **unchanged**: the wrong claim is
withdrawn at all four sites that carried it (the member comment, `CeilingUnitSource`'s header
block, the wiring comment, ADR-0015 §5 and its Consequences bullet) rather than repaired by
inventing a shared handle for a display string, because the false invariant was the actual defect
— it told a maintainer the lifetime was proven, and pointed the remedy at keeping two lines in
order when the real remedy would be a handle the parameters can own. No behaviour change; suites
and pluginval unchanged-green.
Previous: the **second PR #8 review round — the Ceiling unit's missing refresh half
(2026-08-06)**. The mode-aware unit was correct on the parameter side and its test passed there,
but nothing refreshed the CACHED value-box label: a JUCE `Slider` recomputes it only in
`updateText()` — on a value change, a `setTextBoxStyle`, a relayout or a look-and-feel change,
never on a repaint — and flipping TP does none of those (the graph-well branch that used to call
`resized()` from the tick is a visibility flip now). So a generic host editor showed the right
unit while the plugin's own readout carried exactly the stale claim the change existed to remove.
`refreshCeilingUnit()` runs on the same 24 Hz tick, edge-gated, refreshing both ceiling controls
(one parameter shown twice), and is **public for the same reason `refreshInternalSettingsBoxes`
is** — no message loop runs in the headless suite, and this is the second time a state→widget
direction has been the missing half of a change that looked complete from the parameter side. The
guard now checks the LABEL, not a live re-computation, and was mutation-verified: removing the two
`updateText()` calls turns it red. Also corrected: the About copy said "hold a true-peak ceiling",
the same inter-sample over-claim one panel over; the About snapshot test still reconstructed the
old 400×232 geometry, so its "textured rows" heuristic sampled a band that no longer matched the
content inset (440×290, copy stack 200 px of the 238 the inset leaves); the stereo guard's `run`
lambda divided by `256.0 * 512.0` where the settled half is 30 × 512 — an ~8.5× over-division that
left a permanent guard passing by 20 % instead of 3.5× and printing wrong numbers in its own
failure message (now derived from the loop's own `blocks`); KI-009 and the CHANGELOG said "seven
configurations" against their own six-item lists; both `nearest`-rule comments still worked their
examples against the seven-step ladder (92 → 90, 50 → 80, 300 → 200, which are now 85/75/150); and
the manual narrated the GR history as the bottom view a fresh instance shows, when `int_spectrumOn`
defaults true so the well opens on the spectrum, and still described the TP meter row as something
you hide rather than something you show. HANDOVER's Pending Tasks row gained item (g): an explicit
owner acknowledgement of ADR-0015's **schema** half is owed — the directive named the display
removal, the field removal followed from it, and a serialization change is its own Hard Stop.
Suites 233 + 426.
Previous: the **PR #8 review round — ADR-0015 and the true-peak honesty pass
(2026-08-06)**. The round-2 batch changed three things the repository's own rules put behind a
decision record — `ceiling`'s default, `truePeakMode`'s default, and the removal of the
`int_meterTargets` session field — and landed them with prose justification only.
**`ADR-0015`** is that record: it takes the pre-ship latitude
`PARAMETER_COMPATIBILITY_POLICY.md` §"Getting it right the first time" already describes, names
the §4.4 read rules as the migration `SESSION_COMPATIBILITY_POLICY.md` rule 1 demands, and states
the condition that closes the window (**the first build that leaves this repository**, not the
first tag). It amends rather than rewrites the two ADRs whose incidental numbers moved — ADR-0006
(the two defaults it quotes in passing) and ADR-0010 (its ten-field host-hidden inventory) — each
of which now carries a forward-pointing banner, and the index gained an **"ADRs amended by a later
ADR"** registry so an auditor knows before reading whether any of a record has moved. `DESIGN.md`
§4.3 carries the same banner and is otherwise untouched, per `SOURCE_OF_TRUTH.md`: it is the
signed-off P0 record, superseded section by section, never rewritten.
The ADR also settles the consequence the default change created: with `truePeakMode` off the
ceiling is a **sample-peak** limit (`DSP_POLICY.md` invariant 3, ADR-0006 item 3), so the
unconditional `" dBTP"` suffix claimed an inter-sample guarantee the shipped default does not
make. The unit now follows the mode (`CeilingUnitSource` — the holder the processor declares
*before* `apvts`, which is what puts its construction ahead of the layout call that captures its
address; unwired it falls back to the weaker `" dB"`), and the tooltip and four manual sites say
which limit is live. The DSP needed
no change and neither invariant needed amending — both were already mode-conditional, which is the
load-bearing observation. Also fixed: `LoudnessMeterView`'s TP snapshot seed and read fallback both
encoded `true` against a default of `false`, so the first painted frame showed a row the default
does not have; the UI-scale ladder comment still argued from the seven-step ladder's 80 %; the
zone-combo comment described a header placement the code never produced (and the CLIP well's
knob row gives back 8 px, which closes a curve overhang that predates the combo move); and two
factory-index test comments went stale when "Default" took index 0 — one had quietly become the
gentlest preset in the bank where it wanted the loudest, the other had stopped exercising the
override path it exists for. New guard: `testTheCeilingAdvertisesTheUnitItEnforces` (suites
233 + 420).
Previous: **round-2 item 2 — the layout coherence pass, verified against
rendered snapshots (2026-08-05, owner directive)**. The editor was actually LOOKED AT for
this one: an env-gated `createComponentSnapshot` dump (temporary, not committed) rendered
Simple, Advanced and Settings headlessly, and the pass fixed what the pixels showed rather
than what the code implied. (1) Every Advanced zone's MODE combo now sits in the zone
HEADER, top-right beside the caption — the slot EQ's Pre/Post already used; before, three
zones parked it in three different places, and the LIMITER's foot row squeezed AUTO + TP +
Style into one line where both toggle labels truncated to ".." (`drawToggleButton` fits text
into what remains right of the pill; 44–52 px cells leave ~16 px). Foot rows now carry
toggles only, at half-panel widths. (2) The Simple ceiling cell's TP/LOCK column widens
54→74 px — "LOCK" truncated to "L…" at the shipped 52. (3) Input Gain and SC HPF become
horizontal FADERS in the utility strip (the family's `drawLinearSlider` language, value box
beside the track): a strip-squeezed rotary was the smallest control on the page for two
set-by-ear parameters; same Knob objects, attachments, reset and tooltip — presentation
only. (4) The Settings panel shrinks 398→310 px to fit its content — the target checkboxes
and the Spectrum toggle left ~90 px of dead glass when they were removed. GUI appearance
stays Level-5: the snapshots verified geometry (no truncation, consistent slots), not brand
quality, which remains the fine review's item. No parameter, attachment or behaviour change;
suites and pluginval unchanged-green.
Previous: **round-2 item 11 — the complete tooltip set, the sibling's rules
(2026-08-05, owner directive)**. The C8 rule ("free tooltip prose is owner-supplied, not
invented") was discharged for tooltips by the directive itself — the owner asked for a
generated set, so the copy ships ⊕ like the About description. `tidyTip` becomes the
sibling's real rule (trailing full stops stripped centrally) instead of the stub it had been;
every parameter's hint lives in ONE table (`tipFor`, keyed by parameter ID) so a knob and its
Simple-view twin (ceiling, TP) cannot drift apart, with a debug assert on any parameter added
without one. Voice is the sibling's: one terse line, plain " - " dash, no trailing period;
its exact top-bar strings are taken verbatim (A/B "A/B Compare", Copy, Undo/Redo,
Previous/Next preset, "Presets"); `bypass` stays deliberately tipless (the red pill labels
itself) and the graph-well chips name their ACTION ("Switch to the …", firing over the chip
only, since `hitTest` narrows the pointer claim). Accessibility titles deliberately do NOT
follow the tooltips: they stay the registry names (brief §8) — `setupCombo`/`setupToggle` now
derive the title from the parameter, and the Internal helpers take an explicit `name`
parameter (the Settings row label, which is also what the state suite finds them by).
Tests: `testEveryKnobAndComboCarriesATooltip` sweeps every slider/combo and the named toggles
(13 new checks; suites 233 + 412). Manual's Tooltips row, HANDOVER's Test Status and README's
count synced.
Previous: **round-2 items 8/9 — the Simple-view TP switch and the combined
Spectrum/GR graph well (2026-08-05, owner directive)**. Item 8: `truePeakMode` gets the
"highly visible" main-UI switch the directive asked for — a second attachment (`tpSimpleToggle`)
in the Simple ceiling cell, stacked **TP above LOCK** (dual attachments to one parameter are
ordinary APVTS practice; the Advanced limiter zone's TP is unchanged). Item 9: ONE graph well,
both editor modes, two switchable views — `int_spectrumOn` is now the MODE flag (true =
spectrum, false = GR history), both views hold identical bounds and only visibility flips, and
the switch lives on the graph itself as corner chips that name the view you switch TO ("GR" on
the spectrum, "SPEC" on the history; `GrHistoryView` moved from wholesale mouse opt-out to the
same per-pixel `hitTest` discipline the spectrum uses, so trace clicks still fall through). The
Settings "Spectrum" toggle is REMOVED; the editor tick's mode follow is a visibility flip, not
the old `resized()` re-partition. The sibling's LF spectrum smoothing is ported into the trace
read (ADR-0009 provenance: `SpectrumImager::magForColumn`/`magCubic` two-regime rule — columns
spanning <1.5 bins take a Catmull-Rom across the four surrounding bins, wider columns average
every covered bin; adapted to this analyser's dB-domain EMA, and the DC-bin exclusion kept).
The §6.2 "GR-only Simple strip" wireframe is superseded (recorded at the layout site). Tests:
the spectrum-corner test generalised to both views' chips (`…OnlyClaimTheirModeChips`, 5 new
checks; suites 233 + 399). Manual §3.2/§3.4/§3.5, PARAMETER_REGISTRY's host-hidden inventory,
HANDOVER's Test Status row and README's count synced.
Previous: **round-2 items 3/10 — the sibling's icons, About layout and A/B oval
(2026-08-05, owner directive)**. Undo/redo carry the sibling's circle arrows (U+21BA/U+21BB) —
the half-arrows they replace inverted their reading under the icon treatment's 180° rotation,
which is what the "ugly icons" report was; the glyph test now pins the new pair and records
the why. The About panel matches the sibling's content layout exactly: 440×290 geometry, the
12 pt/0.22 subtitle tracking, and the link band LEFT-aligned at the content inset (170×20,
50 px up from the bottom), styled the sibling's way — accent, 13 pt, **no underline**, no
tooltip. The one-sentence product description the C8 rule had deliberately withheld is now IN,
under the owner's explicit round-2 instruction to generate it ("I will let you know later if
it needs tweaking" — the authority supplied, ⊕ on the words), in the family sentence-shape and
drawn with the sibling's fitted-4-lines idiom. A/B takes the sibling's shorter oval
(`removeFromRight (46).reduced (0, 1)`, its #4) instead of the 64 px slab. Brand-checklist
Level-5 deviation candidates shrink accordingly.
Previous: **round-2 items 12/13/14 — Settings polish (2026-08-05, owner
directive)**. The UI-scale ladder is now the SIBLING'S: five steps 75/85/100/125/150 % shown
as **XS/S/M/L/XL** (M = the original size; `ui_scale::names` index-locked to `steps` by a
`static_assert`), keeping the field a percent so the schema's meaning is unchanged — an old
seven-step session converges through `nearest` like any other out-of-list value, which the
uiScaleClamp test now exercises with the new ladder's own numbers (92→85 keeps the
displayed-step-doesn't-move property). Terminology to Title Case and the sibling's names:
"Offline Render", "UI Scale", "UI Animations", "True-Peak Meter", and the vague "Follow" mode
is now "**Follow Online**". The Save-Preset dialog's buttons swapped: **Save left, Cancel
right**. Manual Settings table and both inventories synced.
Previous: **round-2 item 5 — streaming-platform analysis removed outright
(2026-08-05, owner directive; OQ-008 → Resolved-by-supersession)**. Gone together, because they
were one feature: `LoudnessMeterView`'s `kTargets` table + tick overlay + penalty rows + the
"as of" tooltip, the three §6.4 Settings checkboxes, and the `int_meterTargets` field itself —
a pre-ship schema removal, free by the contract's own terms; an old session carrying the field
is ignored by the §4.4 unknown-field rule (the registries record the removal rather than
pretending the field never existed). The meter tooltip is now the click-to-reset line alone.
Tests: the target-checkbox and tooltip-quotes-the-table sections of `settingsFollow` removed
with the feature (suite 404 → 394); the TP-row comment's stale "EDM Club ships −0.5" example
fixed in passing. Manual (five sites), README's planned-scope bullet (which now names the
directive rather than silently contradicting the brief), HANDOVER's three OQ-008 rows and both
registries synced.
Previous: **round-2 item 6 — the "Default" preset (2026-08-05, owner directive,
the sibling's pattern)**. Factory index 0 is now `{ "Default", nullptr, 0 }` — "defaults +
intents" with zero intents IS the default patch — and the fresh-constructed state carries its
identity: `livePresetName` seeds to "Default" before the default slot is captured (so both A/B
slots open named), the dirty baseline seeds right after (so an untouched instance reads clean
and the first edit stars), `resetSlotFieldsToDefaults` makes "Default" the field's §4.4
default (a missing-AB session now shows it — `testAbToleranceRules` pins the new value), and
`getProgramName` returns the sibling's constant. Bank count 12 → 13; EDM-Club-specific test
indices shifted; new checks: index 0 shape, fresh-instance-clean, edit-stars,
re-apply-restores-clean. Manual/README/HANDOVER counts synced.
Previous: **round-2 items 4/7 — True Peak defaults OFF, every ceiling to −0.1 dBTP
(2026-08-05, owner directive)**. `truePeakMode` default 1→0, `int_tpMeterOn` true→false,
`ceiling` default −1→−0.1 (parameter, POD, and EDM Club's now-redundant −0.5 override removed —
its slot in `testFactoryPresets` now proves the *un-overridden-sits-at-default* half instead).
The registry snapshot was **deliberately re-frozen** — the Hard-Stop item, taken under the
owner's round-2 autonomous-decision directive with nothing yet shipped, so the compatibility
cost is zero by the contract's own terms. `testLimiterTruePeakMode` pins its ceiling explicitly
now (its stimulus is calibrated against −1 dBTP and the test is about TP-awareness, not the
default). PARAMETER_REGISTRY rows and the manual's three default mentions updated.
Previous: **three further review-confirmed corrections (2026-08-05)**, all
content-only:
(1) **This file's own inventory understated what exists** — the architecture self-coverage row
still listed five descriptive documents after `SERIALIZATION_REGISTRY.md` and
`LATENCY_MODEL.md` landed, while the gaps list two screens down recorded both as landed and
`REPOSITORY_MAP.md` listed all seven. The audit's own Update protocol says "a new doc → add to
self-coverage", so the file that exists to catch staleness had three lists and two of them
right. The row now enumerates all seven (dates grouped rather than repeated); directory and
row now match one-for-one.
(2) **The last two future-landed pointers to `COMPATIBILITY_MATRIX.md`** — OQ-011's Decision
and the `build.yml` macOS-configure comment both still asked for the supported-OS claim to be
"restated in COMPATIBILITY_MATRIX.md when that document lands (P2)". Both now record the
obligation as discharged **and** that it landed later than the P2 they targeted — the
historical meaning of the original line is what makes the lateness visible, so it is kept
rather than erased. The workflow change is comment-only; no step, condition or value moved.
One occurrence is deliberately untouched: the dated 2026-08-05 entry below **quotes** the old
OQ-011 wording as the evidence for why the matrix was owed, framed in the past tense — a dated
entry recording what a document said then is the historical record working, not drift.
(3) **A user-manual claim the manual's own §6 contradicted.** The Simple-view table said
Loudness at 0 "leaves the sound untouched", while §6 states the ceiling clamp is always last
before dither and the output never exceeds it. At `l = 0` the macro curves do neutralise the
push (`limGainDb`, `clipDriveDb`, `dynTiltDb` all reach 0 — `src/MacroEngine.h`), which is why
the all-defaults null is bit-exact for material below the ceiling; a master already hotter
than the Ceiling is still limited. The row now says what a user can observe — no push at 0,
Ceiling still holding — without importing the curve detail.
Previous: **three review-confirmed documentation corrections (2026-08-05)**, all
content-only:
(1) **The lifecycle trigger map was satisfied at two of its three targets.** The
`ANABASIS_CXX_STANDARD` seam updated `CI_CD.md` (the canary's own section) but not
`BUILD.md`'s "Build options" table — and `DOCUMENTATION_LIFECYCLE_POLICY.md`'s
"Build / CMake / JUCE pin / C++ baseline" row names `BUILD.md` first. It is also the option
that most needs to be there: the knob exists precisely *because* the obvious
`-DCMAKE_CXX_STANDARD=23` is shadowed silently, and the person who would try that is reading
the build guide. Row added with its legal values, the 20-ships/23-canary split, the
never-use-CMAKE_CXX_STANDARD rule, and the cache-persistence consequence. `DEPENDENCY_POLICY.md`
— the row's third target — is deliberately untouched: it already carries the C++23 *policy*
(baseline, feature-test-macro rule, canary-never-gates), and the cache variable's name is a
build-usage fact, not a dependency rule.
(2) **The stale-phase class the batch set out to repair, in the file that defines authority.**
`SOURCE_OF_TRUTH.md`'s legal-class list still read "`NOTICE` and `THIRD_PARTY_LICENSES.md`
(added at P6)". The parenthetical is gone; the authority statement is unchanged. No date or
phase replaces it — an authority document says what ranks above what, and *when a file landed*
is exactly the kind of history this class of drift comes from (status lives in the coverage
self-coverage row and `REPOSITORY_MAP.md`).
(3) **The user manual's automation answer drew the line in the wrong place.** It listed eight
of the nine `withAutomatable(false)` parameters and then said "Settings items are not
parameters at all" — leaving the missing ninth, `advancedMode`, to be read as Settings state
when it is a host parameter (`src/PluginParameters.cpp:145`). Two distinct facts now stated as
two: host parameters not offered as automation targets (now including the **ADV** toggle), and
the Settings overlay's items, which are not host parameters at all. No new claims, no
expansion.
Previous: **`LATENCY_MODEL.md` landing (2026-08-05)** — the second of the two
documents `COMPATIBILITY_POLICY.md` cites as contract authorities that did not exist. The
model is deliberately thin where `src/dsp/Latency.h` is already the single source: the
`kOsLatMin`/`kOsLatLin` values are quoted **nowhere in the document** (they are measured
against the pinned JUCE tree, so a prose copy is a stale copy the day the pin moves —
the same reasoning as the pluginval strictness); what the document adds is the map — the
two-term contract, what never moves PDC (the lookahead knob, presets, A/B, undo, loads)
versus what does (OS factor/phase, the offline Force-Max flip), the ADR-0003 measurement
tap contributing zero with RISK-008's measured resolution, the five recompute triggers plus
the load's deliberately redundant sixth call (documented as a no-op at its own site), the
one-latency-event-per-load batch, and the verification map from property to named test.
Previous: **`SERIALIZATION_REGISTRY.md` landing (2026-08-05)** — the schema-v1
ledger `COMPATIBILITY_POLICY.md` has cited as the serialization authority since bootstrap,
now real: the session blob's full tree (root/children/properties in write order, each cited
to `PluginProcessor.cpp`), the raw-vs-value fidelity split, the SLOT StateSet shape and its
two deliberate asymmetries (full-surface slots with apply-side view exclusion; DETACH_MASK
always written), FROZEN_TRIMS' three conditional-write rules, ADAPTIVE's absent-=-never-learned
discriminator, the read-rule table (including that a version gate would *reverse* ADR-0007),
the `.anabasis` format, and the §4 statement that no legacy fixture exists yet *because
nothing has shipped* — with the freeze scheduled for the v0.1.0 tag.
Two findings the evidence pass itself produced, both fixed in the same unit:
(1) **`BASELINE` has no originator in this build** — the only code that constructs one is the
test that seeds it; the wrapper adopts, carries and drops the child but never creates it. The
registry records it as schema-reserved, adopted-only — tolerate on read, never invent on
write — instead of describing a producer that does not exist.
(2) **A sibling-inherited manual sentence did not survive contact with the code.** The user
manual's §7.3 claimed an old preset's omitted parameters "keep their default" — true of the
sibling, and true here of *sessions* (defaults-first) and *factory tables* (defaults +
intents), but the FILE apply is **overlay-only** (`applyPreset` writes exactly the PARAM list,
no defaults pass), so an omitted parameter keeps its live value. Invisible today (the writer
emits every non-excluded parameter); visible the first time a build adds one. The registry
states the asymmetry precisely, and the manual sentence is corrected to what both paths
actually do.
Previous: **the adversarial verification round over the 2026-08-05 batch** — every
checkable claim in the six new commits was independently re-derived (citations, symbol probes,
workflow structure, cross-document coherence), each reported discrepancy then adversarially
re-verified before being trusted. The canary commit came back clean; eleven findings elsewhere
were confirmed and are fixed in this change:
(1) **A fabricated quotation** — the matrix's AAX row attributed the sentence "AAX is not
supported" to brief §2, which excludes AAX only by omission (the sentence exists in the README;
the brief's §14.3 uses AAX as its Not Supported *example*). The row now says exactly that. The
same mis-attribution pre-exists in `COMPATIBILITY_POLICY.md` — left for its own change, being a
policy file (recorded here per C6 instead).
(2) **A stale-count violation of this batch's own making** — the matrix quoted the
~46-notification preset burst in live prose while assigning ownership of that bound to the
checklist in the same sentence (and the number had already drifted: round 50's
`setParamIfMoved` made it an upper bound). The number is gone; the checklist owns it.
(3) **The one live pluginval-strictness quote left in the doc set** — HANDOVER's Pending Tasks
item (f), pre-existing from 2026-08-02, three rows below the row that says the number lives in
`build.yml` alone. Now it defers like everything else.
(4) **Passed-phase markers this batch's repairs missed, all in `REPOSITORY_MAP.md`**: the legal
class still "[P6]" though its factual half landed; `packaging/`, `release.yml` and PACKAGING
still "[P6]" though OQ-007 moved them; both workflow enumerations missing `cxx23-canary.yml`.
All re-pointed.
(5) **Two lists describing one set with different membership** — the map's planned descriptive
docs (8 names) vs this file's gaps list (6): `API_REFERENCE.md` and `STATE_SERIALIZATION.md`
added here.
(6) Small-bore exactness: the matrix's bus-declaration citation was off by one line (`:10-13`,
the output bus was outside the cited range); `PERFORMANCE_BUDGET.md`'s map entry gets its date
beside the phase label; the gaps list's legal bullet said "producible now" one commit after the
files existed; and the inventory's claim that `NOTICE` uses the FTL credit "verbatim" overstated
byte-fidelity (the year placeholder is filled, as the FTL itself instructs) — reworded to say
precisely that.
Previous: **the User documentation class landing (2026-08-05)** —
`docs/user/USER_MANUAL.md` + `docs/user/INSTALLATION.md`, closing the class that P6's target
passed unmet. Method, because a user manual is where invented facts hide best: every stated
fact was read from the tree before it was written — the registry's ranges and names verbatim
(the 49-row table), the twelve factory preset names from `PresetManager.cpp`'s `kFactory`
table, the preset folder from `userPresetDirectory()`, Learn's 5-second minimum from
`kLearnMinPassMs`, the tilt pivot from `kTiltPivotHz`, Force-Max-equals-16× from the PDC
callback's own comment, the About/‹›/edited-dot/meter-click affordances from the editor's
constructors, and the stereo-only bus contract from `isBusesLayoutSupported`. What the manual
deliberately does NOT do: quote the GR-history window length, the undo cap or any performance
number (the counts rule and C2 apply to user docs too — behaviourally visible limits are
described, magic numbers are not shipped to users); claim a transport-follows meter reset the
sibling has and this product does not; or describe automation of the nine advisory
non-automatable rows as impossible (hosts may expose them — the manual says so). INSTALLATION
is written for what v0.1.x actually is (OQ-007 plain zips): manual copy paths, the
executable-bit restore the artifact transport makes necessary (`build.yml`'s own NOTE), the
quarantine steps for ad-hoc-signed bundles, and an honest "no checksums yet" section. Voice
and structure adapted from the sibling's manual under ADR-0009; all product wording ⊕ for the
fine review. `REPOSITORY_MAP.md`'s user row updated; the gaps item closed.
Previous: **the RISK-009 trigger-passed note (2026-08-05)** — the risk register's
variable-font entry gained a dated blockquote in the RISK-008 pattern: its trigger ("P5 reaching
typography with no approved font licence") has passed without materialising — the shipped P5
uses the platform default and embeds nothing, the fact the attribution inventory independently
re-verified — and the entry now says when it re-arms (a brand-pass decision to adopt a font)
and what that costs (licence-before-adoption per brief §13, plus a new inventory row). The
entry is annotated, not deleted; the register's own rule is that a risk moves only when it
materialises.
Previous: **`COMPATIBILITY_MATRIX.md` landing (2026-08-05)** — the descriptive doc
the fine review could not start without: `OQ-011` directed its supported-OS claim to be
"restated in COMPATIBILITY_MATRIX.md when that document lands", and HANDOVER's Pending Tasks
row points the DAW-matrix audition at "`COMPATIBILITY_MATRIX.md` targets" — while the document
did not exist, references to a plan mistaken for a deliverable. It now exists at the sibling's
location (`docs/architecture/`, ADR-0009 structure, findings re-derived): formats (AAX Not
Supported per the brief's own exclusion), the three blocking platform gates (quoting no
strictness number — the single-place rule), the OQ-011 macOS floor restated with its evidence
and **no invented Windows/Linux floor** (C7), stereo→stereo as the only accepted layout
(`isBusesLayoutSupported`, with the mono delta from the sibling stated as a scope decision),
the audition target rows A1–A3 (all `Unverified`, with the rule that no host flips without
per-host evidence — the ~46-notification preset burst check stays owned by
`RELEASE_COMPATIBILITY_CHECKLIST.md`), and the pins (JUCE SHA, C++20 + the 20/23 canary seam,
pluginval deliberately recorded as unpinned). `REPOSITORY_MAP.md`'s architecture row now says
which descriptive docs exist with dates and which remain planned with none claimed; the gaps
list and self-coverage row updated in step.
Previous: **the third-party attribution landing (2026-08-05)** — the factual half of
the legal class, produced by the method `RELEASE_POLICY.md` §"Third-party attribution" itself
prescribes (which had been *requiring* these files with every binary distribution since
bootstrap, while neither existed — a policy-vs-tree drift this closes):
(1) **`NOTICE` + `THIRD_PARTY_LICENSES.md` (root)** — inventory read from the pinned JUCE
`LICENSE.md` plus a walk of the compiled TUs (which is what catches FreeType and stb, vendored
transitively inside PlutoVG and absent from JUCE's own list); compiled-in status verified by
`nm` probes on **this build's per-TU object files**, because the linked image is LTO'd with
`--gc-sections` and hides most static C symbols — a probe against the `.so` reported almost
everything absent, which is the wrong answer arrived at honestly and is why the method sentence
in the inventory names the objects as the evidence; exclusions (MP3, LV2, AAX, ASIO, Oboe,
CHOC, Box2D) confirmed by their compile gates *and* symbol absence. Structure and protocol
adapted from the sibling under ADR-0009 with provenance stated in-file; **findings re-derived,
not copied** — the sibling's own rule, and this repository's (C7). One genuine delta from the
sibling's record: Anabasis's Linux `.so` carries only seven `DT_NEEDED` entries — JUCE 9 loads
the X11/GL stack dynamically — so §5 records direct-vs-runtime linkage as two lists instead of
one `ldd` closure.
(2) **The obligation now travels with the binaries**: all three `build.yml` staging steps copy
both files into the customer artifact (the IJG/BSD notices attach to binary redistribution, and
CI artifacts reach beta testers), and the three adjacent "added at P6 (packaging/)" comments now
cite OQ-007 — the same passed-phase staleness the drift repair fixed elsewhere.
**Superseded 2026-08-06 by ADR-0021**, and the record is left standing rather than rewritten
because it states what THIS round did: the staging copies were removed, and the obligation now
travels as version-named release-page assets — the one carrier the `.pkg` and Inno routes pass
through, which the staging copies never reached at all.
(3) Synced: README §Licensing (pointers to both files), the legal self-coverage row (factual
half Present; owner-legal half — EULA/PRIVACY/TRADEMARKS — stays absent on C8/OQ-002, stated
per the sibling delta: Anabasis has no EULA even as a draft), HANDOVER's post-v0.1.0 section.
Previous: **the self-coverage drift repair (2026-08-05)** — the change the canary
entry queued, applied row by row against the tree rather than wholesale:
(1) **The audit's own tail was a ~P2-era snapshot**, in the file whose job is noticing exactly
that. Repaired with evidence per row: the code-module table gains the two rows the v0.1.0 tree
has that it did not (MacroEngine/PresetManager/InternalState; `src/gui/` — behaviour Verified via
the editor-constructing state tests, appearance deliberately Level-5), and its closing sentence
no longer calls `src/gui/` "planned" two phases after it landed. The architecture row stops
enumerating ADRs — `ADR_INDEX.md` is the registry, and the enumeration here had gone stale the
moment ADR-0013/0014 were Accepted, the same failure the round-53 README fix removed.
`PERFORMANCE_BUDGET.md` is struck from the absent list (it landed at P6 and sat in the gaps list
for three days after). The PACKAGING and `release.yml` deferrals now cite **OQ-007** instead of a
P6 that has closed. The user/legal/DAW-matrix items state that their P6 targets **passed unmet**
and what each now waits on, instead of pointing at a phase that no longer exists to close them —
no replacement dates invented (C7).
(2) **One repair crossed into a policy, following the TESTING_POLICY precedent.**
`REALTIME_AUDIO_POLICY.md` §Current compliance still read "TODO (no code yet)" three phases after
`REALTIME_SAFETY_AUDIT.md` — the deliverable that very TODO scheduled — landed at P2. It now
points at the audit and states plainly what the audit covers: its audited revision is the P2
commit, the P3–P6 audio-thread additions entered through `THREADING_POLICY.md`'s permitted-path
table and the round-41/42 TSAN passes, and the document's re-baseline against the v0.1.0 tree is
a recorded gap here, not a claim there. This is the section's own scheduled flip applied late —
the policy's RULES are untouched, so no ADR is owed (the same reasoning TESTING_POLICY's 2026-08-02
flip recorded).
(3) **One gap was found to be load-bearing for the fine review**: `COMPATIBILITY_MATRIX.md` is
the document `OQ-011` and HANDOVER's Pending Tasks point the DAW-matrix audition at, so the
audition cannot record results until it exists — stated in the gaps list as owed *before* that
audition rather than eventually.
Previous: **the post-v0.1.0 C++23 canary landing (2026-08-05)** — the first change
after PR #6 merged, found by walking `DEVELOPMENT_BRIEF.md` against the tree:
(1) **An Accepted-ADR mandate had no implementation behind it.** ADR-0008 mandates a non-blocking
C++23 canary on all three platforms; OQ-006 held scope/cadence with its own recommendation
("added at P2"); the P1 phase summary said "scheduled for P2" — and P2 through P6 closed without
it or any phase summary re-raising it. Landed as `.github/workflows/cxx23-canary.yml` (builds the
`AnabasisTests` target at C++23 and RUNS it, weekly + `workflow_dispatch`, non-blocking by
structure) plus the `ANABASIS_CXX_STANDARD` cache seam in `CMakeLists.txt` — needed because the
obvious `-DCMAKE_CXX_STANDARD=23` is shadowed by the project's unconditional `set()` and fails
silently, i.e. a canary wired that way would validate C++20 for ever while reporting green.
Docs synced in the same unit: OQ-006 → Resolved (⊕, adopting its own recommendation — the entry
records why no owner round-trip was needed), `CI_CD.md` (canary section + a fourth
branch-protection trap + the workflows table, whose `release.yml` row now cites OQ-007 rather
than "[P6]"), `HANDOVER.md` (Build Status row + a dated section restoring the §2.1 canary
status-reporting duty that had been silently unreported since the P1 summary), and this file's
`.github` self-coverage row.
(2) **Drift found in this file's own tail, reported before repairing (C6).** The
"Documentation-set self-coverage" and "Known coverage gaps" sections are a ~P2-era snapshot: the
architecture row still enumerates "ADR-0001…0012" (a count duplication of exactly the kind the
round-53 README fix removed, and it HAS gone stale — the index registers later ADRs), the gaps
list still names `PERFORMANCE_BUDGET.md` as absent though it landed at P6, the `.github` row
attributed `release.yml`'s absence to P6 rather than to OQ-007's resolution, and the user/legal
rows still read "Deferred to P6" though P6 has closed. Repair queued as its own change so this
commit stays the canary's; the rows corrected here are only those this change itself touches.
Previous: **review round 64 (2026-08-05)** — two follow-ups to round 63, one
robustness and one documentation:
(1) **A read that was total only because of the line above it.** Round 63 put the `iid::uiScale`
ladder read rule in `replaceFrom`, reading the property with no stated default. That is safe today —
`setDefaults()` runs two lines earlier and always writes the field — but it is the "correct because
of what the previous statement did" shape this file keeps removing, and here the failure mode is
QUIET rather than loud: an absent property reads as `var()`, `var()` converts to 0, and 0's nearest
ladder step is **80**. A missing field would silently become the SMALLEST legal scale instead of the
default one, which looks like a deliberate setting rather than a fault. The read now names
`ui_scale::defaultPercent`, and that constant replaced a literal `100` at both sites so the fallback
and `setDefaults()` cannot come to disagree, with a `static_assert` that the default is itself a
ladder step (otherwise the default would need normalising, which is the shape the rule exists to
remove).
The test states the §4.4 rule directly rather than the implementation: a session whose
`ANABASIS_INTERNAL` child OMITS the field must load at the default step. That is reachable now — a
hand-edited session, or one written before the field existed — so it is a real read-rule check, not
a guard against a hypothetical refactor. **Two-stage mutation measured which line does the work**,
because with both present the test cannot tell them apart: removing `setDefaults()`'s write leaves
the check passing (the fallback carries it, and the collateral failures are the byte-identical
round-trip checks, correctly, since a field left the schema); removing the fallback as well fails it
at exactly the 80 % predicted.
(2) **A comment named one consequence of a change and not the other.**
`SpectrumView::hitTest` declines every region but the dismiss ×, and the comment recorded that the
tooltip therefore narrows to the × — but not that clicks over the trace now REACH WHATEVER IS
BENEATH. Today that is the editor, which installs nothing there, so nothing happens; the point is
that it is a live routing decision rather than a void, and the next person to put an affordance
under the spectrum's footprint needs to know it becomes reachable through the overlay while the
overlay is showing. Both consequences are now stated as the same trade seen from opposite ends
(wanting the tooltip back means intercepting everywhere again, i.e. re-accepting the swallow), with
the resolution named: widen the hit-area, do not revert. No behaviour change.
Previous: **review round 63 (2026-08-05)** — one state-ownership move, one
truthfulness guard, one invariant written down:
(1) **A display poll had become a state writer.** `normalisedUiScale()` wrote `iid::uiScale` back to
`InternalState` when the persisted percent was not a legal ladder step, and it is reached from
`refreshInternalSettingsBoxes` — the 24 Hz settings re-seed. So the editor's display timer was an
opposing writer to `InternalState::replaceFrom`, which `setStateInformation` reaches on whatever
thread the host chose (KI-003), on the second of the two polls round 51 had just cleaned of
`ValueTree` access. Narrow (it converged after one tick per illegal value) but the wrong direction.
The correction moved to `replaceFrom`: that is where a value the schema cannot represent ENTERS, and
where every other field's §4.4 read rule already is — the overlay drops unknown properties, and
`syncAtomics` clamps the four mirrors. `iid::uiScale` was the one field clamped on READ and never
corrected in the tree, which is why `getStateInformation` re-serialised an illegal percent for ever.
**The enabling move, and the reason it is not a refactor for its own sake:** the ladder lived in
`PluginEditor.cpp`, so normalising in the state layer would have meant a second copy of it — the
exact duplication round 50 removed. `steps`/`numSteps`/`nearestIndex` therefore moved to
`InternalState.h`, beside the identifier whose legal values they define ("what may this property
hold?" is a state question), and the editor aliases them. `normalisedUiScale()` is now a pure read.
It still CLAMPS, deliberately: the tree can hold an illegal percent in the window before
`replaceFrom` runs, and a reader that returned it would put the rendered transform and the displayed
combo step back out of agreement — the defect the single reading exists to prevent.
The test followed the ownership rather than being patched around it: the convergence checks now
drive a session LOAD, and a new check asserts the poll writes nothing (an illegal percent written
straight into the live tree must survive a display refresh untouched while still being clamped on
read). Two mutants — dropping the adoption-time normalisation, and reinstating the poll write — each
fail their own check.
(2) **The preset-source hint could describe a preset that never loaded.** *(Superseded 2026-08-08
by ADR-0022 — `rememberPresetSource` and its members were deleted with the hint pattern; the
wrapper-held identity records only successful applies by construction. **The ungated-RING half of
this record survives the deletion as a requirement**, and it is why `stepPreset` now walks on
until an entry actually loads: the identity moves only on a successful apply, so a single-shot
step would re-offer a corrupt file for ever — the trap this paragraph's "advancing its hint" was
avoiding. Pinned since 2026-08-08 by `testTheRingWalksPastAnUnreadablePreset`, so the "not tested"
sentence below no longer applies to the ring (the two menu/chooser paths it also names are still
untested, for the reason given). The record is left standing rather than rewritten.)*
`showPresetMenu` and
`showLoadPreset` called `rememberPresetSource` regardless of `applyPresetFile`'s result, so a corrupt
or foreign file (a documented no-op — `parsePresetFile` refuses a foreign root) left the editor
believing it was the active source while the processor had not moved. Both gate on the return value
now. The ‹ › RING is deliberately left ungated, and that is a decision rather than an oversight: it
walks a list it just enumerated, and advancing its hint past an unreadable entry is what stops the
arrows stalling on that entry for ever. Not tested — both paths run inside async menu/chooser
callbacks that a headless suite has no message loop to deliver.
(3) **The factory defaults pass writes the default unsnapped, and that is now stated.** Overrides go
through `snapToLegalValue`; defaults do not. The asymmetry is the invariant: a registered default
must ALREADY be legal — it is the value the parameter reports before anything writes it, so an
off-step default would mean the plugin starts at a position the preset system considers unreachable
— whereas an override is hand-written table data free to be an approximate intent. Snapping the
default here would paper over a registration bug that `testRegistrySnapshot` is the place to catch.
No behaviour change.
Previous: **review round 62 (2026-08-05)** — two maintainability items, neither of
which changes runtime behaviour:
(1) **The visualisers' lifetime reasoning was inconsistent with the editor's.** `SpectrumView`,
`GrHistoryView` and `LoudnessMeterView` each declare their `abgui::FrameClock` BEFORE the members
their tick callbacks read — `SpectrumView`'s scratch buffers and `shown*` counters,
`GrHistoryView`'s `shownHead`, `LoudnessMeterView`'s whole `shown*` snapshot — and each used
`~View() = default`, so reverse-order destruction freed that state while the vblank attachment was
still armed over it. Nothing is reachable today: a message-thread destructor cannot interleave with
a message-thread vblank callback. But that is the SAME argument
`~AnabasisAudioProcessorEditor` explicitly refuses for its own `animVBlank` (it move-assigns an
empty attachment FIRST, with a comment saying why), so the GUI set was giving two different answers
to one question. Each destructor now calls `clock.stop()`, which clears the attachment and the
callback and is idempotent. Chosen over reordering the members deliberately: a reorder would fix it
invisibly and could be undone by the next person to add a field, whereas a destructor states the
guarantee where a reader looks for it. `CurveView` needs nothing — it has no `FrameClock`, being
driven by the editor timer.
(2) **The benchmark's flag set was described as the shipped plugin's.** `PERFORMANCE_BUDGET.md` read
"Release with the shipped flag set". Verified against `CMakeLists.txt`: `AnabasisBench` links
`AnabasisHardening` + `juce_recommended_config_flags` + `juce_recommended_warning_flags` — the same
set the two test apps use — while the `Anabasis` plugin target links one more,
`juce_recommended_lto_flags`. `AnabasisHardening` also adds Release debug info (`-g` on GCC/Clang,
`/Zi` + `/DEBUG` on MSVC), which is binary hygiene rather than a codegen change. A
build-configuration note now records all of that, plus two things a reader needs: why the difference
is NOT closed by changing the target (that would change the measured results rather than the
documentation, and the numbers would need re-measuring on a recorded machine — C2), and how much it
is likely to matter (the whole DSP is header-only and reaches `bench.cpp` through
`AnabasisEngine.h`, so it is already instantiated in one translation unit and most of what LTO buys
is cross-TU inlining the optimiser can do here anyway). The residual gap is stated as UNMEASURED
rather than argued away. `TEST_REPORT.md`'s performance summary points at that note instead of
repeating it, keeping one authority. No methodology, build configuration or measured value changed.
Previous: **review round 61 (2026-08-05)** — a preset source-tracking regression:
*(Superseded 2026-08-08 by ADR-0022 — the editor-local hint this round patched was replaced
outright by the wrapper-held preset identity, which `savePresetFile` itself now sets; the record
is left standing rather than rewritten.)*
**Saving a preset did not record itself as the preset now in use.** `stepPreset` resolves "where am
I in the list?" from a remembered source first, and only falls back to searching by display name
when that hint no longer describes what the processor shows. The fallback exists because names are
NOT unique across the factory table and the user files — a user preset saved as "EDM Club" resolved
to the factory entry, which is the very failure the remembered source was introduced to remove.
Every route that changes the live preset sets the hint: the menu, the ring itself, the load chooser.
The SAVE button did not. So: apply factory "EDM Club", Save Preset as "EDM Club" — the hint still
said factory index 2 and the unchanged name CONFIRMED it, so ‹ › walked the factory ring from an
entry the user had just replaced, applying the wrong preset and silently changing the sound.
`saveOkButton.onClick` now calls `rememberPresetSource (file)` on a successful save — one line, at
the point the flow already knows both that the save succeeded and which file it wrote. No new name
matching, and no other preset path touched.
**Why the editor and not `AnabasisAudioProcessor::savePresetFile`:** the remembered source is
EDITOR state — this window's idea of where it sits in a list it enumerates itself — and the wrapper
deliberately holds no view of it, because a second editor or a save through another route has its
own answer. The wrapper owns the live name and the dirty datum, which it already set correctly; the
regression was never in what it stored.
The test drives the real controls (the save overlay's name field and Save button, then the ‹ ›
buttons) and distinguishes the two same-named presets by CONTENT rather than by name, which is the
only thing that can tell them apart: the saved preset carries a value for a parameter the factory
table does not name, so a factory apply parks it at its default. Next-then-previous is exactly
reversible through the ring, so a correctly resolved position returns to the saved preset and a
stale hint returns to the factory one. It is the first test to write into the REAL user preset
directory — the save handler and `stepPreset` both resolve that path themselves and neither takes
an injected one — so it backs up and restores any file it would displace, since the scenario
requires that exact filename.
Previous: **review round 60 (2026-08-05)** — a lost-functionality regression restored:
**The About overlay opened blank.** `Backdrop::aboutText` had exactly ONE reader — the
dismiss-anywhere branch in `Backdrop::mouseDown` — so the flag named a panel whose copy nothing
painted: clicking the wordmark produced an empty glass rectangle with only the hyperlink in it, and
a user or tester had no way to see which version or build they were running. The compile definitions
were orphaned with it: `ANABASIS_VERSION_STRING` and `ANABASIS_BUILD_NUMBER` are set by
`CMakeLists.txt` for the plugin AND the state-test target, and `CI_CD.md` still describes the run
number as "the About-box build number", but a repo-wide grep found no consumer in `src/` at all.
`resized()` had even kept the space — the panel is 400×232 with the link at `withTrimmedTop (176)`,
so the top 176 px were reserved for copy that was never drawn.
`Backdrop::paint` now renders it, in the panel rather than in a new child component, because that is
where every other overlay's content already lives. The strings are ones this product already owns:
the wordmark and the `MASTERING MAXIMIZER` subtitle the top bar paints (at the top bar's own accent
tracking, so the panel reads as that wordmark enlarged rather than a second one), `COMPANY_NAME`
from `CMakeLists.txt`, and the two build definitions. The `#ifndef` fallbacks are for a build that
bypasses CMake and are deliberately not a release number, so a stale one cannot masquerade as
shipped.
**No prose description, and that is where this departs from the sibling's About.** Anamorph's panel
carries a product sentence because its owner supplied one; free prose here is owner-supplied wording
(C8), which `PluginEditor.cpp`'s own header says is not invented in code. The panel identifies the
build completely without it, and a description is a one-line addition when the copy lands.
`testTheAboutPanelShowsTheBuildItIsRunning` opens the panel through the REAL path — the wordmark's
ghost hit-area button, found by the component id the LookAndFeel already keys on — and asserts the
copy area shows HORIZONTAL variation. That formulation is the point: an empty panel is a vertical
glass gradient, so every row is near-constant across x, and any rendered copy breaks that. The check
therefore needs no pixel count or glyph match tuned to one machine's font, which is what would have
made an editor-rendering test a platform flake. A mutant restoring the blank paint fails it.
Previous: **review round 59 (2026-08-05)** — one UI interaction fix:
**`SpectrumView` consumed clicks it had no use for.** It left `setInterceptsMouseClicks` at JUCE's
default, so it hit-tested true across its whole area while acting only on clicks in the top-right
26×24 × region. Every other click inside the overlay was swallowed with no affordance and no effect
— the one region of the editor that took a click and did nothing. The fix is `hitTest`, which is
what JUCE provides for a PARTLY interactive component: `GrHistoryView` and `CurveView` opt out
wholesale with `setInterceptsMouseClicks (false, false)` and this view cannot, because it owns the
dismiss ×; `LoudnessMeterView` stays intercepting everywhere because its whole surface IS the
affordance (click = meter reset). So this one opts out per-pixel, and now sits correctly between the
two existing models rather than outside both.
The hit-area moved into a single `dismissHitArea()` that `hitTest` and `mouseDown` share. That is
part of the fix rather than tidying beside it: two separately computed rectangles could accept a
click and then ignore it, which is the same swallowing reintroduced one pixel at a time. The
rectangle matches exactly the set of in-bounds points the old predicate (`x > getWidth() - 26 &&
y < 24`) accepted, so the touch target — deliberately larger than the 18×18 glyph `paint` draws — is
unchanged, and `paint` is untouched.
**One visible consequence, recorded because it is inseparable from the fix rather than added to
it:** hit-testing is also what makes a component "under the mouse", so the `setTooltip ("Spectrum")`
identifier now appears over the × only, not over the whole trace. There is no way to stop claiming
clicks in a region while still claiming the pointer there. That wording carries an explicit C8
owner-supplied TODO, so the narrower scoping is a brand-pass call if it is wanted back.
`testTheSpectrumOverlayOnlyClaimsItsDismissCorner` pins both halves — a click over the trace is not
claimed, and the × still dismisses through the real `MouseEvent` path — and a mutant restoring
JUCE's hit-test-everything default fails the pass-through checks.
Previous: **review round 58 (2026-08-05)** — two accuracy fixes, neither of which
changes behaviour:
(1) **An invariant comment described the wrong execution order.** The round-52 note on `injectTrims`
justified its per-field finite check by saying `sanitiseState()` catches a poisoned trim "a block
LATER", on the stated premise that the injection sites "run after that block's sanitise". The
opposite is true: both sites — the unprimed direct adopt and the §2.8 duck's silent bottom — execute
inside `AnabasisEngine::process` BEFORE its `adaptiveEngine.sanitiseState()` call, and the first
`currentTrims()` read comes after it, in the same call. The audio path therefore has no recovery
LATENCY at all; a poisoned vector is repaired before any block consumes it. Correcting the prose
also surfaced the exposure the old version never named, which is the real reason the boundary check
is necessary: `publishTrims (true)` writes the four published atomics and the retained set AT
INJECTION TIME, `sanitiseState` repairs only the plain struct and never re-publishes, and nothing
else will — an ADR-0014 restore is staged only for a freeze-ON surface and `finishBlock` holds while
frozen. A NaN would sit in the wrapper-visible atomics indefinitely and
`engineFrozenTrimsIfLive()` would serialise it into the saved session, outliving the block, the
session and the file. No per-block sanitiser could close that. The DSP behaviour, ordering and
validation logic are untouched; only the justification changed, from one that was wrong and weak to
one that is right and stronger.
(2) **`AnabasisLookAndFeel` regained its JUCE safety declaration.** Every other class in the new GUI
set carries `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` — `CurveView`, `GrHistoryView`,
`SpectrumView`, `LoudnessMeterView`, `FrameClock`, the editor — and this one lost it when the class
was rewritten for Anabasis, making it the single member that was silently copyable and unleak-checked.
Restored. Verified by building the state suite in DEBUG, where the leak detector is compiled in
(the Release gate never exercises it): 365 checks green, so nothing copies or leaks one. Two JUCE
assertions fire in that Debug run (`juce_AudioProcessorEditor.cpp:199`,
`juce_NormalisableRange.h:265`); both were confirmed present before this round's changes as well, so
they are pre-existing and unrelated — recorded here because a Debug run is not part of the routine
gate and the next person to make one should not read them as new.
Previous: **review round 57 (2026-08-05)** — the third preset walk converted to the
shared traversal; no behaviour changed:
**`PresetManager::applyFactoryPreset`'s defaults pass still iterated `apvts.state`'s PARAM
children** while `savePreset` and `presetShapeFromLive` had shared `forEachPresetParameter` over
`getParameters()` since round 52. The argument that unified those two applies here with more force,
because the consequence is different in kind: for the writer and the marker a divergence is a
cosmetic disagreement about the "edited" mark, but an override table is "defaults + intents", so a
parameter this pass skips keeps the value the PREVIOUS preset left it at — the blend-two-presets
failure the pass exists to prevent, silent and worse the further apart the two presets are.
"One tree child per parameter" is a fact about JUCE, not an invariant of this code.
The shared walk now yields `(id, RangedAudioParameter&)` rather than `(id, double)`. That is what
made the third caller able to use it at all: `savePreset` and the projection ask `presetValueOf`,
while the factory apply needs the parameter's DEFAULT and then writes to it, so handing over a value
and hiding the parameter was the reason this loop had its own collection. `presetValueOf` stays the
single value rule, still one copy, now called by the two callers that want it.
Everything observable is unchanged, and deliberately so: write ORDER (the walk's sorted-by-id order
IS the tree order the loop had — round 54 measured it and `written == fromTree` pins it), values,
exclusions, the ceiling-lock skip and host-notification behaviour. The ceiling lock stays at the
CALL SITE rather than moving into the shared walk, because it is an apply-side rule: a locked
ceiling is never written by a preset (DESIGN §4.2) but is still saved and still compared by the
dirty marker, so it is not a member of the preset parameter set.
It also retires a caveat instead of restating it. The old loop wrote the APVTS tree (through
`setValueNotifyingHost`) while iterating it, safe only because a property write neither adds nor
removes children — with a note to collect the ids first if a listener ever changed that. The shared
walk reads the processor's parameter list and touches no `ValueTree`, so there is no iterator left
to invalidate.
The parity test gained the invariant in the form that needs no second copy of the table logic:
IDEMPOTENCE AGAINST ARBITRARY PRIOR STATE — snapshot what a preset lands, park every non-excluded
parameter at the far end of its range, re-apply the SAME preset, require every one of them back
where it was. A parameter the walk misses keeps its parked value and is named in the failure
message. One mutant (a parameter the pass skips) fails it, and `factory:`'s existing
returns-to-default check as well.
Previous: **review round 56 (2026-08-05)** — one implicit contract made explicit, one
latent aliasing hazard removed; no behaviour changed by either:
(1) **The ADR-0014 restore's generation bump was load-bearing and unstated.** `injectTrims` calls
`publishTrims(true)`, which writes the RETAINED set and advances `retTrimSeq` exactly as an audible
`finishBlock` does. That is what makes `engineFrozenTrimsIfLive()` answer correctly: after
`adoptFrozenMirror()` re-bases `slotFrozenBase` to the current generation — "nothing the engine holds
belongs to this slot yet" — the staged restore's own injection is the only thing that can carry the
generation past that base, because with Freeze ON `finishBlock` publishes nothing. A future change
that made the restore publish without counting (to keep the counter meaning "measured", say) would
leave a freeze-ON slot restored from disk withholding its latch from every save until the next
AUDIBLE block — never, on a stopped transport — so the slot would go on re-serialising the mirror
instead of what the engine is applying. The argument is now stated at the injection site and in
`THREAD_MODEL`'s retained-trim row, framed as the property a future maintainer must preserve rather
than as the line they must not touch.
**And it turned out to be untested where it matters.** A `publishTrims(false)` mutant left the STATE
suite fully green; only the DSP suite's `hostileTrims` caught it, and incidentally, because that test
reads `retainedTrim*()` for an unrelated reason. `testFrozenTrimRestore` case (1) — the unprimed
session load — now asserts the generation advances across the landing block and that
`hasRetainedTrims()` follows. The mutant fails both. The value checks around it could never have
seen this: the mirror and the engine hold the SAME vector at that moment, so only the ownership
answer differs.
(2) **Preset baselines were aliased across slots and history entries.** `copySlotToOther()` did
`storedPresetBaseline = presetBaseline;` and `undo()`/`redo()` did `presetBaseline = prev.baseline;`
— a `juce::ValueTree` assignment shares the refcounted node, so two independent-looking owners
became one tree. Correct today because a baseline is only ever REPLACED wholesale (by
`presetShapeFromLive()` or a history entry) and never edited in place, and `pushUndoStep` already
takes `createCopy()`. Three assignments gained `createCopy()`; an invalid tree copies to an invalid
tree, so `presetDirty()`'s validity guard is untouched, and the cost is a ~46-node clone on a user
action. Deliberately NOT tested: the alias is unobservable through the public API — nothing edits a
baseline in place and `presetBaseline` is private — so a test would need an accessor added purely for
it, which is the abstraction this change is meant to avoid. The existing preset/undo/A-B checks
passing unchanged is the evidence that behaviour is preserved.
Previous: **review round 55 (2026-08-05)** — the component-ID half of the LookAndFeel
migration audit, decided per id rather than as one verdict:
(1) **Four `getComponentID()` branches had no arming site.** Round 53 cleared the PROPERTY side
(`"glow"` wired, `"unit"` removed); the ID side still discriminated on `"icon"`, `"apply"`,
`"metersicon"` and `"vtoggle"`, and `PluginEditor` set only `ghost`, `bypass`, `presetnav`,
`presetname`. The sibling product settles each case, because it is where every one of these ids is
armed: `apply` styles its "Apply Gain" button beside the auto-gain-match readout, `metersicon` its
show/hide LEVEL METERS toggle, and `vtoggle` its Mono / Swap / M-S / polarity-L / polarity-R row.
Anabasis has no auto-gain match, no meters show/hide (the §6.3 strip is always present) and no
stereo-field toggles — those belong to a product with a Widen stage. All three are removed, drawing
code and font row alike.
(2) **`"icon"` was the opposite case and is now WIRED.** Its own header names its owner — "Undo/Redo
glyphs: render them larger AND rotated 180 degrees" — and this product has undo/redo, added at P6 in
the same top bar. Unarmed, a 30 px glyph button rendered a ~13 px character through the generic
text path: not a stylistic difference but the absence of the sizing the branch exists to provide.
The discriminator is between two kinds of button in THIS top bar (the glyph pair against
`Copy`/`Settings`/`Save`), which is the same test that kept `"glow"` last round — a design statement
about Anabasis, not a description of a control it lacks.
(3) **One thing deliberately NOT decided here.** The treatment also rotates the glyph 180°,
specified against the sibling's U+21BA/U+21BB OPEN CIRCLE arrows where a half-turn of a near-circular
shape is a subtle comfort tweak; these are U+21B6/U+21B7 SEMICIRCLE arrows, so the same half-turn
visibly moves the arc from top to bottom. Direction is not at risk — a rotation preserves the curl,
so neither glyph can come to read as the other — but WHICH glyph pair this product ships is a
Level-5 brand question the checklist still holds open, and answering it is not a cleanup pass's job.
Recorded at the arming site so the brand pass finds it.
Previous: **review round 54 (2026-08-04)** — the single-authority sweep finished, and
one reset invariant raised to the standard the rest of the tree already holds:
(1) **Four documents still carried an ADR count or roster.** Round 53 fixed the README; the same
duplication survived in `CLAUDE.md` ("the **fourteen** Accepted ADRs"), `REPOSITORY_MAP.md` (twice —
"the eleven ADRs it spawned", and a file-tree line reading "ADR_INDEX.md + ADR-0001…0012, 0001–0011
Accepted 2026-07-31") and `SOURCE_OF_TRUTH.md` ("Level 3 is populated: **ADR-0001…0011 are
Accepted**"). Three of the four were already WRONG — they had gone stale through ADR-0012, 0013 and
0014 — and the fourth was one acceptance away from it. All four now name `ADR_INDEX.md` and no
number; `SOURCE_OF_TRUTH.md` names the LEVEL rather than its membership, which is the distinction
that makes it stale-proof. `CHANGELOG.md`'s "twelve presets" was deliberately left: a changelog
entry records what a released version did, and freezing that is the point of the file.
(2) **The spectrum reset was inferred, not announced.** `SpectrumView::tick` decided a ring had been
rewound by `writeCount()` going backwards, which is true only while the observed count is still
below the one the last tick stored. Let the producer republish past that value between two ticks —
one suspended message thread, one debugger stop, one host batching redraws — and the reset is missed
OUTRIGHT and permanently, because every count from then on is larger again. The symptom is silent
and is exactly what the rewind exists to prevent: the previous lifecycle's EMA drawn against the new
sample rate's bin mapping. Ordering two counters cannot express "a reset happened". `GrHistoryBuffer`
had already answered the same question with an epoch, so the rings gained a generation counter,
bumped release-after the index rewind, and `SpectrumView` samples it on both sides of its analysis
batch — the pre-batch sample catches a reset since the last tick, the post-batch one catches a reset
that landed mid-batch, whose frames may straddle two configurations. A plain generation, NOT the GR
buffer's odd/even seqlock: that class clears 4096 entries a reader can observe half-done, while
`ScopeBuffer::reset()` writes one atomic and touches no sample, so the extra fence pair would buy
nothing. The test now includes the case the counter predicate could not see, and it fails against a
mutant restoring that predicate.
Previous: **review round 53 (2026-08-04)** — a cleanup pass: two single-authority
repairs in the README, two unreachable styling paths resolved in opposite directions, two unported
LookAndFeel subclasses removed, and one deliberately-retained atomic given the reasoning it needed:
(1) **The front page told contributors to validate at the OLD strictness.** Rounds 35/40/43 made
`ANABASIS_PLUGINVAL_STRICTNESS` in `.github/workflows/build.yml` the single authority and reworked
`TESTING_POLICY` and `CI_CD` to quote no literal — `CI_CD` reads it out with `sed` and documents the
failure against itself ("the literal used to be `5` here and stayed 5 through two raises, under a
comment telling the reader it was current"). The README's quick-start block was the one site the
sweep missed, so it still passed `5` to both `run-pluginval.sh` invocations under a comment naming
the ladder — the exact failure, left standing in the first document a reader opens. It now runs the
same `sed` extraction, and the validation-gate table points at the workflow instead of spelling
`5 → 8 → 10` out again.
(2) **The README contradicted itself about the binding decision set.** The status section (updated
last round) said fourteen ADRs; the Documentation section two screens down still read "twelve
Accepted ADRs: ADR-0001…0011 plus ADR-0012", so a contributor following `CLAUDE.md`'s instruction to
read the registry before writing code could miss ADR-0013 and ADR-0014 entirely. The enumeration is
gone and BOTH numerals with it: `ADR_INDEX.md` is the registry, and the README carries no count for
the same reason it carries no strictness number.
(3) **The Save-Preset name field's focus glow was WIRED, not deleted** — the opposite call to
`resetSweep`, and the difference is which side was missing. `fillTextEditorBackground` and
`drawTextEditorOutline` both key on a `"glow"` component property and fall through to the JUCE
default without it, and nothing set it, so the accent-lit rounded border the family design language
specifies (#11) was unreachable. What makes this a wiring job rather than a removal is the fallback
branch's own comment — "value boxes unchanged": the property discriminates between two kinds of text
field IN THIS editor (the name box and the ValueBox edit fields), which is a design statement about
Anabasis, not migrated state describing a control that does not exist here. The owner sets it at
construction; `testTheSavePresetNameFieldIsTaggedForItsFocusGlow` pins the arming side.
(4) **`rawEditText`'s balance branch was REMOVED**, and the same test decided it. It read a `"unit"`
slider property nothing set, and took a dedicated path when it equalled `"bal"` to decode an
"L 25" / "C" / "R 30" display into a signed number. That is the sibling product's stereo Balance
format: Anabasis registers `colourBalance` with `[](float v, int) { return juce::String (v, 2); }`,
a plain signed decimal the generic path already passes through untouched. Wiring it would have
taught the editor to parse a display string this product cannot produce. The `"unit"` read went with
it — it had no other reader.
(5) **`CompactComboLookAndFeel` and `SimpleComboLookAndFeel` removed.** Their own comments named the
controls they were "applied only to" — the compact Input Channel / M/S Solo combos, and the two
Simple-mode Widen combos (algorithm + Style/Focus). Anabasis has no Widen stage, no input-channel
selector and no M/S solo; nothing in `src/gui` instantiated either class, and the editor holds one
`AnabasisLookAndFeel` for every combo. Left standing they invited a reader to assume a size variant
was wired and to style a new control by reusing it.
(6) **`hasPublishedTrims()` / `pubTrimEver` KEPT, with the reservation made explicit.** The review is
right that the production reader is gone — the ADR-0014 save capture moved to `hasRetainedTrims()`
at round 41 — and "an atomic only a test reads" is exactly the shape items 4 and 5 above just
removed, so the difference had to be written down rather than inferred. Two things are load-bearing:
it is the published set's VALIDITY marker, the question a §6.3 trim readout must ask and cannot
answer from the values (all four read 0 both when nothing was measured and when the measurement was
"no trim"); and it is the ONLY observation of the published/retained SPLIT, which is the whole of
KI-006's two halves — delete it and `testPreparedStateAndSlotOwnership`'s "the APPLIED vector did not
survive re-initialisation / the RETAINED one did" pair collapses to one assertion, after which
nothing stops a future simplification merging the two sets. A contradiction was fixed alongside: the
private section asserted "the P5 UI reads publishedTrim*() like every other display value", which
the accessor's own comment two screens up correctly denied.
Previous: **review round 52 (2026-08-04)** — three fixes and one investigation that
confirmed the behaviour under review and pinned it instead of changing it:
(1) **A state invariant was conditional on a branch that does not always run.** `applyUiScale()`
owned the write-back that converges an illegal persisted `iid::uiScale` onto a ladder step, and
`refreshInternalSettingsBoxes` reached it only when `nearestScaleIndex(stored)` differed from the
combo's current selection. A session carrying 92 while the box already showed 90 % therefore
clamped on every read and healed on none: the transform and the displayed percent still agreed, so
nothing looked wrong, and the only observable was that `getStateInformation` re-serialised 92 for
ever. The round-48 claim that the value "converges where the scale is applied" was stronger than
the code by exactly the write the code skipped. `normalisedUiScale()` now owns the rule and every
reader of the property goes through it, so convergence is a property of reading the value.
Deliberately NOT "call `applyUiScale()` unconditionally from the re-seed": that path also calls
`glContext.triggerRepaint()`, which is not a no-op, and the re-seed is the 24 Hz display poll —
separating the state rule from the application of it is what lets the poll enforce the first without
paying for the second.
(2) **A boundary clamp did not hold against the input it was written for.**
`AdaptiveEngine::injectTrims` bounds a restored frozen-trim vector with `juce::jlimit`, which is
`v < lo ? lo : (hi < v ? hi : v)` — and every comparison against a NaN is false, so a non-finite
value took neither branch and entered the trim state unchanged. This is the one externally authored
thing that reaches the adaptive state (four properties read out of a session or slot tree), so
ADR-0014's "clamped at the boundary … holds against hostile state" was the claim under test, and it
did not hold: past the boundary the value is published — the wrapper can serialise it back out —
and reaches `std::pow` in the release mapping. `sanitiseState()` does catch it, but a block later
and only after it had been the live vector for the remainder of the block it was injected in, which
is a bounded recovery rather than the stated contract. Each field is finite-checked before it is
clamped, PER FIELD rather than whole-struct — the opposite of `sanitiseState`'s choice and
deliberately so, since there the four members descend from one poisoned pipeline while here they are
four independent document properties, and one corrupt attribute is no reason to discard three sound
ones. The recovery path is untouched; it simply has nothing to recover from on this route.
(3) **Two derivations of one answer walked two collections.** Round 51 gave the preset writer and
the dirty-marker projection the same exclusion predicate and the same value rule, but left them
iterating `apvts.state`'s PARAM children and `getParameters()` respectively. Every value agreed,
because APVTS creates one tree child per parameter — a fact about JUCE, not an invariant of this
code, and a parameter registered without a node (or the reverse) would have put content in the file
the marker could not see. `PresetManager::forEachPresetParameter` is the single traversal now. It
visits in ID ORDER, which is a serialisation decision rather than tidiness: the tree order the
writer inherited was JUCE's id-keyed one, so that is the order every `.anabasis` file on disk was
written in, while `getParameters()` is registration order — moving the writer onto it unsorted would
have re-ordered every element of every file on its next save for no semantic gain (`applyPreset`
looks each id up and has never depended on position) and would churn again on any future layout
reshuffle. The test checks both directions and the order.
(4) **Investigated and deliberately NOT changed: a preset apply keeps the frozen latch.** The
review asked whether runtime-only frozen adaptation state should survive a factory preset change,
reading the cleared `BASELINE` beside the kept `FROZEN_TRIMS` as an inconsistency. It is not one,
and the two differ for a stateable reason: `BASELINE` is derived from the parameter surface the
apply replaces, so it describes nothing afterwards; the trims are derived from the AUDIO, `freeze`
is preset-EXCLUDED so the apply never changes whether the slot is frozen, and the engine's latch is
untouched and still exactly what the DSP is applying — so the slot's record of it stays true.
Clearing `liveFrozenTrims` would not clear that latch either; it is only the fallback answer for
ADR-0014's staged-but-not-yet-applied window, so a save inside that window would report "no latch"
while a vector was staged and about to land at the next duck bottom. That loses the vector rather
than tidying it. Recorded in ADR-0014 and pinned by
`testAPresetApplyKeepsTheFrozenLatchItDidNotChange`, so the question does not have to be re-derived.
Previous: **review round 51 (2026-08-04)** — five fixes, and two investigations that
ended in a documentation correction rather than a redesign:
(1) **A comment promised a compile-time guarantee the language does not give.** The badge table in
`PluginEditor::paint` was declared `const juce::Slider* badged[managed_params::kCount]` beside a
comment saying a mismatch was a build error. Aggregate initialisation with FEWER initialisers than
the bound is legal C++ and value-initialises the remainder, so raising `kCount` to 10 without adding
a tenth knob compiled cleanly and then dereferenced a null `juce::Slider*` in that very loop — a
crash at paint time, the opposite of what the comment promised. The array deduces its bound and
`static_assert`s against `kCount`. The same audit found the count itself unchecked:
`managed_params::kCount` is hand-written while `ids` deduces its own bound, so the two could
disagree in either direction (a tenth id silently under-scanned every `kCount` loop; a bumped count
ran one past the end). A `static_assert` at the declaration ties them.
(2) **Widget state was written on non-toggle transitions.** The meter-target checkboxes wrote
`iid::meterTargets` from `onStateChange`, which `juce::Button::sendStateMessage()` fires for every
button-state change — normal→over, over→down — not only for a logical toggle. Hovering a checkbox
therefore stamped that widget's current bit into the live tree, and inside the ~42 ms before the
next re-seed that could overwrite a mask a preset or session load had just installed. The callback
computes the wanted mask and writes only when it differs from the stored one, so hover and press are
no-ops. The load direction was never the problem and is untouched: the re-seed uses
`juce::dontSendNotification`.
(3) **A paint cache labelled its geometry with a fingerprint it was not built from.** `CurveView`
compared and stamped `pathFingerprint` against `shownFingerprint`, a member only `refresh()`
advances — and the editor ticks `refresh()` only while Advanced is showing. Any repaint that no
refresh preceded (a host expose, an overlay dismissal, a `resized()`) built the path from the
CURRENT parameter values and stamped it with the previous tick's label; a subsequent repaint in the
same state then served that geometry as if it described the new one. It self-corrected within a
tick, which is why the previous round recorded it as "no stale curve persists beyond one refresh"
rather than fixing it — but an invariant that only holds because a timer keeps repairing it is not
an invariant. `readInputs()` now reads the mode's parameters and accumulates their fingerprint in a
single pass, so a value cannot enter the curve without entering the hash; `paint()` takes its own
`readInputs()`, compares and stamps with that, and builds from those very values. Exact by
construction, with or without a preceding refresh.
(4) **The dirty marker measured a state no preset can hold.** `presetDirty()` compared SLOT trees,
which carry the full parameter surface (view tier included), the exact-`raw` attribute, `BASELINE`
and `FROZEN_TRIMS`; a `.anabasis` file stores none of those. Resizing the window, switching
Simple↔Advanced, toggling Freeze, or a mid-step raw move on a discrete parameter therefore lit
"edited" on a preset whose file would have been byte-identical, and no re-save could honestly clear
it. `presetShapeFromLive()` projects the live state onto exactly `PresetManager::savePreset`'s
content — non-excluded parameters at their snapped preset values plus the `DETACH_MASK` — through a
`presetValueOf` the writer and the projection now share, so the two cannot drift. This closes KI-007
item 5, the spec question that entry had held open since round 28, and answers it the way the
entry's own text proposed. It also removes the KI-008 exposure recorded at round 49: the ~3 Hz poll
no longer reaches `apvts.copyState()` (the M1 half of the inversion) or any wrapper `ValueTree`,
so the message thread's only remaining M1 acquisition is the gesture-begin snapshot. The inversion
itself is unchanged and KI-008 stays open.
(5) **Two paths for the same operation left different internal state.** `applyFactoryPreset` cleared
`liveBaseline` ("defaults-based: no macro baseline survives"); `applyPresetFile` did not, although a
preset file cannot carry `BASELINE` either. A preset loaded over a state that held a baseline kept
the previous state's vector, and it travelled — into the SLOT tree that A/B swaps and
`getStateInformation` writes. The file path clears it too. Undo is unaffected: the pre-state pushed
before the bracket still carries the old baseline.
**Investigated, and NOT redesigned — with the reason recorded rather than the conclusion assumed:**
the MacroEngine teardown window, and the frozen-trim mirror boundary. For the first, the honest
statement is stronger than the one the code carried: `timerCallback` and `handleAsyncUpdate` both
run on the message thread, so a destructor running there cannot be concurrent with either, and
`stopTimer()`/`cancelPendingUpdate()` shut the window completely rather than narrowing it. The
residual is exactly the off-message-thread host KI-003 is about, and it is now checkable — a
`jassert` on the message thread, not a spin-join, which would block a teardown thread on the message
thread and deadlock whenever that thread is waiting on the caller. For the second, writer
(`adoptFrozenMirror` from `setStateInformation`) and readers (`saveSlotFromLive` via
`getStateInformation`, the A/B swap, the §7 undo push) are unchanged; what changed is that the
display poll is no longer among the readers, which narrows the window without a thread-model
decision. Closing it needs one, and that is an Architecture Review Gate item.
Previous: **review round 50 (2026-08-03)** — four fixes and one assessment that
changed nothing:
(1) **The UI-scale ladder had two representations.** `kScaleSteps` was introduced as the single
source, and the combo's item strings wrote the same seven values out again as labels. They agreed;
adding or removing a step without editing the literal list would have left the displayed label
naming a different scale from the applied transform. The labels are built from the ladder now. The
test had to be strengthened to see it: checking that a label's number round-trips to its own index
passes even with a wrong label, because the clamp maps it back onto the nearest step — so each item
is now SELECTED and the rendered transform compared against the label.
(2) **A preset file was parsed twice.** `applyPresetFile` parsed for the readability gate, discarded
the document, and `applyPreset` parsed the same file again — so a file rewritten between the two
(the ‹/› ring walks this path on every press) passed the gate and then applied different content.
`PresetManager::applyPreset` gained an overload taking the parsed document; the `File` overload
parses and delegates, so both entry points share one readability answer.
(3) **The meter-reset request announced before it published.** The audio thread could consume the
flag and complete an entire block — clearing the hold, running the engine, publishing fresh readings
— between the flag store and `publishSilentMeters()`, after which the message thread wrote silence
over readings that were already post-reset: a blank meter the audio had already restarted. Publish
first, announce second; the flag is release-stored and the block-top `exchange` acquires, because
the six meter atomics are relaxed and source order alone would not stop the consumer seeing the flag
first. **No test catches this** — the interleaving needs a concurrent audio block and the suite is
single-threaded; the swap is recorded at the site, as with the frozen-trim ownership boundary.
(4) **A factory apply wrote every overridden parameter twice.** Defaults first, intents second, so
the host was told about a value the preset never wanted and the surface passed through a state no
preset describes. The apply now computes each parameter's final value and writes once, with the
exclusion and ceiling-lock rules applied in one place instead of once per pass. `setValueNotifyingHost`
is kept — it is what keeps host, APVTS and attachments in agreement — but it is no longer called for
a value that does not move. Automation correctness is unchanged: a host that is not told about a
value that did not change still holds the right value.
(5) **The parented preset menu was assessed and left alone.** The z-order concern is unreachable:
each overlay is `setBounds (getLocalBounds())`, `setAlwaysOnTop (true)` and intercepts mouse clicks
(its `mouseDown` dismisses), so while one is showing a click on the preset name hits the backdrop,
not the button — the menu cannot be opened behind an overlay. The scaling concern does not follow
either: the menu is a child of the editor, so the transform scales menu and window together and the
logical space available is 720 px at every scale. What remains is a user with enough saved presets
to exceed that, where JUCE falls back to a scrolling menu — standard behaviour, not a defect.
**Left documented, unchanged:** KI-006, KI-007, KI-008, the frozen-trim ownership model, the
MacroEngine teardown architecture, the visualiser FrameClock criteria and the CurveView cache
strategy.
Previous: **review round 49 (2026-08-03)** — one real display defect, two duplicated
numbers removed, and three comments brought back to what the code actually does:
(1) **A re-prepare left the OLD spectrum analysis on screen, drawn against the NEW rate's bin
mapping.** Round 39 rewound the rings at `prepare` so stale frames become unreachable; that was
half of it. The reader owns its own smoothed copy, and `SpectrumView::analyse` returns immediately
when `readLatest` yields nothing — exactly the post-rewind state — so `inDb`/`outDb` kept the
previous lifecycle's EMA and went on being drawn. The rewind is observable from the reader as a
write count that went BACKWARDS, so that edge now clears the trace the ring can no longer justify.
Deliberately the edge only: the "should an idle analyser decay to the floor?" branch is the early
return above it, a listening-pass call, and untouched (KI-007 item 6).
`testARewoundSpectrumRingDropsThePreviousTrace`; two mutants — removing the clear, and clearing
unconditionally. The second needed a sharper stimulus than the first attempt: clearing and then
analysing in the same tick rebuilds instantly (the EMA's attack is a straight assignment), so
"still above the floor" could not see it; the check now observes the DECAY a spurious clear would
destroy.
(2) **The per-stage performance figures were published twice, and the second copy was the stale
one.** `TEST_REPORT.md` still carried the pre-correction values that `PERFORMANCE_BUDGET.md` had
already re-measured and labelled wrong in the unsafe direction. The second copy is gone and the
report points at the authority; the "not yet measured (do not cite): CPU/performance" line directly
above it — which had contradicted its own next section since the bench landed — is corrected too.
(3) **`CLAUDE.md` restated the pluginval strictness** that this PR's own rule confines to
`build.yml`. It is the first file every contributor and agent reads, so it is the copy most likely
to be trusted and exactly the one the rule exists to protect; it now describes the gate and names
where the number lives. `HANDOVER`'s two PRESENT-TENSE restatements go the same way; its dated
phase-history mentions stay, being narrative about when the bar moved.
(4) **Three comments corrected to what the code guarantees.** `CurveView`'s cache claimed "there is
no state in which a repaint is requested and the cache is not rebuilt" — the cache can be LABELLED
with a fingerprint it was not built from, and what actually holds is "no stale curve persists beyond
one refresh"; recorded, not fixed, since the cache strategy is deferred. The spectrum publication's
"what this ASSUMES" list gained its second assumption (it publishes before the invariant-9 self-heal
decides the chunk was contaminated — display-only, nothing non-finite escapes). And the macro
mapping's "the pass costs nine comparisons" was an understatement, because `isDetached` built a
`juce::String` per call — nine heap allocations per pass; it takes a `StringRef` now, so the claim is
true rather than the comment being wrong.
KI-008 gained an exposure paragraph: the editor's ~3 Hz dirty poll makes the message thread a
CONTINUOUS acquirer of the APVTS lock, which is what a probability estimate for that inversion
should be based on. The decision itself is untouched.
**Left documented, unchanged:** KI-006, KI-007, KI-008, the MacroEngine teardown architecture, the
PopupMenu ownership question, the visualiser FrameClock criteria and the CurveView cache strategy.
Previous: **review round 48 (2026-08-03)** — one verification that ended in "keep it",
one state-model convergence, one guard made structural:
(1) **The frozen-trim ownership boundary was VERIFIED and deliberately left as it is.**
`adoptFrozenMirror()` writes the mirror and then reads the retained generation, and the two cannot
be made atomic without a lock — the audio thread can publish in the gap (only with Freeze OFF, since
`finishBlock` stops publishing while frozen), so the boundary can be one generation off. The
direction is what matters and it is NOT symmetric. Reading AFTER, as the code does, can only make
the boundary too LATE: a publication in the gap is attributed to the OUTGOING slot and the new slot
withholds a latch for ~10 ms of audio, writing nothing wrong. Reading BEFORE would make it too
EARLY, and a publication in the gap would then satisfy `gen != base` for the INCOMING slot —
serialising a vector measured while the outgoing slot was live, which is the cross-slot leak round
42 closed. The skew is therefore deliberately biased toward silence rather than toward borrowing
another slot's latch. **Reported honestly: no test catches a swap** — the distinguishing case needs a
publication to land between two adjacent statements, and the suite is single-threaded; I ran the
swap and all 307 checks passed. The ordering is now stated at the site with the consequence of
inverting it, because a comment is the only guard available.
(2) **An illegal persisted `uiScale` never converged.** Clamping on read (round 42) fixed the
divergence between the transform and the combo but left the illegal value in `InternalState` for
ever: `getStateInformation` re-serialised it on every save, so the session never healed. It is now
normalised where the scale is APPLIED — the state model's own read-rule discipline, matching
`adoptParamsTree`'s treatment of out-of-range parameter values — and deliberately not on the 24 Hz
display poll: `applyUiScale()` runs from the constructor, a host DPI change, the user's own
selection, and the refresh only on the branch where the stored value changed. A legal value is never
altered. Two checks added; the no-write-back mutant dies. The `stored != step` test is recorded as
belt-and-braces rather than as the mechanism — `ValueTree::setProperty` already compares before
assigning, which the unconditional-write mutant demonstrated by passing.
(3) **`drainDetachBitsSoon()` bypassed the whole-tick restore guard.** `drainTick` suppresses both
halves of the drain inside a `ScopedRestore` because a restore is replacing `liveDetachMask`
wholesale; this entry point reached the same `handleAsyncUpdate()` directly, so the suppression
rested on nobody calling it during a restore. It is genuinely unreachable today —
`parameterChanged` returns early when `isRestoring()`, and the macro gesture-begin path cannot run
concurrently with a message-thread restore — but that is a reachability argument about two callers,
and the third would not know. Behaviour is unchanged for every existing path. MacroEngine itself is
untouched.
**Left documented, unchanged:** KI-006, KI-007, KI-008, the MacroEngine teardown architecture, the
PopupMenu ownership question, the visualiser FrameClock criteria and the CurveView cache strategy.
Previous: **review round 47 (2026-08-03)** — two lifecycle orderings made structural,
one predicate unified, and one documentation overstatement corrected. No behaviour change:
(1) **`macroEngine->startDraining()` ran several statements before the constructor finished**, so
the 30 ms tick — which reaches back into the wrapper through `onDrainTick` →
`handleAsyncUpdate()`, and can drain detach bits, replace the mask and land a mapping pass — was
armed before `addListener(this)` and the nine managed-parameter registrations. A tick in that window
would have run a macro apply the wrapper could not hear. Nothing can deliver one (a `juce::Timer`
fires from the message loop, which cannot run inside a constructor executing on that thread), and
that is precisely the "safe by ordering" argument the startDraining/stopDraining split exists to
retire. Arming is now the constructor's last statement, so the guarantee belongs to the function
rather than to the platform's dispatch rules. Its old comment said "only now may the tick that reads
them run" — true of the two callbacks, not of everything else the tick reaches.
(2) **`animVBlank` was constructed in the member-initialiser list**, i.e. before `lastFrameTime` and
`uiAnimOn`, which are declared after it and which its callback reads — and before the
`registerAnimated` calls that fill `animated`. The destructor already refuses to rely on the same
platform argument (`animVBlank = {}` runs first there); the two ends of the lifetime now say the
same thing. The attachment is assigned at the end of the constructor; animation behaviour is
unchanged.
(3) **`ValueBox` recorded `downProp` under one predicate and consumed it under another.**
`mouseDown` wrote it only when `numberOfClicks < 2 && ! isBeingEdited()`; `mouseDrag` proceeded on
`! isBeingEdited()` alone, so on a NON-editable text box (where a double-click opens no editor) a
drag would have used the origin captured at the first click while the distance was measured from the
second press. Latent — `createSliderTextBox` makes every box editable when its slider is, and
`setupRotary` builds them all editable — but it is the asymmetry this file has removed repeatedly.
A `downArmed` flag makes recording and consuming one predicate. Deliberately NOT tested: the
divergent case is unreachable, so any check would pass identically before and after.
(4) **The MODE policy read as though the frozen-mirror thread crossing had been removed outright.**
It has not. `adoptFrozenMirror()` is a single writer, and a single writer is not a
message-thread-only writer — it is still reached from `setStateInformation`, which VST3 does not
promise on the message thread, while `presetDirty()` reads and `createCopy()`s the same member
several times a second. What rounds 41–42 actually did was remove the SECOND writer round 40 had
added in `prepareToPlay`: the exposure is back to the one KI-003 already owns, reduced to its
pre-round-40 shape rather than eliminated. `MODE_AND_ADAPTATION_POLICY`, `THREAD_MODEL`'s retained
row and KI-003 (which now names `liveFrozenTrims` in its list of restore-written state) all say so.
No guarantee was weakened to make the texts agree — the correction runs the other way.
**Left documented, unchanged:** KI-006, KI-007, KI-008, the MacroEngine teardown architecture, the
PopupMenu ownership question, the visualiser FrameClock criteria and the CurveView cache strategy.
Previous: **review round 46 (2026-08-03)** — a measurement defect, a gesture-model
defect, and one investigation that ended in "leave it":
(1) **The per-stage bench timed its own stimulus generator.** `stageRow` computed an LCG step and a
`std::sin` per sample INSIDE the timed span, while the method line and the matrix section both
promise stimulus generation is outside the stamps. A `sinf` is comparable to — for the cheap stages
larger than — the stage being measured, so every row of the per-stage table carried an unlabelled
constant overhead, in a table used to argue each §9 allocation row is inside budget: wrong in the
unsafe direction. The stimulus is now pre-generated per run outside the stamps (regenerated each
run, because the callbacks mutate their frame in place), leaving one indexed call per sample inside
the timed region. Re-measured on the machine the table already names: EQ 0.16 → 0.10 %, Compressor
0.15 → 0.10 %, Metering 0.18 → 0.10 %, Limiter 0.44 → 0.42 %, Clipper unchanged at 0.21 %. That the
two expensive stages barely moved is the corroboration — their own cost dominated the harness. No
verdict changed. The whole-engine matrix is untouched: it already stamped only `process()`.
(2) **"Worst block" was ambiguous, and stays conservative.** `worstUs` is the maximum over all five
runs rather than the worst block of the median run. That is the right number for a real-time budget
— a dropout is caused by the worst block that ever happens — so the implementation is unchanged and
both the bench's own method line and `PERFORMANCE_BUDGET.md` now say which it is, including that it
is ~5× more exposed to the scheduler noise the caveat already warns about.
(3) **A click on a macro's numeric readout re-engaged every detached parameter.** `ValueBox` opened
its `ScopedDragNotification` on mouse-DOWN, and a gesture-begin on one of the three §5.5 macros is
not neutral: it takes the macro branch, clears the whole §5.3 detach mask and re-lands the curve. So
clicking the number under Loudness to read it — or as the first half of a double-click to type into
it — discarded every manual Advanced edit. §5.3 makes a macro gesture "the clear notice" that the
user chooses the macro over their edits; pressing on a numeric readout is not that notice. The
bracket now opens on the first MOVEMENT, which is the line the code already drew one event later
(`mouseDown` refuses the bracket on a double-click for exactly the "about to type" reason). A real
drag still opens it before its first write, so the imbalance the bracket exists to prevent cannot
return, and the knob is untouched — `juce::Slider` opens its gesture on press, and that is a genuine
macro grab. `testAValueBoxClickIsNotAMacroGesture` drives the real ValueBox through synthesised
mouse events and pins both halves; two mutants (press-opens-gesture, never-opens).
(4) **Preset application through `setValueNotifyingHost`: investigated, left unchanged.** Notifying
is not stylistic — it is the only write that keeps the host's cached values, the APVTS tree and the
editor attachments in agreement, and every other value-landing path uses it (`applyMapping`,
`reassertFromRaw`, `applyOnePresetValue`), so silencing this one would leave exactly one restore the
host never learns about. The real observation is the volume: ~46 notifications per ‹/› step, which
some hosts record or dirty on. That is host behaviour, not correctness, and changing it would change
automation semantics — so it is now a DAW-matrix line in `RELEASE_COMPATIBILITY_CHECKLIST.md` with
the reasoning at the code site.
Item 3 of the brief (the Windows PowerShell guard) was already fixed at round 45 — `$m` is tested
before it is indexed — and needed no further change; verified against the current file.
**Left documented, unchanged:** KI-006, KI-007, KI-008, the MacroEngine teardown architecture, the
PopupMenu ownership question, the visualiser FrameClock criteria and the CurveView cache strategy.
Previous: **review round 45 (2026-08-03)** — four small hardening items, no behaviour
change and no deferred question touched:
(1) **The bench printed a C2 refusal even when the documented override answered.** `ANABASIS_BENCH_CPU`
is the supported way to run on a platform whose lookup is not written yet, so it must not read as a
failure — the previous shape emitted the whole refusal to stderr and *then* accepted the override,
telling an operator following the documentation that something had gone wrong when nothing had. The
two identity sources are now tried in order (automatic, then the override) and the refusal belongs
to exactly one state: neither answered. Verified by execution on both paths — with a suppressed
lookup the override run writes **zero bytes** to stderr, and the no-override run still exits 2
before printing any table.
(2) **The documented Windows repro block failed obscurely when the workflow could not be read.**
`(Select-String …).Matches[0]` indexes before the guard runs, so a renamed env var or a moved
workflow produced a PowerShell property-not-found error instead of the intended message — the same
class of failure round 43 fixed for the POSIX block with `${VAR:?}`. The match is now tested before
it is indexed. The value still comes from `build.yml` and is duplicated nowhere.
(3) **`~MacroEngine` now sets the teardown latch itself** by calling `stopDraining()` rather than
repeating its two tail calls. `~AnabasisAudioProcessor` still calls it first and that ordering is
unchanged — it is what makes `onDrainTick` safe — but round 37's point was to stop a structural
guarantee resting on a rule a caller has to remember, and this was the last place it did. No lock,
no join, no timer-model change. The residual is unchanged and restated at the site: a
`timerCallback` that has already passed the latch check is not waited for, because `juce::Timer`
offers no join. KI-003 stands exactly as written.
(4) **`retTrimSeq`'s comment now says what it counts.** The value is correct and the name stays, but
"generation" invited the wrong reading: it increments on every MEANINGFUL publication — ~90/s at
48 kHz/512 from `finishBlock`'s audible branch, plus once per `injectTrims` — not once per Freeze
latch. The comment now states that a difference of two readings is not a rate, not an interval and
not a latch count, and that the only supported use is inequality against a recorded value, which is
all `engineFrozenTrimsIfLive()` does (and it consults it only with Freeze ON, when publication has
stopped).
Also cleared: two `AffineTransform::getScaleFactor()` deprecation warnings introduced by round 44's
test — JUCE deprecated it for transforms carrying a rotation, and the editor's is a pure scale, so
the assertions read `mat00`. The build is documented as warning-free and now is again.
No new tests: the destructor latch is unobservable after the object is gone, the bench is a separate
console binary verified by running it, and the other two are documentation. **Left documented,
unchanged:** KI-008, KI-006 (both halves), KI-007 items 1/2/5/6, popup-menu ownership, the
MacroEngine re-entrancy architecture, and the visualiser lifecycle criteria.
Previous: **review round 44 (2026-08-03)** — undefined behaviour, platform guarantees
and three consistency repairs:
(1) **Two stored GUI closures captured a reference variable by reference.** `uiScaleBox.onChange`
captured the constructor-local `auto& ist`, and `setupComboInternal`'s captured its reference
PARAMETER `box`. Both entities die when their scope returns, and the closures outlive them:
[expr.prim.lambda.capture] captures the *variable*, not the referent, so invoking them afterwards is
UB even though every compiler resolves it through to the long-lived object. Normalised on the safe
forms the surrounding code already used — `[this]` plus a re-fetch (what the §6.4 toggle callback
does) and `[b = &box]` (a pointer by value, naming an editor member).
(2) **The bench's machine line was Linux-only while `ANABASIS_BUILD_BENCH` is platform-agnostic**,
so a local re-measure on macOS or Windows — exactly the workflow the refresh rule prescribes —
produced a results table identifying nothing, which is `PERFORMANCE_BUDGET.md` C2 failing silently.
`cpuModel()` now has a lookup per platform (`/proc/cpuinfo`, `sysctlbyname
machdep.cpu.brand_string`, the `ProcessorNameString` registry value) and, where none answers,
`main` REFUSES rather than printing: an incomplete table is worse than no table because it can be
pasted into the budget document. `ANABASIS_BENCH_CPU` is the documented override.
(3) **The local-repro block claimed three platforms while being a POSIX pipeline.** Round 43 fixed
the strictness lookup for macOS and then described the result as running everywhere, moving the same
defect one platform along. Split into a POSIX block (Linux/macOS) and a PowerShell block that
mirrors what the `windows` job actually runs, both reading the number from `build.yml` and neither
restating it. The Windows block does not invent a `run-tests.ps1` — there is none, and it says so.
(4) **Three GUI consistency repairs.** The `ValueBox` drag predicate was a STYLE test used by
`mouseDown`/`mouseDrag` while `mouseUp` cleared through a wider `dynamic_cast` — two predicates for
one begin/end pairing, and the style test also left a linear slider's readout unbracketed, which is
the defect the bracket exists to prevent, waiting for the first linear slider. One `sliderParent()`
now serves all three; behaviour is unchanged today because `setupRotary` builds every ValueBox and
builds only rotary sliders. `stepPreset` re-derived its position from the display NAME, which is not
unique across the two sources, so a user preset called "EDM Club" walked from the factory index; the
editor now remembers the source it last applied and uses it only while the name still confirms it,
falling back to the name search when anything else changed the name *(superseded 2026-08-08 by
ADR-0022: that remembered source became the wrapper-held identity, and the hint was deleted)*. And `resized()`'s early-return
guard gained a `jassertfalse` plus an unconditional `resized()` at the end of the constructor,
because `setSize` is a no-op on an unchanged size — so the guard could have produced a silently
blank window rather than a diagnosable failure.
`testTheSettingsCallbacksReachTheLiveTree` (two mutants) covers the closure refactor's direction —
honestly labelled as regression coverage, since no test can kill UB that happens to work. No test
was written for `stepPreset`: reaching it needs a file in the real user preset directory plus a
synchronous button click, and the check would cost more in fragility than it buys.
**Left documented, unchanged:** KI-008, the audio half of KI-006, KI-007 items 1/2/5/6, and the
popup-menu/visualiser/MacroEngine/bench-CI items the brief excluded.
Previous: **review round 43 (2026-08-03)** — two maintenance items, both cases of a
statement and its implementation disagreeing:
(1) **The documented local-validation command could not run on one of the three gate platforms.**
`CI_CD.md`'s repro block read the strictness with `grep -oP … \K`, and `-P`/`\K` are GNU
extensions that BSD grep — `/usr/bin/grep` on macOS — rejects outright. The failure was SILENT
rather than loud: `STRICTNESS` came out empty and the two commands below it ran with no strictness
argument, so a developer following the documented procedure on macOS validated against nothing and
could pass work CI rejects. Replaced with POSIX `sed`, anchored to the `env:` assignment so the
`${{ env.… }}` references in the job steps cannot contribute a second value, plus a `${VAR:?}`
guard that turns an unreadable workflow into a diagnosable failure instead of an empty argument.
The ownership model is unchanged — the value still comes from `build.yml` and appears nowhere else
— and the block now says which of its lines is Linux-only, since that was the other reason it did
not describe a macOS run. Verified by executing the documented pipeline, not by reading it.
(2) **`resetSweep` was read in two draw paths and written nowhere** — the half-ported Anamorph
state round 28 removed for `allCombos`/`hov`. REMOVED rather than wired, which is the opposite of
what its own comment asked for, so the reasoning is recorded at the site: honouring the flag in the
draw path would not have produced the sweep it describes, because `stepMicroAnims` reaches the same
conclusion one level up and SNAPS `vpos` to the target while the button is down. With the ease
already collapsed at its source, the draw-path branch could only have drawn the un-eased value by a
second route. "Sweep while held" needs all three sites to agree — new behaviour, and a listening-pass
call rather than a repair. The reachable reset gesture is unaffected either way: `mouseDoubleClick`
is dispatched from `internalMouseUp`, so the button is already released and the ease runs. The
removal is behaviour-identical by construction (a property that is never set reads false), which is
why it carries no new test — a check that cannot distinguish the two states would be noise.
**Left documented, unchanged:** KI-008, the audio half of KI-006, KI-007 items 1/2/5/6, and the
preset-menu/visualiser/bench-coverage/`resized()`-guard items the brief excluded.
Previous: **review round 42 (2026-08-03)** — ownership and synchronisation, six items:
(1) **The retained frozen-trim set is engine-wide; `FROZEN_TRIMS` is per-slot.** The engine latches
*a vector*, not "slot A's vector", so after an A/B switch into a freeze-ON slot holding none of its
own — where nothing stages a restore, so the generation pair stays equal — the incoming slot's next
save serialised the OUTGOING slot's latch as its own, and the next A/B or undo restore injected it.
The retained set is a runtime CACHE and may only answer for the slot it was filled under: the
wrapper records the retained generation whenever the live surface's frozen ownership changes and
adopts the engine's answer only once it has advanced past that base. `adoptFrozenMirror()` is now
the single writer of `liveFrozenTrims` for the same reason `replaceDetachMask()` is the mask's —
replacing the vector and re-basing the comparand are two halves of one rule, and three call sites
had only the first half. The retained *flag* became a *counter* to carry both questions (does one
exist / was it latched since) without a second atomic. Schema, ADR-0007 and ADR-0014 untouched.
(2) **Publication ordering re-verified and carried onto the counter.** `retTrimSeq` is
release-stored after its four scalars and acquire-loaded, as `pubTrimEver` already was; the new
`slotFrozenBase` is deliberately RELAXED, because it is only ever compared against that counter and
announces nothing itself. `THREADING_POLICY`'s publication-flag row names both.
(3) **§7 history ownership — KI-003's third member CLOSED without the thread-model decision.**
`setStateInformation`, which VST3 does not promise on the message thread, was clearing four
`juce::Array<juce::ValueTree>` stacks and a `ValueTree` that the editor reads at display rate and
pops from. It now only bumps `historyEpoch`; the message thread reconciles at `syncHistory()`, the
one point every read and write of the history passes through. The containers have exactly one legal
thread again, no lock was added, and nothing blocks in a host callback.
(4) **Persisted UI scale normalises once.** Three sites read `iid::uiScale` with three different
fallbacks, so a percent that is not a legal step was silently ignored rather than clamped — and on
a project load the window rendered at 100 % while the Settings panel kept displaying the previously
selected step. One `nearestScaleIndex()` answers for the transform and the combo, clamping to the
nearest step the way the tree's other persisted-value read rules do.
(5) **Initial UI state** was seeded at round 41 (undo/redo beside the preset mark and the animated
knob positions); re-checked, nothing else waits for a tick.
(6) **`CurveView::paint` rebuilt its curve on every repaint** — a full `MasteringEQ::prepare` plus
one `magnitudeDbAt` per pixel column — although `refresh()` already gates the repaint on a
fingerprint. The built path is cached against that same fingerprint, so a host-driven expose, an
overlay dismissal or a window move no longer pays for it. The two reference lines moved to a helper
the cached path also calls, because leaving them inside the rebuilt branch would have made the
cached frame differ from the uncached one — the defect a paint cache introduces if the split is
drawn in the wrong place.
Four mutants, one per behavioural guard. Two test-quality corrections worth recording: the curve
test first fished a `CurveView` out of the editor and returned early on zero bounds — the Advanced
panel is not laid out headlessly, so it asserted NOTHING while reporting a pass, and it could also
have driven the clip-transfer well with EQ parameters; it now constructs its own component and names
the mode. And the bounds half of the cache key is deliberately left uncovered: every stimulus for it
is also satisfied by a cache that ignores bounds, and a check that cannot fail is worse than none.
**Left documented, unchanged:** KI-008, the audio half of KI-006, KI-007 items 1/2/5/6 (including
the factory-apply and dirty-marker questions this round re-reviewed and did not change), and the
preset-menu/visualiser/navigation items the brief excluded.
Previous: **review round 41 (2026-08-03)** — a threading-correctness pass, and the
first item is the previous round's fix being wrong in a way its own tests could not see:
(1) **Round 40's frozen-trim fix introduced a data race.** It declared the wrapper's
`liveFrozenTrims` `juce::ValueTree` the durable owner of the latch and had `prepareToPlay` assign
it — a host callback JUCE does not deliver on the message thread, writing a non-thread-safe
reference-counted object that the editor's `presetDirty()` poll reads and `createCopy()`s
continuously, with both sides gated on Freeze being ON so the windows coincided exactly. The
ownership answer was wrong, not just its placement: the durable copy belongs in the LOCK-FREE layer
where the data already lives. `AdaptiveEngine` now keeps a RETAINED trim set — four scalars plus a
release-stored flag — which `reset()` does not clear, joining `learned`/`refOnsetRate`/`refTiltDb`
in that function's survivors list; the PUBLISHED set keeps its exact former meaning (what the DSP
is applying, zeroed by `reset()`), which is what keeps KI-006's readout half honest. Nothing
crosses a thread that did not already. `MODE_AND_ADAPTATION_POLICY` carries the corrected ownership
statement and the general rule it illustrates; KI-006 records both the closure and the round-40
misfire, because the shape recurs.
(2) **`pubTrimEver` was relaxed although it gates a read of the four trim scalars.** That is the
same shape `frozenAppliedSeq` was corrected to at round 39, whose reasoning is written out in
`AnabasisEngine.h`: THREADING_POLICY's relaxed rule rests on "carries no payload", and a flag
announcing other values is a payload. Both it and the new `retTrimEver` are now release-stored
after their values and acquire-loaded. `THREADING_POLICY` gained a **publication flags** row so the
distinction is a rule rather than four independent judgements — with the test stated (does
observing the flag gate a read of other state?) and the four instances named.
(3) **Freeze/preset questions reviewed, not changed.** A factory apply still leaves the slot's
frozen vector standing (KI-007 item 1) and the dirty marker still keys on preset-EXCLUDED
parameters (item 5); both remain spec questions. Item 5 gained the input it was missing: the
CONTENT of `FROZEN_TRIMS` is a function of *when* Freeze was engaged, so identical parameter state
can produce two different slot trees and two different answers to "edited?".
(4) **Undo/redo buttons are seeded in the constructor**, alongside the preset mark and the animated
knob positions, instead of rendering enabled over an empty stack for the first ~42 ms. The seed and
the 24 Hz tick share `refreshUndoRedoEnablement()` rather than repeating its two comparisons.
**Found while verifying (1), and the more serious finding: KI-008.** The suite's first two-threaded
stimulus let ThreadSanitizer's deadlock detector see that two JUCE locks are taken in opposite
orders on two reachable paths — the §7 pre-state snapshot calls `apvts.copyState()` from inside a
parameter-listener callback (M0 → M1), while `APVTS::setDenormalisedValue` notifies listeners while
holding the tree lock (M1 → M0). One thread cannot deadlock on it; the message thread starting a
drag while a host thread restores state can. It PREDATES this review series (P6 §7 bracketing) and
is NOT fixed here: the M0 → M1 edge is the undo grammar's snapshot point, and moving it is an
Architecture Review Gate item, so it is recorded with its evidence rather than guessed at.
`testTheFrozenLatchNeedsNoThreadCrossing` provides the stimulus; two mutants (the retention itself,
and reinstating round 40's write under TSAN). The stress is honestly weak on a plain build — it
does NOT fail against the round-40 code, because a data race is not a functional difference — and
that is why the verification instrument is a `-fsanitize=thread` build, where the round-40 code
reports races on `ReferenceCountedObjectPtr<ValueTree::SharedObject>::get()` and the refcount
increment, and the current code reports none.
**Left documented, unchanged:** KI-008, the audio half of KI-006, KI-007 items 1/2/5/6, KI-003, and
the preset-UX/popup/spectrum-decay/navigation items the brief excluded.
Previous: **review round 40 (2026-08-03)** — a targeted hardening pass, and every item
is a case of two mechanisms that had to agree with each other:
(1) **A meter reset was invisible from the GUI.** `requestMeterReset()` only set the momentary-request
flag; the DISPLAY publish that makes the clear visible with no audio running lived at the
`setStateInformation` call site, added there at round 33 for a reason — "a project is opened with
the transport stopped" — that is at least as true of the meter panel's click, which is when a user
reads an integrated figure and decides to clear it. The engine half must wait for a block top; the
display half must not. Both are now inside the request, so the two callers agree by construction.
`THREAD_MODEL`'s meter row and momentary-request row name the message thread as a clear writer.
(2) **The frozen vector had no stated owner, and two places were deciding it.** Stated now, in
`MODE_AND_ADAPTATION_POLICY` and at the code: the wrapper's `liveFrozenTrims` mirror is the DURABLE
owner (it is what serialises); the engine's `publishedTrim*()` are a faster copy that does not
survive `AdaptiveEngine::reset()`. A latch established LIVE had no mirror — no restore ever wrote
one — so a host rate change destroyed the only record, and the save wrote initialisation zeros
(pre-39) or nothing at all (post-39): the round-39 fix traded an invalid vector for a missing one,
which is quieter, not better. `prepareToPlay` captures the latch into the mirror before
`engine.prepare` reaches the reset, and the two-clause adoption test moved into
`engineFrozenTrimsIfLive()` so the save and the capture cannot drift. KI-006's heading, status and
both resolution paragraphs are re-synced: the SAVE half is closed, the AUDIO half is untouched and
remains a Freeze-semantics decision behind the Architecture Review Gate — this fix deliberately
stops at the serialization boundary rather than re-staging to the engine.
(3) **Three files each claimed to be the single source for pluginval strictness.** `CI_CD.md`
correctly located the value in `build.yml` and then sent the reader to `TESTING_POLICY.md` as "the
only document that states it"; that policy explicitly refuses to state it and points back at
`build.yml` — while carrying a phase→strictness table and three literal `10`s in its release-gate
bullet. Round 35 moved this duplicate rather than removing it; this time the ownership is a table
in both documents (value → `build.yml`; requirements → the policy; wiring → the procedure), the
policy's table and literals are gone, and `CI_CD.md`'s local-repro block reads the number out of
the workflow instead of pasting one (it had said `5` under a comment calling it current).
(4) **`refreshMapping()` promised what `drainTick` withheld.** The header said "this is what
re-lands the curve values"; inside a `ScopedRestore` the tick is suppressed whole and the scope's
exit ABORTS the arm, so nothing lands then or later. The behaviour is right — a mapping over a
restore is the clobber the guard exists to prevent — and the one caller that discovered it
documented the discovery at its own site (`applyFactoryPreset` drops its guard first) rather than
at the API. The deferral is now explicit and reported: `drainTick`/`flushPendingMapping`/
`refreshMapping` return whether the tick ran, and the ONE decision site stays in `drainTick`.
`testMeterResetClearsSessionHolds` (extended), `testPreparedStateAndSlotOwnership` case 4,
`testTeardownAndReengageInvariants` cases 1 and 4; four mutants, one per guard. The frozen-trim
stimulus first failed on an exact compare of a MEASURED trim through the XML decimal round trip —
the assertion is now the text conversion's tolerance, still separating the vector from both failure
modes (absent child, reset child).
**Left documented, unchanged:** the audio half of KI-006, the frozen vector surviving a factory
apply (KI-007 item 1), the preset dirty model, KI-003, and the preset-UX/popup/spectrum-decay/
navigation items the brief excluded.
Previous: **review round 39 (2026-08-03)** — correctness only, and the first item is
the previous round's safeguard failing to safeguard anything:
(1) **`hasPublishedTrims()` was true for every prepared instance.** Round 38 set the marker inside
`publishTrims()`, and `AdaptiveEngine::reset()` — which `prepare()` calls, so every host reaches it
before the first block — publishes too, and publishes ZEROS. The guard was therefore entered
exactly as often as before it existed. The marker now describes the CURRENT contents of the four
atomics rather than "has one ever been published": `publishTrims (bool meaningful)` takes the
decision as a parameter so no publication can skip it, `reset()` passes false with the
initialisation zeros, and an audible `finishBlock` and an ADR-0014 `injectTrims` pass true. The
reachable case the guard exists for is a host sample-rate change on a frozen slot — `reset()`
zeroes and republishes, the restore is no longer pending, and the next save wrote those zeros over
the slot's latch.
(2) **KI-006 asserted the opposite of the code.** It read "the PUBLISHED trim atomics are NOT
zeroed, and cannot be", reasoning from `finishBlock`'s `if (! freeze && audible)` guard and missing
`reset()`'s publish. Corrected against the tree: the consequence of a re-prepare is SYMMETRIC (the
audio, the Advanced overlay and the capture all read zeros), what survives is the wrapper's
`liveFrozenTrims` mirror, and the entry now says which of its two proposed resolutions that
changes — "keep the trims across `reset()`" has to carry the published copy too.
(3) **The GR ring's seqlock comment argued from the wrong primitive.** The CODE is the canonical
write-begin (relaxed increment + release fence, the `seq++; smp_wmb();` shape) and is unchanged;
the justification described what a release STORE gives. Restated as what the FENCE gives — a
StoreStore+LoadStore barrier, so accesses sequenced before it cannot be reordered after any store
sequenced after it, which is exactly "odd before the clear" — and it now also states what the
barrier does NOT buy: the reader's entry reads are plain, so a racing batch can still observe a
torn value and the epoch re-check DISCARDS it. That is the seqlock bargain, and it was the part the
comment left implicit.
(4) **The spectrum rings survived a re-prepare.** `prepare()` resized their scratch and left the
rings, so a host sample-rate change left up to 4096 frames captured at the old rate readable while
`SpectrumView` maps bins through the CURRENT rate. The GR history ring has been cleared at
`prepareToPlay` since P3; these two were the analyser state that did not follow. `ScopeBuffer::reset()`
rewinds the published INDEX only — `readLatest` copies strictly below it, so every stale frame
becomes unreachable and clearing 2 × 16384 floats would buy nothing observable.
(5) **A §7 pre-state could cross an A/B switch.** `switchToSlot` swapped the baseline but left
`gesturePreState` and the open-drag bits, so a drag open across a switch had its snapshot taken
from the OLD slot while the gesture-end compared it against the NEW slot's values — the difference
is the slot change itself, and the end pushed a step onto the new slot's stack describing a state
that slot never held. Both halves are dropped at the switch, matching what per-slot history already
means. `managedGestureBits` deliberately stays: it decides §5.3 DETACHMENT, and a drag continuing
after the switch is editing the new slot's value.
`testPreparedStateAndSlotOwnership` covers (1), (4) and (5); three mutants, one per guard, each
calibrated to the reachable case (the first two stimuli initially passed against their own mutants
— a restore still pending shadowed the marker, and two identical slots gave the gesture-end nothing
to push).
**Left documented, unchanged:** the frozen vector surviving a factory apply (KI-007 item 1), the
preset dirty model's preset-EXCLUDED half and its trim-content input, the undo/state-restore
threading architecture (KI-003), the duck's cost on a frozen-slot restore, and the
analyser/menu/navigation/CI items the brief excluded.
Previous: **review round 38 (2026-08-03)** — a triaged round of five state-consistency
items, each the minimal change consistent with the model already in the tree:
(1) **A debug build could abort while browsing presets.** `relandMacroCurve()` asserted that no
detach bit was staged, in the one function written to cope with one being staged.
`applyFactoryPreset` calls `replaceDetachMask` INSIDE its `ScopedRestore` and reaches the re-land
several statements after the guard drops, and `parameterChanged` stages a bit from whichever thread
the host delivers the callback on (KI-003) — so the assertion could not hold under the threading
model the same file documents. The two stores were already the whole mechanism; the assert is gone
and the comment now says why dropping the bits is the correct outcome (a gesture racing a bulk swap
belongs to the state being replaced, which `replaceDetachMask` decides for every other path).
(2) **A load reset two of the three gesture-state members.** `openGestureBits` and `gesturePreState`
were cleared; `managedGestureBits` was not — and it is set and cleared on ANY thread, so a BEGIN
delivered off-thread with the session replaced before its END left the bit standing, and the next
UNGESTURED write to that managed parameter satisfied the "gesture-bracketed" half of the §5.3
discriminator. The three are one family and now reset together.
(3) **Frozen-trim persistence, two unambiguous halves.** A freeze-OFF slot no longer serialises a
`FROZEN_TRIMS` child at all: `frozen` used to start from the carried mirror unconditionally, so a
slot that was frozen, loaded, then un-frozen wrote the old vector into every later save — a latch
serialised by a slot §5.4/MODE invariant 3 gives nothing to latch, and a child of the tree
`presetDirty()` compares. And the capture now also requires
`AdaptiveEngine::hasPublishedTrims()`, because the four published atomics start at zero and "all
four read 0" is otherwise indistinguishable between *measured, no trim* and *never measured* — the
save half of KI-006, closed without needing the audio half's owner call, since a value never
measured cannot be more truthful than the one the slot already holds. The round-trip fixture was
corrected with it: it set `FROZEN_TRIMS` on a slot whose `freeze` was OFF, so it pinned the round
trip of a state the product cannot produce; it now sets Freeze ON on both surfaces (the root
`ANABASIS` child carries the ACTIVE slot's live values, the `AB` child the per-slot copies) and a
new check pins the freeze-OFF half.
(4) **Undo/redo restore the dirty datum beside the state.** They restored the whole slot including
`presetName` while `presetBaseline` stayed put, so undoing a preset apply left the name and the
datum describing different presets. Neither route this was recorded as needing was required — the
baseline did not go into the StateSet (an ADR-0007 schema change and a Hard Stop) and undo does not
recompute it: the stacks are session-local and never serialized, so an entry is now the pair
`{ slot, baseline }`, taken and restored together.
(5) **Reset-to-macro is undoable.** It clears the mask and re-lands nine values while pushing
nothing onto the §7 stack, and its writes are ungestured so no drag step appeared either. It now
pushes its pre-state exactly as the preset applies do. No duck request added: unlike a preset apply
or an undo it rewires no discrete stage, and DSP invariant 8's enumeration is about the bulk swaps
that do.
`testStateReplacementAndHistoryConsistency` covers (2), (4) and (5); the frozen-trim half is covered
by `testFrozenSlotRoundTrip`'s new freeze-OFF check. Four mutants, one per guard. Documents
re-checked and synced: `KNOWN_ISSUES` KI-006 (save half CLOSED, audio half untouched and still an
owner call) and KI-007 items 3, 5 (the `FROZEN_TRIMS` input settled, the preset-EXCLUDED half still
open) and 9.
**Left documented, per the triage:** the frozen vector surviving a factory apply (KI-007 item 1 — a
MODE-invariant-3 question, and `applyPresetFile` behaves identically, so there is no internal
inconsistency to correct), the preset dirty model's preset-EXCLUDED half, the undo/state-restore
THREADING architecture (KI-003), the duck's cost on a frozen-slot restore, the spectrum rings not
being cleared on `prepare()`, and the analyser/menu/navigation/CI items the brief excluded.
Previous: **review round 37 (2026-08-03)** — a triaged round, limited to findings that
violate an invariant this build has already established. Four, plus one pairing hardening:
(1) **The post-teardown drain guarantee was unenforced.** Round 36 removed the `std::function`
nulling from `stopDraining()` for a sound reason (it raced a tick already about to invoke one) and
in doing so dropped what the nulling also bought: `drainTick`, `flushPendingMapping` and
`refreshMapping` are all PUBLIC, so "nothing drains after stopDraining" became a rule a future
caller had to remember — and after `~AnabasisAudioProcessor` has called it, the members
`onDrainTick` reaches are already destroyed. A one-way atomic latch (`drainStopped`), read at the
top of `drainTick`, restores the structural guarantee for every trigger at once and without the
race the nulling had. One-way on purpose: an object whose owner has begun teardown never becomes
drainable again.
(2) **The preset menu's raw `LookAndFeel` pointer** — the one part of round 24's `SafePointer`
hardening the look-and-feel did not cover — is closed by CONSTRUCTION rather than by either repair
previously weighed and rejected. `Options::withParentComponent (this)` makes JUCE's MenuWindow a
CHILD of the editor, so it cannot outlive it, and `Component::getLookAndFeel()` reaches `lnf` up
the parent chain: the explicit `setLookAndFeel (&lnf)` is gone and there is no pointer to dangle.
(3) **§5.3's re-engage was half applied on the gesture path.**
`MODE_AND_ADAPTATION_POLICY` invariant 3 already reads "the next macro gesture re-engages every
detached parameter **through the normal rate-limited glide**", and round 30 fixed the identical
"re-engaged but off-curve" shape on the tick path — so this was code drifting from a written
invariant, not an open question. A macro gesture that moved NOTHING cleared the mask and armed no
mapping, leaving the re-engaged parameters on the user's values. The begin now calls
`MacroEngine::armMapping()` (a relaxed store, safe from whichever thread the gesture arrives on)
beside the re-engage, so the gesture route and `resetToMacro()` do the same two things. Inert when
nothing was detached — `setParam` skips writes that would not change the value.
(4) **Copy A→B left the destination slot's undo history describing the state it overwrote**, so the
first undo after switching silently discarded the copy AND that slot's last edit. It needed no new
semantics either: `setStateInformation` already clears both slots' stacks because "a load starts a
fresh history", and a Copy is that event for one slot.
(5) Pairing hardening on round 36's own new code: `ValueBox::mouseDown` now calls `drag.reset()`
before opening a bracket, because assigning over a live `unique_ptr` runs the new constructor
(begin) before the old destructor (end) — a mouse-down that never saw its mouse-up would have
handed the host begin/begin/end/end.
Regression coverage is mechanical for the first, third and fourth
(`testTeardownAndReengageInvariants`, three mutants, one per invariant); the menu parenting and the
bracket pairing are lifetime properties with no headless observable, verified by reading the pinned
JUCE dispatch. Documents re-checked against the implementation and synced: `MODE_AND_ADAPTATION_
POLICY` §P5 gesture grammar (the arming is the code half of invariant 3), `THREAD_MODEL` rows 39
and 40 (the teardown latch, the re-engage arming), `KNOWN_ISSUES` KI-003 (the stopDraining
paragraph said "clears the callbacks", which stopped being true in round 36) and KI-007 items 4, 7
and 8, marked RESOLVED in place so the numbering other documents cite still resolves.
**Left documented, per the triage:** frozen-trim ownership (the never-run capture, the factory
apply's carry-over, the freeze-OFF slot that still serialises a child), the preset dirty model, the
undo/state-restore THREADING architecture (KI-003's third member — architectural, not introduced),
the spectrum analyser's idle behaviour, preset-ring navigation by name, the duck's extra bottom
hold, and the phase table now present in both `build.yml` and `TESTING_POLICY.md` (consistent
today; collapsing it decides which file owns the number).
Previous: **review round 36 (2026-08-03)**, whose first finding is the previous
round's fix landing in the wrong place:
(1) **The `vpos` seed ran BEFORE the APVTS attachment, so it stored the minimum it was meant to
stop showing.** `setupRotary`/`setupToggle` call `registerAnimated` and the per-control helper
attaches SECOND, so at registration a slider still carries JUCE's default 0..10 range and value 0 —
`valueToProportionOfLength (getValue())` is 0.0, and every knob still swept up from its minimum
when the window opened. The same ordering hit `onA` on toggles. The value-derived seeding moved
out of `registerAnimated` into `seedAnimatedFromValues()`, ONE pass over the registry called at the
end of the constructor, which is the only point at which every attachment exists. The test asserts
it over EVERY registered slider — with a guard that at least one default sits off its minimum, so
the sweep would be visible — and dies against both the unseeded state AND the round-35 placement.
(2) **The value box's drag wrote the parameter without gesture brackets.** `ValueBox::mouseDrag`
calls `setValue (…, sendNotificationSync)` directly, so dragging the numeric readout of a managed
parameter neither DETACHED it (§5.3 keys on gesture-bracketed) nor produced an undo step (§7 keys
on a completed message-thread drag) — the same asymmetry the double-click fix removed one control
over. Bracketed with `juce::Slider::ScopedDragNotification`, the stock RAII pair the attachment
already listens to, opened on mouse-down and closed unconditionally on mouse-up so a press that
moves nothing still balances. Not headlessly asserted: the box is a LookAndFeel-created child and
driving it needs synthetic mouse events, so this one is verified by reading the JUCE dispatch and
by the Level-5 pass.
(3) **`stopDraining()` nulled two `std::function`s, making its own residual worse.** The window it
cannot close is a tick ALREADY EXECUTING while another thread destroys the processor; such a tick
has entered `drainTick` and is about to invoke `onDrainTick`, so assigning it is a data race on a
non-atomic object where leaving it alone was merely a call into an owner still alive at that
instant. Both assignments dropped; `stopTimer()` + `cancelPendingUpdate()` do the work, and the
objects die with the engine.
(4) **Round 35's enforcement was debug-only.** `relandMacroCurve()` asserted the staged detach bits
were clear, so a future third caller would have shipped a RELEASE binary applying them in the
middle of its own apply. It now clears them as well — a no-op for both existing callers, since
`replaceDetachMask` already did it, and enforcement in every build.
(5) **`resized()` dereferenced five view `unique_ptr`s, safe only by construction order** (nothing
sets a size before they exist). One guard states the requirement instead.
(6) **The redo cap lived in the undo path.** `pushUndoStep` capped `undoStacks`; `undo()`/`redo()`
pushed onto the opposite stack uncapped, bounded only transitively. One `pushCapped` helper is now
the single place a StateSet joins either stack.
Documentation: `THREADING_POLICY`'s SPSC row said the index is published "once per block", while
the spectrum taps publish inside `processChunk` and therefore several times when a host block
exceeds the prepared size. Re-worded around the COMMITTED UNIT — a block for the GR history, a
chunk for the spectrum — because the guarantee the row exists for ("every frame below the acquired
index is complete") holds identically either way and only the reader's cadence differs; the
publication site carries the same note.
Reviewed and NOT changed: the async preset menu's raw `LookAndFeel` pointer (KI-007 item 4), and
the KI-003/KI-006/KI-007 restatements, already recorded in the same words.
Previous: **review round 35 (2026-08-03)** — two documentation drifts this PR itself
caused, and three more copies:
(1) **The pluginval strictness had a second copy that still said 5.** Round 29 collapsed the
duplicate rows inside `build.yml` and wrote that the block "is cited as the single authority for
the number, so it carries no duplicate rows" — while `CI_CD.md` kept its own fenced `env:` block
quoting 5, under a heading reading "Strictness escalates by phase — **in one place**". The
duplicate had not been removed, it had moved one file over, and
`DOCUMENTATION_LIFECYCLE_POLICY.md` makes CI workflow → `CI_CD.md` a mandatory sync. The section
now names where the number lives and quotes nothing; the same section gained the
`-DANABASIS_BUILD_BENCH=ON` configure flag the Linux job has carried since round 29 and this
document never recorded.
(2) **The README still advertised the pre-P5 project.** "phases P1–P4 complete … P5 (GUI) is
next", "the eleven ADRs", "pluginval L5", and OQ-013's Hard Stop "stands" — every one of them
invalidated by this PR, in the first document a reader opens, while `CLAUDE.md` and `HANDOVER.md`
were updated in the same commits. Rewritten to v0.1.0 CODE COMPLETE with the P5/P6 work named, the
ADR count pointed at `ADR_INDEX.md` as the count of record, the strictness pointed at `build.yml`
and `TESTING_POLICY.md` (which is where the number belongs at all), and OQ-013 dropped from the
open list.
(3) **The target-line CARDINALITY was still duplicated three ways** after round 34 removed the
names and numbers: `kNumTargets = 3` written out beside the table it counts, two fixed
`{ &targetSpToggle, &targetApToggle, &targetYtToggle }` arrays, and three hand-placed rows in
`resized()`. OQ-008 explicitly leaves a fourth line (club/CD) pending an owner decision, so a
fourth entry is the change most likely to be made — and it would have drawn a fourth tick and a
fourth penalty row with no checkbox to switch it off. `kNumTargets` is now `std::size (kTargets)`,
the three named toggles are one `std::array` sized from it, and the Settings row divides itself.
(4) **The nine managed ids were written out again in the editor's badge table.**
`managed_params::ids` is introduced in this PR as THE list the mapper and the wrapper's detach
discriminator share; the Advanced-view badges paired their own string literals with the knobs, so a
tenth managed parameter would have left one knob silently un-badged while the mask, the mapper and
the serialized slot all tracked it. The badges now index that list against a parallel `Knob*` array
whose FIXED size makes a mismatch a compile error.
(5) **The ADAPTIVE-mirror comment had been orphaned from its members** by the §5.3 block spliced
between them — the same defect round 34 fixed for the Learn paragraph, one file over, so the
relaxed-atomic justification read as if it belonged to `managedGestureBits`/`pendingDetachBits`.
Moved back onto `stagedAdaptiveLearned`/`stagedRefOnset`/`stagedRefTilt`.
(6) **`registerAnimated` seeded every animated property except `vpos`.** `stepMicroAnims` eases
`props["vpos"]` toward the slider's real proportion and an unset `var` reads as 0.0, while
`drawRotarySlider` prefers `vpos` whenever the control is not being dragged — so every knob swept
up from its minimum over the first frames after the editor opened. Seeded from
`valueToProportionOfLength`, which is what `Knob::doReset` already does for the case where the
sweep IS wanted.
Enforcement rather than a comment: round 34 recorded that "every `refreshMapping()` caller must
have replaced the mask first" and nothing checked it. Both callers now go through
`AnabasisAudioProcessor::relandMacroCurve()`, which asserts the staged bits are clear before
refreshing — so a third caller inherits the check instead of the trap.
Reviewed and NOT changed: the async preset menu's raw `LookAndFeel` pointer (KI-007 item 4), and
the KI-003/KI-006/KI-007 restatements, already recorded in the same words.
Previous: **review round 34 (2026-08-03)** — the copies this build kept finding, and
one guarantee that was two thirds applied:
(1) **The OQ-008 target values had three copies.** `LoudnessMeterView::kTargets` is documented as
THE compiled table, but the meter's tooltip carried the platform names, the numbers and the "as of"
date as free text, and the three §6.4 Settings checkboxes carried the names again. OQ-008
prescribes a per-release refresh, which touches the table — so the copies are exactly what would
have gone stale, leaving a tooltip quoting last year's figures. `Target` gains a `fullName`,
`kTargetsAsOf` becomes a constant, `tooltipText()` builds the string, and the checkbox labels come
from the table. The test rebuilds its expectation from `kTargets`, so it dies against the real
future event: a refreshed table plus a hard-coded string.
(2) **The editor destructor's guarantee was two thirds applied.** Round 33 added `stopTimer()` and
`cancelPendingUpdate()` with a comment rejecting "safe by ordering"; the THIRD message-thread
callback source, `animVBlank`, was left armed while `stepMicroAnims`'s state (`animated`,
`uiAnimOn`, `lastFrameTime`) was destroyed around it. `animVBlank = {}` detaches it there too.
(3) **The preset dirty mark lagged up to ~333 ms after an action that changed it.**
`refreshPresetDisplay()` throttles the full slot-tree compare to every 8th call, which is right for
the 24 Hz tick asking "did anything change?" — but undo/redo, the A/B toggle, a preset apply, ‹/›
and a save all call it immediately AFTER changing the state the mark describes, and all they did
was advance the divider. Those callers now pass `recomputeNow`.
(4) **The Learn-command paragraph had been orphaned by the ADR-0014 block spliced above it**, so in
a file where comments are the contract it read as if the frozen-trim consume were the Learn
command. Moved back onto `learnCmd.exchange`.
(5) **`publishSilentMeters()` outran its documented row.** Round 33 gave the meter atomics a
non-audio writer (the state load) while `THREAD_MODEL`'s and `THREADING_POLICY`'s meter rows still
scoped that publication to the audio thread. Both rows now name the clear writers and state why the
concurrency is benign: six INDEPENDENT relaxed scalars with no ordering role, so a load-clear racing
an end-of-block publish is last-writer-wins per scalar and the worst outcome is one display frame
mixing pre- and post-clear values.
(6) **Two "untouched" factory presets landed on the Tape colour model.** `colourModel`'s registered
default is 1 = Tape, and an override table is defaults + intents — so "Transparent Master" and
"Classical Dynamics" inherited Tape, inaudible only because the managed `colourDepth` the macro
mapping writes from Character is ~0 there. Their intent was resting on a §5.5 curve constant that
is ⊕ for the listening pass; both now name Clean explicitly (⊕ with the rest of the table).
Comments where a future change would break something silently: `drainTick` and `refreshMapping`
record the RE-ENTRANCY routing every trigger through one sequence created — `refreshMapping()` now
runs the wrapper's detach drain synchronously from inside `applyFactoryPreset`/`resetToMacro`, safe
because both reach `replaceDetachMask()` first and because `isApplyingMacro()` blocks the mapping's
own writes from re-entering, and stated so that breaking either is visible; the ADR-0014 consume
records that deriving the duck request from the record costs one extra ~11 ms bottom hold when the
engine is ALREADY at the bottom (the alternative reopens the `duckAskedWhileOut` ordering question);
and `build.yml` now names both bench residuals — platform-gated compilation AND the fact that
compiling is not running, so a semantics-only break surfaces at the next human re-measure.
Reviewed and NOT changed: the async preset menu's raw `LookAndFeel` pointer (KI-007 item 4 — both
repairs carry their own risk), and the KI-003/KI-006/KI-007 restatements, already recorded in the
same words.
Previous: **review round 33 (2026-08-03)**, a round of second copies and deferred
work that a stopped transport never reaches:
(1) **A project loaded with the transport stopped showed the previous session's meters.**
`setStateInformation` staged the meter-hold clear through the momentary-request row, and the
request is consumed at a BLOCK TOP — but opening a project with the transport stopped is the
ordinary case and no block runs at all, so an open editor kept reading the old integrated LUFS and
dBTP maximum indefinitely. The engine-side clear legitimately waits (it is engine state); the
DISPLAY does not, and `prepareToPlay` already published exactly these constants for exactly this
reason. Factored into `publishSilentMeters()` — one list, three callers (prepare, the block-top
consume, the load) — because two of the three had grown their own copy of it and they disagreed
about which of the six atomics to clear. `dbTpMaxHold` deliberately stays OUT of the helper: it is
plain audio-thread state, and only the two callers that own that thread touch it. Mutation-verified
in `testMeterResetClearsSessionHolds`, which now asserts before feeding any audio.
(2) **The editor's `Timer` and `AsyncUpdater` bases stopped themselves only after its members were
destroyed.** Base destructors run last, so `timerCallback`/`handleAsyncUpdate` — which touch
`meterView`, `animated`, the attachments — were quiet only because the message thread happens to be
the one executing `~AnabasisAudioProcessorEditor`. That is "safe by ordering", the argument this PR
stopped relying on for `MacroEngine::startDraining`/`stopDraining` and for the processor
destructor; `stopTimer(); cancelPendingUpdate();` now run first, so the editor says what it
guarantees.
(3) **The loudness meter's target ticks re-derived the row origin from `getLocalBounds()`.** The
review read the second copy as a 2 px misalignment; the arithmetic in fact AGREED — the tick spans
`bar.getY() - 2` to `bar.getBottom() + 2`, a deliberate symmetric overhang on an 8 px bar, so there
was no visual defect. The duplication was real: a change to the header height, the 24 px row or the
2 px gap would have moved one copy and not the other. The three bar rectangles are now kept and the
ticks derive from them, so there is one source.
(4) **`One factor, computed once` called `std::pow` twice.** The ADR-0013 release scale was
computed separately for the manual time and for `setAutoReleaseScale`; the comment claiming
otherwise was the drift, and the second `pow` was the only measurable part of that block. Computed
once into a local, bit-identical. The call stays unconditional and the comment now says why: it is
idempotent, factor 1.0 reproduces the prepared alphas exactly (so the invariant-7 null holds with
adaptation live), and a changed-since-last-block gate would be a second piece of state that has to
agree with the first — for one `pow` and two `exp` per block.
Comments where a future change would break something silently: `setupToggleInternal`'s four
`referTo`-bound toggles now say why they are NOT in `refreshInternalSettingsBoxes()` (a bool↔bool
binding `juce::Value` can express, unlike index↔value, index↔percent or one-bit-of-an-int) and
that the trade is testability, since `juce::Value` delivers asynchronously through a message loop
the headless suite does not run; and `testAMacroGestureWinsADetachRacingItInOneDrain` states that
it asserts the MASK only on purpose, because pinning the curve would silently decide KI-007 item 8.
KI-007 gains a ninth item (reset-to-macro is a nine-parameter, mask-wide change with no undo step —
the verb that DOES re-land the curve, where item 8 is the gesture that does not, and neither is
undoable) and item 5 gains the fact that feeds the same decision: a freeze-OFF slot still
serialises a stale `FROZEN_TRIMS` child, which is precisely the child whose presence flips the
dirty comparison. Reviewed and NOT changed: the async preset menu's raw `LookAndFeel` pointer stays
KI-007 item 4 — both available repairs carry their own risk (`dismissAllActiveMenus()` in the
destructor also closes another instance's menu; a shared static trades it for static-destruction
order at DLL unload) — and the KI-003/KI-006/KI-007 restatements are already recorded in the same
words.
Previous: **review round 32 (2026-08-03)**, where the recurring shape is a
DIRECTION or a GUARD applied to some of the things it names:
(1) **The Settings panel's three §6.4 target checkboxes never followed a project load.** They are
hand-built — three BITS of one int, so `Value::referTo` cannot express them — and were seeded once
at construction, while `LoudnessMeterView` reads `int_meterTargets` from the tree every frame. A
panel left open across a load therefore showed the previous project's targets against the new
project's meter, and the first click read as toggling the wrong thing. Exactly the one-way shape
round 26 removed from the three combos and `uiScaleBox`, still present one row down. Re-seeded on
the same 24 Hz tick, and `refreshInternalSettingsBoxes()` is now public **because this direction
has been the missing half twice**: no message loop runs in the headless suite, so the tick never
fires there and nothing could call it. `testTheSettingsPanelFollowsAProjectLoad` is the first test
in the tree that CONSTRUCTS the editor; it covers the round-26 combos as well, and three mutants
kill it (drop the checkbox re-seed, drop the combo re-seed, invert the direction).
(2) **The macro tick's restore guard covered the mapping half only.** It sat inside
`drainPendingMapping`, so a tick landing in a `ScopedRestore` still ran the WRAPPER's drain — which
writes `liveDetachMask`, the plain `juce::StringArray` the restore is itself replacing. Harmless on
the message thread; on the off-message-thread `setStateInformation` VST3 permits, it made the tick
a second concurrent writer and WIDENED the window KI-003 records. Guard moved up to `drainTick`,
covering the whole sequence, and the outcome is unchanged (`replaceDetachMask` drops the staged
bits either way) — which is why the test asserts on the mask DURING the restore.
(3) **`MacroEngine::applying` was the non-atomic half of an atomic pair.** `restoreDepth` beside it
is `std::atomic<int>` precisely because `AnabasisAudioProcessor::parameterChanged` reads the §5.3
discriminator from whichever thread the host chose, including the audio thread; `applying` was a
plain `bool`. In the case that matters the read is synchronous on the writing thread, so the
discriminator was always correct — but a genuinely concurrent read was a formal data race whose
worst outcome is one managed parameter wrongly detaching. Now `std::atomic<bool>`, relaxed.
(4) **The wrapper registered three listeners it discarded.** `addParameterListener` for
`loudness`/`character`/`tone` fed a `parameterChanged` whose first line rejects every non-managed
id — three registrations that read as load-bearing and were not. Dropped; the macros' listener is
`MacroEngine`, and the wrapper hears them through the gesture callbacks, where §5.3's rule lives.
(5) **The family tooltip style was unreachable.** `juce::TooltipWindow tooltips { nullptr, 600 }`
is a DESKTOP window with no parent to inherit from, so it resolved
`LookAndFeel::getDefaultLookAndFeel()` and `AnabasisLookAndFeel::drawTooltip`/`getTooltipBounds` —
adapted brand code under ADR-0009 — never ran. `setLookAndFeel (&lnf)` in the constructor, cleared
in the destructor beside the editor's own. Not headlessly assertable: the pixels are a Level-5
brand-pass item, so this is a wiring fix the human pass verifies. It also surfaced a DRIFT in the
adapted file: `drawTooltip`'s own comment stated that "on macOS the editor marks its TooltipWindow
non-opaque … see PluginEditor.cpp", describing a fix that came across in prose but not in code. The
macOS `setOpaque (false)` half is now adapted from Anamorph with the rest of it (ADR-0009,
provenance in place), so the comment describes what the tree does — and it only mattered once the
capsule became reachable, since undefined corner pixels are invisible in a capsule nothing draws.
Comments where a future change would break something silently: `ScopeBuffer::readLatest` now states
that it needs no `capacity - 1` clamp because of HEADROOM (4096 of 16384 — ~12288 frames must be
written during one copy), not because it is a different mechanism from the GR ring's `peek`, so a
capacity reduction or window widening is recognised as needing the explicit clamp; and `Knob`
records two accepted properties — an alt-PRESS-AND-DRAG is inert because the alt-click is a
complete gesture opened and closed in `mouseDown`, and a triple-click's third click resets nothing
because `getNumberOfClicks() != 2`. Reviewed and NOT changed: `AnabasisBench`'s link shape is
identical to both test console apps (`PRIVATE AnabasisDSP` + `PUBLIC AnabasisHardening` and the two
JUCE flag interfaces), so there is nothing to normalise; and `saveSlotFromLive`'s never-run capture,
the factory apply's stale frozen vector, the off-thread undo-stack mutation, Copy A→B's undo
history, name-keyed preset navigation, the dirty marker's tree-wide comparison, the menu's raw
LookAndFeel pointer and the frozen spectrum trace are each already recorded in KI-003/KI-006/KI-007
in the same words.
Previous: **review round 31 (2026-08-03)**, whose first finding is the previous
round's fix counted in prose instead of enforced in code:
(1) **The posted drain was a FOURTH entry point.** Round 30 fixed the tick's order and wrote "all
three paths now apply the same precedence" — while `MacroEngine::handleAsyncUpdate`, the path a
message-thread macro write posts to, still called `drainPendingMapping()` alone. Host automation
of Loudness/Character/Tone (message thread, no gesture, so nothing re-engages) racing a gestured
managed edit delivered off-thread mapped over the user's value, and the NEXT tick then marked that
parameter detached at the value the macro had just written. All three triggers — the 30 ms timer,
the posted update, and `flushPendingMapping` — now call `drainTick()`, so there is one sequence
and three ways to ask for it rather than a count that has to be re-checked; `handleAsyncUpdate` is
public for the reason `drainTick` is, because no message loop runs in the headless tests and an
entry point no test can call is exactly how this one drifted. Both the posted path and the flush
are mutation-verified.
(2) **The GR history frame asked for the ring's FULL capacity.** `peek` masks the absolute index,
so `head - kSize` aliases the slot the audio thread is filling at that instant — the producer
writes the slot and THEN publishes `head + 1`, so the oldest entry of a full-capacity window is
half-written. Reachable at ordinary settings (20 s at 48 kHz saturates the clamp for any block up
to ~234 samples). Clamp corrected to `kSize - 1`, the reason mirrored into
`GrHistoryBuffer::peek`'s contract, and the bound extracted to `GrHistoryView::windowEntries` so
it is testable without a graphics context — the tear itself is only observable with a concurrent
producer, so the bound has to be the guard.
(3) **The processor's destructor did not deregister its listeners.** It is safe today by
declaration order alone; the `startDraining`/`stopDraining` split exists precisely to stop relying
on that argument, so the teardown now says what it guarantees.
Comments where a future change would silently break something: `Knob::mouseDoubleClick`'s gesture
bracketing now records the settled JUCE dispatch order with its citation (two reviews read it in
opposite directions; the pinned `juce_Component.cpp` dispatches `mouseDoubleClick` from
`internalMouseUp` AFTER `mouseUp`, so the bracket cannot nest inside the drag's); and
`applyFactoryPreset` states why iterating the APVTS tree while writing it is safe (a property write
neither adds nor removes children) so a listener that ever added a PARAM node from there is
recognised as invalidating the iteration. KI-007 gains an eighth item: a macro-knob gesture that
moves nothing re-engages the mask without re-landing the curve, while `resetToMacro()` does both —
a §5.3 wording question, not a defect.
Previous: **review round 30 (2026-08-03)**, both of whose findings were the SAME
failure the previous rounds kept producing — a rule applied at one of its sites:
(1) **Round 29's staged-bit drop was written at ONE of the mask's five replacement sites.**
`applySlotToLive` got it; `applyFactoryPreset`, `applyPresetFile`, `setStateInformation` (which
rebuilds the mask inline rather than through `applySlotToLive`) and `resetToMacro` did not — so a
gestured edit delivered off the message thread just before a preset apply or a project load would
be stamped onto the freshly installed mask by the next tick, leaving a parameter detached in a
state it was never edited in, skipped by the mapper from then on, and serialized with the slot.
This round's own documentation had already stated the rule as covering "a slot switch, preset
apply or session load" — the sentence was right and the code was not. Fixed STRUCTURALLY rather
than by four more copies: `replaceDetachMask()` is now the mask's only writer, so a sixth site
cannot forget the drop. Each of the four paths has its own assertion.
(2) **The drain tick ran the mapping BEFORE the wrapper's bits**, which undid round 27's
precedence one level up: the wrapper's bits decide the detach mask, and the mapping pass reads
that mask to know what it may write. When a macro gesture and its value change both arrive off the
message thread — the only way both reach one tick — the mapping skipped a parameter the gesture
was about to re-engage and the mask was cleared a moment later, so the parameter read as
re-engaged while holding the user's off-curve value with nothing left to re-arm the mapping.
Order swapped; `drainTick()` is extracted so the order is testable at all, since an order that
exists only inside a private timer callback is an order no test can pin. All three paths
(message-thread, tick-internal, tick-order) now apply the same precedence.
Comments added where a future change would silently break something: the preset-exclusion
predicate now says that a new view-tier/monitor parameter added without it will be reset by
BROWSING factory presets (the defaults pass is what makes that predicate load-bearing); the
spectrum publication states the assumption it rests on (both stage loops run the full chunk, so an
early exit added later would publish stale scratch); `build.yml` notes that only the Linux job
compiles the bench, so a platform-gated signature change could still rot it. KI-007 gains a
seventh item: Copy A→B leaves the destination's undo history describing the state it overwrote.
Previous: **review round 29 (2026-08-03)**, a round of loose ends rather than defects
— most of its findings were already recorded in KI-003/006/007, and the rest were places where a
rule this PR wrote had not been applied to itself:
(1) **A restore replaced the detach mask but not the STAGED bits.** An off-thread gestured edit
leaves its bit un-drained for up to one 30 ms tick; a slot switch, preset apply or session load
landing in that window would have the following tick stamp that id onto the mask the restore had
just installed — the carry-over those paths clear the mask to prevent. Both staged inputs are now
dropped where the mask is replaced, which is what `MacroEngine::ScopedRestore` already does to a
pending mapping and for the same reason: a restore is not a gesture, so a gesture racing it
belongs to the state being replaced. Mutation-verified.
(2) **The accessibility claim did not hold for the host-hidden controls.** `setupCombo`/
`setupToggle` (the APVTS paths) set a title; `setupComboInternal`/`setupToggleInternal` did not,
and `uiScaleBox` — hand-built because it maps index↔percent — missed `registerAnimated` too, so
its hover skipped the easing every other combo has. A ComboBox with an empty title exposes NO
accessible name (a Button falls back to its text), so "accessibility names on every control" was
false for exactly the Settings panel.
(3) **`ANABASIS_BUILD_BENCH` was nested inside `ANABASIS_BUILD_TESTS`**, so
`-DANABASIS_BUILD_BENCH=ON -DANABASIS_BUILD_TESTS=OFF` silently built nothing — the option's
documented contract was stronger than its implementation. Moved to file scope and verified with
tests OFF. (4) `build.yml`'s header carried TWO P6 strictness rows that disagreed, in the block now
cited as the single authority for that number — collapsed, with a note that the env var is
unconditional rather than phase-gated. (5) `TESTING_POLICY.md` stated the rule against restating
the strictness and restated it parenthetically in the same sentence. (6) OQ-013/014/016 were
marked Resolved but left physically among the open entries while OQ-007 was moved — a reader
scanning the top saw three resolved questions mixed with the live ones; all three moved.
Recorded rather than fixed: the `startDraining`/`stopDraining` pair is not symmetric in STRENGTH
(construction is closed structurally, teardown cannot join a tick already executing — KI-003), and
the spectrum view freezes rather than decaying when audio stops, which is a listening-pass call
(KI-007 item 6).
Previous: **review round 28 (2026-08-03)**:
(1) **The dBTP readout warned against a literal −1**, which is merely the ceiling's DEFAULT, while
the view's own banner and this file both describe the row as "dBTP in `warn` over the ceiling". At
any other setting the warning fired at the wrong level — silent while genuinely over at a −6
ceiling, red while legal at a raised one — and a factory preset already ships a moved ceiling
(EDM Club, −0.5), so the non-default case is ordinary, not an edge. The ceiling joins the view's
snapshot so a ceiling move repaints the colour on its own.
(2) **The mutant count for `testFrozenTrimRestore` existed in six places and two of them
disagreed** (seven vs nine — nine is current, seven predates the round-24 additions). Resolved the
way the pluginval strictness was: ONE authority (ADR-0014's evidence line, which also enumerates
them) and every other reference says "each killed by its own mutant" without a number. The
historical coverage entry keeps its seven, marked AT THIS DATE.
(3) **The bench target was compiled by no automated job**, so `tests/bench.cpp` would rot silently
the first time a DSP signature moved — surfacing only when someone re-ran the measurement
`PERFORMANCE_BUDGET.md`'s refresh rule requires. The Linux job now configures with
`ANABASIS_BUILD_BENCH=ON`; it is still never RUN in CI (timings from a shared runner are exactly
the numbers C2 says must not be quoted). Its stale "run() above" comment is gone with it.
(4) **`allCombos` was dead state and the LookAndFeel's authoritative hover flag was never
published** — a half-ported piece of the Anamorph editor, so `drawComboBox` fell back for ever to
the live cursor test it documents as a pre-first-tick stopgap. The 24 Hz tick now publishes `hov`.
(5) The tooltip comment in `LookAndFeel.cpp` cited **KI-006**, which in Anamorph is the
transparent-window artefact and in Anabasis is an unrelated Freeze/re-prepare issue — the
reference came with the adapted file and pointed readers at the wrong document; now attributed
explicitly to Anamorph's numbering. **KI-007's heading said "Three" while four items were listed**
(and this round adds a fifth: the dirty marker keys on the whole slot tree, so preset-EXCLUDED
parameters mark a preset as edited) — the count is out of the heading entirely, because a
fine-review checklist read as shorter than it is, is worse than one with no number.
KI-003 gains a third family member: the §7 undo stacks are cleared from `setStateInformation`,
which the same file says may not arrive on the message thread — the same exposure as
`replaceState` on that path, recorded because the stacks are new P6 state.
Previous: **review round 27 (2026-08-03)**:
(1) **The preset dirty datum was engine-wide while the name it describes is per-slot.**
`presetBaseline` survived a session load (which cannot restore it — a session records WHICH preset
a slot holds, never whether it had been edited since) and did not travel across an A/B switch, so
after applying a preset in B and switching back, A's name was marked against B's baseline. Now one
per slot, swapped by `switchToSlot`, copied by `copySlotToOther`, and dropped with the other slot
fields on a load. **The first version of the test was uncalibrated and the mutant walked past it**
— slot B had no preset NAME, so `presetDirty()` early-returned before reaching the datum at all;
both slots must hold a named preset for the stimulus to touch the defect. Recorded because it is
the same lesson as round 25's three-attempt mutant: a passing mutant is as often a weak stimulus
as a correct guard.
(2) **Teardown was not symmetric with round 26's construction fix.** `startDraining()` was split
out of MacroEngine's constructor so the tick could not read half-assigned callbacks; the
destructor side went unhandled, and `macroEngine` is declared BEFORE every member
`handleAsyncUpdate()` touches — reverse-order destruction frees them all while the timer is still
armed. `stopDraining()` + an explicit `~AnabasisAudioProcessor` closes it. Half a fix is its own
category of bug: the premise that justified the first half justified the second equally.
(3) **The drain applied detach bits after the re-engage clear**, so a detach that raced a macro
gesture WON — the opposite of §5.3's "the next macro-knob gesture re-engages ALL detached params",
and the opposite of what the comment three lines above it claimed. Only reachable when both land in
one 30 ms tick, i.e. off-thread delivery; the new test builds exactly that (a gestured managed edit
from a worker thread, then a macro gesture on the message thread).
Also: the double-click knob reset is gesture-bracketed like the alt-click path — unbracketed it
produced neither an undo step nor a detach, so two gestures the UI presents as identical behaved
differently; `CurveView`'s repaint fingerprint includes the sample rate, whose coefficients the
drawn response depends on. KI-006 gains the SAVE half of its gap (an instance that never processed
a block captures all-zero published trims into `FROZEN_TRIMS`) and KI-007 the menu's raw
LookAndFeel pointer — both recorded rather than patched, the latter because each available repair
carries a risk of its own.
Previous: **review round 26 (2026-08-03)**, which caught two REGRESSIONS from the two
rounds before it and one drift in the highest-authority document in the tree:
(1) **The round-25 combo fix broke the other direction.** Replacing the two-way `Value::referTo`
with an explicit seed + `onChange` writer fixed the off-by-one but left state→widget silent, so a
project loaded with the Settings panel open showed the PREVIOUS project's oversampling, phase and
offline quality (`InternalState::replaceFrom` rewrites the same tree object the editor is bound
to) — and "correcting" one would have written back a setting that was already active. The boxes
are now re-seeded from the tree on the existing 24 Hz tick, keeping the explicit index↔value
mapping the `referTo` could not express. `uiScaleBox` is re-seeded with them, including the
`applyUiScale()` its display alone would not reach; the step list it shares with two other sites
is now one file-scope constant instead of three copies.
(2) **The editor still had the red line the wrapper removed in round 25.** Its `parameterChanged`
called `triggerAsyncUpdate()` unconditionally while listening to two AUTOMATABLE ids
(`advancedMode`, `bypass`), so automating Bypass could post to the message queue — lock, and an
allocation on some platforms — from inside the audio callback. Same shape as the wrapper's fix:
post only when already on the message thread, otherwise raise a flag the 24 Hz tick consumes. The
lesson recorded rather than the fix: when a rule is enforced at one site, grep for the construct
before calling the round done — the wrapper and the editor implement the same listener interface.
(3) **ADR-0014 described a duck request the code deliberately does not make.** The round-24
hardening added `duckRequested` beside the record flag; round 25 replaced it with a derivation at
the consume (one mechanism instead of two observations) and updated the code comment and this
file — but not the ADR, which outranks DESIGN.md. Corrected, with the history kept: an ADR that
describes a mechanism that is not there invites a future change to "restore" it.
Also this round: `MacroEngine::startDraining()` splits the 30 ms timer out of the constructor, so
the tick cannot read `isDetached`/`onDrainTick` while the owner is still assigning them (the owner
is not promised to construct on the message thread — KI-003's premise); ADR-0014 gains two
known-limits entries (the generation pair is engine-wide while the mirror is per-slot; and the
"vector lands in whatever slot is live at the bottom" degradation is any live-slot change inside
the duck, not only a Freeze toggle); the spectrum `static_assert` moved from the per-sample loop
to the scratch declaration a widening would actually touch; `TESTING_POLICY.md` stopped restating
the pluginval strictness in the present tense (`build.yml` holds it in one place, and the prose
copy had already gone stale by a day) and HANDOVER's P6 note reads as history. Comments added for
the two asymmetries a reader would otherwise read as oversights: `managedGestureBits` is
maintained on any thread while `openGestureBits` is armed only on the message thread (detachment
keys on "gesture-bracketed", an undo step on "message-thread drag"), and the factory-preset early
return leaves its undo step and duck standing for the same reason `applyPresetFile`'s does.
Previous: **review round 25 (2026-08-03)**, whose two flagged findings were both
user-visible and neither reachable by any existing test:
(1) **The three Settings combos were off by one.** `setupComboInternal` bound
`getSelectedIdAsValue()` — a 1-based ComboBox item ID — straight onto InternalState fields whose
encodings are 0-based and serialized that way, so picking "Off" turned oversampling ON, "Minimum"
gave linear phase, "Follow" forced maximum offline quality, and the stored default 0 matched no
item so the boxes opened blank. Now mapped index ↔ value explicitly, the shape `uiScaleBox` next
to it already used. **Level-5, and stated as such**: the state suite does not construct an editor
(doing so headlessly is what pluginval-under-xvfb is for, and it would catch a crash, not a wrong
mapping), so this fix carries a code comment and this entry instead of a regression test.
(2) **Factory presets were inaudible.** A factory table is defaults + a handful of intents and
expresses itself through the MACROS; the apply ran inside `ScopedRestore`, whose destructor
aborts the mapping those macro writes arm, and nothing re-ran it — so "EDM Club" moved `loudness`
to 80 and left the nine managed parameters at M(0,0,0). The guard is now scoped to the
value-landing and `refreshMapping()` runs after it, before the dirty baseline is captured (the
existing "clean right after the apply" check turned out to guard that ordering — a mutant that
captures the baseline first dies on it). Both directions are pinned: the factory path maps
(against the §5.5 curves, not magic numbers, so the ⊕ tuning can move), and a FILE preset's
managed values still survive untouched, because a file carries every parameter and its values are
authoritative — the mutant for that one had to be written three times before it was faithful, since
`refreshMapping()` inside a live `ScopedRestore` is inert and the first two mutants were quietly
no-ops.
Also this round: the wrapper no longer calls `triggerAsyncUpdate()` from a listener callback (it
can arrive on the audio thread, where posting takes a lock and may allocate — the hard red line
`MacroEngine` already refused for the same reason); the detach bits ride that class's existing
30 ms tick and the `AsyncUpdater` base is removed so the route cannot be re-opened. The
frozen-trim record's duck request moved from a second store beside the flag to a derivation at
the consume, removing the last ordering question between them. `GrHistoryBuffer::reset`'s opening
seqlock increment is a relaxed increment + release FENCE, since a release store orders only
earlier writes and the bulk clear could be observed above it. **KI-007 opens with the three items
this round deliberately did not fix** — the frozen vector surviving a factory apply, preset-ring
navigation identifying entries by name, and undo not restoring the dirty baseline — because each
is a semantics call and two of them are KI-006's question.
Previous: **review round 24 (2026-08-03)** — the first review of the v0.1.0 tree, and
three of its findings were live defects in code this repository shipped the day before:
(1) **`undo`/`redo` never requested the forced duck.** DSP_POLICY invariant 8 has named the undo
step as one of three bulk-swap routes since ADR-0004, and the code did not take it — an undo that
moved no discrete stage never reached a silent bottom, so after ADR-0014 the frozen-trim vector it
stages sat pending indefinitely and was injected at the next unrelated duck, into whatever slot
was live by then. Both halves are now tested (`testUndoRequestsDuck` for the dip, the
`frozenRestore/undo` case for the vector), each killed by the same mutant.
(2) **The frozen-trim capture's "pending" test was off by one step.** It read the ADR-0012 record
flag, which the block top clears — but the vector is only published at the duck bottom up to
~34 ms later, and the editor's ~3 Hz dirty-marker poll reaches that window in ordinary use, so a
save there serialised the PRE-restore trims and (because the capture also rewrote the mirror)
destroyed the loaded vector permanently. Fixed with a stage/applied generation pair advanced only
by `injectTrims`, and the capture now writes a LOCAL — a display query must not rewrite
serialisable state. The generation pair is mutation-verified; the local is structural (with the
counters in place the write-back is benign, and it is removed so a future caller cannot make it
harmful again — recorded rather than given a synthetic test).
(3) **The meter-reset watermark admitted the straddling sub-block.** `subCount + 4` lets the first
fresh gating block average the sub-block that was PARTIALLY FILLED at the reset — up to 100 ms of
the old programme at a quarter of the block's energy, which the −10 LU relative gate then turns
into "the integrated figure stays pinned to the previous material", the exact failure the
watermark exists to prevent. The wrapper-level test could not see it (its stimulus drains quiet
audio before the reset, so the straddler was already quiet); the new
`testMeterResetIgnoresTheStraddlingSubBlock` drives the meter directly, where "old programme" and
"lookahead tail" cannot be confused, and the two readings are ~30 dB apart.
**An adversarial verification pass over the round-24 diff then found four more, three of them in
the round-24 fixes themselves** — recorded because the pattern is the round's own lesson repeating
one level down: (a) the staged frozen record could still strand, because the caller's duck request
and the stage are two separate stores with a full `adoptParamsTree` between them, so a block
landing in the gap spends the whole duck before the record exists — the record now carries its own
duck request, which makes "a staged record always gets a bottom" structural rather than a rule four
call sites must remember (`testAStagedFrozenVectorAlwaysGetsABottom` reproduces the interleaving
deterministically); (b) `frozenAppliedSeq` was relaxed on both sides although it GATES a read of
the four published trim atomics — release/acquire now, since the staleness-counter row's "carries
no payload" justification does not apply to a counter that announces other values; (c) the
consumer sampled the generation AFTER the payload, so a stage landing mid-consume stamped the
newer number onto the older vector and claimed settled — sampled first now, which makes the same
interleaving self-correcting instead; (d) the gesture fix closed only one asymmetry — a
message-thread begin whose END arrived off-thread leaked its bit for ever and undo then never
fired again for the rest of the session. The mask is atomic and the end clears it on whichever
thread it arrives on; only the ValueTree work stays message-thread-gated. (b) and (c) are
memory-ordering corrections that no single-threaded mutant can kill on x86-TSO — recorded as
correctness-by-construction rather than given a synthetic test. The pass also surfaced **KI-006**
(a sample-rate change drops a frozen slot's adaptation from the audio while the readout and save
still report it) which PREDATES this work and is recorded, not folded in.
Also fixed, smaller: the gesture-end counter was asymmetric (a host delivering begin off-thread
and end on the message thread closed a different open drag, pushing its step mid-gesture — now
keyed per parameter, `testAGestureEndWithoutACountedBeginIsIgnored` with the off-thread begin as
the real stimulus); the preset gate accepted any well-formed XML, so a foreign root cost an undo
step for a guaranteed no-op (both gate and apply now read through
`PresetManager::parsePresetFile`); `prepare()` no longer inherits an ADR-0013 auto-release scale;
the editor's async menu/file-chooser callbacks hold a `SafePointer`; the spectrum taps' L/R
assumption is a `static_assert` instead of a comment; the factory-index validation no longer hides
in a comma expression. **The lesson of the round, recorded because it recurs:** every one of the
three real defects was a *second* mechanism that had to agree with a first — the duck request with
the staging site, the capture guard with the application site, the watermark with the sub-block
that was mid-flight. Each first half was tested and correct; nothing tested the agreement.
Previous: the **v0.1.0 completion batch (2026-08-02, owner blanket approval)**
(PR #5): the two owed ADRs are written and registered — **ADR-0013** (OQ-016: the release trim
scales the auto poles; `MODE_AND_ADAPTATION_POLICY.md`'s "three of four audible" scope note
rewritten to four) and **ADR-0014** (OQ-013: the frozen-trim restore; the Hard Stop banners in
`PluginProcessor.h`, `AdaptiveEngine.h`, `THREADING_POLICY.md` and `THREAD_MODEL.md` lifted and
replaced with ADR citations, a frozen-trim row added to the implemented-edges table, ADR-0007's
index row upgraded to Verified). **A drift found and fixed while writing the ADR-0014 test:**
`setStateInformation` adopted `liveFrozenTrims` but never staged the engine restore — only
`applySlotToLive` did — so a freeze-ON *session load* (the primary OQ-013 case) would have
silently dropped the vector; the stage is now in both paths and `testFrozenTrimRestore`'s
unprimed/primed load cases pin it (seven mutants AT THIS DATE, each killed by a distinct check — both
application sites, both staging sites, the capture, the no-audio mirror guard, the freeze-off
condition). OQ-007 (plain zips), OQ-014 (reading 1 — `THREADING_POLICY.md` gains the
listener-guard row citing ADR-0005/ADR-0011 as enacting authority) and OQ-013/OQ-016 moved to
Resolved; `testFrozenSlotRoundTrip`'s fixture property names corrected to the real ADR-0014
field names (`releaseOctaves`/`scHpfHz`/`dynTiltDb` — the old test asserted byte transport, so
it passed either way; the fixture was drifting, not the test); the preset bank extended to the
brief's 12 (names/values ⊕); the brand checklist marked provisionally passed (boxes untouched —
C7); CI strictness 8 → 10; HANDOVER carries the v0.1.0 completion summary. Previous: for the
**Windows-CI stack fix (2026-08-02)** (PR #5): the spectrum commit's
two ScopeBuffers carried 2 × 128 KB of INLINE arrays inside every engine, and the state suite
builds processors on the stack — three coexisting in one test ≈ 1.2 MB against Windows' 1 MB
default (Linux's 8 MB hid it), and the crash ate its own fully-buffered CI output, which is why
the log showed exit 1 and nothing else. Storage moved to the heap (constructed off the audio
thread; the push path still never allocates — the one functional delta from the Anamorph copy,
recorded in the provenance header), and both suites now run UNBUFFERED stdout so no future crash
can hide its position again. Previous: (PR #5): the §9 allocation
table moves from ⊕ targets to measured-against-target — every stage standalone under its
allocation, with the honest caveats stated in place (standalone ≠ in-chain attribution; the OS
row is bounded by the matrix difference, which bundles the region stages' rate multiplication).
Previous: (PR #5): the compiled-in
override tables (defaults + intents through the shared lock/exclusion core; empty mask; one undo
step; the five brief-named presets as ⊕ drafts), the FACTORY menu section with the ‹ › ring, and
the dirty marker. Mutation pass: defaults-first, the lock skip, index-validated-before-the-
bracket, and the empty-mask contract each die against their own assertion. Previous: (PR #5): `AnabasisBench` (OFF by
default, `-DANABASIS_BUILD_BENCH=ON`) implements the DESIGN §9 procedure — SR × block × OS × mode
matrix, median ns/sample of the timed region over 5×1 s runs, worst block, machine recorded —
and `docs/architecture/PERFORMANCE_BUDGET.md` now exists with the measured table: the budget case
(48 kHz · 4× · working) reads 3.0 % of a 2.1 GHz Xeon core against the ≈5 %-of-a-desktop-core
target. Whole-engine numbers only; the per-stage ⊕ allocation stays unclaimed until a profiler
pass. Previous: (PR #5): the §7 per-slot undo stacks —
the StateSet slot tree as the undo unit (the widening rationale now mechanical: an undone edit
reverts value and detach bit together), gesture-gated coalescing with automation folded silently,
preset applies bracketed with the parse before the bracket, session loads clearing both
histories, top-bar ↶ ↷. One mutant survived its first pass — pushing an undo step on a FAILED
parse was invisible to "the stack is unchanged" because canUndo() is a bool, not a count; the
stimulus now drains the stack first so the check is "still EMPTY". Previous: (PR #5): panel wells (clip transfer
+ EQ response through the DSP's own code; per-stage GR bars answering the recorded which-GR
question), accessibility names, and the phase documentation — HANDOVER's P5 summary and phase
row, CLAUDE.md's phase line, the CHANGELOG entry, the brand checklist's status note (human boxes
deliberately untouched — Level 5), and a four-phase drift in TESTING_POLICY's "Current status"
("TODO (no code yet)" since P0), reported here and corrected as the smallest edit. Previous: (PR #5): the last planned edge is
implemented — two ScopeBuffer capture rings in the engine (post-input-gain and render taps, one
release-store per chunk, scratch preallocated in prepare) with the FFT strictly GUI-side; the
dual-trace SpectrumView takes the Advanced strip's middle share, dismisses to int_spectrumOn and
returns from Settings; GR widens into the share when it is off. Previous: (PR #5): the §2.9 display
layer — LoudnessMeterView (M/S/I bars, target lines off the OQ-008 compiled table with the
"as of" tooltip, penalty rows gated on a valid integrated figure, dBTP in `warn` over the
ceiling, click = the meter-hold reset request) and GrHistoryView (the first consumer of the GR
ring, reading under the epoch contract decided at the P5 opening — odd/moved epoch discards the
frame). OQ-008's values are gathered and cited; club/CD stays absent rather than invented.
Previous: (PR #5):
ADR-0005's deferred half is wired and Verified — the §5.3 detach discriminator (three conditions,
each killed by its own mutant, including the two mid-gesture overlap cases), the mapping skip,
re-engage-on-gesture and reset-to-macro; the §6.2 Simple view (big knob, macro row, ceiling
lock, monitor toggles, out-LUFS readout) and the §5.4 Learn button with the 5 s minimum pass and
the wordless empty-pass readout. Previous: (PR #5): the two planned edges
THREAD_MODEL reserved for P5 are implemented and their open decisions taken — the meter-hold
reset (momentary-request row; a state load stages it; `resetIntegrated` watermarks the
gating-block assembly so a straddling block cannot pin the relative gate at the old programme's
loudness) and the GR ring's reset epoch (index may rewind; readers discard a batch that raced a
clear). `AudioProcessor::reset()` stays un-overridden, now as a recorded decision. Previous round: (PR #5): the Learn command
was a code plus a flag, which a consumer could take between the writer's two stores and then have
re-raised behind it — the same command twice, and a repeated commitThenStart commits a pass one
block old. Two bits need no record: the code IS the flag now. Previous round: (PR #5): three documentation
drifts corrected (a changelog stimulus count, HANDOVER's phase header, a missing separator), the
forced-duck request added to what `reset()` clears, and two ⊕-draft/P5 questions recorded where
the code that raises them lives. Previous round: (PR #5): the per-block
repairs moved ahead of every consumer that reads what they repair, the oversampler latency table
is now bound to its headroom constant by a `static_assert` rather than a Debug-only `jassert`, and
the meter's ring guard came back — with the stimulus that can tell it is there. Previous round: (PR #5): the previous round's
Learn cancellation runs later in the block than the Learn COMMIT is consumed, so a ruined pass
could still be saved as the reference — refused at the writer instead. Previous round: (PR #5): the two stages that
emit no audio — the BS.1770 meters and the §5.4 feature extractor — overflow on a legal float and
fail silently, so they are now repaired per block like the limiter's detector. Previous round: (PR #5): the invariant-9
recovery had one more hole, and it was in the sentence that justified leaving stage E without a
boundary — the Post EQ can overflow, and did, permanently. Previous round: (PR #5): one of the four
§5.4 trims is inert in the factory state — the release trim lands on a parameter the limiter reads
only in manual mode — recorded as **OQ-016** rather than wired, because making it audible changes
the default sound. Previous round: (PR #5): the previous round's
recovery covered the engine's own stages but not the oversampler, whose default path is a
recursive IIR — so the same permanent silence survived at every oversampling factor. Previous round: (PR #5): a finite input could
make a gain-carrying stage overflow, and the boundary that swallowed the resulting NaN left the
stage holding it — the plugin went silent until it was re-prepared. The boundaries now record
what they substitute and the affected stage is cleared. Previous round: (PR #5): the ADR index still
told readers the project had no `src/` and no `tests/`, and the eleven sign-off ADRs still read
`Unverified (no src/ yet)`. Documentation only. Previous round: (PR #5): the Learn commands
join the restore on ADR-0012's staged-record row — two flags in a fixed order lost BOTH commands
when a stop and a start fell in the same block. Previous round: (PR #5): the clipper's engage
edge is a confirmed defect recorded as **KI-005** after a fix attempt was tried and reverted, plus
seven scope/comment corrections. Previous round: (PR #5): invariant 12's scope
now names the bypass crossfade as its second post-dither leg (the claim written two rounds ago
was too absolute), and three comment/scope corrections land with it. Previous round: (PR #5): `AdaptiveEngine::reset()`
cancels an in-flight Learn pass. Previous round: (PR #5): the direct-adopt branch
now clears EQ state on a position change (the offline-entry edge made that branch reachable
mid-stream), the wrapper's staged mirror is atomic, and two reset-lifecycle carry-overs are
closed. Previous round: (PR #5): the previous round's
offline-flip fix was direction-agnostic and broke the RETURN edge — fixed, with both directions
now pinned by their own tests. Previous round: (PR #5): a realtime→offline flip
no longer ducks the head of a bounce, `learnActive` joins `learned` as an atomic, and CLAUDE.md
stops claiming the project is at P1. Previous round: (PR #5): the limiter push was
being applied BEFORE the clipper instead of after it — a real chain-order deviation that made the
macro's primary push drive the clipper as well. Previous round: (PR #5):
**ADR-0012** ratifies the GUI→Audio staged record unchanged (owner chose option 1), adding the row
to `THREADING_POLICY.md`; invariant 7 gains the bypass-null scope sentence. Previous round: (PR #5): the load-then-save
learned-reference loss, the publish-on-a-short-circuited-block duplicate, and **OQ-015** — an
external review found that the P4 learned-target restore is an off-table cross-thread path that
never passed the Architecture Review Gate. Previous round: (PR #5): the meters moved off
the monitor path onto the engine's render tap, the limiter's three level-affecting controls
(link / preserve / detector HPF) smoothed per invariant 8, and the round's doc-drift corrections
(45 not 44 cached atomics, README's re-staled check count, the registry's unlanded-§2.8 text).

### P5, the grammar before the pixels — ADR-0005's other half, and a discriminator that had to
survive its own overlap cases (2026-08-02)

**The detach discriminator's third condition looked redundant and was not.** Gestured-and-not-
macro-originated seemed sufficient: macro writes and restores are ungestured, so the gesture
condition filters them. The first mutation pass proved otherwise by SURVIVING the removal of the
isApplyingMacro/isRestoring condition — the stimuli never created the overlap it exists for. A
mapping pass or an A/B restore landing while the user's gesture is OPEN on the same parameter is
gestured from the discriminator's viewpoint but is not a user edit; two new stimuli create
exactly that overlap, and the condition's removal now fails both. A guard no mutant kills is
either dead code or an uncalibrated stimulus — the same lesson as the meter ring, from the other
side.

**Learn's minimum pass is enforced where the bias lives.** The 5 s floor is not UX polish: the
features are ~1.5 s integrated, so a shorter pass commits mostly what played BEFORE the button
press (the P4-recorded debt). The button counts the remainder down in numerals — no invented
wording — and an empty pass flashes `warn`, which is the readout `commitLearn`'s no-op owed its
caller since P4. Undo bracketing of the commit waits for the P6 undo machinery, recorded rather
than half-built.

### P5 opens — the planned edges are settled before any pixel is drawn (2026-08-02)

P5's first commit contains no GUI: it takes the decisions the GUI code would otherwise have made
implicitly. THREAD_MODEL's planned-edge section reserved two edges for P5 and named the open
questions; both are now implemented rows with their decisions recorded where the reservation was.

**The meter-hold reset produced one real DSP finding before any button exists.** The obvious
`resetIntegrated` — clear the histogram, keep the rolling windows — fails its own test: gating
blocks are assembled from the last four 100 ms sub-blocks, so the first block committed after a
reset straddles up to 300 ms of pre-reset material. A reset issued during loud playback puts one
loud straddling block into the fresh histogram, and the −10 LU relative gate then excludes every
quieter block measured after it — the "reset" figure reads the OLD programme's loudness for the
rest of the session. The watermark (only gating blocks whose four sub-blocks all post-date the
reset may enter) is the fix, and the stimulus that found it is the one the test keeps: reset
between a loud and a quiet passage, which is exactly how the escape hatch will be used.

**Both open decisions are taken, in the documents that were holding them.** A state load clears
the holds (staged through the same momentary-request row, so it lands at a block top like every
other command). `AudioProcessor::reset()` stays un-overridden — a transport stop must not cancel
a Learn pass (invariant 3's explicit start/end would silently become "within one play") or a
mastering measurement, and the tail it would flush is ≤ 10 ms.

**The GR ring's reader contract is the seqlock the planned edge predicted.** The index may
rewind across a reset; a `resetGuard` epoch brackets the host-thread bulk clear (odd = in
flight), and a reader that samples it before and after a batch of peeks discards a racing batch
and re-anchors. One display frame dropped at worst, on an event that already blanks the
programme.

### Twenty-third review round — the fix was a narrower mechanism, not a wider one (2026-08-02)

Nine items: one defect fixed, one warning made mechanical, one comment completed, three repeats,
three verifications that agree with the code.

**A staged record with a two-bit payload was the wrong shape, and the second window proves it.**
ADR-0012's Known limits described one consequence of publishing code-then-flag: a composing writer
that reads the flag back between the consumer's `exchange` and its use drops an outstanding
commit. The review found the same window in the other direction — a consumer whose `exchange`
lands between the writer's two stores runs the already-visible new code, and the writer's following
`store(pending, true)` re-delivers it. A repeated bare commit is harmless; a repeated
`commitThenStart` calls `commitLearn()` on the pass its own first delivery started one block
earlier, so `learnBlocks == 1` and the saved reference is measured from a single block of audio
and then serialized into the project.

**The fix removes a mechanism rather than adding one.** The payload is two bits, so there is
nothing to stage: `learnCmd` is one atomic word with `kLearnNone` meaning nothing pending, and one
store cannot be split. The edge moves from ADR-0012's staged-record row to the single-lock-free-
scalar row it always fitted — a narrowing, no new path, one fewer window. ADR-0012 keeps its
contract (the learned-target restore has a real payload) and gains the amendment; `THREAD_MODEL.md`
carries the new row text. The surviving residual is the opposite direction, and closing it would
trade a dropped command for a duplicated one — which is the trade this round just refused in the
other direction. **The race is not headlessly reproducible**, like KI-003's: what the suite pins
is that the composed semantics still hold (`testStopThenStartInOneBlockKeepsBoth`).

**A warning became a tripwire.** Invariant 7's third leg — the bit-exact null holding because
`ClipSat::activityEnv` is exactly `0.0f` while nothing clips — was carried by comments in two
files and by nothing executable. `activityEnvelope()` is now public for the test that asserts the
zero directly. Verified the way it should be: a mutant that floors the envelope at `1e-9` passes
the null test (the tame branch needs `tameGainDb < -0.01f`, so a tiny value changes no sample) and
fails the new check. That is exactly the failure the comments feared, and it is now mechanical.

**Three verifications, and one of them extended a comment.** The reviewer re-traced the limiter's
detector recovery and agreed with last round's comment, then found what it did not cover: in
true-peak mode a NaN also enters the estimator's 12-tap FIR history, so the un-limited stretch is
one window PLUS `kTaps` region samples. Added. The other two — the loudness-compensation measure
being only marginally perturbed by the duck, and the three-way render equality being structural
rather than incidental — confirm existing comments and needed no change.

### Twenty-second review round — the round with no defects in it (2026-08-02)

Six items, and the review classes five of them as not-defects itself. Nothing changed about what
the plugin computes.

**Three documentation drifts, all of the same kind: a number or a sentence that no test reads.**
The changelog said the extreme-level test carries four stimuli; it carries five (the Post-EQ case
landed a round later). `HANDOVER.md`'s preamble still opened "Snapshot taken at the P0 → P1 phase
boundary" above a status table reading P4 and three later phase summaries. `KNOWN_ISSUES.md` lost
one `---` between KI-003 and KI-004. The changelog entry now says where its count comes from — the
test's own `run(...)` cases — because "five" is as re-derivable as "four" was.

**The forced-duck request joins what `reset()` clears.** Behaviour is unchanged and no mutant can
kill the line: the first block after a reset takes the `! smoothersPrimed` branch, which already
discards the request. The line is there so that discard is not load-bearing for a fact stated in
another branch — and the comment says exactly that, rather than dressing it as a guard. What it
does not fix is recorded in KI-004: a swap made while the transport is stopped leaves the request
standing, and ~34 ms of fade plays over the head of the next take. Ageing it needs a time base the
audio thread does not have, so it is a P5 wrapper question.

**Two ⊕ drafts recorded next to the constants rather than in a backlog.** The tilt→scHpf mapping
saturates at its +30 Hz bound as soon as the measured split is ~5 dB darker than the −6 dB
reference, and the split is a first-order 800 Hz one-pole that biases the measurement low on real
programme — so the trim can converge onto a RAIL while `testTrimBounds` (membership) and
`testAdaptationConvergesAndHolds` (convergence) both pass. The P6 listening pass owes a check that
the vector sits away from every bound on representative material. That check cannot be written
before the constants are tuned, which is why it is a comment at the mapping and not a test today.

**Nothing was done about the limiter's wedge item**, because it is the reviewer confirming the
comment written last round by tracing the code — the recovery bound, the reason domination cannot
remove a NaN entry, and what holds invariant 4 meanwhile all match. An independent trace agreeing
with a comment is the outcome the comment was written for.

### Twenty-first review round — ordering, a Release-only assert, and a guard that came back (2026-08-02)

Eleven items, most of them explicitly not defects. Three fixed, five recorded in code where a
reader will meet them, three already carried.

**The ordering fix generalises the previous round's.** The per-block repairs ran near the end of
`process()`, while `currentTrims()` — which reads state one of them repairs — is read earlier. The
reviewer verified the hole is not live (a NaN target fails the trim hysteresis, so a NaN trim is
never stored, which makes that branch of `sanitiseState` dead code today) and asked the right
question anyway: it stops being dead the moment the hysteresis changes, and a NaN trim would reach
`limiter.setStereoLink`, whose `SmoothedValue` nothing sanitises. Moved to the top of the block,
where the comment now says the position is load-bearing. Same lesson as the Learn commit: "the
sweep runs later in the same function" is a fact about call order that nothing enforces.

**A `jassert` that guards a shipping build guards nothing.** The dry ring is sized with
`kMaxOsLatencySamples` standing in for the per-config oversampler latency, and the only thing
tying the two together was a `jassert` — compiled out in Release, which is what ships. The tables
moved to namespace scope and a `static_assert` now binds them to the constant at compile time.
The same argument had already promoted the JUCE-vs-table comparison to an always-recorded flag;
this was the entry it missed.

**The meter's ring guard came back, and the reason it was removed was a bad stimulus, not a bad
guard.** Round nineteen removed it because no mutant could distinguish it. The review pointed out
why: the reasoning ("the absolute gate rejects NaN") holds for the integrated histogram and not
for the sliding windows, where a stored NaN reads for up to ~3.2 s and freezes the §2.7
compensation with it. The mutant survived because the test used a 512-sample block — the per-block
repair clears the accumulator before the 100 ms sub-block boundary about nine times in ten. At
8192 the boundary falls inside the poisoned block every time, and the mutant dies. **A guard that
no mutant kills is either dead code or an uncalibrated stimulus, and telling them apart is work,
not a judgement call.**

**Five things recorded rather than changed**, each next to the code it constrains: the §5.4
features re-converge over tens of seconds after a huge-but-finite excursion (no accumulator is
non-finite, so no repair touches it; bounding them is a mapping change); the limiter's wedge keeps
NaN entries until index expiry, so detector recovery is one window rather than one block; the
integrated histogram's top bin is a hard range limit above +5 LUFS; and one GR-history entry spans
the HOST block, so the P5 renderer needs a time base rather than an index count. `DSP_POLICY.md`
invariant 9 now carries the recovery times together, because "self-heals" had come to mean four
different latencies.

### Twentieth review round — the guard was right, and ran too late (2026-08-02)

Three items: one defect fixed, two repeats already recorded (one of which the review itself
classes as not-a-defect).

**The previous round added exactly the right guard in exactly the wrong place.** `sanitiseState()`
cancels a Learn pass whose sums went non-finite, so that `commitLearn()` cannot store one — but it
runs late in `process()`, while the staged Learn command is consumed at the block TOP. A commit
landing on the block after an overflow therefore gets in first: `refTiltDb` becomes NaN,
`publishRefs()` publishes it, `learned` goes true. From there the damage is permanent (every trim
target derives from the reference, and both `jlimit` and the hysteresis `|tgt − state| > deadband`
pass NaN through untouched, so the vector never moves again) and persistent (`hasLearned()` true
means the next save writes it into the session's `ADAPTIVE` child). Reproduced before fixing:
both assertions of the new test fail on the tree as it stood.

**Fixed at the writer, not by reordering the calls.** Moving `sanitiseState()` above the Learn
consume would work today and would be a fact about call order that nothing enforces — the same
class of argument as the reachability claim that put the Post-EQ hole in. `commitLearn()` now
refuses a non-finite measurement outright, with the outcome the empty pass already has (previous
reference stays, nothing published, `learned` not raised). `setLearnedTargets()` — the other
writer, reached by a session restore — gets the same check, so a file written by a build that did
commit one reads as never-learned rather than re-poisoning a healthy engine.

**The stimulus is the constant huge block, not the Nyquist one.** The extractor's Learn
accumulation is inside `if (learnActive && audible)`, and `audible` is `ms > 1e-7` — false for the
NaN that Nyquist-at-full-scale produces, true for the `inf` a constant block produces. The
previous round's stimulus, reused here, would have tested nothing.

**A warning caught by the build, not by review.** The first version of the test compared trims
with `!=`; `-Wfloat-equal` is on, and the project's gate treats warnings as findings. Replaced
with `juce::exactlyEqual`, the idiom the rest of the tree uses.

### Nineteenth review round — the failures that are invisible because they are not audio (2026-08-02)

Seven items: one defect fixed with a two-part regression test, three one-line consistency
corrections, three repeats left alone.

**A third class of stage, and it is the one the contract kept missing.** Rounds fifteen to
eighteen were about stages that produce a non-finite SAMPLE, which a boundary can catch. The
meters and the feature extractor produce no samples at all. Both are fed the delay-aligned dry
signal — finite by the input boundary, unbounded by anything — and both overflow on it: the
K-weighting shelf at ~FLT_MAX/3, the band-energy square at ~1.8e19. What follows is silent in
every sense: a NaN reading compares false against `>= -70 LUFS`, so the §2.7 compensation freezes
and the integrated histogram stops counting; a NaN feature fails the trim hysteresis
`|tgt − state| > deadband`, so the trim vector holds its last value for the session **and looks
entirely plausible doing it**. Measured: `tilt=-nan crest=-nan` and a frozen trim vector, 359
blocks after a single extreme block.

**Stimulus calibration decided the shape of the test, twice.** A constant huge block does NOT
poison the extractor's filter state — `bandLp` tracks toward it and stays finite; only the SQUARE
overflows, and that recovers. The state only breaks on a sign flip, where `x − bandLp` is
2·FLT_MAX: the stimulus is Nyquist at full scale. And the output meter is unreachable through the
programme path (the render tap is ceiling-bounded), so the case runs in **bypass**, where
`render = delayedDry` is the raw input. Each half dies against its own `sanitiseState()`.

**One guard was written, tested, and removed again.** The first draft also dropped a non-finite
sub-block mean before it entered the meter's ring. No mutant could tell the difference: the NaN
that this failure actually produces is rejected by the `lufs >= -70.0` absolute gate anyway, since
every comparison against NaN is false. The property is now an assertion in the test instead of a
branch in the meter — last round's lesson about unreachable defensive code, applied to my own
first draft rather than to someone else's line.

**What is NOT fixed, and is now written down.** After an extreme block the features re-converge
from a huge-but-finite value through the 1.5 s integrator, which can take tens of seconds of
wrong-but-finite tilt. Bounding the features to fix that is a §5.4 mapping change, not a repair.

**Three consistency corrections.** The limiter's detector-HPF design clamp was 20 Hz where the
compressor's identical filter uses 10 Hz — unreachable divergence today, aligned rather than
argued about. The wet ring's zero slack is now justified in place (its read offset is fixed, the
dry ring's is a three-term sum, which is what the spare slot there guards). And Learn's
accumulation now says that it sums the ~1.5 s integrated features on purpose, that the bias is
~1.5 s of pre-start material, and that the P5 grammar owes a minimum pass length rather than this
owing a re-seed.

### Eighteenth review round — the hole was in the sentence that said there was no hole (2026-08-02)

Six items: two defects fixed, one flag-coverage gap closed with them, three repeats left alone.

**The stage-E boundary I removed two rounds ago was load-bearing.** It was dropped on this
argument: *"Stage E needs no boundary of its own: its input is the limited signal, bounded by the
ceiling (invariant 4), so eqPost cannot overflow."* Wrong twice. The `CeilingClamp` runs **after**
the Post EQ — that is what ADR-0002 exists for — so it bounds the EQ's output, not its input. And
what actually bounds stage E's input is the limiter's **attack**, not the ceiling: at the default
2 ms lookahead the envelope is down to ~0.008 by the time an extreme peak plays, which is why the
first stimulus found nothing, but at 0.1 ms it has only ~5 samples and reaches ~0.29 — and a fully
boosted EQ multiplies by ~3.4. Measured with the engine instrumented: `postEqIn = 9.9e37`,
`out = 3.4e38`, 402494 non-finite samples, output `0.000000` for ever after.

**Two boundaries, not one.** The staging read at the top of stage E is the other missing one: with
oversampling on it carries `processSamplesDown`'s output, the decimation half of the same polyphase
filters the region read already guards, so a non-finite value there is attributed to the
oversampler and repairs it. That closes the flag-coverage asymmetry the same review flagged
separately — `regionInputNonFinite` covered the up-sampler only.

**The limiter's detector high-pass needed the opposite treatment.** Its corruption produces no
non-finite output to detect: `det` goes NaN, every wedge value goes NaN, and `peak > ceiling` is
**false** for NaN, so `needed` stays 1.0f and the limiter emits unity gain for ever — finite,
silent, and invisible to every boundary in the engine. A repair hung on a flag cannot fire for a
fault that raises no flag, so the state is checked unconditionally once per block instead. The
sanitisation added to `resetWindow()` last round moved there rather than being duplicated.

**Stimulus calibration was the whole difficulty, again.** The obvious Post-position test — a
+12 dB shelf at the default lookahead — passes against the bug, because the limiter really does
bound stage E's input under normal settings. The case only exists at a short lookahead, with the
peak detector (the RMS square collapses the level first) and every EQ band boosted. A test written
from the review's description alone would have gone green and closed the item.

**Lesson, and it is the same one twice.** Round fifteen removed a guard because a reachability
argument said the case could not happen; round sixteen found the recovery incomplete for a stage
the invariant text itself named. Both times the defect was inside carefully argued prose. The rule
this branch now carries in `DSP_POLICY.md`: a reachability argument is not a boundary — it is a
claim about today's chain, and it belongs in a comment next to a check, not instead of one.

### Seventeenth review round — an adaptive behaviour that is computed, published, latched and silent (2026-08-02)

Eleven items: one product question raised rather than answered, four documentation/comment
corrections, six repeats of items already recorded (three of them verbatim from the previous
round, against comments this branch already carries).

**The one substantive finding is a question, not a bug.** `LookaheadLimiter::setRelease` writes
`aRelManual`, which `processSample` reads only in the `else` branch of `if (autoRelease)`; the auto
path steps its two envelopes with fixed 40 ms / 600 ms constants. `limAutoRelease` defaults to on.
So the §5.4 release trim is computed, slewed, published, displayed as an overlay, latched by Freeze
and serialized — and inaudible at factory defaults, while the other three trims reach their stages.

**Why it was not simply wired.** Both readings are backed by documents this repository is governed
by. `DESIGN.md` §5.4 defines trims as deltas "around the current parameter values" and
`MODE_AND_ADAPTATION_POLICY.md` states that every trim "is inert while its host stage is inert" —
the property the invariant-7 null rests on — so an inert trim on an unused parameter is that rule
working. Against that, a release trim that cannot open up on sparse material is not delivering what
§5.4 exists for. Making it audible means scaling the two auto poles, which changes **what the
plugin sounds like at factory defaults**: a product decision, and `CLAUDE.md`'s standing rule is
that open decisions go to `OPEN_QUESTIONS.md` rather than being guessed at. **OQ-016**, with the
recommendation stated and the scope now written into the policy's "Current implementation"
section, `HANDOVER.md`'s P4 summary and the application site itself.

**Three corrections that are one sentence each.** Invariant 9 now says what "self-heals" costs —
the repair runs at the end of the chunk, so recovery is one chunk plus the lookahead line, not one
sample. `setTruePeakMode`'s history clear now states its own cost: ~12 region samples that
under-report, against an unbounded over-report if the stale window were kept. And a typo in the
delta-leg duck comment ("undicked") is fixed, in the sentence that is the load-bearing explanation
for why `dryForDelta` carries the duck gain.

**Six repeats, and what that says.** The clipper's engage edge (KI-005), the dynTilt trim sitting
at its clamp, the GR tap being the limiter's alone, the detector high-pass during a lookahead
glide, the bulk swap's two halves, the GR ring's rewinding index — all six are already recorded,
four of them in comments this branch added in the previous two rounds, and the ring's "generation
counter or never rewind" choice is already written into `THREAD_MODEL.md`'s planned edges. A review
reading a slightly older tree will re-raise what it cannot see; the answer is to check the current
file before re-fixing, which is why nothing was touched for those six.

**The check-count item was the reviewer reading the PR description, not the tree.** README and
HANDOVER both say 293 (206 + 87), which is what the suites print; the description said 282. The
description is what was stale, and it is the one place the "re-count from the output" rule does not
enforce itself, so it is updated with the push rather than left to drift again.

### Sixteenth review round — the recovery covered every stage the engine owns (2026-08-02)

Ten items: two defects fixed, one seed corrected, four comment/scope corrections, three already
recorded and left alone.

**The previous round's fix was incomplete in exactly the way its own text advertised.** The
rewritten invariant 9 names "a polyphase filter" among the stages that can generate a non-finite
value from a legal float, and the region-read boundary sets the flag for precisely that case — but
the repair block cleared `eq`, `comp` and `clip` and never touched the oversampler. Measured: at
`FLT_MAX` with x4 or x16 **minimum-phase** oversampling the plugin is silent for ever; the same
stimulus on the **linear-phase** path recovers by itself. That is the difference between an IIR
allpass chain, which feeds its own poisoned state, and an FIR, which flushes in a few samples.
`osActive->reset()` now runs on a separate flag set only by the region boundary — a full reset is
a discontinuity and must not fire for a fault another stage caused.

**A fix from the last round was itself a regression, and the review caught it.** `comp.reset()`
zeroes the gain-reduction envelope, so a block that tripped the recovery resumed at unity with no
release ramp — the exact effect `LookaheadLimiter::resetWindow` exists to avoid, argued at length
in its own comment. Replaced by `sanitiseState()` on the EQ, the compressor and the clipper:
clear the members that are actually non-finite, carry the rest. The envelope is cleared only when
it is itself NaN, which is the one case where there is nothing to carry. `primed` is no longer
disturbed either, so the smoothers no longer snap.

**Defence in depth where no stimulus reaches.** The limiter's detector high-pass is recursive and
fed from the wet ring, which the engine keeps finite but not bounded; `|b1| ≈ 2` means a ring
sample past ~`FLT_MAX/2` overflows it, and `resetWindow` sanitised the envelopes but not the
biquad. No probe in the suite reaches it — the compressor's identical sidechain filter sits
upstream on the same parameter and poisons first — so this is stated as an argument about today's
chain rather than a guarantee, and the two lines went in anyway.

**One measurement that looked like a defect and was not.** With the colour model at full depth, an
input around 3e7 leaves the output pinned at the ceiling for ≈2.8 s afterwards. Nothing is
poisoned: the colour DC blocker is a 5 Hz one-pole, and 87 time constants of decay from 1e35 is
exactly that long. Recorded because the first probe ran a 2.1 s tail and reported it as permanent.

**A seed that published a physically impossible number.** `AdaptiveEngine::reset` seeded
`msAvg = peakAvg = 1e-6`, but crest is `20·log10(peakAvg / sqrt(msAvg))` — two quantities a square
apart — so the published crest started at −60 dB and took seconds to climb out. Nothing consumes
crest today, so this is a P5 readout fix, one line, with the seeds now a square apart.

**Three enumerations corrected in place.** Invariant 12 said "exactly two legs sit downstream of
the quantiser" and there are three — the delta substitution subtracts the undithered dry from the
dithered processed signal. The GR tap is the limiter's alone, which the published meter, the
history ring and the §2.7 predict floor all inherit. The detector high-pass sees a
non-contiguous sample stream while the lookahead glides, the same class of cost the wedge already
records.

**Lesson.** Both defects this round were in the previous round's fix, and both were findable from
its own documentation: the invariant text named the polyphase filter the code forgot, and the
limiter's comment argued against the reset the new code performed. A fix that ships with a
carefully written rationale should be re-read against that rationale before it is called done —
the rationale is the most precise specification of the fix that exists.

### Fifteenth review round — the boundary protected everything except the stage that broke (2026-08-01)

Eleven items: one real defect fixed with a four-stimulus regression test, two documentation
corrections, eight already recorded in earlier rounds.

**The defect falsifies a comment written three rounds ago.** That round added the invariant 9
self-heal rationale: the limiter is the only stage repaired because *"the EQ biquads, the
compressor envelope and detector HPF, ClipSat's ADAA and filter states … all sit downstream of
one of those [sanitisation boundaries], which is why none of them is reset here: they cannot hold
a NaN to begin with."* The reviewer read the code instead of the comment and found stage A
sanitises **before** the EQ and only re-checks **after** the compressor, so the post-EQ value
reaches `comp.processSample` unchecked — and the same shape exists in the region, where
`ClipSat::processSample` consumes `region.getSample()` directly.

**Confirmed by measurement before anything was changed.** One block of `0.5 × FLT_MAX` with a
+12 dB shelf engaged, then ordinary programme material: output RMS `0.000000` for every subsequent
block, for ever. The mechanism is the one the review described — the EQ biquad overflows, the
compressor's `levelDb` goes infinite, its GR target goes to `-inf`, and the next sample's
`-inf + inf` is a NaN the envelope keeps.

**The load-bearing half of the fix is not the one the review asked for.** Adding the two missing
boundaries CONTAINS the contamination but does not remove it: the stage that overflowed still
holds the NaN, so the boundary keeps substituting `0.0f` and the silence is just as permanent. The
real hole is that a substituting boundary was **silent** — a NaN generated inside the chain was
swallowed with nothing set, so the existing self-heal never fired. The boundaries now record
(`stageGeneratedNonFinite`), and the three stages that can poison themselves are reset on that
record. Non-finite INPUT still costs no state: it is zeroed before the EQ, so it cannot set the
flag, and `testSelfHealDoesNotSnapTheEnvelope` — which poisons one input sample — is byte-for-byte
unaffected.

**Four stimuli, because each stage overflows on a different quantity and shields the next.** The
EQ needs `|x|` within a few dB of `FLT_MAX`; the compressor's RMS detector squares, so ~1.8e19 is
its ceiling; the colour model's `c⁵` term needs ~5e7 **and** drive at zero, because a driven
clipper's transfer function bounds its own output and protects the polynomial; the oversampler's
filters need the peak detector, or the compressor squares first and collapses the level before the
region sees it. Each case dies against exactly one element of the fix being reverted — three
resets and two recording boundaries, verified one at a time.

**The two containment boundaries are kept although no single mutant kills them.** They are what
confines an EQ overflow to the EQ: with the post-EQ boundary present, case 1 recovers on
`eq.resetState()` alone; with it removed, `comp.reset()` becomes load-bearing for a fault the
compressor did not cause. That is a property of a mutant PAIR rather than of one output, so it is
recorded here and in the code comment rather than asserted by a test that cannot see it.

**A guard was also removed.** The first draft sanitised after the ceiling clamp in stage E too.
Stage E's input is the limited signal, bounded by the ceiling under invariant 4, so `eqPost`
cannot overflow and the branch is unreachable — it went back out rather than shipping as a
per-sample cost with no reachable case. Unreachable defensive code is a claim about the chain that
nothing checks.

**Lesson.** "Sanitised at the boundary" describes what a value does, not what a stage holds. Every
boundary in this engine was written as a *propagation* control and then read, in a comment, as a
*state* guarantee — including by the author of that comment. The two are only the same for stages
whose arithmetic cannot overflow, which is none of them.

### Fourteenth review round — the decision index was still describing an empty repository (2026-08-01)

Eight items, all documentation or comments: three drift corrections, two residuals written into
the contract that already governs them, three repeats already recorded.

**The index that CLAUDE.md makes mandatory reading was the most wrong document in the tree.**
`ADR_INDEX.md` still ended its registry paragraph with "Anabasis has no `src/` and no `tests/`, so
every runtime claim is a contract the P1+ code must satisfy, not an observation", and every
sign-off ADR still read `Unverified (no src/ yet)` — while `DOCUMENTATION_COVERAGE.md`'s
module table in the same PR marks the modules those ADRs govern `Full | Verified`. A reader
following the mandated order (index first, then code) was told the codebase did not exist.

Fixed by doing the assessment rather than flipping a column: each row now carries the **named test
that discharges it** — ADR-0002 → `testLimiterPushDoesNotDriveTheClipper` + the EQ-position pair,
ADR-0004 → `testReportedLatencyMatchesImpulse` + `testOsLatencyMatrix`, ADR-0009 → the −3.01 LKFS
compliance vector, and so on. Three stay **Partially Verified** with the unwired half named:
ADR-0005 (the detach/re-engage gesture grammar is P5), ADR-0007 (the FROZEN_TRIMS inject is
OQ-013), ADR-0011 (OQ-014 and the KI-003 `replaceState` race, plus ADR-0012's amendment). The
same stale reasoning in this file's own gaps list is corrected to match. Upgrading confidence
without an evidence citation is what the audit rules forbid; upgrading it *with* one was simply
owed and had been outstanding for four phases.

**Two residuals of last round's Learn fix, both written into the contract that governs them.** The
reviewer found the composition's own consume-then-read window — a `requestLearnStart()` landing
between the consumer's `exchange` and its payload read sees `pending == false`, composes a bare
start, and drops the outstanding commit. That is ADR-0012 Known-limits #1 with a *composing*
writer, a case limit 1's wording did not cover, so it is now limit #2 in the ADR itself. And my
justification for collapsing start→stop to a bare commit ("a commit with nothing accumulated is
the documented empty-pass no-op") was too broad: it holds only when no pass was running. With one
already running, that collapse commits ITS statistics — a valid reference, not the one the user's
two clicks described. Both now stated at the site instead of implied by "last-writer-wins".

**A new document re-introduced the pattern the same PR had just retired.** `REALTIME_SAFETY_AUDIT.md`
shipped with `file:line` citations for the allocation sites, and every one had drifted by the time
it was next read (`pushArr` did not exist when they were written). Converted to symbol-based
citations, with the rule stated in the file so the next author does not re-learn it: **a line
number in a document is an assertion nobody re-runs.** That makes three files that have now
learned this — THREAD_MODEL, this audit, and the coverage table.

**Recorded, no change:** the §2.7 measure gates on momentary (400 ms) while its value is a
short-term (3 s) difference — deliberate, not a mismatched window: the gate answers "is there
programme here", and between 0.4 s and 3 s both short-term readings are the same sentinel so their
difference is exactly 0 and only the predict floor acts, which is the documented split. The two
meters advance in lockstep, so one can never be valid while the other is silent.

**Repeats already handled:** the ClipSat engage edge (KI-005 — attempted, measured, reverted), the
GR-ring index rewind and the Learn-commit-without-audio gap (both in the P5 planned-edge scope),
and the dynTilt third mechanism (landed two rounds ago in both files).

Suites: 198 + 87, unchanged — nothing behavioural in this round.

### Thirteenth review round — the same two-flag defect, in the path that was left behind (2026-08-01)

Nine items: one defect fixed, two doc drifts corrected, six already handled in the previous two
rounds (KI-005, the GR-ring index, the Learn-commit mirror gap, the verified duck/rings, the
dynTilt third mechanism, the ceiling ZOH).

**The defect is the one this PR already fixed once, in the path that was not touched.** Learn
start/stop were two independent flags consumed in a fixed order: `learnStartReq` then
`learnStopReq`. A stop followed by a start inside one audio block left both flags up, so
`startLearn()` ran first — zeroing the accumulator the stop was about to commit — and
`commitLearn()` then no-opped on `learnBlocks == 0`. **Both commands were lost**: the finished
pass never committed and the new one never began, with nothing to tell the user. The adaptive
restore had exactly this shape and was rewritten as a staged record three rounds ago; the Learn
commands were left in the older form because nobody re-read the neighbouring code when the class
of defect was identified.

**Fixed on ADR-0012's row, which already authorises it** — no new mechanism, no gate item: one
`learnCmdCode` payload behind one release-stored `learnCmdPending` flag, acquire-exchanged at the
block top. The one thing a staged record cannot carry is ORDER, and that information exists only
on the writer's thread, so the composition happens there: a start arriving on top of an unconsumed
commit becomes a single **commitThenStart**. Everything else degrades to last-writer-wins, which
is correct because a commit with nothing accumulated is the documented empty-pass no-op — so
start→stop in one block ends with nothing running and nothing spuriously committed, which is what
the user asked for. ADR-0012 condition 5 (the writer may read the flag back) is exactly what
makes the composition legal.

**Two test-craft notes from this round, both worth keeping.** The first draft asserted that the
committed onset reference had MOVED off its 4.0 factory default — and it failed, because a steady
sine's own startup transient (envFast rising faster than envSlow) leaves the onset feature near
that value by coincidence. The replacement asserts `hasLearned()`, which is not a weaker proxy but
the exact property: `commitLearn` latches only when `learnBlocks > 0`, so it can be true only if
the accumulator survived. And a python edit removing the now-unused `ref0` line hit the FIRST of
two identical lines — the one belonging to the neighbouring Learn test — which the compiler caught
immediately. Both are instances of the same discipline this file keeps recording: assert the
property, not a correlate of it; and anchor an edit on something unique.

**Doc drift, both introduced by this PR and both in files it edited.** DSP_POLICY's invariant→test
map still had row 1 (chain order) at `TODO (P2)` while this PR's headline defect — the limiter
push applied before Clip/Sat — was fixed and pinned; the row now cites
`testLimiterPushDoesNotDriveTheClipper` plus the two EQ-position tests and reads **live**.
REPOSITORY_MAP still advertised `ADR-0001…0011` although ADR-0012 landed in the same PR that
rewrote the lines immediately below it. Both are the "touched the paragraph, missed the sentence"
pattern the CLAUDE.md entry-point drift already illustrated two rounds ago.

Suites: 198 + 87.

### Twelfth review round — a fix attempted, measured, and reverted (2026-08-01)

Ten items: one confirmed defect recorded rather than patched, seven notes/corrections, two
verifications.

**The defect: leaving 0 dB clip drive steps the transfer.** The sub-block is skipped exactly at
0 dB (the bit-identity contract); one sample later the ADAA-1 branch runs, and in the curve's
linear region its divided difference is `(u + u_prev)/2` — so the stage swaps *identity* for a
`(1 + z⁻¹)/2` FIR in one sample. The reviewer's key observation is the one that matters: the step
is proportional to the signal's **slew**, not to the drive, so smoothing `driveDb` cannot shrink
it, and the existing header note about `adaaPrev` covers only the memory VALUE, not the transfer.
Confirmed against the arithmetic; it is an invariant-8 violation on a reachable knob move.

**A fix was written, tested, and reverted — the reason is worth more than the patch.** Blending
the ADAA output toward dry over the first 0.5 dB of drive makes the engage continuous and keeps
the exact-zero skip intact. It also broke `testClipCurveAndCompensation`, which deliberately uses
**0.001 dB drive with a unity-amplitude signal** to isolate the knee shape: at that setting the
clipper must shape (the signal reaches the knee on its own), and a drive-keyed blend removes
precisely that. The blend conflates "how hard we drive" with "how far into the engage we are" —
they are different axes, and the test was right. A correct fix needs a TIME-based engage ramp
(~20 ms) that keeps the ADAA branch running while it fades out, primed like the other smoothers,
landing on exactly 0/1 so the bit-identity skip survives: new state, a changed skip condition, and
its own coverage. That is ⊕ tuning-pass work, not a review-round patch, so the tree was restored
byte-for-byte and the defect is **KI-005** with the failed approach recorded in it.

**This is the round's real lesson, and it is the standing instruction working as intended.** Nine
rounds of this PR have shown the pattern: rounds 7→8→9 were each a defect introduced by the
previous round's fix. Attempting this one anyway — with an existing test as the tripwire — cost
one build and left no residue. Reverting on a broken test rather than tuning the constant around
it is the difference between recording a known issue and shipping the tenth consequential change.

**Two figures reconciled.** The true-peak estimator's worst-case reporting lag is **6** input
samples, not the 5.5 quoted in four places: the nominal FIR group delay is (12−1)/2 = 5.5, but each
call returns the maximum over `x[n−6]` and the interpolated points at n−5.75/−5.5/−5.25, so the
oldest sample an estimate can describe is n−6 — which is the number a lookahead-margin argument
must use. `TruePeak.h`, `LookaheadLimiter.h`, TEST_REPORT and RISK-008 now agree, and the
"~23 % of a 0.5 ms window" figure becomes ~25 %.

**Invariant 4 gains its third leg.** Dither runs after the clamp by ADR-0002's explicit order, and
TPDF rounding can put a sample ~1.5 LSB above the ceiling — 0.004 dB at 16-bit and the lowest
ceiling, two orders inside the invariant's own ≤ 0.1 dBTP tolerance, and zero with dither off
(which is the configuration its test runs in). The strict reading of "never exceeds" would
otherwise be falsified by the stage the ADR deliberately puts there.

**Load-bearing facts written where they are load-bearing.** The `ceilArr` zero-order hold is not
merely tolerated: the region's gain computer and stage E's clamp must use the SAME instantaneous
ceiling, which is exactly `CeilingClamp`'s "backstop, never a second differently-timed threshold"
contract — interpolating it across the region would break that, so the comment now says "do not
improve it". `activityEnv` being bit-zero is load-bearing from a file that never mentions the
adaptive engine: the dynTilt trim reaches its +0.5 dB clamp with features un-converged, so the
null rests on the tame branch never being taken — a THIRD inertness mechanism, distinct from the
arithmetic one (release/link) and the detector-threshold one (scHpf), and the AdaptiveEngine
header no longer lumps it in with the first.

**Also recorded:** the scHpf trim's steady-state trade (a detector high-pass under-reports
low-frequency peaks, so bass transients reach the clamp instead of the limiter — the failure mode
changes from limiting to clipping, inside the declared bound and therefore intended); the
Learn-commit direction's mirror gap (a stop with no further audio leaves the pass uncommitted, and
unlike the restore direction it is NOT mirror-fixable — the sums live on the audio thread, so the
P5 Learn grammar owes an acknowledged commit); the GR ring's reset rewinding its monotonic index
and bulk-clearing against a `const` peek, folded into the same P5 reader-contract decision as the
meter holds; and the confirmation that the latency-table tripwire holds because one
`FetchContent_MakeAvailable(JUCE)` feeds every target, so the suite verifies the revision the
release ships.

Suites: 195 + 87 (unchanged — nothing behavioural landed this round).

### Eleventh review round — the absolute claim, and two comments that outlived their code (2026-08-01)

Eight items (two of them the same defect reported twice): one scope correction with its code
comment, two comment-drift fixes, one cheap publication fix, two notes recorded, one stale
external artefact.

**The invariant-12 scope sentence I wrote two rounds ago was wrong, and it was wrong by being
absolute.** It said the monitor gain is the only leg downstream of dither and therefore "no render
is affected: an exported file is on the 2^-15/2^-23 grid exactly". The §2.8 **bypass crossfade**
is also downstream of dither, and unlike the monitor gain it is not gated on `nonRealtime` — a
host automating `bypass` inside an offline render emits ~10 ms of samples that are a convex
combination of the dithered wet leg and the UNDITHERED dry one. Both endpoints take exact
branches, so every steady state is on the grid; the ramp is not.

**Fixed as a scope correction, not a DSP change, and the reason is invariant 7.** The reviewer's
first option — move the crossfade upstream of dither — would send the dry leg THROUGH the
quantiser, and invariant 7 requires bypass to be a bit-exact null. That trade is strictly worse: a
bounded off-grid ramp on an audition toggle against breaking the null that the bypass test pins.
The invariant now enumerates both post-dither legs with what each costs, and the crossfade carries
the same reasoning inline. Lesson: an invariant amended to carve out one exception should be
checked for OTHER instances of the same shape before the word "no" is written — the carve-out I
added for the monitor gain read as a survey and was a single case.

**Two comments outlived the code they described**, both from changes I made in earlier rounds:
- The "one deliberate dropped duck request" note still claimed the first-block path was the only
  case. Since the offline-flip fix, `enteringOffline` shares that branch, so a bulk swap landing
  on the flip block is also dropped. Behaviour was already recorded in KI-004; the in-code text
  now names both cases and points there.
- MODE_AND_ADAPTATION's new Learn-cancellation sentence said "sample-rate change, host stop".
  `AdaptiveEngine::reset()` is reachable only through `prepare()` — the processor deliberately
  does not override `AudioProcessor::reset()`, which THREAD_MODEL states in the same PR — so a
  transport stop cancels nothing. Narrowed to what is reachable, with the override left as the P5
  question it belongs to (it also governs the delay-line tails and the meter holds). The code
  comment carried the same overclaim and is corrected with it.

**One cheap publication fix.** `prepareToPlay` cleared `dbTpMaxHold`, the GR ring and the engine's
meters but left the six published atomics holding the previous session's readings — until the next
block completes, and indefinitely if the host prepares without processing (rate change while
stopped, plugin rescan). No reader exists before P5, which is exactly why it is cheap now and a
stale-peak bug report later.

**Recorded, not changed:** ADAA-1's `(1 + z⁻¹)/2` divided difference low-passes the WHOLE
programme whenever drive is non-zero (cos(πf/fs), ≈2 dB at 10 kHz — the same droop the
oversampling test measures as a +1.3 dB "recovery") and adds a half-sample group delay
`Latency.h` does not model; the impulse-position test stays sample-exact only because
`clipDriveDb == 0` on that path. Both now in the ClipSat header and TEST_REPORT. And the staged
mirror's pairing invariant is stated at its only writer: a future stager that raises
`adaptivePending` without updating the mirror would have `getStateInformation` serialize the
stale one. A helper would make that unbreakable; deliberately not refactored this round, since the
hazard is future and the wrapper's state path is the highest-consequence code in the file.

**External artefact:** the PR description still quoted 223 checks (151 + 72) from an early round.
The suites' own output is the rule of record and says 195 + 87; the description is corrected to
match rather than the docs being bent to it.

Suites: 195 + 87 (unchanged — the only behavioural change publishes values no reader consumes).

### Tenth review round — a reset that cleared the features but not the pass measuring them (2026-08-01)

One item, fixed. `AdaptiveEngine::reset()` zeroed the trims, the features and the block
accumulators but left `learnActive`, `learnOnsSum`, `learnTiltSum` and `learnBlocks` standing.
`AnabasisEngine::reset()` — and therefore `prepare()` — calls it, so a sample-rate change or host
stop **during** a Learn pass kept accumulating into the same sums across the discontinuity while
`onsetRate` restarted from zero. The committed reference was then a mix of pre- and post-reset
statistics plus a stretch of re-converging onset rate: a wrong answer that looks like a
successful commit.

The pass is now **cancelled**, not paused: `learnBlocks == 0` makes the next commit a no-op, which
is the already-documented empty-pass path, so the reference the session already had survives
untouched. `learned`, `refOnsetRate` and `refTiltDb` are deliberately NOT cleared — they are the
answer a previous commit or a session restore established, and a rate change is not a reason to
forget it. The reviewer's framing was right that this needed deciding rather than defaulting:
"clear everything" would have discarded session state, "clear nothing" is the defect.

Test uses a steady sine — no transients, so a pass that commits lands the onset reference near 0
against the 4.0 factory default, making "did the cancelled pass commit?" a disjoint question
rather than a tolerance one. Mutation-verified: removing the four lines fails both checks.

Not touched, per the round's scope: the bypass/dither grid question, the published meters' reset
lifecycle, a staged-mirror helper refactor, and the zero-length-block guard.

Suites: 195 + 87.

### Ninth review round — the second-order consequence of making a branch reachable (2026-08-01)

Ten items: four fixes, two recorded, four verifications/repeats.

**The EQ state fix is the same class of miss as last round's, one level deeper.** The direct-adopt
branch assigns `appliedEqPos` without the `eq.resetState()` its silent-bottom twin performs. That
was harmless for as long as the branch was only reachable immediately after `reset()`, which has
already cleared the biquads — and round seven made it reachable **mid-stream** by routing the
offline-entry edge through it. So a bounce started on the exact block the EQ position changes
began with the other position's filter history, which is the rule `MasteringEQ::resetState`'s own
comment states. Fixed by pairing the two operations on this branch as well.

The pattern across rounds seven → eight → nine is worth naming: widening a branch's reachability
is not a local change, because every *omission* inside it that was safe under the old
precondition becomes a defect under the new one. Round eight caught the branch running in the
wrong direction; round nine caught what the branch fails to do now that it runs mid-stream. The
check that would have caught both at once is to re-read the whole branch body against the new
precondition, not just the condition guarding it.

**Test design, since "no artefact" is hard to assert directly:** the input goes SILENT at the flip
and Force Max changes the factor, so `latchOsConfig` empties the lookahead ring — everything
downstream of the region is then fed exact zeros and the Post EQ is the only thing that can
produce a nonzero sample. Clean state gives exactly 0.0; stale state rings out the charged
history. Mutation-verified against the missing `resetState`.

**Three smaller closures.** The wrapper's staged ADAPTIVE mirror is now three atomics: writer and
reader are both nominally the message thread, but KI-003 records that VST3 does not promise which
thread delivers `setStateInformation`, so a concurrent save could read a half-written mirror —
it is a new instance of KI-003's own shape, and the entry now says so. `lastNonRealtime` and
`grMinLinear` are cleared in `reset()`: neither changed behaviour today (the first is shadowed by
`smoothersPrimed`, the second is one 200 ms-smoothed monitor target), but both made a reset-scoped
invariant depend on something outside `reset()` — the GR tap in particular carried the previous
session's last reduction into the first block's §2.7 predict floor.

**Recorded, not fixed:** entering offline abandons an in-flight duck at its current gain (a step
bounded to the first sample of a render, and the alternative — carrying a monitor fade into a
bounce — is worse), now in KI-004 with the other flip-window costs.

**Verified by the reviewer, no action:** ring sizing and tap offsets against their worst cases,
again. Repeats already recorded: the metering CPU budget (P6), the ZOH push idiom (noted at the
site last round), the session-cumulative meter holds (P5), and the `bool` return conflating three
early-return conditions.

Suites: 193 + 87.

### Eighth review round — the previous round's fix broke the other direction (2026-08-01)

Eight items: one regression fixed (mine, from the round before), one hardening, six
verifications and repeats needing no change.

**The regression, stated plainly.** Last round's `offlineFlip` flag was written as
`p.nonRealtime != lastNonRealtime` — direction-agnostic — so the OFFLINE→REALTIME edge took the
direct-adopt branch too. That edge lands in **live playback**, where direct adopt calls
`latchOsConfig` (clearing the lookahead ring and resetting the oversampler) at FULL gain: ~11 ms
of exact silence followed by an abrupt resumption. That is the click DSP_POLICY invariant 8 names
for precisely this switch — the fix for one edge introduced the defect on the other. It is now
`p.nonRealtime && ! lastNonRealtime`; the return edge goes through the §2.8 duck like any other
factor rewire, and both directions have their own test (`testOfflineFlipDoesNotDuckTheRender`,
`testReturnFromOfflineIsDucked`), each mutation-verified — the second one kills exactly last
round's expression.

**Why my own testing missed it.** The round-seven test rendered a single flip into offline and
stopped there; the return edge was never exercised because the *reported bug* was about bounces.
A test written from the bug report tests the bug report. The symmetric case — flip there and
back — costs one extra line of loop condition and is now what the offline tests do. Second
instance this session of a fix whose blast radius was wider than the case that motivated it (the
first was the limiter-smoothing change silently un-killing the stale-detector mutants), and the
cheap guard is the same both times: after changing a condition, ask which OTHER inputs reach it.

**Hardening: `currentTrims()` is now private with `AnabasisEngine` a friend.** It returned a
reference to the plain `Trims` struct that `finishBlock` mutates, sitting in the public surface
the wrapper hands to message-thread callers through `adaptiveReadout()` — the same shape that
produced the `learned` and `learnActive` races, two rounds and one round ago respectively. The
readout surface is now atomics-only **by construction** rather than by convention; the engine
(the only in-tree caller, on the audio thread) reaches it through the friendship and the P5 UI
reads `publishedTrim*()` like every other display value. Third instance of this pattern, and the
first one fixed before a caller existed rather than after.

**Recorded, no change: the push is a zero-order hold inside the region.** Carrying the smoothed
gain per BASE sample and holding it across all `osN` region samples makes it piecewise-constant
rather than band-limited, which puts images around multiples of the base rate while the 20 ms
glide runs. They sit above the decimation cutoff and are removed on the way down, so the steady
state is unaffected — but the `ceilArr`/`wArr`/`pushArr` idiom is the obvious one to reuse for the
next level-affecting control inside the region, where a slower glide or a higher factor could put
an image under the cutoff. Noted at the site rather than left for someone to rediscover.

**Verified by the reviewer, no action:** the ring sizing and detector tap offsets against their
worst cases (wet ring, dry ring, `wOs ≤ maxWindow`), and the exhaustiveness of duck-request
consumption across the four block-top branches — the only discarded request being the documented
first-block path. Repeats already recorded: the metering CPU budget (P6), the session-cumulative
meter holds surviving state loads (P5, scope now in THREAD_MODEL), and the `bool` return
conflating three early-return conditions (the header enumerates them; nothing needs to tell them
apart today).

Suites: 191 + 87.

### Seventh review round — a fade at the head of every Force-Max bounce (2026-08-01)

Ten items: two code fixes, seven notes/corrections, one repeat.

**The render-path defect.** `effectiveFactor` depends on `nonRealtime` (Force Max forces 16×), so
the realtime→offline flip changes `wantIdx` and `rewireWanted` goes true with `smoothersPrimed`
already set from the realtime session — the engine ducked out, latched, and held
`bottomHoldSamples` before recovering. A host that calls `setNonRealtime(true)` and renders
**without an intervening `prepareToPlay`** therefore wrote ~45 ms of fade into the head of the
bounce. Probed before deciding: the render drops to EXACT silence, so it is reachable through the
engine's own API contract, not just in theory. A realtime↔offline flip is now treated as the
reset-class event it is — direct adopt, exactly like the first block after prepare — so the
no-re-prepare path behaves like the re-prepare path most hosts take. What remains at the flip is
the pipeline refill, which is honest latency: the host re-reads PDC across the flip
(`setNonRealtime` is an ADR-0004 recompute trigger) precisely because the factor changed.

**Why the offline tests missed it:** every existing offline test either prepares in the offline
state or flips `nonRealtime` while the factor is unchanged (`forceMaxOffline` false). The
combination that breaks — Force Max, a non-16× selection, and a flip with no re-prepare — was
untested because each ingredient was individually covered. Mutation-verified.

**`learnActive` promoted to an atomic**, joining `learned` from round four. It is written on the
audio thread and read by `isLearning()`, which the wrapper hands to message-thread callers through
`adaptiveReadout()`. Nothing polls it today — the P5 Learn indicator will, which is exactly when
it would have become the same data race that was fixed for `learned` two rounds ago. Fixing the
second instance of a pattern before its caller exists is cheaper than rediscovering it.

**CLAUDE.md said P1.** The entry point every contributor and agent reads first still declared
"Current phase: P1 (skeleton)" while README said P1–P4 complete and HANDOVER's status row said P4.
The previous round edited that very paragraph — for the ADR count — and left the phase line, which
is the most avoidable kind of drift: touching a sentence is the moment to check the rest of it.
Now states the phase, the blocked item, and points at HANDOVER as the status of record.

**Corrections where code and comment disagreed.** The §2.7 predict floor is corrected by the
previous block's DEEPEST reduction (`grMinLinear` is a per-call minimum), not the "block average"
the comment claimed — the floor is therefore slightly more aggressive than documented, which is
the safe direction for a monitor-only attenuation, but the text now matches the code. The limiter
style's alpha-domain approximation names its true worst case: the shortest release at the LOWEST
engaged rate (1 ms at 44.1 kHz with OS off, 1.3 %), not the 48 kHz figure quoted last round.

**Implicit couplings made explicit.** `ClipSat::setRate` now says it MUST be followed by `reset()`
and why: the snap it performs would let `depth`/`driveDb` leave zero in one step, and both
skipped-branch arguments in that file (cold colour filter states, pre-drive ADAA memory) rest on
those two moving only through their 20 ms glide. `latchOsConfig` pairs the calls; a future caller
that does not would break both at once.

**Budget note promoted from a comment to TEST_REPORT.** Stage E now runs ~6 biquads + ~72 MACs per
frame of metering on top of the chain, and DESIGN §9's ≤ 0.5 % allocation is the binding
`Unverified` number for the subsystem — with an explicit "measure before adding another per-sample
tap", since the P5 spectrum rings are next. THREAD_MODEL's meter-hold-reset planned edge gained
its full scope: the session-cumulative holds also survive `setStateInformation` and
`AudioProcessor::reset()`, which a "GUI reset button" description does not obviously cover.

Suites: 189 + 87.

### Sixth review round — the push was in the wrong stage, and only a clipper could show it (2026-08-01)

Fourteen items: one real DSP defect, two small structural fixes, ten notes/scope corrections, one
repeat of a known P5 gap.

**The defect is a chain-order deviation, and it is the kind this project is built to prevent.**
`limGain` — the limiter's drive, and the macro layer's primary push (`18·l^1.2`, up to +18 dB) —
was multiplied into the signal in stage A, right after the compressor and BEFORE `processSamplesUp`
and `ClipSat`. Invariant 1 / ADR-0002 put the limiter after Clip/Sat, DESIGN §2.5 defines `limGain`
as the gain that drives the limiter's fixed threshold, and **the engine's own header comment**
states the intended layout verbatim: `OS region: [up ×N] → Clipper/Sat → limiter push → 10 ms
lookahead line → LookaheadLimiter`. Because ClipSat clips against a fixed unity threshold scaled
only by `clipDrive`, every dB of push moved the clip point down by a dB: at +18 dB, material 18 dB
below the intended clip point saturated. On the default Simple-mode path.

**Why five review rounds and 185 checks did not catch it.** The all-defaults null has
`clipDrive = 0`, and ClipSat skips its whole sub-block at exactly 0 dB drive — so the two gains'
ORDER is unobservable on every existing null, bypass and latency test. The property needs a test
where the clipper is doing real work and the two gains are told apart: input gain +12 with push 0
against input gain 0 with push +12, stimulus low enough that the compressor is inert in both.
Correct code puts the clipper at 0.05 in one run and 0.20 in the other (h3 at −127 dB against
−17.7 dB); the buggy code feeds it 0.20 in both and the renders are bit-identical. 110 dB of
separation, asserted at 20, mutation-verified by restoring the old placement. The fix carries the
push per base sample in `pushArr` (the `ceilArr`/`wArr` idiom already there) and applies it inside
the region after `clip.processSample`, with the exact-1 skip that keeps the null bit-exact — all
187 + 87 checks stayed green through the move, which is what "restructure, then extend" is for.

**Lesson worth keeping:** a stage that is *skipped* at defaults hides the ordering of everything
around it. When a bypassed-at-defaults stage lands, the tests that must be written are the ones
that engage it — the null tests get *weaker* at exactly that moment, not stronger.

**Two structural fixes.** `AnabasisEngine::process` now returns `bool` (false = short-circuited),
and the wrapper's publish guard asks instead of re-deriving the early-return condition — the
previous round's guard already had drifted, missing `ringSizeOs <= 0`. And stage A clears the
staging rows above `nCh` so a caller handing fewer channels than `prepare()` was told cannot have
last block's leftovers filtered through the oversampler (test-only today; `isBusesLayoutSupported`
enforces stereo).

**ADR-0012 gains a "Known limits" section** rather than leaving the reviewer's two coherence
observations as folklore: the consume-then-adopt window (a save landing between the flag
`exchange` and the adoption reads pre-adoption values — one save's worth; closing it trades for a
lost-update window, so it is not closed) and the one-block torn pair if a second restore lands
between the flag exchange and the payload loads (self-correcting at the next block top). Both are
now part of the contract's text, which is where OQ-015 asked for them.

**Scope and comment corrections, no behaviour change.** The §2.9 render tap states its deliberate
inclusion of the duck (meters report what was EMITTED; the alternative describes audio nobody
heard) with the cost named. `AdaptiveEngine`'s "structural, not a gate" header now separates the
three trims that are inert by arithmetic from `scHpf`, whose inertness rests on the detector
staying below both thresholds — a property of the stimulus, which is why `testNullWithDefaults`
asserts its own precondition. `commitLearn` records that an empty Learn pass leaves an older
learned state live with no signal back to the caller (a P5 UI item, not a P4 defect). `ClipSat`'s
ADAA memory note says why storing the pre-drive sample is sound only while drive is smoothed.
`switchToSlot` no longer claims the duck covers the whole glide — it covers all but the first
~6 ms, since the audio thread reads parameters before the flag. `LoudnessMeter` records the
relative gate's 0.1 LU bin quantisation. TEST_REPORT records what the true-peak estimator's
5.5-sample group delay costs the shortest lookahead setting (~23 % of a 0.5 ms window) and why it
is recorded rather than compensated.

Unchanged and repeated from previous rounds: the meter-hold reset stays a P5 planned edge, and the
integrated-LUFS histogram walk stays a P6 CPU-budget note. Suites: 187 + 87.

### OQ-015 decided, and the last scope sentence — ADR-0012 (2026-08-01)

Two items, both settled without touching a line of DSP.

**The owner took option 1: ratify the implementation.** `ADR-0012` — numbered 0012, not 0015,
because `ADR_POLICY.md` rule 6 makes the ADR sequence follow the highest existing ADR and the
question sequence is independent — adds a **GUI → Audio bounded staged record** row to
`THREADING_POLICY.md` with six mandatory conditions: bounded and fixed at compile time · one
writer (off the audio thread), one consumer · payload relaxed, then one flag release-stored,
consumed `exchange(acquire)` **at a block top** · last-writer-wins only, never a queue · the
writer may acquire-load the flag to test consumption (this is what lets `getStateInformation`
serialize a staged-but-unconsumed record) · the consumer only adopts. The learned-target restore
is ratified **unchanged** as the first instance, and `AdaptiveEngine::learned` as its Audio→GUI
mirror. `THREAD_MODEL.md`'s two rows now cite ADR-0012 instead of "no row — see OQ-015"; the
sentinel row's exclusion sentence now points at the new row rather than at the Gate.

**What the ADR deliberately did not do**, because ratifying a mechanism is not the same as
approving every use of it: OQ-013 stays **open**. Its trim vector now has a permitted transport —
the mechanism objection is gone — but whether a restored vector may be injected into a running
engine, and what that does to the adaptation state machine, is a product question nobody has
answered. The Hard Stop stands; only its reason changed, and both OQ-013 and `THREADING_POLICY`'s
blockquote now say so explicitly rather than leaving a reader to infer that ADR-0012 unblocked it.

**Invariant 7 gets the third scope sentence.** Invariants 4 (ceiling) and 12 (dither last) were
scoped to the programme path in the previous two rounds; invariant 7's "bypass is a null test"
was still unqualified while the §2.7 monitor gain is applied POST-mix — deliberately, since that
is exactly what makes A/B against bypass loudness-matched. So the bypass null is bit-exact with
the monitor functions off and bit-exact in every render (both are snapped inert offline), and a
null measured with Loudness Comp engaged is measuring the monitor. Stated, with the three tests
that guard each half named. That completes the set: every invariant whose text could be read as
covering an audition-only leg now says which side of the line it sits on.

**Process note worth keeping.** Three consecutive review rounds each found one unqualified
invariant, in descending order of obviousness. The invariants were written at P0 against a chain
that had no monitor layer; the monitor layer arrived at P3 and nobody re-read the invariant list
against it. A new subsystem that sits downstream of the render path is a prompt to re-read every
invariant, not just the ones it obviously touches.

### Fourth P4 review round — a save that ran before the audio thread, and a threading path that never passed the gate (2026-08-01)

Twelve items: two defects fixed, one **Hard Stop escalated rather than patched**, six
comment/scope clarifications, three confirmations that needed no action.

**The Hard Stop is the important one.** The §5.4 learned-target restore stages two floats plus a
discriminator behind a separate release-stored flag. `THREADING_POLICY.md`'s sentinel row excludes
that shape verbatim — *"anything unbounded, wider than one lock-free scalar, or needing ordering
against other state is **not** this row"* — and the table closes with *"any path not in this table
is a new cross-thread path → Architecture Review Gate."* It was built at P4 by analogy to the GR
ring's release/acquire pair, which is an **Audio→GUI** row and does not authorise a **GUI→Audio**
record; two rounds of review (including mine) then documented it under invented row names, which
made it read as authorised. It is now `OPEN_QUESTIONS.md` **OQ-015** with three costed options
(ratify with a small ADR / re-express as sentinel slots and accept a tearing window / defer the
restore to P5), both THREAD_MODEL rows are marked **"no row — see OQ-015"** exactly as the
MacroEngine edge is under OQ-014, and the shape is frozen — no further staged fields, no second
record, and OQ-013's trim transport still cannot be wired. The code ships meanwhile because it is
tested and reverting it unreviewed would be a larger unreviewed change than leaving it.
**Lesson, and it is the one worth keeping from this round:** "it uses the same memory-ordering
primitives as an approved mechanism" is not the same claim as "it is an approved mechanism", and
writing a plausible row name into the architecture doc is how the first claim gets mistaken for
the second. The MacroEngine edge was handled correctly a phase earlier; the pattern existed and
was not followed.

**Learn was lost by a load-then-save with no audio between.** `setStateInformation` only STAGES
the ADAPTIVE record; the engine adopts it at the next block top, and `getStateInformation` gated
the child on the engine's `hasLearned()`. A host that duplicates a track, copies plugin state, or
opens a project and re-saves without transport therefore serialized the engine's one-session-stale
answer: the child omitted (Learn silently gone) and, in the mirror case, an old learned child
resurrected over an un-learned session. The wrapper now mirrors the staged record on the message
thread and prefers it while `adaptiveRestorePending()` is true; once consumed the two agree, so a
concurrent consume hands back the same values. The existing round-trip test hid this by running
two blocks before re-saving — a test that models the *unhurried* path only.

**A short-circuited block still published.** `AnabasisEngine::process` returns early on zero
samples/channels without touching the render-tap values, and the wrapper published anyway:
previous-block peaks re-reported, a duplicate GR-history entry pushed, breaking the
one-entry-per-processed-block property `testMeterPublication` itself asserts. Guarded, with the
zero-length block now in that test.

**Scope clarifications, all doc/comment-only.** Invariant 12 ("dither is the last stage before
output") now carries the same scope sentence invariant 4 got last round: last on the PROGRAMME
path — the §2.7 monitor gain is applied after it, post-mix, and is snapped inert offline, so no
render leaves the 2^-15/2^-23 grid; auditioning with Comp on does, correctly. `ClipSat`'s colour
sub-block says why its tone/DC state is deliberately NOT kept warm while skipped (it filters the
residue, which is multiplied by a `dep` that only leaves zero through a 20 ms smoother — unlike
the ADAA memory and tame filter beside it, which filter the signal). The Latency-table flag says
why it is recorded and not self-healing (clamping the engine to the measured value would make it
disagree with the wrapper's `predictLatencySamples`, manufacturing the desync it prevents). The
duck request dropped at `! smoothersPrimed` is marked as the one deliberate drop among the four.
`LoudnessMeter`'s un-normalised stage-2 numerator is marked as BS.1770-4's own, so a future reader
does not "fix" 0.04 LU into every reading.

**One review claim was arithmetically wrong, checked rather than taken.** The limiter's
`relScale` multiplies the release alpha while the comment says it scales release TIME; the review
put the resulting error at "~0.98 ms rather than 0.5 ms" for a 1 ms release. It is not: α = 1 −
e^(−1/48) = 0.0206 at 48 kHz, ×2 → τ = −1/ln(1 − 0.0412) = 23.8 samples = **0.495 ms**, a 1 %
error that shrinks as the release lengthens and shrinks again at every oversampled rate. The
comment now records the exact relationship and that TEST_REPORT's style numbers are alpha ratios;
no code change, because re-deriving from `onePoleMs(releaseMs / k)` would move every measured
style number for a 1 % correction — a ⊕ tuning change, not a fix.

Also noted without action: the integrated-LUFS histogram walk (~1500 iterations/block, a P6 CPU-
budget candidate, now commented at the call site) and the still-missing meter-hold reset (P5
planned edge). Suites: 185 + 87.

### Third P4 review round — the meters were watching the monitor, and the limiter's levels were stepping (2026-08-01)

Fourteen items: four defects fixed, two accepted-and-recorded, two dismissed-or-confirmed by the
reviewer's own trace, six doc/comment corrections. Every code fix mutation-verified; six mutants,
six kills, each by exactly its own test.

**The meters were measuring the wrong signal.** `processBlock` metered the buffer AFTER
`engine.process()` — but that buffer carries the LISTENING path: §2.7 delta replaces it with
`dry − processed` and loudness comp multiplies it by the monitor gain. So Delta showed the
difference signal's LUFS, Comp showed the attenuated level, and — because integrated LUFS is a
gated histogram over the whole session and the dBTP hold is a session max — a few seconds of
either permanently biased both, in a loudness maximizer, where the meter is half the product.
The §2.9 output meters (LUFS M/S/I, dBTP, PLR, GR-history peak) now read a per-sample RENDER tap
inside the engine: the bypass-mixed programme path with no delta and no monitor gain — exactly
what an offline render emits, since both monitor functions are inert offline (invariant 10). The
tap is bit-identical to the buffer whenever the monitor functions are off, so nothing moved on
the default path. The wrapper's own LoudnessMeter/TruePeakEstimator members are gone; it
publishes from engine accessors on the same audio thread. The regression test pins EXACT equality
of every published reading across comp-on/comp-off/delta-on runs, with a listening-path-differs
guard so it cannot pass vacuously.

**Three limiter controls were stepping.** The per-block-setter header claimed "rates and modes,
not levels", and for release/style it is true — but `link` blends the detector LEVEL per sample,
`preserve` selects the attack alpha, and the detector HPF moves the detector spectrum, and all
three were adopted raw at buffer boundaries: a full-scale link step moved a limited channel's
gain 0.44 in ONE sample. All three now glide (20 ms SmoothedValue at the engaged rate, primed on
the first block, snapped at a latch — a silent-bottom event). The HPF re-derives its biquad per
step while gliding, the same pattern MasteringComp already used for the SAME shared scHpfFreq
value — the asymmetry between the two consumers of one parameter was the tell. The preserve map's
discontinuity at exactly 0 (instant attack vs ~0.05 ms for any positive value) is deliberate —
the wedge-contract tests rely on exactness at 0 — and now says so in place.

**The glide changed what one existing test proved.** The stale-detector test turned the HPF off
and fed silence; with the off-switch now a ~960-sample glide, the biquad would have DRAINED
during the glide and the missing-state-clear mutant would have survived on an empty delay line.
The test now keeps the loud signal running until the off edge actually fires, so the state is
charged when the clear does or does not happen. Same rule as the crest alignment last round —
put the property where the assertion looks — arising not from a bad stimulus but from a fix
changing the timing underneath a good one. Both re-run mutants still die.

**Accepted and recorded.** The latch-boundary step in the delay-aligned dry leg also feeds the
§2.7 dry measure and the adaptive feature extractor un-ducked — appended to KI-004 with why it
stays measurement noise (seconds-scale trim slew, −70 LUFS gate). Invariant 4's "under every
condition" now states its scope explicitly: the PROGRAMME path — delta can reach ~2× full scale
on decorrelated material by construction, bypass carries unclamped dry, both are audition-only
and inert offline; recorded in DSP_POLICY as a scope clarification with the ceiling test's
comment updated to match. The wedge buffer's exactly-at-capacity sizing got the same one-spare-
slot treatment as the dry ring.

**The doc drift this round was partly self-inflicted and partly systemic.** THREAD_MODEL said 44
cached atomics the same day the PR made it 45 — the freeze cache append (P4) never touched the
row. README said 228 checks while HANDOVER said 253: the SECOND round updated HANDOVER and not
README, one round after the audit claimed README was "caught up" — the claim was true when
written and stale two commits later. Both now carry the HANDOVER row's own rule ("re-count from
the suites' output when editing"). PARAMETER_REGISTRY still described §2.8 as unlanded
("will be duck-routed when the transition layer lands") two rounds after it landed. And
THREAD_MODEL's `file:line` citations had gone stale twice (`engagedWindow` at :84 → :128), so the
volatile ones are now symbol-based — a line number in a document is a assertion nobody re-runs.

Also: the null test's stimulus level (−12 dBFS vs the −3 dBFS knee bottom) is now a named
precondition WITH a self-enforcing check — the reviewer's structural-inertness trace showed the
bit-exact null with trims live rests on every stage being below its engage point, so the test
now refuses a stimulus that would quietly change what it proves; the preset-apply duck request
carries a comment stopping a future "optimisation" from moving it after the success check
(re-opening INC-001's hole); the meter-hold reset stays a P5 planned edge (the render-tap fix
removed the amplification that made its absence bite). Suites: 185 + 80.

### Second P4 review round — the transition layer had a click in it, and the test that should have caught it stopped thirty samples early (2026-08-01)

Fourteen items. Nine fixed, one accepted and appended to KI-004, one dismissed by its own
reviewer, three cosmetic/latent and fixed with the rest. Every fix carries a mutation-verified
test; two of those tests had to be rebuilt after the mutants survived them.

**The headline defect was in the layer that exists to prevent it.** `latchOsConfig` clears the
lookahead ring and resets the oversampler, so after a factor/phase latch the processed path emits
EXACT silence for `delaySamples + osLatBase` base samples — 541 at 4×/48 kHz. The bottom branch
started the 28 ms in-leg on that same block top, so the first real sample arrived 541 samples up a
1344-sample ramp, at gain ≈ 0.35: a −9 dB step, i.e. a click, on every oversampling change. The
fix holds the bottom until the refill completes (`bottomHoldSamples`, counted down per processed
base sample). A factor switch now mutes ~45 ms instead of ~34; that cost is recorded in KI-004
because it is audible in a different way and testers must not report it as a hang.

**Why the suite did not see it, precisely.** `testDuckWrapsOsLatch` measured its maximum
sample-to-sample delta over `20*512 … 22*512` = 10240…11263, and the splice landed at ~11293 —
thirty samples past the last one examined. The window is now the whole transition, and the
property has a direct assertion rather than a proxy: find the first non-zero sample after the
latch, peak the cycle that follows it. Held bottom → ~0.01 (the ramp is at its own start);
unheld → ~0.34. A factor of thirty between the two outcomes, which is what a bound should look
like.

**Two more dropped-request holes of the same shape.** A `requestForcedDuck()` consumed while the
OUT leg was still running fell through every branch and vanished (the bottom-state case was
fixed last round; this was the state before it). It is now remembered in `duckAskedWhileOut` and
spent as one held bottom block. Reaching it in a test needs 128-sample blocks — at 512 the 288-
sample out-leg always completes inside the block that starts it, so the `out` state is never
observed at a block top.

**Delta monitoring was the opposite of ducked.** `wetLeg = delayedDry − processed` subtracted a
ducked processed term from an unducked dry one, so during a transition the delta output rose to
the FULL dry signal exactly when the layer was meant to be silent — the loudest possible artefact
from the click-free mechanism. The dry term now carries the same duck gain (`dryForDelta`), which
makes the whole difference scale by `duckGain`; the bypass leg keeps the pure `delayedDry`, so
invariant 7 is untouched. Chosen over moving the duck downstream of the delta mix because that
would have re-ordered dither against the mix for no additional benefit.

**Last-writer-wins needed one flag, not two.** `adaptiveRestorePending` and `adaptiveClearPending`
were independent flags consumed in a fixed order (clear, then restore), so two session loads
between audio blocks — a learned session then an un-learned one — left the LAST loaded session
holding the FIRST one's references, which the next save then serialized. They are now one staged
record: a `pendingLearned` discriminator plus the two refs behind a single release-stored flag.
This retracts the "two INDEPENDENT self-correcting scalars" reasoning in the P4 Learn entry
below: self-correction was an argument about torn *values*, and it was never an argument about
*which staged intent* wins.

**Stale detector state on re-entry.** The true-peak estimator's 12-tap history only advances
while `tpMode` is on (the engine flips it from the OS factor), and the detector HPF kept its
biquad delay line when the frequency dropped to the range floor (the adaptive scHpf trim can
drive that edge). Both now clear on their re-entry edge. **The first version of this test proved
nothing:** its 4000-sample 60 Hz charging passage ended at exactly 5.00 periods, freezing the
history on a zero crossing, and both mutants survived. 4200 samples (5.25 periods) ends on the
crest and both mutants die. Third instance of this family in the project — a stimulus that does
not put the property where the assertion looks is not a weak test, it is a green one that tests
nothing.

**Measured, not assumed, while setting that test's bound:** the limiter's one-pole release stalls
at 0.99999857 rather than reaching unity — once `(1−env)·a` falls below half an ULP near 1.0 the
addition rounds away. Harmless (−0.00001 dB) but it means the engine's `exactlyEqual(gain, 1.0f)`
fast path never re-arms after the first gain reduction. Recorded here; not chased, because
snapping it would change gain behaviour to save an exact-compare branch.

Also fixed: the ADAPTIVE child's fields are read with the factory references as their defaults
(`getProperty` with no default yields `var()` → 0.0, and a session missing them restored a
reference no material can match — §4.4's rule was applied everywhere else on that path); the
`Latency.h` cross-check against JUCE's own reported latency is recorded unconditionally and
asserted by the suite instead of living only in a `jassert` that compiles out of every shipping
build; the dry ring gets one spare slot (its worst case sat at exactly size−1 — correct, but with
zero slack); the wrapper's metering block no longer indexes channel 0 unconditionally nor feeds a
mono buffer twice; `resetWindow`'s sanitise loops moved out of the per-channel loop; the unused
`fresh` processor left in the Learn round-trip test is gone. Suites: 177 + 76.

### P4 review round — two real races, a dropped duck request, and a coverage table that still denied the code existed (2026-08-01)

A ten-item external review. Six items were real and are fixed; two were accepted and recorded
rather than fixed; one was already-correct behaviour; one was dismissed by its own reviewer.

**The two memory-ordering findings were correct.** `AdaptiveEngine::learned` was a plain `bool`
written on the audio thread (`commitLearn`) and read by `getStateInformation` off it — a data
race in the ISO sense, and a practical one: `hasLearned()` could see `true` before the reference
targets it guards were visible, serializing a half-written ADAPTIVE child. Same shape on the
restore path: `restoreLearnedTargets` staged its two payload atomics and its flag ALL relaxed, so
the consuming block could see the flag without the pair. Both now follow the flag-orders-payload
discipline the GR ring already used: payload first, flag **release**-stored, consumer
**acquire**s (`learned.store(true, release)` after `publishRefs()`; `adaptiveRestorePending`
release-stored, `exchange (false, acquire)` at the block top). The comment that had justified the
relaxed pair — "each scalar is self-correcting, a torn pair re-slews" — was true of the *trims*
but not of the *refs a save can immediately re-serialize*, and it is gone. Lesson repeated from
the P1 rounds: a comment that argues a race is benign is usually describing a different variable
than the one it annotates.

**The dropped duck request was the subtle one.** A `requestForcedDuck()` landing while the
engine sat at the silent bottom was consumed at the block top and then ignored — the bottom
branch unconditionally began recovery, so the bulk swap that request was guarding (arriving in
the NEXT snapshot) stepped in mid-recovery at audible gain. The fix holds the bottom one more
block when the flag arrives there. The regression test is a two-run comparison (second request
during the bottom block → the following block is EXACT silence where the control run is already
recovering) and was mutation-verified: reverting the hold fails exactly that check.

**Monitor snap on the realtime→offline flip.** Invariant 10's test rendered offline from sample
zero, so it never saw the flip case: a mid-stream `nonRealtime` flip left `monitorGain` slewing
200 ms toward unity and the delta fade draining ~10 ms into the render. The §2.7 block now SNAPS
both (`setCurrentAndTargetValue (1.0f)`, `deltaMix = 0`) the block `nonRealtime` is observed,
and the extended test asserts the offline tail is bit-identical between comp on and off — while
also asserting the runs DIFFER before the flip, so the identity check cannot pass vacuously.

**Accepted, not fixed — now KI-004.** The ≤ one-block + ~6 ms window where reported and actual
latency disagree during a ducked OS switch, and the bypassed-instance step (factor change adopted
without the duck while fully bypassed), are ADR-0004's deliberate trade. They are now a
KNOWN_ISSUES entry with the bound (≤ 67 samples) instead of tribal knowledge.

**The coverage lie in this file.** The module-coverage table still read "*(none — `src/` does not
exist)*" with eight DSP modules and the wrapper in the tree — this audit file failed its own
update protocol for four phases while its narrative entries stayed current. The table, the
self-coverage rows (TEST_REPORT, THREAD_MODEL, PARAMETER_REGISTRY, REALTIME_SAFETY_AUDIT), the
gaps list and README's status block are caught up; THREAD_MODEL gained the three P4 edges
(Learn commands, restore hand-off, learned flag) its table was missing. Lesson: chronological
entries do not keep summary tables honest — only touching the table on every round does.

Also in this round: the dry-ring capacity envelope is now a `jassert` at `latchOsConfig` (it
held by construction; now it trips the moment a Latency.h table entry outgrows the prepare-time
sizing), the stale "P1 form / TODO(P2)" comments in `switchToSlot` and `PluginProcessor.h` that
described the opposite of the code are gone, the onset-detector comment says 6 dB (2.0×) as the
code implements, and `AdaptiveEngine::expectedBlockLen` (written, never read) is removed.
Suites: 156 + 72. Evidence: PR #5.

### P4, Learn commit — and a silent no-op replace caught by its own test (2026-08-01)

**Learn (core)**: start → silence-gated accumulation of the feature means → commit fixes the
reference targets (the analysed passage becomes what trims toward zero — Learn feeds the
references, never the output stage). Targets serialize in the global `ADAPTIVE` child, written
only once learned; restore rides the host-hidden mirror pattern as two INDEPENDENT self-correcting
scalars — documented as deliberately distinct from OQ-013's coherence-critical four-vector, whose
transport stays hard-stopped. `testLearnCommitAndAdaptiveRoundTrip` covers commit, byte-identical
round trip with the child present, and the absent-child = never-learned read rule.

**Two catches during the increment, both by the test written first:** (1) the wrapper's restore
edit anchored on a comment an earlier round had rewritten — the python replace silently no-op'd
and the ADAPTIVE child saved but never restored; the round-trip check failed on exactly that.
Edits now assert their anchors. (2) The onset detector under-counted dense clicks (the fast
envelope released at 80 ms and was still elevated when the next hit landed): 1.8/s measured on a
10/s stimulus. A 30 ms release and a 500 ms symmetric baseline read 8.25/s — and the Learn test's
"reference moved" premise is what surfaced it.

### P4 core — adaptation that provably cannot break the null (2026-08-01)

**`AdaptiveEngine`** (src/dsp/AdaptiveEngine.h): block-rate features of the delay-aligned input
(crest, spectral tilt via an 800 Hz one-pole split, transient density via a fast/slow envelope
onset detector with a 50 ms re-arm), silence-gated so a breakdown holds rather than re-slews; the
four-member trim vector slewed at ~2 s with deadbands, hard-bounded, applied to the per-block
EFFECTIVE settings only. The load-bearing design fact, stated in the header and proven by the
suite: **every trim is inert while its host stage is inert**, so the bit-exact null runs with
adaptation LIVE — no adaptation on/off gate exists to forget. `freeze` now reaches the engine
(cache slot 45 — an internal cache change, not a surface change; the registry snapshot is
untouched). Freeze latches the vector exactly (ulp-equality across a programme change, mutation-
verified), `testModeSwitchIsSoundNeutral` pins invariant 2 sample-identically, and the policy's
Current-implementation section is populated with the evidence map. OQ-013 still blocks the
frozen-trim RESTORE; Learn is the remaining P4 item.

### P3, publication + monitor commit — meters reach the GUI boundary, KI-002 closes (2026-08-01)

**Meter publication**: the wrapper measures the OUTPUT per block and publishes LUFS M/S/I, dBTP
max-hold (shared TruePeakEstimator), PLR and GR through relaxed atomics — the THREAD_MODEL meter
row, now implemented, plus **`GrHistoryBuffer`**: the first Audio→GUI SPSC ring in the tree
(4096 entries ≈ 43 s at 512/48 k; entry written first, index release-stored after, stateless
peeks). `testMeterPublication` pins the readings, the PLR identity and the one-entry-per-block
ring advance.

**§2.7 monitor layer** (KI-002 → INC-002): Measure (K-weighted ST loudness, dry vs processed,
frozen under the −70 LUFS absolute gate) + Predict (deterministic gain lift, GR-corrected,
floor-only), min-combined, 200 ms smoothed, applied POST-mix so the bypass leg carries the same
gain — the loudness-matched bypass falls out of the placement rather than needing machinery.
Delta behind its own ~10 ms crossfade; a transparent chain's delta is EXACT silence because the
default path is bit-exact. Invariant 10 is **live**: the offline render is bit-identical with
either toggle in either position, and the predict floor is pinned by an early-window check the
measure could not satisfy (short-term needs seconds; the floor acts in one block). Mutants
killed: comp/delta ignoring nonRealtime, predict floor dropped.

### P3 opens — the LUFS meter, and a gate half that only mutation could see (2026-08-01)

**`LoudnessMeter`** (src/dsp/LoudnessMeter.h): BS.1770-4 K-weighting (the ADR-0009 pre-warped
biquad design from the sibling's LoudnessMatch, provenance in the header), 100 ms sub-blocks →
400 ms gating blocks at 75 % overlap, M/S off the sub-block ring, INTEGRATED through the
fixed-size histogram accumulator (751 × 0.1 LU bins — REALTIME_AUDIO_POLICY's named consequence:
never a growing container). Calibrated against the standard's own compliance sentence — 0 dBFS
997 Hz in one channel reads −3.01 LKFS — at 48 AND 44.1 kHz, ≤ 0.1 LU.

**The audit-worthy find: the absolute gate was UNTESTABLE with the obvious stimuli.** The
dropped-absolute-gate mutant survived both gating tests, because silence lands at the histogram
floor, below any plausible relative threshold — the relative gate masks the absolute one
completely on programme-plus-silence material. The absolute gate's only distinct observable is
its effect on the pass-1 mean that SETS the relative threshold; the killing stimulus needed a
band placed between the correct threshold (−34.8) and the silence-dragged one (−41.7): 10 s at
−20 + 20 s at −38 + 120 s of silence reads −20.0 correctly and ≈ −24.7 on the mutant. Fifth
stimulus-calibration case this project has recorded, and the sharpest: the redundancy between
two protection mechanisms is itself what hides the loss of one.

### P2, transition-layer commit — the §2.8 duck, and KI-001 becomes INC-001 (2026-08-01)

**The duck exists**: asymmetric raised cosine (~6 ms out / ~28 ms in), advancing per base sample,
multiplying the PROCESSED path only (downstream of the clamp — a gain ≤ 1 cannot re-exceed the
ceiling; upstream of dither — the export grid stays intact; never the dry path — bypass stays a
bit-exact null). Engine-side discrete rewires (`eqPosition`, `colourModel`, OS factor/phase) are
held in applied-state fields and execute ONLY at the silent bottom at a block boundary — the POD
the stages see carries the applied values, so nothing rewires at audible gain and OS latency still
never moves mid-block. Wrapper bulk swaps (A/B, preset apply, session load) call
`requestForcedDuck()` BEFORE the swap — the first implemented instance of the THREADING_POLICY
momentary-request row, recorded as such in `THREAD_MODEL.md`. The first block after prepare/reset
adopts directly: a duck there would dip the head of every render for no transition at all (and
would have broken the OS latency matrix's fresh-engine timing — noticed at design time, not by a
red test, for once).

**KI-001 is closed and moved**: `POSTMORTEMS.md` INC-001 carries the mechanism, the fix, and the
four mutation-verified prevention tests (`testDuckWrapsDiscreteRewires`, `testDuckWrapsOsLatch`,
`testDuckOnWrapperRequest`, `testAbSwitchRequestsDuck` — the last one pins the WRAPPER wiring and
fails when the `switchToSlot` request call is removed). DSP_POLICY invariant 8 is now **live**.

**One defect caught in-flight**: the duck-out phase inversion carried a spurious `1−p`, sending a
fresh duck to the bottom in ONE sample — precisely the step the smoothness test exists to catch,
and it did, before commit. The inversion's derivation now lives in the comment.

### P2, oversampling + dither commit — the engine restructure, and TEST_REPORT.md exists (2026-08-01)

**The engine is now staged**: base-rate front (input gain → EQ-Pre → compressor), the ADR-0003
oversampled region (up ×N → clipper/sat → the 10 ms lookahead line AT the region rate → limiter →
down ×N), base-rate back (EQ-Post → clamp → dither → bypass). All eight `juce::dsp::Oversampling`
instances (2×/4×/8×/16× × IIR-min/FIR-linear) are built and `initProcessing`'d at `prepare()`
per ADR-0011; a runtime factor/phase change latches at a block boundary as a reset-class event
(KI-001 extended — third member of the §2.8 family). `useIntegerLatency` keeps every
configuration's group delay a whole base sample, so `Latency.h` now carries the measured table
(min {4,6,6,6} / lin {49,61,65,67}) and `prepare()` asserts table == `getLatencyInSamples()` per
instance — a JUCE bump that redesigns either cascade fails loudly twice. Oversize host blocks are
processed in prepared-size **chunks** — the earlier "correctness needs only delaySamples+1"
argument no longer holds for a block-structured engine, so the guarantee moved from arithmetic to
structure. Dither: TPDF 16/24 + first-order shaping, deterministic xorshift (offline renders
repeat), after the clamp, processed path only.

**Every existing check survived the restructure bit-for-bit** — the null, the wedge alignment,
the priming, byte-identity — before any new test was added; that ordering (restructure green
first, then extend) is what kept a 400-line rewrite from being a bug factory.

**`docs/TEST_REPORT.md` now exists** and closes part of the "no measured numbers anywhere" gap:
aliasing (ADAA −14.8/−10.4 dB; 4× ≈ −74 dB; the +1.3 dB fundamental recovery that looks like a
bug and is oversampling removing ADAA's sinc droop), true-peak accuracy (−0.004 grid / −0.171
off-grid), the full latency matrix (linear cells sample-exact; min-phase ±1 with the dispersion
rationale), transparency (−69 dB), dither shaping (+12.6 dB tilt). Each number is asserted with
margin by a named test, so the report cannot silently rot.

**One calibration note for the record:** the OS aliasing test's first "fundamental untouched"
bound (±1 dB) failed on CORRECT code — 4× genuinely raises the fundamental 1.3 dB by removing the
ADAA droop. Third instance of the same lesson this phase; the bound now names the mechanism.

### P2, clipper and limiter commits — two stimulus-calibration catches in one day (2026-08-01)

**`ClipSat`** landed with the knee-morph ADAA clipper, colour models, the dynamic HF tame and the
mix — see the commit for the mechanism inventory. The audit-worthy finding was in its ADAA test:
the first stimulus (5 kHz tone, folded 5th) showed only 4.8 dB of improvement — and that is the
**theoretically correct** number for ADAA-1 at a 25 kHz source harmonic (≈ |sinc(π·f/fs)|), so a
6 dB assertion there fails on correct code. The fix was to *calibrate the stimulus, not loosen the
bound*: at 11.72 kHz the folded 3rd/5th improve by a measured 14.8/10.4 dB, asserted at 6/8. The
same class recurred in the true-peak test: the "off-grid" ISP phase π/8 put the continuous peak at
t = 0.75 — exactly ON a 4× interpolation point — and measured −0.002 dB, i.e. it tested nothing;
φ = 0.3125π puts the peak between points and measures the real −0.171 dB max-reading property.
Both are the smoothing-test lesson in a new costume: **compute where the property lives before
asserting on it**.

**`LookaheadLimiter` grew into the §2.5 spec** — per-channel wedges with stereo link, the
ADR-0003 true-peak estimator as its detector option (12-tap × 4-phase windowed sinc designed at
prepare, group delay 5.5 samples — RISK-008 materially reduced with measured numbers), transient
preservation (instant attack at 0, the exactness the wedge tests pin), Transparent/Punchy/Loud as
constant-presets, the shared sidechain HPF with **floor-as-off semantics** (an exact skip at
20 Hz, now consistent across both detectors — the compressor was changed to match), and the
two-pole auto release pinned by the same disjoint-bounds technique as the compressor's. One
genuine bug was caught by test-first here: the style factor multiplied the release *alpha* by 0.5,
making Loud the SLOWEST style — the probe run showed Transparent 0.413 vs Loud 0.336 recovered,
inverted from the contract, fixed to time-domain scaling before commit. Six mutants killed: link
ignored, tp ignored, styles inert, preserve inert, single-pole auto, HPF inert. The engine tests
that pin the wedge contract sample-exactly (`testLimiterAlignment`,
`testControlsPrimedOnPrepare`, self-heal) now pin `truePeakMode=false / transientPreserve=0 /
limAutoRelease=false` explicitly, with comments naming why each control legitimately blurs what
those tests measure.

### P2, first commits — EQ and compressor, and a two-stage test that a mid-speed pole slipped through (2026-08-01)

**P1 closed** (PR #4 merged with 3-OS CI; `THREAD_MODEL.md` + `PARAMETER_REGISTRY.md` written from
ADR-0011/ADR-0010; OQ-014 remains the one open owner call). **P2 opened** on PR #5, per the brief
§11 module list, in chain order.

**`MasteringEQ`** (src/dsp/MasteringEQ.h): six RBJ sections — the §2.2 complementary tilt pair at
700 Hz, shelves at fixed Q 0.707, two bells. All-flat is bit-transparent **by structure** (a
section at exactly 0 dB is skipped; a biquad at 0 dB gain is only approximately identity in float,
which would break invariant 7's null). Eleven smoothed parameters inside the module; coefficients
recompute per sample only while a smoother moves. Pre upstream of the wet ring (the limiter
detector sees the EQ'd signal), Post between limiter and clamp — and
`testOutputNeverExceedsCeiling` now runs ADR-0002's **mandated stimulus**: both positions, +12 dB
shelf after the limiter, mutation-verified by moving the clamp upstream of the post EQ. The
`eqPosition` rewire is a KI-001-class step until §2.8 lands (KI-001 extended). Four EQ mutants
killed: tilt sign, unsmoothed targets, always-engaged sections, clamp-before-post-EQ.

**`MasteringComp`** (src/dsp/MasteringComp.h): feed-forward log-domain gain computer — per-channel
detector-side sidechain HPF (20–300 Hz), stereo-linked max detector, RMS (10 ms) / Peak modes,
soft-knee static curve, ballistics on the GR signal in dB, parallel mix with exact endpoints, and
the §2.3 **two-pole auto release** (80 ms + 900 ms averaged in dB). Below the knee bottom the
sample passes bit-exact — the all-defaults null path (threshold 0 dBFS). Five mutants killed:
knee dropped, RMS mode dropped, HPF dropped, both-poles-slow, both-poles-fast.

**The finding worth the audit entry: the first two-stage-release test was satisfiable by a single
pole.** Its three assertions (fast initial recovery, deceleration, tail held at −0.5 dB after
200 ms) all pass for one ~150 ms pole — found because the both-poles-fast mutant survived. The
fix was to choose bounds that are **provably disjoint for any single exponential**: the
deceleration ratio (rec₂ < 0.6·rec₁ ⇔ e^(−100ms/τ) < 0.6) forces τ < 196 ms, and the 800 ms
tail-hold (< −1.5 dB) forces τ > 322 ms — no single τ satisfies both, so only a genuine
two-stage release passes. Verified in both directions: the real code passes; the fast-pair,
slow-pair and single-150 ms mutants each fail a named check. Same lesson as the self-heal test
last commit, sharpened: it is not enough for a test to fail on *the* mutant you tried — the
assertion set must exclude the whole family of wrong shapes, and back-of-envelope algebra on the
bounds is how you know it does.

### P1 skeleton, seventh commit — a review claim falsified by its own regression test (2026-08-01)

**The headline "bug" of this round does not exist on the pinned JUCE, and the proof is now a
test.** A review asserted that a session tree missing an individual `PARAM` child leaves the
previous session's value live (`replaceState` re-creating the child "seeded from the parameter's
CURRENT value"), producing the chimera one level below the whole-child read rules. The mechanism
reads plausibly from `flushParameterValuesToValueTree` alone — but the test written to fail on it
**passed on the unmodified code**, and the pinned source explains why: the reconnection appends an
id-only child to `state`, the APVTS hears its own `appendChild` via `valueTreeChildAdded` →
`setNewState`, and that call's value-property fallback is `getDenormalisedDefaultValue()` — the
parameter lands on its declared default *before* the flush writes anything. The test was made
non-vacuous (a premise check pins that the dirtied value really was off-default) and **kept as the
tripwire** for a JUCE upgrade changing the reconnection semantics; the fix it was written for was
**not applied**. That matters beyond this item: the drafted fix (fill gaps from the default tree
inside `adoptParamsTree`) would have introduced a real regression on the `applySlotToLive` path —
an A/B slot missing a view-tier child would have clobbered live view state with defaults. The
review cycle the project keeps warning about ("each fix breeds the next round's bugs") is exactly
this shape, and test-first is what caught it.

**`LookaheadLimiter` now carries the uniform ownership macro.** The previous round spelled the
guard `= delete` to keep the header "JUCE-free" — a constraint no policy imposes: `DSP_POLICY.md`
invariant 13 forbids plugin-client/GUI headers and explicitly permits `juce_audio_basics`/
`juce_dsp`, beneath both of which sits `juce_core`, where the macro lives. The justification cited
a rule that does not exist, so the deviation is reverted to the mechanical convention
(`CODE_STYLE.md` §Structure) the sibling owning classes already follow.

**OQ-014 records the one governance question this round surfaced instead of deciding it.**
`mappingPending` and `restoreDepth` are payload-free any-thread → message-thread guards — a
direction `THREADING_POLICY.md`'s permitted-path table does not enumerate, and the policy routes
any off-table path to the Architecture Review Gate. Whether ADR-0005/0011's "async message-thread
listener" clause already blesses them (they implement the mandated shape — `juce::AsyncUpdater` is
itself an atomic flag plus a message post) or whether a small ratifying ADR is owed is an owner
call, not an agent inference; `OPEN_QUESTIONS.md` OQ-014 states both readings and what each
implies for the still-owed `THREAD_MODEL.md`.

Comment-only syncs from the informational findings: the second `updateLatency()` in
`setStateInformation` is labelled as the deliberate, no-op-when-unchanged second recompute (the
batch fires the first — the "exactly one" the test pins is at the `InternalState` level); the
`ScopedRestore` header states the accepted swallow of a gesture armed microseconds before a
restore; `saveSlotFromLive` warns that the view-tier exclusion lives entirely on the apply side, so
a future path adopting a slot without `applySlotToLive` re-introduces view-tier travel; and
`updateLatency` documents the 48 kHz placeholder before the first `prepareToPlay`, with the note
that a future PDC test must assert only after a prepare.

### P1 skeleton, sixth commit — owning-class guards, and a test that measured the wrong thing (2026-07-31)

**The self-heal test passed against its own mutant, and the stimulus was why.** The invariant-9
recovery called `limiter.reset()`, which empties the sliding window *and* snaps the envelope back
to unity; the delay line still holds the material the old envelope was holding down, so the
recovery hands it to the clamp at full level. The first test drove the engine 4× over the ceiling
and counted clamped samples — but material that far over rides *at* the ceiling whether it is
limited or clipped, so both variants scored identically (9 / 8 clamped samples). What separates
them is a signal that is **quiet under a held-down envelope**: a burst drives the gain to ~0.22, a
slow release holds it there, and the quiet tone that follows plays attenuated. Carrying the
envelope keeps it attenuated; snapping to unity steps the level up ~4.5× *and stays there*, because
the quiet tone never asks for gain reduction again. `limiter.resetWindow()` now carries the
envelope across (sanitising it, since the self-heal is defence in depth and a guard that trusts its
own reachability argument is not one), and the redesigned test fails on the mutant. The lesson is
the one this audit keeps recording: a test name does not carry the property, and a stimulus in
which both behaviours look the same is not a test.

**Owning classes carry the guard now.** `AnabasisEngine` (two heap rings), `LookaheadLimiter` (the
wedge vectors) and `InternalState` (a `ValueTree` it registers *itself* as a listener on — a copy
would share the tree unregistered while its destructor deregistered the original) had no
non-copyable declaration, against `CODE_STYLE.md` §Structure. `AnabasisEngine` and `InternalState`
take `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR`; `LookaheadLimiter` takes `= delete` instead,
because the leaf DSP headers are JUCE-free by construction and only `AnabasisEngine` pulls
`juce_audio_basics`. The macro is a user-declared constructor, so it suppresses the implicit
default one — both classes needed an explicit `= default` and the build said so immediately.

**A session load re-reported PDC up to six times.** `replaceFrom` writes all ten properties as
defaults and then overlays the incoming ones; three are latency inputs and each write fired
`onLatencyInputChanged`, walking the reported figure through the Off value before landing. Invisible
while `osLatencySamples()` returns 0, a burst of mid-load PDC changes the moment oversampling lands
— exactly what ADR-0004's constant allowance exists to prevent. A `ScopedLatencyBatch` coalesces the
read into one notification, fired from the destructor so no early return can skip it.
`testLatencyNotifyIsBatchedAcrossARead` pins both halves (one fire for a bulk read, still one for a
single interactive write) and fails unbatched.

**Two review findings were declined, with the reasoning written into the code rather than a reply.**
The ring is *not* undersized for an oversized block: `process()` is a per-sample circular delay
line, so `ringSize >= delaySamples + 1` is the whole invariant and the `+ maxBlockSize` is slack.
The old comment ("room past the tap") read as a block-size dependency and invited a `jmin` on
`numSamples` — which would leave the tail of an oversized block unprocessed, i.e. bypassing the
ceiling clamp, to fix a bug that is not there. The comment now states the invariant and names that
trap. Channels past the prepared count stay untouched rather than cleared (silencing a caller's
audio is a worse failure than passing it through), so the contract is now asserted at the boundary
instead of assumed.

Also: `bypassMix` is reset alongside the rings, so a re-prepare mid-fade no longer resumes a fade
against zeroed delay lines; `CachedParams::resolve` asserts every id resolved and `allResolved()`
is pinned by the state suite (a null slot silently feeds `0.0f` into its engine field, and only the
fields a test happens to set would catch it); the stale "34 checks" in `README.md` and
`HANDOVER.md` is now 84 with the row telling the next editor to re-count; the `[Unreleased]`
changelog entry cites **PR #4** plus the five SHAs, since a branch-relative "the commit that
follows" stops resolving when the branch is deleted at merge (`CHANGELOG_POLICY.md` rule 2); and
`KNOWN_ISSUES.md` entries are `###` under **Open issues**, in ascending KI order, with the
convention written into the file because the doc lint cannot check heading nesting.

### P1 skeleton, fifth commit — the Windows-only snapshot red, and the restore guard (2026-07-31)

**The Windows CI red was a line-ending artefact, not a parameter change.** `AnabasisStateTests`
failed exactly one check on the `windows` leg — the frozen registry snapshot, which reports as a
Hard Stop — while the Linux and macOS legs were green. Cause: Git for Windows defaults to
`core.autocrlf=true` on the hosted runners, so `parameter_registry.snapshot` (LF in the index) is
checked out with CRLF, and the comparison is byte-wise against a dump built with `"\n"`. Confirmed
by reproducing it locally: CRLF-ifying the fixture reproduces the failure to the check and the
count (48 checks, 1 failure), and the fix passes with the fixture in either encoding. `.gitattributes`
now pins `tests/fixtures/*.snapshot` (and `*.sh`) to LF, the comparison normalises line endings —
and only those — and a mismatch prints the **first differing line with both sides**, because a bare
`FAIL` on a 49-line byte comparison costs a full CI round to diagnose.

**The macro abort was ordering-dependent, and the ordering is a host's to break.**
`abortPendingMapping()` at the END of each restore path drops the mapping the restore's own
notifications armed — but only if the restore finishes before the 30 ms drain timer next fires.
VST3 does not promise `setStateInformation` arrives on the message thread, and on a host that
restores off it the timer can apply the mapping *mid-restore*, rewriting the nine managed
parameters from the curves; no later abort takes that back. `MacroEngine::ScopedRestore` now wraps
each restore body: the drain is suppressed for the scope's whole lifetime and the flag is dropped
on the way out (abort **before** the depth decrement, so no window exists where the flag is armed
and the guard is already down). `abortPendingMapping()` is private — the scope is the only way to
reach it, so a new restore path cannot forget the step. Depth counter, not a bool, so a nested
restore's exit cannot re-open the window for the rest of the outer one.

This is **not** a thread-model change: no new thread, no new cross-thread edge (the same
any-thread → message-thread flag `mappingPending` already is), no new ordering primitive. It
implements ADR-0005/ADR-0011's "MacroEngine is message-thread-only" and §5.3's "a restore is not a
macro gesture" without depending on a race outcome. What it does **not** cover — `replaceState`
mutating a `ValueTree` the editor may be reading, and the few-instruction check-then-act window in
the guard — needs a synchroniser outside the set `THREADING_POLICY.md` admits, so it is an
Architecture Review Gate item and is recorded as **KI-003** rather than patched.

`testDrainInsideRestoreIsSuppressed` models the mid-restore drain single-threaded (a flush inside a
`ScopedRestore` is exactly what the timer would do there); removing the guard fails both of its
checks.

### P1 skeleton, fourth commit — thread discipline and the structural-tolerance read rules (2026-07-31)

**`MacroEngine::parameterChanged` was an audio-thread hazard.** APVTS delivers parameter-change
callbacks on whichever thread wrote the value — the audio thread during automation — and the
listener called `applyMapping()` inline: `setValueNotifyingHost` on nine parameters, `juce::String`
construction, host notification. It now sets an `std::atomic<bool> mappingPending` and only calls
`triggerAsyncUpdate()` when `MessageManager::existsAndIsCurrentThread()`; a 30 ms `Timer` drains the
flag otherwise. `THREADING_POLICY`'s message-thread-only rule for the macro layer (ADR-0005) is
enforced by construction rather than by convention, and the abort path
(`abortPendingMapping()`, called on every restore) is mutation-verified.

**All four control smoothers now prime together.** `inputGain` and `pushGain` primed in `prepare()`
while `ceilingLinear` and `windowSamples` did not, so the first block after `prepare()` glided the
ceiling and the lookahead up from zero — an audible ramp on the first buffer of every render.
`testGainsPrimedOnPrepare` pins all four; removing any single prime fails it.

**§4.4 structural tolerance was implemented in one direction only.** A valid `AnabasisRoot` that
omits `ANABASIS_INTERNAL` returned early, and one that omits `ANABASIS` fell through — both left
the *previous* session's values live, which is exactly the "chimera of two sessions" the read rules
forbid. `InternalState::replaceFrom` now applies `setDefaults()` first and overlays what the
incoming child actually carries, and the wrapper adopts the pristine default slot when `ANABASIS`
is absent. `testMissingChildrenReadAsDefaults` covers both directions against an already-dirtied
processor, and both mutants (early return / dropped `else`) fail it.

**The log-taper default drift is now recorded, not silently carried.** `limRelease` (100.000015)
and `eqBell2Freq` (2999.999756) appear in the frozen registry snapshot as the images of their
declared defaults under the taper's `exp(log(x))`. `docs/procedures/TESTING.md` states why rounding
them away would blind the snapshot to a real taper change, and `testRawRoundTripIsIdempotent`
carries the property byte-identity actually depends on: one save→load→save pass is a **fixed
point** for every parameter at eight probes across its range. A taper change now fails there,
naming the parameter, instead of intermittently reddening `testStateRoundTrip`.

Docs synced this pass: `BUILD.md` and `TESTING.md` (status blocks — the suites and the build exist,
descriptions of what the tree does rather than specifications), `DSP_POLICY.md` (invariant → test
map rows 2/4/7/8/9/13 marked live or partial), `KNOWN_ISSUES.md` (KI-002: `loudnessComp` and
`deltaMonitor` are parameter-surface-only at P1).

### P1 skeleton, third commit — the smoothing rule, and a test that proved itself worthless (2026-07-31)

**The substantive fix: `ceiling` and `lookahead` reached the DSP unsmoothed.** `CODE_STYLE.md`
§Real-time discipline says every parameter that reaches the DSP is smoothed, and `DSP_POLICY.md`
invariant 8 names *the lookahead* as "the one switchable path with neither a duck nor a latch …
the path most likely to be skipped at P1", requiring its move to be "a smooth, band-limited control
signal". Both were adopted per block: a ceiling automation lane stepped the limiter's gain and the
clamp together, and a lookahead move jumped the detector tap by hundreds of samples at a block
boundary. Both are now per-sample values from `juce::SmoothedValue` (20 ms), handed down to the
limiter and the clamp — which also means the clamp uses the **same instantaneous ceiling** the gain
computer used for that sample, so it stays a backstop rather than a second, differently-timed
threshold.

**The finding that matters more than the fix.** The first smoothing test passed against
deliberately unsmoothed code. Two separate reasons, both worth recording:

1. The scan started at `stepAt + 1` — one sample *past* the discontinuity it existed to catch.
2. After fixing that, the *lookahead* half still passed, and instrumenting it explained why:
   enlarging the window cannot retroactively add samples the wedge already dropped, and shrinking
   it only relaxes the gain, which goes through the slow release. **An unsmoothed `W` steps the
   detector tap without stepping the output** — so the output-watching assertion was measuring a
   property that does not hold.

The response was not to loosen the threshold but to move the assertion to where the property
actually lives: the engine now exposes `engagedWindowSamples()` and the test pins the glide itself
(first block adopts, one block in it is strictly between the two values, it reaches the target, and
no block moves the tap more than a block's worth). **Five mutants are now caught** — ceiling
unsmoothed, W unsmoothed, either prime removed, and the detector tap reverted to the write head.
A test that passes against the bug it was written for is not a test, and mutation is the only thing
that tells you which kind you have.

**Six smaller items from the same review, all fixed.** `bypassStep` is derived in `prepare()` so
the ~10 ms figure holds across a sample-rate change mid-fade; `CachedParams` is sized from the real
count (44, not 49) with a `static_assert`, and `testCachedParamsMapping` pins the *positional*
coupling between `kCacheOrder` and `toEngine` field by field — a swap of two adjacent lines now
fails (verified by mutation), where before it would have silently shifted every later field; the
`CHANGELOG` entry cites its Evidence Source commits per `CHANGELOG_POLICY.md` rule 2; KI-001 moved
from the format template into `## Open issues`, replacing the "none — no build exists" placeholder
it was contradicting; `REPOSITORY_MAP.md`'s `[P1]` markers dropped from `CMakeLists.txt` and
`tests/`; and two stale comments corrected — the schema banner said `activeIndex` where the code
writes `active`, and the editor banner claimed an unconditional OpenGL include that is in fact
platform-gated (the *link* is unconditional, the include and the context member are not).

**Two accepted without change, with the reasoning recorded in the code:** bypass carries the
unclamped dry signal — invariant 7 requires a bit-exact null, so invariant 4's ceiling guarantee is
a property of the *processed* path, and that reading is now stated at the branch rather than
inferred; and the invariant-9 self-heal empties the sliding window, so for one window's worth of
samples a peak already in the delay line meets the clamp instead of being pre-empted — a transient
quality cost, not a contract break, since the clamp is unconditional. **Declined:** `setNonRealtime`
being `noexcept` is the base-class signature, and the `updateLatency` call in it is ADR-0004 item
5's mandated trigger.

After: 71 checks green (29 DSP + 42 state), warning-free, pluginval L5 both modes ×3.

### P1 skeleton, second commit — the adversarial review earned its cost (2026-07-31)

Three read-only agents were run against the fresh skeleton (parameter contract · DSP/latency/
threading correctness · state schema) before it could reach a human. The parameter surface came
back clean row-for-row. The other two agents found **2 blockers and 8 real defects** — in code
whose 34 checks and pluginval L5 were all green, which is the point worth recording: *the suites
proved the contracts they encoded, and the review found the contracts they didn't.*

- **The limiter's gain computer was misaligned with the audio tap** (blocker, proven by
  simulation): the sliding window watched the newest inputs — 10 ms ahead of the output regardless
  of the engaged lookahead — so the envelope attacked early, then *released before the peak
  played*; peaks reached the clamp under-attenuated (up to +5 dB over at fast release) and were
  flat-topped. Invisible to the green tests because the clamp masks overshoot at the peak and the
  null tests use sub-ceiling stimuli. Fixed by feeding the detector from the ring at
  `writePos − (delay − W)` — the sample that plays W steps from now — with the window widened to
  W+1 so the playing sample stays covered (the second, off-by-one finding). The corrected contract
  is now stated ON the class and pinned by `testLimiterWindowCoverage` (unit) and
  `testLimiterAlignment` (engine-level: the duck may begin no earlier than the engaged lookahead).
- **Every state restore armed a macro re-map that clobbered the restored values** (blocker):
  landing the macro parameters notifies their listeners, so session load / A/B switch / preset
  apply queued a mapping pass that rewrote the nine managed parameters from the curves — exactly
  the off-curve values a restore exists to bring back. Green in the suite only because no message
  loop runs there. Fixed at the §5.3 boundary: a restore is not a gesture, so every restore path
  ends with `abortPendingMapping()`; the suite now flushes deterministically and pins both
  directions (restore preserves, gesture still maps).
- **Six more, all fixed and pinned:** A/B slots saved without the `raw` overlay (switching was not
  raw-exact; log-taper values drifted ulps per round trip); a slot tree carrying `raw` leaked it
  into the live tree; the ceiling lock was write-then-revert (an audio block between the two writes
  starts the smoother toward the preset's ceiling — now a skip, no write at all); a root without an
  `AB` child kept the previous session's trims/mask/name (chimera state on next save — slot fields
  now reset to defaults first); AB slots were read by child position, so a tolerated unknown child
  shifted both slots (now collected by type); presets serialised UNSNAPPED mid-step discrete values.
- **Schema names aligned with ADR-0007 before first ship freezes them:** the AB index property is
  `active` (not `activeIndex`), and an empty `ADAPTIVE` child is no longer written — "absent =
  never learned" is the discriminator and writing it from day one would have destroyed it.
- **Also from the review:** `getBypassParameter()` now routes host bypass through the engine's
  delay-aligned crossfade; `prepareToPlay(sr, 0)` can no longer zero the group delay; the
  invariant-9 header comment now says what the code does. Accepted without change, with reasons:
  the MacroEngine's `triggerAsyncUpdate` on the unsupported automated-macro path, and
  `int_uiScale`'s value-set enforcement (the editor's job at P5).

After the fixes: 59 checks green (23 DSP + 36 state), warning-free, pluginval L5 both modes ×3.
ADR-0007's own test obligation — a frozen slot and a non-clear mask in the round-trip fixture —
is now met rather than vacuously satisfied.

### P1 skeleton lands — the first code, verified by building and running it (2026-07-31)

**What exists now.** `CMakeLists.txt` (ADR-0008's five-target graph, identity frozen, JUCE 9.0.0
fetched at the pinned SHA — `GIT_SHALLOW TRUE` worked, so the documented fallback was not needed),
`src/` per DESIGN §1.3's P1 subset, `tests/` with 34 checks, and the frozen
`tests/fixtures/parameter_registry.snapshot`. Everything below was verified by running it, not by
inspection: **build warning-free** under the recommended flags, **both suites green**,
**pluginval L5 green ×3 in both modes** on Linux — the P1 exit criterion, pending the 3-platform
CI confirmation. **OQ-011 resolved** with evidence read from the pinned JUCE tree (deployment
floor macOS 10.11 → 10.13 kept deliberately; arm64 floors at 11.0 by toolchain).

**Contracts enforced in code, not prose, from the first commit:** the constant-allowance latency
model lives in ONE header (`src/dsp/Latency.h`) that both the engine's real delay and the
wrapper's predictor call, so they cannot drift silently; ADR-0004 item 5's full PDC trigger set is
wired (`prepareToPlay`, three `int_` onChanged, `setNonRealtime`); the OQ-013 Hard Stop is
honoured — `frozenTrims` is serialized per slot and *nothing* pushes it toward the engine, with
the stop restated at the exact code site a P2 author would touch.

**Defects found and fixed during the build loop** (each caught by running something, which is the
lesson of this whole branch): `scripts/setup-linux.sh` broke as root — `$SUDO VAR=x cmd` puts the
assignment out of assignment position at parse time, so with `$SUDO` empty it became the command
name; CI's sudo path never exercised it (green-means-correct again). The registry snapshot
compared CRLF against LF (`replaceWithText`'s default line endings). And `replaceState` leaked the
additive `raw` annotations into the live tree, making save→load→save non-byte-identical — the
annotations are now stripped at the load boundary, which is what makes the round-trip test's
byte-identity claim true rather than approximately true.

**Known P1 holes, recorded not hidden:** KI-001 (A/B swap runs duck-less until §2.8 lands at P2);
dither modes inert by construction until P2; EQ/comp/clip stages pass-through. Remaining to close
P1: Windows/macOS CI runs, `THREAD_MODEL.md`, `PARAMETER_REGISTRY.md`.

### Seventeenth post-sign-off pass — one fix: indentation was counted in characters, not columns

**One actionable item in the whole review, and it is in the lint again.** `indented_code_mask`
measured indentation with `len(line) - len(line.lstrip())`, which counts *characters*. CommonMark
advances a tab to the next four-column tab stop, so a single tab is four columns — but one
character. A tab-indented example therefore failed the `>= 4` test, was never masked, and had its
contents inspected as document structure: a table, link or quote shown that way produced findings on
prose GitHub renders as code, and the `docs` job would have exited 1 on a correct document.

Fixed with an `indent_columns()` helper used at all three sites that measured indentation (the
qualifying test, the list-context predecessor test, and the run-extension loop). Pinned four new
cases — tab table, tab link, `"  \t"` reaching column four, and a tab-nested table under a list
marker that must **still** be checked — plus the existing space-indented cases as regressions. The
corpus uses no tab indentation, so this was latent; the reason to fix it anyway is that the failure
mode is a red CI run pointing at correct prose, which is the one outcome that makes a lint get
switched off.

**Two limits found while fixing it, both stated rather than papered over.** An indented code block on
a file's *first* line is not masked (the mask requires a preceding blank line, which CommonMark does
not) — no file opens that way. And `FENCE` still measures its own three-column allowance in
characters, so a tab-indented ` ``` ` reads as a fence opener where CommonMark would call it indented
code. Both readings mask the block, so no finding differs — except that an *unpaired* tab-indented
fence line would be reported as an unclosed fence. Left deliberately: tightening `FENCE` would trade
that narrow case for the risk of a spurious unclosed-fence report, which is the louder failure, and
the last three passes have shown that widening a matcher to close a hypothetical gap is how this
script acquires real ones.

**Declined, unchanged** — both for the fifth-plus time, and neither is presented as a defect by the
reviewer: the ring-read / OpenGL tension (ADR-0011 records it and defers to `THREAD_MODEL.md` at P1)
and the `docs` job's same-repo-PR skip (documented in the job's comment block; whether branch
protection should require it is an owner decision, not a repository edit).

### Sixteenth post-sign-off pass — two latent lint false positives, one uniform-failure gap, one stale snapshot

**Nothing in the design set moved this pass.** Three of the four fixes are in the lint; the fourth
is a status row that the lint's own CI job made stale. The reviewer independently re-verified the
amendment bookkeeping (five policy-amending ADRs against the four policies they touch), the
49/9/40 parameter split, the fixed-point rule at `(0,0,0)`, and OQ-013's propagation — all clean.

- **The link matcher truncated at the first `)`.** `\[[^\]]*\]\(([^)]*)\)` cannot see a destination
  or title containing a parenthesis, so `[t](docs/a(1).md)` became the non-existent path
  `docs/a(1)` and would have been reported broken. Replaced with a scanner that counts paren depth,
  suppresses counting inside a quoted title, and skips an angle-bracketed destination whole; an
  unterminated `](` yields nothing, which is what CommonMark does too. Latent — no such link exists
  today — but this is the false-positive class the docstring names as the reason lints get ignored,
  and the parenthesised-title form is ordinary markdown.
- **A non-UTF-8 file crashed the job with a traceback** instead of producing a finding. The `docs`
  job's whole contract is `path:line: message`; a stray legacy-encoded byte in a corpus this full of
  typographic characters (— · ⊕ ≥) is a plausible accident, and a `UnicodeDecodeError` stack trace
  is not something a contributor can act on. Now a finding, with the offending byte, its offset and
  the line it falls on.
- **Pointing the lint at a non-Markdown file reported "0 file(s) clean"** and exited 0 — fixed in the
  previous commit; noted here because it is the same *false pass* family as the two above: the tool
  saying "fine" about something it did not examine.

**The `docs` job made two status records stale, and this is the second-order effect worth naming.**
`HANDOVER.md`'s Build Status row and `REPOSITORY_MAP.md`'s workflow note both said the P0 scaffold
never reports a red build, because all build/analysis workflows self-skip until `CMakeLists.txt`
appears. That stopped being true the moment the `docs` job was added *outside* the `preflight` gate —
which was the correct design (pre-P1 the documentation is the deliverable) but is exactly what those
two sentences deny. Both now say a pre-P1 run is not all-skip, and that the docs job is the one thing
that can legitimately go red before `src/` exists. Adding a CI job is a documentation-affecting
change under `DOCUMENTATION_LIFECYCLE_POLICY.md`'s trigger map; the map routed it to `CI_CD.md` and
`TESTING.md`, both of which were updated, but the *status snapshots* are not on that map and were
missed.

**Checked and found correct, not changed:** `indented_code_mask`'s run-consumption across blank
lines. A run that qualifies as top-level indented code continues through blank lines to any further
4-column-indented content — which is precisely CommonMark's rule, so a "table" indented that far in
the same block is not a table. Verified that a list marker breaks the run and the table nested under
it *is* checked. The asymmetry the reviewer flagged is the correct behaviour rather than a limit to
document. **Declined, unchanged:** the ring-read / OpenGL tension; the `docs` job's same-repo-PR
skip; blockquoted tables outside check 1.

### Fifteenth post-sign-off pass — a fence rule the lint had backwards, and one file in two places

**`check-docs.py` treated an info-string fence line as a closer.** CommonMark §4.5 is explicit: a
closing fence may carry nothing but trailing whitespace, so a line with an info string is an
*opening* fence and can never close anything. The mask compared only the fence character and run
length, so in a `` ```markdown `` example containing `` ```cpp ``, the inner line closed the outer
block — the example's contents were then scanned as real structure (a false "table fragment") and
the real closer *re-opened* a block that read as unclosed. Two invented findings on valid markup,
which is the failure the module docstring calls the worst outcome. Fixed by requiring the closer's
remainder to be blank, and pinned: the reviewer's exact shape, plus the case that must still fire
(a genuinely unterminated fence) so the fix does not buy silence with a blind spot.

No corpus document nests a fenced example today — but `TESTING.md` now tells contributors to run
this lint, and a documentation page quoting fenced markdown is the obvious next thing someone
writes.

**A fixture of mine was wrong before the code was.** My first self-test case for the fix had five
lines, the fifth a bare fence that legitimately opens an unterminated block — so the test failed
and the code was right. Caught by running it, corrected to four lines. Worth recording because the
reflex under "don't add bugs" is to trust the fixture and change the code.

**`PluginEditor` had two paths across three Accepted ADRs.** ADR-0008 owns `ANABASIS_PLUGIN_SOURCES`
— the list both the plugin target and `AnabasisStateTests` read verbatim — and places the editor at
`src/gui/PluginEditor.{h,cpp}`, consistent with `DESIGN.md`'s `gui` module rows and with every other
`src/gui/` file. ADR-0009, ADR-0010 and ADR-0005 wrote `src/PluginEditor.{h,cpp}`; ADR-0009 did so
*on the same line* as a correct `src/gui/FrameClock.h`, which identifies the cause: **Anamorph really
does keep its editor at `src/PluginEditor.h`**, and the path came across with the idiom. All three
corrected to ADR-0008's path. The five `Anamorph:src/PluginEditor.h` evidence citations are
deliberately **not** touched — they describe the sibling's real layout and are correct as written.

**One false pass in the lint's argument handling:** an existing file with a non-`.md` suffix took
neither the `is_file()` branch nor `rglob("*.md")`, so `check-docs.py notes.txt` printed
"0 file(s) clean" and exited 0. Unreachable from CI, which passes no arguments, but reachable by the
contributor `TESTING.md` now instructs. Now an error.

**Already fixed at HEAD, re-reported against an older SHA:** `TESTING.md`'s documentation-lint
section, which discharges the `DOCUMENTATION_LIFECYCLE_POLICY.md` trigger-map row for CI-workflow
changes. **Declined, unchanged:** the ring-read / OpenGL tension; the `docs` job's same-repo-PR skip;
blockquoted tables being outside check 1 (all three already recorded, the last in KNOWN LIMITS).

### Fourteenth post-sign-off pass — the undercount had siblings, and the bulk swaps were one name for three paths

**The ADR_INDEX undercount fixed last pass existed in two more records, and in a third form.**
`HANDOVER.md`'s phase summary and this file's own phase-boundary entry both still said "Three
`DSP_POLICY.md` invariants amended" — the count predates ADR-0004's invariant-8 amendment and was
never revisited, exactly as the index's "three ADRs" wasn't. Four invariants are amended (1, 2, 5,
8). Both records now say so, and `HANDOVER.md` explicitly defers to `ADR_INDEX.md`'s five-ADR table
as the registry of record rather than carrying a second count to keep in step — the lesson of the
last three counting defects is that a number duplicated across records *will* drift, so the copies
now name their source.

**Invariant 8's enumeration named one bulk swap out of three.** `DESIGN.md` §2.8 routes
"preset/A-B/undo bulk swaps" through the same forced duck, but the click-free enumeration said only
"a preset load" — and the previous correction note's claim that "every one" of §2.8's rewires
appeared save the colour model overlooked this. Under a guard that reads "one per switchable path",
the ambiguity decides whether two click tests exist. All three routes are now named in the policy
and in ADR-0004's prescribed block (byte-identical), with the shared mechanism stated in the same
breath — the mechanism is one, the tests are three, because what a test exercises is the route into
the duck (preset apply, A/B restore, undo pop), not the duck itself. A third dated correction note
owns the previous note's overreach.

**The lint's delimiter-row pattern demanded a trailing pipe GFM does not require.** `|---|---` is
as valid as `|---|---|`, but `SEPARATOR` rejected it, so a correctly rendered table would have
failed CI — the false-positive class again, in the fourth of the four checks this time. The same
pattern also accepted `|   |` (no dash) as a delimiter, a false *negative* hiding a headerless
table. Both directions fixed in one regex (at least one dash required, trailing pipe optional) and
both pinned in the self-test; the mixed form (a delimiter row that omits its *leading* pipe under a
piped header) joins KNOWN LIMITS with the other pipeless shapes.

**Two citation/routing fixes.** ADR-0002's scope note attributed the +12 dB post-limiter shelf
argument to "item 2", which is the `eqPosition`-mobility item; the argument lives in the Problem
statement and option A's rejection, and the note now says so. And
`DOCUMENTATION_LIFECYCLE_POLICY.md`'s trigger map routes CI-workflow changes through `TESTING.md`,
which had never mentioned the `docs` job — it now documents the two commands the job runs and why a
docs lint is recorded in a testing procedure at all.

**Declined, unchanged:** the ring-read / OpenGL tension (P1, `THREAD_MODEL.md`); the `docs` job's
same-repo-PR skip (documented in the job's comment block; whether branch protection should require
it is the owner's call, not a repo edit). **Confirmed by the reviewer:** the numeric sweep again,
and the lint's own measurements.

**Post-commit adversarial verification caught two residues of this very pass's fix** — three
read-only agents were run against the edited spots before this entry closed, and two findings
survived:

- `ADR_INDEX.md`'s row 0004 — the table this pass declared "the registry of record" — described the
  invariant-8 amendment as gaining the phase mode, lookahead and colour model, *omitting the
  bulk-swap expansion made in the same pass*. The registry undercounted the amendment it was
  created to count. Completed.
- ADR-0004's own evidence line still read "Policy amended by this ADR: … invariant 2", while its
  block 8(c) amends invariant 8 too — the sibling ADRs' equivalent lines list complete sets. Now
  "invariants 2 and 8".

Also from that verification: invariant 8 lacked the dated `*(Amended by ADR-0004, 2026-07-31.)*`
stamp invariants 1/2/5 carry (added — the stamp is appended after the prescribed text, so the
verbatim prefix match is unaffected), and the delimiter-row pattern is looser than cmark-gfm for
cells with internal spaces — a false negative in the tolerated direction, now stated beside the
regex. The lesson stands another round: **an edit that adds to a set must update every record that
enumerates the set, including the ones created in the same pass.**

### Thirteenth post-sign-off pass — two stale records, one truncated sentence

**The registry of binding decisions undercounted its own amendments.** `ADR_INDEX.md` said "Three of
them amended a Policy" and named only the `DSP_POLICY.md` edits (ADR-0002, ADR-0003, ADR-0004). Two
more do: **ADR-0005** amends `MODE_AND_ADAPTATION_POLICY.md` invariants 1, 4, 5 and 6 plus
`PARAMETER_COMPATIBILITY_POLICY.md` rule 7, and **ADR-0011** amends `THREADING_POLICY.md` in three
places. Both carry explicit *Policy amendments enacted by this ADR* sections, and every one of those
edits is present in the tree.

The consequence is specific and bad: `ADR_POLICY.md` rule 5 makes an ADR the *only* instrument that
changes a policy, so someone auditing the sign-off against this index would find two rewritten rule
documents with no listed authority — and would be entitled to conclude they were changed without
one. The count was written when only three ADRs amended policies and was never revisited as
ADR-0005 and ADR-0011 grew their sections.

Replaced with a **table of the five**, naming for each the policy, the specific invariants or rules,
and what changed — because the failure here was that a bare count carried no way to check itself.
The distinction the index now records: three ADRs amend inside a decision item, two in a dedicated
section, and both forms are equally binding.

**A sentence in ADR-0004's Related-code paragraph was truncated mid-clause** — "…and `DESIGN.md`
§3.3's)," — an editing artefact left by the pass that relabelled the PDC trigger count. The
substantive content (three inputs, plus the transition, plus `prepare()` as a fifth call site) was
complete and consistent with `DSP_POLICY.md` invariant 2 and `DESIGN.md` §3.3, so nothing was
miswired; only the sentence was unfinished. Completed.

**One more limit added to the lint's KNOWN LIMITS**: GFM accepts tables written without a leading
pipe (`A | B` / `---|---`), and `TABLE_ROW` requires the pipe to be the first non-whitespace
character, so such a table is invisible to check 1. Matching the pipeless form would mean treating
any prose line containing a `|` as a candidate row — the false-positive direction this script
refuses — so the limit stands and is now stated. The repository writes leading pipes throughout.

**Declined, unchanged:** the ring-read / OpenGL tension. **Noted, no change:** the `docs` job's
same-repo-PR skip, already documented in its comment block. **Confirmed by the reviewer, twice
over:** the linter's own measurements reproduce (37 self-test cases, 50 files clean,
`indented_code_mask()` exempting zero corpus lines, ~96.8 % of lines in scope), and the full numeric
sweep of `DESIGN.md` §4.2/§5.5 — 49 rows, nine non-automatable, the fixed-point property, every
curve maximum, the interpolator group delay, the ten `int_` fields, the 7/2/1 split, Tape Glue's
40 % cap and the wireframe arithmetic — is consistent for the fifth pass running.

### Twelfth post-sign-off pass — the widening from last pass had a cost I had not measured

**Last pass widened `check_tables` to match a table row at any indent, and that created a
false-positive class.** CommonMark has two code-block forms and the mask only knew one: a
**four-space-indented** block is code too, and its contents were being examined as document
structure. So an author illustrating table, link or quote syntax in indented form — the ordinary
alternative to a fence — got a red CI run on a valid document. All three checks were affected;
reproduced against the committed script before changing anything.

The cause is worth stating plainly, because it is the same error twice in two passes: I widened the
match to close a *hypothetical* gap (tables nested deeper than three columns) after measuring that
**zero such tables exist**, and did not measure what the widening let in. A change with no measured
benefit and an unmeasured cost is not a safe change.

Fixed properly rather than by reverting, because reverting would restore the original blind spot:
`indented_code_mask()` now masks indented code blocks alongside fences. It is deliberately
conservative — a run qualifies only when CommonMark's own precondition holds (preceded by a blank
line, so it cannot be paragraph continuation) **and** the nearest preceding non-blank line is at
column 0 and is not a list marker. That last clause is what keeps a table nested in a list item —
also indented four or more columns — in scope. Both directions are pinned: four cases that must now
stay silent, three that must still fire (bullet-nested table, number-nested table, and an indented
run with no blank line before it, which is paragraph continuation and not code).

**Measured, not assumed:** the new mask exempts **zero lines** of the current corpus, so it removes
a future false-positive class without giving up any coverage today. That is the check I failed to
run last pass.

**The self-test count was a hand-maintained literal, and it had already drifted.**
`total = len(cases) + 2 + 5 + 3` understates itself the moment an assertion is added to one of the
three trailing blocks — which is exactly how `REPOSITORY_MAP.md` came to advertise 29 cases against
a script running 30. The total is now **counted as the assertions execute**, and the figure has been
removed from `REPOSITORY_MAP.md` and from this file's two references rather than corrected in three
places: a number duplicated across records is a number that will drift again.

**Two docstring inaccuracies, both overstating what the tool does.** Root-relative destinations were
documented as resolving "against the scan root"; `main` always passes the repository root, so
`check-docs.py docs/policies` still resolves them repo-wide — the behaviour is the more useful one
and the wording was wrong. And two real coverage gaps the reviewer identified were unlisted:
**blockquoted tables are not checked at all** (deliberately — a prescribed block often quotes a
single row, which has no separator and would be reported as a fragment; the enacted copy is checked
in the policy file), and an indented code block *inside* a list item is not masked. Both are now in
KNOWN LIMITS with their reasons.

**Noted, no change:** the `docs` job reports as skipped on same-repo PRs, so making it a required
status check works only if branch protection accepts a skipped conclusion as passing — worth
confirming with whoever configures it. Already stated in the job's comment block. **Declined,
unchanged:** the ring-read / OpenGL tension. **Confirmed by the reviewer:** invariant 2's "all four"
is not exclusive and does not contradict `prepare()` being a fifth call site — the earlier defect
was the opposite shape (an "only by" list omitting a real trigger), so the current wording is the
safe direction.

### Eleventh post-sign-off pass — one DSP-relevant omission, five precision fixes

**The one with audible consequence: `DSP_POLICY.md` invariant 8 never named the colour model.**
`DESIGN.md` §2.8 lists the genuine discrete rewires that get the asymmetric raised-cosine duck as
"OS factor/phase change, EQ position, **colour model**, preset/A-B/undo bulk swaps", and ADR-0010's
duck-routed note names `colourModel` (row 23) beside `eqPosition` (row 45). Every one of those was
in invariant 8's enumeration except the colour model — which appeared **nowhere in `docs/policies/`
at all**. Since the invariant's guard is "one per switchable path", the enumeration *is* the list of
owed tests, so the click test least likely to be written was the one no rule named. This is the same
omission shape as the phase mode two passes ago, and ADR-0004's block 8(c) had already stated the
consequence in as many words. Added to the policy and to the prescribed block together. **No new
obligation is created** — the duck already covers that path by design; what was missing was the test
that proves it.

**Four precision fixes, all of them stated numbers or stated reasons that were wrong.** The
repository treats an unevidenced justification as a defect in its own right, and each of these is
one:

- **`.gitignore`'s rationale described something the script does not do.** It said
  `check-docs.py` "leaves this behind when imported (e.g. by its own `--self-test` run)". Running a
  script as `__main__` writes no bytecode cache; the directory appears when the module is
  *imported*, which is how ad-hoc verification snippets exercise it. Reason corrected; the entries
  themselves were always right.
- **ADR-0004's Related-code line labelled four things "latency-input triggers"** while quoting
  invariant 2's "three inputs … plus the transition … all four" in the same sentence. The call
  sites were complete and unambiguous, so nothing would have been miswired — but the label folded
  the transition into the *inputs*, which `DESIGN.md` §3.3 explicitly separates. Relabelled to the
  policy's counting: three inputs, plus the transition, plus `prepare()` as a fifth call site.
- **The coverage figures in the previous entry were 61 lines stale**, having been measured before
  the final edits to the file they describe. The ratio (~97 %) is the durable claim; the absolute
  counts move with every edit to the corpus, *including edits to this file*, so they are now
  re-derived rather than quoted — a number that cannot be kept true should not be written down.
- **The lint's own KNOWN LIMITS list had gone stale in one entry and was silent on another.** It
  still claimed "code spans are matched within a single line" after the previous pass made the
  matching paragraph-wide, and it did not mention that a fence written inside a blockquote does not
  open the mask. Both corrected — a limits list that overstates coverage is worse than no list.

**One real gap closed, with the measurement to say what it did and did not change.** `check_tables`
matched pipes at up to three columns of indent (GFM's rule), so a table nested deeper — where a
container's content column commonly sits — was silently skipped. Widened to any indent, corpus still
clean. Measured honestly: the corpus contains **10 table rows at 1–3 columns and none at 4+**, so
this changes nothing today and removes a blind spot for tomorrow. Pinned with a self-test case
rather than left as a claim.

**Noted, no change:** the `docs` job is skipped on same-repo PRs (the branch push already ran it for
that SHA), so it reports as skipped in a PR's checks list, and the `workflow_call` hazard documented
for `preflight` applies to it verbatim. Both consequences are now written into the job's comment
block. Nothing gates on this job, so the failure mode is a missing check rather than a green run
with zero builds.

**Already fixed at HEAD, re-reported against an older SHA:** the phantom fence in this file, the
unclosed-fence diagnostic, and inline-code-span exclusion — all three landed in the previous commit.
**Declined, unchanged** (fifth and sixth time for two of them): the ring-read / OpenGL tension;
ADR-0006's base-rate clip trade. **Confirmed by the reviewer independently:** OQ-013's Hard Stop is
propagated to every record that names the trim-vector transport, with the precedent citations
correctly left as statements about Anamorph's code rather than authorisations.

### Tenth post-sign-off pass — the lint reported clean on a file it had not read

**The finding that matters: `check-docs.py` printed "50 file(s) clean" while skipping 1382 of the
1401 lines of this file.** Line 20 of the previous entry began, after two spaces of indent, with
three backticks — prose describing fenced blocks. CommonMark reads that as an opening fence whose
info string is the rest of the sentence, so (a) half a sentence and the two lines after it rendered
as a code block on GitHub, and (b) `fence_mask` masked every line from there to EOF. Both status
tables in this file, every link in it, every blockquote: unchecked, silently, with a green result.

That is strictly worse than having no checker. It is also the third time in this branch that
*a passing result was mistaken for a correct one* — after `--random-seed 0` and `build.sh`'s exit
code — and the second time in three passes for this script specifically. The prose defect is fixed
(the line now reads "triple-backtick fences"), and four changes make the class detectable:

- **An unclosed fence is now a finding**, not a silent exemption. `fence_mask` returns the opener's
  line number and the run reports it. An unclosed fence is a real rendering defect on its own — the
  rest of the file renders as code on GitHub — so this catches a document bug *and* closes the blind
  spot with one check.
- **Coverage is now measured, not assumed.** The exempt lines are inside genuine fenced blocks and
  no file is more than half masked, so "clean" has a denominator behind it. The *ratio* is the
  durable claim (~97 %); the absolute counts move with every edit to the corpus — including edits to
  this file — so they are re-derived rather than quoted here. To reproduce, iterate
  `markdown_files()` and sum `fence_mask()` over the tree.
- **A `docs` job runs the lint in CI** (`build.yml`), deliberately outside the `preflight` gate and
  outside every build job's `needs`: it must run in the pre-P1 scaffold, and a prose defect should
  fail the run without skipping a binary. `--self-test` runs first, because a zero exit is not
  evidence unless the script's own guarantees were exercised in the same run.
- **`--self-test` grew** to cover the shapes this review named.

**Three more false-positive classes in the same script, all reported and all real.** Inline code
spans were not excluded, so the illustration `` `[t](path "Title")` `` in the previous entry was a
"broken link" the moment the phantom fence stopped hiding it; a fence closer shorter than its opener
ended the block early; and `check_tables` only matched pipes at column 0, so every indented table —
including ADR-0003's detector-rate table and ADR-0007's schema table, exactly the nested tables where
a mid-table intrusion is hardest to see — was never examined at all.

**Fixing those exposed a fourth, and the corpus run is what caught it.** With indented tables now in
scope, ADR-0003 line 100 was reported as a table fragment: it begins `|| isMod)` — the continuation
of a code span opened on the *previous* line (`` `oversample != Off && (drive > 0.01 || isMod)` ``).
Code spans may wrap across lines within a paragraph, and the blanking was line-local. Fixed at the
root — spans are now matched over each paragraph and split back with lengths preserved — rather than
by special-casing the symptom. **This is the method paying off:** the run was not treated as done
when it exited non-zero on one file; the single finding was chased to a cause, and the cause was a
real gap rather than a nuisance to suppress.

**Two limits are now stated rather than implied** (C7), because the reviewer was right that the
docstring claimed more than the code does: link existence is checked against the filesystem, so a
case-mismatched path passes on macOS and 404s on GitHub; and reference-style links and autolinks are
not checked. Root-relative (`/docs/x.md`) and percent-encoded destinations now resolve correctly.

**Declined, unchanged** (fourth and fifth time for two of them): the ring-read / OpenGL tension;
ADR-0006's base-rate clip trade; and invariant 4's wording, again confirmed literally true.

### Ninth post-sign-off pass — the lint itself was the defect; six fixes, three declined

**Three bugs in `scripts/check-docs.py`, all mine, all shipped last pass.** The script was written to
stop a class of regression and was itself the regression: on documents it had no business rejecting,
it would have failed. It scanned clean only because no current document happens to exercise any of
the three paths.

- **No fence tracking.** Every line starting with `|` was treated as a table row, including inside
  triple-backtick fences — so any document showing table syntax as an *example* would be reported as
  a broken table. This file and the ADRs are full of quoted markup; the script's own docstring would
  have tripped it.
- **Lazy continuation over-reported.** CommonMark applies it only to *paragraph continuation text*.
  A quote ending in a bare `>` has closed its paragraph, and an ordered list starting at 1
  interrupts one — neither is absorbed, and both were flagged. The interrupter set also mistook
  prefix-matching for block detection (`*emphasis*` counted as a bullet).
- **Link titles broke link checking.** `[t](path "Title")` is valid markdown; the code stripped only
  a `#` fragment, so the destination became `path "Title"` and the file "did not exist".

Rewritten: a fence mask excludes fenced content from all three checks; `interrupts_paragraph()`
encodes the actual CommonMark interrupters (including the only-at-1 rule for ordered lists); and
`link_destination()` parses angle-bracketed destinations and all three title forms.

**A fourth bug appeared during that fix, and is the one worth recording.** The rewritten interrupter
patterns anchored at column 0 (`^\s{0,3}`), which is what CommonMark specifies — measured against the
*container's* content column. A line-based lint has no container stack, so inside a numbered ADR item
(five columns of indent) every quote line stopped counting as a quote, and the run reported **31**
findings in ADR-0004 alone. All 31 were false: each "absorbed" line was itself a quote line. Fixed by
stripping indentation before matching, which trades a few false negatives for zero false positives —
the correct direction for a check nobody is obliged to run. **The finding is the method, not the
patch:** the first version was verified only against a hand-written fixture and the clean tree, which
is exactly the "green means correct" mistake that produced `--random-seed 0` and the `build.sh` exit
code. A check must be run against the *real corpus* and every finding accounted for.

`--self-test` now pins all fourteen cases — five that must stay silent, four that must fire, five
destination-parsing forms — so the next edit to this script cannot quietly re-open any of them.

**Two residual singular descriptions of the frozen-trims transport**, the tail of the OQ-013
propagation. ADR-0011's Related-ADRs line ("restore uses **the inject atomic**") and ADR-0009's
copied-scope list (the pattern authorised for copy with no caveat, under a standing authorisation
whose only conditions are provenance and the item-5 list). Neither is as load-bearing as the
ADR-0005/ADR-0010 sentences fixed last pass, but both are records a P1 author could cite while
wiring the blocked path — and "almost fully propagated" is precisely what made the last pair
dangerous. ADR-0009's authorisation is now explicitly scoped to **single-scalar commands only**.

**Two rendering/terminology fixes.** `REPOSITORY_MAP.md`'s docs tree carried `**bold**` and backticks
*inside* the fence, where they render as literal asterisks — the same "invisible in source, wrong when
rendered" class the lint exists for, and one its three checks do not cover. Removed. (A sweep found
18 other fenced-emphasis occurrences, all of them **intentional**: the entry-format templates in
`FUTURE_RISKS`, `KNOWN_ISSUES`, `POSTMORTEMS` and `ADR_INDEX` show the literal markup an author is
meant to copy. Left alone.) And `DESIGN.md` §3.3 called the three `onChanged` callbacks plus
`setNonRealtime()` "those four … latency *inputs*", while `DSP_POLICY.md` invariant 2 says three
inputs **plus the transition** — the transition is a trigger, not an input. Same words, different
referents, in a spot this audit had just claimed to align. Re-worded to the policy's counting.

**Declined, unchanged and for the same reasons** (third and fourth time for two of them): the
ring-read / OpenGL tension; ADR-0006's base-rate clip trade; and invariant 4's wording, again
confirmed literally true.

### Eighth post-sign-off pass — six fixes, one new script; three declined

**The validation battery is now an artefact, not a habit — and the reviewer was right that it
wasn't.** Last pass claimed "a table-integrity check is now part of the validation battery", which
described a capability the repository did not have: no script under `scripts/`, no docs job in
`.github/workflows/`. That is exactly the unevidenced claim constraint C7 forbids, made by the audit
file that enforces C7. Fixed by building the thing: **`scripts/check-docs.py`** now runs three
mechanical checks over every `.md` — GFM table integrity, broken relative links, blockquote lazy
continuation — each one present because that defect shipped here at least once and was invisible in
the diff that introduced it. Verified against a fixture containing all three defects (it reports all
three, exit 1) as well as against the real tree (50 files clean, exit 0); a script that only ever
returns "clean" proves nothing. *(That verification was still insufficient — see the following pass,
where the "clean" result turned out to be false.)* It was **not** wired into CI at this point;
a **docs** job was added the pass after. Deliberately *not*
included: the ADR-prescribed-block ↔ enacted-policy comparison, which is still run by hand, because
it has documented cosmetic artefacts (headline bolding, dated attributions) and encoding those as an
allowlist would make the script assert more than it can check.

**Two Accepted ADRs still licensed the path a Hard Stop blocks.** OQ-013's gap was propagated to
ADR-0007, ADR-0011, `THREADING_POLICY.md`, `HANDOVER.md`, `README.md` and `DESIGN.md` §5.4 — but
**ADR-0005** item 10 ("the frozen vector serialized per A/B slot via the sentinel-atomic inject
pattern") and **ADR-0010** ("restored through the sentinel-atomic inject at the forced duck's silent
bottom") were missed, as was `DESIGN.md` §7. Since an ADR outranks both the policy and `DESIGN.md`,
a P1 author reading only those two records had an Accepted-ADR licence to wire the exact path the
Hard Stop forbids — the propagation being *almost* complete is what made it dangerous rather than
obviously incomplete. All three corrected, each keeping the property its own record actually needs
(per-slot travel), which holds under either candidate transport.

**Three records still described the latch sentence in its pre-fix form.** `DESIGN.md` §3.3's
sign-off summary, OQ-010's Decision paragraph and this file's "what the sign-off enacted" all said
the latch sentence "names only the oversampling factor" — the wording a previous pass corrected in
the policy and in ADR-0004's prescribed block precisely because latching the factor alone leaves a
phase switch free to move reported latency mid-block. The decision records (ADR-0004 item 4,
ADR-0003 item 4, §3.4) were always right; these three are summaries of the amendment, which is what
a reader checking "what did it change?" lands on. All now say the sentence drops the lookahead and
names **factor and phase mode**.

**Two clarifications where a reader had no sentence to cite.**

- **`dynTilt` runs at the oversampled rate**, being a sub-block of Clipper/Sat, while invariant 5
  enumerates "the EQ" as base-rate and grants exactly one exception. The reading was always
  defensible — invariant 5 enumerates *chain stages*, and "the EQ" is the `eqPosition`-mobile stage,
  not every filter in the path — but nothing said so. Noted in ADR-0002 item 2, where `dynTilt`'s
  not-a-stage status is already established, so **no policy text changed** and no new exception was
  created: `dynTilt` needs none, because the estimator's exception exists for a stage-external tap
  and `dynTilt` is not one.
- **`DESIGN.md` §3.3's PDC bullet read as an exhaustive call-site list** while naming only the four
  latency inputs. Aligned with ADR-0004's phrasing: four inputs, `prepare()` counted separately as
  the host re-establishing the model. Same shape as the miscounts fixed twice before, caught this
  time before it became one.

**Declined, unchanged from last pass and for the same reasons:** the ring-read / OpenGL tension
(acknowledged in ADR-0011, deferred to `THREAD_MODEL.md` at P1); ADR-0006's base-rate clip under a
dBTP tolerance (the ADR states the limitation itself; the guarantee is claimed only for
estimate-plus-backstop, and the P2/P3 gates would expose it); and invariant 4's wording, which the
reviewer again confirms "remains literally true" — re-wording a correct rule against a hypothetical
future edit is how two of the recent regressions started.

**Confirmed, no change:** the reviewer's independent re-derivation of the 49-row surface, the nine
non-automatable flags, the fixed-point property, every curve maximum, the 7/2/1 managed-set split,
Tape Glue's 40 % cap, the interpolator group delay, the wireframe arithmetic and the ten `int_`
fields; the full prescribed-block sweep; and — now scripted — that the OQ-013 note sits below the
permitted-path table with all seven rows intact.

### Seventh post-sign-off pass — seven fixes, two of them self-inflicted; three declined

**The worst defect in this set was invisible in the source and only existed when rendered**, which
is a class this audit had not hit before and now checks for mechanically.

- **A blockquote was inserted *inside* the permitted-path table** in `THREADING_POLICY.md` — the
  OQ-013 gap note added last pass landed after the fourth row. In GFM a table cannot resume after an
  intervening block, so the last **three** permitted paths (SPSC ring, meter atomics, staleness
  counters) rendered as a paragraph of pipe-separated text with no header, outside the table their
  own header governs. The source diff looked entirely reasonable; nothing but rendering shows it.
  Note moved below the table, immediately after "Any path not in this table is a new cross-thread
  path → Architecture Review Gate" — which is also where it belongs logically, since the missing
  edge *is* an instance of that sentence, and where both records already said it was ("under the
  table"). A table-integrity check was added to the validation battery in response — see the
  following pass, where it became an actual script rather than a described habit.
- **The PDC amendment note miscounted the recompute triggers** — it said "two of the **four**
  recompute triggers … `prepareToPlay` and `setNonRealtime()`", which puts `prepareToPlay` inside the
  canonical four and silently displaces one of the three host-hidden latency inputs. This is the
  *same* miscount a previous pass fixed in ADR-0004's Related-code line, reintroduced by a note
  written to fix something else. Fixed by **removing the count entirely** rather than correcting it:
  the sentence now reads "two of the recompute call sites ADR-0004 item 5 mandates". A number that
  does not appear cannot drift out of step with `DSP_POLICY.md` invariant 2. Applied to the policy,
  ADR-0011's prescribed block, ADR-0011's prose and this file's own narrative of the last pass.

**One more count, and three residual-reading fixes.**

- **`MODE_AND_ADAPTATION_POLICY.md` announced "Two consequences" and listed three.** The third is the
  macro-curve freeze — the obligation `RELEASE_COMPATIBILITY_CHECKLIST.md` and
  `PARAMETER_COMPATIBILITY_POLICY.md` rule 7 both rest on, so a reader checking against the stated
  count could have treated the one load-bearing bullet as stray. Now "Three".
- **ADR-0007's Decision body still read as if the trim transport were settled.** The gap is
  propagated everywhere else, but the bullet said "a sentinel-valued atomic" with the disclaimer
  trailing as a parenthetical — and since an ADR outranks both `DESIGN.md` and the policy, that
  singular would have won for anyone reading only the Decision. The disclaimer is now *in* the
  sentence: the transport is explicitly not fixed by that record, with OQ-013 and the Hard Stop
  named inline.
- **ADR-0002 asserted "Nothing else in invariant 1 changes"** while the enacted invariant carries a
  rationale paragraph and an amended `Guarded by:` line its prescribed block does not. Neither is
  unauthorised — the rationale restates the ADR's own item-2 argument, the guard change is item 7 —
  but ADR-0003 already carries a "scope of verbatim" note for exactly this situation and ADR-0002
  did not. Note added; no text moved.
- **`README.md` named two of the three P1/distribution blockers** and then deferred to the list of
  record. The pointer keeps it from drifting, but naming two of three reads as exhaustive. OQ-013
  added.

**Declined, with reasons.** The ring-read / OpenGL tension (reviewer agrees it is acknowledged, not
hidden; ADR-0011 defers it to `THREAD_MODEL.md` at P1). ADR-0006's base-rate clip under a dBTP
tolerance (the ADR states the limitation itself; the property is claimed only for the
estimate-plus-backstop combination, and `testOutputNeverExceedsCeiling` plus the invariant-11 matrix
are the gates that would expose it). Invariant 4's wording, which the reviewer notes remains literally
true and which no record contradicts — re-wording a correct rule to pre-empt a hypothetical future
edit is how the last two regressions started.

**One item the reviewer raised as a note became a decision worth recording.** `eqPosition` and
`colourModel` are automatable *and* duck-routed, so a stepped automation lane produces repeated
~34 ms dips. That is not an invariant-8 violation — the duck **is** the click-free mechanism, and a
smooth deliberate dip is not a level jump — and it does not make them candidates for the
non-automatable set, because unlike `lookahead` (a live read offset that cannot be swept at all)
a stepped rewire is well-behaved at every value it visits; only its *rate* is unmusical. Recorded in
ADR-0010 as supported-but-audible so P4 does not file it as a defect, with the note that changing
either flag after v0.1.0 is a `kVersion` bump + ADR.

**Confirmed, no change:** the reviewer's independent re-derivation of the 49-row surface, the nine
non-automatable flags, the fixed-point property, every curve maximum, the managed-set split by
driver (7/2/1), Tape Glue's 40 % cap, the interpolator group delay, the wireframe meter arithmetic,
the ten `int_` fields, and the prescribed-block sweep — all agree with this audit's own figures.

### Sixth post-sign-off pass — three fixes, one of them self-inflicted; one new open question

**The first item is a defect this audit's own previous pass introduced**, and it is worth recording
as such rather than as a neutral finding. The pass before this one added a seventh row to
`THREADING_POLICY.md`'s permitted-path table to cover the sentinel-valued command atomic. The row
said "carrying a value … **one value per slot**", named the **frozen trim vector** as its use, and
closed by excluding anything "multi-word" — three clauses that cannot all hold, because the trim
vector is **four** scalars (release, stereo-link, sidechain-HPF, dynamic-tilt — `DESIGN.md` §5.4,
ADR-0005 item 10). Read strictly the frozen-trim restore had no permitted mechanism at all; read
loosely, an implementer publishes four independent atomics with no ordering and consumes them
half-updated — and a half-consumed vector is a **permanently** half-restored slot, so a frozen A/B
slot renders differently from the slot that was saved, defeating the bit-repeatability
`MODE_AND_ADAPTATION_POLICY.md` invariant 3 requires of Freeze.

The cause is worth naming because it is the pattern behind most of the recent regressions: the row
was written to make ADR-0011's "every edge is in the table" claim true, and the claim was allowed to
drive the rule instead of the other way round. The `abMatchGain` precedent carries **one float**;
extending it to a vector was an inference, not a reading.

- **Fixed by narrowing, not by inventing.** The row now covers **one scalar**, which is exactly what
  the precedent establishes. The trim-vector transport is *not decided* — choosing between *N*
  parallel sentinel scalars with a stated ordering guarantee and a single release/acquire-gated
  per-slot POD is a thread-model decision (Architecture Review Gate + ADR + Hard Stop). It is raised
  as **OQ-013**, carried as an explicit gap in `THREADING_POLICY.md` under the table, excepted from
  ADR-0011's compliance claim, and pointed at from ADR-0007, ADR-0011 §Decision and `DESIGN.md` §5.4
  so no record reads as if the mechanism were settled. **P1 may not wire that restore path**;
  nothing else in the P1 skeleton depends on it.

**Two genuine defects in the accepted set, both created by an edit that removed one thing and
silently removed a second.**

- **`DSP_POLICY.md` invariant 2's latch sentence had lost the phase mode.** ADR-0004's amendment
  removed "or lookahead" from "an oversampling-factor **or lookahead** change is latched" and left
  only the factor — but reported latency is `maxLookahead + osLatency(factor, **phaseMode**)`, and
  linear-phase FIR stages carry group delay the minimum-phase path does not. Every other record has
  it right (ADR-0004 item 4, ADR-0003 item 4, `DESIGN.md` §3.4 all say "factor **or phase**"); the
  binding policy did not, so a P1 author following the highest-ranked rule could apply a phase switch
  immediately and move reported latency mid-block. Restored in the policy **and** in ADR-0004's
  prescribed block 8(a), which must stay verbatim-identical.
- **Invariant 8's click-free enumeration was missing the phase mode too**, while ADR-0003 item 9
  requires "OS factor and phase each get their own click-free path test" — so the test most likely
  to be skipped was the one the enumeration did not name. Added to the policy and to ADR-0004's
  block 8(c), carried there rather than in a new ADR-0003 block because two ADRs prescribing the
  same sentence differently is precisely the divergence the block format prevents.

**One rule that no implementation could have obeyed.**

- **"PDC/latency must be recomputed on the message thread"** — in `THREADING_POLICY.md`'s
  forbidden-access list and in ADR-0011's Decision ("recomputed **only** on the message thread").
  Neither `prepareToPlay` nor `setNonRealtime()` is a message-thread callback; hosts call them from
  their own setup/processing threads, and ADR-0004 item 5 mandates **both** as recompute triggers.
  The rule therefore forbade two of the call sites it required. The substance was never thread
  identity — it is that the predictor is `const` and race-free, that there is a single
  `setLatencySamples` call site, and that nothing recomputes PDC from `processBlock` — so both
  records now say **off the audio thread, never inside `processBlock`**, enacted as a prescribed
  block in ADR-0011. No property the original protected is lost.

**Unchanged, as agreed with the reviewer:** the ring-read / OpenGL-context tension. Explicitly
acknowledged in ADR-0011, deferred to `THREAD_MODEL.md` at P1.

**Also this pass:** four earlier-reported items were re-checked and found already fixed at HEAD
(ADR-0004's PDC trigger list and its Consequences sentence, the release checklist's macro-mapping
rationale, ADR-0007's conditional factory-mask rule, and the PR description's sign-off framing). The
phrases quoted against them survive in exactly one place — this file's own narrative of the passes
that fixed them. **No edits were made to any of the four.** Rewriting text that is already correct is
how several of the recent regressions entered, including the one at the top of this entry.

### Fifth post-sign-off pass — seven fixes, three confirmations

**The first defect in this set would have failed the first real build**, which is a different class
from anything the previous four passes found.

- **ADR-0008's target graph omitted `juce::juce_opengl`.** `DESIGN.md` §6.1 mandates an
  `OpenGLContext` attached on macOS and Windows (never Linux/X11), ADR-0009 carries that as
  copied-in scope and ADR-0011 reasons about components painting on that context — but the ADR that
  *owns the CMake target graph* listed the plugin's `PRIVATE` links as `AnabasisDSP` /
  `juce_audio_utils` / `juce_dsp` only. An ADR outranks `DESIGN.md`, so a P1 author working from the
  highest-authority record writes a `CMakeLists.txt` that either fails to compile the GL path or
  silently ships without it on the two platforms that use it. Fixed on **both** targets that need
  it: the sibling links it `PRIVATE` on its plugin target *and* its state-test target
  (`Anamorph:CMakeLists.txt:206,271` [Verified]), the second because the state test compiles the GUI
  sources and therefore the editor's context member — an omission there is a link error, not a
  missing render. `BUILD.md` §"One source list, two consumers" now says so too.
- **ADR-0008 said "Four declared targets" and enumerated five.** Item 4 declares two console apps.
  The miscount was inherited verbatim from `worklogs/2026-07-30-p0-anamorph-research.md:394`, which
  numbers to (5) under a "Four" heading — corrected there as well, since it is the evidence file
  every build claim cites. `AnabasisStateTests` is the one target that compiles the wrapper sources,
  so a reviewer checking against the stated count could have passed the build with the whole
  state / parameter-compatibility suite unbuildable. Same class as ADR-0004's trigger list last pass.

**Two policy records that outlived their reasons — the pattern this project keeps producing.**

- **`PARAMETER_COMPATIBILITY_POLICY.md` rule 7 still justified the macro-curve freeze by
  automation.** `MODE_AND_ADAPTATION_POLICY.md` invariant 6 was re-grounded on **recall** because
  the macros are non-automatable (ADR-0005/ADR-0010) and a lane on a managed parameter writes it
  directly without consulting the mapping — but rule 7 sits at the *same* authority level, so the
  corrected record did not outrank the uncorrected one, and a maintainer could have concluded the
  freeze does not apply and waved a post-release curve change through. The obligation was never in
  doubt; only its reason was false. Rule 7 re-grounded, enacted by a prescribed block appended to
  ADR-0005 (`ADR_POLICY.md` rule 5), and invariant 6's now-stale "independent of rule 7's automation
  framing" aside updated to point at the corrected rule.
- **ADR-0011 claimed conformance to a permitted-path row that did not describe its edge.** Its
  Consequences said every cross-thread edge "is one of its six rows", but the Decision routes the
  **frozen trim vector** (ADR-0007) through sentinel-valued per-slot inject atomics that *carry a
  value*, while the nearest row is a payload-free `atomic<int>` where the arrival is the whole
  message. The policy says "Any path not in this table is a new cross-thread path → Architecture
  Review Gate", so the claim was self-refuting. Rather than qualify it, ADR-0011 now **enacts a
  seventh row** for the `abMatchGain` idiom — value plus sentinel in one `exchange`, one writer, one
  consumer, a compile-time-bounded slot set — with the boundary stated explicitly so the row cannot
  be read as a licence for a general message queue (which *is* a thread-model change). Settled now
  because `THREAD_MODEL.md` is generated from this ADR at P1.

**Three smaller ones.**

- **ADR-0010's exclusion-tier table defined `preset-excluded` as "view tier + `freeze`"**, which made
  its "travels in A/B and undo" column false for the four view-tier members swept in — only `freeze`
  travels. A reader building the shared predicate from that row alone could have let `advancedMode`
  into A/B and undo, which is the X11 editor-resize crash path the next paragraph exists to prevent.
  Rows now list disjoint membership with the `view ∪ {freeze}` predicate stated separately — the
  phrasing `DESIGN.md` §4.2 and ADR-0004 already used ("*Preset-excluded* adds `{freeze}`"), so the
  tier name is unchanged and nothing else in the corpus moves.
- **The mandated true-peak matrix has one degenerate cell.** At OS Off no filter is instantiated, so
  `int_osPhase` cannot reach the estimator and `Off × linear` duplicates `Off × minimum`. Kept — a
  uniform sweep is harder to get wrong than a hand-pruned one — but named in ADR-0003 item 9 and in
  `TESTING.md` so a P3 implementer neither hunts for a phase difference at Off nor prunes further.
- **OQ-001 ended with the obligation the same sentence says is discharged** ("…and recorded in the
  P0 build-decision ADR", after "**Recorded by ADR-0008**"). OQ-003's sibling edit dropped that
  trailing clause correctly, so this was an inconsistency between two edits rather than a decision.

**Confirmed, no change:** the §4.2 arithmetic and count sweep; the prescribed-block ↔ enacted-policy
comparison (the emphasis/attribution differences are the artefact ADR-0003 now documents); and
`HANDOVER.md`'s Blocking-P1 claim. **Still open by design:** the ring-read / OpenGL-context tension —
the reviewer agrees it is acknowledged rather than hidden, and ADR-0011 defers it to `THREAD_MODEL.md`
at P1 rather than amending the policy's single-reader phrasing without grounds.

### Fourth post-sign-off pass — eight fixes, three confirmations

The theme this pass is **records that still describe a decision as open after it was taken**. Three
of the eight are that exact shape, and the worst of them sits in a *binding rules* file.

- **`THREADING_POLICY.md` still let a contributor choose where the adaptive engine runs.** Its
  "Adaptive engine — where it runs" clause deferred the placement to "an ADR before implementation";
  ADR-0011 took that decision and its Consequences section says in so many words that the clause "is
  discharged here". A P4 author reading only the policy — the file that is binding from day one —
  could have implemented a placement the Accepted ADR forbids. The clause now states the decided
  split: **feature extraction and adaptive trim slewing on the audio thread** inside the ≤ 0.5 %
  budget, **macro mapping (MacroEngine) on the message thread only**, with either alternative called
  out as a Hard Stop. §"Current model" gained the ADR-0011 pointer it lacked, so the `TODO (no code
  yet)` no longer reads as *no model yet*.
- **`AI_AGENT_POLICY.md` still routed the shared-module question to OQ-005**, which is `Resolved`.
  ADR-0009 chose copy-and-adapt with provenance headers, no shared module for v1, and a named
  revisit after v0.1.0 — so an agent proposing a shared module was contradicting an Accepted ADR
  while believing it was answering an open question. Now points at the ADR and marks the proposal a
  Hard Stop.
- **`ADR_INDEX.md` rated ADR-0008 `Partially Verified` on sibling-repo evidence.** The JUCE pin and
  the identity codes were read out of *Anamorph*, which is evidence about how Anamorph builds, not
  about how Anabasis does — Anabasis has no `CMakeLists.txt` to verify against. Downgraded to
  `Unverified` with that reasoning recorded, so the rating no longer borrows another repository's
  confidence.

**Two count/reference defects in implementation checklists** — the class of defect the previous pass
named, so they were looked for deliberately.

- **ADR-0004's Related-code line said "all four triggers" and then listed five.** The canonical four
  are the three host-hidden latency inputs plus the realtime→offline transition (`DSP_POLICY.md`
  invariant 2 states it that way); `prepare()` is a fifth call site but not a latency *input*. Split
  and labelled, so the ADR and the policy now count the same things.
- **`DSP_POLICY.md` invariant 5 carried a dangling `(item 6:` reference** that read as a
  self-reference to invariant 6 of the same policy rather than to ADR-0003 decision item 6. Fixed in
  the policy **and** in ADR-0003's prescribed amendment block, so the verbatim ADR↔policy match is
  preserved.

**Two transcription gaps and one typo.**

- **`TESTING.md`'s true-peak row dropped two of ADR-0003 item 9's axes** — *both phase modes* and the
  **clamp's tap** as distinct from the limiter's detector. A suite built from the procedure alone
  would have swept the OS factor and called the invariant covered, which is precisely the vacuous
  pass the mandated-stimulus table exists to prevent. Both axes restored, with the source narrowed
  to `ADR-0003 item 9`.
- **`DESIGN.md` §2.3's compressor sentence** had an unbalanced parenthesis and a stray line break —
  its third rewrite in as many passes, now closed out.

**One thing the mechanical check turned up that the review did not.** Diffing every ADR-prescribed
block against the enacted policy text byte-for-byte — rather than reading them — showed ADR-0003's
invariant 2 and 5 blocks differing from `DSP_POLICY.md` in emphasis and attribution only: the
policy bolds each invariant's opening sentence as a headline, stamps the dated
`(ADR-0003, 2026-07-31)` attribution rule 5 requires, and carries ADR-0004's later emphasis on
`**lookahead allowance**`. No clause differs. Left as it is — forcing byte-identity would make the
ADR reproduce the policy file's formatting conventions — but ADR-0003 now states the scope of
"verbatim" explicitly, so the next person to run this diff reads a recorded artefact instead of a
fresh divergence.

**Confirmed, no change:** the arithmetic and count sweep again (49 rows, nine non-automatable, the
fixed-point property, curve maxima, interpolator group delay, ten `int_` fields); and
`HANDOVER.md`'s Blocking-P1 claim against `OPEN_QUESTIONS.md`. **Deliberately not changed:** `THREADING_POLICY.md`'s ring rule ("no reads off
the message thread") versus ADR-0011 §6.1's OpenGL attachment. ADR-0011 records the tension and
explicitly declines to amend, deferring it to `THREAD_MODEL.md` at P1 — amending the rule anyway
would be a policy change with no ADR behind it (`ADR_POLICY.md` rule 5). A non-normative note now
sits under the rule so a contributor meets the nuance where the rule is, without the rule moving.

### Third post-sign-off pass — eleven fixes, one confirmation

Two of these are the *third* time the same fact needed propagating, which is itself the finding:
**a cross-record correction is not done until every record that carries the claim is fixed, and the
implementation checklists count as records.**

- **ADR-0004's Related-code list still named two of the four PDC triggers.** Its decision body was
  corrected last pass and its own correction note explains why the omission would ship a misaligned
  bounce — but the planned-code section a developer actually works from still read "PDC recompute on
  `int_oversample` / `int_osPhase`". Now all four.
- **`DESIGN.md` §3.3's "Remaining rules" bullet still gave the retired PDC-spray reason** for
  lookahead non-automatability, which footnote ³ and ADR-0004 had already replaced. It was also
  wrong for the OS controls in a second way: they are **host-hidden**, so "not automatable"
  understates them — they are not in the parameter tree at all. Rewritten to separate the two cases.

**Two governance gaps where an ADR mandated something without carrying its escape or its
amendment.**

- **ADR-0008 mandated `GIT_SHALLOW TRUE` + a commit SHA with no caveat.** `DEPENDENCY_POLICY.md`
  records that pairing as a documented CMake trap whose resolution is *drop `GIT_SHALLOW`, never the
  SHA* — but an ADR outranks a policy, so an author following the higher record had no sanctioned
  escape and the first real build could fail to configure with no permitted fix. The caveat and the
  fallback are now in the ADR itself, with the reason they must live there.
- **Two policy edits had no prescribed amendment block.** `DSP_POLICY` invariant 8's lookahead entry
  and `MODE_AND_ADAPTATION_POLICY` invariant 4's re-grounding were substantively authorised by
  ADR-0004 and ADR-0005 but not carried as verbatim blocks, unlike the invariant 1/2/5 edits — and
  this change set treats ADR/policy divergence as a defect. Both ADRs now carry prescribed blocks;
  ADR-0005 gained a *Policy amendments enacted by this ADR* section it was missing entirely.

**A rendering defect with a real consequence.** ADR-0011's same-day correction note ended mid-line
and the following unquoted lines were absorbed into it by Markdown lazy continuation — so two
sentences of **binding** contract (which thread owns the latency figure, and the dry-fill gate)
rendered as part of a historical aside a reader could dismiss. Blockquote closed; a scan of all
eleven ADRs found no other instance.

**Four smaller ones.** §10's ADR-0002 and ADR-0004 rows still read "**Must amend** … Hard Stop,
human review required" for amendments that landed at sign-off. §3.4 still framed the invariant 2
re-phrasing as future work while §3.2 had been switched to a "**Done:**" note for the analogous
ADR-0003 case. The compressor justification had its reason stated twice in one sentence — the
previous pass fixed the garbling and left the duplication. Invariant 5 diverged from ADR-0003's
prescribed text in two cosmetic spots (`ADR-0003:` for `item 6:`, and a re-worded closing clause);
now verbatim, with the attribution moved outside the quoted block. The release checklist's latency
item still said "at both ends of the lookahead range" where ADR-0004 requires **every** value —
a release run following only the checklist would have exercised the weaker property. And the blank
lines left by excising the three resolved entries were collapsed, since `README` now points at that
file instead of enumerating entries itself.

**Confirmed, no change:** the arithmetic and count sweep, for the third time — 49 rows / nine
non-automatable, the fixed-point property, curve maxima inside range, interpolator group delay, the
ten `int_` fields, wireframe meter values, and the cited worklog's existence.

### Second post-sign-off pass — eight fixes, one confirmation

The offline-render PDC gap was fixed in the *wrong two records first*. The previous pass corrected
ADR-0011 and `DESIGN.md` but missed **ADR-0004**, which is the record that actually owns the
latency contract and outranks `DESIGN.md` — its decision item 5 still said recomputation is
triggered "**only** by `prepare()` and by the `int_oversample` / `int_osPhase` `onChanged`
callbacks", contradicting its own next sentence, and its Consequences repeated "OS factor and phase
mode are the only remaining latency sources". A P1 author reading the highest-authority record
would still have shipped the misaligned bounce. Fixed there, and the fourth record with the same
hole — `DSP_POLICY.md` invariant 2 — now names all three inputs plus the realtime→offline
transition. The lesson is worth stating: *fixing a cross-record contradiction means fixing every
record that carries it, starting with the most authoritative*, not the one where it was noticed.

**Two more amendments had landed only on one side.** The release checklist's macro-mapping gate
still justified itself with "so a recorded macro-automation lane still sounds the same" while
citing the very invariant that had just been rewritten to say such a lane cannot exist — a gate
item a reviewer could dismiss as inapplicable, letting a changed curve ship. Re-grounded on recall,
with an explicit "do not dismiss this on the grounds that no such lane is possible". And ADR-0007
asserted unconditionally that "factory presets ship an all-clear mask", contradicting ADR-0005's
conditional rule at equal authority; ADR-0007 now carries the conditional form and an explicit
scope note (it owns where the mask is *stored*; ADR-0005 owns the rule).

**The verbatim-amendment test, applied to myself.** The previous pass treated an ADR/policy
*paraphrase* divergence as a defect. The same test showed `DSP_POLICY` invariant 5 carrying a
substantive clause — the true-peak-estimator exception — that ADR-0003's prescribed block never
authorised. Resolved by extending the ADR's prescribed text so the enacted policy and the text its
ADR authorises match verbatim, rather than deleting correct material from the policy.

**Three smaller ones.** ADR-0005's factory-preset bullet still restated the stronger unconditional
form two sentences after the conditional one — the exact shape the previous pass claimed to have
collapsed; now one statement. A compressor-justification sentence in `DESIGN.md` had been garbled by
an earlier edit into "confirmed or refuted by the P2 aliasing measurement *if the null tests say
otherwise*", which makes the verification contingent on an unrelated test; rewritten with the
band-limited-gain-signal reason ADR-0003 states cleanly, and with the consequence if it comes out
otherwise (the compressor moves inside the OS region — an ADR-0003 amendment).
`MODE_AND_ADAPTATION_POLICY` now names the **binary** alongside `testMacroDefaultIsFixedPoint`, so
the deliberate placement exception (behavioural guard in the state suite, because only that target
links the wrapper) is not "fixed" by a later contributor moving it somewhere it cannot compile.

**Confirmed, no change:** the arithmetic and count sweep again — 49 rows / nine non-automatable,
the fixed-point property, curve maxima inside their ranges, the interpolator group delay, the ten
`int_` fields, the wireframe meter values, and that the worklog every ADR cites exists.

### Post-sign-off review pass — thirteen fixes, one confirmation

The sign-off changed what is true; this pass fixed the places that had not caught up, plus one
genuine technical gap between two Accepted ADRs.

**The one that would have shipped a defect: offline renders reported the wrong delay.** ADR-0004
decision item 5 makes `int_offlineQuality = Force Max` render an offline bounce at 16× with the
reported figure under `isNonRealtime()` using the forced factor — so it is a **third** input to
reported latency. ADR-0011's PDC section listed only the OS factor and phase as recompute
triggers, and `DESIGN.md` §4.3 had the same gap. A P1 author following ADR-0011 would have wired
no recompute for a Force-Max change and, in hosts that do not re-`prepare` on entering offline
render, the host would compensate for the *live* factor while the render ran at 16× — a bounce
time-shifted against the rest of the project. Both records now list all three fields **plus
`setNonRealtime()`**, which is the only callback guaranteed to fire on that transition. ADR-0011
carries the correction inline as a same-day consistency fix against ADR-0004, not a reversal
(`ADR_POLICY.md` rule 4 governs reversals; this was an omission).

**A binding rule was justified by a scenario this architecture makes impossible.**
`MODE_AND_ADAPTATION_POLICY` invariant 6 froze the macro curves on the grounds that changing one
alters how a recorded automation lane plays back. It does not: the macros are non-automatable, a
lane on a managed parameter writes that parameter directly, and `M` is evaluated only on a macro
gesture. A maintainer checking the stated reason would find it false and could conclude the freeze
does not apply. The real argument is **recall**: every saved session and preset stores a macro
position, and re-mapping that position through a new curve makes a user's saved master reload
differently — a `COMPATIBILITY_POLICY` violation on its own terms, independent of
`PARAMETER_COMPATIBILITY_POLICY` rule 7's automation framing.

**Three ADR-mandated test stimuli were never transcribed.** `TESTING.md` had the test *names* but
not the stimuli each ADR flagged as load-bearing — and a name without its stimulus passes
vacuously. Now a table: `testOutputNeverExceedsCeiling` in **both EQ positions** with a +12 dB
post-limiter shelf (ADR-0002); true-peak accuracy across the **whole OS matrix**, because the
estimator's input path differs per setting (ADR-0003); the impulse landing at exactly
`maxLookahead + OS` for **every** lookahead value, and a **lookahead move** among the click-free
paths (ADR-0004). `DSP_POLICY` invariant 8's enumeration gained lookahead for the same reason —
it is the one switchable path with neither a duck nor a latch, so it is the one most likely to be
skipped.

**Two policy/ADR literal divergences.** ADR-0002 quotes its amendment verbatim; the policy had
paraphrased it, so a reader diffing the two would see an incomplete amendment — the prescribed
sentence is now used. And invariant 5's "metering taps stay at base rate" appeared to contradict
ADR-0003's estimator reading oversampled signal at ≥ 4×; the true-peak estimator is now named as
the explicit exception, with the reason (it is a measurement tap, not an audio capture point).

**Pre-sign-off language still standing in a signed-off document.** `DESIGN.md` still said
`ADR_INDEX.md` is empty and its §10 table the only record, still headed §10 "Proposed initial ADR
set", and still promised amendments to `DSP_POLICY` as future work in two Hard-Stop blockquotes —
all false as of the same change set. The header's drift disclaimer covers divergence from *shipped
behaviour*, not false claims about current repository contents.

**Four smaller ones.** ADR-0005 said a macro gesture is "nine parameter writes" — the managed set
splits by driver, so it is seven for `loudness`, two for `tone`, one for `character`. The factory
preset mask rule said both "all-clear" and "non-clear" four sentences apart; now phrased once,
conditionally. RISK-007 pointed at OQ-005, which this change set Resolved — re-aimed at ADR-0009
and its scheduled post-v0.1.0 revisit. `README`'s open-question summary omitted OQ-009; replaced
with a pointer to the list of record so it cannot drift again. The release checklist's `Ref:` line
still contemplated lookahead "gaining an explicit off position", which ADR-0004 forecloses.

**Confirmed, no change:** the count and arithmetic sweep — 49 rows with nine non-automatable
(matching ADR-0010), the fixed-point property across all nine managed parameters, every curve
maximum inside its declared range, the interpolator group delay inside the minimum lookahead, the
ten `int_` fields, and the wireframe meter values.

### What the sign-off enacted

The sign-off is the event the last five passes were building toward, and it changed the
repository's authority structure rather than just a status field:

- **`docs/DESIGN.md` → `Accepted`**, and per `SOURCE_OF_TRUTH.md` it now occupies **level 5**
  (descriptive Architecture) — outranked by every ADR it spawned, and superseded section by section
  as P1–P6 land. It is explicitly *not* maintained as a living spec.
- **ADR-0001…0011 authored, all `Accepted` and dated 2026-07-31**, registered in `ADR_INDEX.md`
  (`ADR_POLICY.md` rule 1 — an unregistered ADR is invalid). Level 3 of the authority chain is
  populated for the first time. Confidence is `Unverified` across the set by construction: there is
  no `src/`, so each is a contract the P1+ code must satisfy, upgraded as its code and tests land.
- **Four `DSP_POLICY.md` invariants amended, each by its ADR** (rule 5 — a policy changes only
  through an ADR). **Invariant 1** (ADR-0002): the chain now prints `… Limiter → [EQ (post)] →
  Ceiling → Dither` and states that the clamp is always last before dither in *both* EQ positions —
  the pre-amendment text left Post-EQ's placement relative to the clamp unstated, which a literal
  reader could take as EQ-after-clamp, making invariant 4 unsatisfiable. **Invariant 2**
  (ADR-0004): reported latency is now the **constant lookahead allowance + OS**, the latch sentence
  drops the lookahead and names the oversampling **factor and phase mode**, and the measurement-tap
  reading is asserted rather than left open. **Invariants 2 and 5** (ADR-0003): the open point is closed, so "oversampling off ⇒ no
  oversampling latency" is now asserted unconditionally. **Invariant 8** (ADR-0004 block 8(c)): the
  click-free enumeration gains the oversampling phase mode, the colour model, the lookahead and the
  three named bulk swaps — the enumeration is the list of owed click tests, so an unnamed path is a
  test nobody writes. Both invariant-1 and invariant-2 changes
  were **Hard Stops** — ratified by a human, which is the only thing that clears them.
- **`MODE_AND_ADAPTATION_POLICY.md`**: invariant 4's bar on adaptation moving the lookahead lost its
  original derivation when ADR-0004 removed the latency link; it is now grounded on its own reason
  (a time-varying read offset drags the tap through the delay line). Invariant 1 gained
  `testMacroDefaultIsFixedPoint` as its named guard — the correct home for it, since it guards a
  macro-layer rule rather than a DSP one.
- **Five open questions Resolved**: OQ-001 and OQ-003 (→ ADR-0008), OQ-004 (→ ADR-0005), OQ-005
  (→ ADR-0009), OQ-010 (→ ADR-0004). `OPEN_QUESTIONS.md` keeps them in its `Resolved` section with
  the date and the ADR, per its own never-delete rule.
- **P0 closed, P1 opened**: `HANDOVER.md` is now a phase-boundary snapshot carrying the
  `DEVELOPMENT_BRIEF.md` §13 phase summary (changes, next-phase plan, current risks, C++23 canary
  status), and `CLAUDE.md`, `README.md`, `REPOSITORY_MAP.md` and `SOURCE_OF_TRUTH.md` all moved off
  "P0, no code until sign-off". The lifecycle policy's phase-completion trigger fired properly this
  time — the previous pass had correctly identified that it had *not* yet fired.

### Review items handled in the same unit of work

The final review round's two authority defects both **dissolved** once the amendments landed rather
than needing a workaround: `RELEASE_COMPATIBILITY_CHECKLIST.md`'s latency checkbox now cites the
amended `DSP_POLICY.md` invariant 2 (not the design document, which had no authority to settle it),
and OQ-010's claim that the invariant *is* phrased against the allowance became true instead of
premature. Four smaller items fixed: the audit's ADR count (said nine, is eleven) and its
test-registration instruction (which contradicted `DESIGN.md` by sending the macro guard to
`DSP_POLICY`'s map); a duplicated `AB` child in the §4.4 schema listing; footnote ⁶ printed before
⁵; two unmarked ⊕ defaults in §4.3; and §5.3's inaccurate "ties all nine managed values to one
`l`" (three of the nine are driven by `character`/`tone` — the curves are jointly coupled to the
triple, which is what makes "heavy colour + gentle limiting" unreachable).

Prior: for the **P0 design document** (2026-07-30). `docs/DESIGN.md` created —
status `Proposed`, awaiting owner sign-off (`DEVELOPMENT_BRIEF.md` §11 exit criterion).

The document was produced from a five-domain research pass over the full Anamorph repository
(source, not just docs — §24 step 1), recorded with file:line evidence in
`worklogs/2026-07-30-p0-anamorph-research.md`. It contains: the two-layer architecture and
module inventory; per-stage DSP design; the oversampling/true-peak/latency contract — including
the **measurement-tap** resolution proposed for `DSP_POLICY.md` invariants 2/5's open point
(the policy text itself is edited only when ADR-0003 is accepted, per C6); the full
**49-parameter table** plus the host-hidden state table and serialization schema v1; the macro
layer and OQ-004 coexistence argument; draft macro curves (explicitly ⊕-marked as tuning
material, C2); UI wireframes with the family-consistency contract; the OQ-005 recommendation;
the performance-budget allocation and benchmark commitment; and the proposed ADR set — **eleven**
by the end of the review rounds (0008 build/identity and 0009 code reuse added in the third pass;
0010 parameter surface and 0011 threading in the fourth). Every
value the brief does not specify is marked **⊕ proposed** so sign-off ratifies it explicitly
rather than absorbing it silently (C7).

Synced in the same unit of work: `OPEN_QUESTIONS.md` (OQ-004/005/008/010 now carry the DESIGN
recommendations, all *pending sign-off* — none moved to Resolved, since the sign-off is the
decision event), `HANDOVER.md` (all P0 work items delivered; the phase stays open until sign-off),
`REPOSITORY_MAP.md`, `README.md` §Project status. Confidence: the design document is a
*contract proposal* — nothing in it is `Verified` about Anabasis (there is no code); Anamorph
precedent claims cite file:line and are `Verified` from the research pass.

**Fourth review pass on the design document (same day).** Eight review findings fixed, plus
**seven blockers of my own making** caught by an adversarial verification pass run *before*
commit — the constant-latency decision below had a blast radius I had not propagated.

### The self-inflicted blockers (verification pass, pre-commit)

Making §3.3's latency decision was correct; landing it in one section was not. A three-lens
verification sweep found the decision half-applied, and the pattern is worth recording because it
is the failure mode of any change to a *contract*: the new statement was written, the old one was
left standing in six other places, several of them more binding than the section that changed.

1. **§3.4 still asserted the old model** ("reported latency … phrased against the engaged
   lookahead") four paragraphs below the new formula.
2. **`DSP_POLICY.md` invariant 2's binding body was falsified without being flagged.** Its latch
   sentence names "an oversampling-factor **or lookahead** change" — under the decision only OS
   latches. A reported-latency change is an AI-agent **Hard Stop** and an Architecture Review
   item, and unlike the structurally identical §1.2 case it had no Hard-Stop framing. Now carried
   as a ⚠ blockquote in §3.3, an amendment obligation on ADR-0004, and a Hard-Stop checklist line.
3. **The §10 ADR-0004 row still specified the superseded contract** — and per `SOURCE_OF_TRUTH.md`
   the ADR is the binding artifact, so a P1 author would have written the rejected model into an
   `Accepted` record that then outranks §3.3.
4. **`lookahead`'s non-automatability was frozen on a rationale the decision destroyed** ("a host
   cannot spray PDC changes" — under constant latency it cannot spray anything). The flag is
   right, the reason was not: it is non-automatable because the engaged value is a *read offset
   into a live delay line*, so sweeping it at automation rate drags the tap through the buffer.
5. **The mask move's own precedent was never actually implemented.** The previous pass said the
   frozen trim vector "travels per-slot", but only §4.2's prose changed — §4.4 still stored one
   global vector in `ADAPTIVE`, so two frozen slots would have shared it and Freeze would not have
   been bit-repeatable. Both the trims *and* the mask now live inside each `AB` slot, and the
   **undo unit was widened to match** (`StateSet {params, presetName, baseline, frozenTrims,
   detachMask}`) — otherwise undoing an edit restores the value and strands its detach bit.
6. **The §4.2/§4.3 parameter surface had no ADR at all** — the checklist asked the owner to
   ratify 49 IDs, both exclusion tiers and the lockable set, and per the authority rule that
   ratification would have bound nothing. Exactly the defect the ADR-0008/0009 additions fixed one
   section over. Added **ADR-0010** (parameter surface) and **ADR-0011** (threading model), both
   mandatory subjects under `ADR_POLICY.md`.
7. **§2.8 removed the duck from lookahead without replacing its click-free coverage.** Moving a
   read tap is not inherently click-free. The mechanism is now stated — the *audio* delay stays
   fixed at 10 ms and only the detector alignment moves — with a per-path click test owed.

Three consequences reached beyond `DESIGN.md` and were corrected as drift (C6):
`RELEASE_COMPATIBILITY_CHECKLIST.md` had "the reported value is exactly the engaged lookahead" as
a **release-gate checkbox** a correct build would now fail; RISK-008 and OQ-010 carried the same
phrasing. `ADR_INDEX.md`'s expected-first-batch list gained the three new subjects.

Two arguments were also found to be *unsound rather than merely stale*, and were rewritten to
what is actually true. §5.3's case for presets carrying the detach mask claimed it prevents a
value jump — but rule 3 re-engages detached parameters anyway and preset apply lands stored values
either way, so the mask changes **no** trajectory; what it changes is what the UI claims and what
"reset to macro" can do. And "factory presets are curve-consistent by construction" was asserted
with nothing constructing it: §5.5 ties all nine managed values to one `l`, so a heavy-colour
gentle-limiting patch is not reachable from a single macro triple. It is an authoring constraint
checked at P6, not a property. An off-curve-but-engaged residue via ungesture-d automation writes
is now stated as **accepted** rather than left implicit.


*Two decisions the sign-off is asked to approve had no decision record, so they could never
become binding.* This is the sharpest finding of the four passes, because it is created by a rule
this very change set added: `SOURCE_OF_TRUTH.md` now says DESIGN decisions "become binding only
through the ADRs it names". The §10 table named seven, and **two mandatory records were missing**
— `CLAUDE.md` §3 requires code reuse across the two products to be "recorded in an ADR, not an
ad-hoc copy" (§8 decides copy-and-adapt *now* and only promised an ADR for the *later* extraction
question), and `ADR_POLICY.md` requires an ADR for build architecture *and* format support, with
OQ-001's standing obligation naming "the P0 build-decision ADR" explicitly. Ratifying §8 and the
JUCE-pin/identity values would have bound nothing. Added **ADR-0008** (build architecture +
plugin identity: CMake structure, the JUCE SHA pin, C++20 baseline, formats, frozen identity
codes — closes OQ-001 + OQ-003) and **ADR-0009** (code reuse from Anamorph — closes OQ-005).

*Reported latency was a function of a parameter that presets carry — decided rather than
documented.* `lookahead` is in neither exclusion tier, so every preset, A/B slot and undo step
carries one, and reported latency was `engagedLookahead + OS`. Browsing presets or A/B-comparing
**during playback** would therefore change host PDC on nearly every step, and the forced duck
could not dry-fill across it (Anamorph's dry-fill engages only when latency is preserved) — a
dropout in the middle of the one workflow a mastering plugin exists for. The research pass had
flagged this as a decision Anabasis owed and §7 had copied the machinery "wholesale" without
taking it. Resolved by making the lookahead contribution **constant at its maximum**: the limiter
reads at a variable offset inside a fixed 10 ms line, so the engaged value moves freely while the
reported figure never does. Consequences are stated rather than buried — bulk swaps can now
*never* cross reported latency (OS is host-hidden and not carried by presets/A-B/undo), lookahead
needs no latch and no duck, and the price is ~8 ms of PDC nobody asked for at the 2 ms default.
That price is on the §11 checklist as its own line: it is a genuine trade, not a free win.

*The macro detach mask was global state describing per-slot facts.* It lived in the single
`ADAPTIVE` child while A/B holds per-slot parameter trees — so slot A's detached `clipDrive`
would describe slot B, and the next macro gesture would re-engage parameters the user never
detached in the active slot. Exactly the hazard the previous pass fixed for the frozen trim
vector, missed one field over. Moved into each `AB` slot. The preset half of the same hole is
also closed, and in the opposite direction from the earlier draft: presets now **carry** the mask
(⊕, reversing "cleared on load"), because a preset saved after manual edits stores off-curve
values, and clearing the mask would mark them engaged so the next macro gesture would jump them —
the fixed-point defect re-entering through recall. With it stated that preset/A-B/undo writes are
ungesture-d *and* inside the re-entrancy flag, such a preset is recallable at all: without both,
the mapper would overwrite the stored managed values from the restored macro position.

*The ADR set's `Proposed` state never actually existed.* The header said sign-off promotes the
set from `Proposed` to `Accepted`, while §10 said they are written as `Proposed` *when approved* —
approval and sign-off being the same event, the state was instantaneous and no ADR would ever be
reviewable in it. Pinned to one order: authored **directly as `Accepted`, dated at sign-off, as
the first P1 task**, each citing this document and the worklog. Writing them speculatively as
`Proposed` beforehand is what C1 forbids, and this document already fills the reviewable-proposal
role — so `ADR_INDEX.md` stays empty until sign-off and the §10 table is the interim record,
which is why it names what each ADR must settle.

*Three smaller items.* The footnote markers ran ¹ ² ³ ⁴ then jumped to ¹⁰ with no 5–9 anywhere —
a leftover that would send a P1 transcriber looking for five missing notes; renumbered to ⁵. The
wireframe frame sizes (940×720 / 940×900) are Anamorph's hard-coded constants, which the research
pass said Anabasis must replace — they are ⊕-marked, but sign-off ratifies ⊕ values, so the
reader is now told plainly that ratifying them ratifies the sibling's exact frame and that P5 is
expected to re-derive them. And the three test names this document introduces
now carry an explicit registration obligation — *corrected in the fourth pass*, because two of
the three were already registered (`testReportedLatencyMatchesImpulse` is row 2 of `DSP_POLICY`'s
map; `testModeSwitchIsSoundNeutral` is the named guard for `MODE_AND_ADAPTATION_POLICY`
invariant 2) and the genuinely new one, `testMacroDefaultIsFixedPoint`, guards a **macro-layer**
rule, so `DSP_POLICY`'s map was the wrong destination for it.

*Confirmations:* the Post-EQ/`DSP_POLICY` wording item was re-reported for the third time and
remains correctly handled (Hard-Stop blockquote, ADR-0002 obligation, checklist entry). The
measurement-tap group-delay arithmetic, the nine managed rows' fixed-point property, every curve
maximum against its declared range, the wireframe meter values against §2.9's formulas, the 49-row
count against the §4.2 heading, and every unmarked value against brief §4–§5 were all
independently re-derived with no discrepancy.

*Housekeeping:* this file's review-pass blocks had drifted out of the newest-first order the rest
of the file uses (an unlabelled first pass sat above the third and second, with its confirmations
stranded two passes below its findings). Relabelled and reordered fourth → third → second →
first.

**Third review pass on the design document (same day).** Six findings fixed, three confirmations.

*The Character macro was inert in the factory patch.* `character` drives exactly one target,
`colourDepth`, and `colourModel` defaulted to **Clean** — the *null* model, per the brief's own
`Clean ↔ Colour` framing. Applying more of nothing is nothing, so the one-knob product's
second-most-prominent control would have looked dead until the user opened Advanced and changed a
discrete parameter the macro never touches. This is the **same class** as the `clipMix` defect two
passes ago, one level up: there a managed target's *value* was not a fixed point; here the value
matched but its *audible effect* was gated by an unmanaged discrete parameter. Fixed by defaulting
`colourModel` to ⊕ `Tape` (`colourDepth = 0` keeps the default patch bit-identical either way, so
invariant 7 is untouched), documenting that Clean-makes-Character-inert is deliberate — it is the
"no colour whatever the knob says" escape — and adding the generalised rule to §2.4: *a managed
target's audibility must not be gated by an unmanaged discrete parameter at the factory default.*
Which flavour is the default is a taste call and is now on the §11 checklist.

*`advancedMode` in A/B and undo meant a compare could crash an X11 host.* It sat in the
preset-excluded tier only, so an A/B switch or an undo step would change the view mode — driving
exactly the editor resize that the table's own footnote ¹ cites as the Anamorph X11 crash (its
KI-003). Moved to the **view tier** (excluded from A/B, undo and presets; still session-
serialized), a deliberate departure from Anamorph, which lets its `advancedMode` travel with A/B.
`freeze` deliberately stays in A/B and undo — it genuinely affects the sound, so reproducing a
slot means reproducing whether adaptation was latched — with the obligation that makes that safe
now stated: **the frozen trim vector travels per-slot with it**.

*The macro-write / manual-edit discriminator was never named.* §5.2 writes managed parameters
through `setValueNotifyingHost` and §5.3 detaches a parameter when the user edits it — both land
on the same listener surface. Taken literally the MacroEngine's own writes would trip the detach
rule and every managed parameter would detach on the first macro gesture, i.e. the exact inverse
of the re-engage contract. Now pinned as two required conditions: **not macro-originated** (a
message-thread re-entrancy flag around the write burst) **and gesture-bracketed** (so automation
playback, preset apply, A/B and undo never detach — Anamorph's rule). Comparing against the
expected curve value was considered and rejected: it is a float comparison against a value the
engine is mid-glide toward, so it misfires in both directions.

*Two open dependencies lived only inside a document that gets superseded.* §11 named the
detector-delay bound and the variable-font licence as risks, deferring the register entry
conditionally — but `DOCUMENTATION_LIFECYCLE_POLICY.md`'s *new unresolved limitation* trigger
points at `FUTURE_RISKS.md` for exactly this shape, and `DESIGN.md` is superseded section by
section, so a risk parked there is a risk that disappears. Registered as **RISK-008** (latency
contract) and **RISK-009** (font licence), with §11 now citing them.

*Two smaller corrections.* The limiter has no separate threshold parameter because **the ceiling
*is* the threshold** — the conventional maximizer reading of the brief's "Gain/Threshold, Ceiling"
— now stated in §2.5 so the omission reads as a decision, since *adding* a parameter later is a
kVersion bump. And `int_spectrumOn` defaulted to off while the brief calls the spectrum
"dismissible", which implies visible until dismissed; flipped to ⊕ on.

*Confirmations:* the Post-EQ/DSP_POLICY wording item was re-reported and is already handled — the
Hard-Stop blockquote in §1.2, the amendment obligation on the ADR-0002 row, and the checklist
entry are exactly what it asks for. `SOURCE_OF_TRUTH`'s "Levels 1, 2 and 5 are empty" paragraph
gained a clause about `DESIGN.md` occupying level 5 once ratified, so it does not silently become
wrong at sign-off. The document-registration check was re-confirmed across all four places.

**Second review pass on the design document (same day).** Three findings fixed, three
confirmations.

*The section heading said 48 parameters; the table has 49.* `colourDepth` was added by the fix
above and the heading was not updated — the one artifact in the document that a P1 implementer
could have trusted over the rows, on the surface that freezes at v0.1.0. Corrected. (`HANDOVER`
and this audit already said 49.)

*The Post-EQ position is a Hard-Stop item and was not marked as one.* `DSP_POLICY.md` invariant 1
prints the chain as `… Limiter → Ceiling → Dither` and says the EQ switch moves the block "after
the limiter" — it does **not** say where Post-EQ sits *relative to the clamp*. This design reads
it as Limiter → EQ(post) → Ceiling because the literal-appended alternative makes invariant 4
unsatisfiable; a reader taking the diagram literally would read it the other way. That is
ambiguity being resolved, but resolving it still touches DSP signal order — an
`ARCHITECTURE_REVIEW_GATE` item and an AI-agent Hard Stop. `DSP_POLICY.md` is deliberately **not**
edited here (`ADR_POLICY.md`: a policy changes only through an ADR); instead the obligation is
attached to ADR-0002 and raised to the top of the §11 sign-off checklist, so a human ratifies the
reading rather than inheriting it from a diagram.

*"P0 execution is done" was the wrong claim.* `DOCUMENTATION_LIFECYCLE_POLICY.md` maps
*complete a phase* to `HANDOVER.md` + `DOCUMENTATION_COVERAGE.md` + the `DEVELOPMENT_BRIEF.md` §13
phase summary — and the brief's own §11 makes P0's exit criterion the **sign-off**, not the
delivery. So the trigger has **not** fired and no phase summary is due yet; the earlier wording
invited the opposite reading. `HANDOVER` now states plainly that P0 is *not* complete, that all
P0 work items are delivered, and that the phase-completion trigger fires at sign-off — with the
three required updates named so the next agent does not have to re-derive them.

*Confirmations:* the parameter table's macro fixed-point rule was independently re-verified across
all nine managed rows (each `M(0,0,0)` equals its declared default) together with every curve
maximum against its declared range, and all nine managed rows confirmed `Auto: yes` — consistent
with §5.2's claim that the managed Advanced parameters are the automation surface. The wireframe's
meter values were re-checked against §2.9's formulas (PLR = −1.02 − (−9.5) = 8.5; Spotify penalty
= −14 − (−9.5) = −4.5) and match. The document-registration fix was confirmed correct in all four
places, with the note that `SOURCE_OF_TRUTH.md` has no enumerated developer-class file list, so
the new §"Where `DESIGN.md` sits" is the closest equivalent — and it does more than the trigger
asks by pinning the authority rank.

**First review pass on the design document (same day).** Three findings fixed, two
confirmations.

*The new document was registered in only one of the four required places.*
`DOCUMENTATION_LIFECYCLE_POLICY.md`'s add-a-document trigger requires four updates — the
`REPOSITORY_MAP` **tree entry**, the `SOURCE_OF_TRUTH` **class list**, `README` **§Documentation**,
and this audit — and only this audit plus a prose note at the bottom of `REPOSITORY_MAP` had been
done. So the P0 deliverable was invisible to anyone following the standard navigation path, and
its authority rank was undefined. All four are now correct, and `SOURCE_OF_TRUTH` gained a
dedicated §"Where `DESIGN.md` sits": no authority before sign-off; ranked with descriptive
Architecture after it; **superseded section by section** by the ADRs it spawns, which win on any
disagreement. Getting that rank written down matters more than the index entry — ADR-0001…0007
will cite this document, and without the rule a reader could not tell which side of a future
conflict wins.

*The default patch was not a fixed point of the macro mapping.* The colour-amount curve
(`character · (0.4 + 0.6·l)`) evaluates to **0** at the default macro position, but its declared
managed target was `clipMix`, whose default is **100 %** — so the first touch of the big knob
would have collapsed the clipper blend from fully wet to nearly dry, an unexplained jump in the
factory patch. Root cause: `clipMix` is a *parallel dry/wet blend* and was the wrong target for a
*colour amount*. Fixed by giving the colour amount its own parameter — `colourDepth` (row 49,
⊕ 0…100 %, default ⊕ 0) — and removing `clipMix`/`compMix` from the managed set entirely. The
general rule the defect exposed is now stated as binding in §5.5: **for every managed parameter,
`M(0,0,0)` must equal that parameter's declared default**, verified by inspection across all nine
managed rows and guarded by `testMacroDefaultIsFixedPoint`. A P4 curve revision that breaks it is
a defect, not a taste choice.

*Freezing display names contradicted a policy that outranks the design document.*
`PARAMETER_COMPATIBILITY_POLICY.md` rule 2 explicitly permits renaming a user-facing name at any
time while the ID stays fixed, and the same policy advises decoupling the ID vocabulary from
display wording *precisely so* copy stays revisable under C8. DESIGN.md said names "freeze in the
P1 registry snapshot". Softened to the accurate reading: sign-off ratifies the names as **launch
wording**, IDs/ranges/defaults freeze at v0.1.0, and a later rename is rule 2's normal workflow
(registry + a `Changed` CHANGELOG entry + a deliberate snapshot re-freeze).

*Confirmations:* the reviewer independently re-derived the BS.1770-4 interpolator group delay
((48−1)/2 = 23.5 upsampled ≈ 5.9 base samples ≈ 0.122 ms at 48 kHz, inside the 0.5 ms minimum
lookahead) and confirmed the measurement-tap latency claim is arithmetically sound with the P2
impulse verification correctly scheduled; and cross-checked every unmarked value in the parameter
table against the brief §4–§5 plus every macro-curve endpoint against its declared range — all
consistent. The wireframe's illustrative meter values were made self-consistent with the formulas
they sit beside (I −9.5 LUFS, TP −1.02 dBTP ⇒ PLR 8.5; Spotify penalty −14 − (−9.5) = −4.5),
since a sign convention read off a mock-up is the kind of thing an implementer copies.

Prior: for the **tenth review pass** (2026-07-30). Four findings fixed, three
confirmations. **This is the closing pass of the P0 scaffolding work** — see the note at the end.

**The hex replay recipe works; the trap is next to it.** The review suspected `--random-seed
0x4aeacb4` would parse as `0` — pluginval's "generate a random seed" sentinel — making the
documented way to reproduce a randomise-only failure silently reproduce nothing. It does not:
`CommandLine.cpp` branches on `startsWith ("0x")` and calls `getHexValue64()`, and 1.0.4 round-trips
`0x4aeacb4` exactly. But reading it surfaced a real adjacent trap: the whitelist it is validated
against, `containsOnly ("x-0123456789acbdef")`, is **case-sensitive**, so an uppercased
`0X4AEACB4` is rejected with exit `-1` — the one code both scripts misclassify as an abnormal
termination and retry three times. Documented, with the decimal form as the alternative.

**The PE parser's hardening was only partial.** The previous pass bounds-checked the CodeView
record but left `e_lfanew`, the section table and the debug-directory array indexed unchecked, so a
truncated image still threw a raw .NET `IndexOutOfRangeException` instead of the diagnosable error
the function exists to produce. All four offsets are now checked in the same style. Diagnostics
quality, not correctness — these images come from MSVC in the same job — but a half-hardened parser
reads as a hardened one.

**"The staging step self-validates" was true on two platforms of three.** Linux reads ELF section
headers and asserts the stripped `.so` still exports `GetPluginFactory`; macOS asserts both slices
are present. **Windows re-lists the extensions the purge just deleted** — it can only fire if
`Remove-Item` silently failed, so it is a delete-confirmation, not a property check, and nothing
asserts the shipped `.vst3` is still loadable. `CI_CD.md` now carries a per-platform table instead
of one sentence covering all three, and the gap is a `TODO(P1)` in the workflow. Recorded with it:
the honest closure is the PE export table, **not** a byte-string search for the symbol name, which
would prove only that the name appears somewhere in the file — the same "looks like a check, checks
nothing" shape as the `lipo -archs` finding in the fourth pass.

**CodeQL `actions` coverage is narrower than its rationale implied.** The comment argues workflow
scanning must stay on through P0 "exactly the phase in which these workflow files are being
written", but both triggers are `branches: [main]`: a workflow change is analysed on the PR into
`main` and on the post-merge push, never on a direct feature-branch push. Nothing reaches `main`
unscanned — the property that actually matters — but the scan is not continuous during iteration.
Stated in the workflow rather than left to be rediscovered.

**Confirmations (all previously recorded, re-reported unchanged):** the `GIT_SHALLOW` + commit-SHA
trap, the reusable-workflow caller-event hazard, and CodeQL's `paths-ignore` being an alert filter.

### Closing note on the review cycle

Ten passes. The defects that mattered were found by **executing** something — pluginval's seed
sentinel, `build.sh`'s exit status, the `lipo` non-assertion, the folded-scalar flag loss — and the
last three passes returned only documentation-consistency findings, because there is nothing left in
this repository to execute: no `src/`, no `tests/`, no `CMakeLists.txt`. Further review of the
scaffolding is negative-yield; the remaining risk lives entirely in code that does not exist yet.
The P0→P1 gate is `DEVELOPMENT_BRIEF.md` §11/§24 (owner sign-off on `DESIGN.md`) plus the three
`Blocking P1` entries in `OPEN_QUESTIONS.md` — decisions, not findings.

Prior: for the **ninth review pass** (2026-07-30). Four findings fixed, four
confirmations — two of them closed with live evidence from the repository's own CI rather than
reasoning.

**`CI_CD.md` still described the pre-per-platform pluginval guard.** The eighth pass re-synced
`DEVELOPMENT_BRIEF` §19.1 and left behind the procedure a developer actually opens: it named only
the randomise step and omitted Linux's `steps.strip.outcome` term — the one term that stops the
gate validating a partially-stripped binary. Replaced with a per-job table of the *actual*
conditions plus why the Linux term is load-bearing, so weakening it requires overruling a stated
reason rather than deleting an unexplained clause.

**Two stale section pointers, found by sweeping rather than by report.** `DSP_POLICY` invariant 11
cited `TESTING_POLICY.md §20.4`, which does not exist — `TESTING_POLICY` has no numbered sections;
§20.4 belongs to `DEVELOPMENT_BRIEF`. The review caught that one. Sweeping every `FILE.md §N`
reference in the repository against the target's actual headings caught a second the review missed:
`FUTURE_RISKS` RISK-005 cited `RELEASE_POLICY.md §8`, also unnumbered. Both now point at heading
names, which do not renumber.

**The debug-directory comment stated the wrong invariant.** It warned against "a rename to a PREFIX
relationship", but `dist/Anabasis-Linux` *is* already a string prefix of `dist/Anabasis-Linux-debug`
— the comment described the current layout as avoiding something it does not avoid. The real
invariant is **path ancestry**: `upload-artifact` and `find` both treat the path as a literal
directory, so a shared prefix is harmless and only *nesting* would leak symbols. Restated.

**`dependency-review.yml` closed with evidence, not analysis.** The review reasoned it could fail
on every PR, since the action needs the dependency graph and a *private* repo would additionally
need GHAS, while the product is described as closed-source. Checked the repository instead: it is
**public**, and the `dependency-review` check on PR #1 is **green**. The fork-PR half of the
finding is real but already handled — `pull-requests: write` is not grantable to fork PRs, so
`comment-summary-in-pr: on-failure` simply cannot comment there; results still appear in Checks,
which is where the gate lives.

*Separately, and not a code matter:* the repository being public contradicts the "closed-source
commercial software" description in `README.md` and `bug_report.yml`. Flagged to the owner; not
changed from here, in either direction.

**Confirmations:** the pre-P1 CI state was verified live rather than predicted — `preflight`
succeeds, all three build jobs skip, `Analyze (actions)` succeeds and no `Analyze (c-cpp)` check is
created, exactly as the dynamic matrix and the branch-protection note in `CI_CD.md` describe. The
`GIT_SHALLOW` + SHA trap, the reusable-workflow caller-event hazard, the CodeQL `paths-ignore`
scope and the macOS `set -e` / dSYM interaction were all re-reported by the review and are
unchanged from the passes that recorded them.

Prior: for the **eighth review pass** (2026-07-30). Six findings fixed, five
confirmations — two of them upgraded from `Unverified` to `Verified` by running the actual tool.

**The "deterministic" pluginval mode was never deterministic.** Both scripts passed
`--random-seed 0`, and **0 is pluginval's sentinel for "generate a random seed"** — `PluginTests.h`
states it outright ("the seed to use for the tests, 0 signifies a randomly generated seed") and
`CommandLine.cpp` only forwards the flag to the validator when it differs from that default, so
passing 0 was byte-for-byte equivalent to passing nothing. The release gate's mode A therefore drew
a fresh seed on every run while the policy, the brief and the procedure all called it reproducible;
a seed-dependent failure would have been unreproducible from the CI log. Confirmed against
pluginval 1.0.4 before and after: `--random-seed 0` printed a different `Random seed:` on each of
three runs, `--random-seed 1` printed `0x1` every time, and the fixed script now prints `0x1`
twice while randomise mode still varies. Both scripts pin a nonzero constant, documented as
load-bearing in each. *Nothing enforces that the two constants stay equal* — each script's comment
names the other, which is the entire mechanism; stated as such rather than dressed up as a check.

*The review raised this differently* — that `--random-seed` is a no-op without `--randomise`. That
premise is wrong, and checking it is what surfaced the real defect: the seed feeds the RNG the
tests themselves draw from (`Validator.cpp` hands it to `UnitTestRunner::runTests`), while
`--randomise` only shuffles test order. The flags are independent; the value 0 was the bug.

**`scripts/build.sh` reported failure after a successful build.** The last statement was
`[ -n "$STATE_TESTS" ] && echo …`; `set -e` does not abort on the left of an `&&` list, but the
last command's status *is* the script's status, so a build without the state-test binary exited 1.
`docs/procedures/DEVELOPMENT.md` documents `scripts/build.sh Debug && scripts/run-tests.sh`, which
would then never have run the tests — and it fails *silently*, since the build genuinely succeeded.
Reproduced (exit 1 with the artefacts absent, exit 0 only when the last one happened to be
present), converted to `if … fi`, and re-verified: exit 0 with none, some, and all artefacts.

**The Windows retry rationale was invented (C7).** `TESTING_POLICY` rule 3 and `TESTING.md` both
said "pluginval returns its assertion count directly", which is why 128…255 counts as a real
failure there. It does not: `CommandLine.cpp` routes every failure through `exitWithError`, which
sets the return value to **1** — the count goes to the log text only. The *conclusion* survives
(Windows has no signals, so nothing in 1…255 can be a crash) but the reason is now the evidenced
one. Also recorded: a malformed argument exits `-1` (255 on POSIX), which both scripts misclassify
as an abnormal termination and retry three times — harmless, since only a broken script can produce
it, but it is the one code neither classifier gets right.

**Action refs verified rather than asserted.** The previous audit said the pinned majors were
"re-aligned to the versions the sibling repository runs green", which this repository cannot
substantiate. All seven `uses:` refs were resolved against GitHub: all exist. One is not what its
spelling suggests — **`actions/dependency-review-action@v5` resolves through a branch**
(`refs/heads/v5`); the tags run `v4.9.0` → `v5.0.0` with no bare `v5` tag. It works and is the
vendor's advertised usage, but it is strictly less immutable than a tag, on the one workflow *not*
gated behind `preflight` and therefore running on every PR today. Recorded in `DEPENDENCY_POLICY`
with the deliberate trade behind floating majors.

**`DEVELOPMENT_BRIEF` §19.1 still described the pre-per-platform randomise guard** — it named only
the randomise step and omitted Linux's `steps.strip.outcome` term added last pass. §20.5 rule 3 had
the matching platform-neutral drift (`exit < 128`). Both re-synced to the workflow.

**Two comments added where the correctness is load-bearing but invisible:** that
`dist/Anabasis-Linux-debug` and `dist/Anabasis-Linux` must stay *siblings* (a rename to a prefix
relationship would put the symbols inside the uploaded tree and the scanned tree at once), and that
CodeQL's `paths-ignore: build` only keeps JUCE out of the results while `FETCHCONTENT_BASE_DIR`
stays at its default under `-B build` — to re-verify when `CMakeLists.txt` lands.

**Confirmations:** the PE/CodeView offsets, the macOS `set -e` / best-effort-dSYM interaction,
`run-tests.sh`'s fail-closed discovery, the reusable-workflow caller-event hazard, and the
`GIT_SHALLOW` + SHA trap (recorded last pass) all re-checked with no change needed.

Prior: for the **seventh review pass** (2026-07-30). Four findings fixed, five
confirmations.

**The Linux randomise gate could validate bytes nobody ships.** Both pluginval steps keyed on
`steps.build.outcome == 'success'`, which stays `success` when the Linux **strip** step fails — so
`build.yml`'s header claim that "on Linux the gate validates exactly the stripped bytes users
receive" was false in precisely the case that matters. Both steps now carry the same explicit
per-platform condition, Linux additionally requiring `steps.strip.outcome == 'success'`; the earlier
asymmetry (deterministic carried no `if:` and self-skipped, randomise carried one) is gone, so the
two modes still report independently but neither runs against a binary in an unshippable state.
Verified by parsing the workflow: Linux both modes identical and requiring strip, Windows/macOS both
modes identical and not.

**`TESTING.md` stated the pluginval retry boundary platform-neutrally**, the same defect corrected
in `TESTING_POLICY` rule 3 last pass — the procedure a developer actually reads still said
`exit ≥ 128` is a crash everywhere, while `run-pluginval.ps1` treats 128…255 on Windows as a *real*
failure. Replaced with the per-platform table and the reason (Windows has no signals; pluginval
returns its assertion count directly — **that second clause was wrong; see the eighth pass above,
which replaces it with the evidenced reason**). A policy and the procedure describing it diverging
is the same class of drift as a policy and its script.

**Requiring the CodeQL check by name would block every pre-P1 PR.** The dynamic matrix means the
`Analyze (c-cpp)` check *does not exist* until `CMakeLists.txt` lands — a required check that is
never created never reports, so the PR waits forever. This is a second, distinct branch-protection
trap from the `paths-ignore` one already recorded (that one is about docs-only PRs; this one is
about the phase). Written into `CI_CD.md` as its own item: require `Analyze (actions)` now,
`Analyze (c-cpp)` only from P1.

**`GIT_SHALLOW` + a commit SHA is a documented CMake trap**, recorded in `DEPENDENCY_POLICY` to be
verified when `CMakeLists.txt` lands. CMake's own documentation says `GIT_SHALLOW` expects `GIT_TAG`
to name a branch or tag; fetching an arbitrary SHA shallowly works only where the server permits
`uploadpack.allowReachableSHA1InWant`. Failure is either a silent full clone (slow, not wrong) or a
hard configure error against a stricter mirror. The resolution is stated in advance so nobody
"fixes" it the wrong way: **drop `GIT_SHALLOW`, never the SHA** — the SHA is what makes the pin
immutable.

**Confirmations:** the PE/CodeView offsets, the macOS `set -e` / best-effort-dSYM interaction,
CodeQL's `paths-ignore` as an alert filter, `run-tests.sh`'s fail-closed discovery, and the
reusable-workflow caller-event hazard (already written into `build.yml` and `CI_CD.md`) were all
re-checked with no change needed.

Prior: for the **sixth review pass** (2026-07-30). Seven findings fixed, four
confirmations.

**`HANDOVER` understated the P1 blockers — the one drift that could actually mislead someone.** It
said "One item blocks P1: owner sign-off on `DESIGN.md`", while `OPEN_QUESTIONS` tags **three**
entries `Blocking P1`: that sign-off plus **OQ-010** (lookahead 0/off position — must be settled
before the parameter exists, since widening a range later re-scales saved sessions) and **OQ-011**
(macOS deployment target, carrying a `TODO(P1)` in `build.yml`). Someone resuming from the
designated status snapshot would have started P1 believing it was clear. Corrected, with a standing
instruction that the row must agree with every `Blocking P1` entry in `OPEN_QUESTIONS`.

**`DSP_POLICY` invariants 2 and 3 could not both hold as written.** Invariant 2 asserted that with
oversampling off the reported latency is "exactly the engaged lookahead, and nothing else
contributes", while invariant 3 requires true-peak detection at **≥ 4× regardless of the user
setting**. If that ≥ 4× path sits in the signal path (linear-phase/polyphase in particular) it
contributes its own delay and invariant 2 is simply false. The two readings — measurement tap vs
in-path — are now stated explicitly as an **open point the P0 oversampling/latency ADR must
settle**, with the guarding test to be written against whichever is chosen rather than against the
convenient one. Invariant 5's "oversampling off ⇒ no oversampling latency" is cross-referenced,
since it rests on the same assumption.

**`TESTING_POLICY` rule 3 stated the retry boundary platform-neutrally** (`exit ≥ 128` is a crash)
while `run-pluginval.ps1` classifies 128…255 as a *real* failure and only ≥ 256 / negative / absent
as an abnormal termination — correct for Windows, which has no signals, but the binding policy did
not carve it out. Now stated per platform, because this repository's own rule is that a script and
the policy describing it never diverge silently.

**Two PowerShell discovery sites threw instead of diagnosing.** `Get-OneTestExe` and the Windows
staging locates run `Get-ChildItem` under `$ErrorActionPreference = 'Stop'` with no
`-ErrorAction SilentlyContinue`, so a missing `build/` produces a .NET stack trace instead of the
"build first" message written right below. Same defect fixed in `run-pluginval.ps1` last pass;
fixed here for consistency, which is the point — two discovery sites behaving differently is what
invites the next bug.

**`CI_CD.md` gave an inaccurate reason for `msvc.yml` being inert.** It said the path filters mean
"push/PR events cannot start it" — but `.github/workflows/msvc.yml` is *itself* in the filter list,
so a change to that workflow on `main` does start it; what makes it a no-op is `preflight`. The
conclusion held, the reasoning did not.

**The ambiguity diagnostics mangled paths containing spaces.** `printf '  %s\n' $matches` relied on
word splitting; quoting it would have indented only the first line. Both scripts now read line by
line, verified against a build tree with a space in a directory name.

**Recorded, not changed:** inside a reusable workflow `github.event_name` reflects the **caller's**
event, so a future `workflow_call` caller invoked on a same-repo `pull_request` would skip
`preflight` and get **zero** build jobs while still reporting success. The planned caller
(`release.yml`, tag-push-triggered) is unaffected, so this is latent — noted in `build.yml` and
`CI_CD.md` to be re-read when `release.yml` lands at P6.

**Confirmations:** the PE/CodeView offsets, the randomise-gate and checkpoint behaviour, the macOS
`set -e` / best-effort-dSYM interaction, and CodeQL's `paths-ignore` as an alert filter were all
independently re-verified with no change needed.

Prior: for the **fifth review pass** (2026-07-30). Three findings fixed, the rest
confirmations or repeats against an earlier revision.

**The per-entry CodeQL gate added last pass did not work.** It used a job-level
`if: matrix.language != 'c-cpp' || …`, but **`matrix` is not an available context in
`jobs.<id>.if`** (only `github`, `needs`, `vars`, `inputs` are). The expression either fails
workflow validation or evaluates empty — making the test always true, so the `c-cpp` entry would
have run regardless and failed on the missing project: precisely the red-CI noise the guard exists
to prevent, and the previous entry claimed it was fixed. Replaced with the mechanism that actually
holds: `preflight` **emits the matrix as JSON**, `analyze` consumes it through
`strategy.matrix: ${{ fromJSON(needs.preflight.outputs.matrix) }}` (where `needs` *is* available),
and the job carries no `if:` at all. Both matrix shapes were validated as JSON.

**`run-pluginval.ps1` was the last discovery site still taking the first match**, on the one
platform where it matters most — `windows-latest` uses the multi-config Visual Studio generator, so
several configurations of `Anabasis.vst3` genuinely coexist in one tree and the release gate could
have passed on a Debug or leftover bundle. Now exactly one match or fail, matching `run-tests.sh`,
`run-pluginval.sh` and the Windows staging step. A second defect in the same lines: with
`$ErrorActionPreference = 'Stop'`, a missing `build/` made `Get-ChildItem` **throw**, so the
intended "build first" message was unreachable and the operator got a stack trace instead of the
one-line fix — fixed with `-ErrorAction SilentlyContinue`.

**The Linux debug-leak check had a dead predicate.** `find … -type f \( … -o -name '*.dSYM' \)`
can never match: a `.dSYM` is a **directory**. Harmless on Linux (no dSYMs are produced there), but
the macOS step calls this "parity with the Linux/Windows staging self-checks" while correctly
omitting `-type f`, so the Linux check read stronger than it was. Aligned, and confirmed by test
that the predicate now matches a `.dSYM` directory.

**Repeats already handled, re-confirmed here:** the merge-commit / merge-queue nuance of the
same-repo PR skip and the `msvc.yml` double-no-op rehearsal note were both written into `CI_CD.md`
in the previous pass; the deployment target (OQ-011) and the CodeQL `paths-ignore` required-check
trap remain tracked and unchanged. Independently verified again with no change needed: the
PE/CodeView offsets, the randomise-gate and `public_ok`/`debug_artifacts` checkpoint behaviour, the
macOS `set -e` / best-effort-dSYM interaction, CodeQL's `paths-ignore` being an alert filter, and
`run-tests.sh`'s fail-closed discovery.

Prior: for the **fourth review pass** (2026-07-30). Six findings fixed, six were
confirmations or already-documented repeats.

**The macOS universality "check" checked nothing.** The packaging step *printed* `lipo -archs` for
each bundle. `lipo -archs` exits 0 for any valid Mach-O including a thin one, so a single-slice
build would have been packaged, uploaded and labelled universal — Intel users would download a
plug-in that cannot load, with CI green. It is now an assertion: both `arm64` and `x86_64` must be
present or the job fails. This is the exact failure mode the folded-scalar regression above would
have produced had it hit `-DCMAKE_OSX_ARCHITECTURES`, so the two findings are the same story twice.

**A developer-side symbol problem could withhold the Windows customer artifact.** The upload gated
on the whole staging step succeeding, and PDB retention lived inside it — so a missing CodeView
record or a non-unique PDB blocked the beta artifact even though the public copy was already
assembled, purged and clean. macOS treats that class of failure as best-effort, so the platforms
had opposite policies. The staging step now emits `public_ok=true` immediately after the public
copy passes its leak check and *before* the PDB work; the customer upload gates on that checkpoint,
the `-debug` upload on the later one. PDB retention stays strict on purpose — on macOS a missing
dSYM is expected under Release+LTO, whereas `/DEBUG` guarantees a PDB — but its strictness now
costs only the artifact it protects.

**Workflow security scanning was switched off for all of P0.** `codeql.yml`'s `preflight` gated the
whole matrix, including the `actions` entry, which needs no build and analyses the very workflow
files being written this phase. The gate is now per matrix *entry*: `actions` always runs, `c-cpp`
waits for `CMakeLists.txt`.

**Windows self-test discovery took the first match.** `Select-Object -First 1` against a
multi-config generator could run a Debug binary while the artifacts come from Release. Now exactly
one match or fail — matching the staging step in the same job and `run-tests.sh`. The same
`head -n1` pattern in `run-pluginval.sh` is fixed the same way (verified across none / one / two
matches), so the release gate cannot validate a different bundle than the one just built.

**This audit claimed a checker that does not exist (C7).** The previous entry said "a checker
asserts every configure flag survives comment-stripping on all three platforms". No such checker is
in any workflow or script — the flag list was verified *by hand* during that fix and nothing re-runs
it. Corrected to say so. Inventing a safety net is worse than having none, because it stops anyone
writing the real one.

**`CI_CD.md` documented the old randomise guard** (`!cancelled()` only) after the workflow gained
`&& steps.build.outcome == 'success'`; the summary in `DEVELOPMENT_BRIEF` §19.1 had the same drift.

**Confirmations and already-handled repeats:** the PE/CodeView offsets were independently
re-verified against the spec; the macOS `set -e` / best-effort-dSYM interaction was traced and is
sound; CodeQL's `paths-ignore` is an alert filter as its comment says; `run-tests.sh`'s fail-closed
discovery was re-confirmed. Newly recorded in `CI_CD.md` rather than changed: the same-repo PR skip
means the **push** build validates the branch head while a PR-event build would have validated the
*merge commit* — a real gap only once a merge queue or "require branches to be up to date" is
enabled; and `msvc.yml` is doubly inert until P1, so its pinned third-party action should be
rehearsed via `workflow_dispatch` at P1 rather than first executing inside the P1 build PR.
`CMAKE_OSX_DEPLOYMENT_TARGET` (OQ-011) and the CodeQL `paths-ignore` / required-check trap are
unchanged and already tracked.

Prior: for the **third review pass** (2026-07-30). Four findings fixed, three were
confirmations or verified non-issues.

**A regression introduced by the previous pass — the macOS build silently lost two flags.** The
`TODO(P1, OQ-011)` comment added last pass sat *inside* a `run: >` folded block scalar. YAML joins
every line of a folded block into one line and gives `#` no meaning, so bash saw a single command
with a trailing comment and dropped everything after it — `-DCMAKE_OSX_DEPLOYMENT_TARGET` **and**
`-DANABASIS_BUILD_NUMBER`. Consequence once P1 lands: macOS builds would carry an unintended
minimum-OS setting and report **build number 0** in the About box, which the bug-report form asks
testers to quote. No error would have revealed it. The comment now lives above the step, says why
it must stay there. **No automated checker guards this** — the flag list was verified by hand
during that fix (parsing the workflow, stripping at the first `#` the way bash would, and
confirming every configure flag survives on all three platforms) and nothing re-runs that check;
an earlier revision of this paragraph claimed a checker exists, which was an invented facility
(constraint C7). If it is worth guarding, it needs writing.

**The randomise pluginval step ran after a failed build.** Its `if: ${{ !cancelled() }}` forced it
to run when the build had failed, producing a second red step complaining about a missing plugin —
exactly the noise the staging/packaging guards were fixed for in the previous pass. Now
`!cancelled() && steps.build.outcome == 'success'` on all three jobs (the Linux build step gained
the `id` it needed); the deterministic step has no `if:` and already skipped itself correctly. The
"both modes report independently" intent is unchanged.

**`FUTURE_RISKS` RISK-003 cited `TESTING_POLICY` rule 4** for the hostile-input requirement, which
the previous pass's renumbering moved to rule **5** (rule 4 is now the skipped-test-category rule).
Swept the repository for other stale rule pointers — the two remaining (`POSTMORTEMS`, `TESTING.md`,
both rule 1) are correct.

**Verified, no change:** the issue-form / `config.yml` URLs use `skyRolly/Anabasis`, which matches
both this repository's git remote and the sibling product's own links — the casing is the canonical
display slug, not a mismatch. CodeQL's `paths-ignore` is an alert filter, which is exactly what its
inline comment claims. The `run-tests.sh` fail-closed discovery was re-confirmed behaviourally.
The branch-protection interactions (`build.yml` same-repo PR skip, `codeql.yml` `paths-ignore`)
were already documented in `CI_CD.md` §"Before enabling branch protection" in the previous pass.

Prior: for the **second review pass** (2026-07-30). Eleven findings addressed.

**The overview contradicted the decisions.** README §Project status still listed the JUCE pin and
the plugin identity as open items "not to be guessed at" after the same change set resolved and
froze both, and the `DEVELOPMENT_BRIEF` §23 delta table still said the JUCE tag "must be checked at
P0" — contradicted three sections later by §23.2. README now separates *decided and frozen* from
*still open*, and §23 gained a **Plugin identity** row so the delta table records the real
difference (`RTec`/`Anbs`/`com.rollytech.anabasis`, VST3 categories Fx/Dynamics/Mastering) rather
than a stale instruction.

**CI correctness.** The Windows staging and macOS packaging steps ran on `!cancelled()` alone, so a
failed *build* let them run and fail a second time on missing paths — a red step unrelated to the
cause; both now also require `steps.build.outcome == 'success'` (the Linux job needs no guard, its
strip step has no `if:`). `objcopy --add-gnu-debuglink` recorded a CI-workspace path that exists on
no user's machine, so a downloaded `-debug` artifact would never be found automatically; objcopy now
runs from inside the debug dir with a bare basename, the conventional lookup. The hand-rolled PE
CodeView parser now bounds-checks `PointerToRawData` before indexing, so a truncated image produces
the intended diagnosable error instead of a raw .NET exception.

**Honesty about what the gate covers.** The `build.yml` header claimed the release gate validates
the shipped bytes; that is true only on **Linux**, where the strip precedes pluginval. macOS and
Windows strip (and macOS signs) *after* validation, so a defect introduced by stripping or signing
would ship unvalidated there. Stated plainly in the header and in `CI_CD.md`, and raised as
**OQ-012** for P6 — reordering is not free (macOS must codesign last) and deserves a measurement,
not a guess.

**Branch-protection traps documented before they bite.** `CI_CD.md` gained a "before enabling
branch protection" section: `build.yml`'s same-repo PR skip reports a *skipped* conclusion (fine,
but the first thing to check if a required check ever hangs), and — the sharper one — `codeql.yml`'s
`paths-ignore` means the workflow is **never created** for docs-only PRs, so a required CodeQL check
would block them forever. Neither is a bug; both are traps if configured blindly. The standard
same-job-names no-op companion workflow is named as the fix, to be added when CodeQL is made
required and not before.

**Scripts.** `run-tests.sh` now fails closed on **ambiguity** as well as absence: `find | head -n1`
would silently gate on whichever binary `find` emitted first, so a stale second build tree could
produce a green report about the wrong artifact. Verified in all three cases (none / exactly one /
two). `run-pluginval.sh`'s `chmod +x … || true` no longer swallows a setup failure that would
otherwise resurface as an opaque "cannot execute" from the validation loop.

**Also:** `CMAKE_OSX_DEPLOYMENT_TARGET=10.13` was inherited unexamined — arm64 macOS starts at 11.0,
so the arm64 slice's minimum is silently raised, and 10.13 may sit below JUCE 9's floor. Carrying a
`TODO(P1)` and raised as **OQ-011**; not guessed at, since the deployment target is a user-visible
support claim (C7). `TESTING_POLICY`'s rule "3a" did not render as a list item (Markdown does not
recognise `3a.`), so the skipped-test-category rule — cross-referenced from the coverage audit — is
renumbered to **rule 4**, with the hostile-inputs rule to 5.

Prior: for the **first review pass** (2026-07-30). Ten findings fixed. Corrected in that
audit: the policies row said 15 docs, the tree has **16**. Scripts: `setup-linux.sh` now installs
`curl` + `unzip` (`run-pluginval.sh` calls both; `libcurl4-openssl-dev` is headers, not the CLI, and
GitHub runners preinstall them — so the gap only ever showed on a fresh machine);
`run-pluginval.ps1` **no longer passes `--skip-gui-tests`**, which was inherited from the sibling
product where an evidenced runner limitation justifies it, and here suppressed nothing while
contradicting the "uniform and blocking on every platform" gate — `TESTING_POLICY` gains rule 4
requiring any future skip to be documented, not merely scripted; `build.sh` `find` calls take
`-maxdepth` before `-name`. CI: all actions re-aligned to the versions the sibling repository runs
green (`checkout@v7`, `upload-artifact@v7`, `codeql-action@v4`, `dependency-review-action@v5`) —
the scaffold had them a major version behind; `preflight` now skips same-repo `pull_request` events
that `push: ["**"]` already built; the Linux/Windows debug uploads gate on a `debug_artifacts`
output written last rather than on "not skipped", so an aborted symbol step cannot produce a second
misleading `if-no-files-found` failure. Docs: `CI_CD.md`'s pipeline list claimed self-tests before
symbol handling — on Linux the strip runs **first**, deliberately, so the gate validates the
shipped bytes; the list is now per-platform. `DSP_POLICY` invariant 2 and the release checklist
asserted "with lookahead 0 … reported latency is 0", which §4.3's 0.5–10 ms range makes unreachable
— rephrased against the engaged lookahead, with the underlying question raised as **OQ-010**
(does lookahead get an explicit off position? — it must be settled before the parameter exists,
since widening a range later is compatibility-gated). `bug_report.yml` uses an absolute doc URL
(a relative one does not resolve from `/issues/new`).

Prior: for the **OQ-001 / OQ-003 resolutions** (2026-07-30). Two blocking decisions
moved to `Resolved` in `OPEN_QUESTIONS.md` (entries are never deleted): the JUCE pin is **9.0.0 at
commit `f8f8864…`**, the same revision Anamorph pins, so the product line shares one framework
baseline; and the plugin identity is **`RTec` / `Anbs` / `com.rollytech.anabasis`**, with the
vendor code spelling RollyTech rather than the first product. Synced: README (§Requirements),
`DEPENDENCY_POLICY` (pin row + the shared-pin rationale), `BUILD.md` (toolchain, options table,
new §Plugin identity), `HANDOVER` (dependency row, Known Blockers — now one, `DESIGN.md`
sign-off), `DEVELOPMENT_BRIEF` §23.2. Both values must be written into `CMakeLists.txt` at P1 and
are frozen from the first build that leaves this repository. Anabasis pays nothing for the
identity decision because it has never built; the sibling product absorbs the one-time break
(Anamorph 0.9.1 / its ADR-0023 / its KI-016). No `src/` change — there is still no `src/`.

Prior: repository bootstrap — the migration of Anamorph's governance system,
documentation library, build/CI scaffolding and working conventions into a previously empty
Anabasis repository, plus the product brief (`docs/DEVELOPMENT_BRIEF.md`, Part I unchanged from
the owner-supplied prompt + an additive Part II recording the inherited engineering standard).
No `src/`, no `tests/`, no `CMakeLists.txt` — so every claim about runtime behaviour in this
repository is `Unverified` **by construction**, and the policies state invariants the future code
must satisfy rather than compliance it already has (constraint C7).

## Code-module coverage

| Module | Documented in | Coverage | Confidence |
|---|---|---|---|
| `src/dsp/AnabasisEngine.{h,cpp}` (staged chain, OS region, §2.8 duck, §2.7 monitor hooks) | `THREAD_MODEL.md`, `REALTIME_SAFETY_AUDIT.md`, `DSP_POLICY.md` invariant map, ADR-0004/0011 | Full | Verified (`tests/dsp_tests.cpp`) |
| `src/dsp/LookaheadLimiter.h` | `TEST_REPORT.md` (styles/auto-release/TP numbers), `DSP_POLICY.md` inv 8/9 | Full | Verified |
| `src/dsp/MasteringEQ.h` · `MasteringComp.h` · `ClipSat.h` | `TEST_REPORT.md` (ADAA aliasing, comp two-stage bounds), DEVELOPMENT_BRIEF §2.2–2.4 cross-refs | Full | Verified |
| `src/dsp/TruePeak.h` · `LoudnessMeter.h` | `TEST_REPORT.md` (BS.1770-4 compliance vector, ISP estimator), ADR-0003 | Full | Verified |
| `src/dsp/AdaptiveEngine.h` | `MODE_AND_ADAPTATION_POLICY.md` Current implementation, `THREAD_MODEL.md` | Full | Verified |
| `src/dsp/GrHistoryBuffer.h` · `Latency.h` · `EngineParameters.h` | `THREAD_MODEL.md` (SPSC row), ADR-0004 (latency table) | Full | Verified |
| `src/PluginProcessor.{h,cpp}` · `PluginParameters.{h,cpp}` (wrapper, APVTS, state, macro layer) | `PARAMETER_REGISTRY.md`, `SERIALIZATION` notes in `THREAD_MODEL.md`, ADR-0005/0006 | Full | Verified (`tests/state_tests.cpp`) |
| `src/MacroEngine.{h,cpp}` · `PresetManager.{h,cpp}` · `InternalState.h` (macro drain, preset contract, host-hidden state) | ADR-0005 (macro layer + listener-guard, OQ-014), ADR-0007 §preset exclusion, `THREAD_MODEL.md` (listener-guard / staged-record rows), ADR-0010 | Full | Verified (`tests/state_tests.cpp` — drain/teardown/preset/read-rule tests) |
| `src/gui/` (editor, LookAndFeel, LoudnessMeter/GrHistory/Spectrum/Curve views) | `BRAND_CONSISTENCY_CHECKLIST.md`, `THREAD_MODEL.md` (SPSC + relaxed-meter reader rows), ADR-0009 provenance headers in each adapted file | Full | Partially Verified — behaviour is Verified (the state suite constructs the real editor since round 32), **appearance is Level 5** and deliberately not claimed headlessly (`TESTING_POLICY.md`) |

Rows were added as modules landed; with the P5/P6 rows above, the table covers the v0.1.0 tree.
(Until 2026-08-05 this section still called `src/gui/` a "planned" module two phases after it
landed — the staleness the same date's audit entry reports.)

## Documentation-set self-coverage (deliverables present)

| Tier | Files | Status |
|---|---|---|
| docs root | DEVELOPMENT_BRIEF, **DESIGN**, SOURCE_OF_TRUTH, REPOSITORY_MAP, OPEN_QUESTIONS, HANDOVER, DOCUMENTATION_COVERAGE, KNOWN_ISSUES, FUTURE_RISKS, POSTMORTEMS, BRAND_CONSISTENCY_CHECKLIST | Present (`DESIGN.md` is **`Accepted`**, signed off 2026-07-31; it ranks at level 5 and is superseded section by section by its ADRs — SOURCE_OF_TRUTH §"Where `DESIGN.md` sits") |
| worklogs | `2026-07-30-p0-anamorph-research.md` | Present (raw evidence trail; never cited as policy) |
| policies | 16 docs (incl. the Anabasis-specific `MODE_AND_ADAPTATION_POLICY`) | Present |
| procedures | BUILD, DEVELOPMENT, CI_CD, TESTING, RELEASE_PROCESS, RELEASE_COMPATIBILITY_CHECKLIST, TROUBLESHOOTING | Present (PACKAGING moved with the installer set to the first commercial release — **OQ-007**, resolved 2026-08-02; no longer a P6 item) |
| architecture | `design-decisions/ADR_INDEX.md` — **the registry**: take the ADR set and each entry's confidence from it, never from a row here (this row enumerated "ADR-0001…0012" and went stale the moment ADR-0013/0014 were Accepted — the same staleness the round-53 README fix removed, in the file whose job is noticing it). Descriptive set: `THREAD_MODEL.md`, `PARAMETER_REGISTRY.md` (P1), `REALTIME_SAFETY_AUDIT.md` (P2), `PERFORMANCE_BUDGET.md` (P6), and `COMPATIBILITY_MATRIX.md`, `SERIALIZATION_REGISTRY.md`, `LATENCY_MODEL.md` (all 2026-08-05) | Decisions registered in the index; remaining descriptive docs — see the gaps list below |
| docs root — testing/status (since P2) | `TEST_REPORT.md` (measured aliasing / TP / latency-matrix / dither / LUFS data, updated per phase) | Present |
| user | `USER_MANUAL.md`, `INSTALLATION.md` | **Present (2026-08-05)** — written against the v0.1.0 surface with every stated fact taken from the tree (registry ranges, factory names, preset paths, Learn's 5 s minimum, the constant-latency contract, the chmod/quarantine realities of the OQ-007 zips). Derived class — never evidence (`SOURCE_OF_TRUTH.md`); prose voice adapted from the sibling's manual under ADR-0009 and ⊕ for the fine review like all product wording taken under a standing approval |
| root — developer/status | README, CHANGELOG, CLAUDE | Present |
| root — legal | `NOTICE`, `THIRD_PARTY_LICENSES.md` | **Factual attribution half Present (2026-08-05)** — produced against the actually-pinned JUCE tree per `RELEASE_POLICY.md`'s own prescription: inventory from JUCE's `LICENSE.md` plus a compiled-TU walk, compiled-in status from `nm` probes on this build's per-TU objects (the LTO'd image hides them), exclusions from their gates plus symbol absence. **Delivery changed in 0.1.1** (**ADR-0021**, `RELEASE_POLICY.md` §Third-party attribution): both ship as version-named release-page assets — `Anabasis-<version>-NOTICE.txt` and `Anabasis-<version>-THIRD_PARTY_LICENSES.md` — and are no longer copied into the zips, the `.pkg` or the Inno payload, because the release page is the one carrier every distribution route passes through and a loose unversioned copy cannot be told apart from another build's once extracted. The **owner-legal half** (`EULA.md`, `PRIVACY.md`, `TRADEMARKS.md`) stays absent — waits on OQ-002 and owner wording, never invented (C8) |
| root — internal/testing | `SUPPORT.md` (**landed 0.1.1**), `.github/ISSUE_TEMPLATE/` | The class's rule is "restates the legal class, never diverges from it" — and Anabasis has no approved licence, EULA or privacy document to restate (OQ-002 / OQ-009 open). `SUPPORT.md` states that as its own §1 and confines itself to what the repository can evidence: the reporting channel, what a usable report contains, and that terms come from the owner rather than from it. Deliberately SHORTER than the sibling's, which restates documents that exist there |
| .github | workflows/{build,codeql,msvc,dependency-review,cxx23-canary,release}.yml, dependabot.yml, ISSUE_TEMPLATE/{bug_report,config}.yml | Present — **`release.yml` landed in 0.1.1** (**ADR-0021**: tag-triggered validate → build → draft-release, publishing left a human action). The OQ-007 deferral this row quoted is **superseded 2026-08-07** for the pipeline-and-installers half; only signing/notarization stays deferred |
| scripts | setup-linux, build, run-tests, run-pluginval.{sh,ps1} | Present |

## Known coverage gaps / TODOs

These are **deliberate**, not oversights. Each names what would close it.

- **Architecture set, partially closed** — `THREAD_MODEL.md` and `PARAMETER_REGISTRY.md` landed
  with P1, `REALTIME_SAFETY_AUDIT.md` with P2, `PERFORMANCE_BUDGET.md` with P6 (struck from the
  absent list 2026-08-05 — it had sat here for three days after landing),
  `SERIALIZATION_REGISTRY.md` on 2026-08-05 — another absent document a binding policy already
  cited as an authority (`COMPATIBILITY_POLICY.md` §"Where each contract is specified"), the
  same class as the attribution files and the matrix; `LATENCY_MODEL.md` (the other
  policy-cited one) landed the same day. Still absent:
  `ARCHITECTURE.md`, `SIGNAL_FLOW.md`, `DSP_GRAPH_REFERENCE.md`,
  `DSP_ALGORITHMS.md`, `API_REFERENCE.md`, `STATE_SERIALIZATION.md` (the
  last two were listed in `REPOSITORY_MAP.md`'s planned set but omitted here — the two lists
  described the same set with different membership until the 2026-08-05 verification round
  aligned them). The "closed by P5–P6" target has **passed unmet**
  for these; no replacement date is invented (C7). `COMPATIBILITY_MATRIX.md` — the one that was
  load-bearing, being what `OQ-011` and `HANDOVER.md`'s Pending Tasks row point the DAW-matrix
  audition at — **landed 2026-08-05**, so the audition has its target document; every host row
  in it is deliberately `Unverified` until the audition supplies evidence.
- **`REALTIME_SAFETY_AUDIT.md`'s audited revision is the P2 commit (2026-08-01)** — stated in
  the audit itself. The P3–P6 audio-thread additions entered through `THREADING_POLICY.md`'s
  permitted-path table and the round-41/42 TSAN passes, but the document has not been
  re-baselined against the v0.1.0 tree. Closed by a re-audit at (or after) the fine review.
- ~~**No ADRs**~~ — **closed 2026-07-31**: ADR-0001…0011 are Accepted and registered. They were
  authored `Unverified` (no `src/` existed then), which is a *different* gap from absence: each is
  a contract the code must satisfy. **Closed for most of them as of the P1–P4 code**: `ADR_INDEX.md`
  now carries a per-ADR confidence with the named test that discharges it, and the three still at
  `Partially Verified` say which half is unwired (ADR-0005's gesture grammar → P5, ADR-0007's
  FROZEN_TRIMS inject → OQ-013, ADR-0011's OQ-014/KI-003 questions).
  New decisions still follow C1 — evidence-driven, no quota.
- ~~**Policy compliance sections are `TODO (no code yet)`**~~ — **closed 2026-08-05**, and the
  closure was itself overdue in one place: `TESTING_POLICY`'s section flipped 2026-08-02,
  `DSP_POLICY`'s invariant→test map and `MODE_AND_ADAPTATION_POLICY`'s "Current implementation"
  filled as their phases landed — but `REALTIME_AUDIO_POLICY` §Current compliance still read
  "TODO (no code yet)" three phases after `REALTIME_SAFETY_AUDIT.md` (the deliverable the TODO
  itself scheduled) existed. It now points at the audit and states the re-baseline gap above.
- **Performance numbers** — the C2 rule stands (no number without machine + method).
  ~~CPU budget open~~ — **closed 2026-08-02**: `PERFORMANCE_BUDGET.md` carries the measured
  whole-engine matrix and per-stage table with the machine and method recorded, plus the DESIGN
  §9 allocation review. (This entry still said "CPU … remains open" three days after that
  document landed.) No separate memory-budget figure exists, and none is invented.
- **No host (DAW) matrix** — requires manual testing. P6 closed without it; it is now the
  post-v0.1.0 fine review's DAW-matrix audition (`HANDOVER.md` Pending Tasks), which needs
  `COMPATIBILITY_MATRIX.md` first (above).
- ~~**Legal / attribution class absent**~~ — **factual half closed 2026-08-05**: `NOTICE` +
  `THIRD_PARTY_LICENSES.md` landed (self-coverage row carries the method); since 0.1.1 they
  ship as version-named release-page assets rather than inside the packages (**ADR-0021** — the
  legal row above carries the reasoning). Still genuinely absent: the owner-legal half (`EULA.md`,
  `PRIVACY.md`, `TRADEMARKS.md`) — waits on OQ-002 and owner wording (C8). (This bullet said
  "producible now" for one commit after the files existed — the attribution change synced the
  row and missed the bullet; caught by the 2026-08-05 verification round.)
- ~~**`docs/user/` absent**~~ — **closed 2026-08-05**: `USER_MANUAL.md` + `INSTALLATION.md`
  landed against the v0.1.0 surface (see the self-coverage row for the evidence discipline);
  the class's ⊕ wording review joins the fine review's brand pass.

## Update protocol

On any change, set this file's "Last updated" to the new HEAD (or the change description before
the first tag) and adjust the affected rows. A new module → add a row; a new doc → add to
self-coverage; new measured data → upgrade the confidence. Never upgrade a confidence level
without the evidence that justifies it.
