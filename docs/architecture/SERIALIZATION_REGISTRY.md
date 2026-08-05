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

There are **two formats**, deliberately different in fidelity (ADR-0007 option H):

| Format | Fidelity contract | Carrier |
|---|---|---|
| Host session blob | **raw-exact** — a host round-trip restores the exact normalised value, mid-step positions included | `getStateInformation` / `setStateInformation` |
| `.anabasis` preset file | **snap-equivalent** — snapped denormalised values only | `PresetManager::savePreset` / `applyPreset` |

---

## 1. The session blob

`getStateInformation` builds a `juce::ValueTree`, serialises it to XML and wraps it with
`copyXmlToBinary` (`src/PluginProcessor.cpp:1068-1124`). Structure, in write order:

```
AnabasisRoot                      schemaVersion = 1 (int; kSchemaVersion, PluginProcessor.cpp:7)
├── ANABASIS                      the APVTS tree — the LIVE parameter surface
│   └── PARAM ×49                 id · value (denormalised) · raw (normalised double, additive)
├── ANABASIS_INTERNAL             host-hidden session state (10 int_* properties)
├── AB                            active = 0|1
│   ├── SLOT                      (slot 0)
│   │     presetName (string)
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
holding the exact normalised position (`copyStateWithRaw`, `src/PluginProcessor.cpp:568`;
the Anamorph ADR-0013 pattern, adopted by our ADR-0007). Restore prefers `raw` — clamped to
[0, 1] at the boundary — and falls back to `value` when `raw` is absent
(`reassertFromRaw`, `src/PluginProcessor.cpp:811-…`). This is what makes a mid-step position
on a discrete `Raw*` parameter survive a host round-trip bit-exactly.
Pinned by: `testStateRoundTrip` (byte-identical `getState → setState → getState`),
`testAbRawExact`, `testRawRoundTripIsIdempotent` (`tests/state_tests.cpp`).

### 1.2 The `SLOT` unit — StateSet on disk

A slot serialises the **widened StateSet** `{params, presetName, baseline, frozenTrims,
detachMask}` (ADR-0007) via `saveSlotFromLive()` (`src/PluginProcessor.cpp:695-752`). Two
properties of the shape that are rules, not accidents:

- **The slot's `ANABASIS` copy carries the full surface, view-tier entries included.** The
  "view state never travels" rule lives entirely on the **apply** side —
  `applySlotToLive` overwrites the view-tier entries from the live state before adopting.
  Any future path that adopts a slot tree without going through `applySlotToLive`
  re-introduces view-tier travel (the function's own comment carries this warning).
- **`DETACH_MASK` is always written**, even empty — its children are `PARAM` nodes with an
  `id` property each, one per §5.3-detached parameter. On read, a missing mask means
  all-clear (the missing-fields rule).

### 1.3 `FROZEN_TRIMS` — written conditionally, by design

Properties: `releaseOctaves`, `stereoLink`, `scHpfHz`, `dynTiltDb` (doubles;
`src/PluginProcessor.cpp:687-692`). Three rules the shape encodes:

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
test that seeds one (`tests/state_tests.cpp:1454`). The wrapper *adopts* the child from an
incoming slot or session, *carries* it through A/B, undo and saves
(`src/PluginProcessor.cpp:708-709, 851, 1208`), and *drops* it where the state it describes
is replaced — both preset-apply paths and the defaults-based restores
(`src/PluginProcessor.cpp:972, 1039, 1053`). So the child is live schema with defined
carriage semantics and no producer — a reader must tolerate it, a writer must not invent
one. Pinned by: `testAPresetApplyDropsTheMacroBaselineOnBothPaths`.

### 1.5 `ADAPTIVE` — absent = never learned

Written **only** after a Learn commit; its absence is the discriminator, so an
instance that never learned writes no child and a loaded reference is never resurrected
from defaults (`src/PluginProcessor.cpp:1083-1120`). While a loaded restore is staged but
unconsumed, the staged values are authoritative for a re-save (the mirror rule); the
residual one-save window between the consumer's `exchange` and its adoption is documented
at the site and in ADR-0012 §Known limits, not claimed away.
Scope is **global** (the learned reference describes the material, not a slot) — per-slot
placement was rejected for the trims, not for this (ADR-0007 §Options D).

### 1.6 `ANABASIS_INTERNAL`

The ten host-hidden fields (`src/InternalState.h` — inventory in
`PARAMETER_REGISTRY.md` §Host-hidden state). Read by `InternalState::replaceFrom`:
**defaults first**, then overlay only properties the schema knows (unknown ignored), the
`int_uiScale` ladder normalisation applied at adoption with the default as the read
fallback, and the whole read coalesced into **one** latency notification
(`ScopedLatencyBatch`). Never in A/B, undo or presets.
Pinned by: the batched-latency, ADAPTIVE-missing-field and `uiScaleClamp` tests.

### 1.7 What is deliberately NOT serialized

- **The per-slot undo/redo stacks** (cap `kUndoCap = 128`,
  `src/PluginProcessor.h:388`) — session-local; a load announces a fresh history via
  `historyEpoch` and the message thread clears at `syncHistory()`
  (`testHistoryOwnershipAcrossAStateLoad`).
- **`presetBaseline`** (the dirty-marker comparand, a `PRESET_SHAPE` projection) — rebuilt,
  never persisted; deliberately outside the StateSet (`src/PluginProcessor.h:296-306`).
- Gesture bookkeeping, meter holds (a load *clears* holds via `requestMeterReset`), and
  every GUI-only datum.

---

## 2. Read rules (structural tolerance — ADR-0007 option C)

`setStateInformation` (`src/PluginProcessor.cpp:1126-…`), in order:

| Input | Behaviour |
|---|---|
| Undecodable blob | **No-op** — current state kept (`testCorruptAndForeignState`) |
| Foreign root tag | **No-op** — current state kept |
| `schemaVersion` missing | Treated as 1 |
| `schemaVersion` > 1 | **Not a rejection** — the reader falls back to shape. A future contributor adding a version gate to the read path is reversing ADR-0007 (its §Consequences says exactly this) |
| Missing `ANABASIS` child | **Defaults**, not "keep live" — a valid root that omits the surface means the default surface |
| Missing `ANABASIS_INTERNAL` / missing fields | Defaults first, overlay what exists (§1.6) |
| Missing / partial `AB` | `resetSlotFieldsToDefaults()` first, then overlay; `active` clamped through `anabasis::clampAbSlotIndex`; **SLOT children collected by type, never by index**, so a tolerated foreign child cannot shift both slots (`src/PluginProcessor.cpp:1188-1200`) |
| Unknown properties/children anywhere | Ignored, and **not** preserved on re-save (the writer emits the schema, not the input) |
| Out-of-range values | Clamped at the read boundary (`raw` to [0,1]; indices through their clamps; trims per-field finite-checked in `injectTrims`; `int_uiScale` to the ladder) |

Every load: requests the §2.8 forced duck (a session load is the biggest bulk swap),
bumps `historyEpoch`, clears all three gesture-state members, requests the meter reset,
and runs under `MacroEngine::ScopedRestore` (the KI-003 off-message-thread premise).
Pinned by: `testAbToleranceRules`, `testCorruptAndForeignState`,
`testStateReplacementAndHistoryConsistency`.

---

## 3. The `.anabasis` preset file

Plain XML, written by `PresetManager::savePreset` (`src/PresetManager.cpp:4-30`) into
`<userAppData>/RollyTech/Anabasis/Presets` (`presetDirectory()`,
`src/PresetManager.h:32-33`):

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
