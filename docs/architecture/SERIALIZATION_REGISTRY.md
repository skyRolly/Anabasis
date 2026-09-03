# SERIALIZATION_REGISTRY.md

The **ledger of every byte this plugin persists** — the serialization authority
`COMPATIBILITY_POLICY.md` §"Where each contract is specified" points at. ADR-0007 is the
*deciding* record (written before the code existed, and marked so); this file is the
*descriptive* one: what the code actually writes and reads today, each claim cited to the
source and the test that pins it. When the two disagree, the code and its tests win and the
drift is reported (C6) — none is known at the time of writing.

**Everything in this file is frozen contract** from the first build that leaves the
repository (`COMPATIBILITY_POLICY.md`, `SESSION_COMPATIBILITY_POLICY.md`): additions must
tolerate absence, removal is prohibited, and any change here is a **serialization schema
change — an AI-agent Hard Stop** (`ARCHITECTURE_REVIEW_GATE.md`), enacted only by a
superseding ADR.

**Two removals and two additions have been enacted that way**, while the window described in
that first sentence is still open. Removed: `int_meterTargets` on 2026-08-05 with the
streaming-target display, under
[**ADR-0015**](design-decisions/ADR-0015-pre-ship-contract-refreeze.md); and `int_tpMeterOn` in
0.1.1 under [**ADR-0020**](design-decisions/ADR-0020-waveform-statistics-panel.md), because the
statistics panel shows the true peak unconditionally and a field whose only job was hiding one
row had nothing left to gate. Added by that same record: `int_integratedStd` and `int_rmsRef`,
which select which STANDARD two shown readings follow (BS.1770-2+ gated vs -1 ungated; AES-17 vs
mathematical RMS) — never whether a reading appears, which is the distinction the removed toggle
failed to draw. Net, the count below reads **ten**, which is the number `ADR-0007`/`ADR-0010`
decided arrived at by a different route. A Serialization Registry change is an
`ARCHITECTURE_REVIEW_GATE.md` item and a Hard Stop, and **the owner cleared that gate on
2026-08-06** for both records, signing the field changes by name; the sign-offs are quoted in
each ADR's Status banner, which is the record of authority for them. The migration is §2's
read rules doing their ordinary job: an older blob carrying the property loads with every other
field intact and the unknown one ignored, and the writer emits the schema rather than the input,
so it does not survive a re-save. No legacy read path is owed, because no build carrying the
field has left the repository.

**A third addition landed the same way on 2026-08-08**: the preset-identity trio
`presetSource` / `presetFactoryId` / `presetUserFile` on the `SLOT` unit, under
[**ADR-0022**](design-decisions/ADR-0022-preset-identity.md) — the owner cleared the gate in
writing, approving exactly that trio and its defaults-on-absence. It touches no `int_*` count:
the fields live on the slots, and absence decodes to the pre-ADR-0022 name-fallback behaviour,
so an older blob loads unchanged. See §1.2.

There are **two formats**, deliberately different in fidelity (ADR-0007 option H):

| Format | Fidelity contract | Carrier |
|---|---|---|
| Host session blob | **raw-exact** — a host round-trip restores the exact normalised value, mid-step positions included | `getStateInformation` / `setStateInformation` |
| `.anabasis` preset file | **snap-equivalent** — snapped denormalised values only | `PresetManager::savePreset` / `applyPreset` |

---

## 1. The session blob

`getStateInformation` builds a `juce::ValueTree`, serialises it to XML and wraps it with
`copyXmlToBinary` (`getStateInformation`, `src/PluginProcessor.cpp:1691-1747`). Structure, in write order:

