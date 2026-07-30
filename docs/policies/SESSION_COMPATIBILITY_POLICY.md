# SESSION_COMPATIBILITY_POLICY.md

Subset of `COMPATIBILITY_POLICY.md`. Governs state serialization
(`getStateInformation` / `setStateInformation`). Ledger:
`docs/architecture/SERIALIZATION_REGISTRY.md` (written at P1).

## Rules

1. **Serialization fields are immutable.** No field in the state tree — the versioned root, the
   APVTS subtree, the host-hidden session subtree, or the A/B subtree — may be removed or have its
   meaning changed without an ADR + migration.
2. **Additions must tolerate absence.** A new field must have a default applied when an older
   session lacks it, so old sessions still load.
3. **Every legacy read path stays.** Once a state format has shipped in any build that left this
   repository, its read path is permanent and is listed in `COMPATIBILITY_POLICY.md` with a frozen
   fixture in `tests/fixtures/`.
4. **A save → load round-trip must reproduce** the sound, the preset name, the dirty marker, both
   A/B slots, the active slot, and any parameter locks.
5. **View / session params are preserved on restore.** Applying an A/B slot, an undo step, or a
   preset must not clobber the current view state (mode, window size, tooltips, meter options).
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
