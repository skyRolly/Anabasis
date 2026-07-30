# OPEN_QUESTIONS.md

Decisions that this repository is **not** allowed to guess at
(`docs/DEVELOPMENT_BRIEF.md` §13). Each entry states the question, why it cannot be answered from
the repository, the options with their consequences, and a recommendation where one exists.

An answered question moves to `Resolved` with the decision, the date, and — if it is an
architecture decision — the ADR that records it. Questions are never deleted.

Status values: `Open` · `Blocking <phase>` · `Resolved`.

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

## OQ-010 — Does the limiter lookahead get an explicit 0 / off position? · `Blocking P1`

**Question.** §4.3 specifies the limiter lookahead as **0.5–10 ms** (default ≈ 2 ms). On that
range the plugin **always reports non-zero latency to the host**; there is no zero-latency
configuration.

**Why it cannot be deferred.** Parameter ranges are semantic and become contract at the first
shipped build (`PARAMETER_COMPATIBILITY_POLICY.md` rule 3): widening the range later to add a 0
position re-scales every saved session's normalised value and is an `ARCHITECTURE_REVIEW_GATE`
item. It also determines what `DSP_POLICY.md` invariant 2 and the release checklist can assert —
"with lookahead 0 and oversampling off, reported latency is 0" is not testable on a 0.5–10 ms
range, so the invariant is currently phrased against the engaged lookahead instead.

**Trade-off.** An off position enables a genuinely zero-latency mode (useful while tracking, and
in hosts with poor PDC), at the cost of a limiter that cannot catch transients ahead of time — a
markedly different, and worse, sound. Several mastering limiters deliberately omit it for exactly
that reason.

**Action.** Decide in `DESIGN.md` before `createAnabasisLayout` exists. If the answer is "no off
position", say so explicitly in the parameter table so it reads as a decision rather than an
oversight.

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

### OQ-001 — Which JUCE 9.x point release do we pin? · `Resolved 2026-07-30`

**Question.** §2 requires pinning the newest stable JUCE 9.x point release by its tag's
**immutable commit SHA**, and recording the tag in `README.md`.

**Decision (owner, 2026-07-30).** Pin **the same JUCE the sibling product pins: 9.0.0**, by the
tag's immutable commit SHA `f8f8864172464b9adf9eba6101e1f784838d1597`.

**Rationale.** A single shared framework version across the product family keeps the Level-5
audition baseline comparable between the two plugins and means one dependency audit, one set of
JUCE-attributable behaviour, and one bump decision for both products. Anamorph has already
verified this exact commit headlessly (its ADR-0022 records a 32-scenario twin dump proving
engine output bit-identical 8.0.14 → 9.0.0, including reported latencies), so Anabasis inherits a
framework revision with evidence behind it rather than an unexercised newer one.

**Recorded in.** `README.md`, `docs/policies/DEPENDENCY_POLICY.md`, `docs/procedures/BUILD.md`,
`docs/HANDOVER.md`. It must additionally be written into `CMakeLists.txt`
(`ANABASIS_JUCE_VERSION "9.0.0"` + `ANABASIS_JUCE_TAG "f8f8864…"`) when that file is created at
P1, and recorded in the P0 build-decision ADR.

**Evidence [Verified]:** the version + SHA are read from the sibling repository's
`CMakeLists.txt:36-38` and its ADR-0022. **Not verified from this repository** — Anabasis has no
build yet, so "this pin configures and builds" becomes Verified only at P1.

**Standing obligation.** This is now a pin, so `DEPENDENCY_POLICY.md` applies in full: any later
change is an `ARCHITECTURE_REVIEW_GATE` Build System change requiring an ADR plus the rule-2
verification. Also: §2 requires checking for a newer stable 9.x (and reporting rather than
adopting a JUCE 10) — that check is now a *deliberate deferral*, not an oversight. Re-run it if
9.0.0 turns out to lack something this project needs.

---

### OQ-003 — Plugin identity codes and bundle ID · `Resolved 2026-07-30`

**Question.** `juce_add_plugin` needs `PLUGIN_MANUFACTURER_CODE`, `PLUGIN_CODE`, `BUNDLE_ID`.
These are **permanent host-facing identity**: the manufacturer code is the AU component's
manufacturer field, and JUCE derives the VST3 class UID from the manufacturer code + plugin code +
plugin name. A host that recorded the old identity in a session does not load a *changed* plugin —
it reports the plugin as **missing**.

**Decision (owner, 2026-07-30).**

| Field | Value | Rationale |
|---|---|---|
| `COMPANY_NAME` | `RollyTech` | same brand |
| `PLUGIN_MANUFACTURER_CODE` | **`RTec`** | The manufacturer code identifies the **vendor**, so it is shared by every RollyTech plug-in. `Anmf` was rejected: it abbreviates *Anamorph*, the first product, which does not survive a product line. **Anamorph is changing to `RTec` in its 0.9.1** so the two products agree from the start (Anamorph ADR-0023). Also considered and rejected: `Roll` (a common English word — higher chance of colliding with another vendor's registered code), `RolT`, `RlyT`. AU requires ≥ 1 uppercase character; `RTec` satisfies it. |
| `PLUGIN_CODE` | **`Anbs`** | Per-product and must be unique; `Anmr` is Anamorph's. |
| `BUNDLE_ID` | **`com.rollytech.anabasis`** | Matches the sibling product's pattern. |
| `VST3_CATEGORIES` | **`"Fx" "Dynamics" "Mastering"`** | A maximizer, not a spatial effect (Anamorph uses `"Fx" "Spatial" "Stereo"`). |

**Consequence — none, and that is the point.** Anabasis has never built, so it adopts the final
vendor code before it can ever have an identity to break. Anamorph pays a one-time disruption
(its KI-016) precisely so that this repository does not.

**Standing obligation.** From the first build that leaves this repository these values are frozen
(`COMPATIBILITY_POLICY.md`). Anamorph has already spent the "before the first release" exception
for the manufacturer code; there is no comparable exception available here.

**Recorded in.** `docs/procedures/BUILD.md` §Plugin identity. Must be written into
`CMakeLists.txt` when it is created at P1, and into the P0 build-decision ADR.

**Evidence [Unverified].** No build exists, so "these values register correctly in a host" is
unproven — it becomes Verified at the first Level-5 check (P1).
