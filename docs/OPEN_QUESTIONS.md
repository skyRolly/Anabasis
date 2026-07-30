# OPEN_QUESTIONS.md

Decisions that this repository is **not** allowed to guess at
(`docs/DEVELOPMENT_BRIEF.md` §13). Each entry states the question, why it cannot be answered from
the repository, the options with their consequences, and a recommendation where one exists.

An answered question moves to `Resolved` with the decision, the date, and — if it is an
architecture decision — the ADR that records it. Questions are never deleted.

Status values: `Open` · `Blocking <phase>` · `Resolved`.

---

## OQ-001 — Which JUCE 9.x point release do we pin? · `Blocking P1`

**Question.** §2 requires pinning the newest stable JUCE 9.x point release, by its tag's
**immutable commit SHA**, and recording the tag in `README.md`. This repository has no network
evidence of the current 9.x release list.

**Why it cannot be guessed.** A wrong pin is a build-system change later
(`ARCHITECTURE_REVIEW_GATE.md`) and can silently move DSP behaviour, latency, the editor path and
the state ABI (`DEPENDENCY_POLICY.md`). Anamorph currently pins 9.0.0
(`f8f8864172464b9adf9eba6101e1f784838d1597`).

**Action.** At P0, read `github.com/juce-framework/JUCE` releases/tags, take the newest stable
`9.x`, resolve the tag to its commit SHA, and record **both** in `CMakeLists.txt`
(`ANABASIS_JUCE_VERSION` + `ANABASIS_JUCE_TAG`) and `README.md`. If a JUCE 10 has appeared, report
it and **ask** before adopting it (§2).

**Recommendation.** Prefer matching Anamorph's pin unless a newer 9.x fixes something this project
needs — a single shared framework version across the product family keeps the Level-5 audition
baseline comparable.

---

## OQ-002 — Which JUCE licence tier does Anabasis ship under? · `Blocking commercial release`

**Question.** §2 says to confirm which tier Anamorph ships under and keep this project on the
same tier.

**Why it cannot be guessed.** The Anamorph repository records the tier as an **open owner/legal
decision**, not as a settled fact: it states that the closed-source commercial model rules out
the AGPLv3 arm and that the commercial tier "must be in place before commercial distribution."
There is therefore no answer to copy — this is an owner action, not an engineering one.

**Consequence if unresolved.** Does not block development or internal testing. It blocks
commercial distribution absolutely.

**Related.** Commercial VST3 distribution additionally requires reviewing Steinberg's licensing
terms separately.

---

## OQ-003 — Plugin identity codes and bundle ID · `Blocking P1`

**Question.** `juce_add_plugin` needs `PLUGIN_MANUFACTURER_CODE`, `PLUGIN_CODE`, `BUNDLE_ID`.
These are **permanent host-facing identity** — a host keys its plugin database and a user's saved
sessions off them, so changing one after any build reaches a tester breaks that tester's sessions.

**Anamorph's values (for reference):** `COMPANY_NAME "RollyTech"`,
`BUNDLE_ID "com.rollytech.anamorph"`, `PLUGIN_MANUFACTURER_CODE Anmf`, `PLUGIN_CODE Anmr`.

**Proposal (needs confirmation, not assumed):**

| Field | Proposed | Rationale |
|---|---|---|
| `COMPANY_NAME` | `RollyTech` | same brand |
| `PLUGIN_MANUFACTURER_CODE` | `Anmf` | manufacturer codes are per-*vendor*, so the family should share one; note it reads as an Anamorph abbreviation, which is why this needs a decision rather than a copy |
| `PLUGIN_CODE` | `Anbs` | must be unique per product; `Anmr` is taken by Anamorph |
| `BUNDLE_ID` | `com.rollytech.anabasis` | matches the Anamorph pattern |
| `VST3_CATEGORIES` | `"Fx" "Dynamics" "Mastering"` | maximizer, not a spatial effect |

