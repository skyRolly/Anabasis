# DOCUMENTATION_COVERAGE.md

Permanent documentation-coverage audit. **Future contributors/AI must update this on every
documentation-affecting change** (`docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md`).

Coverage = how well the module/topic is documented. Confidence = strength of the evidence behind
that documentation (Verified / Partially Verified / Unverified / Not Supported).

**Last updated:** for **review round 27 (2026-08-03)**:
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
earlier writes and the bulk clear could be observed above it. **KI-007 records the three items
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
unprimed/primed load cases pin it (seven mutants total, each killed by a distinct check — both
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

Rows were added as modules landed. The remaining planned modules (`src/gui/`, P5) are listed in
`docs/REPOSITORY_MAP.md` §`src/`; that is a **plan**, not coverage.

## Documentation-set self-coverage (deliverables present)

| Tier | Files | Status |
|---|---|---|
| docs root | DEVELOPMENT_BRIEF, **DESIGN**, SOURCE_OF_TRUTH, REPOSITORY_MAP, OPEN_QUESTIONS, HANDOVER, DOCUMENTATION_COVERAGE, KNOWN_ISSUES, FUTURE_RISKS, POSTMORTEMS, BRAND_CONSISTENCY_CHECKLIST | Present (`DESIGN.md` is **`Accepted`**, signed off 2026-07-31; it ranks at level 5 and is superseded section by section by its ADRs — SOURCE_OF_TRUTH §"Where `DESIGN.md` sits") |
| worklogs | `2026-07-30-p0-anamorph-research.md` | Present (raw evidence trail; never cited as policy) |
| policies | 16 docs (incl. the Anabasis-specific `MODE_AND_ADAPTATION_POLICY`) | Present |
| procedures | BUILD, DEVELOPMENT, CI_CD, TESTING, RELEASE_PROCESS, RELEASE_COMPATIBILITY_CHECKLIST, TROUBLESHOOTING | Present (PACKAGING deferred to P6) |
| architecture | `design-decisions/ADR_INDEX.md` + **ADR-0001…0012** (0001…0011 Accepted 2026-07-31, **ADR-0012** 2026-08-01); descriptive set so far: `THREAD_MODEL.md`, `PARAMETER_REGISTRY.md` (P1), `REALTIME_SAFETY_AUDIT.md` (P2) | Decisions complete for P0; remaining descriptive docs (ARCHITECTURE, SIGNAL_FLOW, LATENCY_MODEL, …) land by P6 |
| docs root — testing/status (since P2) | `TEST_REPORT.md` (measured aliasing / TP / latency-matrix / dither / LUFS data, updated per phase) | Present |
| user | — | Deferred to P6 |
| root — developer/status | README, CHANGELOG, CLAUDE | Present |
| root — legal | — | Deferred to P6 (produced against a real dependency tree; copying another project's inventory would be invented evidence) |
| root — internal/testing | — | Deferred to P6 (SUPPORT.md ships with the first tester build) |
| .github | workflows/{build,codeql,msvc,dependency-review}.yml, dependabot.yml, ISSUE_TEMPLATE/{bug_report,config}.yml | Present (release.yml deferred to P6) |
| scripts | setup-linux, build, run-tests, run-pluginval.{sh,ps1} | Present |

## Known coverage gaps / TODOs

These are **deliberate**, not oversights. Each names what would close it.

- **Architecture set, partially closed** — `THREAD_MODEL.md` and `PARAMETER_REGISTRY.md` landed
  with P1, `REALTIME_SAFETY_AUDIT.md` with P2. Still absent: `ARCHITECTURE.md`,
  `SIGNAL_FLOW.md`, `DSP_GRAPH_REFERENCE.md`, `SERIALIZATION_REGISTRY.md`, `LATENCY_MODEL.md`,
  `COMPATIBILITY_MATRIX.md`, `DSP_ALGORITHMS.md`, `PERFORMANCE_BUDGET.md` — closed by P5–P6 as
  the code they would describe stabilises.
- ~~**No ADRs**~~ — **closed 2026-07-31**: ADR-0001…0011 are Accepted and registered. They were
  authored `Unverified` (no `src/` existed then), which is a *different* gap from absence: each is
  a contract the code must satisfy. **Closed for most of them as of the P1–P4 code**: `ADR_INDEX.md`
  now carries a per-ADR confidence with the named test that discharges it, and the three still at
  `Partially Verified` say which half is unwired (ADR-0005's gesture grammar → P5, ADR-0007's
  FROZEN_TRIMS inject → OQ-013, ADR-0011's OQ-014/KI-003 questions).
  New decisions still follow C1 — evidence-driven, no quota.
- **Policy compliance sections are `TODO (no code yet)`** in `REALTIME_AUDIO_POLICY`,
  `THREADING_POLICY`, `DSP_POLICY` and `MODE_AND_ADAPTATION_POLICY`. Closed as each phase lands,
  with evidence citations.
- **Performance numbers** — none may be written until measured with a recorded machine and
  methodology (C2). `docs/TEST_REPORT.md` exists since P2 (2026-08-01) and carries the measured
  aliasing / true-peak / latency-matrix / dither data; CPU and memory budgets remain open until a
  machine spec is recorded (P2/P6).
- **No host (DAW) matrix** — requires manual testing. Closed by the P6 DAW smoke tests.
- **Legal / attribution class absent** — closed at P6 against the actually-pinned JUCE tree.
- **`docs/user/` absent** — closed at P6.

## Update protocol

On any change, set this file's "Last updated" to the new HEAD (or the change description before
the first tag) and adjust the affected rows. A new module → add a row; a new doc → add to
self-coverage; new measured data → upgrade the confidence. Never upgrade a confidence level
without the evidence that justifies it.
