# ADR-0022 — Preset identity: factory ids + user files, carried on the `SLOT` unit

**Status:** Accepted (2026-08-08 — the owner's explicit approval of the migration plan:
"Approved. Proceed. Implement only the approved preset-identity migration plan. Before changing
code, add/update the required ADR and serialization registry entries required by Anabasis
governance.")

> **ARCHITECTURE REVIEW GATE — CLEARED (2026-08-08).** A **Serialization Registry change**:
> three additive string properties — `presetSource`, `presetFactoryId`, `presetUserFile` — on
> the `SLOT` unit (so they appear twice per session blob, once per A/B slot). Nothing is
> removed and no existing field changes meaning; absence decodes to identity `unknown`, which
> is byte-for-byte this build's pre-change behaviour (the name fallback). The owner reviewed
> the migration plan naming exactly this trio, its defaults-on-absence, and the constraint
> that user preset files stay byte-identical, and approved it in writing on 2026-08-08.
> **Scope of this sign-off:** it covers *these* fields in *this* change set. It is not a
> standing approval — `ARCHITECTURE_REVIEW_GATE.md` still classifies every future
> Serialization Registry change as a gated item and an AI-agent Hard Stop, and the next one
> needs its own review and its own record (the same bound Anamorph's ADR-0024 places on its
> own serialization sign-off).

## Context

The preset browser shows one flat list: the compiled-in FACTORY presets first
(`src/PresetManager.cpp`, `kFactory[]`), then the USER `.anabasis` files found in the local
preset folder. Nothing prevents a user preset from being saved under a factory preset's name —
"Save Preset…" pre-fills the current name, so saving straight after loading *EDM Club* produces
exactly that.

Until this change the current preset existed only as a display string (`livePresetName` on the
processor), and every consumer resolved it by NAME:

- the drop-down tested **each row independently** against the name
  (`src/gui/PluginEditor.cpp`, `showPresetMenu`), so a shared name ticked **both** rows;
- `‹ ›` stepping re-derived its position from the name, patched by an editor-local
  "remember preset source" hint (`rememberPresetSource`) that dies with the window and needs
  the name to confirm it — which is the very thing that fails on a clash;
- a factory preset had no identity at all beyond its position in `kFactory[]`;
- a reloaded session carried the name alone, so it could not restore *which* of two same-named
  presets was selected.

The sibling product **Anamorph** solved this in its 0.9.2 (its ADR-0024 as amended, merged in
its PR #100): a factory preset is identified by an immutable internal id, a user preset by its
file on disk, the identity travels with everything the preset name travels with — including the
saved session — and anything the stored identity cannot resolve ticks **nothing**, never a
same-named substitute. That design is implemented, reviewed and pinned by tests there. This ADR
is the **product-family reuse decision ADR-0009 requires** for porting it: a design port, not a
patch port, because the two architectures carry preset state differently (see §Decision 6).

## Problem

With a duplicate name: the tick sits on both rows at once; `‹ ›` steps from whichever row the
name scan hits first (the factory block is list-front); saving a user preset over a factory name
leaves the indicator claiming the factory row too; and reopening a project cannot restore which
of the two was selected. The name is a *label* being used as an *identity*, and the two
namespaces (a fixed built-in table, an arbitrary user folder) are not disjoint.

## Options

- **A. Forbid the collision** (reject or auto-rename the save). Rejected — takes a naming
  decision away from the user to work around an internal representation (Anamorph ADR-0024 §A).
- **B. Tie-break the name scan.** Rejected — whichever side wins, the other becomes
  unreachable; it moves the bug (Anamorph ADR-0024 §B).
- **C. Port Anamorph's identity model, in-memory only** (its original pre-amendment shape).
  Rejected — impossible here without inventing a parallel carrier: in Anabasis the undo/A-B
  unit **is** the serialized `SLOT` tree, so an identity that travels with undo/A-B is in the
  session blob by construction. A second, non-serialized carrier (the `presetBaseline` pattern)
  would be new architecture Anamorph never validated.
- **D. Port the amended design onto the `SLOT` unit.** Chosen. Three additive strings beside
  `presetName`, the one place slot metadata already lives — undo, A/B, Copy and the session all
  inherit the identity from the existing slot-tree plumbing with no new mechanism.

## Decision

1. **Identity, not name.** `PresetManager::FactoryPreset` gains an immutable internal `id`
   string; `PresetManager::Selection` records what produced the current sound — a factory id
   (`Kind::factory`), a user file (`Kind::userFile`), or `Kind::unknown`. Names remain what is
   displayed everywhere (menu, top bar, Save pre-fill); the id never surfaces in the UI.
2. **Factory ids are immutable.** They are compile-time strings, not table positions:
   reordering or renaming the table re-points nothing. Renaming a *preset* is a display change;
   renaming an **id** is not permitted — live A/B slots, undo entries and saved sessions may
   still hold the old one.
3. **Resolution is identity-first, and a known-but-absent identity ticks NOTHING.**
   `PresetManager::selectedPresetRow` resolves a factory selection by id and a user selection by
   file against the row list, returns −1 when a *known* identity is absent (a file loaded from
   outside the preset folder; a user preset deleted, renamed or moved; a factory id a later
   version removed), and falls back to the name scan **only** for `Kind::unknown` — state that
   carries no identity at all. Falling through on a known identity is precisely the mis-tick
   this ADR removes (the H1 defect Anamorph's own first implementation had). The name fallback
   keeps the pre-change answer: first matching row, factory block first — a documented
   tie-break for identity-less state, not an accident. Exactly one row is ever ticked.
4. **Saving selects what was written, by file** — saving a user preset under a factory
   preset's name moves the tick to the user row. Both apply paths and the save path set the
   identity beside the name they already set; the editor's `rememberPresetSource` hint is
   deleted, not wrapped: the real identity answers the question the hint approximated.
5. **The identity is metadata, applied after parameters, never influencing them.** The
   restore path decodes it after `adoptParamsTree`; every fallback in the table below restores
   the exact saved sound. Resolve-before-commit already holds on both apply paths (the index is
   validated and the file parsed *before* the undo bracket opens).
6. **The carrier is the `SLOT` unit — three properties, not Anamorph's nine.** Anamorph
   writes a root trio plus one per A/B slot because its root carries the live preset name
   separately from its slots. Anabasis has no root-level preset name: the live state *is*
   `slot[active]`, `saveSlotFromLive()`/`applySlotToLive()` are the one serialisation seam for
   slot metadata, and `getStateInformation` writes the active slot by REBUILDING it through
   `saveSlotFromLive()` and the inactive one verbatim from `storedSlot`. Three additive
   strings on `SLOT` therefore give undo, A/B, Copy **and** the session the identity through
   the existing plumbing — which is also why this is the minimal shape. One consequence of the
   verbatim half is documented in `SERIALIZATION_REGISTRY.md` §1.2: a pre-ADR-0022 session
   re-saved by this build gains the trio on the active slot immediately, while the inactive
   slot's absence survives the re-save until a slot switch rebuilds it — harmless, because
   absence and the empty trio decode identically.
7. **User preset files are untouched.** `.anabasis` files gain no field — `savePreset` is not
   modified. The identity belongs to the *project*: a preset file cannot know which sessions
   reference it, and an id inside a shared or hand-copied file would carry someone else's
   identity (Anamorph ADR-0024 Amendment, property 1).
8. **Wire form.** `encodeSelection`/`decodeSelection` on `PresetManager` are the single place
   that knows the encoding. A user preset sitting **directly in** the preset folder is stored
   as its **file name** — a complete identity there (`refresh`-equivalent scans are
   non-recursive), it keeps the user's home directory out of the saved project, and the
   project resolves on another machine. Everything else stores its absolute path: a file from
   outside the folder, one nested in a **sub-folder** (the direct-child test is
   `getParentDirectory() == userPresetDirectory()`, deliberately not the recursive
   `juce::File::isAChildOf`), and a direct child whose name `juce::File::isAbsolutePath`
   accepts (a leading `~` on POSIX). All three conditions are ports of defects Anamorph's
   reviews found and fixed (its worklog §§7, 9); `decode(encode(s)) == s` is the invariant.
   Identity matching is a path-string compare with no canonicalisation — no symlink
   resolution, no `/private/var`↔`/var` folding, no UNC↔mapped-drive folding. It is not
   case-sensitive everywhere, though: `juce::File::operator==` compares through
   `compareFilenames`, which folds case on Windows and macOS and does not on Linux. So a
   differently-CASED spelling matches on those two platforms — the same file, which is the
   answer wanted — while every other re-spelling of the same file misses on all three and
   shows no tick, which is the safe direction: a miss, never a *wrong* row.

### Fallbacks (all verified by `testPresetIdentityAcrossRestore`)

| stored | on reload | result |
|---|---|---|
| a factory id that still exists | resolved | that factory row is ticked |
| a factory id that no longer exists | not found | **no row ticked** — never a same-named substitute |
| a user file that still exists | resolved | that user row is ticked, even against a same-named factory preset |
| a user file that is gone / moved | not found | **no row ticked** — never the same-named factory row |
| nothing (a pre-change session, or hand-stripped) | `unknown` | the name fallback — this build's pre-change behaviour |

In every row the restored parameters are bit-identical to what was saved — the identity is
metadata and never reaches the sound.

## Consequences

- A user preset may share a factory preset's name; both rows stay individually selectable, and
  `‹ ›` steps from whichever was actually loaded. The UI tells them apart by their
  FACTORY / USER section, since the label is identical by construction.
- An `.anabasis` file loaded from outside the preset folder ticks nothing. Correct: it is on no
  row.
- Cross-machine resolution holds only for the name-encoded case (a preset directly in the
  folder). An absolute path saved on one platform fails `isAbsolutePath` on another, resolves
  to nothing, and ticks nothing — safe, and consistent with the fallback rule.
- **Absent and empty trio compare equal in the undo change-guard.** `strippedForUndoCompare`
  defaults the three properties to `""` before comparing, because the decoder cannot tell them
  apart (both are `unknown`) but `isEquivalentTo` fails on the property count first — without
  the normalisation, the first Copy after loading a pre-ADR-0022 session minted a dead undo
  step from the encoding difference alone. A REAL identity move under an unchanged name and
  sound — the saved-under-a-factory-name shape — still differs and still mints its step;
  both directions are pinned by the Copy cases in `testPresetIdentityAcrossRestore`.
- **The `‹ ›` ring walks on past an entry it cannot apply**, and that is a consequence of
  Decision 3 rather than a refinement of it. The identity moves only on a *successful* apply,
  so a single-shot step would re-derive the same starting row on the next press and re-offer an
  unreadable file for ever — one corrupt `.anabasis` would wall the arrows off in that
  direction. `stepPreset` therefore keeps stepping in the pressed direction until an entry
  loads, bounded by the list length so the pass terminates having visited each entry at most
  once. It cannot wrap back to the row the press started from and re-apply it: that needs every
  other entry to fail, and the factory rows — a contiguous prefix of the ring, at least two of
  them, each unable to fail (a factory apply fails only on an out-of-range index) — guarantee
  the walk stops first. So a folder of nothing but unreadable *user* files lands on a factory
  preset. Both premises are pinned by `testTheRingWalksPastAnUnreadablePreset`'s
  `factoryCount >= 2`; a one-row bank, or a factory source that can fail, would need the
  starting row excluded from the walk explicitly. This carries forward the reason the deleted hint was advanced *unconditionally*
  (`DOCUMENTATION_COVERAGE.md`, round 63 item 2) and improves on it: the press lands on the
  next preset that actually loads rather than merely stepping over the broken one. Retrying is
  free — the only reachable failure is `parsePresetFile` refusing the file, which returns
  before the undo bracket and the §2.8 duck, so one press still mints one undo step. Pinned by
  `testTheRingWalksPastAnUnreadablePreset`.
- The editor's `lastPresetWasFactory` / `lastPresetFactoryIdx` / `lastPresetFile` members and
  both `rememberPresetSource` overloads are **removed**. Deliberately not deprecated in place:
  the hint pattern is the thing the identity replaces, and Anamorph's review recorded it as
  "not the model" for exactly this design.
- **Deliberately NOT ported** from Anamorph's implementation, with the architectural reason:
  its `onSaved → syncCommitted()` re-baseline hook (its H3 fix) — Anabasis pushes pre-state
  snapshots at the moment of each action instead of keeping a rolling `committed` baseline, so
  a save is transparent to undo by construction; its §8 identity-moved redo narrowing —
  Anabasis records an undo step for every preset apply (`pushUndoStep` before the bracket,
  redo cleared unconditionally), so the same-sound coalescing branch the narrowing guards does
  not exist here; its root-level property trio (§Decision 6); and everything else in its
  0.9.2 change set (menu lifetime, wording) — separate concerns, and the menu-lifetime fix
  already exists here.
- The open-gesture-across-save undo residual Anamorph's maintainer accepted (its worklog §16)
  exists in the same narrow shape here: a gesture open across `savePresetFile` snapshots the
  pre-save name/identity. Accepted with the same reasoning; recorded, not fixed.

## Related code

- `src/PresetManager.h` / `.cpp` — `FactoryPreset::id`, `Selection`, `SelectionFields`,
  `encodeSelection` / `decodeSelection`, `selectedPresetRow`, the id column in `kFactory[]`
- `src/PluginProcessor.h` / `.cpp` — `liveSelection`; the trio written in `saveSlotFromLive`,
  adopted in `applySlotToLive`, decoded in `setStateInformation`'s slot overlay, defaulted in
  `resetSlotFieldsToDefaults`; identity assignments in `applyFactoryPreset`,
  `applyPresetFile`, `savePresetFile`
- `src/gui/PluginEditor.cpp` — `showPresetMenu` single-tick resolution, `stepPreset`
  resolver-based position and its walk-past-an-unreadable-entry loop
- `docs/architecture/SERIALIZATION_REGISTRY.md` §1.2 — the three field rows
- `docs/policies/SESSION_COMPATIBILITY_POLICY.md` rule 4 — round-trip list

Evidence [Verified]:
- Source: the Related code list above.
- Test: `AnabasisStateTests` — `testPresetIdentitySharedName` (live behaviour: save-selects,
  both rows selectable, stepping origin, undo after save, A/B round-trip, outside-folder file,
  deleted file, the preset FILE gaining no field), `testFactoryPresetIdIntegrity` (ids
  non-empty and unique, each id round-tripping to its own row, and exactly one factory preset
  landing on the all-defaults parameter snapshot — a second one would be an override table that
  silently applied nothing), `testPresetIdentityAcrossRestore` (the whole fallback matrix
  above, per A/B slot, plus the nested-sub-folder and tilde-named-file round-trips, the
  pre-change-session case, the Copy carrier with its identity-only-move and phantom-free
  cases, and the no-AB identity reset — each restore case asserting bit-identical parameters).
- Worklog: the review trail lives in the sibling repository
  (`Anamorph:worklogs/PRESET_MENU_AND_IDENTITY_v0.9.2.md`); this port deliberately imports its
  three review-found traps (the −1 fall-through, the recursive `isAChildOf`, the tilde-named
  direct child) as first-class cases rather than re-discovering them.