**Action.** Owner confirms (or corrects) the table above before the first `juce_add_plugin` call.

---

## OQ-004 — Simple ⇄ Advanced coexistence strategy · `Blocking P4`

**Question.** §5.3 requires a decision: when the user has edited parameters manually in Advanced
and then returns to Simple, how do macro values and manual values coexist? The brief names two
candidate directions (macro takes precedence with a clear notice; or offer a "carry over" option)
and requires the strategy to be **argued in the design document before implementing**.

**Hard constraint regardless of the answer.** Switching modes must not change the sound *at the
moment of the switch* (`MODE_AND_ADAPTATION_POLICY.md`). Any strategy that fails that is
excluded.

**Action.** Argue and decide in `DESIGN.md`; record as an ADR before P4 implementation.

---

## OQ-005 — Extract a shared `rollytech-ui` module? · `Open`

**Question.** §1.2 asks for an assessment of whether the shared UI layer (LookAndFeel, About page,
Settings page, Bypass placement, preset/A-B interaction) is worth extracting into a shared module
consumed by both products, versus copy-and-adapt.

**Trade-off.** Extraction gives one place to fix brand drift, but couples two release cycles and
makes any change to Anamorph's UI a change to a *shipped* product — which is an
`ARCHITECTURE_REVIEW_GATE` item over there. Copy-and-adapt ships faster (the brief explicitly
prioritises shipping on schedule) at the cost of guaranteed divergence.

**Note.** Anabasis must not modify the Anamorph repository, so extraction is not unilaterally
available to this project in any case — it would require a coordinated change to both.

**Action.** Give a recommendation in `DESIGN.md` (§1.2 requires one). Do not extract without
owner approval.

---

## OQ-006 — Where does the C++23 canary run, and what does it gate? · `Open`

**Question.** §2.1 requires a **non-blocking** CI canary job building at C++23 on all three
platforms, whose failure must never block the main pipeline, with its status reported in each
phase summary.

**Open detail.** Whether the canary builds the full target set or only the DSP core + tests, and
whether it runs per-push or on a schedule. Full-matrix per-push roughly doubles CI cost for an
early-warning signal.

**Recommendation.** DSP core + tests only, on a weekly schedule plus `workflow_dispatch`, added at
P2 when there is DSP code for it to compile. Confirm at P2.

---

## OQ-007 — Does the release pipeline ship installers at P6? · `Open`

**Question.** Anamorph ships an Inno Setup installer (Windows), a `.pkg` (macOS) and shell
installers inside the Linux zip, plus a tag-triggered draft-release pipeline. Anabasis's §11 P6
says only "presets, performance optimisation, pluginval L10, DAW matrix, documentation."

**Action.** Confirm at P5 whether P6 includes the full packaging/installer set (a substantial,
well-understood port from Anamorph) or whether v0.1.0 ships as plain zips.

---

## OQ-008 — Loudness-penalty reference values · `Open`

**Question.** §6 requires streaming-target lines (Spotify −14, Apple Music −16, YouTube −14,
club/CD) **plus a loudness-penalty estimate in dB per platform**.

**Why it cannot be guessed.** Platform normalisation targets change, and several platforms do not
publish an authoritative figure. Shipping stale numbers as if they were facts is a **C2**
violation and would mislead a mastering decision.

**Action.** Decide the source of these values and how they are dated/updated (hard-coded with a
documented "as of" date, or user-editable), and record the decision plus each value's source in
`DESIGN.md`. Any number that reaches the UI needs a citable origin.

---

## OQ-009 — Ownership and support contact · `Open`

**Question.** `docs/HANDOVER.md` requires owner/team metadata and a support contact. No such
metadata exists in this repository.

**Action.** Owner supplies. Until then the field stays `TODO` (constraint C7) — it is not
inferred from Anamorph.

---

## Resolved

*(none yet)*
