# SESSION_COMPATIBILITY_POLICY.md

Subset of `COMPATIBILITY_POLICY.md`. Governs state serialization
(`getStateInformation` / `setStateInformation`). Ledger:
`docs/architecture/SERIALIZATION_REGISTRY.md` (written at P1).

## Rules

1. **Serialization fields are immutable.** No field in the state tree — the versioned root, the
   APVTS subtree, the host-hidden session subtree, or the A/B subtree — may be removed or have its
   meaning changed without an ADR + migration.

   *A MEANING change was enacted this way, and the rule caught it late rather than early:* **ADR-0026**
   records 0.1.4's two `SLOT` read rules — a slot carrying no `ANABASIS` child resolves to defaults
   as a whole, and the active slot's metadata is adopted only when the ROOT surface was restored.
   That is a semantic change to how a stored session is interpreted, so this rule and
   `ARCHITECTURE_REVIEW_GATE.md` both apply; it was implemented **without being flagged as gated**,
   review caught that on 2026-08-13, and **the owner ratified it on 2026-08-14**. No blob this plug-in
   writes reaches either rule, which bounded the exposure but did not clear the gate — the owner
   did. The lesson
   for the next reader is the one this line exists to carry: the change looked like a bug fix, and
   a bug fix that alters what a stored session MEANS is exactly what rule 1 is about.

   *Exercised once, pre-ship:* **ADR-0015** removes `int_meterTargets` from `ANABASIS_INTERNAL`,
   with the §4.4 defaults-first read rules as the migration. That ADR also records the condition
   that closes the pre-ship window — the first build that leaves this repository — after which
   this rule binds with no latitude at all. The rule itself is unchanged; the pointer is here so a
   reader who finds a field missing from an old blob knows where the authority is.
2. **Additions must tolerate absence.** A new field must have a default applied when an older
   session lacks it, so old sessions still load.
3. **Every legacy read path stays.** Once a state format has shipped in any build that left this
   repository, its read path is permanent and is listed in `COMPATIBILITY_POLICY.md` with a frozen
   fixture in `tests/fixtures/`.
4. **A save → load round-trip must reproduce** the sound, the preset name, the preset indicator
   identity (which row is ticked — ADR-0022; when the stored identity no longer resolves, the
   reproduction is "no row", never a same-named substitute), the dirty marker, both
   A/B slots, the active slot, and any parameter locks.
5. **View / session params are preserved on restore — with the undo exception ADR-0018 added.**
   Applying an **A/B slot** or a **preset** must not clobber the current view state (window size,
   tooltips, and the Advanced/Simple mode). An **undo step** is deliberately different since
   ADR-0018: it restores `advancedMode`, because an ADV toggle is a user action the user can take
   back, and it is the ONLY adoption path that does — `applySlotToLive`'s adopt-flag pins the
   value on every other. The authoritative membership of "view state" is the code predicate
   `isViewTierParam`, not this list. *("meter options" named `int_meterTargets`, removed by
   ADR-0015 with the streaming-target display; the phrase is dropped rather than re-pointed,
   because no field replaced it — the ADR-0020 selectors choose a STANDARD, not what is shown.)*
6. **Discrete parameters round-trip exactly.** Store the raw normalised value alongside the
   human-readable one where a choice/step parameter would otherwise be re-quantised on load.
7. **Corrupt or foreign state must not crash or produce a bad sound.** An unreadable state falls
   back to defaults; a state from another plugin is rejected cleanly.

## Design guidance for P1 (the schema does not exist yet)

The schema is written once. Make the first version the one that can grow:

- A **versioned root element** with an explicit schema version attribute — so a future migration
  has something to branch on that is not "guess from what's present".
- **Separate subtrees** for automatable parameters, host-hidden session/view state, A/B slots and
  preset metadata. Anamorph learned this the expensive way: view parameters started inside the
  APVTS and had to be migrated out later under an ADR.
- **Host-hidden state is deliberate, not accidental** — oversampling factor, phase mode,
  offline-render quality, UI scale, tooltips, animation toggle and meter options are session
  state, not automation lanes.
- Decide up front whether the **adaptive engine's learned state** (`Learn` targets, frozen
  adaptation state) is persisted. If it is, it is schema, and everything above applies to it.

## Required verification before release

- `[ ] Session reload verified` (save in vN−1, load in vN — sound identical).
- `[ ] Presets migrated` (factory + a user `.anabasis` still load).

These are enforced at release time via `docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md`.

## Enforcement

A serialization schema change is an **Architecture Review Gate** item and an **AI Agent Hard
Stop** (`AI_AGENT_POLICY.md`). Changing this policy requires an ADR.