```
AnabasisRoot                      schemaVersion = 1 (int; kSchemaVersion, src/PluginProcessor.cpp:10)
├── ANABASIS                      the APVTS tree — the LIVE parameter surface
│   └── PARAM ×50                 id · value (denormalised) · raw (normalised double, additive)
│                                 (×49 until 0.1.1 — ADR-0019 ADDED compStereoLink, an
│                                  additive change under the §2 missing-field rule: an old
│                                  session simply loads the default 100 %, no migration)
├── ANABASIS_INTERNAL             host-hidden session state (10 int_* properties)
├── AB                            active = 0|1
│   ├── SLOT                      (slot 0)
│   │     presetName (string)
│   │     presetSource (string)      "" | "factory" | "user"    [ADR-0022]
│   │     presetFactoryId (string)   factory id, else ""        [ADR-0022]
│   │     presetUserFile (string)    file name or path, else "" [ADR-0022]
│   │     ├── ANABASIS            full parameter tree copy, raw attributes included
│   │     ├── BASELINE            [optional — see §1.4]
│   │     ├── FROZEN_TRIMS        [only while Freeze is ON — see §1.3]
│   │     └── DETACH_MASK         PARAM(id) ×N — always written, possibly empty
│   └── SLOT                      (slot 1, same shape)
└── ADAPTIVE                      [only once Learn has committed — see §1.5]
      refOnsetRate · refTiltDb (doubles)
```

### 1.1 The `raw` attribute (host-session contract)

Each `PARAM` node carries APVTS's denormalised `value` **plus** an additive `raw` attribute
holding the exact normalised position (`copyStateWithRaw`, `src/PluginProcessor.cpp:1012`;
the Anamorph ADR-0013 pattern, adopted by our ADR-0007). Restore prefers `raw` — clamped to
[0, 1] at the boundary — and falls back to `value` when `raw` is absent
(`reassertFromRaw`, `src/PluginProcessor.cpp:1279-1313`). **A `raw` that is not finite takes that
same absent-attribute fallback**, and the test is a finite one rather than a tighter clamp because
a clamp cannot do it: every clamp the value would meet is comparison-based and every comparison
against a NaN is false — `juce::jlimit` here, `NormalisableRange::snapToLegalValue` on the preset
path (§2), and Steinberg's own `Parameter::setNormalized` last of all — so a NaN admitted at this
boundary reaches the host through `performEdit` and is written back by the next save. Same read
rule, and the same shape, as the trims' finite check in the tolerance table below. This is what makes a mid-step position
on a discrete `Raw*` parameter survive a host round-trip bit-exactly.
Pinned by: `testStateRoundTrip` (byte-identical `getState → setState → getState`),
`testAbRawExact`, `testRawRoundTripIsIdempotent` (`tests/state_tests.cpp`).

### 1.2 The `SLOT` unit — StateSet on disk

A slot serialises the **widened StateSet** `{params, presetName, baseline, frozenTrims,
detachMask}` (ADR-0007) — plus, since **ADR-0022**, the preset-identity trio described below —
via `saveSlotFromLive()` (`src/PluginProcessor.cpp:1153-1220`). Two
properties of the shape that are rules, not accidents:

