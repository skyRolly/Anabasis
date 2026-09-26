# Anabasis product / UX audit — findings: State, presets and persistence and Operation model

Part of [`2026-09-26-anabasis-product-ux-audit.md`](../2026-09-26-anabasis-product-ux-audit.md) (audited revision `e769f33`, 2026-09-26). This file holds the complete record of each finding in these categories; the report carries the index, the systemic themes, the roadmap and the decision record. Code anchors are pinned to `e769f33`; runtime observation ids refer to [`worklogs/2026-09-26-product-ux-audit.md`](../../../worklogs/2026-09-26-product-ux-audit.md).

Each record: decision, priority and confidence after calibration; evidence; current behaviour; problem; root cause; user impact and scope; proposed improvement; alternatives considered; decision rationale (with any calibration or challenge outcome); architecture gates; dependencies; acceptance criteria; and the verification record.

## STATE — State, presets and persistence

### STATE-001

**The '*' edited marker stops working after every session load: a reopened project claims the clean preset and never marks later edits**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Preset/state workflow | Save, load, browse and restore change or lose state without saying so | Phase 4 |

**Evidence**

- e769f33:src/PluginProcessor.h:165-170 — presetDirty() returns false whenever presetBaseline is invalid
- e769f33:src/PluginProcessor.h:435-444 — presetBaseline / storedPresetBaseline are 'Deliberately NOT serialized and NOT part of the ADR-0007 StateSet'
- e769f33:src/PluginProcessor.cpp:1753-1761 — resetSlotFieldsToDefaults() sets both baselines to an invalid ValueTree; it is called from setStateInformation at e769f33:src/PluginProcessor.cpp:1884
- e769f33:src/PluginProcessor.cpp:1774-1776 — the AB slots are written with no dirty datum
- e769f33:src/gui/PluginEditor.cpp:2160-2170 — shownDirty = presetDirty(), and the ' *' suffix is appended only when it is true
- e769f33:src/PluginProcessor.cpp:1654, e769f33:src/PluginProcessor.cpp:1724, e769f33:src/PluginProcessor.h:171-182 — only a preset apply or a preset save re-seeds the baseline
- e769f33:tests/state_tests.cpp:1644-1673 — asserts 'a restored session reads CLEAN' and that an edit after the load reads clean until a re-apply ('where before the re-apply the same edit read clean')
- e769f33:tests/state_tests.cpp:2059-2067 — 'a session load drops the datum instead of marking the loaded name'
- e769f33:docs/policies/SESSION_COMPATIBILITY_POLICY.md:34-36 — rule 4: a save→load round trip must reproduce '…the dirty marker, both A/B slots…'
- e769f33:docs/architecture/design-decisions/ADR-0007-state-schema-v1.md:73-74 — 'Remaining root properties follow the copied machinery (live preset name + dirty-star baseline, Anamorph:…)'
- Anamorph@fd78c3b:src/PluginProcessor.cpp:583 — sibling writes presetBaseline 'so the dirty-star survives reload (#6)'; per-slot bases at :598 and :602
- e769f33:docs/architecture/design-decisions/ADR-0026-slot-payload-read-rules.md:77-79 — asserts rule 4 (including the dirty marker) is untouched for every blob the plugin produces
- e769f33:docs/architecture/SERIALIZATION_REGISTRY.md:271-272 — records presetBaseline as 'rebuilt, never persisted' (sides with the code)
- e769f33:docs/user/USER_MANUAL.md:403 and :414-418 — '*' marks an edited preset; each slot keeps its own edited state; both slots travel with the session
- Runtime E09 — screenshots session capture `rt/edges/53d-after-valid-state-load.png` and 53f-after-load-ui-drag-crop.png (viewed): 'Default' with no '*' at Loudness 100 %, Ceiling -20 dB, Input +24 dB, and still after a UI drag
- Runtime ST-16 — session capture `rt/state/32d-topbar.png` (viewed): saved as 'AuditTest *', reopened as 'AuditTest'
- Runtime E18 — session capture `rt/edges/63a-undo-x25.png` (viewed): 'Default' shown at +24 dB input and -20 dB ceiling
- Own reproduction (verify-16, stepped motion) — session capture `rt/verify-16/12-loudpop-edited.png` (the edited Transparent Master before the save; Character 0.80), 14-after-load.png ('Transparent Master', no marker, Character 0.80), 15-crop.png (after the UI drag to 0.92: no marker, Undo enabled)

**Current behaviour.** Each setStateInformation clears both slots' dirty baselines. presetDirty() treats the missing baseline as 'not edited', so the top bar shows the restored preset name with no '*', whatever the saved state was. The marker stays dead for that slot through any number of automation or UI edits, A/B switches and undo steps. It comes back only after the user applies a preset or saves one.

**Problem.** The top bar makes a false claim on every reopen: 'Loud Pop *' reopens as 'Loud Pop'. For the rest of the session it cannot report edits. This contradicts SESSION_COMPATIBILITY_POLICY rule 4, the ADR-0007 decision text (and its Anamorph precedent), ADR-0026's own compliance claim, and the user manual. Only the serialization registry and the code comments describe the actual behaviour.

**Root cause.** The dirty datum (the PRESET_SHAPE baseline per slot) is kept out of the session blob on purpose. The stated reason is that 'a session records which preset a slot holds, never whether it had been edited since'. The load path then invalidates the datum, and presetDirty() maps 'unknown' to 'clean' (e769f33:src/PluginProcessor.h:167-168). The editor has no third state for 'unknown'.

**User impact.** On every project reopen the preset label reports an untouched preset for a state that may be heavily edited. Later tweaks never light the marker, so the user cannot use it to see whether they have drifted from the preset. A user may trust the wrong label, for example by reporting 'stock Loud Pop' to a client, or by comparing A/B believing one slot is unmodified. Nothing is destroyed: preset applies are undoable. *Scope:* Every session restore, whether a DAW project reopen or a host state recall, on both A/B slots, until the next preset apply or save in each slot. The display itself is one label (e769f33:src/gui/PluginEditor.cpp:2144-2171). The datum lives in the processor, and its per-slot plumbing through undo, A/B and Copy already exists.

**Proposed improvement.** Target behaviour: a reopened session shows exactly the name and '*' state it was saved with, per slot, and later edits light '*' as they do in a fresh session. Minimal mechanism: add one additive per-SLOT attribute such as presetEdited (bool), written wherever the slot tree is built (saveSlotFromLive, and storedSlot when a slot is stored). On load, if the flag is false, seed that slot's baseline from presetShapeFromLive() once the slot is live; the raw-exact round trip makes this equal to the shape at save time. If the flag is true, show '*' until the next apply, save or undo lands on a known baseline. The rare case of editing back to the exact preset values would keep '*'; that is conservative and honest. If the flag is absent (older blob), use an explicit 'unknown' rendering, not 'clean'. Then update SESSION_COMPATIBILITY rule 4, ADR-0026:77-79, SERIALIZATION_REGISTRY §1.7 and the two tests that pin today's behaviour.

**Alternatives considered.**

- *A. Persist the full PRESET_SHAPE baseline per slot (Anamorph parity)* — Exact, and it can also detect an edit back to clean. But it adds about 50 PARAM nodes per slot to the schema, versus one attribute. It is the same hard-stop gate for more frozen surface.
- *B. No schema change: recompute the baseline at load from the restored ADR-0022 identity (factory table or a resolvable user file)* — This has to simulate an apply on a scratch surface, including relandMacroCurve and the locked-ceiling skip. That is non-trivial on a load path that may be off the message thread (KI-003). It also silently changes meaning when the preset file changed on disk, and it cannot resolve a missing file. Higher risk and complexity for the same result.
- *C. No schema change: render an explicit 'unknown' state after load (for example a distinct neutral mark) until the next apply or save, and correct the docs* — Honest and gate-free, but the marker stays dead for the session. This is the right fallback if the owner declines the schema change.
- *D. Seed baseline = loaded surface ('clean at load')* — Later edits would light '*', but a session saved as edited would still reopen claiming clean, so the false statement stays. Reject.
- *E. Leave the code; amend policy rule 4, ADR-0007/0026 text and the manual* — Removes the documentation contradiction but keeps a label that misstates what is loaded on every reopen. Reject as the sole fix.

**Decision: Modify · P2.** The behaviour is intentional in code and tests, but it breaks the product's own higher-ranked contract (Accepted ADR-0007 text and policy rule 4) and the manual, on every reopen. The fix needs a human decision on an additive schema field, which is cheap in the pre-ship window (the ADR-0015 precedent). If the owner declines, option C plus the doc corrections is the required minimum: the label must stop asserting 'clean' when the answer is unknown.