- **The slot's `ANABASIS` copy carries the full surface, view-tier entries included.** The
  "view state never travels" rule lives entirely on the **apply** side —
  `applySlotToLive` overwrites the view-tier entries from the live state before adopting.
  Any future path that adopts a slot tree without going through `applySlotToLive`
  re-introduces view-tier travel (the function's own comment carries this warning).
- **`DETACH_MASK` is always written**, even empty — its children are `PARAM` nodes with an
  `id` property each, one per §5.3-detached parameter. On read, a missing mask means
  all-clear (the missing-fields rule).

**The preset-identity trio** (`presetSource` / `presetFactoryId` / `presetUserFile`,
**ADR-0022** — the gate for this addition was cleared by the owner on 2026-08-08; the sign-off
and its non-transferable scope are quoted in that ADR's Status banner):

| field | type | default if absent | meaning |
|---|---|---|---|
| `presetSource` | string `""` / `"factory"` / `"user"` | `""` → identity `unknown` | which namespace produced this slot's sound |
| `presetFactoryId` | string | `""` | the immutable factory id (`presetSource == "factory"`) |
| `presetUserFile` | string | `""` | the user preset file (`presetSource == "user"`) |

Rules the shape encodes, each pinned by `testPresetIdentityAcrossRestore`:

- **Metadata only.** Written by `saveSlotFromLive` beside `presetName`, decoded after the
  parameter adopt; no fallback path touches a parameter — the sound restores bit-identically
  whether or not the identity resolves.
- **Absence decodes to `unknown`**, which selects the name fallback — the pre-ADR-0022
  behaviour, so an older blob loads unchanged (`SESSION_COMPATIBILITY_POLICY` rule 2). A
  known identity that no longer resolves ticks **no row**, never a same-named substitute.
- **`presetUserFile` holds the bare file name** only when the preset sits **directly in**
  `PresetManager::userPresetDirectory()` (a direct-child test, deliberately not the recursive
  `juce::File::isAChildOf`) **and** its name is not something `juce::File::isAbsolutePath`
  accepts (a leading `~` on POSIX). Everything else — outside the folder, nested in a
  sub-folder, or a path-like name — stores the absolute path, so `decode(encode(s)) == s`
  holds. Identity matching is a path-string compare with **no canonicalisation** — but
  `juce::File::operator==` is case-INSENSITIVE on Windows and macOS, so a differently-cased
  spelling matches there (the same file, the answer wanted) while every other re-spelling
  misses on all three platforms and shows no tick: the safe direction, a miss rather than a
  wrong row.
- **User preset FILES are untouched** — the trio lives in the session blob only; the
  `.anabasis` format is unchanged (§3).
- **DISCLOSURE OBLIGATION — a saved session can contain an absolute filesystem path.** Stated here
  because this repository has no user-facing privacy document yet and, under constraint C8 /
  OQ-002, one cannot be written with invented owner wording. The fact is recorded at the schema it
  arises from so that whoever writes that document cannot write it without this paragraph. The
  disclosable content, exactly:
  - **How many references, and where.** The trio is written on the `SLOT` unit and nowhere else,
    so a session carries **at most two** preset references — one per A/B slot. There is no
    root-level copy.
  - **When a PATH rather than a name is stored.** Only for `presetSource == "user"`, and only in
    the complement of the bare-name case above: a preset outside `userPresetDirectory()`, one
    nested in a sub-folder of it, or one whose own file name is something
    `juce::File::isAbsolutePath` accepts. An absolute path typically contains the user's account
    name, which is the privacy-relevant part.
  - **How long it persists.** For as long as the host keeps the session. The INACTIVE slot's copy
    outlives its use: `getStateInformation` writes that slot verbatim from `storedSlot`, so a
    reference put there by an earlier A/B state is re-emitted on every subsequent save until that
    slot is overwritten by a new sound. Switching away from a preset does not clear it.
  - **What is NOT stored.** No file CONTENT, and nothing about presets other than the one a slot
    holds. Factory presets store an id, never a path.
- **On the INACTIVE slot, absence survives a re-save** — a deliberate, narrow exception to
  this file's "the writer emits the schema rather than the input" pattern. `getStateInformation`
  rebuilds the ACTIVE slot through `saveSlotFromLive()` (which always writes the trio) but
  writes the inactive slot verbatim from `storedSlot`, and a pre-ADR-0022 session's stored
  SLOT carries no trio — so a load→re-save of such a session emits the trio on the active
  slot only, until a slot switch rebuilds the stored tree. Harmless by construction: absence
  and the empty trio decode to the same `unknown`, and the undo change-guard normalises the
  two before comparing (ADR-0022 §Consequences), so no behaviour depends on the difference.

### 1.3 `FROZEN_TRIMS` — written conditionally, by design

Properties: `releaseOctaves`, `stereoLink`, `scHpfHz`, `dynTiltDb` (doubles;
`saveSlotFromLive`, `src/PluginProcessor.cpp:1146-1149`). Three rules the shape encodes:

- **Freeze OFF ⇒ no child at all.** A slot that is not frozen has nothing latched; writing
  a stale vector was a live defect (it flipped the preset-dirty mark) and is now pinned
  against (`testFrozenSlotRoundTrip`'s freeze-OFF check).
- **The capture follows the mirror rule**: with a restore staged but not yet applied (a
  load→save with no audio between), the staged copy is serialised, not the engine's stale
  published trims — the same rule `ADAPTIVE` follows (`saveSlotFromLive`'s ADR-0014 block).
- **Restore is audio state**: on load the vector is staged on the ADR-0012 bounded record
  and injected via `AdaptiveEngine::injectTrims` — per-field finite-checked and clamped at
  the boundary — at the §2.8 duck's silent bottom or the unprimed direct-adopt, only for a
  freeze-ON adopted surface (ADR-0014).
  Pinned by: `testFrozenTrimRestore` (every element mutation-killed; ADR-0014 enumerates),
  `testAFrozenLatchDoesNotFollowTheSlotSwitch`, `testAPresetApplyKeepsTheFrozenLatchItDidNotChange`,
  and `AnabasisTests`' `testHostileFrozenTrimsCannotEnterTheAdaptiveState`.

### 1.4 `BASELINE` — schema-reserved, currently adopted-only

The §5.3 macro-baseline child ADR-0007 gave a per-slot home. **No code path in this build
originates one**: the only constructor of a `BASELINE` tree in the whole repository is the
test that seeds one (`tests/state_tests.cpp:2266`). The wrapper *adopts* the child from an
incoming slot or session, *carries* it through A/B, undo and saves
(`src/PluginProcessor.cpp:1176-1177, 1435, 1878` — `saveSlotFromLive`, `applySlotToLive`,
`setStateInformation`), and *drops* it where the state it describes
is replaced — both preset-apply paths and the defaults-based restore
(`src/PluginProcessor.cpp:1556, 1643, 1676` — `applyFactoryPreset`, `applyPresetFile`,
`resetSlotFieldsToDefaults`). So the child is live schema with defined
carriage semantics and no producer — a reader must tolerate it, a writer must not invent
one. Pinned by: `testAPresetApplyDropsTheMacroBaselineOnBothPaths`.

### 1.5 `ADAPTIVE` — absent = never learned

Written **only** after a Learn commit; its absence is the discriminator, so an
instance that never learned writes no child and a loaded reference is never resurrected
from defaults (`getStateInformation`'s ADAPTIVE block, `src/PluginProcessor.cpp:1729-1743`). While a loaded restore is staged but
unconsumed, the staged values are authoritative for a re-save (the mirror rule); the
residual one-save window between the consumer's `exchange` and its adoption is documented
at the site and in ADR-0012 §Known limits, not claimed away.
Scope is **global** (the learned reference describes the material, not a slot) — per-slot
placement was rejected for the trims, not for this (ADR-0007 §Options D).

### 1.6 `ANABASIS_INTERNAL`

> **One field's MEANING changed, 2026-08-05 — recorded here because this ledger treats a semantic
> change like a removal.** `int_spectrumOn` used to mean *"does the spectrum take half of the
> Advanced metering strip?"* (the GR trace was never hidden by it, and the Simple view did not
> read it at all). It now means *"which of the two graph-well views is active"*, in **both**
> editor modes — `true` spectrum, `false` GR history.
> [**ADR-0016**](design-decisions/ADR-0016-spectrumon-becomes-the-graph-well-mode.md) carries the
> decision, the before/after table and the rejected alternatives.
>
> **No migration is owed and none exists**: the type (`bool`) and default (`true`) are unchanged,
> so the defaults-first overlay below reads an old blob exactly as it always did. What differs is
> what the same stored value *shows* — `false` displays what it always displayed, `true` now
> hides the GR trace that used to sit beside it. Display-only, one click to change, and free
> while the pre-ship window is open.
>
> **0.1.2 flips the DEFAULT to `false`** — GR is the graph well's default view (owner directive
> item 4, [**ADR-0023**](design-decisions/ADR-0023-012-field-fix-contracts.md)). A default-only
> change under this ledger's rules: the type and the ADR-0016 semantics are untouched, a stored
> value still wins over the default on every load, so only a fresh instance (or a blob without
> the field) reads differently — it opens on the GR history instead of the spectrum.
>
> **Gate CLEARED 2026-08-06**, separately from ADR-0015's: the owner's confirmation names the
> semantic change, the decision to keep it a pre-1.0 migration change, and the acceptance that
> stored values load with no migration path.
>
> **A SECOND field's accepted DOMAIN changed in the same batch**, and that one is still open.
> `int_uiScale`'s ladder narrowed from seven steps (80/90/100/125/150/175/200) to five
> (75/85/100/125/150). Type, unit and default are unchanged — it is still a percent, 100 is still
> the default — but the ladder is part of this field's read contract (§1.6's normalisation note
> and §2's out-of-range row both name it), so a stored **80 → 75, 90 → 85, 175/200 → 150**, and
> because the normalisation happens at adoption the corrected percent is what the next save
> writes. 100/125/150 are common to both ladders and survive untouched.
> [**ADR-0017**](design-decisions/ADR-0017-uiscale-ladder-narrowing.md) carries it, and **its gate
> was cleared 2026-08-06** — separately again, the owner's confirmation naming the reduced ladder,
> the acceptance that out-of-set stored values normalise on adoption, and that this is a pre-1.0
> decision with no released-session migration obligation.

The host-hidden fields (`src/InternalState.h` — inventory in
`PARAMETER_REGISTRY.md` §Host-hidden state). Read by `InternalState::replaceFrom`:
**defaults first**, then overlay only properties the schema knows (unknown ignored), the
`int_uiScale` ladder normalisation applied at adoption with the default as the read
fallback, and the whole read coalesced into **one** latency notification
(`ScopedLatencyBatch`). Never in A/B, undo or presets.
Pinned by: the batched-latency, ADAPTIVE-missing-field and `uiScaleClamp` tests.

### 1.7 What is deliberately NOT serialized

- **The per-slot undo/redo stacks** (cap `kUndoCap = 128`,
  `src/PluginProcessor.h:457`) — session-local; a load announces a fresh history via
  `historyEpoch` and the message thread clears at `syncHistory()`
  (`testHistoryOwnershipAcrossAStateLoad`).
- **`presetBaseline`** (the dirty-marker comparand, a `PRESET_SHAPE` projection) — rebuilt,
  never persisted; deliberately outside the StateSet (`src/PluginProcessor.h:435-443`).
- Gesture bookkeeping, meter holds (a load *clears* holds via `requestMeterReset`), and
  every GUI-only datum.

---

## 2. Read rules (structural tolerance — ADR-0007 option C)

`setStateInformation` (`src/PluginProcessor.cpp:1749-1946`), in order:

| Input | Behaviour |
|---|---|
| Undecodable blob | **No-op** — current state kept (`testCorruptAndForeignState`) |
| Foreign root tag | **No-op** — current state kept |
| `schemaVersion` missing | Treated as 1 |
| `schemaVersion` > 1 | **Not a rejection** — the reader falls back to shape. A future contributor adding a version gate to the read path is reversing ADR-0007 (its §Consequences says exactly this) |
| Missing `ANABASIS` child | **Defaults**, not "keep live" — a valid root that omits the surface means the default surface. **This now defaults the ACTIVE SLOT'S FIELDS too**, and that is a widening of the rule worth stating rather than deducing from the row below: `presetName`, the ADR-0022 identity trio, `BASELINE`, `FROZEN_TRIMS` and `DETACH_MASK` are adopted from the active `SLOT` only when the root surface was restored, so a blob carrying a full `AB` block under a root with no `ANABASIS` loads with the DEFAULT name, identity, baseline, trims and an EMPTY detach mask, where it previously adopted all five. Deliberate, and the §5.3 detach mask is the case that has to be looked at rather than waved through: detachment is not otherwise recoverable, so dropping it loses user intent. It is dropped anyway, because the alternative loses more — a mask names parameters that are detached from the MACRO SURFACE, and the surface this restore installed came from defaults, so the mask would describe detachments from a mapping the session never had. A shape with no producer: nothing this plug-in writes omits the root `ANABASIS` while writing `AB`, so no session in the wild takes this path |
| Missing `ANABASIS_INTERNAL` / missing fields | Defaults first, overlay what exists (§1.6) |
| Missing / partial `AB` | `resetSlotFieldsToDefaults()` first, then overlay; `active` clamped through `anabasis::clampAbSlotIndex`; **SLOT children collected by type, never by index**, so a tolerated foreign child cannot shift both slots (`src/PluginProcessor.cpp:1819`, the `hasType ("SLOT")` filter) |
| `SLOT` present but carrying **no `ANABASIS` child** | **The whole slot resolves to defaults**, and the two slots reach that by different routes because they are not symmetric. The STORED slot is declined outright and keeps the `defaultSlot` planted by `resetSlotFieldsToDefaults()`, because `storedSlot` is a processor member that survives across restores — accepting a payload-less tree there would leave the PREVIOUS project's sound under this project's name, since `applySlotToLive` adopts parameters only when the payload is valid but adopts `presetName`, the identity trio, `BASELINE`, `FROZEN_TRIMS` and `DETACH_MASK` unconditionally. The ACTIVE slot needs no such test for its sound (that comes from the ROOT `ANABASIS`, not from the slot's redundant copy), so instead its METADATA is adopted only when the root surface was actually restored — otherwise the surface came from defaults and the labels would describe a sound that was never installed. The consequence to read off that, because it is the shape a reviewer expects to be symmetric and is not: a blob with a valid ROOT surface whose ACTIVE slot has lost its payload still adopts that slot's name, identity, baseline, trims and mask — the metadata is gated on the root surface, never on the active slot's own `ANABASIS` child. Defensible (both halves came out of the same blob, and the sound that was installed is the one the root described), and it is why the two slots are governed by two different tests rather than one. One rule, stated per slot ([**ADR-0026**](design-decisions/ADR-0026-slot-payload-read-rules.md) — Accepted 2026-08-14, the owner cleared the gate on this semantic change): **metadata is adopted only alongside the parameters it describes** (`src/PluginProcessor.cpp:1802` the `liveSurfaceRestored` flag, `:1693` the stored-slot guard, `:1712` the active-slot gate; pinned by `testAMalformedStoredSlotCannotSplitSoundFromMetadata`) |
| Unknown properties/children anywhere | Ignored, and **not** preserved on re-save (the writer emits the schema, not the input) |
| Out-of-range values | Clamped at the read boundary (`raw` to [0,1]; indices through their clamps; trims per-field finite-checked in `injectTrims`; `int_uiScale` to the ladder) |
| **Unusable (non-finite) numbers** | Declined, never clamped — a comparison-based clamp cannot reject a NaN. `raw` falls back to `value` (§1.1); a preset's `value` is skipped like an unknown id (§2); trims and the learned targets take their seed (`injectTrims` and `setLearnedTargets`, `src/dsp/AdaptiveEngine.h`). Pinned by `testAWellFormedDocumentCannotCarryAnUnusableNumber` |

Every load: requests the §2.8 forced duck (a session load is the biggest bulk swap),
bumps `historyEpoch`, clears all three gesture-state members, requests the meter reset,
and runs under `MacroEngine::ScopedRestore` (the KI-003 off-message-thread premise).
Pinned by: `testAbToleranceRules`, `testCorruptAndForeignState`,
`testStateReplacementAndHistoryConsistency`.

---

## 3. The `.anabasis` preset file

Plain XML, written by `PresetManager::savePreset` (`src/PresetManager.cpp:4-30`) into
`<userAppData>/RollyTech/Anabasis/Presets` (`userPresetDirectory()`,
`src/PresetManager.h:30-34`):

```xml
<AnabasisPreset schemaVersion="1">
  <PARAM id="…" value="…"/>      <!-- snapped denormalised; NO raw attribute -->
  …
  <DETACH_MASK>
    <PARAM id="…"/>              <!-- the §5.3 mask travels in presets (ADR-0007 option G) -->
  </DETACH_MASK>
</AnabasisPreset>
```

- **Content = the non-excluded parameters + the mask, nothing else.** The exclusion set is
  `isPresetExcludedParam` = view tier (`bypass`, `loudnessComp`, `deltaMonitor`,
  `advancedMode`) ∪ {`freeze`} — one predicate pair shared with the A/B tier logic
  (`src/PluginParameters.cpp`). No `BASELINE`, no `FROZEN_TRIMS`, no
  `ANABASIS_INTERNAL`, no A/B structure.
- **One traversal for writer and dirty marker.** `forEachPresetParameter` (id-sorted,
  preserving the shipped byte layout) is the single walk both `savePreset` and
  `presetShapeFromLive()` use, so the file's content and the `*` marker's datum cannot
  diverge (`testThePresetWriterAndTheDirtyMarkerCoverTheSameParameters`,
  `testTheDirtyMarkerMeasuresOnlyWhatAPresetCanCarry`).
- **Values are snapped through the range** (`presetValueOf` — `snapToLegalValue` on the
  denormalised value): the preset contract is snap-equivalence, not raw-exactness. The two
  registry defaults that are taper images of round values (`PARAMETER_REGISTRY.md`
  §Conventions) are therefore legal file content, not defects.
- **Apply**: refuses a foreign root (`AnabasisPreset` checked), parses **once** and shares
  the parsed document between the readability gate and the apply (a file rewritten between
  two parses applied different content — round 50); a **locked ceiling is skipped**, never
  written-then-reverted; missing mask = all-clear. Host notification happens only for
  values that actually move (`setParamIfMoved`).
  Pinned by: `testPresetContract`, `testFactoryPresets`.
- **The two apply paths differ on omissions, and the difference is worth stating
  precisely.** A **factory** apply is defaults-then-overrides (`applyFactoryPreset` lands
  the default patch first — "an override TABLE is defaults + intents"), so anything a table
  omits IS at its default. A **file** apply is **overlay-only**: `applyPreset` writes
  exactly the `PARAM` elements the file carries (`src/PresetManager.cpp:108-110`) with no
  defaults pass, so a parameter the file omits keeps its **current live value** — unlike a
  session load, which is defaults-first (§2). Today this distinction is invisible: the
  writer emits every non-excluded parameter, so a current-version file is a full surface
  replacement. It becomes visible the first time a build adds a parameter and reads an
  older file — the new parameter stays where it was, it does not reset. Whether that is
  the wanted semantics is a decision the moment it matters; recording it here is what
  keeps it from being *discovered* then.
- **Factory presets** are compiled-in override tables (`kFactory`,
  `src/PresetManager.cpp:208-221`) expressed through the same shared lock/exclusion core —
  a table is defaults + intents, never a file.

---

## 4. Fixtures — what exists and what is owed

`tests/fixtures/` holds **`parameter_registry.snapshot`** (the frozen parameter surface,
re-frozen only via `--write-snapshot`; `testRegistrySnapshot`). There is **no frozen legacy
session fixture yet, and that is correct**: nothing has ever shipped, so no historical
format exists to freeze (`DEVELOPMENT_BRIEF.md` §23's delta table records this
deliberately). **The moment v0.1.0 is tagged, a session blob and a preset file from that
build must be frozen as fixtures** — with a frozen slot and a non-clear mask, or the two
ADR-0007 additions are exercised vacuously (its §Consequences states the same) — and every
later read path change must keep them loading. That step belongs to the release checklist,
not to this file.

---

## Changing anything here

| Change | Cost |
|---|---|
| New optional child/property (absence-tolerated) | ADR + this file + a read-rule test |
| Any rename, removal, or meaning change | **Hard Stop** — serialization schema change (`ARCHITECTURE_REVIEW_GATE.md`) |
| A version gate in the read path | Reverses ADR-0007 — superseding ADR required |
| First shipped build | Freezes all of the above; freeze the §4 fixtures in the same release |