*Calibration:* the verifier judged Proceed / P1; the final judgement is Modify / P2. Challenge accepted: a stale '*' changes no sound and per-slot undo recovers a browse-away cheaply, so P2 (raise to P1 only with evidence that hosts call setStateInformation mid-session or that users rely on '*' before browsing). Modify: gate-free now, seed presetBaseline from the loaded surface at the end of setStateInformation (option D) plus an explicit 'unknown' rendering at load (option C); update the two tests that pin today's behaviour. A persisted per-slot flag goes to the owner as a new ADR (serialization-schema hard stop; ADR-0022 as precedent; ADR-0026 metadata rules; written only at serialization, never in saveSlotFromLive, sentinel-baseline encoding); the ADR also resolves ADR-0007's contested 'baseline'. Same '*' visibility family as [UI-013](findings-ui.md#ui-013) (P2).

*Adversarial challenge:* evidence holds: yes; priority justified: no (suggested P2); decision justified: no (suggested Modify). The evidence holds. Priority, rubric check: the judge's own scores are user_impact 3, severity 3 and workflow_efficiency 2. No sound changes, nothing is destroyed, and applies are undoable. The P1 is carried by frequency 5 and coherence 5, and the coherence part is document drift, which the SOURCE_OF_TRUTH order resolves toward the code unless the product harm justifies a change. The strongest harm argument the judge did not make: after a reopen, the user browses presets away from an unmarked edited slot and loses the edits when coming back. Per-slot undo recovers this cheaply, so it is not 'costly recovery'. P2 fits: a meaningful clarity improvement on a frequent but low-cost path. Evidence that users rely on '*' before browsing, or that hosts call setStateInformation mid-session (for example, a host undo that restores FX state), would support raising it back to P1. Decision: Modify. The underlying ask is sound, but the specific mechanism is under-specified in ways that hit the undo/A-B unit and the retraction logic, and the governance route is misnamed. Constrained path: (1) Gate-free now: at the end of setStateInformation, seed presetBaseline from the loaded surface (option D) so the marker works for the rest of the session. Pair it with an explicit 'unknown' rendering at load (option C) so the label never asserts clean. The judge treated C and D as exclusive; they compose, and together they fix both halves without touching the schema. They still need updates to the two tests that pin today's behaviour. (2) Put a per-slot persisted flag to the owner through a new ADR. It is written only at serialization, uses a sentinel-baseline encoding, follows the ADR-0026 metadata rules, and uses ADR-0022 as precedent. The ADR also resolves the ADR-0007 'baseline' ambiguity and whether rule 4 keeps the dirty marker. *Proposal risks:* (a) Wrong write site. The proposal writes presetEdited in saveSlotFromLive, which makes the flag part of the ADR-0007 StateSet, the A/B and undo unit. The code explicitly declined that (PluginProcessor.h:337-340: 'would be an ADR-0007 schema change and a Hard Stop'). It would also feed strippedForUndoCompare, which drives the Copy dedupe (PluginProcessor.cpp:415-420), the preset-bracket retraction (:555-557) and gesture diffs. A clean-to-dirty flip would then count as a slot change, and the flag would carry a second copy of dirtiness next to UndoEntry.baseline. The flag should be written only in getStateInformation's AB serialization. The active slot's value comes from presetDirty(). The inactive slot's value should be a bit latched at switchToSlot/copySlotToOther, since a stored slot cannot be edited. The alternative is a new presetShapeFromSlot() projection, which would break the 'one traversal for writer and dirty marker' invariant (SERIALIZATION_REGISTRY.md:326-330). (b) There is no state for 'edited, no baseline'. The claim that the plumbing 'already exists' holds only if that state is encoded as a sentinel baseline tree that never compares equal. Otherwise UndoEntry, the switch swap, Copy and the retraction all need a tri-state. The retraction arm `! b.preBaseline.isValid()` equates invalid with CLEAN (PluginProcessor.cpp:549-553) and must be re-argued under any encoding. (c) ADR-0026 governs which slot's METADATA is adopted when a SLOT or root payload is missing. The new flag is metadata and must follow that asymmetry; the proposal does not say so. (d) Gates the judge did not name. This needs an ADR, not only 'owner clearance'. ADR-0022 (the per-slot identity trio) is the right precedent for additive per-slot metadata, not ADR-0015, which was a removal. The same ADR must settle what ADR-0007's 'baseline' means. Any fallback that amends SESSION_COMPATIBILITY rule 4 is itself a Policy change, and ADR_POLICY rule 5 says a Policy change is enacted by an ADR. Correcting Accepted ADR-0026 text follows the append-only rule (ADR_POLICY rule 4). (e) KI-003: seeding the baseline inside setStateInformation writes the same ValueTree member that today's invalidation writes, off-thread on some hosts. That is not a new exposure class, but marshalling it to the message thread would be a Thread Model change, which is a hard stop. (f) The '*' is ellipsised on long names (verify-16/12-crop), so the acceptance criteria are only visually testable with short names until that dependency is fixed.

**Architecture gates.**

- Serialization schema change (additive per-SLOT field) — ARCHITECTURE_REVIEW_GATE hard stop; needs owner clearance (pre-ship window per COMPATIBILITY_POLICY 'When the contract starts' / ADR-0015 precedent)
- Existing conflict to resolve either way: code and tests versus Accepted ADR-0007 Decision (ADR-0007:73-74, dirty-star baseline follows the Anamorph machinery) and SESSION_COMPATIBILITY_POLICY rule 4
- ADR-0026 Migration paragraph (ADR-0026:77-79) asserts rule 4 holds; it must be corrected under any resolution

**Dependencies.** NEW: '*' suffix is ellipsised away on long preset names (see new_findings) — both concern whether the edited marker is visible at all; [STATE-005](findings-state-model.md#state-005) (a shared load-diagnostics/notice surface could also carry an 'edited state unknown' hint for legacy blobs)

**Acceptance criteria.**

- Apply 'Loud Pop', move Character, save the session, load it into a fresh instance: the top bar reads 'Loud Pop *'.
- Apply 'Loud Pop' untouched, save, load into a fresh instance: it reads 'Loud Pop'. Then move any preset-carried parameter (UI drag or automation): it reads 'Loud Pop *' within one display refresh (≤ ~333 ms).
- With different presets and edit states in slots A and B, after the load each slot shows its own saved marker on A/B switching.
- A blob written without the new field loads with byte-identical sound and never renders the marker as positively 'clean' unless that is known (the rendering is documented).
- The byte-identical getState→setState→getState test still passes, and a new test pins the marker round trip for both slots. The tests at state_tests.cpp:1644-1673 and 2059-2067 are updated to the new contract.
- SESSION_COMPATIBILITY rule 4, ADR-0007, ADR-0026, SERIALIZATION_REGISTRY §1.7 and USER_MANUAL §7.3/§7.4 all describe the same behaviour.

<details><summary>Verification record</summary>

**Method.** Read every cited anchor at e769f33. presetDirty() is at PluginProcessor.h:165-170. The per-slot baselines and their 'Deliberately NOT serialized' comment are at PluginProcessor.h:435-444. resetSlotFieldsToDefaults (PluginProcessor.cpp:1753-1761) is called from setStateInformation at :1884. getStateInformation writes the AB slots at :1774-1776. The editor label is at PluginEditor.cpp:2144-2171. The only baseline writers are the two preset applies (:1654, :1724), savePresetFile (PluginProcessor.h:171-182) and undo/redo (:632, :647). Also read the tests that pin the behaviour (state_tests.cpp:1644-1673, 2031-2067), SESSION_COMPATIBILITY_POLICY rule 4, ADR-0007 Decision, ADR-0026 Migration, SERIALIZATION_REGISTRY §1.7, and Anamorph's writer for comparison. Viewed rt/edges/53d, 53f, 63a and rt/state/32d-topbar. Reproduced on :146 with stepped pointer motion: took Transparent Master, edited Character to 0.80 (preset value 0.05), saved state, loaded it, then dragged Character to 0.92 in the UI. The label read 'Transparent Master' with no marker throughout, and Undo was enabled.

**Corrections to the candidate claim.** (1) The marker is re-armed by the next preset apply OR preset save, not only by an apply. 'Never marks later edits' holds until one of those two actions. (2) The conflict is wider than the policy. ADR-0007 Decision (ADR-0007:73-74) says the root properties follow the copied Anamorph machinery 'live preset name + dirty-star baseline'. Anamorph persists that baseline 'so the dirty-star survives reload' (Anamorph@fd78c3b:src/PluginProcessor.cpp:583, 598, 602). ADR-0026:77-79 states that rule 4, dirty marker included, holds for every blob the plugin produces, which is false. So the code conflicts with Accepted-ADR text as well as with the policy. (3) This is deliberate, not a regression: two tests assert that a restored session reads clean, and that an edit made after the load reads clean until a re-apply. (4) It affects every setStateInformation, including host preset recall, not only a project reopen. (5) The impact 'overwrite a preset believing nothing changed' is weak, because Save writes the live state. The real harm is a false statement about what is loaded, plus an edit indicator that does not work for the session. No work is destroyed, since applies are undoable.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 5 · severity 3 · discoverability 4 · efficiency 2 · coherence 5 · change risk 3 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### STATE-002

**Every factory-preset browse resets TP, Dither and Noise Shaping to Off: a locked Ceiling keeps its number but becomes a sample-peak limit, and 16-bit dither turns itself off**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P1** | high | confirmed | Preset/state workflow | Save, load, browse and restore change or lose state without saying so | Phase 0 |

**Evidence**

- e769f33:src/PresetManager.cpp:310-357 — factory apply writes param.getDefaultValue() to every non-excluded parameter except a locked ceiling
- e769f33:src/PresetManager.cpp:152-243 — no factory table names truePeakMode, dither or ditherShaping; 'Default' has no overrides
- e769f33:src/PluginParameters.cpp:377, :399-400 — truePeakMode default false, dither default Off, ditherShaping default false
- e769f33:src/PluginParameters.cpp:433-437 — preset exclusion = view tier + freeze + advancedMode only
- e769f33:src/PluginParameters.cpp:303-308 — Ceiling text suffix is ' dBTP' only while TP is engaged
- e769f33:docs/architecture/design-decisions/ADR-0010-parameter-surface.md:87-90 — option I (a wider v1 lockable set including output/dither rows) rejected; :191-195 lockable set = {ceiling}
- e769f33:docs/architecture/design-decisions/ADR-0015-pre-ship-contract-refreeze.md:120-123 — truePeakMode default on->off; :193-199 — the readout unit is the stated mitigation. The ADR does not discuss the lock or preset interaction
- e769f33:docs/user/USER_MANUAL.md:109-111, :171, :399-403, :426-428, :526-528 — TP 'is what makes the number mean dBTP'; LOCK 'keeps the ceiling fixed while you browse presets'; FAQ tells users to lock then browse
- e769f33:tests/state_tests.cpp:2019-2025, :2087-2135 — lock tests assert the ceiling value only; nothing pins truePeakMode under lock
- VER0-1: session capture `rt/verify-0/03-crop.png` (TP on, LOCK on, '-1.00 dBTP'; dump: dither 16-bit, ditherShaping On) -> session capture `rt/verify-0/05-crop.png` (Loud Pop: '-1.00 dB', TP pill off, Statistics TP -0.72 dBTP in red; dump: truePeakMode Off, dither Off, ditherShaping Off; name 'Loud Pop' clean)
- VER0-4: session capture `rt/verify-0/08-crop.png` -> session capture `rt/verify-0/09-crop.png` — Advanced '>' to Transparent Master: Dither 16-bit -> Off, SHAPE on -> off, TP on -> off, Ceiling '-1.00 dBTP' -> '-1.00 dB'
- VER0-3: rt/verify-0/app.log — after Undo: ceiling -1.00 dBTP, truePeakMode On, dither 16-bit, ditherShaping On

**Current behaviour.** Loading any factory preset writes every non-excluded parameter to its default before applying the table's intents. truePeakMode, dither and ditherShaping are not excluded and no table names them, so every factory browse sets TP Off, Dither Off and Noise Shaping Off. With LOCK on, the ceiling value is skipped and survives, but TP is not locked. The same number is now a sample-peak limit, displayed as 'dB', and true peaks can exceed it. The same happens when loading a user preset saved with TP off.

**Problem.** LOCK's documented purpose is to keep the delivery ceiling while browsing presets. The FAQ tells users to lock and then browse, and first-push step 5 tells them TP is what makes the number mean dBTP. One preset click then keeps the number but drops its meaning, so output exceeded the held -1.00 by about 0.28 dB true-peak in the repro. It also switches off a chosen 16-bit dither, which is not shown at all in the Simple view.

**Root cause.** The factory apply is 'defaults + intents' over the whole non-excluded surface (PresetManager.cpp:133-138, :310-357). The exclusion set protects only view state, freeze and advancedMode (PluginParameters.cpp:433-437), and the lockable set is {ceiling} alone (ADR-0010:191-195; a wider set including output/dither rows was rejected as option I, :87-90). ADR-0015 flipped the truePeakMode default from on to off (:120-123) and made the unit follow the mode (item 5), so the defaults pass now writes TP Off on every browse. The ceiling's meaning is split across two parameters, and only one of them is lockable. Neither ADR text addresses that interaction.

**User impact.** This hits a user preparing a streaming or broadcast deliverable who sets -1.00 dBTP, engages TP (and possibly 16-bit dither), locks the ceiling, and then auditions factory presets. The user can bounce a master that violates the dBTP spec, or an undithered 16-bit file. The only cues are a unit suffix changing, a small pill and a red Statistics row. Recovery (Undo) exists but requires noticing. *Scope:* All 13 factory presets on every load path (menu, the ‹ › buttons that step to the previous/next preset, re-apply of Default), both views, with or without LOCK (the LOCK case is the trap). User presets whose files store TP Off or Dither Off behave the same.

**Proposed improvement.** Target behaviour: a locked ceiling is held as a delivery limit, value and mode together. With LOCK on, any preset apply also skips truePeakMode, so '-1.00 dBTP' stays '-1.00 dBTP'. The owner also decides, via a superseding ADR, whether a factory browse leaves the output rows (dither, ditherShaping) untouched, since no factory table expresses an intent for them. Interim, ungated, can land first: (a) USER_MANUAL §3.2, §7.3 and the FAQ (:526-528) state that presets set TP, Dither and Noise Shaping, that factory presets reset them to Off, and that LOCK holds the number only; (b) when a preset apply changes truePeakMode or dither, the change is flagged in the active view, e.g. the TP pill and ceiling unit highlight briefly, plus a Simple-visible cue that dither was turned off.

**Alternatives considered.**

- *Add truePeakMode/dither/ditherShaping to isPresetExcludedParam* — The strongest fix, but it also stops user presets recalling TP. That is a move between exclusion tiers (ADR-0010 Hard Stop) and removes a legitimate preset intent.
- *Factory-only skip of delivery rows (TP, dither, shaping) in applyFactoryPreset* — A viable narrower variant for dither and shaping. It changes the 'defaults + intents' contract and the preset exclusion behaviour (rule 6 gate). On its own it does not protect TP under a user-preset load.
- *Documentation plus a visible change cue only* — Ungated and worth doing now as the interim. It does not prevent the spec violation, so it cannot be the final answer.
- *Leave as-is* — Rejected. It contradicts the manual's LOCK and TP statements and produced output above the held number in the repro.

**Decision: Proceed · P1.** An ordinary action (browsing presets under a lock meant to protect the delivery ceiling) changes what the ceiling limits and turns off dither. The core fix touches the ADR-0010 lockable set and possibly the exclusion tiers, so it needs a superseding ADR and human review. The doc and cue interim is ungated and should not wait for it. Priority is P1 rather than P0 because the TP change has visible (if low-salience) cues and Undo fully restores. Severity is still maximal because a bounced master can fail its spec.

*Adversarial challenge:* evidence holds: yes; priority justified: yes (suggested P1); decision justified: yes (suggested Proceed). Evidence holds: the code path, the defaults, the missing table entries and the runtime transitions in both views are all confirmed. The inter-sample overshoot mechanism has an independent controlled A/B in visuals.md:84. P1 holds under the rubric. It is not P0, because the TP change is visible where it happens (the TP pill flips and the unit changes dBTP -> dB, right beside LOCK) and Undo restores all three parameters. It is not P2: the manual's own FAQ tells users to lock and then browse, a dBTP delivery spec is the common streaming case, and an unnoticed failure costs a re-delivery. The one overstated cue is the red Statistics TP row, which is red in both modes and holds across presets, so the P1-vs-P0 argument should rest on the pill and the unit suffix alone. Proceed is right, and the proposal (lock holds value plus TP mode; a separate ADR decision for dither and shaping) is already the constrained option. It needs three amendments: add the Serialization Registry gate, route any factory-only skip through the shared predicate rather than a special case, and limit the interim to documentation while the project is pre-release. *Proposal risks:* (1) A gate is missing from gate_flags: Serialization Registry change. ARCHITECTURE_REVIEW_GATE.md:16 covers 'any field add/remove/semantic change'. int_ceilingLock is a serialized ANABASIS_INTERNAL field (state.md:104 XML), and making it also hold truePeakMode widens what it means. SERIALIZATION_REGISTRY.md:337-339 pins the apply rule as 'a locked ceiling is skipped'. The dither/shaping factory-only route would also rewrite :341-356 ('a factory apply is defaults-then-overrides ... anything a table omits IS at its default'). PARAMETER_REGISTRY.md:165-170 ('Widening the set is a registry entry + ADR') needs the same update. (2) The dither route must not become a special case inside applyFactoryPreset. The design keeps ONE exclusion predicate pair shared by A/B, undo, presets and the dirty marker so that they cannot drift (PluginParameters.h exclusion-tier comment; PresetManager.cpp:306-309). A factory-only skip creates a third category of exclusion. It must be expressed through the shared walk or predicate and pinned against the dirty-marker projection (testTheDirtyMarkerMeasuresOnlyWhatAPresetCanCarry). (3) Locking dither itself would re-open ADR-0010 option I, which is explicitly rejected (:87-90). The judge correctly keeps dither out of the lock, and the ADR must say why TP is inside the lock and dither is not. (4) The lock is hard-coded as `id == pid::ceiling` at two sites (PresetManager.cpp:67 and :314), although ADR-0010:191-192 says the mechanism is generic. Adding truePeakMode at both sites by hand repeats the drift risk. A shared isLockedByPresetLock(id) predicate is the smaller-risk implementation. (5) HANDOVER.md:30: no tag has been cut, so per ADR-0015 item 6 the contract can still be reshaped at zero cost. That argues for doing the ADR now rather than building interim UI cue (b), which the fix would make partly redundant. Keep interim (a), the manual text. Build cue (b) only for whatever the ADR leaves resettable (e.g. dither). Any cue must respect int_uiAnimations. A new dither indicator in the DESIGN-signed Simple surface is a brand-checklist item, not a hard stop. (6) Dirty marker and ADR-0022 identity are unaffected, because presetBaseline is taken from live after apply (PluginProcessor.cpp:1654, :1724), the same as for the locked ceiling. No test pins the lockable set as exactly {ceiling}, so nothing existing breaks.

**Architecture gates.**

- Parameter Registry change: exclusion list / lockable set (PARAMETER_COMPATIBILITY_POLICY.md rule 6; ARCHITECTURE_REVIEW_GATE 'changing … exclusion')
- Accepted ADR-0010 conflict: lockable set = {ceiling} (:191-195) and option I (wider v1 lockable set incl. output/dither rows) rejected (:87-90) — needs a superseding ADR
- ADR-0010 exclusion tiers, if the exclusion-set or factory-only route is chosen
- Ceiling-guarantee semantics (ADR-0006/ADR-0015 item 5): the change strengthens rather than weakens, but it alters what a held ceiling means and should be reviewed under that gate

**Dependencies.** [UX-001](findings-ux.md#ux-001) (the lock indicator should describe value+mode)

**Acceptance criteria.**

- With TP On, Dither 16-bit, Noise Shaping On, Ceiling -1.00 dBTP and LOCK on, stepping through all 13 factory presets and loading a user preset saved with TP Off leaves the Ceiling reading '-1.00 dBTP' with truePeakMode On in both views
- Dither/Noise Shaping after a factory browse match the ADR decision, and USER_MANUAL §7.3 states that outcome
- A state test pins truePeakMode under a locked factory apply and a locked file apply (companion to testALockedCeilingSurvivesAPresetThatNamesIt)
- With LOCK off, factory browse behaviour matches the ADR (unchanged from today unless the ADR says otherwise)
- Interim: the manual (§3.2, §7.3, FAQ) states LOCK holds the number only and presets set TP/Dither/Shaping; a preset apply that changes truePeakMode or dither produces a visible cue in the active view
- Regression: Undo of a preset apply still restores truePeakMode, dither and ditherShaping

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/PresetManager.cpp:152-243 (no table names truePeakMode, dither or ditherShaping; Default is an empty table) and :306-357 (one pass writing getDefaultValue() to every non-excluded, non-locked parameter). Read e769f33:src/PluginParameters.cpp:303-308 (unit follows TP), :377, :399-400 (defaults Off) and :413-437 (exclusion set). Read ADR-0010 :87-90 and :191-195, and ADR-0015 :120-123 and :193-199. Checked e769f33:tests/state_tests.cpp:2005-2025 and :2087-2135: only the ceiling value is pinned under lock. Reproduced on :130 with stepped motion (VER0-1, VER0-3, VER0-4).

**Corrections to the candidate claim.** (1) 'Silently loses its dBTP unit' overstates. The suffix changes from dBTP to dB and the Simple TP pill turns off, both visible though low-salience; the Statistics TP row also turned red. The Dither and Noise Shaping reset IS invisible in Simple, where those controls do not exist. (2) The harm is more than a label. TP detection is actually off, so inter-sample peaks exceed the held number: after one Loud Pop click the Statistics TP row read -0.72 dBTP against a Ceiling reading -1.00 dB. (3) The factory browse resets TP, Dither and Shaping with or without LOCK. LOCK makes it a trap because it signals that the ceiling is protected. (4) Loading a user preset saved with TP off has the same effect, because files carry truePeakMode. (5) Undo of the apply restores all three (verified), so recovery exists if the user notices.

</details>

<sub>Verifier scores (1-5): impact 5 · frequency 3 · severity 5 · discoverability 4 · efficiency 3 · coherence 5 · change risk 3 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### STATE-003

**Settings, LOCK, Learn, BYPASS and MATCH/DELTA are not recorded by undo, and the Undo button does not say what it will revert, so pressing Undo after one of them silently reverts the last recorded step instead**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Feedback/observability | The tier and undo model is principled in code but invisible at the point of action | Phase 4 |

**Evidence**

- e769f33:src/InternalState.h:16-17 — InternalState fields 'NEVER participate in A/B, undo, or presets'. The fields include int_oversample, int_osPhase, int_offlineQuality, int_ceilingLock, int_uiScale and the meter standards (ids at :27-38).
- e769f33:src/PluginParameters.cpp:413-417 — isViewTierParam = bypass, loudnessComp (MATCH), deltaMonitor (DELTA).
- e769f33:src/PluginProcessor.cpp:259-276 — the undo snapshot is armed only when undoEligible (not view-tier) and only on the message thread. :339-356 pushes only if strippedForUndoCompare differs.
- e769f33:src/gui/PluginEditor.cpp:621-640 — the LEARN onClick only calls proc.startLearn()/stopLearn(). e769f33:src/PluginProcessor.h:654,662 — these only set engine command atomics, so no undo push.
- e769f33:docs/policies/MODE_AND_ADAPTATION_POLICY.md:161-166 — the Learn commit stays outside undo deliberately (global ADAPTIVE child against the per-slot unit). The policy calls this 'a product decision for the P6 pass'.
- e769f33:src/PluginProcessor.h:346-350 — UndoEntry is {slot, baseline} and carries no description of the step. e769f33:src/PluginProcessor.cpp:608-633 — undo() pops the active slot's top entry, whatever control the user touched last.
- e769f33:src/gui/PluginEditor.cpp:359-362 — the Undo tooltip is the literal 'Undo' and onClick = proc.undo() + refreshPresetDisplay. Tooltips are off by default (e769f33:docs/user/USER_MANUAL.md:150-151).
- e769f33:docs/user/USER_MANUAL.md:161 — the undo row says bypass and the monitor toggles are 'never recorded'. It does not mention Settings, LOCK or LEARN. :278 calls Settings 'session state'.
- e769f33:docs/architecture/design-decisions/ADR-0004-latency-contract-constant-lookahead-allowance.md:256-260 — OS factor, phase and offline quality never travel with undo, so an undo 'never touches PDC'.
- Runtime verify-17 BYPASS. Loudness had been dragged 0->20 %. BYPASS On, then Undo: Loudness went 20->0 % and BYPASS stayed On. Redo re-landed 20 %. — session capture `rt/verify-17/02-undo-after-bypass-crop.png`
- Runtime verify-17 Oversampling. Off->2x moved latency 480->484. After dismissing the popup, Undo left latency at 484 and moved Loudness 20->0 %. — session capture `rt/verify-17/04-os-menu-crop.png`, .../rt/verify-17/06-undo-after-os-crop.png
- Runtime verify-17 MATCH. MATCH On, then Undo: Loudness went 20->0 % and loudnessComp stayed On. — session capture `rt/verify-17/07-undo-after-match-crop.png`
- Runtime verify-17 LEARN. A LEARN pass with the countdown shown (08-learning-crop.png), then Undo: Loudness went 20->0 %. The learned reference survived (state-after-learn-undo.xml: ADAPTIVE refOnsetRate=2.81 refTiltDb=-4.68). — session capture `rt/verify-17/10-undo-after-learn-crop.png`
- Runtime verify-17 FREEZE control. FREEZE On, then Undo: Freeze Off. The recorded FREEZE and the unrecorded LEARN/MATCH/DELTA share one row and look the same.
- Phase-2 ST-09 (f)(g): after an Oversampling change and after a UI Scale change, Undo reached back to an earlier preset step ('AuditTest'->'Lo-Fi Crush'). — session capture `rt/state/21e-topbar.png`, .../rt/state/27c-topbar.png

**Current behaviour.** Undo and Redo always apply the active slot's most recent recorded slot snapshot. Several controls never create a step: Settings (Oversampling, Phase, Offline Render, Integrated, RMS Reference, UI Scale, Animations, Tooltips), Ceiling LOCK, LEARN, BYPASS, MATCH and DELTA. Pressing Undo right after one of them leaves that control as it is and reverts the previous recorded edit, which may be a knob, a preset load or the ADV view. The Undo button shows only a glyph and, when tooltips are on, the word 'Undo'. No preview of the step comes before the click and no acknowledgement comes after it.

**Problem.** Undo is blind. From the UI a user cannot tell which controls are recorded or what the next press will revert. Right after touching an unrecorded control, the natural 'take that back' click changes something the user was not looking at, such as a knob in the other view or an earlier preset choice. It gives no signal that the intended control did not revert.

**Root cause.** The undo unit is the per-slot StateSet (e769f33:src/PluginProcessor.h:190-210, UndoEntry :346-350). Controls outside that tree are excluded on purpose: InternalState settings (ADR-0004 needs undo never to touch PDC), view-tier toggles (ADR-0018 D4) and the global Learn reference (MODE policy :161-166). Entries carry no description, and the editor exposes only enabled/disabled state (PluginEditor.cpp:2136-2142), so the exclusions are invisible when the user clicks.

**User impact.** A user changes Oversampling, LOCK, MATCH or runs LEARN, then presses Undo expecting that change to go back. Instead the last knob or preset edit is reverted, sometimes in the other view or minutes old. Redo recovers it, but only if the user notices before the next edit clears the redo line. Learn cannot be taken back at all, and nothing says so. *Scope:* Every A/B slot and both views. It affects any Undo/Redo press that follows an unrecorded control: all Settings rows, LOCK, LEARN, BYPASS, MATCH and DELTA. It also affects the host-folded writes covered by [STATE-010](findings-state-model.md#state-010) and the arrow-key edits covered by [STATE-007](findings-state-model.md#state-007), which likewise leave no step.

**Proposed improvement.** Keep the recorded set as designed and make the next step legible. (1) On hover, Undo and Redo show a one-line description of the step they would apply, independent of the global Tooltips switch. The description is computed on the message thread by diffing the top entry's slot tree against saveSlotFromLive(): the changed parameter's display name (plus 'and N more'), 'preset <name>' when presetName or identity differs, 'view: Simple/Advanced' for advancedMode, and 'Copy' for a Copy entry. Nothing new is stored and there is no schema change, because the stacks are never serialized. (2) After a click, a short acknowledgement (~1.5 s) beside the buttons or in the preset slot reads 'Undone: Loudness' or 'Redone: preset Loud Pop'. (3) The USER_MANUAL §3.1 undo row lists what is not recorded (Settings incl. Oversampling/Phase/Offline Render, Ceiling LOCK, LEARN, BYPASS, MATCH, DELTA) and that FREEZE is recorded. Whether Learn becomes undoable stays with the owner (MODE policy :161-166) and is not built here.

**Alternatives considered.**

- *Record Oversampling/Phase/Offline Render (or all Settings) in the undo history* — Reject. ADR-0004 Consequences and DESIGN.md:364-376 depend on undo never touching PDC, and InternalState.h:16-17 forbids it. An undo would then change the reported latency (Architecture Review Gate: conflict with an Accepted ADR).
- *Make the LEARN commit an undo step* — Defer to the owner. MODE policy :161-166 records this as an unmade product decision: the reference is global while undo is per slot. Widening the slot tree to carry it would be a serialization-schema change (gate).
- *Disable Undo, or warn, after an unrecorded action* — Reject. The recorded history is still valid, and disabling Undo hides a legitimate step and adds modal friction.
- *Documentation only (manual row)* — Necessary but not sufficient. The trap happens at the click, tooltips are off by default, and the manual is read rarely.
- *Leave as is* — Not justified. Silent reverts of an unrelated step were reproduced across four control families.

**Decision: Modify · P2.** The exclusions themselves are deliberate and anchored in Accepted records (ADR-0004, ADR-0018, MODE policy), so the obvious fix of widening undo is off the table. The harm comes from undo being invisible, not from the exclusion set. A diff-derived preview and acknowledgement plus a manual correction removes the surprise without touching any gate.

**Dependencies.** [STATE-007](findings-state-model.md#state-007) (arrow-key edits also leave no step; the preview would show that); [STATE-010](findings-state-model.md#state-010) (the automation/host fold would show up in the same preview); Owner decision on Learn undoability (MODE_AND_ADAPTATION_POLICY.md:161-166)

**Acceptance criteria.**

- With Tooltips OFF, hovering Undo shows a label naming the step it will revert (e.g. 'Undo: Loudness', 'Undo: preset Loud Pop', 'Undo: view Advanced'), and Redo does the same.
- After Settings>Oversampling 2x (or BYPASS, MATCH, DELTA, LOCK, LEARN), hovering Undo names the previous recorded step, not the control just changed.
- Clicking Undo or Redo shows a transient acknowledgement naming what changed, visible for about 1 s or more.
- Undo semantics are unchanged: Oversampling/Phase/Offline Render still never enter the stacks (latency unchanged by any undo), and the existing undo and ADR-0018 state tests stay green.
- USER_MANUAL §3.1 undo row lists Settings, Ceiling LOCK, LEARN, BYPASS, MATCH and DELTA as not recorded and FREEZE as recorded.

<details><summary>Verification record</summary>

**Method.** Read the e769f33 anchors: e769f33:src/InternalState.h:16-17 and :27-38, e769f33:src/PluginParameters.cpp:413-417, e769f33:src/PluginProcessor.cpp:221-356 and 588-648, e769f33:src/PluginProcessor.h:346-350 and 652-662, e769f33:src/gui/PluginEditor.cpp:359-362 and 621-640, e769f33:docs/user/USER_MANUAL.md:141-161, 276-294 and 308-315, e769f33:docs/policies/MODE_AND_ADAPTATION_POLICY.md:158-166, ADR-0004:256-260 and ADR-0018. Viewed the ST-09 screenshots session capture `rt/state/20-undo-topbars.png`, 21e-topbar.png and 27c-topbar.png. Reproduced on :147 with 5-step pointer motion before every click (rt/verify-17). The sequence was a Character drag and a Loudness drag (0->20 %), then each of BYPASS, Settings>Oversampling 2x, MATCH and a ~7 s LEARN pass, each followed by Undo (with Redo between trials). FREEZE followed by Undo was run as a control.

**Corrections to the candidate claim.** The anchor InternalState.h:15-16 is really :16-17. The BYPASS/MATCH/DELTA exclusion is documented (USER_MANUAL.md:161) and deliberate (ADR-0018 Decision 4). Settings and LOCK are documented only as 'session state' (:278, :293-294). LEARN's exclusion is a deliberate but still-open product decision (MODE policy :161-166), which DESIGN.md:797 and PluginProcessor.h:652-653 still contradict. The step that gets reverted is the active slot's most recent recorded step, so it is not an arbitrary one. Redo recovers it, because the unrecorded actions leave the redo line intact (observed). FREEZE sits on the same row as LEARN/MATCH/DELTA and is recorded (observed). Ceiling LOCK is a further unrecorded control that the title does not name.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 2 · severity 3 · discoverability 4 · efficiency 2 · coherence 3 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### STATE-004

**Any prepareToPlay, including one at the same rate and block size, silently drops the frozen trims from the audio. FREEZE stays lit and the save keeps the vector (KI-006 audio half)**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P1** | medium | confirmed | State management | The adaptive engine changes the audio from state the user cannot see, keep or reset | Phase 0 |

**Evidence**

- e769f33:src/PluginProcessor.cpp:776 — prepareToPlay calls engine.prepare unconditionally; :754-770 states that no ValueTree state is touched there, by design
- e769f33:src/dsp/AnabasisEngine.cpp:134 adaptiveEngine.prepare(...) and :144 reset() → :281 adaptiveEngine.reset()
- e769f33:src/dsp/AdaptiveEngine.h:106-115 (prepare → reset), :120 `trims = {};`, :166 `publishTrims (false);` (published zeros); :418 `if (! freeze && audible)` — with Freeze ON nothing re-slews, so zero is held
- e769f33:src/dsp/AnabasisEngine.h:283-286 — a staged-but-unapplied restore survives prepare. An already-applied latch does not.
- e769f33:tests/state_tests.cpp:3325-3330 — the suite asserts that 'the APPLIED vector did not survive re-initialisation' while the RETAINED one did (current behaviour is pinned)
- e769f33:docs/KNOWN_ISSUES.md:336-344, 402-408 (audio half open; fix deferred as a Freeze-semantics owner call); e769f33:docs/user/USER_MANUAL.md:308-311 (promises exact restore)
- e769f33:src/PluginProcessor.cpp:779-781 ('Hosts re-prepare on transport start'); e769f33:src/dsp/AnabasisEngine.cpp:428-434 ('the re-prepare path most hosts take' on entering offline)
- probe rt/verify-18/probe/probe.out: latched rel +0.550 oct / link −0.110 / hpf +29.0 Hz / tilt +0.003 dB. After a re-prepare (all three variants) published = 0 and hasPub = 0, while retained = latched. The re-prepared render is bit-exact with a never-adapted engine. Against the latch kept, the difference is −26.5 dB relative, RMS L −9.63 vs −10.02 dB, and R-channel min GR −9.79 vs −9.29 dB.
- probe bounce.out (same dir): offline render after a host re-prepare → trims 0; without the re-prepare → trims held; RMS differs by about 0.4 dB
- runtime: rt/verify-18/a-before-sr.xml and b-after-sr.xml — identical FROZEN_TRIMS (rel 0.1088, link −0.0218, hpf 0.996, tilt 0.126) before and after `sr 96000 512`. The app.log dump shows freeze = On. Screenshot session capture `rt/verify-18/03-freeze-row-before-after.png` (FREEZE lit in both).
- runtime ST-17 composite session capture `rt/state/33-freeze-sr-composite.png` (FREEZE stays On through 96k/512, 48k/64, 192k/2048)
- *Added from another verifier's note:* A second trigger of the same audio/save disagreement. Code-inferred; I re-checked it but did not run it. The shape arises after an A/B switch into a Freeze-ON slot that has no FROZEN_TRIMS; testAFrozenLatchDoesNotFollowTheSlotSwitch builds exactly that shape (e769f33:tests/state_tests.cpp:3380-3391). applySlotToLive stages no restore, because staging is gated on liveFrozenTrims being valid (e769f33:src/PluginProcessor.cpp:1516-1522). The engine therefore keeps applying the OUTGOING slot's latch, held by Freeze. Meanwhile engineFrozenTrimsIfLive returns {} while gen == slotFrozenBase (:1202-1216), so the slot saves no vector (KNOWN_ISSUES.md:376-386, round 42). A reload renders with zero trims, not what was heard. The test pins only the save half. [STATE-004](findings-state-model.md#state-004)'s fix (keep trims across reset) does not cover this path. Add an acceptance item: after such a switch, the applied vector and the saved record agree. The option consistent with round 42's no-borrowing rule is to stage a never-latched (zero) vector on that ownership change. This is the same Freeze-semantics owner call (MODE policy Enforcement).

**Current behaviour.** With FREEZE on, any host prepareToPlay (rate change, buffer change, or a same-configuration re-prepare) zeroes the internal trim vector and its published copy. The audio from then on is processed with zero adaptive trims, held there by Freeze until the next load, A/B or undo re-injects the saved vector. FREEZE stays lit, nothing on screen changes, and the session save still writes the latched vector (the engine's retained set). Reopening the project therefore restores a sound different from the one the user heard after the re-prepare.

**Problem.** Freeze exists for repeatability ('Freeze is for repeatability', manual §4; the podcast workflow's 'one consistent sound for the whole episode'). A routine host event silently breaks that promise. The audio, the indicator and the saved state then disagree three ways, and nothing lets the user detect it.

**Root cause.** AdaptiveEngine::prepare() always calls reset(), which zeroes `trims` and republishes zeros (AdaptiveEngine.h:120,166). finishBlock's `! freeze` guard then holds those zeros. The retained set that the save reads deliberately feeds no audio path (KI-006, AdaptiveEngine.h:130-145 comment). The fix was deferred because keeping trims across a discontinuity changes MODE invariant 3's Freeze semantics (an owner decision), not because it is technically hard.

**User impact.** A user who froze the adaptive state and then changes buffer size, switches interface or sample rate, or works in a host that re-prepares on transport start or before a bounce gets a different master than the one auditioned and frozen. In the probe the difference was about 0.3–0.4 dB RMS and about 0.5 dB of per-channel GR (bounded by the trim limits: release ±1 oct, link ±0.2, SC HPF +30 Hz, tilt +0.5 dB). The user has no cue, because FREEZE stays lit and the trims are invisible ([VIS-011](findings-visualisation.md#vis-011)). The only recovery (reload, or an A/B round trip) is not discoverable. *Scope:* Every host and format, whenever Freeze is ON with a non-zero latch and prepareToPlay runs: a rate or block change, a same-configuration re-prepare, and (per the code's own premise) transport start or offline entry in some hosts. It affects the realtime audio and possibly bounces, not the saved state. The Freeze-OFF path also snaps trims to zero on re-prepare, but it re-adapts within seconds by design.

**Proposed improvement.** Target experience: 'FREEZE on' means the adaptive contribution in the audio stays exactly what was frozen across any host re-prepare, including bounces, until the user turns Freeze off or loads, A/Bs or undoes to a slot with a different vector. Mechanism (owner decision behind the gate): AdaptiveEngine::reset() stops discarding the CURRENT applied vector. It keeps `trims` across reset and republishes them as meaningful when they were meaningful, still cancelling any in-flight Learn and resetting the features. This preserves pre-prepare audio continuity, adds no cross-thread path (reset runs while audio is stopped, inside the engine that owns the values), and also lets an un-frozen re-prepare re-slew from where it was instead of snapping to zero. Update the liveLatch test premise, close KI-006's audio half, and record the rule in an ADR-0014 amendment. Until then, correct USER_MANUAL §4 so it does not promise what a re-prepare breaks.

**Alternatives considered.**

- *Re-inject the engine's RETAINED set at the end of reset() (KI-006's 'one-line re-injection')* — Equivalent in the common case and still thread-safe. However, the retained set is engine-wide and cache-scoped to a slot (round 42), so it answers 'last latch' rather than 'what was playing'. Carrying the applied `trims` is the more exact statement of 'a re-prepare changes nothing'.
- *Wrapper re-stages liveFrozenTrims / the retained vector from prepareToPlay* — Rejected. This is the round-40 design: a juce::ValueTree read or write on a host callback that is not the message thread, which TSAN caught as a race. It would be a Thread Model change under the Architecture Review Gate.
- *Carry only while Freeze is ON (engine remembers the last p.freeze)* — A narrower semantics change. It keeps today's snap-to-zero for unfrozen re-prepares. Acceptable if the owner prefers the smallest Freeze change, but it adds state that must agree with the parameter snapshot.
- *Keep the behaviour but make it visible: a 'Freeze reset by host' badge with one-click re-apply (message-thread restoreFrozenTrims from the retained set)* — Honest but manual. It does not protect bounces, and it needs [VIS-011](findings-visualisation.md#vis-011)'s readout to be meaningful. Useful only as an interim.
- *Leave as is and only correct the manual* — Rejected. It defeats Freeze's stated purpose (repeatability) in routine host events.

**Decision: Modify · P1.** Code, a bit-exact engine probe and the harness save check all confirm the defect. It silently breaks the single promise of an advertised workflow control, and the audio, indicator and save disagree. The fix is small and engine-local. It is a Freeze-semantics change, so it must go through the MODE-policy Architecture Review Gate and owner sign-off (KI-006 already defers it there), decided together with KI-007 item 1 ([STATE-011](findings-state-model.md#state-011)).

*Calibration:* the verifier judged Proceed / P1; the final judgement is Modify / P1. Challenge agreed P1 (undetectable; the frozen audition and a re-prepared bounce can differ) but changed Proceed->Modify, and P1 is borderline: drop to P2 if [TEST-002](findings-doc-test.md#test-002) shows major hosts do not re-prepare on transport start or bounce. Constraint: restore the published atomics and pubTrimEver without touching the retained set or its generation, else round-42 slot isolation regresses; add the test 'A/B into a vectorless freeze-ON slot -> re-prepare -> save -> no FROZEN_TRIMS'. Merge note adds a second, code-inferred trigger: an A/B switch into a Freeze-ON slot with no FROZEN_TRIMS keeps applying the outgoing latch while the slot saves none (e769f33:src/PluginProcessor.cpp:1202-1216, :1516-1522); add 'applied vector and saved record agree' and prefer staging a zero vector on that ownership change. Freeze-OFF carry is a separate owner call; MODE Enforcement gate plus ADR-0014 amendment. The interim manual/tooltip fix targets 'locks ... exactly', not the reopen sentence.

*Adversarial challenge:* evidence holds: yes; priority justified: yes (suggested P1); decision justified: no (suggested Modify). The evidence is solid and code-, test- and probe-confirmed. The behaviour silently breaks the one promise of an advertised control (manual §4, the podcast workflow), and the audio, the lit indicator and the save disagree. P1 stands under 'frequent trap with costly recovery': the user cannot detect it, and in hosts that re-prepare before a bounce the deliverable differs from the frozen audition. It is borderline rather than clear-cut, though. Freeze is optional, the real-programme magnitude can be far below the near-rail probe figure, and the transport-start and bounce exposure rests on an unverified code comment that the MODE policy contradicts. If per-host checks show that major hosts do not re-prepare on bounce or transport start, P2 would fit better. The decision should be Modify rather than plain Proceed. The obvious implementation (`publishTrims(true)` in reset) regresses round-42 slot isolation, so the change must be constrained to republishing the applied set without advancing the retained generation. The Freeze-OFF carry should be a separate, explicitly optional owner choice. The interim USER_MANUAL/tooltip correction should also land now, independent of the gated fix, and it should target 'locks … exactly', not the reopen-restore sentence. *Proposal risks:* (1) Judge missed this, and it re-opens a fixed defect: 'republishes them as meaningful' implemented through the existing `publishTrims (true)` (AdaptiveEngine.h:718-741) also rewrites the RETAINED set and increments `retTrimSeq`. engineFrozenTrimsIfLive (PluginProcessor.cpp:1208) adopts the engine's vector as soon as `gen != slotFrozenBase`. Take an A/B switch into a freeze-ON slot that has no vector of its own, followed by any re-prepare: the bump makes the incoming slot claim and serialise the OUTGOING slot's latch. That is exactly the round-42 defect. Acceptance criterion 4 would not catch it, because testAFrozenLatchDoesNotFollowTheSlotSwitch prepares once and never re-prepares (state_tests.cpp:3346). The fix must restore the four published atomics and `pubTrimEver` (to its pre-reset value) WITHOUT touching the retained set or generation. It also needs a new test case: A/B into a vectorless freeze-ON slot → re-prepare → save → no FROZEN_TRIMS. (2) Carrying `trims` for Freeze OFF as well widens the change from 'Freeze semantics' to 'adaptation behaviour on every re-prepare'. It is arguably more consistent with inv 3 ('rate-limited, not stepped'; today's snap to zero is a step), but it is a separate owner call behind the same gate. The narrower 'carry only while frozen' alternative should be offered as the default scope, not as a fallback. (3) The two-set rationale documented at AdaptiveEngine.h:586-612, 624-636 and MODE policy 219-222 ('reset() must zero it or the P5 overlay would report a vector the DSP is not using') becomes false. The same round must rewrite it, or a later cleanup will misread which divergences the `hasPublishedTrims`/retained split still guards (sanitiseState repair, freeze-before-audio, slot scope). (4) Gate naming is correct (MODE Enforcement + ADR-0014 amendment). Because ADRs are append-only (ADR_POLICY rule 4), the 'amendment' must be a new ADR that cross-links ADR-0014. (5) Test pins that change: the liveLatch premise at state_tests.cpp:3325-3326. Any DSP test that reuses one engine instance across prepare() and expects a clean adaptive state must also be audited.

**Architecture gates.**

- MODE_AND_ADAPTATION_POLICY Enforcement: change to Freeze semantics = Architecture Review Gate item + AI Agent Hard Stop
- ADR-0014 (Accepted): the frozen vector's landing and retention rules; carrying it across reset() needs an ADR amendment or new ADR
- Thread Model (ADR-0011) only if the wrapper-side re-stage alternative were chosen (rejected); the engine-local fix adds no cross-thread path

**Dependencies.** [STATE-011](findings-state-model.md#state-011) (KI-007 item 1: settle the Freeze-across-discontinuity rule together); [VIS-011](findings-visualisation.md#vis-011) (without a trim readout neither the defect nor the fix is user-verifiable); KI-006 / ADR-0014 documentation

**Acceptance criteria.**

- With Freeze ON and a latched non-zero vector, prepareToPlay at (same rate, same block), (new rate) and (new block size) leaves publishedTrim*() equal to the pre-prepare values, with hasPublishedTrims() true after the next block
- Output rendered after such a re-prepare is bit-identical to a fresh instance injected with the same vector, and differs from a never-adapted instance (the probe's B vs D and B vs C comparisons invert)
- An offline render preceded by a host re-prepare renders with the frozen vector (trims non-zero during the bounce)
- The session save still writes the same FROZEN_TRIMS, and the round-42 slot-scope test (testAFrozenLatchDoesNotFollowTheSlotSwitch) stays green
- testNullWithDefaults remains bit-exact, testFreezeLatchesTrims passes, TSAN is clean on the re-prepare stimulus, and no new cross-thread write is added
- KI-006 audio half is closed, the liveLatch test premise is updated, and USER_MANUAL §4's exact-restore promise is true (or explicitly amended if the owner rejects the change)

<details><summary>Verification record</summary>

**Method.** Read the chain e769f33:src/PluginProcessor.cpp:754-776 → AnabasisEngine.cpp:134,144,281 → AdaptiveEngine.h:106-120,166 and finishBlock's freeze guard (:418). Read KI-006 (KNOWN_ISSUES.md:336-414), USER_MANUAL.md:308-311 and the liveLatch test (e769f33:tests/state_tests.cpp:3318-3330). Built a DSP-only probe against the real AnabasisEngine.cpp at e769f33 (scratchpad rt/verify-18/probe): latch a vector with Freeze, re-prepare, then render the same test programme (a) through the re-prepared engine, (b) through a never-adapted engine and (c) through a fresh engine with the latched vector injected. I ran three variants: 48k/512→48k/512, →96k/512 and →48k/64. I also ran an offline (nonRealtime) bounce with and without a re-prepare. Harness on :148 (stepped pointer motion to FREEZE): savexml before and after `sr 96000 512`, a dump, and screenshots of the FREEZE row.

**Corrections to the candidate claim.** The scope is wider than the title and the KI-006 heading. The trigger is EVERY prepareToPlay, not only a rate or block change. A same-rate, same-block re-prepare also rendered bit-identical to a never-adapted engine. The codebase itself assumes hosts re-prepare on transport start (PluginProcessor.cpp:779-781) and that most hosts re-prepare when entering an offline render (AnabasisEngine.cpp:428-434). If that holds, a frozen session's bounce is rendered without its frozen trims. I did not verify this per host. The suspected root cause overstates the gate: KI-006 itself says the engine's RETAINED set lets the audio fix stay inside AdaptiveEngine with no thread crossing. The threading-model concern applies only to the wrapper re-stage variant (the round-40 approach). ST-17's 'limGain 9.8 dB (frozen trim) unchanged' reads a macro-managed parameter, not a trim.

</details>

<sub>Verifier scores (1-5): impact 4 · frequency 3 · severity 4 · discoverability 5 · efficiency 3 · coherence 5 · change risk 3 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### STATE-005

**A failed session restore is silent: corrupt, truncated or foreign blobs are rejected and the plugin keeps whatever state it had, with no warning**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | confirmed | Feedback/observability | Save, load, browse and restore change or lose state without saying so | Phase 4 |

**Evidence**

- e769f33:src/PluginProcessor.cpp:1824-1829 — `if (xml == nullptr) return; // corrupt input: keep current state` and `if (! root.hasType ("AnabasisRoot")) return; // foreign input: keep current state`
- e769f33:src/PluginProcessor.cpp:1871 — requestMeterReset() runs only after both guards, so a rejected load leaves every hold in place
- e769f33:tests/state_tests.cpp:155-172 — testCorruptAndForeignState pins the silent no-op
- e769f33:docs/architecture/design-decisions/ADR-0007-state-schema-v1.md:69-71 — 'a foreign root tag or an undecodable blob leaves state untouched'
- e769f33:docs/policies/SESSION_COMPATIBILITY_POLICY.md:49-50 — rule 7: 'An unreadable state falls back to defaults' (drifts from the ADR and the code)
- e769f33:docs/architecture/design-decisions/ADR-0026-slot-payload-read-rules.md:52-58 and :123-125 — the detach mask is dropped 'silently — there is no load-diagnostics channel'; 'if one is ever added … this ADR should be revisited'
- e769f33:src/gui/PluginEditor.cpp — no notice, banner or status component (grep); the only user-facing text channels are labels and tooltips
- Runtime E10 — session capture `rt/edges/53b-after-load-etc-hostname.png` (viewed): 3 B, 100 B, 4000 B and 8 KB random blobs left the state unchanged with no message
- Own reproduction — session capture `rt/verify-16/30-after-trunc-load.png`; pre-trunc.xml and post-trunc.xml byte-identical after 'load trunc.bin' (2000 of 8556 bytes)

**Current behaviour.** When the host hands the plugin a blob that fails XML decoding, or has a foreign root, setStateInformation returns immediately. Parameters, settings, both slots, the preset label and the meter holds all stay as they were, and nothing is recorded or shown. A live instance keeps its current settings; a freshly created instance stays on defaults.

**Problem.** The user cannot tell that their saved plugin state was not restored. The robustness decision (no partial application) is sound, but the absence of any signal turns a rare corruption into an unnoticed wrong master.

**Root cause.** The ADR-0007 read rules make rejection a no-op by design, and the product has no user-facing diagnostics or notice surface to report it on. ADR-0026 records the same gap for partial drops.

**User impact.** Rare, but the consequence is high when it happens. A damaged or truncated project chunk reopens on Default, or on the previous settings, while the user believes it is their mastered chain. They may keep working or bounce without noticing. The statistics are not even reset, so nothing changes visibly at the moment of the failure. *Scope:* The two early-return paths in setStateInformation. The same notice surface would also serve the ADR-0026 slot-decline and mask-drop cases, and the silent preset-file failures observed in E11.

**Proposed improvement.** Target behaviour: when a non-empty state blob is rejected, the state stays untouched (ADR-0007 unchanged), and the editor, whenever it is next visible, shows a persistent dismissible notice in the top bar. The notice says that the saved plugin state could not be read and that the plugin is running its current or default settings, with wording supplied by the owner under C8. A later successful load clears it. Mechanism: setStateInformation increments an atomic 'rejectedLoadEpoch' (relaxed, lock-free, no allocation; the same shape as the existing historyEpoch). The editor timer compares it to its last-seen value. Build this one notice surface so that the ADR-0026 partial-drop cases and the preset-load failures can reuse it. Correct SESSION_COMPATIBILITY rule 7 to match ADR-0007 ('keeps the current state').

**Alternatives considered.**

- *A. Leave silent (status quo)* — Keeps the ADR-0007 robustness but leaves the high-consequence case undetectable. Not justified now that ADR-0026 has itself named the missing channel.
- *B. On rejection, reset to defaults (the literal rule 7 text)* — Changes ADR-0007 read semantics (a gate) and still tells the user nothing. It would destroy the live state on a host recall of a bad blob. Reject.
- *C. Log only (DBG or a file)* — Invisible to users. At most a complement to the notice.
- *D. A modal alert from setStateInformation* — This path may run off the message thread, and a modal would block project loading. Reject.

**Decision: Proceed · P2.** Low frequency but high consequence, and it can be addressed without touching the read semantics or the schema. ADR-0026 explicitly invites a diagnostics channel and names itself for revisiting, and the same surface would close the silent preset-file failures. The policy-text drift should be fixed in the same change.

**Architecture gates.**

- No schema or read-rule change if the ADR-0007 no-op is kept
- New cross-thread datum (setStateInformation is not promised on the message thread): add a THREAD_MODEL.md row. The existing relaxed-epoch pattern (historyEpoch) is reused, so this should not be a threading-model change; confirm under ARCHITECTURE_REVIEW_GATE
- User-visible wording is owner-owned (DEVELOPMENT_BRIEF C8)

**Dependencies.** E11-derived preset-file silent-failure finding (preset batch) — should share one notice surface; ADR-0026 revisit (it names the diagnostics channel as the trigger); [STATE-001](findings-state-model.md#state-001) (an 'unknown edited state' hint could use the same surface)

**Acceptance criteria.**

- Feeding setStateInformation a non-empty undecodable blob, a truncated valid blob or a foreign-root blob leaves getStateInformation byte-identical (testCorruptAndForeignState still passes).
- After such a load, the open editor (or one opened later) shows a visible notice within one timer tick. It persists until dismissed or until a subsequent successful load.
- A zero-length blob does not raise the notice, and a successful load clears any pending notice.
- The flag is published with no lock or allocation on the setStateInformation path, and a THREAD_MODEL.md row documents it.
- SESSION_COMPATIBILITY_POLICY rule 7 and ADR-0007 describe the same rejection behaviour.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/PluginProcessor.cpp:1822-1829 (two early returns: 'keep current state') and :1871 (the meter reset runs only on the accepted path). Read e769f33:tests/state_tests.cpp:155-172, ADR-0007:69-71, ADR-0026:52-58 and 123-125, and SESSION_COMPATIBILITY_POLICY rule 7. Grepped src/gui for any notice, banner or status surface: none exists besides tooltips. Viewed session capture `rt/edges/53b-after-load-etc-hostname.png` ('Default *' at 40 %, unchanged, no message). Reproduced on :146: truncated a valid 8.5 KB state to 2000 bytes and loaded it. The before and after savexml were byte-identical, the UI showed no message, and the statistics holds were retained (verify-16/30-after-trunc-load.png).

**Corrections to the candidate claim.** (1) The cited manual passage (USER_MANUAL.md:406-409) is about forward compatibility ('sessions from older versions keep loading'), not a promise about corrupt data. The relevant contract is SESSION_COMPATIBILITY_POLICY rule 7 (:49-50). That rule says an unreadable state 'falls back to defaults', while the code and ADR-0007:69-71 keep the current state, so the policy text itself drifts. (2) In the commonest real case, a DAW reopening a project into a freshly constructed instance, 'keep current state' means defaults. The user therefore sees 'Default', which is a hint only if the session used another preset. (3) Because a rejected load skips requestMeterReset, even the statistics give no sign; a successful load visibly resets them. (4) Silent partial drops also exist (the ADR-0026 slot decline and detach-mask drop), and ADR-0026 itself names the missing load-diagnostics channel. (5) Real-world frequency is unknown; nothing in the repo measures it.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 1 · severity 4 · discoverability 5 · efficiency 2 · coherence 3 · change risk 2 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### STATE-006

**The ADV toggle is an undo step by owner decision (ADR-0018); undoing across it switches view, resizes, clears redo and dips the audio**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Preserve** | **none** | high | confirmed | State management | The tier and undo model is principled in code but invisible at the point of action | — |

**Evidence**

- e769f33:src/PluginProcessor.cpp:263 — undoEligible excludes only view-tier ids; advancedMode is no longer view-tier (ADR-0018)
- e769f33:src/PluginProcessor.cpp:354 and :588-597 — gesture-end pushUndoStep; 'redoStacks[activeSlot].clear(); // a new edit invalidates the redo line'
- e769f33:src/PluginProcessor.cpp:617-619 — undo: engine.requestForcedDuck(); applySlotToLive(prev.slot, /*adoptAdvanced*/ true)
- e769f33:docs/architecture/design-decisions/ADR-0018-copy-and-advanced-join-the-undo-history.md:88-95 — Consequences: an ADV click is one working undo step; the ADV-only undo still ducks, 'inaudible-by-design'
- e769f33:docs/architecture/design-decisions/ADR-0018-copy-and-advanced-join-the-undo-history.md:178-201 — owner-reviewed cross-slot view adoption, left as is
- e769f33:docs/KNOWN_ISSUES.md:690-712 — KI-010: the forced duck never dry-fills; every undo step dips to silence for ~34 ms
- e769f33:docs/user/USER_MANUAL.md:161 — undo 'covers … the ADV view switch'
- ST-09(e) + session capture `rt/state/20-undo-topbars.png` — Redo enabled before the ADV click, dimmed after; undo turns ADV off (822→720)
- E18 + session capture `rt/edges/63a-undo-x25.png` — the undo walk includes a view switch

**Current behaviour.** Clicking ADV pushes an undo step on the active slot's stack and clears that slot's redo line. Undo/redo across that step restores the recorded view: the editor switches view and resizes, and the engine takes the §2.8 forced duck (~34 ms dip to silence, per KI-010). An undo can adopt a view that was toggled in the other A/B slot.

**Problem.** A sound-neutral view switch behaves like an edit. It spends an undo press, invalidates redo (a user who undoes several edits and then opens Advanced to inspect loses the way back), and produces an audible dip on an undo that changes no sound. These are the recorded consequences of an owner-directed Accepted ADR, except the redo invalidation, which the ADR does not state, and the dip, which the ADR assumes is inaudible.

**Root cause.** ADR-0018 (owner's 0.1.1 directive, sibling parity) made advancedMode a real undo participant. pushUndoStep's standard linear-history rule clears redo, and undo()/redo() request a forced duck unconditionally. The duck has no dry-fill (KI-010).

**User impact.** Occasional. A redo line lost to a view switch forces the user to redo edits by hand. An ADV-only undo produces a ~34 ms dropout, which may happen mid-comparison. The view jump on undo is expected by the owner's contract. *Scope:* All sessions that use undo/redo together with view switching; per A/B slot.

**Proposed improvement.** Keep the ADR-0018 contract. For the fine review: (1) record in ADR-0018 §Consequences, and in USER_MANUAL §3.1, that an ADV click is a new action and therefore clears the redo line; (2) reconcile ADR-0018:93-95's 'inaudible-by-design' with KI-010. If the KI-010 dry-fill lands ([DSP-002](findings-dsp-tech.md#dsp-002)), the ADV-only undo dip becomes a masked crossfade and needs no special case.

**Alternatives considered.**

- *Remove ADV from undo (restore ADR-0010 option E's undo half)* — Reverses an explicit owner directive and an Accepted ADR. Gate; not justified by the evidence.
- *Let an ADV step leave the redo line intact* — Breaks linear-history coherence: redo entries carry their own advancedMode, so redo after an unrelated ADV step would restore a stale view. Also an ADR-0018 amendment. Reject.
- *Skip the forced duck when the popped entry differs from live only in advancedMode (strippedForUndoCompare already exists)* — Removes the only audible symptom, but amends ADR-0018 Consequences :93-95 and touches the §2.8/DSP_POLICY invariant-8 bulk-swap rule and the ADR-0014 frozen-trim staging. Better handled by the KI-010 dry-fill fix, which covers every undo.
- *Pin the view on undo across A/B divergence* — Owner-reviewed and explicitly rejected (ADR-0018:196-201); it would make the ADV undo step unreachable.
- *Leave behaviour as is and document the redo consequence (chosen)* — Respects the owner's decision; closes the only undocumented consequence.

**Decision: Preserve · none.** Each behaviour is a deliberate, owner-directed and owner-reviewed outcome of Accepted ADR-0018, with its trade-offs analysed there, and the alternatives either reverse the owner or break history coherence. What remains is documentation (redo invalidation is unstated) and a premise drift about the duck, which belongs to KI-010/DSP-002 rather than to the undo contract.

**Architecture gates.**

- Conflict with Accepted ADR-0018 (Decision 2-4; Consequences :88-95; Review confirmation :178-201) — any behavioural alternative
- Simple/Advanced macro-layer contract family (ADR-0018's gate note: the toggle's tier membership) — any change to advancedMode's undo/A-B travel
- DSP_POLICY invariant 8 / §2.8 transition layer — the skip-the-duck alternative only

**Dependencies.** [DSP-002](findings-dsp-tech.md#dsp-002) (KI-010 forced duck without dry-fill); [UX-015](findings-ux.md#ux-015)

**Acceptance criteria.**

- ADV remains an undo step; undo restores the previous view; an A/B switch and Copy never move the view (testTeardownAndReengageInvariants cases (3)/(3b)/(5) unchanged)
- ADR-0018 §Consequences and USER_MANUAL §3.1 state that an ADV click clears the redo line
- ADR-0018:93-95's audibility statement is reconciled with KI-010 (or superseded by the [DSP-002](findings-dsp-tech.md#dsp-002) dry-fill outcome)

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/PluginProcessor.cpp:252-275 (gesture-begin: advancedMode passes the view-tier filter), :354 (gesture-end pushUndoStep), :588-598 (pushUndoStep clears redo), :608-633 and :635-648 (undo/redo requestForcedDuck + applySlotToLive(..., adoptAdvanced=true)); e769f33:docs/architecture/design-decisions/ADR-0018-copy-and-advanced-join-the-undo-history.md (Status, Decision 2-4, Consequences :88-98, Review confirmation :178-201); ADR-0010 option E (:65-74); e769f33:docs/KNOWN_ISSUES.md:690-712 (KI-010); e769f33:src/dsp/AnabasisEngine.h:49-53. Viewed session capture `rt/state/20-undo-topbars.png`: in row 9 (slot A) Redo is enabled; in row 10 (ADV on) Redo is dimmed, i.e. the ADV click cleared a non-empty redo line. Row 11: undo returns ADV off.

**Corrections to the candidate claim.** Every mechanic is real, but each except one is a recorded, owner-approved decision. ADV as an undo step comes from the owner's 0.1.1 directive (ADR-0018 Decision 2-4). Undo restoring the previous view and resizing is Consequences :88-89. Cross-slot view adoption was owner-reviewed on 2026-08-07, 'no change requested' (:178-201). The duck on an ADV-only undo is accepted 'for uniformity' (:93-95). ADR-0018 re-reads ADR-0010's X11 hazard as automation-thread-only (:70-71); no crash was observed. NOT recorded in ADR-0018: that an ADV click invalidates the redo line. ADR-0018's premise that the duck is 'inaudible-by-design' is contradicted by KI-010: the forced duck never dry-fills, so every undo dips to silence for ~34 ms.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 2 · severity 2 · discoverability 3 · efficiency 2 · coherence 2 · change risk 4 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### STATE-007

**How an edit is made decides its undo and macro behaviour: arrow keys make no undo step and never detach, while each wheel notch is its own undo step and, over a macro, re-engages every detached parameter**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Interaction model | The tier and undo model is principled in code but invisible at the point of action | Phase 3 |

**Evidence**

- JUCE pin build/_deps/juce-src/modules/juce_gui_basics/widgets/juce_Slider.cpp:1029-1058 — keyPressed calls setValue(..., sendNotificationSync) with no ScopedDragNotification. :1160-1166 — mouseWheelMove wraps each notch in ScopedDragNotification (:1164). :445-456 — textChanged is bracketed (:451).
- e769f33:src/gui/PluginEditor.h:206-270 — Knob overrides only mouseDown/mouseDoubleClick and not keyPressed or mouseWheelMove. e769f33:src/gui/PluginEditor.cpp:1153-1169 — every knob accepts keyboard focus (0.1.1), with no gesture handling.
- e769f33:src/PluginProcessor.cpp:259-276 — an undo step is armed only by a gesture begin. :650-664 — detach needs managedGestureBits, which only gestures set. :286-310 — a gesture begin on loudness/character/tone sets pendingReengage and clears the whole mask.
- e769f33:src/PluginProcessor.cpp:366-372 with e769f33:src/PluginProcessor.h:457 — every push is capped at 128 and the oldest entry is trimmed. At 2.93 % per notch, about 34 notches sweep one knob's full range.
- e769f33:src/MacroEngine.cpp:225-256 — any macro change, gestured or not, rewrites all nine managed parameters except detached ones.
- e769f33:src/gui/PluginEditor.h:237-246 and e769f33:src/gui/LookAndFeel.cpp:823-828 — the code base's own rule is that an unbracketed write 'was neither an undo step nor a DETACH'. Double-click reset and value-box drag were bracketed to remove exactly that asymmetry, but keys were not.
- e769f33:src/gui/LookAndFeel.cpp:898-918 — a click on a macro's number deliberately opens no gesture, because a macro gesture begin wipes the detach mask ('pressing on a numeric readout is not that notice'). No comparable consideration exists anywhere for the wheel.
- e769f33:docs/architecture/design-decisions/ADR-0005-macro-layer-architecture.md:111-114, 131-133, 249-250 — detach needs a gesture ('a real UI drag'), the next macro gesture re-engages, and undo gets 'one coalesced step per knob drag'. e769f33:docs/user/USER_MANUAL.md:141-151 — the universal gestures list omits both the wheel and the keys.
- Runtime verify-17, slot B with empty history. Click Tone (no drag), Up x3: Tone 0.00->0.06, colourTone and eqTilt moved, preset 'Default *', Undo still disabled. — session capture `rt/verify-17/12-after-3-up-crop.png`, .../12-after-3-up-undo.png
- Runtime verify-17. Three wheel notches on Tone took it to 0.24. Undo x4 gave 0.18, 0.12, 0.06, 0.06: one step per notch, and the arrow-key edit 0.00->0.06 was never undoable. — session capture `rt/verify-17/13-after-undos-undo.png`
- Runtime verify-17, Advanced. Down x5 on Comp Threshold took it -12->-14 dB with no badge (15-thr-zoom2.png) and no Simple edited dot (16-simple-after-arrow-crop.png). One wheel notch on Loudness then reset Threshold to -12.0 dB. — session capture `rt/verify-17/15-thr-zoom2.png`
- Runtime verify-17. Dragging Threshold to -16 dB showed the badge (17-thr-zoom.png). With Tone reached via Shift+Tab from Ceiling, Up x2 ran the mapping (colourTone 0.08->0.10) and Threshold stayed at -16 with the edited dot. ONE wheel notch on Tone then took Threshold to -12.0 dB and the dot disappeared. — session capture `rt/verify-17/23-dot-compare.png`
- Phase-2 G-04 (2.93 % per notch; session capture `rt/gestures/18-tone-wheel.png`), G-08 and E08 (arrow steps of 1 % or 0.01 dB; focus invisible; [capture](captures/12-keyboard-focus-invisible.png)), G-14 (typed Loudness re-engaged).

**Current behaviour.** Input methods that make the same value change behave differently. A drag, double-click, Alt-click, value-box drag and typed entry each make one undo step, detach a managed parameter and, on a macro, re-engage everything. Arrow keys make no undo step and never detach. On a macro they remap without re-engaging, so an arrow-edited managed value is folded into the neighbouring step and is later overwritten by any macro move, with no badge. Each wheel notch is a full gesture: one undo step per notch against a 128-entry cap, and one notch over Loudness, Character or Tone clears every detached edit. A plain press on a macro knob to focus it also re-engages.

**Problem.** The same intention ('nudge this knob') has different undo and macro-layer consequences depending on the device. A short scroll floods the history (N presses to take back N notches, and older steps are evicted beyond 128). An accidental scroll over the big Loudness knob both changes Loudness and discards all hand-tuned Advanced edits. Keyboard edits to managed parameters are unrecorded and unprotected. The code base explicitly removed this class of asymmetry for double-click and value-box drag.

**Root cause.** Undo coalescing (PluginProcessor.cpp:259-356) and the detach/re-engage discriminator (:286-310, :650-664) both key on the host gesture bracket. Knob inherits stock juce::Slider input handling, which brackets the wheel per notch (juce_Slider.cpp:1164) and typed text (:451) but not keys (:1057). Anabasis has no Knob override for either keys or the wheel.

**User impact.** Wheel users (common on desktop) get cluttered, deep undo histories and can wipe detached Advanced edits with one stray notch over a macro. Undo recovers the edits, but only one notch per press and only if the user notices that the badges vanished. Keyboard users (rare, and hampered by the invisible focus in E08) get edits they cannot undo separately and that silently fall back to the macro curve. *Scope:* All 40 rotary knobs for the wheel, and the three macros for re-engage. Arrow keys on every focused knob, and the nine managed parameters for detach. Both views.

**Proposed improvement.** One user intention should be one gesture, whatever the input device. In Knob (src/gui/PluginEditor.h), make two changes. (1) Override keyPressed: the first arrow key of a burst opens a bracket held in a member (resetParam->beginChangeGesture() or a Slider::ScopedDragNotification). Each key applies the stock step inside it, and the bracket closes after about 500 ms without a key or on focusLost. A key burst is then one undo step and, on a managed parameter, detaches with the badge like a drag. (2) Override mouseWheelMove to keep one bracket open across a burst of notches with the same idle close. It applies the stock delta without Slider's per-notch ScopedDragNotification, which avoids a nested-begin jassert. A wheel burst is then one undo step. (3) Owner decision at the gate: whether a key or wheel burst over Loudness/Character/Tone re-engages detached parameters. Recommended: yes for a deliberate burst, as a macro move under ADR-0005 item 6, but identical for keys and wheel. Document the wheel and keys in USER_MANUAL §3 'Universal gestures'.

**Alternatives considered.**

- *Leave as is and document the behaviour* — Cheapest, but keeps one undo step per notch against the 128 cap and the keyboard detach gap the code base already closed for the other inputs.
- *Merge consecutive same-parameter undo steps inside pushUndoStep within a time window* — A plausible gate-free first step that fixes the wheel flooding. It does not touch detach or re-engage asymmetry, and it changes the undo contract of 'one step per completed gesture'.
- *Disable the wheel on the three macro knobs* — Prevents an accidental re-engage but removes a common input and makes the macros inconsistent with the other knobs.
- *Make wheel notches over a macro never re-engage (as the value-box click does)* — This is a macro-layer contract change (ADR-0005 item 6) and conflicts with the code's own view that a knob press is 'a genuine macro grab'. It is owner-only.

**Decision: Modify · P2.** The defect is real and code-confirmed, and it contradicts the product's own stated rule that every input writing a parameter should be bracketed. Burst coalescing (step granularity) can go ahead without a gate. Bringing keys into the bracket changes detach and re-engage behaviour, and so does any change to wheel re-engage. Those parts need owner confirmation under the macro-layer contract rather than a unilateral change.

**Architecture gates.**

- Simple/Advanced macro-layer contract change (ADR-0005 items 3 and 6). Bracketing arrow keys makes a key nudge on a managed parameter detach and a key nudge on a macro re-engage. Any change to whether a wheel notch over a macro re-engages is also a contract decision for the owner. No conflict with ADR-0005's text was found, since item 3 already treats UI edits as gestured.

**Dependencies.** [STATE-003](findings-state-model.md#state-003) (an undo preview would expose the per-notch steps and the missing key steps); [STATE-010](findings-state-model.md#state-010) (same gesture-keyed undo model; ungestured edits fold); Keyboard-focus visibility finding from E08/G-08 (the arrow-key path is barely reachable while focus is invisible); Wheel step size (G-04)

**Acceptance criteria.**

- A burst of arrow presses on a focused knob (closed by about 500 ms idle or focus loss) makes exactly one undo step, and one Undo restores the value before the burst.
- An arrow-key edit on any of the nine managed parameters in Advanced shows the detach badge and the Simple edited dot, as a drag does.
- N wheel notches within the idle window make exactly one undo step, not N; a 40-notch scroll does not evict older history.
- Keys and the wheel behave identically over Loudness/Character/Tone with respect to re-engage, per the owner's recorded decision.
- The host sees balanced begin/end for key and wheel bursts: no JUCE isPerformingGesture jassert in a Debug build, and pluginval stays green.
- USER_MANUAL §3 'Universal gestures' documents the wheel and arrow keys, including what they do to undo and to macro-managed knobs.
- A test drives Knob keyPressed and mouseWheelMove and asserts undo-step counts and detach bits.

<details><summary>Verification record</summary>

**Method.** Read the pinned JUCE (build/_deps/juce-src) juce_Slider.cpp at :445-456, :1029-1058 and :1139-1171. Read e769f33 e769f33:src/gui/PluginEditor.h:206-270 (Knob overrides only mouseDown/mouseDoubleClick), e769f33:src/gui/PluginEditor.cpp:1153-1169, e769f33:src/gui/LookAndFeel.cpp:820-925, e769f33:src/PluginProcessor.cpp:221-356 and 650-664, e769f33:src/MacroEngine.cpp:103-116 and 225-256, ADR-0005:105-140 and 249-250, and USER_MANUAL.md:141-151. Grepped src/ and docs for any deliberate wheel handling and found none. Reproduced on :147 with stepped motion. In slot B with an empty history: Up x3 on Tone, then 3 wheel notches, then Undo x4. In Advanced: Down x5 on Comp Threshold, then a wheel notch on Loudness. Detached Threshold by dragging, then Up x2 on Tone reached via Shift+Tab, then one wheel notch on Tone.

**Corrections to the candidate claim.** Typed entry is bracketed (juce_Slider.cpp:451), so PF-anamorph-reference-3 is refuted, as the candidate already noted. The claim 'the next macro gesture silently overwrites it with no badge' is true but incomplete. By design, a gestured macro move also overwrites a dragged (detached) edit (ADR-0005 item 6). The arrow-key edit differs in three ways: it shows no badge or edited dot, it has no undo step, and an ungestured macro change such as an arrow key on a macro also overwrites it, whereas a detached edit survives that change (observed). Undo after arrow keys and after the wheel is now observed. Additionally, a plain press with no drag on a macro knob, for example to focus it for keyboard use, is itself a macro gesture and re-engaged a detached parameter (observed).

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 3 · severity 3 · discoverability 4 · efficiency 3 · coherence 4 · change risk 3 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### STATE-008

**The A/B 'independent setups' share the learned reference, LOCK, every Settings row (oversampling included) and the session meter holds**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | partially-confirmed | State management | The tier and undo model is principled in code but invisible at the point of action | Phase 1 |

**Evidence**

- e769f33:src/InternalState.h:15-16 — 'NEVER participate in A/B, undo, or presets'; :25-41 field list (oversample, osPhase, offlineQuality, ceilingLock, integratedStd, rmsRef…)
- e769f33:docs/architecture/design-decisions/ADR-0007-state-schema-v1.md:81-86 — ANABASIS_INTERNAL session scope; ADAPTIVE global
- e769f33:docs/architecture/design-decisions/ADR-0004-latency-contract-constant-lookahead-allowance.md:256-260 — OS/phase/offline never travel with A/B ⇒ A/B can never cross reported latency
- e769f33:src/gui/LoudnessMeterView.cpp:67-70 and e769f33:src/PluginProcessor.cpp:1871 — the only requestMeterReset callers
- e769f33:src/gui/LoudnessMeterView.cpp:86-94 — PLR = shown TP hold − shown integrated
- e769f33:docs/user/USER_MANUAL.md:411-418 ('complete, independent sound setups'), :424-435 (transparent-master step 4 'A/B + Copy, judge PLR'), :271-274 (Statistics reset click), :278 (Settings = session state; silent on A/B scope)
- VER8-7 (stats before vs after A/B: I −12.9 → −12.9, TP 0.62 kept, LRA 7.0 kept): session capture `rt/verify-8/v03v04-stats-pair.png`
- VER8-8 (OS 2x set in B persists in A; latency 484 in both): session capture `rt/verify-8/v16-crop.png`
- VER8-9 (Settings overlay carries no A/B scope statement): session capture `rt/verify-8/v13-settings.png`
- V-08: session capture `rt/visuals/10b-after-AB-editor.png`

**Current behaviour.** The A/B switch swaps every sound parameter, the name, identity, detach mask and frozen trims. It leaves the following session-wide and unchanged:
• oversampling, phase and offline render;
• the meter standards and LOCK;
• the learned reference;
• the session-cumulative Statistics (integrated, LRA, TP and SP holds, and the PLR derived from them).
Nothing in the UI or the manual's A/B section says which state is per-slot and which is shared.

**Problem.** The manual's recommended compare workflow reads PLR after A/B, but the displayed PLR is max TP over both slots minus integrated loudness over both slots' playback: a figure that describes neither candidate. Separately, the 'complete, independent setups' wording leads users to expect that oversampling or phase can differ between A and B. It cannot.

**Root cause.** There are two causes.
1. The meter holds are engine-global accumulators, and the A/B path (switchToSlot, e769f33:src/PluginProcessor.cpp:1556-1601) never touches them; only a Statistics click and a session load reset them.
2. The tier split is deliberate (ADR-0004/ADR-0007) but is invisible in the UI and misstated in the manual, which says 'complete' where it means 'all sound parameters'.

**User impact.** A user comparing a gentle A against a louder B reads a PLR for B that is inflated by A's lower loudness in the shared integrated window, and may pick the wrong candidate on dynamics. Users who try to A/B oversampling settings find that the change leaks into both slots. *Scope:* Every A/B comparison that reads the Statistics panel, which is the default view in both modes. The Settings-scope confusion applies whenever a user expects per-slot oversampling or phase.

**Proposed improvement.** Keep the tiers and make their scope honest.
1. Statistics after an A/B switch: the session-cumulative rows (I, LRA, TP, SP, PLR) show that they now span both slots. Add a small 'A+B' tag beside the STATISTICS header and dim PLR until the next reset, clicking the panel as today. Target experience: the user sees at a glance that the PLR describes a mix, and one click gives a clean measurement of the slot they are hearing.
2. Follow-up, deferred until [VIS-009](findings-visualisation.md#vis-009) settles the reset matrix: per-slot session-cumulative accumulators that swap with the slot at the block top, so each slot's I/LRA/TP/SP/PLR reflect only its own playback.
3. Docs and UI copy:
   • USER_MANUAL §7.4 lists what is per slot and what is shared (Settings including Oversampling, Phase and Offline Render; LOCK; the learned reference; the Statistics holds);
   • §8 step 4 reads 'switch, click STATISTICS to reset, play the passage, then read PLR';
   • the Settings overlay title carries 'shared by A and B'.

**Alternatives considered.**

- *Make oversampling/phase/offline per-slot* — Rejected. A/B would change reported latency (hard-stop), contradicting ADR-0004 §Consequences and the manual's latency promise; it is also a schema change under ADR-0007.
- *Make Learn per-slot* — Rejected. ADR-0007 deliberately keeps learned targets global, both slots process the same material, and it would be a serialization change.
- *Auto-reset the Statistics on every A/B switch* — Not preferred as the default. Integrated and LRA need seconds to settle, so rapid toggling would leave them permanently unsettled, and it silently discards a deliberate whole-programme measurement. Acceptable as an opt-in Settings row.
- *Docs-only correction* — Necessary but insufficient: the misleading PLR is still shown by default with nothing on screen to flag it.

**Decision: Modify · P2.** The claim that everything shared should become per-slot is the obvious fix, and it collides with ADR-0004's latency contract and ADR-0007. The part that actually misleads a mastering decision (the PLR and holds spanning slots in the documented compare workflow) can be fixed without any gate: a scope tag plus doc corrections, with per-slot accumulators as a deferred follow-up. P1 because it corrupts the figure the manual's primary workflow tells the user to judge, silently, on every compare pass. The cheap workaround (click to reset) is not signposted where it matters.

*Calibration:* the verifier judged Modify / P1; the final judgement is Modify / P2. Challenge accepted: accumulate-until-reset is the ADR-0020 meaning and recovery is one documented click, and the discoverability harm is carried by [UX-002](findings-ux.md#ux-002) (P1), so P2. Smaller Modify: USER_MANUAL §8 step 4 = switch, reset STATISTICS, play, read PLR; a per-slot vs shared list in §7.4 (Settings incl. Oversampling/Phase/Offline Render, LOCK, learned reference, Statistics holds); extend the A/B tooltip; fix the stale 'OS factor' comment at e769f33:src/PluginProcessor.cpp:1573. Any on-panel cue must be change-agnostic (not A/B-only), is an ADR-0020 amendment, and must not break the 350 px Settings height; per-slot accumulators dropped. Per-slot OS/Phase would be a reported-latency hard stop (ADR-0004).

*Adversarial challenge:* evidence holds: yes; priority justified: no (suggested P2); decision justified: yes (suggested Modify). **Why not P1.** The rubric reserves P1 for material harm in every session, or a frequent trap with costly recovery. Here recovery is one click on a panel whose reset is documented in bold (USER_MANUAL :271-274) and in its tooltip. The accumulate-until-reset behaviour is the panel's documented, ADR-0020-specified meaning, not a silent substitution. The fault is narrower:
• workflow step 4 (:434) omits the reset;
• §7.4 (:413) overclaims 'complete, independent';
• the reset affordance is hard to discover while tooltips are off by default.
The severity (4) and frequency (4) scores rest on an unmeasured assumption: that reading PLR in A/B is the deciding cue on every pass, which is not in evidence. That makes this a meaningful clarity and consistency fix in a lower-cost situation: P2 (severity about 3, frequency about 3, evidence_confidence about 4).

**Why Modify, but smaller.** Keeping the tiers is right; the gate analysis for per-slot OS and Learn is correct. The smallest sufficient change is:
1. Fix §8 step 4 to say switch, reset STATISTICS, play the passage, then read PLR.
2. Add a per-slot vs shared list to §7.4: Settings including Oversampling, Phase and Offline Render; LOCK; the learned reference; the Statistics holds.
3. Put the scope where users look, for example by extending the A/B tooltip.
4. Correct the stale 'OS factor' comment at e769f33:src/PluginProcessor.cpp:1573.
Any on-panel indicator should be change-agnostic, such as a visible reset affordance or a since-reset age, rather than A/B-only. Drop the deferred per-slot accumulators or move them to Investigate further. *Proposal risks:* 1. **The 'A+B' tag contradicts the product's own model.** The session-cumulative rows mix configurations just as much after:
   • a preset step (‹ ›), which is the other explicit compare action;
   • an undo or redo;
   • any knob move.
   The manual's reset rule (:271-274) is change-agnostic for that reason. A tag that fires only on A/B implies the figure is clean after those other changes. It also fires falsely when the slots are identical right after a Copy.
   A change-agnostic signal covers every case with one mechanism, for example:
   • a visible reset affordance on the STATISTICS header (the tooltip that explains the click is off by default, e769f33:src/InternalState.h:110);
   • or a 'since reset' age readout.
2. **The dim-PLR and header-tag changes amend the panel spec.** ADR-0020 §6 fixes that spec (header, rows, the 202/234 px budget), so it needs an ADR-0020 amendment and a brand-checklist pass. This is not a hard stop, but the judge's gate list leaves it out.
3. **The Settings subtitle may break the recorded panel height.** A 'shared by A and B' line may break the 350 px height ADR-0020 §7 records as 'recomputed, not nudged'. Putting the scope in the manual and in the A/B tooltip avoids the relayout.
4. **The tag flag must be written race-free.** setStateInformation also calls requestMeterReset, so the flag needs a thread-safe write if a host restores state off the message thread.
5. **Per-slot accumulators have weak payoff and real cost.**
   • They still mix configurations whenever the user edits within a slot.
   • Copy gains undefined semantics: should it copy the holds?
   • Returning to a slot shows stale figures from minutes earlier.
   • They need a new block-top swap request. That adds a row to THREAD_MODEL's meter contract, so it should be named as a threading-model review item, not just 'stays inside ADR-0011'.
   • They amend ADR-0020's session-cumulative reset contract at :134-135.
   Recommend Reject or Investigate further rather than Defer.
6. **The judge's gate list is correct** for per-slot OS/phase/offline quality (reported latency, ADR-0004 §Consequences, ADR-0007) and for per-slot Learn.

**Architecture gates.**

- NOT proposed but must be named: per-slot oversampling/phase/offline-quality = reported-latency change on A/B (hard-stop) + conflict with Accepted ADR-0004 §Consequences (e769f33:docs/architecture/design-decisions/ADR-0004-latency-contract-constant-lookahead-allowance.md:256-260) + serialization-schema change (ADR-0007 tier table :81-86)
- NOT proposed: per-slot Learn = conflict with ADR-0007 (ADAPTIVE global, :83) + serialization change
- Deferred per-slot meter accumulators: no serialization (holds are not serialized), but they amend ADR-0020's session-cumulative reset contract (e769f33:docs/architecture/design-decisions/ADR-0020-waveform-statistics-panel.md:134-135) and must stay inside ADR-0011's threading model (swap at the block top via the momentary-request row, like requestMeterReset) — owner decision
- The 'A+B' tag and doc corrections touch no gate

**Dependencies.** [VIS-009](findings-visualisation.md#vis-009) (Statistics reset matrix — the meter-hold part should be resolved together; dedupe in synthesis); [UX-011](findings-ux.md#ux-011) (the inactive-slot readout is the natural place to say what is per-slot); [DSP-002](findings-dsp-tech.md#dsp-002) (duck fades enter the integrated figure, but the pull is ≈0.02 LU and immaterial)

**Acceptance criteria.**

- After an A/B switch with no reset in between, the Statistics panel shows a visible 'spans A+B' indication within one display frame, and PLR is visibly marked; clicking the panel clears the indication together with the holds
- A switch that follows a reset shows no tag until the next A/B
- USER_MANUAL §7.4 lists per-slot vs shared state explicitly (Settings incl. Oversampling/Phase/Offline Render, LOCK, learned reference, Statistics holds), and §8 step 4 instructs a reset before reading PLR
- The Settings overlay states that its rows apply to both A/B slots
- Reported latency remains unchanged across A/B (testReportedLatencyMatchesImpulse / OS matrix tests unchanged)

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/InternalState.h:15-16 and :25-41: oversample, osPhase, offlineQuality, ceilingLock and the meter standards live in ANABASIS_INTERNAL, which never takes part in A/B. Read the ADR-0007 tier table (e769f33:docs/architecture/design-decisions/ADR-0007-state-schema-v1.md:81-86): ANABASIS_INTERNAL is session-scoped and ADAPTIVE is global. Read ADR-0004 §Consequences (e769f33:docs/architecture/design-decisions/ADR-0004-latency-contract-constant-lookahead-allowance.md:256-260). Grepped requestMeterReset: the only callers are e769f33:src/gui/LoudnessMeterView.cpp:67-70 and e769f33:src/PluginProcessor.cpp:1871. PLR = shown TP − shown I (e769f33:src/gui/LoudnessMeterView.cpp:86-94). Reproduced on :138:
• A stepped A/B click left I at −12.9, the TP hold at 0.62 dBTP and LRA at 7.0 LU unchanged (VER8-7).
• Oversampling set to 2x in B was still 2x after switching to A; reported latency was 484 in both slots (VER8-8, app.log).

**Corrections to the candidate claim.** The finding frames the global tiers as a side effect of 'session robustness'. For oversampling, phase and offline quality that is wrong: ADR-0004 §Consequences depends on those three never travelling with A/B, preset or undo, so that an A/B switch can never change reported latency. The manual promises the same ('switching A/B — none of it changes reported latency', e769f33:docs/user/USER_MANUAL.md:357-361, 485-488). ADR-0007 places the learned targets globally on purpose. Learn calibrates to the programme material, which both slots process identically. So per-slot oversampling, phase or Learn is not a fix; it is a hard-stop change. LOCK being global is harmless: it gates only preset applies, and the ceiling value itself is per slot. The confirmed harms are narrower:
1. The session-cumulative Statistics rows (I, LRA, TP, SP and therefore PLR) span both slots after an A/B, inside the manual's own 'A/B + Copy, judge PLR' workflow (:434-435).
2. The manual's 'two complete, independent sound setups' (:413) overclaims, because audible oversampling and phase are shared and nothing in the Settings overlay says so (VER8-9).
The manual does document the Statistics reset click (:271-274), but workflow step 4 does not mention it.

</details>

<sub>Verifier scores (1-5): impact 4 · frequency 4 · severity 4 · discoverability 4 · efficiency 3 · coherence 4 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### STATE-009

**The learned reference is global, cannot be undone or reset, and a Learn stopped just before saving with the transport stopped is lost**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | medium | partially-confirmed | State management | The adaptive engine changes the audio from state the user cannot see, keep or reset | Phase 5 |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:622-640 — onClick calls proc.startLearn()/stopLearn() only; no undo step, no duck
- e769f33:src/PluginProcessor.h:652-653 — comment 'the P5 UI adds the duck-routed engage + undo bracketing' (not implemented)
- e769f33:src/PluginProcessor.h:655-661 — a stop with no further audio leaves the pass uncommitted and getStateInformation writes no ADAPTIVE; 'The P5 Learn grammar owes an acknowledged commit … which is where this closes'
- e769f33:src/PluginProcessor.cpp:1779-1815 — ADAPTIVE child written only when learnedNow
- e769f33:src/PluginProcessor.cpp:1984-1989 — INVARIANT: session load is 'the only site that stages an adaptive record today'; a new stager must pair the mirror and engine stores
- e769f33:src/PluginProcessor.cpp:1994-2011 — the only path to never-learned is a session load without ADAPTIVE (engine.restoreNeverLearned)
- e769f33:src/dsp/AnabasisEngine.cpp:415-422 — the Learn command is consumed at a block top; commit is unconditional on audibility
- e769f33:docs/architecture/design-decisions/ADR-0007-state-schema-v1.md:83 — ADAPTIVE | only the learned reference targets | global; :85-88 StateSet (A/B and undo unit) excludes it
- e769f33:docs/policies/MODE_AND_ADAPTATION_POLICY.md:161-166 — Learn commit deliberately outside undo; undoable Learn = 'a product decision for the P6 pass'
- e769f33:docs/user/USER_MANUAL.md:314-315 'The learned reference is saved with the session'; :413 'two complete, independent sound setups' (no Learn exception stated)
- Runtime verify-4: rt/verify-4/s02-after-learn.xml (ADAPTIVE written after commit), 11-commit-sheet.png (no visible acknowledgement, no dirty marker), s03-after-empty.xml (empty pass keeps previous refs)

**Current behaviour.** A committed Learn replaces the global reference pair. It applies to both A/B slots and is saved in the root ADAPTIVE child. It is not on any undo stack, and no UI action restores the factory or previous references. A stop click only requests the commit, which lands at the next processed block. If the host is not processing, the saved state keeps the previous (or no) reference while the button stays accent-coloured.

**Problem.** The user has no way back from a Learn except another Learn. The manual promises independent A/B setups without stating the shared-reference exception. The UI gives no acknowledgement that the commit reached the engine, although the processor header says it was owed.

**Root cause.** ADR-0007 puts the references in global state outside the StateSet undo unit. The MODE policy deferred the undo mechanism to a P6 product decision that was never recorded. The only writers of the references are commitLearn and the session-load staged record (PluginProcessor.cpp:1994-2011); no UI path stages never-learned or a previous pair. The commit is necessarily audio-thread (the sums live there), and the editor does not acknowledge the commit (see [UX-005](findings-ux.md#ux-005)).

**User impact.** A Learn on the wrong material leaves a persistent, subtle bias in the adaptive trims until another Learn overwrites it. Returning to neutral means reloading an older session or re-inserting the plugin. A user comparing A/B after a Learn is changing a hidden shared input. In hosts that suspend processing when stopped, 'stop Learn, save, close' can silently drop the pass. *Scope:* Adaptive-reference lifecycle: the Learn button, processor state I/O, USER_MANUAL §4 and §7.4, and OPEN_QUESTIONS. The engine DSP is unchanged.

**Proposed improvement.** Keep the global scope and the exclusion from per-slot undo, and add the missing lifecycle affordances:
(a) A 'Reset learned reference' action, e.g. in a LEARN context menu or a Settings row. It stages the never-learned record through a helper that pairs the stagedAdaptive* mirror stores with engine.restoreNeverLearned(), which is the pairing the INVARIANT comment at PluginProcessor.cpp:1984-1989 requires. That puts the session-load path and the new action on one audited route. The next save then omits ADAPTIVE.
(b) Optionally, a one-level 'Revert to previous reference': the message thread keeps the previous pair and stages it the same way. This is the 'dedicated mechanism' the MODE policy names, so it needs the owner's decision first; record it in OPEN_QUESTIONS rather than guess.
(c) Via [UX-005](findings-ux.md#ux-005): a pending-commit state after the stop click until isLearning() drops, then a success acknowledgement and a persistent learned marker.
(d) In USER_MANUAL §4 and §7.4, state that the learned reference is shared by both A/B slots, is not undoable, and how to reset it.

**Alternatives considered.**

- *Per-slot learned references* — Rejected for now. It is a serialization schema change and contradicts ADR-0007's explicit rationale; it needs an ADR and owner review.
- *Push the Learn commit onto the per-slot undo stack* — Rejected. It is the resurrection hazard the MODE policy documents, and it conflicts with ADR-0007's StateSet being both the A/B and the undo unit.
- *Widen the undo unit to carry global state* — Deferred. It is an ADR-0007 (and ADR-0018 undo-contract) change and awaits the owner's undo decision.
- *Commit on the message thread so a save never misses it* — Not feasible without a threading-model change (ADR-0011): the sums live on the audio thread.
- *Documentation only* — The minimum acceptable step. It leaves the missing reset path in place.

**Decision: Modify · P2.** The parts that are defects (no reset, no acknowledgement, undisclosed A/B exception) can be fixed without touching the Accepted ADR-0007 scope or the policy's out-of-undo decision. The parts that would need gates (per-slot, undoable, message-thread commit) should wait for the owner. It is P2: Learn is optional and the damage is subtle and recoverable by re-learning, but a misplaced Learn currently has no clean way back.

**Architecture gates.**

- Reset action: reuses ADR-0012's ratified staged-record row and the existing ADAPTIVE semantics ('absent = never learned'), so it is not a gate if routed through the paired helper. A new cross-thread path would be a threading-model change (ADR-0011/ADR-0012).
- Revert-to-previous / undoable Learn: product decision reserved by MODE_AND_ADAPTATION_POLICY.md:164-166; putting it in the undo stack conflicts with ADR-0007 (StateSet = A/B and undo unit) — Accepted-ADR conflict.
- Per-slot learned references (alternative): serialization schema change and ADR-0007 conflict.
- Commit on the message thread (alternative): threading-model change (ADR-0011).

**Dependencies.** [UX-005](findings-ux.md#ux-005) (acknowledged commit, pending state, learned marker); [UX-019](findings-ux.md#ux-019) (shared reference versus per-slot Freeze); [DOC-004](findings-doc-test.md#doc-004) (stale undo-bracketing promises)

**Acceptance criteria.**

- A user-reachable action returns the engine to the never-learned references. After it, getStateInformation writes no ADAPTIVE child and a save/reload restores never-learned (a state test extends the testLearnCommitAndAdaptiveRoundTrip coverage).
- The reset and the session-load path stage the adaptive record through one helper that pairs the mirror and engine stores; THREAD_MODEL.md gains no new row.
- After a stop click with no block processed, the UI shows a pending-commit state until isLearning() drops, then the [UX-005](findings-ux.md#ux-005) acknowledgement.
- USER_MANUAL §4 and §7.4 state that the learned reference is shared by both A/B slots, is not undoable, and how to reset it.
- The deferred Learn-undo decision is recorded as an entry in OPEN_QUESTIONS.md with the policy's two options.
- A/B switch and undo/redo leave ADAPTIVE unchanged; the existing state tests stay green.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/PluginEditor.cpp:621-640 (no undo push, no duck request); e769f33:src/PluginProcessor.h:652-662; e769f33:src/PluginProcessor.cpp:1779-1815 (ADAPTIVE written only when learned) and :1975-2011 (the only site that stages ADAPTIVE: session load; INVARIANT comment at :1984-1989); e769f33:src/dsp/AnabasisEngine.cpp:359-369 and :411-422; e769f33:src/dsp/AdaptiveEngine.h:479-553. Read ADR-0007 :79-88, MODE_AND_ADAPTATION_POLICY.md:158-166, and USER_MANUAL.md:312-317 and :411-417. Searched docs/OPEN_QUESTIONS.md and docs/HANDOVER.md for any recorded decision on Learn undo/reset and found none. Runtime on :134: a commit wrote ADAPTIVE (s02-after-learn.xml) with no visible change and no '*' on the preset name (11-commit-sheet.png), and an empty pass kept the earlier references (s03-after-empty.xml). The no-processing loss case cannot be reproduced in the harness, whose audio always flows.

**Corrections to the candidate claim.** 1. The global scope is a deliberate decision in an Accepted ADR (ADR-0007: learned targets are 'a property of the user's material, not of a slot'). Keeping the commit out of undo is a recorded policy decision with a rationale (a per-slot stack would let an A/B switch resurrect a superseded reference; MODE :161-166). Neither is a defect in itself. The defects are that there is no reset or revert path, that the manual's A/B text ('two complete, independent sound setups', :413) does not mention the exception, and that the commit is not acknowledged.
2. The loss case is narrower than claimed. commitLearn runs at the next block top whether or not the block is audible (AnabasisEngine.cpp:415-422), so the pass is lost only if the host calls no processBlock between the stop click and the save and the session is not saved again after processing resumes. Until the commit the button keeps its accent colour (isLearning() stays true), which is a cue but looks the same as 'still running'. No code in src/ calls updateHostDisplay, so a later commit does not mark the host project modified; that host-side effect was not verified.
3. A bad Learn can be overwritten by a new Learn. What cannot be done is to return to the factory references or to the previous learned pair; only loading a session that has no ADAPTIVE child, or a new instance, clears it.
4. The policy deferred Learn-undo as 'a product decision for the P6 pass with the preset bank' (MODE :164-166). No record of that decision exists in OPEN_QUESTIONS.md or HANDOVER.md.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 2 · severity 3 · discoverability 4 · efficiency 3 · coherence 3 · change risk 3 · complexity 3 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### STATE-010

**Host automation and host-panel edits are not undo steps, so the next in-plugin Undo reverts them together with the user's last edit**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | medium | partially-confirmed | Documentation | The tier and undo model is principled in code but invisible at the point of action | Phase 4 |

**Evidence**

- e769f33:tests/state_tests.cpp:2494-2500 — an ungestured setValueNotifyingHost pushes no step. The single remaining Undo reverts the drag and the automation write together, and Redo re-lands them.
- e769f33:src/PluginProcessor.cpp:221-225 and 265-276 — undo is armed only by message-thread gesture begins. :329-356 — an off-thread end clears its bit and pushes nothing.
- e769f33:src/PluginProcessor.h:194-197 and 208-210 — 'host AUTOMATION — ungestured — folds silently'. Off-thread callbacks 'degrade to the automation path'.
- e769f33:src/PluginProcessor.cpp:608-633 — undo restores the whole slot snapshot, so every ungestured change made after it was captured is reverted. :614-615 — the current state is pushed onto redo first, so Redo recovers it.
- e769f33:docs/DESIGN.md:960 ('gesture-gated undo coalescing with host automation folded silently') and e769f33:docs/architecture/design-decisions/ADR-0005-macro-layer-architecture.md:111-114 — both deliberate.
- Anamorph (read-only) Anamorph@fd78c3b:src/PluginProcessor.cpp:408-411 and 456-475 — non-gesture changes fold into the committed baseline, and undo restores the pre-edit set: the same semantics.
- JUCE pin: juce_audio_plugin_client/*.cpp and *.mm contain no beginChangeGesture call. juce_audio_plugin_client_VST3.cpp:827-834, 976 and 3537 — host edits apply setValueNotifyingHost without a gesture. The gesture sources in src/ are only e769f33:src/gui/PluginEditor.h:230,266 and the editor attachments/ScopedDragNotification (e769f33:src/gui/LookAndFeel.cpp:920).
- e769f33:docs/user/USER_MANUAL.md:161 and 504-515 — neither the undo row nor the 'Automation and sessions' FAQ says how undo treats automation or host-panel edits.
- Runtime: not reproduced. Phase-2 ST-07's 'host-automation change is undoable' used the harness param command, which is a gestured (touch-style) write, not playback automation.

**Current behaviour.** Automation playback and edits made from the host's own parameter controls reach the plugin ungestured. They create no undo step, do not detach managed parameters, and mark the preset modified. The next in-plugin Undo restores the slot as it was before the user's last recorded edit, so it also reverts any values automation or host controls changed since that point. Redo brings them back. Whether and when a host re-writes an automated value after that is host-dependent (not verified).

**Problem.** The undo scope is undocumented for anyone who automates Anabasis or edits it from a control surface or generic panel. An Undo aimed at the last knob move can also move parameters the user changed elsewhere, and host-side edits cannot be undone from the plugin. The design choice itself is standard and sound.

**Root cause.** Undo is a snapshot taken at the first message-thread gesture begin and pushed at the last gesture end (PluginProcessor.cpp:259-356). Writes outside a gesture never mint a step, by design, so that automation cannot flood the history. JUCE delivers host edits without gestures. The documentation describes only what undo covers, not what it folds.

**User impact.** Low. Mastering sessions rarely automate the plugin's own parameters. An unexpected revert is recoverable with Redo, and during playback the host's automation usually takes over again. The main cost is confusion when it happens. *Scope:* Hosted VST3/AU sessions that use automation playback, control surfaces or a generic parameter panel on Anabasis parameters. The Standalone is not affected.

**Proposed improvement.** Keep the semantics: automation must not create undo steps, and host-side edits belong to the host's own undo. Close the documentation gap with one sentence in the USER_MANUAL §3.1 undo row or the Automation FAQ: 'Automation and your DAW's own parameter controls are not recorded; Undo returns the sound to how it was before your last edit in Anabasis, including anything automation changed since; Redo brings it back.' Correct the comments at PluginProcessor.cpp:223-224 and PluginProcessor.h:208-210 so they say host-UI edits arrive ungestured, and keep the off-thread branch as a defensive path. If [STATE-003](findings-state-model.md#state-003)'s diff-based preview lands, a folded change shows in the Undo label at no extra cost.

**Alternatives considered.**

- *Record automation or host edits as undo steps* — Reject. It floods the history during playback and contradicts DESIGN §7, ADR-0005 and the sibling.
- *Rebase the top undo entry on every ungestured write so that Undo reverts only the user's own edit* — Reject for now. It needs a ValueTree merge driven by writes that can arrive on the audio thread (VST3 process queue), which is a threading-model change (ADR-0011, gate) for a low-impact case.
- *Capture off-thread gestures* — Reject. Nothing in the shipped formats produces them, and it would need cross-thread ValueTree copies (threading-model gate).
- *Preserve with no change* — Acceptable for the behaviour. The remaining gap is only documentation and code-comment accuracy.

**Decision: Modify · P3.** The behaviour is deliberate, conventional and the same as the sibling's, so it stays. The only justified action is small: document the fold for users and correct the code comment that attributes off-thread gestures to 'host UI'.

**Dependencies.** [STATE-003](findings-state-model.md#state-003) (the undo preview would make the fold visible); [STATE-007](findings-state-model.md#state-007) (the same gesture-keyed model leaves arrow-key edits unrecorded)

**Acceptance criteria.**

- USER_MANUAL (§3.1 undo row or the Automation FAQ) states that automation and host parameter controls are not recorded, that Undo reverts to before the last in-plugin edit including values they changed since, and that Redo restores them.
- The comments at e769f33:src/PluginProcessor.cpp:223-224 and e769f33:src/PluginProcessor.h:208-210 no longer name 'host UI' as a source of off-thread gestures, or name a reachable source.
- No behavioural change: testUndoIsPerSlotGestureCoalescedAndMaskWide step 2 stays green.

<details><summary>Verification record</summary>

**Method.** Read e769f33 e769f33:tests/state_tests.cpp:2467-2500, e769f33:src/PluginProcessor.cpp:221-356 and 608-648, e769f33:src/PluginProcessor.h:190-217, DESIGN.md:951-961 and ADR-0005:105-115. Grepped the pinned JUCE plugin-client wrappers (build/_deps/juce-src/modules/juce_audio_plugin_client) for beginChangeGesture and read the gesture dispatch (juce_audio_processors_headless/processors/juce_AudioProcessorParameter.cpp:65-108, juce_AudioProcessor.cpp:1477-1488) and the VST3 host-edit path (juce_audio_plugin_client_VST3.cpp:827-834, 964-979, 3535-3538). Compared with Anamorph (read-only) e769f33:src/PluginProcessor.cpp:408-411 and 456-475. Not reproduced at runtime: the harness param/paramtext commands bracket begin/end on the message thread, so an ungestured path would need a harness rebuild.

**Corrections to the candidate claim.** The automation fold is confirmed and deliberate (DESIGN.md:960, ADR-0005:111-114), and the sibling behaves the same way. Redo restores the reverted values, because undo() first pushes saveSlotFromLive() onto redo (:614-615). The 'off-message-thread gesture' half does not happen in the shipped formats: no JUCE 9.0.1 VST3/AU/Standalone client wrapper calls beginChangeGesture, and every gesture source in src/ is editor code on the message thread. Edits from a host's generic panel therefore arrive ungestured (VST3: setValueAndNotifyIfChanged, which calls setValueNotifyingHost), not as off-thread gestures. They behave exactly like automation: no step, no detach, and they are folded into the next Undo. The comment at PluginProcessor.cpp:223-224 ('an off-thread gesture (host UI)') describes a source that JUCE does not produce here.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 1 · severity 2 · discoverability 3 · efficiency 1 · coherence 2 · change risk 1 · complexity 1 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### STATE-011

**A factory (or user) preset apply keeps the frozen trim latch (KI-007 item 1). The behaviour is coherent and should be kept.**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Preserve** | **none** | high | partially-confirmed | Preset/state workflow | The adaptive engine changes the audio from state the user cannot see, keep or reset | — |

**Evidence**

- e769f33:src/PluginProcessor.cpp:1628-1629 — factory apply: replaceDetachMask + liveBaseline = {} only; no adoptFrozenMirror, no trim staging
- e769f33:src/PluginProcessor.cpp:1705-1716 — user-preset apply: same, no frozen-mirror change (symmetric)
- e769f33:src/PluginParameters.cpp:433-436 — isPresetExcludedParam includes pid::freeze
- e769f33:src/dsp/AnabasisEngine.cpp:1219-1225, 1299 — adaptiveEngine.pushFrame(monFrameDry) — features come from the delayed dry INPUT only
- e769f33:docs/user/USER_MANUAL.md:397-402 — 'A preset changes sound parameters only. Deliberately left alone: … Freeze'
- e769f33:docs/KNOWN_ISSUES.md:462-468 — KI-007 item 1, held as a MODE invariant-3 semantics question to settle with KI-006
- runtime: rt/verify-18/c-after-preset.xml (slot 'Transparent Master', FROZEN_TRIMS rel 0.1088 / link −0.0218 / hpf 0.996 / tilt 0.126 — identical to a-before-sr.xml under 'Default'); screenshot session capture `rt/verify-18/04-crop.png`
- probe rt/verify-18/probe/probe.out: dense clicks (12/s) latch link +0.19 → applied 1.00 at 100 % but 0.79 at a 60 % link

**Current behaviour.** With FREEZE on, choosing a factory or user preset changes the sound parameters and keeps Freeze ON. The latched adaptive vector keeps applying around the new preset's values, is serialised with the slot, and is re-injected by later A/B or undo.

**Problem.** None is demonstrated. The finding assumed the vector belongs to the previous preset, but it is a function of the input programme only. Carrying it is exactly what 'Freeze locks the current adaptive state' plus 'a preset leaves Freeze alone' implies. What remains open is the owner's semantics question in KI-007, which must be answered the same way as [STATE-004](findings-state-model.md#state-004).

**Root cause.** Intended by construction: freeze is preset-excluded, and neither preset apply path touches the frozen mirror or the engine vector. Because the features derive from the dry input, the latch is independent of the preset.

**User impact.** Low. A user browsing presets with Freeze on keeps the frozen adaptation for the same programme, which is consistent. The only surprise is that a latched trim that was inert at a range limit (e.g. a positive link trim at 100 %) becomes audible under a preset whose value is not at the limit. The same happens unfrozen and belongs to [VIS-011](findings-visualisation.md#vis-011) and [DSP-008](findings-dsp-tech.md#dsp-008). *Scope:* Every factory and user preset apply while Freeze is ON with a latch.

**Proposed improvement.** Keep the behaviour. When the fine review settles KI-007 item 1, record the rationale (the latch is derived from the programme and independent of the preset, and presets leave Freeze alone per manual §7.3). Extend the item to user presets, and pin the carry with a state test. The answer must match [STATE-004](findings-state-model.md#state-004)'s: Freeze memory survives presets and re-prepares, and changes only when the user turns Freeze off or adopts a slot, session or undo state with a different vector.

**Alternatives considered.**

- *Clear the frozen vector on preset apply (Freeze stays ON)* — Rejected. It would produce the [STATE-004](findings-state-model.md#state-004) incoherence on purpose: FREEZE lit while the audio applies an unmeasured zero vector that nothing will re-slew.
- *Turn Freeze OFF on preset apply* — Rejected. It writes a preset-excluded parameter, contradicts USER_MANUAL §7.3, and adds a hidden parameter change to preset browsing.
- *Keep the carry and add a cue (e.g. the [VIS-011](findings-visualisation.md#vis-011) overlay shows the frozen deltas on the new preset's controls)* — Good. It comes free with [VIS-011](findings-visualisation.md#vis-011) and needs no separate change.

**Decision: Preserve · none.** The carry is coherent with the documented model (Freeze locks programme-derived state; presets leave Freeze alone), and the audio, save and restore are consistent. Changing it would make preset browsing with Freeze on less predictable. The deciding reading is that the vector depends on the programme, not the preset, which the code confirms.

**Architecture gates.**

- None for the proposed docs/test-only outcome. Either rejected alternative (clear or unfreeze on preset) would be a Freeze-semantics change under MODE_AND_ADAPTATION_POLICY Enforcement (Architecture Review Gate + AI Agent Hard Stop) touching ADR-0014

**Dependencies.** [STATE-004](findings-state-model.md#state-004) (KI-007 requires the two answers to agree); [VIS-011](findings-visualisation.md#vis-011) (supplies the only cue)

**Acceptance criteria.**

- KI-007 item 1 is closed with the recorded rationale and covers user presets as well as factory presets
- A state test applies a factory preset and a user preset with Freeze ON and a latched vector, then asserts: Freeze still ON, publishedTrim*() unchanged, the next save's FROZEN_TRIMS equal to the pre-apply vector, and undo restoring the same vector
- The [STATE-004](findings-state-model.md#state-004) decision text and the KI-007 closure state the same Freeze-memory rule

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/PluginProcessor.cpp:1609-1659 (applyFactoryPreset), 1695-1725 (applyPresetFile), 1168-1216 (adoptFrozenMirror, engineFrozenTrimsIfLive), 1236-1276 (save), 1509-1522 (slot restore), plus PluginParameters.cpp:433-436 (freeze is preset-excluded) and USER_MANUAL.md:397-402. Traced what feeds the trims: AnabasisEngine.cpp:1219-1225 and :1299 (delay-aligned DRY input into pushFrame). Runtime on :148: with Freeze ON and a latched vector, I stepped to the next factory preset via '>' (stepped pointer motion) and ran savexml.

**Corrections to the candidate claim.** The mechanism is confirmed: the preset apply leaves liveFrozenTrims and the engine's applied vector untouched, and Freeze stays ON. The runtime save after stepping Default → Transparent Master showed identical FROZEN_TRIMS. The impact hypothesis is not supported. The trim vector is computed only from the DRY input programme and the Learn reference, not from any preset parameter. On the same audio, an unfrozen engine would converge to the same vector under any preset, so it is not 'the previous programme's' adaptation. The audio, the save and later A/B or undo restores all agree on the carried vector (no incoherence). USER_MANUAL §7.3 states that presets deliberately leave Freeze alone. The carry is not factory-specific: applyPresetFile is symmetric (it also leaves the mirror untouched), which KI-007 item 1 does not mention. Remaining preset-dependent effect: trims are deltas around the new preset's values, so a latched +0.19 link trim that is inert at 100 % acts as +19 points at a 60 % preset. That is equally true with Freeze off.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 2 · severity 1 · discoverability 3 · efficiency 1 · coherence 1 · change risk 3 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### STATE-012

**UI preferences (UI scale, tooltips, animations) and the metering standards are per-instance session state with no global preference, so every new instance opens at M with tooltips off**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Defer** | **P3** | high | confirmed | State management | The tier and undo model is principled in code but invisible at the point of action | — |

**Evidence**

- e769f33:src/InternalState.h:103-121 — hard-coded defaults, including uiScale 100 (M), tooltipsOn false, uiAnimations true
- e769f33:src/InternalState.h:139-167 — replaceFrom(): defaults first, then overlay (§4.4 read rule)
- grep src for PropertiesFile|ApplicationProperties|Preferences → 0 hits (no global preference store)
- e769f33:docs/user/USER_MANUAL.md:278 — Settings are 'Session state — saved with your DAW project'
- e769f33:docs/DESIGN.md:604-606 — ⊕ defaults for int_uiScale (100), int_tooltipsOn (off) and int_uiAnimations (on); ratified per e769f33:docs/DESIGN.md:8-9
- e769f33:src/gui/PluginEditor.cpp:1930 and :1937-1940 — the host scale factor is multiplied into the UI scale
- Anamorph@fd78c3b:src/PluginProcessor.cpp:628 and Anamorph@fd78c3b:src/InternalState.h:51 — the sibling uses the same per-session model with tooltips off
- Runtime G-01 and LAY-09 (fresh launches open at M with Tooltips OFF); ST-16 (the values round-trip within a session)
- Own reproduction — session capture `rt/verify-16/02-settings-fresh.png` (fresh: M, Tooltips off), prefs-set.xml (125/1) vs prefs-relaunch.xml (100/0) with the same HOME

**Current behaviour.** Every newly created instance starts at UI Scale M, Tooltips off and Animations on, with the gated integrated standard and AES-17 RMS. A choice made in one instance or project never carries over to the next new instance. Within a project, all these values save and restore with the session as documented.

**Problem.** Per-user ergonomic choices (window size, tooltips, motion) are treated like per-project decisions. A user with a stable preference repeats the same Settings clicks on every new project. The cost is small and there is no evidence of a larger harm.

**Root cause.** By design, all Settings live in ANABASIS_INTERNAL, the session state (DESIGN §4.3, manual §3.5). There is no application-level preference store, in this product or in the sibling.

**User impact.** Two or three extra clicks per new instance for users who prefer L/XL or tooltips on. No wrong output and no lost work. *Scope:* Only the new-instance path. The three UI ergonomics fields are candidates. Oversampling, phase, offline render and the two metering standards should remain session-only, because they define a project's sound or numbers.

**Proposed improvement.** If taken up, keep the constrained version. UI Scale, Tooltips and Animations get a per-user 'default for new instances', written when the user changes them and read only at construction. It is never used inside setDefaults() or replaceFrom(), so a restored session, including an old blob missing a field, still gets the hard-coded §4.4 default and stays deterministic. The metering standards and the processing settings stay session state (Preserve). Document the split in manual §3.5.

**Alternatives considered.**

- *A. Status quo (pure session state)* — Documented, deterministic, family-consistent; costs a few clicks per new instance. Acceptable for v0.1.
- *B. A global store for ALL Settings, including the metering standards and oversampling* — The metering standard and OS factor are deliverable- or project-specific. Carrying them silently into new projects risks wrong readings. Reject.
- *C. Seed new instances from the global store through setDefaults()* — Would change what an absent field restores to: a serialization read-rule (semantic) change, and non-deterministic across machines. Reject.
- *D. Only flip the tooltips default to on* — That is a ⊕ owner decision covered by the G-01 finding, not a persistence change.

**Decision: Defer · P3.** This is a convenience with weak evidence of harm, the documented model is coherent, and the sibling behaves identically. A per-user preference file is a new persistence surface and a product-family choice (ADR-0009 reuse). It waits for the owner's post-v0.1.0 fine review to decide whether Settings stay purely session-scoped, ideally informed by user feedback, and for the G-01 tooltip-default decision, which removes most of the cost on its own.

**Architecture gates.**

- None if implemented as construction-only seeding (the §4.4 read rule and the schema are untouched)
- A new persisted per-user file is a new persistence surface; record it in an ADR (ADR_POLICY) before implementation
- Seeding via setDefaults()/replaceFrom() would be a Serialization Registry semantic change (hard stop). Excluded by the proposal

**Dependencies.** G-01-derived tooltips-default finding (gestures batch); Owner/product-family decision per ADR-0009 (Anamorph has the same model)

**Acceptance criteria.**

- (If taken up) After choosing UI Scale L and Tooltips ON in one instance, a brand-new instance with no setStateInformation opens at L with tooltips on.
- Loading any session restores that session's UI Scale, Tooltips and Animations regardless of the stored per-user defaults, and a blob lacking a field gets the hard-coded default (getState→setState→getState stays byte-identical).
- Oversampling, Phase, Offline Render, Integrated and RMS Reference never take a value from the per-user store.
- USER_MANUAL §3.5 states which Settings are per-project and which are remembered as per-user defaults.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/InternalState.h:103-121 (setDefaults: uiScale 100, tooltipsOn false, uiAnimations true, integratedStd 0, rmsRef 0) and :139-167 (replaceFrom calls setDefaults() and then overlays). Grepped src for PropertiesFile, ApplicationProperties and Preferences: 0 hits. Read USER_MANUAL.md:276-290, DESIGN.md:600-608 (the ⊕ defaults, ratified per DESIGN.md:8-9), the editor's host-scale handling (PluginEditor.cpp:1930, 1937-1940), and Anamorph's InternalState and restore. Reproduced on :146 with an isolated HOME: set UI Scale L and Tooltips ON (savexml showed int_uiScale=125 and int_tooltipsOn=1), quit, relaunched with the same HOME. The new instance had int_uiScale=100 and int_tooltipsOn=0, and no file had been written under HOME. Viewed verify-16/02-settings-fresh.png.

**Corrections to the candidate claim.** (1) 'HiDPI users re-pick L or XL' is overstated. The editor multiplies the host-reported scale factor (setTransform(hostScale * scale), PluginEditor.cpp:1930), so HiDPI legibility where the host reports a scale does not depend on L or XL. Picking L/XL is a size preference. Hosts that report no scale were not tested. (2) Only brand-new instances are affected: a project reopen restores that session's values (ST-16). (3) The tooltips-off default is an owner-ratified ⊕ value (DESIGN.md:605, ratified at :8-9), not an accident. (4) The sibling has the same model: Anamorph restores UI Scale, Tooltips and Animations from the session (Anamorph@fd78c3b:src/PluginProcessor.cpp:628) and also defaults tooltips off (Anamorph@fd78c3b:src/InternalState.h:51), so this is family-consistent. (5) The metering standards being session state is correct: they define what a project's numbers mean.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 3 · severity 1 · discoverability 2 · efficiency 2 · coherence 1 · change risk 2 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### STATE-013

**Slot B starts as the Default patch rather than a copy of A, so the first A/B press compares against an unrelated sound**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Preserve** | **none** | high | confirmed | Documentation | The tier and undo model is principled in code but invisible at the point of action | — |

**Evidence**

- e769f33:src/PluginProcessor.cpp:46-59 — both slots open carrying 'Default'; storedSlot = defaultSlot.createCopy()
- e769f33:src/PluginProcessor.cpp:1731-1760 — resetSlotFieldsToDefaults (no-AB-child blob only) re-seeds storedSlot from defaultSlot
- e769f33:src/PluginProcessor.cpp:1773-1777 — every saved session writes both slots
- Anamorph e769f33:src/PluginProcessor.cpp:51-58 — eager init of both slots to the open state; lazy copy-of-A rejected because the edit leaks into B
- e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:55-56 — A/B 'the same slot semantics'
- e769f33:docs/user/USER_MANUAL.md:411-418 — §7.4 silent on B's initial content
- VER8-2 (fresh instance, A = 'Default *' 34 %, first A/B → 'Default' 0 %): session capture `rt/verify-8/v02-A-edited-knob.png`, session capture `rt/verify-8/v04-after-ab1-top.png`, session capture `rt/verify-8/v04-after-ab1-knob.png`
- ST-07: session capture `rt/state/18b-after-ab-1.png`

**Current behaviour.** A new instance opens with both slots holding the Default preset. Loading a preset or editing affects only the active slot, so the first A/B press after working in A lands on Default until the user has pressed Copy.

**Problem.** A user who expects B to start as 'what I have now' hears a level drop and a different sound on the first compare. The manual does not warn about this.

**Root cause.** This is a deliberate design, not a defect: B is seeded from the pristine defaults slot at construction, matching Anamorph's eager open-state initialisation. The only defect is a documentation gap: B's initial content is not stated.

**User impact.** One surprising compare per instance, which also costs a duck. It is cheap to recover from (Copy, then A/B), and invisible once [UX-011](findings-ux.md#ux-011) shows the inactive slot. *Scope:* The first A/B press on each fresh instance, or after loading a blob with no AB child.

**Proposed improvement.** Keep B seeded to the opening state, and close the documentation gap:
• USER_MANUAL §7.4 gains one sentence: 'A new instance opens with both slots at Default; to compare a variation of your current sound, press Copy first, then A/B.'
• [UX-011](findings-ux.md#ux-011)'s inactive-slot readout ('B: Default' on hover) removes the surprise before the press.

**Alternatives considered.**

- *Seed B lazily from A at the first A/B press* — Rejected. This is the behaviour Anamorph removed: B's content would depend on edit history and on whether the host called getStateInformation early, and edits made before the first visit would leak into B.
- *Auto-Copy A into B on the first A/B press* — Rejected. It is a hidden destructive action, and the first compare would still duck while comparing two identical sounds.
- *Seed B from A on every preset load in A* — Rejected. It silently destroys whatever the user built in B.

**Decision: Preserve · none.** The behaviour is intentional, sibling-consistent, and backed by a recorded rationale in the reference product. Changing it breaks a brand-checklist must-match item for a one-time, cheaply recovered surprise that [UX-011](findings-ux.md#ux-011) plus one manual sentence fully address.

**Architecture gates.**

- Changing the seeding would touch e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:55-56 (A/B 'same slot semantics' with Anamorph) — product-family decision; not proposed
- No hard-stop category touched by the preserved behaviour or the doc sentence

**Dependencies.** [UX-011](findings-ux.md#ux-011) (makes B's Default content visible before switching)

**Acceptance criteria.**

- Constructor and resetSlotFieldsToDefaults continue to seed storedSlot from defaultSlot (existing A/B tier tests unchanged)
- USER_MANUAL §7.4 states B's initial content and the Copy-first workflow
- With [UX-011](findings-ux.md#ux-011) in place, hovering A/B on a fresh edited instance shows 'B: Default'

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/PluginProcessor.cpp:46-59 ('Named BEFORE the default slot is captured so both slots open carrying it'; storedSlot = defaultSlot.createCopy()) and :1731-1760 (resetSlotFieldsToDefaults, which re-seeds storedSlot only for a session blob with no AB child; getStateInformation always writes AB at :1773-1777). Compared with the sibling: Anamorph seeds both slots from the open state at construction and explains why B-as-copy-of-A was rejected (Anamorph e769f33:src/PluginProcessor.cpp:51-58 and :495-512). Reproduced on :138: on a fresh instance, after editing A to 34 % Loudness, the first stepped A/B click shows 'Default' at 0 % (VER8-2).

**Corrections to the candidate claim.** The behaviour is exactly as described, but it is deliberate family semantics, not an oversight. Anamorph's code records the rejected alternative: seeding B lazily from A 'leaks the edit into B, so B never shows the open state', and makes B's content depend on host timing. The brand checklist requires 'the same slot semantics' as the sibling (e769f33:docs/BRAND_CONSISTENCY_CHECKLIST.md:55-56). The :1752 path is reached only by a blob with no AB child, not by ordinary session loads. 'Unprocessed default' overstates it: Default at 0 % still limits to the −0.1 ceiling. The real gap is that USER_MANUAL §7.4 never says what B contains initially.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 3 · severity 1 · discoverability 3 · efficiency 2 · coherence 2 · change risk 3 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### STATE-014

**After a preset loads under LOCK, the name reads as the clean preset although its Ceiling is not the preset's**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Preserve** | **none** | high | confirmed | Preset/state workflow | Save, load, browse and restore change or lose state without saying so | — |

**Evidence**

- e769f33:src/PluginProcessor.cpp:1654 — factory path: presetBaseline = presetShapeFromLive() after the apply
- e769f33:src/PluginProcessor.cpp:1724 — file path: same re-seed
- e769f33:src/PluginProcessor.h:153-170 — dirty = live differs from 'the state the named preset landed'
- e769f33:src/PresetManager.cpp:306-309 — locked ceiling 'still saved and still compared by the dirty marker'
- e769f33:docs/user/USER_MANUAL.md:336-337 — '*' is the 'edited since the preset' mark
- ST-20: [capture](captures/10-lock-toggle.png)
- VER0-1: session capture `rt/verify-0/05-crop.png` — 'Loud Pop' clean, Ceiling -1.00 dB, LOCK on
- VER0-4: session capture `rt/verify-0/09-crop.png` — 'Transparent Master' clean, Ceiling -1.00 dB, lock invisible in Advanced

**Current behaviour.** After any preset apply, the dirty baseline is taken from the post-apply live state. With LOCK on, that state includes the held ceiling, so the name shows without '*'. Any later edit, including a ceiling edit, marks it dirty.

**Problem.** The top bar names a preset whose ceiling is not that preset's value. In Simple, the only cue is the LOCK pill; in Advanced there is none.

**Root cause.** This is deliberate: the '*' measures change since the load landed, and the locked ceiling is part of what landed.

**User impact.** Low. It is an honesty gap about provenance: a user reporting or re-deriving 'I used Loud Pop' has a different ceiling. It does not change the sound or save behaviour. *Scope:* Every preset load with LOCK on and a ceiling that differs from the preset's value.

**Proposed improvement.** Keep the '*' semantics. Close the provenance gap through [UX-001](findings-ux.md#ux-001)'s lock indicator on both Ceiling readouts, optionally with a transient 'Ceiling held' cue at load when the skipped value differed. No '*' change.

**Alternatives considered.**

- *Mark '*' whenever a locked value differs from the preset's* — Rejected. Every locked load would read as edited. The manual's own delivery workflow recommends locking, so the '*' would be lit permanently for exactly the users who lock, and would stop meaning 'you changed something'.
- *A distinct top-bar marker such as 'Loud Pop (ceiling held)'* — Heavier. Top-bar space is already short: long names truncate and drop the ' *' (see new_findings). The cue belongs on the ceiling.
- *Leave as-is with no additional cue* — Acceptable once [UX-001](findings-ux.md#ux-001) lands. The gap only matters while the lock is invisible.

**Decision: Preserve · none.** The behaviour follows the documented definition of '*' and the ADR-0010 lock design. Changing the marker would degrade its main signal. The residual gap belongs to [UX-001](findings-ux.md#ux-001)'s visibility fix.

**Dependencies.** [UX-001](findings-ux.md#ux-001)

**Acceptance criteria.**

- Immediately after a preset load with LOCK on, the name shows without '*'; any subsequent parameter edit, including a ceiling edit, adds '*' within one dirty-poll interval
- After [UX-001](findings-ux.md#ux-001), the held ceiling shows the lock indicator immediately after the load in both views

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/PluginProcessor.cpp:1652-1657 and :1724 (baseline re-seeded from live after each apply), e769f33:src/PluginProcessor.h:153-170 (presetDirty definition) and e769f33:src/PresetManager.cpp:306-309. Viewed [capture](captures/10-lock-toggle.png). Reproduced with LOCK on: 'Loud Pop' clean at Ceiling -1.00 (VER0-1, 05-crop) and 'Transparent Master' clean at -1.00 in Advanced (VER0-4, 09-crop).

**Corrections to the candidate claim.** The cited comment is at PresetManager.cpp:306-309, not :296-303. It means a later ceiling edit is compared, which holds: the '*' still appears on any subsequent edit. By the product's own definition (PluginProcessor.h:153-158 'differs from the state the named preset landed'; USER_MANUAL.md:336-337 'edited since the preset'), a clean name after a locked load is correct, because the user has not edited anything since the load. The claimed meaning 'edited relative to the preset' is a different, stricter reading than the documented one.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 2 · severity 1 · discoverability 2 · efficiency 1 · coherence 2 · change risk 2 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### STATE-015

**A deleted or moved user preset leaves its name in the bar with no menu tick, and ‹ › restart from the edge of the list**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | confirmed | Preset/state workflow | Save, load, browse and restore change or lose state without saying so | Phase 4 |

**Evidence**

- e769f33:src/PresetManager.cpp:459-462 — user identity not in the scanned list → return -1 ('outside the folder, or deleted/renamed on disk — no row')
- e769f33:src/gui/PluginEditor.cpp:455 — here < 0 → start at 0 (›) or total-1 (‹)
- e769f33:src/gui/PluginEditor.cpp:397-401 — comment: 'A known identity that is on no row … answers -1 and the arrows start from the list edge'
- e769f33:docs/architecture/design-decisions/ADR-0022-preset-identity.md:82-91 (Decision 3: known-but-absent identity ticks nothing), :151-152 ('An .anabasis file loaded from outside the preset folder ticks nothing. Correct: it is on no row.')
- e769f33:src/gui/PluginEditor.cpp:972 — tooltips default OFF (a tooltip-only cue would be invisible by default)
- verify-2 repro — label 'Beta' after deletion — session capture `rt/verify-2/08-beta-deleted-crop.png`
- verify-2 repro — menu with no tick — session capture `rt/verify-2/09-menu-orphan-crop.png`
- verify-2 repro — › from orphan → Default — session capture `rt/verify-2/11-after-next-crop.png`
- verify-2 repro — Undo → 'Beta', then ‹ → Gamma (last row) — session capture `rt/verify-2/12-13-strip.png`

**Current behaviour.** When the current preset's identity is a user file that is not in the scanned folder list (deleted, renamed, moved, loaded from elsewhere, or a session from another machine), the top bar keeps showing its name and the menu ticks nothing. › applies the first row (Default) and ‹ applies the last row (the last USER preset, or Lo-Fi Crush if there are none), not the neighbours the file used to have.

**Problem.** The state is legitimate and documented in ADR-0022, but the UI gives no sign of it. The unticked menu and the jump to the list edge look like malfunctions.

**Root cause.** This is ADR-0022 working as designed: resolution returns -1 and the ring starts from the list edge. The editor has no visual or text state for 'the current preset is not in the list'. refreshPresetDisplay renders the name the same way whether or not it resolves to a row.

**User impact.** Mild surprise. One arrow press replaces the sound with Default or the last preset. Undo brings it back and nothing is lost. The user does not learn why the tick disappeared. *Scope:* Only identities that don't resolve: files deleted, renamed or moved on disk, files loaded from outside the preset folder, and sessions opened where the preset file is missing. Rare in everyday use.

**Proposed improvement.** Keep ADR-0022's resolution and the list-edge rule. Make the state readable. When selectedPresetRow returns -1 for a known identity, draw the preset name in the dim text colour. When tooltips are on, set the name's tooltip to "'Beta' is not in the preset list (loaded from elsewhere, renamed or deleted)". Add one sentence to USER_MANUAL §7.1: no row is ticked in this case, and ‹ › start from the first or last preset.

**Alternatives considered.**

- *Step from the orphan's sorted position among the user files (true neighbours) for orphans that were in the folder* — Gives the expected neighbour, but changes arrow semantics that the code attributes to ADR-0022 §Decision 3 (PluginEditor.cpp:448-450), so it needs an ADR amendment and owner review. Not worth that for a rare case.
- *Show a disabled 'Beta (missing)' row in USER* — Could read as a tick on a row, which ADR-0022 Decision 3 rules out. Clutters the list. Rejected.
- *Documentation only* — The smallest step, but the in-app state stays unexplained. Acceptable if the owner prefers not to change the label's look.
- *Leave as-is* — Defensible, since behaviour matches the Accepted ADR and nothing is lost. The cost is unexplained behaviour.

**Decision: Modify · P3.** The claim is confirmed at runtime for the first time. The behaviour is deliberate under an Accepted ADR, recoverable with Undo, and rare. So the justified change is a legibility cue plus documentation, not new arrow semantics. The recommended change touches no gate. Tooltips are off by default, so the visible cue has to be the label styling, not a tooltip alone.

**Architecture gates.**

- Conditional only: the 'true neighbours' alternative would change arrow behaviour that the code attributes to ADR-0022 §Decision 3 (Accepted), so it needs owner review. The recommended legibility-only change touches no gate.

**Dependencies.** [UX-017](findings-ux.md#ux-017) (the same menu/ring readability model; do both together)

**Acceptance criteria.**

- Load a user preset, then delete its file on disk. Within one display refresh the top-bar name is drawn in the dim text colour, and the menu shows no tick, as now.
- With tooltips enabled, hovering the name shows the not-in-list explanation. With tooltips off, nothing else changes.
- Loading any preset that resolves to a row restores normal label styling.
- A file loaded via Load Preset… from outside the preset folder shows the same dimmed state.
- USER_MANUAL §7.1 states that no row is ticked and ‹ › start from the first or last preset when the current preset is not in the list.
- ADR-0022 fallback tests (testPresetIdentityAcrossRestore, testPresetIdentitySharedName) pass unchanged.

<details><summary>Verification record</summary>

**Method.** Code read at e769f33: e769f33:src/PresetManager.cpp:430-475 (the user-file branch returns -1 at :462), e769f33:src/gui/PluginEditor.cpp:397-405 and :455 (the edge rule), :2218-2228 (menu tick), :2144-2171 (the label shows currentPresetName regardless of resolution), ADR-0022:82-91 and :151-157. Reproduced at runtime, which the claim had not done, on :132 with stepped motion. I saved Alpha (0 %), Beta (30 %) and Gamma (60 %) and loaded Beta (07-beta-loaded-crop.png), then deleted Beta.anabasis on disk. The label still read 'Beta' and the menu had no tick (08-beta-deleted-crop.png, 09-menu-orphan-crop.png). › then applied 'Default' (Loudness 0 %) rather than the neighbour Gamma (11-after-next-crop.png). Undo restored the orphaned 'Beta'. ‹ then applied 'Gamma', the last row, rather than the neighbour Alpha (12-13-strip.png).

**Corrections to the candidate claim.** (1) Anchor: :452-458 is the comment block; the -1 return is at e769f33:src/PresetManager.cpp:462. (2) ‹ lands on the last row of the ring. When any user presets exist that is the last USER preset (Gamma here); Lo-Fi Crush only when the user folder is empty. (3) The same state arises, probably more often, from Load Preset… on a file outside the folder, and from opening a session on a machine that lacks the preset (ADR-0022:151-157). (4) Keeping the name is correct, not stale: the parameters did come from that preset and are restored bit-identically; only the row is gone. (5) An accidental arrow press is recoverable with Undo (verified).

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 1 · severity 1 · discoverability 3 · efficiency 1 · coherence 2 · change risk 2 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### STATE-016

**The Standalone persists its full state across launches and offers Save, Load and an irreversible Reset in its Options menu. The manual says it has 'no session to save into'**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Documentation | User and design documents contradict the shipped product | Phase 4 |

**Evidence**

- JUCE 9.0.1 (fetched) juce_StandaloneFilterWindow.h:366-384: savePluginState/reloadPluginState write and read 'filterState'; :119-123: init() reloads it; :782-786: closeButtonPressed saves it; :767-780: resetToDefaultState deletes and recreates the plugin and removes filterState; :833-841: Options menu items
- JUCE 9.0.1 juce_audio_plugin_client_Standalone.cpp:71-80: settings file under ~/.config on Linux; :148-163: saved on shutdown and on systemRequestedQuit
- e769f33:docs/user/USER_MANUAL.md:122: 'There is no host, so there is no automation and no session to save into'
- e769f33:src/PluginProcessor.cpp:508-512: presetBaseline is invalidated on every setStateInformation and is not serialized, so presetDirty() is false after a restore (the E09 mechanism)
- Runtime V13-3: session capture `rt/verify-13/05-crop.png` (Loudness 36%, 'Default *') and session capture `rt/verify-13/06-adv-on.png` (ADV on before close)
- Runtime V13-3, saved file decoded: rt/verify-13/filterState.bin (advancedMode=1, loudness 36.0, int_tooltipsOn=1, AB slots)
- Runtime V13-3, relaunch: session capture `rt/verify-13/07-relaunch.png` (ADV and values restored) and session capture `rt/verify-13/08-crop.png` (Loudness 36%, label 'Default' with no '*')
- Runtime V13-4: session capture `rt/verify-13/09-crop.png` (Options menu), session capture `rt/verify-13/10-crop.png` (after Reset: Loudness 0%, no confirmation), session capture `rt/verify-13/11-crop.png` (Undo inert after Reset)
- Runtime E01: session capture `rt/edges/02-standalone-options-menu.png`

**Current behaviour.** The Standalone silently saves the entire plugin state (both A/B slots, view mode, every Settings item) plus the audio device and mute flag when it quits, and restores them at launch. The Options menu offers Save current state… and Load a saved state… (raw state files with no extension filter) and Reset to default state (immediate, unconfirmed, cannot be undone). After a relaunch the preset label shows the slot's preset name without the modified marker, even when the restored sound differs from that preset.

**Problem.** The manual says the opposite ('no session to save into'). Users therefore do not know that their last state returns, where it is stored, or that a one-click wrapper menu item erases everything irreversibly. Two unrelated save systems (wrapper state files and plugin presets) sit side by side with no explanation. The restored label misstates what is loaded.

**Root cause.** Stock JUCE StandalonePluginHolder and StandaloneFilterWindow behaviour, which the product documentation never describes, combined with the plugin's deliberate non-serialisation of the preset baseline (PluginProcessor.cpp:508-512).

**User impact.** There is no data loss on normal quit. A user can lose a whole A/B comparison and all preferences with one unconfirmed menu click. A user can confuse a state file with a preset, which Load Preset… does not accept. A user reopening the app is told the unmodified 'Default' is loaded while a different sound is active. *Scope:* Standalone format only, on all desktop OSes. It affects every launch after the first (automatic restore) and the Options menu. Plugin formats are unaffected.

**Proposed improvement.** Keep the persistence. Correct USER_MANUAL §2.5 to say: 'The Standalone remembers its last state (both A/B slots, view, Settings) and its audio device between launches, in ~/.config/Anabasis.settings (Linux), ~/Library/Application Support/Anabasis.settings (macOS) or %APPDATA%\Anabasis\Anabasis.settings (Windows). Options → Save current state…/Load a saved state… store a whole-plugin snapshot file, which is not a preset; use Save Preset… to share settings with the plug-in versions. Options → Reset to default state takes effect immediately and cannot be undone.' Add a Level-5 checklist item: quit and relaunch restores A/B and Settings. The misleading label after a restore is fixed with the E09 finding, not here.

**Alternatives considered.**

- *Disable persistence to match the manual* — Rejected. It would destroy the user's work on every quit, and it needs a custom Standalone app (build-system gated).
- *Custom Standalone app that adds a confirmation to Reset, gives state files an extension, or hides the state menu* — Deferred. Build-system gated and a maintained wrapper fork; revisit if the Standalone becomes a first-class product.
- *Leave the docs unchanged* — Rejected. The docs contradict verified behaviour, and DOCUMENTATION_LIFECYCLE_POLICY requires syncing them.

**Decision: Modify · P2.** The behaviour is useful and should stay, but the documentation is wrong and hides one destructive action. A documentation correction removes the contradiction at no risk. The wrapper-level safeguards are disproportionate for a format the brief calls optional.

**Architecture gates.**

- None for the documentation fix; serialization schema is untouched
- The deferred wrapper alternatives would be a build-system change (custom Standalone app) and need human review

**Dependencies.** [UX-023](findings-ux.md#ux-023); E09 (preset label and modified marker after a state load; the restore at launch triggers it automatically)

**Acceptance criteria.**

- USER_MANUAL §2.5 no longer says 'no session to save into'
- USER_MANUAL §2.5 states that the Standalone restores its last state and audio device at launch, and names the settings file per OS
- USER_MANUAL §2.5 distinguishes Options → Save/Load state files from Save Preset…/Load Preset…
- USER_MANUAL §2.5 warns that Reset to default state is immediate and cannot be undone
- A Level-5 checklist item records a quit/relaunch round trip that restores both A/B slots and Settings

<details><summary>Verification record</summary>

**Method.** Code: in JUCE 9.0.1 (fetched), savePluginState and reloadPluginState (juce_StandaloneFilterWindow.h:366-384) are called from init (:119-123), closeButtonPressed (:782-786) and systemRequestedQuit (juce_audio_plugin_client_Standalone.cpp:156-163). The settings file is ~/.config/Anabasis.settings on Linux (:71-80). e769f33 has no override. Runtime on my own display :143 (stepped input), fresh HOME: dragged Loudness to 36% (label 'Default *'), turned Tooltips on and ADV on, then closed with the window close button. That created ~/.config/Anabasis.settings with filterState, windowX/Y, audioSetup and shouldMuteInput. Decoding filterState gave an 8705-byte blob with advancedMode=1, loudness=36.0, int_tooltipsOn=1 and both AB SLOTs (presetName 'Default'). On relaunch, ADV, the 36% macro-derived values and Loudness 36% were restored, and the label read 'Default' with no '*'. Options → Reset to default state then set Loudness to 0% at once with no confirmation, and Undo did nothing afterwards. Binary caveat as in [UX-023](findings-ux.md#ux-023): built 2026-09-08, and no state or editor code has changed since (git log).

**Corrections to the candidate claim.** 'Probably persists' becomes 'does persist' (verified on Linux; the macOS and Windows locations are from reading JUCE's PropertiesFile: ~/Library/Application Support/Anabasis.settings and %APPDATA%\Anabasis\Anabasis.settings). The persistence itself does not harm the user; it prevents loss on quit. The harm is: (1) docs drift; (2) an unconfirmed, un-undoable 'Reset to default state' that wipes both A/B slots, all Settings and the undo history; (3) 'Save current state...' writes a raw, extensionless state blob (getFilePatterns("") gives no filter, :168-173), a second save system beside the plugin's .anabasis presets; (4) after relaunch the preset label claims an unmodified 'Default' for a modified sound, which is E09's baseline-not-serialized behaviour triggered automatically at every launch.

</details>

<sub>Verifier scores (1-5): impact 2 · frequency 2 · severity 3 · discoverability 3 · efficiency 2 · coherence 3 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### STATE-017

**A saved session can embed an absolute filesystem path, including the username, for an out-of-folder user preset, and the inactive slot re-emits it**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P3** | high | confirmed | Documentation | User and design documents contradict the shipped product | Phase 4 |

**Evidence**

- e769f33:src/PresetManager.cpp:375-400 — userFile: bare name only when getParentDirectory() == userPresetDirectory() and the name is not absolute-path-like; otherwise s.file.getFullPathName()
- e769f33:src/PluginProcessor.cpp:1774-1776 — the inactive slot is written verbatim from storedSlot on every save
- e769f33:docs/architecture/SERIALIZATION_REGISTRY.md:147-165 — 'DISCLOSURE OBLIGATION — a saved session can contain an absolute filesystem path … typically contains the user's account name'; the inactive slot re-emits it 'until that slot is overwritten'; no user-facing privacy document exists yet (C8 / OQ-002)
- e769f33:docs/architecture/design-decisions/ADR-0022-preset-identity.md:115-122 — Decision 8, wire form: everything outside the folder stores its absolute path; decode(encode(s)) == s is the invariant
- e769f33:docs/user/USER_MANUAL.md — no statement that a project can contain a preset file path
- Own reproduction — session capture `rt/verify-16/22-chooser.png`, 23-after-extload.png; s4.xml (active slot presetUserFile = absolute path under home/Downloads); s5.xml (after switching to B, slot A still carries it); s3.xml (in-folder preset stores the bare name)

**Current behaviour.** If a user preset is applied from outside the preset folder (via Load Preset…, from a sub-folder, or with a '~'-prefixed name), the slot's identity is saved as the file's absolute path. That path usually contains the OS account name and folder layout. It stays in the inactive A/B slot and is re-saved with every project save until that slot receives a new sound.

**Problem.** A shared project file can disclose the author's username and directory structure without the user having any way to know. The repository has recorded the obligation to disclose this, but has not yet disclosed it to users.

**Root cause.** ADR-0022 identity must resolve out-of-folder files exactly (decode(encode(s)) == s), so the wire form stores the full path. User-facing disclosure is blocked on owner-supplied wording (C8).

**User impact.** A low-severity privacy leak when projects are sent to clients or collaborators. There is no functional impact: identity resolution works on the author's machine and safely resolves to 'no row' elsewhere. *Scope:* Only presetSource == 'user' with an out-of-folder, nested or path-like file. At most two references per session (one per slot); factory presets store an id only.

**Proposed improvement.** Keep the ADR-0022 encoding. Fulfil the recorded disclosure obligation with one owner-worded line in the user manual (§7.3/§7.4) or the future privacy note, drawn from SERIALIZATION_REGISTRY:147-165. It should say that loading a preset from outside the preset folder stores that file's full path in the DAW project, that it stays in the other A/B slot until that slot is overwritten, and that saving the preset into the preset folder avoids it. Optionally, as a later UX refinement, 'Load Preset…' could offer to import the file into the preset folder, which also makes the project portable across machines.

**Alternatives considered.**

- *A. Store only the file name (or a hash) for out-of-folder files* — Breaks exact resolution and the decode(encode(s)) == s invariant: a same-named file could then be ticked as the wrong row, the defect Anamorph fixed. It is a serialization semantic change and conflicts with ADR-0022 Decision 8. Reject.
- *B. Clear the stored slot's identity when switching away* — Loses the inactive slot's indicator identity that ADR-0022 exists to carry, and is a semantic change. Reject.
- *C. Import-on-load into the preset folder* — No schema change, and it improves portability. But it adds a file-copy side effect and a new dialog step, so it is a product decision. Optional follow-up.
- *D. Do nothing* — Leaves the repository's own recorded disclosure obligation unmet. Reject as the end state.

**Decision: Modify · P3.** The mechanism is an Accepted, deliberate design with sound reasons, and it is family-consistent. The obvious 'fix' (changing the encoding) would re-open a resolved identity defect and hit a hard-stop gate. The proportionate change is the disclosure the repository has already committed to, with wording supplied by the owner.

**Architecture gates.**

- None for the recommended documentation-only change
- Alternatives A and B would be a Serialization Registry semantic change (hard stop) and would conflict with Accepted ADR-0022 Decision 8 (wire form)

**Dependencies.** Owner-supplied wording (DEVELOPMENT_BRIEF C8; SERIALIZATION_REGISTRY cites C8 / OQ-002 for the missing privacy document)

**Acceptance criteria.**

- USER_MANUAL (or a published privacy note) states that a project can contain the full path of a preset loaded from outside the preset folder, and that the other A/B slot keeps it until overwritten, in owner-approved wording.
- SERIALIZATION_REGISTRY's DISCLOSURE OBLIGATION paragraph links to the user-facing statement.
- encodeSelection output is unchanged: testPresetIdentityAcrossRestore passes unmodified, and an in-folder preset still stores only its file name.

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/PresetManager.cpp:368-400 (encodeSelection: a bare name only for a direct child of userPresetDirectory() with a non-path-like name, otherwise getFullPathName()), SERIALIZATION_REGISTRY.md:118-165 (identity trio plus the DISCLOSURE OBLIGATION), and ADR-0022 Decision 8 (ADR-0022:115-122). Reproduced on :146 with stepped motion. Saved preset 'SharedMix' through the UI, copied it to $HOME/Downloads/ClientPreset.anabasis, and loaded it through Preset menu → Load Preset… (JUCE chooser, Open). savexml showed presetUserFile="/tmp/.../verify-16/home/Downloads/ClientPreset.anabasis" (s4.xml). Switched A/B to B and saved again: the inactive slot A still carried the absolute path (s5.xml, active=1). An in-folder preset stored only 'SharedMix.anabasis' (s3.xml).

**Corrections to the candidate claim.** (1) Runtime-confirmed here; the original claim was code-only. (2) This is the documented, Accepted ADR-0022 Decision 8 behaviour. The repository records it as a disclosure obligation that has not been fulfilled user-side, since no privacy or user doc mentions it. (3) The sibling uses the same encoding (it is a port of Anamorph's amended ADR-0024). (4) The exposure needs a deliberate 'Load Preset…' from outside the preset folder, or a sub-folder or '~'-prefixed name. Saving through the UI always writes into the folder and stores a bare name. (5) Host project files commonly reference media by path as well, so the extra exposure is small. This is general context, not verified in this repo.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 1 · severity 2 · discoverability 4 · efficiency 1 · coherence 2 · change risk 1 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### STATE-018

**Non-parameter state changes never tell the host the project changed: Settings rows, LOCK, Copy and a Learn commit call no updateHostDisplay, so a host that tracks edits through parameters may close without a save prompt**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Investigate further** | **P2** | medium | recorded at triage | Preset/state workflow | Save, load, browse and restore change or lose state without saying so | Phase 0 |

**Evidence**

- git grep -E 'updateHostDisplay|ChangeDetails|setDirty|nonParameterStateChanged' e769f33 -- src tests: no sender. e769f33:src/PluginProcessor.h:499 is the processor's own no-op audioProcessorChanged override
- e769f33:src/InternalState.h:27-41 — the Settings rows, LOCK, UI prefs, GR|SPEC and the metering standards are ANABASIS_INTERNAL ValueTree properties, not host parameters
- e769f33:src/InternalState.h:259-264 — valueTreePropertyChanged only calls syncAtomics() and the latency callback for oversample/osPhase/offlineQuality
- e769f33:src/PluginProcessor.cpp:391-430 — copySlotToOther pushes onto undoStacks[other] and assigns storedSlot; no parameter write, so the host sees nothing
- e769f33:src/PluginProcessor.h:654, :662 — startLearn()/stopLearn() post engine requests; the learned reference is global session state ([DOC-010](findings-doc-test.md#doc-010) evidence, ADR-0007) and is committed inside the engine
- JUCE 9.0.1 build/_deps/juce-src/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp:1560-1561, :1623 — nonParameterStateChanged → dirty flag → IComponentHandler2::setDirty; juce_audio_plugin_client_AU_1.mm:2047 PropertyChanged(kAudioUnitProperty_ClassInfo); juce_audio_plugin_client_AAX.cpp:1294; juce_audio_plugin_client_LV2.cpp:846
- Anamorph@fd78c3b:src — no updateHostDisplay call (family-wide)
- [STATE-009](findings-state-model.md#state-009) verification: 'No code in src/ calls updateHostDisplay, so a later commit does not mark the host project modified; that host-side effect was not verified.'

**Current behaviour.** Parameter edits reach the host through setValueNotifyingHost: knobs, BYPASS, MATCH/DELTA, FREEZE, ADV, and preset and A/B applies that land values. Everything else the session stores changes silently: the Settings rows (Oversampling, Phase, Offline Render, Integrated, RMS Reference, UI Scale, Tooltips, UI Animations), LOCK, the GR|SPEC mode, Copy (which rewrites only the inactive slot) and the learned reference a Learn pass commits. None of these calls updateHostDisplay(ChangeDetails().withNonParameterStateChanged(true)). An Oversampling, Phase or Offline Render change reaches the host only as a latency change.

**Problem.** Some hosts decide 'project modified' from parameter traffic or from the plugin's dirty signal. Those hosts get no signal for these edits. If the user's last action before closing was a Learn pass, a Copy, an Oversampling or Offline Render choice, or engaging LOCK, the host may close without asking to save. The next open then silently restores the older state: a different render quality, a missing learned reference, or a lost A/B setup.

**Root cause.** The processor never uses JUCE's non-parameter-state notification. InternalState's property listener was written only to keep audio-thread mirrors and latency in sync. Copy and the Learn commit are processor- and engine-internal writes. The sibling has the same gap, so it was inherited rather than decided.

**User impact.** Work loss that stays invisible until the project is reopened: a lost Learn, which cannot be recreated without replaying the programme ([STATE-009](findings-state-model.md#state-009)); a lost Copy or A/B setup; or Offline Render or Oversampling reverting, so the next bounce differs from what was auditioned. It happens only when no parameter changed after the non-parameter edit and the host relies on such signals. How often that is true per host is unknown.

**Proposed improvement.** Investigate first, then make a small message-thread change.
1. Real-host check, as part of [TEST-002](findings-doc-test.md#test-002)'s Level-5 pass. In REAPER (VST3), Ableton Live (VST3) and Logic (AU): save a project, then make only one of these changes: Oversampling 4x, LOCK on, Copy, or a completed Learn. Close the project and record whether the host asks to save. Also record whether the latency restart from an Oversampling change alone marks the project modified.
2. If any mainstream host does not prompt: call updateHostDisplay (juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged (true)) on the message thread from three places:
   a. InternalState::valueTreePropertyChanged, for the session-meaningful rows (Oversampling, Phase, Offline Render, LOCK, Integrated, RMS Reference). Whether the UI-only rows (UI Scale, Tooltips, Animations, GR|SPEC) also notify is an owner decision. Suppress the call while replaceFrom, setStateInformation or a preset apply runs.
   b. copySlotToOther, when the Copy changed the destination.
   c. The Learn commit, detected on the message thread from state the editor or processor already reads. Add no new audio→GUI path.
3. Record the rule in THREAD_MODEL and SERIALIZATION_REGISTRY, and pass it to the sibling as a family note.

**Alternatives considered.**

- Document 'save after Learn or Settings changes' in the manual and change no code. Cheapest, but it relies on users reading it; acceptable only if the host check shows every target host already prompts.
- Notify for every InternalState property, including the UI preferences. Simplest code, but a UI Scale or Tooltips change would then mark projects modified, which may feel noisy. Owner choice.
- Bump a hidden dummy parameter so the host sees a change. Rejected: it adds a parameter (parameter-surface change, hard stop) and pollutes automation.
- Notify from the audio thread at the Learn commit. Rejected: it adds a cross-thread path (threading-model gate); detecting the commit on the message thread avoids that.

**Decision: Investigate further · P2.** The code gap is certain: no notification is sent anywhere, and JUCE's mechanism is unused. The consequence the user sees depends on host behaviour nobody has measured: some hosts snapshot plugin state on every save and prompt on any change, others rely on parameter traffic or setDirty. The evidence needed is the per-host save-prompt check in step 1. If a mainstream host fails to prompt, the fix is a small message-thread change with no hard-stop gate, provided the Learn detection stays on the message thread.

**Architecture gates.**

- None if the notification is issued on the message thread only. Detecting the Learn commit through a new audio→message published flag would be a threading-model change (ARCHITECTURE_REVIEW_GATE); avoid it or name it at the gate.

**Dependencies.** [STATE-009](findings-state-model.md#state-009) (a Learn commit cannot be undone or restored, which raises the cost of losing it); [TEST-002](findings-doc-test.md#test-002) (the Level-5 real-host pass where the save-prompt check belongs); [STATE-003](findings-state-model.md#state-003) (the same non-parameter set is also outside undo)

**Acceptance criteria.**

- A recorded host matrix (at minimum REAPER VST3, Live VST3 and Logic AU) states, for each of an Oversampling change, LOCK, Copy and a completed Learn made after a save, whether the host prompts on close.
- If the fix lands: after a save, changing only Oversampling (or LOCK, or Copy, or completing a Learn) produces IComponentHandler2::setDirty in a VST3 test host and the ClassInfo property change in AU.
- setStateInformation, preset applies, A/B switches and undo/redo issue no extra non-parameter notification (a state test counts callbacks on a registered AudioProcessorListener).
- No new audio-thread → message-thread path is added; THREAD_MODEL is unchanged or amended through the gate.
- USER_MANUAL or the FAQ states which Settings changes mark the project modified, following the owner's choice on the UI-only rows.

<details><summary>Verification record</summary>

What I re-checked at e769f33:
- `git grep -E 'updateHostDisplay|ChangeDetails|setDirty|nonParameterStateChanged' e769f33 -- src tests` finds no sender. The only hit is the processor's own no-op audioProcessorChanged listener override (e769f33:src/PluginProcessor.h:499).
- InternalState.h:27-41: Oversampling, Phase, Offline Render, LOCK, UI Scale, Tooltips, Animations, GR|SPEC, Integrated and RMS Reference are ValueTree properties, not parameters. Its only listener (:259-264) re-syncs the atomic mirrors and fires the latency callback.
- PluginProcessor.cpp:391-430: copySlotToOther rewrites storedSlot and the other slot's undo stack. No parameter moves.
- The pinned JUCE 9.0.1 wrappers have the mechanism, unused: VST3 juce_audio_plugin_client_VST3.cpp:1560-1561 and :1623 (nonParameterStateChanged → pluginShouldBeMarkedDirtyFlag → IComponentHandler2 setDirty), AU_1.mm:2047 (PropertyChanged ClassInfo), AAX.cpp:1294, LV2.cpp:846.
- Anamorph@fd78c3b:src has no updateHostDisplay either, so the gap is family-wide.
The discovering verifier (learn batch) found the same grep result. [STATE-009](findings-state-model.md#state-009)'s verifier recorded the Learn-commit case as a host effect it did not verify. No DAW was exercised: whether a given host prompts to save after only a non-parameter change is unmeasured.

</details>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

## MODEL — Operation model

### MODEL-001

**Any macro gesture, including one wheel notch, a typed value or a press that moves nothing, re-engages every detached parameter across all three axes and discards Advanced hand edits with only a vanishing 10 px dot as notice**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | high | confirmed | Interaction model | The macro layer overwrites hand edits and automation with almost no notice | Phase 4 |

**Evidence**

- e769f33:src/PluginProcessor.cpp:285-308 — a gesture-begin on loudness/character/tone sets pendingReengage, calls armMapping() and drains
- e769f33:src/PluginProcessor.cpp:718-719 — the drain clears the WHOLE liveDetachMask, with no per-axis scoping
- e769f33:src/MacroEngine.cpp:225-244 — applyMapping re-lands all nine from (l,c,t); :246-257 setParam skips only detached ids and no-op writes
- e769f33:src/PluginProcessor.cpp:262-276,339-354 — the pre-state is snapshotted at the first gesture-begin and one undo step is pushed at gesture-end, so the re-engage is undoable
- build/_deps/juce-src/modules/juce_gui_basics/widgets/juce_Slider.cpp:1164 (wheel) and :451 (typed commit) — both inside ScopedDragNotification, so both are host gestures (JUCE 9.0.1 pinned, CMakeLists.txt:76-82)
- e769f33:src/gui/LookAndFeel.cpp:897-919 — the value-box press was deliberately made non-re-engaging ('pressing on a numeric readout is not that notice'), while the knob press is kept as 'a genuine macro grab'
- e769f33:src/gui/PluginEditor.cpp:1797-1803 — the macro knobs are Simple-only, so the re-engaged knobs are never on screen when it happens
- e769f33:docs/architecture/design-decisions/ADR-0005-macro-layer-architecture.md:131-133 (decision 6) and e769f33:docs/policies/MODE_AND_ADAPTATION_POLICY.md:82-84 (invariant 3) — the contract
- e769f33:docs/user/USER_MANUAL.md:328-333 — 'The next time you move a macro knob, detached parameters re-engage'
- e769f33:docs/KNOWN_ISSUES.md:579-590 — KI-007 item 8: a press that moves nothing re-lands the curve
- Runtime verify-14 (display :144): Tone wheel notch → Threshold -14.3→-10.0 dB, Limiter Gain 5.9→7.8 dB, dot gone — session capture `rt/verify-14/04-05-wheel-sheet.png`
- Runtime verify-14: Undo restores both values and the dot — session capture `rt/verify-14/06-crop.png`
- Runtime verify-14: a zero-movement Character press re-lands both — session capture `rt/verify-14/07-crop.png`
- Runtime G-14: typed Loudness 50 and a 30 px Loudness drag each re-landed Threshold — session capture `rt/gestures/27-simple-sheet.png`, [capture](captures/11-detach-badges.png)
- *Added from another verifier's note:* Add a trigger that [MODEL-001](findings-state-model.md#model-001) is missing and the code explicitly forbids: opening the inline value editor on a macro readout and dismissing it WITHOUT typing is a macro gesture. The editor is pre-filled by rawEditText with the unit stripped (e769f33:src/gui/LookAndFeel.cpp:796-817, :931), so '52.9' never equals the label text '52.9 %'. On focus loss, juce::Label::updateFromTextEditorContents (juce_Label.cpp:255-273) therefore fires textWasChanged. Slider::Pimpl::textChanged (juce_Slider.cpp:445-452) parses the display-rounded value; whenever the current value is off the display grid (after any wheel, drag or automation) that value differs, and it opens a ScopedDragNotification. The result is macro gesture-begin and a whole-mask re-engage. Runtime session capture `rt/verify-10/09-sheet.png`: Loudness 0.5293→0.5290, Comp Threshold -25.0→-10.6 dB, edited dot gone. This contradicts LookAndFeel.cpp:898-918 ('pressing on a numeric readout is not that notice'). On non-macro parameters the same path silently rounds the value and pushes an undo step (link [STATE-007](findings-state-model.md#state-007) and [UI-002](findings-ui.md#ui-002)). Candidate fix with no gate: in ValueBox, skip the commit when the editor text still equals the pre-fill, or when the parsed value equals the display-rounded current value. I re-checked the JUCE and LookAndFeel code paths.
- *Added from another verifier's note:* Sharper trigger for [MODEL-001](findings-state-model.md#model-001). A click made only to give a macro knob keyboard focus counts as a 'genuine macro grab' (e769f33:src/gui/LookAndFeel.cpp:916-918) and re-engages everything (e769f33:src/PluginProcessor.cpp:286-310). Runtime verify-17 (:147): Comp Threshold was detached at -16 dB. Click Tone, then Up×2, and Threshold goes to -12.0 dB. Reach Tone by Shift+Tab from Ceiling, then Up×2, and Threshold stays at -16 dB (session capture `rt/verify-17/18-simple-after-tone-arrows-crop.png`, 21-after-tone-arrows-crop.png). So whether arrow keys discard Advanced edits depends on how focus was obtained (see [STATE-007](findings-state-model.md#state-007)). The only safe route is Tab navigation while focus is invisible ([INPUT-003](findings-input.md#input-003)). Add a click-then-arrow case to [MODEL-001](findings-state-model.md#model-001)'s acceptance. Note that [MODEL-001](findings-state-model.md#model-001)(c), re-engaging on the first value change rather than on press, would also fix this.

**Current behaviour.** Detach works in Advanced: a gestured edit to one of the nine managed parameters sets its bit, and the mapper skips it. On the first gesture-begin on ANY macro, the whole mask is cleared and the curve is re-landed on all nine managed parameters. The macros exist only in Simple. The trigger can be a knob press with no movement, a single wheel notch, a typed commit, a drag, or a double-click or Alt-click reset. The axis does not matter: Tone drives only eqTilt/colourTone but still re-lands Limiter Gain and Threshold. The only visible change is the 10 px edited dot disappearing, and the re-landed knobs are not on screen. One Undo reverts it.

**Problem.** The contract treats any macro gesture as the user 'explicitly choosing the macro over their edits'. The evidence shows three cases where that premise does not hold. (1) A Tone or Character touch re-lands parameters that knob does not drive: a +1.9 dB Limiter Gain jump from a one-notch Tone nudge. (2) A wheel notch or a click that moves nothing counts as the choice. (3) No feedback names what was discarded. The product itself already decided that a value-box press is 'not that notice' (LookAndFeel.cpp:897-919), but the knob-face press, wheel and cross-axis cases were left in.

**Root cause.** Re-engage is keyed on the macro gesture-BEGIN (PluginProcessor.cpp:285-308) and scoped to the whole mask (:718-719), not to the managed parameters that macro drives (the per-axis split is recorded in ADR-0005:249: seven for loudness, two for tone, one for character). JUCE turns a wheel notch and a typed commit into full gestures. The editor gives no transient feedback when a re-engage actually changed values. The scope and trigger are what ADR-0005 decision 6 and MODE invariant 3 specify, so narrowing them is a macro-layer contract change.

**User impact.** A user fine-tunes Limiter Gain or Threshold in Advanced, switches to Simple, and nudges Tone or scrolls over a knob. Their hand-set values silently snap back to the curve: +1.9 dB of push and a 4.3 dB threshold move in the reproduction. They are judging and possibly bouncing a sound they did not choose. Recovery is one Undo only if they notice the 10 px dot vanish. After further edits it means walking the undo stack back or re-tuning by ear. *Scope:* Every session that uses both layers: Advanced fine-tuning followed by any Simple macro interaction. It hits all nine managed parameters, both A/B slots, and every input path to a macro knob except a value-box click.

**Proposed improvement.** Constrained change, no gate: (a) Make the re-engage observable when it happens. If a macro gesture actually re-lands ≥1 detached parameter, show a transient, non-modal notice next to that macro for about 4 s. It names how many knobs were returned to the macro (ideally which) and offers a one-click Undo equivalent to the toolbar Undo. It does not interrupt the drag. The wording is owner-supplied (C8). (b) Correct USER_MANUAL §5 to say exactly which inputs re-engage (press, drag, wheel, typed value, reset; not a value-box click), that ANY macro re-engages ALL detached parameters, and that Undo reverts it. (c) Take the contract question to the Architecture Review Gate with this runtime evidence: should a Tone or Character gesture re-engage only the parameters it drives (axis-scoped re-engage), and should re-engage wait for the first value change rather than the press?

**Alternatives considered.**

- *Leave as-is (contract-conforming, undoable, documented in the manual)* — Rejected as the end state. Runtime shows the only notice is a 10 px dot vanishing in a view where the affected knobs are hidden. The contract's 'clear notice' premise is not met in practice.
- *Axis-scoped re-engage (Tone re-engages eqTilt/colourTone; Character colourDepth; Loudness its seven)* — This is the most direct fix for the cross-axis trap, and the per-axis split is already recorded in ADR-0005:249. It changes ADR-0005 decision 6 and MODE invariant 3, so it is a hard-stop, and owner approval is required. Recommended as the gate question, not implemented unilaterally.
- *Re-engage on first value change instead of on press (extend the LookAndFeel.cpp:897-919 value-box rule to the knob face)* — This removes the press-without-move case and makes the manual's 'move' true. It reverses the KI-007 item 8 reading of 'gesture' and is a contract nuance, so it goes to the gate. A wheel notch still re-engages.
- *Disable the mouse wheel on the three macro knobs* — Pure UI, but it removes a convenience for everyone, and there is no evidence yet that accidental wheel input is common. Not recommended without evidence from a real host.
- *Modal confirmation before re-engaging* — Rejected: it blocks a continuous knob drag and punishes the deliberate case.

**Decision: Modify · P2.** The mechanism is the letter of an Accepted ADR, so the scope and trigger cannot be changed here. The observed harm, though, is a silent loss of hand-set values from ordinary Simple-view interactions, reproduced with a wheel notch and a zero-movement press on a macro that does not drive those parameters. The smallest change that addresses the harm without touching the contract is to make the re-engage visible and one-click reversible, and to make the manual exact. The contract narrowing (axis-scoped, first-move) is recommended to the owner at the gate, with this evidence.

*Calibration:* the verifier judged Modify / P1; the final judgement is Modify / P2. Challenge accepted: fully reproduced, but the re-engage fires once per detach cycle after a deliberate Advanced->Simple round trip, is audible, and one Undo restores it, so P2 not P1. Merge notes add accidental triggers that widen the fix scope, not the frequency evidence: (1) opening a macro readout's editor and dismissing without typing commits the unit-stripped pre-fill (e769f33:src/gui/LookAndFeel.cpp:796-817) and opens a macro gesture (session capture `rt/verify-10/09-sheet.png`: Comp Threshold -25.0 -> -10.6 dB), contradicting :898-918; (2) a click given only to focus a macro is a 'genuine grab' (:916-918), so click-then-arrow re-engages while Tab-then-arrow does not (rt/verify-17). Add both as acceptance cases; the ValueBox no-op fix is shared with [INPUT-017](findings-input.md#input-017). Risks: the owner must approve the notice itself (0.1.3 directive, C8); its Undo must target the re-engage step (each wheel notch is a step) and dismiss on later history changes; the trigger needs a processor-side message-thread signal; part (c) is a macro-layer contract change (ADR-0005 d6, hard stop).

*Adversarial challenge:* evidence holds: yes; priority justified: no (suggested P2); decision justified: yes (suggested Modify). Evidence: fully reproduced and code-confirmed. The cross-axis case is real: a Tone notch changed limGain and compThreshold, which Tone does not drive, and it contradicts ADR-0005's own Consequences line (:249, 'two for tone'). The zero-movement knob press is the same class of accidental re-engage the project already fixed for the value box in round 46. Priority: P1 is overstated under the rubric. The trap fires once per detach cycle and only after a deliberate Advanced-then-Simple round trip; macros are unreachable in Advanced since ADR-0023. The main path, deliberately moving Loudness, is the owner-signed contract (OQ-004, 2026-07-31) and is documented in USER_MANUAL.md:330-331. Recovery is a single Undo, confirmed at runtime, and the change is audible. The judge's own scores (frequency 3, severity 3) do not meet P1's 'every session / every mastering pass' bar or its 'costly recovery' bar. 'Every session that uses both layers' is asserted without usage evidence. P2 fits: a meaningful clarity and robustness gain in a less frequent, lower-cost situation. P1 would be defensible only for the accidental-trigger subset (wheel, zero-move press, cross-axis), and only with evidence that the round trip is routine. Decision: Modify is right. Surface the change without touching the contract, correct the manual, and take the scope and trigger questions to the owner. The proposal needs the constraints in proposal_risks. Without them its own acceptance criterion 2 fails for multi-notch wheel input. *Proposal risks:* (1) The notice's Undo is not the same as the toolbar Undo, so acceptance criterion 2 fails as written. Every wheel notch is its own gesture and its own undo step (juce_Slider.cpp:1164), and pushUndoStep does no coalescing (e769f33:src/PluginProcessor.cpp:588-598). After a scroll of more than one notch, or any other edit inside the 4 s window, a toolbar-equivalent Undo reverts only the latest step and leaves the re-engage in place. The notice must either undo back to the step that holds the re-engage (record the stack depth when it happens) or dismiss itself on any later pushUndoStep, undo or redo, A/B switch, preset apply or setStateInformation. The stacks are per slot (undoStacks[activeSlot]), so an A/B toggle would otherwise point it at the wrong stack. (2) The editor cannot detect the trigger from its current poll. The mask-fingerprint check (PluginEditor.cpp:2119-2129) looks the same for a gesture re-engage, resetToMacro (a dot click), undo, preset apply and an A/B switch. The processor needs a new signal that fires only when pendingReengage clears a non-empty mask (:718-719). Keep that signal on the message thread: a paint-thread read would fall under the ADR-0027/ADR-0038 cross-thread gating, and [TECH-002](findings-dsp-tech.md#tech-002) covers only part of this. (3) Owner direction the judge missed. The 0.1.3 owner directive removed every corner-dot explanation from the tooltips (CHANGELOG.md:1426-1429; PluginEditor.cpp:92-98). C8 (DEVELOPMENT_BRIEF.md:338) says a behaviour change 'does not license announcing it in the UI'. DESIGN §5.3 rule 2 makes the visual of the edited indicator owner-specified as well. So part (a) crosses no hard-stop, but the owner must approve whether the notice exists at all, not just its wording. 'Constrained change, no gate' understates this. (4) Part (c), first-move trigger, may be over-gated. A purely editor-side change would copy the round-46 value-box fix (LookAndFeel.cpp:897-919, e769f33:tests/state_tests.cpp:4437): defer the knob face's ScopedDragNotification to the first movement. The macros are non-automatable, so there is no host-automation consequence, and ADR-0005 decision 6 still reads literally true. It does reverse the recorded 'genuine macro grab' judgement (LookAndFeel.cpp:917-919), so it needs the owner's call. If it is done in the processor instead (defer the clear until the first value change), it breaks testTeardownAndReengageInvariants (e769f33:tests/state_tests.cpp:2917-2925) and reverses KI-007 item 8; that route is gated. (5) Axis-scoped re-engage also reopens the OQ-004 owner sign-off (OPEN_QUESTIONS.md:460) and DESIGN §5.3 rule 3, not only ADR-0005 d6 and MODE inv 3. colourDepth depends on both Loudness and Character (MacroEngine.cpp:238), so the axis sets overlap. The Simple dot and resetToMacro semantics would need restating, because the dot would persist after a Tone touch. (6) Part (b), the manual text, must reflect the JUCE asymmetries in note (3) above. (7) A smaller gate-free option was not considered: a pre-emptive cue. Today the only detach indicator sits beside Loudness (PluginEditor.cpp:1710), yet Tone and Character re-engage too. A cue before the discard fits the brief's 'clear notice' (DEVELOPMENT_BRIEF.md:137) better than a toast after it. It is still subject to C8 and the DESIGN §5.3 rule 2 visual ownership.

**Architecture gates.**

- Part (c) only: Simple/Advanced macro-layer contract change — conflicts with Accepted ADR-0005 decision 6 ('next macro-knob gesture re-engages every detached parameter') and MODE_AND_ADAPTATION_POLICY invariant 3; hard-stop, owner approval required
- Parts (a)/(b) touch no gate; notice wording is owner-supplied product text (C8, DEVELOPMENT_BRIEF.md:338)

**Dependencies.** [UI-001](findings-ui.md#ui-001) (the notice and the edited dot are one detach-feedback design); [MODEL-003](findings-state-model.md#model-003) (the same precedence mechanism re-lands off-curve automated values); [DOC-006](findings-doc-test.md#doc-006) (the same USER_MANUAL §5 paragraph); [TECH-002](findings-dsp-tech.md#tech-002) (any new paint-side read of detach state must use the published scalar)

**Acceptance criteria.**

- With ≥1 parameter detached, a macro gesture that re-lands at least one value shows a visible notice in Simple within one editor tick. The notice names the number (or the ids) of re-engaged parameters and does not interrupt the drag.
- Activating the notice's Undo restores every re-engaged value and the detach mask exactly: the dump matches the pre-gesture dump, and the Simple edited dot reappears.
- With no parameter detached, macro gestures show no notice.
- USER_MANUAL §5 lists the inputs that re-engage (knob press, drag, wheel, typed value, reset) and the one that does not (value-box click), and states that any macro re-engages all detached parameters and that Undo reverts it.
- If the gate approves axis-scoped re-engage: with limGain and eqTilt detached, a Tone wheel notch re-lands eqTilt only, and limGain keeps its value and its badge. A state_tests case pins this.

<details><summary>Verification record</summary>

**Method.** Code read at e769f33, all anchors confirmed: gesture-begin macro branch (e769f33:src/PluginProcessor.cpp:285-308), whole-mask clear (:718-719), mapper (e769f33:src/MacroEngine.cpp:225-244) with no-op skip (:246-257), undo snapshot at gesture-begin (:262-276) and push at gesture-end (:339-354), and JUCE 9.0.1 Slider wheel (juce_Slider.cpp:1164) and typed commit (:451), both inside ScopedDragNotification. Reproduced on :144 with stepped motion (rt/verify-14). At Loudness 50, hand-dragged Comp Threshold -10.0 to -14.3 dB and Limiter Gain 7.8 to 5.9 dB in Advanced; both badges appeared (03-crop.png). In Simple, one wheel notch over Tone (0.00 to 0.06) re-landed Threshold to -10.0 dB and Limiter Gain to 7.8 dB, and the edited dot vanished (04-05-wheel-sheet.png). One Undo restored both values and the dot (06-crop.png). A press-and-release on Character with zero movement (Character stayed 0.00) re-landed both again (07-crop.png), and Undo restored them again. G-14 screenshots viewed (typed Loudness 50; 30 px Loudness drag).

**Corrections to the candidate claim.** (1) The edit is not lost beyond recovery. The gesture that re-engages is itself one undo step, so a single Undo restores values and mask (runtime-confirmed). The harm is that the loss is easy to miss and costly once other edits pile on top. (2) A click on a macro's numeric value box does NOT re-engage: the value box opens its gesture on first movement (e769f33:src/gui/LookAndFeel.cpp:897-919, test e769f33:tests/state_tests.cpp:4437). The knob face, a knob drag, the wheel, a typed commit and a double-click or Alt-click reset do re-engage. (3) The mapper writes all nine targets but skips no-op writes, so only off-curve values change. (4) The manual says 'The next time you move a macro knob' (USER_MANUAL.md:330-331), but a press that moves nothing also re-engages (KI-007 item 8, a deliberate choice), so the manual is slightly inaccurate. (5) The behaviour is the letter of Accepted ADR-0005 decision 6 and MODE invariant 3, not a defect against the spec.

</details>

<sub>Verifier scores (1-5): impact 4 · frequency 3 · severity 3 · discoverability 4 · efficiency 3 · coherence 3 · change risk 3 · complexity 3 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### MODEL-002

**Advanced does not show the macro positions, but nothing that consumes them can be reached from Advanced, and factory presets carry their own**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Preserve** | **none** | high | partially-confirmed | Information architecture | The macro layer overwrites hand edits and automation with almost no notice | — |

**Evidence**

- e769f33:src/gui/PluginEditor.cpp:594-600 — the Advanced read-only macro row was removed at 0.1.2 (ADR-0023)
- e769f33:docs/architecture/design-decisions/ADR-0023-012-field-fix-contracts.md:250-253 — Decision item 9: macro row removed, kAdvancedH = 822; owner-directed, gate cleared
- e769f33:src/PluginParameters.cpp:280-288 — loudness/character/tone are non-automatable
- e769f33:src/PluginProcessor.cpp:283-309 — pendingReengage armed only in gesture-begin for the three macro ids
- e769f33:src/gui/PluginEditor.cpp:1810-1811 — the edited dot (resetToMacro) is hidden in Advanced
- e769f33:src/PresetManager.cpp:253-262 and e769f33:src/PluginProcessor.cpp:1636-1652 — factory apply: defaults pass, then intents, then relandMacroCurve from the preset's own macros
- LAY-03 + [capture](captures/02-advanced-view.png) — no macro control visible in Advanced

**Current behaviour.** In Advanced the three macro positions are not displayed anywhere, and no host lane shows them. Their effect is visible as the managed knobs' values. Every action that re-lands managed knobs from the current macro positions is available only in Simple, where the positions are displayed.

**Problem.** Minor information gap only. An Advanced user cannot read the macro positions without switching views, but cannot act on them in Advanced either, and factory presets do not depend on them.

**Root cause.** The mirror was removed by owner directive (ADR-0023 item 9), on the rationale that it 'answered no question the detach badges and the Simple view do not' (PluginEditor.cpp:595-600). Verification supports that rationale.

**User impact.** Negligible for mastering decisions. The one plausible cost is curiosity ('what macro setting produced these values?'), answered by one view switch that changes nothing (sound-neutral, but an undo step; see [STATE-006](findings-state-model.md#state-006)). *Scope:* Advanced view display only.

**Proposed improvement.** No change. Keep Advanced without a macro mirror. Optionally note in the fine review that DESIGN §6.3 still draws the removed macro row without a superseded banner (see new_findings).

**Alternatives considered.**

- *Compact text readout in Advanced (e.g. 'Macros  L 60 · C 0.25 · T 0.15')* — Cheap and needs no height change, but it restores what the owner removed, for a value the user cannot act on in that view. The evidence shows no decision it would change.
- *Restore the read-only macro row* — Directly conflicts with Accepted ADR-0023 item 9 and costs 78 px. Reject.
- *Leave as-is* — Chosen: every consumer of the hidden values is gated behind the view that shows them.

**Decision: Preserve · none.** The harmful part of the claim does not survive verification. Factory presets land on their own macros, and re-engage can be triggered only from Simple, where the macros are visible. The remaining invisibility is the direct result of an owner-directed, gate-cleared ADR whose rationale the evidence supports.

**Architecture gates.**

- Conflict with Accepted ADR-0023 Decision item 9 (read-only macro row removed; Advanced 940×822) — any re-added macro mirror

**Dependencies.** [UX-007](findings-ux.md#ux-007); [MODEL-001](findings-state-model.md#model-001)

**Acceptance criteria.**

- Advanced continues to show no macro mirror and remains 940×822
- Loading a factory preset from Advanced yields managed values equal to M(preset macros), independent of the macro positions before the load
- Every re-engaging action (macro gesture, edited-dot click) remains reachable only where the macro positions are displayed

<details><summary>Verification record</summary>

**Method.** Read e769f33:src/gui/PluginEditor.cpp:594-600 (the macro row removal note) and :1797-1801, e769f33:src/PluginParameters.cpp:275-288 (macros non-automatable), e769f33:src/PluginProcessor.cpp:283-309 (re-engage armed only by a macro-id gesture begin), e769f33:src/gui/PluginEditor.cpp:658 and :1810-1811 (edited dot → resetToMacro, hidden in Advanced), e769f33:src/PluginProcessor.cpp:1603-1652 (factory apply) with e769f33:src/PresetManager.cpp:253-300 (defaults pass over every non-excluded parameter, then intents), and ADR-0023 Decision item 9 (e769f33:docs/architecture/design-decisions/ADR-0023-012-field-fix-contracts.md:250-253). Viewed [capture](captures/02-advanced-view.png).

**Corrections to the candidate claim.** Invisibility is confirmed. 'Decides where factory presets land' is REFUTED: a factory apply first runs a defaults pass over every non-excluded parameter, macros included, then applies the table's intents, then relandMacroCurve(). The preset's own macro positions decide the landing, and the previously hidden positions are overwritten. For re-engage, the positions do drive the landing, but both re-engaging verbs (a macro-knob gesture, PluginProcessor.cpp:283-309, and the edited dot, PluginEditor.cpp:658) exist only in Simple, where the macros are visible before the user acts. The consequence is never triggered from the view where the macros are hidden. Unobserved edge: a host generic editor that lists the non-automatable macros could gesture them.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 2 · severity 1 · discoverability 2 · efficiency 1 · coherence 2 · change risk 2 · complexity 1 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### MODEL-003

**Host automation of a managed parameter is silently overridden by the next macro touch, even on another axis, and the manual's automation FAQ does not say so**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Modify** | **P2** | medium | partially-confirmed | Documentation | The macro layer overwrites hand edits and automation with almost no notice | Phase 4 |

**Evidence**

- e769f33:src/PluginProcessor.cpp:658-659 — '(managedGestureBits & bit) == 0 → return; // ungestured: automation/restore — never detaches'
- e769f33:src/MacroEngine.cpp:6-8 — mapping listener on loudness/character/tone only
- e769f33:src/MacroEngine.cpp:225-244,246-257 — every mapping pass writes each non-detached managed parameter whose value differs from the curve
- e769f33:tests/state_tests.cpp:3640-3646 — 'automation (no gesture) never detaches' then 'the ungestured edit was re-mapped by the macro'
- e769f33:docs/architecture/design-decisions/ADR-0005-macro-layer-architecture.md:253-258 — accepted residue 'off-curve but engaged … bounded — the next macro gesture re-engages them'
- e769f33:docs/user/USER_MANUAL.md:506-512 — 'Every stage parameter … can be automated as usual'; the macros are excluded because 'automating a macro that itself writes other parameters would fight the host', but the reverse precedence is not stated
- Runtime verify-14: off-curve clear-mask state, no dot, then a Tone wheel notch → Limiter Gain 5.9→7.8 dB, Threshold -14.3→-10.0 dB — session capture `rt/verify-14/08-offcurve-loaded.png`, session capture `rt/verify-14/09-after-tone-wheel.png`

**Current behaviour.** An automation lane on a managed parameter writes it ungestured, so it never detaches and no badge appears. The next mapping pass takes the value back to the curve, and nothing on screen indicates that this happened. A mapping pass is triggered by any Loudness/Character/Tone change or gesture, a factory preset or reset-to-macro. The value then stays at the curve until the host sends the next lane value.

**Problem.** The manual promises managed parameters 'can be automated as usual' and warns only about the opposite direction, macros fighting the host. It does not tell the user that touching any macro during playback overrides an automated managed parameter. The user can then audition a value the lane does not hold.

**Root cause.** One value per managed parameter is shared by the lane and the mapper (ADR-0005 'One value model'). The mapper wins whenever it runs because it compares against the curve, not against the last automation write. The FAQ was written from the macro-lane side only.

**User impact.** Consider a user who automates Limiter Gain and touches Loudness or Tone during playback. They hear the macro-curve value until the lane's next point, and on a flat segment that can be many bars. They may judge the master on a sound the bounce will not reproduce. There is no badge or notice explaining it. *Scope:* Only sessions that automate one of the nine managed parameters (limGain, compThreshold, compRatio, clipDrive, clipShape, colourDepth, dynTilt, eqTilt, colourTone) AND touch a macro while that automation is active. This is less frequent than [MODEL-001](findings-state-model.md#model-001) but has the same mechanism.

**Proposed improvement.** Documentation now, no behaviour change. Add one FAQ sentence to USER_MANUAL 'Can I automate the controls?' and a line in §5. It should say that the nine macro-managed parameters can be automated, but touching Loudness, Character or Tone re-applies the macro curve to every managed parameter that is not detached, including automated ones, until the lane's next value arrives. To keep an automated value, avoid moving the macros during playback, or detach the parameter first by editing it by hand in Advanced. List the nine parameters. Separately, measure in two or three real hosts whether the mapper's ungestured writes get recorded to a lane in Touch/Latch/Write modes before deciding whether a behaviour change is warranted.

**Alternatives considered.**

- *Make ungestured writes detach* — Explicitly rejected in ADR-0005 (automation playback would silently detach parameters, which is worse). Contract change; gate.
- *Suppress the mapper for parameters written by automation within the last N ms* — This adds hidden, history-dependent state, contrary to ADR-0005's pure-function mapping. It is a macro-layer contract change, so it goes to the gate. Not justified without host evidence.
- *Leave the FAQ as-is* — Rejected: 'automated as usual' is contradicted by a reproducible precedence rule, and the fix is one paragraph.

**Decision: Modify · P2.** The behaviour is an Accepted-ADR residue, and changing the precedence would be gated and is not supported by host evidence. The smaller, non-gated change is to make the documentation state the rule it currently omits. The host-recording question is recorded for measurement, not guessed at.

**Architecture gates.**

- Documentation change: none
- Any behaviour change to lane-vs-macro precedence would be a Simple/Advanced macro-layer contract change conflicting with Accepted ADR-0005 (Consequences 'Accepted residue' and decision 3) — hard-stop

**Dependencies.** [MODEL-001](findings-state-model.md#model-001) (same re-land mechanism and the same manual §5 rewrite)

**Acceptance criteria.**

- USER_MANUAL's automation FAQ names the nine macro-managed parameters and states that moving any macro re-applies the curve to non-detached automated parameters until the lane's next value.
- USER_MANUAL §5 cross-references that rule.
- A host-behaviour note (recording or not recording mapper writes in Touch/Latch/Write) exists for at least two hosts before any precedence change is proposed.

<details><summary>Verification record</summary>

**Method.** Code read at e769f33. The discriminator (e769f33:src/PluginProcessor.cpp:650-664) never detaches an ungestured write. The mapper re-lands every non-detached off-curve value on any mapping pass (e769f33:src/MacroEngine.cpp:225-257). The mapping listener is registered only on the three macros (e769f33:src/MacroEngine.cpp:6-8), so an automation write itself never arms it. The test that pins 'automation never detaches; the macro takes it right back' was read (e769f33:tests/state_tests.cpp:3640-3646), as were ADR-0005:253-258 and USER_MANUAL.md:504-512. Runtime on :144 (rt/verify-14): host automation playback cannot be produced in the harness, because its `param` command is gestured and therefore detaches. The precedence mechanism was reproduced through the equivalent ungestured path instead. A state was loaded with Limiter Gain 5.9 dB and Threshold -14.3 dB off-curve at Loudness 50 and a clear DETACH_MASK, the ADR-0005 'off-curve but engaged' shape. No dot was shown. One wheel notch on Tone re-landed both, to 7.8 dB and -10.0 dB (08-offcurve-loaded.png, 09-after-tone-wheel.png).

**Corrections to the candidate claim.** (1) ADR-0005:253-258 does not say 'a macro gesture overwrites the lane value'. It records the off-curve-but-engaged state and presents 'the next macro gesture re-engages them' as the BOUND on that residue, not as a harm. (2) The override is not limited to a macro gesture. Any macro VALUE change arms the mapping (MacroEngine::parameterChanged), and it is cross-axis: a Tone nudge retakes an automated Limiter Gain. (3) What happens after the override is host-dependent and unverified. It depends on whether the host re-sends the lane at the next breakpoint or continuously, and on whether hosts in Touch/Latch/Write modes record the mapper's ungestured setValueNotifyingHost writes to the lane.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 2 · severity 3 · discoverability 4 · efficiency 2 · coherence 3 · change risk 1 · complexity 1 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### MODEL-004

**The Character macro does nothing after loading 'Transparent Master' or 'Classical Dynamics' (Clean colour model), and nothing on screen says why**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Proceed** | **P2** | high | confirmed | Interaction model | The macro layer overwrites hand edits and automation with almost no notice | Phase 5 |

**Evidence**

- e769f33:src/PresetManager.cpp:143-151 (comment: Clean chosen so the 'untouched' intent does not rest on a curve constant)
- e769f33:src/PresetManager.cpp:152-155 (kTransparentMaster: character 0.05, colourModel 0)
- e769f33:src/PresetManager.cpp:195-198 (kClassicalDynamics: character 0.0, colourModel 0)
- e769f33:src/PluginParameters.cpp:334-349 (default Tape: 'Clean is the null model and would make the Character macro inert')
- e769f33:docs/architecture/design-decisions/ADR-0005-macro-layer-architecture.md:149-154 (audibility rule)
- e769f33:src/MacroEngine.h:57 and e769f33:src/MacroEngine.cpp:239 (Character drives only colourDepth)
- e769f33:src/dsp/ClipSat.h:54-56 and e769f33:src/dsp/ClipSat.h:282 (colour sub-block runs only when dep > 0 && model != 0)
- e769f33:src/gui/PluginEditor.cpp:1778 and e769f33:src/gui/PluginEditor.cpp:1797-1801 (modelBox Advanced-only; no Simple indicator)
- e769f33:src/gui/PluginEditor.cpp:39 (tooltip 'Clean to Color - ...')
- e769f33:src/InternalState.h:110 (tooltips default OFF)
- e769f33:docs/user/USER_MANUAL.md:424-427 (the manual's transparent-master workflow starts from 'Transparent Master')
- V15-01 runtime: Transparent Master loaded, dump colourModel Clean / colourDepth 2.8 % — session capture `rt/verify-15/02-transparent-master.png`
- V15-02 runtime: Character dragged 0.05 -> 0.55, dump colourDepth 30.2 %, colourModel Clean, no Simple-view cue — session capture `rt/verify-15/03-crop.png`

**Current behaviour.** 'Transparent Master' (Loudness 25, Character 0.05) and 'Classical Dynamics' (Loudness 15, Character 0) both set colourModel = Clean. In Simple view the Character knob turns normally, and the mapping writes colourDepth (0.55 gives 30.2 %). ClipSat then skips the colour residue for model 0, so the output does not change. Simple view has no colour-model control or indicator, and the only explanation is a tooltip, which is off by default.

**Problem.** One of the three Simple-view macros becomes a dead control in two of the thirteen factory presets, with no visible reason. This is the same trap ADR-0005 item 9 removed from the default patch.

**Root cause.** Character's single managed target (colourDepth) is gated by colourModel, an unmanaged discrete parameter shown only in Advanced. Two factory tables set Clean to protect an 'untouched' intent that Character = 0 already guarantees on any model. The Simple view does not show the state of this dependency.

**User impact.** A user who starts from 'Transparent Master' (the manual's recommended starting point for a transparent master) or 'Classical Dynamics' and reaches for Character hears no change. They can conclude that Character is ineffective on their material, or broken, and cannot find the cause without opening Advanced and knowing that 'Clean' in the Color combo is a null model. *Scope:* Factory presets transparentMaster and classicalDynamics. It also covers any state viewed in Simple with colourModel = Clean, from a user preset, a session or an Advanced edit. Only Character is affected; Loudness and Tone's eqTilt still work.

**Proposed improvement.** (1) Factory bank. In both tables, replace the colourModel 0 override with Tape (or drop it so the Tape default applies), and set Transparent Master's Character to 0.00. Loading stays sound-identical: colourDepth becomes exactly 0, and the colour sub-block then contributes nothing on any model. Character then gives audible colour from the first touch. Rewrite the PresetManager.cpp:143-151 comment to match. (2) Simple view, display-only. While colourModel == Clean, draw Character visibly inert (dimmed arc/value) with a short caption or always-visible hint, for example 'Clean color - Character adds nothing (choose a model in ADV)'. It adds no value path and takes one UI tick to follow a model change. The values and wording go to the owner's fine review.

**Alternatives considered.**

- *Leave as-is* — Rejected. It contradicts ADR-0005 item 9's own reasoning and the knob's 'Clean to Color' promise, and the user cannot tell why the knob is inert.
- *Preset change only* — Fixes the factory case at zero sonic cost. States set to Clean by the user are still silent in Simple.
- *Indicator only* — Explains the inert knob, but two shipped presets still start with a dead macro.
- *Make Character select or override the colour model (e.g. treat Clean as Tape when Character > 0)* — Changes how Simple maps onto Advanced (macro-layer contract gate) and changes recall. Out of proportion to the problem.
- *Add a colour-model selector to the Simple view* — Widens the deliberately minimal Simple surface. Could be revisited later; not needed to remove the trap.

**Decision: Proceed · P2.** Confirmed in code and at runtime. The preset fix is sound-neutral, because Character = 0 already nulls the colour stage on every model, and it restores ADR-0005 item 9's intent for the factory bank. The display-only cue covers user-created Clean states. Neither touches a hard-stop category.

**Dependencies.** [MODEL-005](findings-state-model.md#model-005) (Tone's colourTone target sits behind the same Clean/depth gate); G-01 (tooltips off by default, so the only textual hint is hidden); PF-parameter-macro-model-8 (macro/state dependencies invisible across views)

**Acceptance criteria.**

- Loading 'Transparent Master' and 'Classical Dynamics' produces output bit-identical to the e769f33 build on a fixed programme at the same rate and OS setting (null test).
- After loading either preset, raising Character in Simple from 0 to 0.5 audibly changes the output (a non-null against Character 0), and the dump shows colourModel != Clean.
- A state test asserts that no factory preset table sets colourModel = Clean (ADR-0005 item 9 extended to the factory bank).
- With colourModel set to Clean in Advanced and the view switched to Simple, the Character knob shows the inert state and hint with tooltips OFF; selecting Tape removes it within one editor tick.

<details><summary>Verification record</summary>

**Method.** Code read at: PresetManager.cpp:143-155 and 195-198; PluginParameters.cpp:282-285 and 334-349; ADR-0005:149-154; MacroEngine.h:57; MacroEngine.cpp:239; ClipSat.h:54-56 and 282. Grepped every pid::character use: Character's only DSP target is colourDepth. Also read PluginEditor.cpp:39 (tooltip), PluginEditor.cpp:1771-1801 (modelBox is Advanced-only; the Simple list has no model indicator) and InternalState.h:110 (tooltips default OFF). Runtime on :145, stepped pointer motion throughout. Loaded Transparent Master from the preset menu; the dump read character 0.05, colourModel Clean, colourDepth 2.8 %. Then dragged Character up 140 px in 10-px steps; the dump read character 0.55, colourDepth 30.2 %, colourModel still Clean. The Simple view shows nothing about the model.

**Corrections to the candidate claim.** The inertness is total, not partial: colourDepth is Character's only target, and ClipSat skips the whole colour sub-block when model == 0, at any depth. The preset comment (PresetManager.cpp:143-151) shows Clean was a deliberate choice to protect the 'untouched' intent. That intent does not need Clean: Character = 0 gives colourDepthPct(l,0) = 0 exactly, and depth 0 is exact identity on every model (ClipSat.h:54-55). Transparent Master uses Character 0.05, not 0. Even the Character tooltip is hidden by default, because tooltips ship OFF.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 2 · severity 2 · discoverability 5 · efficiency 2 · coherence 4 · change risk 2 · complexity 2 · evidence 5</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### MODEL-005

**The same Tone position means different things depending on Character: at the default Character 0, Tone is only a ±2 dB EQ tilt**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Preserve** | **none** | medium | partially-confirmed | Interaction model | The macro layer overwrites hand edits and automation with almost no notice | — |

**Evidence**

- e769f33:src/MacroEngine.h:57-60 (colourDepth = 100·c·(0.4+0.6·l); eqTilt = 2·t; colourTone = 0.5·t)
- e769f33:src/MacroEngine.cpp:241-242 (both Tone targets written every pass)
- e769f33:src/dsp/ClipSat.h:282 (colour residue gated on dep > 0 && model != 0)
- e769f33:src/dsp/ClipSat.h:329-330 (colourTone = 2 kHz split on the residue only)
- e769f33:src/dsp/MasteringEQ.h:12-14 and e769f33:src/dsp/MasteringEQ.h:221-224 (tilt = LS -G / HS +G at 700 Hz)
- e769f33:src/PluginParameters.cpp:380 (eqTilt range ±3 dB)
- e769f33:src/gui/PluginEditor.cpp:40 ('Dark to bright tilt of the overall result')
- e769f33:docs/user/USER_MANUAL.md:173
- E08 runtime (edges observer): Tone Up moved colourTone and eqTilt — [capture](captures/12-keyboard-focus-invisible.png)

**Current behaviour.** Tone drives eqTilt (±2 dB per shelf) and colourTone (±0.5). At Character 0 (the default), or with the Clean model, colourDepth is 0 and colourTone has no audible effect, so Tone acts as a pure tilt. Once Character adds colour, the same Tone position also darkens or brightens that colour.

**Problem.** The target coupling is real but co-directional. No evidence shows it misleads users or harms a workflow; raising Character adds colour whose brightness follows the Tone the user already chose, which is consistent with the knob's stated meaning.

**Root cause.** This is a deliberate design of the frozen §5.5 curves: one macro drives two brightness targets in the same direction, one of them inside the colour residue.

**User impact.** Slight. The colour Character adds is voiced by Tone; the dominant effect of Tone, the tilt, is constant across Character. The ±2 dB Simple-view tilt is a normal macro scope, and Advanced reaches ±3 dB. *Scope:* Every session that uses Tone. The effect difference appears only once Character > 0 with a non-Clean model.

**Proposed improvement.** Keep the curves. Optional polish at the owner's discretion: add one clause to the manual's Tone row and tooltip, for example 'also voices the color Character adds'. This makes the coupling explicit without changing behaviour.

**Alternatives considered.**

- *Decouple Tone from colourTone (eqTilt only)* — A macro-layer contract change and a post-freeze curve change (PARAMETER_COMPATIBILITY rule 7 requires an ADR). It breaks recall of saved sessions for no demonstrated benefit.
- *Widen Tone's tilt to ±3 dB* — Same gate and recall cost. No evidence that ±2 dB is insufficient.
- *Show the Tone split (tilt / colour tone) in Simple* — Adds UI weight for a secondary effect. Not justified.
- *Preserve and optionally document* — Chosen. Behaviour is coherent; documentation is optional polish.

**Decision: Preserve · none.** The code facts are confirmed, but the coupling is co-directional and consistent with 'tilt of the overall result'. Harm is unevidenced. Any change is a macro-layer contract and recall change under an Accepted ADR, which costs more than the problem.

**Architecture gates.**

- Macro-layer contract change (only for the rejected curve-change alternatives)
- PARAMETER_COMPATIBILITY_POLICY rule 7 / ADR-0005 frozen curves (only for the rejected alternatives)

**Dependencies.** [MODEL-004](findings-state-model.md#model-004) (the same Clean/depth gate silences colourTone); [DSP-004](findings-dsp-tech.md#dsp-004) (the Tone tilt cannot offset the ADAA-1 top-end droop)

**Acceptance criteria.**

- MacroEngine.h eqTiltDb/colourTone curves remain eqTilt = 2·t dB and colourTone = 0.5·t; testMacroDefaultIsFixedPoint and the curve-evaluation tests stay unchanged.
- If the optional wording is adopted, the USER_MANUAL Tone row states that Tone also voices the colour Character adds.

<details><summary>Verification record</summary>

**Method.** Code read at MacroEngine.h:57-60 and MacroEngine.cpp:239-242. Also read ClipSat.h:282 and 321-335: colourTone acts only inside the residue branch, which is gated on dep > 0 && model != 0; it is a 2 kHz one-pole split weighted (1-ton)/(1+ton) on the residue only. Read MasteringEQ.h:12-14 and 217-224 (tilt = low shelf -G plus high shelf +G at 700 Hz), PluginParameters.cpp:380 (eqTilt ±3 dB), PluginEditor.cpp:40 and 66 (tooltips) and USER_MANUAL.md:172-173. Runtime E08 (edges observer) showed one Up arrow on Tone moving both colourTone and eqTilt. I did not measure audibility; the harness has no analyser.

**Corrections to the candidate claim.** The mapping facts hold: eqTilt = 2t dB, colourTone = 0.5t, and colourTone is silent at Character 0, or with the Clean model. The claimed harm is not established. Both targets move in the same direction (dark to bright), so 'Tone = brightness of the overall result, including the added colour' stays true at every Character. colourTone only re-voices the residue that colourDepth scales, with |ton| ≤ 0.5, so its effect is secondary to the tilt. 'Half silent' is true by target count, not by audible weight. The ±2 dB is per shelf at the extremes, a 4 dB end-to-end span around 700 Hz, which is an ordinary mastering tilt; Advanced keeps ±3 dB.

</details>

<sub>Verifier scores (1-5): impact 1 · frequency 3 · severity 1 · discoverability 2 · efficiency 1 · coherence 2 · change risk 5 · complexity 3 · evidence 4</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

### MODEL-006

**What a DAW records during a macro gesture is undefined: the managed writes are unbracketed, and the macros are only advisorily non-automatable**

| Decision | Priority | Confidence | Verification | Workstream | Theme | Roadmap |
|---|---|---|---|---|---|---|
| **Investigate further** | **P2** | medium | partially-confirmed | Technical robustness | The macro layer overwrites hand edits and automation with almost no notice | Phase 0 |

**Evidence**

- e769f33:src/MacroEngine.cpp:246-257 (setParam: setValueNotifyingHost, no begin/endChangeGesture)
- e769f33:src/MacroEngine.cpp:225-244 (applyMapping writes all nine every pass; no-op writes suppressed)
- e769f33:src/MacroEngine.cpp:103-116 (host-written macro: flag only; drained by the 30 ms timer off the message thread)
- e769f33:src/gui/PluginEditor.cpp:603-605 and e769f33:src/gui/PluginEditor.cpp:1174 (SliderAttachment brackets only the macro id)
- e769f33:src/gui/PluginEditor.h:230-232 (the only other gesture brackets are knob resets)
- e769f33:src/PluginParameters.cpp:276-288 (macros withAutomatable(false))
- e769f33:docs/architecture/design-decisions/ADR-0005-macro-layer-architecture.md:89-100 (Decision 1-2: 'gesture-bracketed per knob drag')
- e769f33:docs/architecture/design-decisions/ADR-0005-macro-layer-architecture.md:249-252 ('appear in the host's parameter history')
- e769f33:docs/policies/PARAMETER_COMPATIBILITY_POLICY.md:18-20 vs e769f33:docs/policies/PARAMETER_COMPATIBILITY_POLICY.md:29-31 (REAPER shows it anyway vs 'no macro automation lane can exist')
- e769f33:src/MacroEngine.h:17-21 (bracketing 'land[s] at P4' — never landed on the managed ids)
- e769f33:docs/user/USER_MANUAL.md:504-515 (automation FAQ says nothing about what a macro move records)
- Vendored JUCE (build tree, not revision-pinned): juce_audio_plugin_client_VST3.cpp:1458-1485 — performEdit without beginEdit for unbracketed changes

**Current behaviour.** During a Simple-view Loudness drag, the host receives beginEdit/endEdit on 'loudness', which is flagged non-automatable. It also receives, from the message thread 0-30 ms later, performEdit (VST3) or value-change events (AU) on up to seven managed ids, with no gesture around them. A host that writes a macro directly gets the mapping on the message thread at up to 30 ms wall-clock latency; REAPER is documented to expose the macros.

**Problem.** What a DAW records in Write, Touch or Latch mode during an ordinary macro move is not defined by the product, not tested, and described inconsistently by the ADR and the policies. Touch and Latch semantics depend on gestures that the plugin never sends for the managed ids.

**Root cause.** ADR-0005 option C made the macros non-automatable, and the fan-out was implemented as plain setValueNotifyingHost calls. Decision 2's bracketing was never implemented on the managed ids, and no host-matrix test exists.

**User impact.** An engineer riding Loudness during a write pass may get any of three results. Up to seven managed lanes may be recorded, which afterwards override later macro moves (the accepted 'off-curve' residue). Nothing may be recorded, so the ride is lost on playback. In REAPER, a Loudness lane may be recorded whose mapping on an offline bounce follows wall-clock timing rather than the render timeline. Recovering means finding and deleting or redrawing lanes, and the manual gives no warning. *Scope:* VST3 and AU hosts with automation write modes; Simple-view gestures on Loudness, Tone and Character; REAPER for the macro-lane path. Not the Standalone.

**Proposed improvement.** Target: a macro drag in any host write mode gives one defined, documented result. Preferably the host records exactly the managed lanes the macro moved, bracketed as one touch, so playback and offline bounce reproduce the pass sample-accurately and no macro lane is used. Steps: (1) Run the host matrix listed in the acceptance criteria. (2) If hosts drop or mangle the unbracketed writes, implement ADR-0005 Decision 2 literally. At macro gesture begin, call beginChangeGesture on the non-detached managed ids that macro drives. At macro gesture end, flush the mapping, then end those gestures. Mark these macro-opened gestures so that managedGestureBits and the undo coalescer ignore them: no detach, no extra undo steps. (3) Reconcile the docs: ADR-0005 Decision 2 and Consequences, the MacroEngine.h header, PARAMETER_COMPATIBILITY rule 7 against rule 5, and the USER_MANUAL automation FAQ. The FAQ should say what a macro move records, that automation belongs on the Advanced parameters, and that a macro lane in hosts that expose one is unsupported and not bounce-deterministic.

**Alternatives considered.**

- *Leave the code as-is and only document* — Cheapest, but the recorded outcome stays host-dependent and unknown. Acceptable only if the host matrix shows consistent behaviour.
- *Bracket the managed writes per macro gesture* — Gives deterministic Touch/Latch semantics and matches ADR-0005 D2's wording. It interacts with the detach discriminator (gestured managed writes currently mean 'manual edit') and the undo coalescer, so it needs targeted tests.
- *Move the macros out of the APVTS into host-hidden session state* — Removes the REAPER macro-lane path entirely, but it removes parameter IDs and changes the serialization schema, both hard stops. Rejected for now.
- *Make the macros automatable with audio-thread mapping (reverse option C)* — Needs a kVersion bump, a superseding ADR and a threading-model change. Out of proportion.

**Decision: Investigate further · P2.** The code-level facts are confirmed, including the JUCE wrapper behaviour, but the user-visible outcome depends on host behaviour that cannot be observed without real DAWs. Needed evidence: for REAPER, Cubase/Nuendo, Logic Pro (AU), Ableton Live, Bitwig Studio and Studio One, in Write, Touch and Latch modes, a 4-second Simple-view Loudness drag. Record which lanes (macro and managed) receive points, then compare two offline bounces and one realtime render of that pass by null test. The doc contradictions can be fixed now, independent of the result.

**Architecture gates.**

- ADR-0005 Decision 3 (the discriminator treats gesture-bracketed managed writes as manual edits; bracketing macro writes must not change that contract)
- Thread Model change — only if bracketing adds a cross-thread gesture path (macro gesture callbacks may arrive off the message thread, PluginProcessor.cpp:275-309)
- Parameter Registry / Serialization Registry change and conflict with ADR-0005 option C / ADR-0010 — only for the host-hidden or automatable-macro alternatives

**Dependencies.** PF-parameter-macro-model-6 (macro touch overrides a managed lane; FAQ omits it); PF-parameter-macro-model-13 (stale MacroEngine header); [DSP-010](findings-dsp-tech.md#dsp-010) (same automation FAQ and registry rationale text)

**Acceptance criteria.**

- A recorded host matrix (REAPER, Cubase/Nuendo, Logic Pro AU, Ableton Live, Bitwig Studio, Studio One × Write/Touch/Latch) lists, for a 4-second Simple-view Loudness drag, which lanes received automation points.
- For every host in the matrix, two offline bounces and one realtime render of the recorded pass null against each other to below -90 dBFS, or each residual difference is documented in KNOWN_ISSUES.
- ADR-0005 Decision 2/Consequences, MacroEngine.h:17-21 and PARAMETER_COMPATIBILITY_POLICY rules 5 and 7 describe the same, implemented behaviour, with no statement that a macro lane cannot exist.
- The USER_MANUAL automation FAQ states what a macro move records in a host write mode and recommends automating the Advanced parameters.
- If bracketing is implemented, a state test shows a macro drag opens and closes exactly one gesture on each managed id it moves, sets no detach bit, and yields exactly one undo step.

<details><summary>Verification record</summary>

**Method.** Code read at MacroEngine.cpp:103-116 and 225-257. Grepped begin/endChangeGesture across src: only the knob reset paths at PluginEditor.h:230-232 and 266-268 call them. The macro knobs attach through SliderAttachment (PluginEditor.cpp:603-605, 1172-1174), which brackets only the macro id. Read PluginParameters.cpp:276-288 and ADR-0005:89-103 and 249-252. Read PARAMETER_COMPATIBILITY_POLICY.md:18-20 against :29-31, MacroEngine.h:17-21 and USER_MANUAL.md:504-515. Checked the vendored JUCE VST3 wrapper (build/_deps/juce-src/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp:1458-1485): on the message thread, paramChanged calls setParamNormalized + performEdit, and beginEdit/endEdit come only from gesture callbacks. No DAW is available here, so host recording in Write/Touch/Latch could not be reproduced.

**Corrections to the candidate claim.** The managed set is nine. Loudness drives up to seven: limGain, compThreshold and compRatio always; clipDrive and clipShape above l = 0.3; dynTilt above 0.5; colourDepth when Character > 0. Tone drives two and Character one. The ADR is not just silent: ADR-0005 Decision 2 (:97-100) says the writes are 'gesture-bracketed per knob drag', while the code brackets only the macro id. MacroEngine.h:17-21 still lists the bracketing as future P4 work. PARAMETER_COMPATIBILITY_POLICY rule 7 (:29-31) says 'no macro automation lane can exist', which contradicts rule 5 (:18-20) in the same file ('REAPER shows it anyway'). ADR-0005 explicitly accepts the non-deterministic offline mapping of a host-written macro as unsupported usage. REAPER's exposure of the macro lanes is asserted by the policy, not tested here.

</details>

<sub>Verifier scores (1-5): impact 3 · frequency 2 · severity 3 · discoverability 4 · efficiency 3 · coherence 3 · change risk 4 · complexity 3 · evidence 3</sub>

[Back to the index](../2026-09-26-anabasis-product-ux-audit.md#finding-index)

