# ADR-0009 — Code reuse from Anamorph: copy-and-adapt with provenance, no shared module for v1

**Status:** Accepted (2026-07-31 — owner sign-off on `docs/DESIGN.md`)

## Context

`CLAUDE.md` §3 states the cross-repository rule: **Anamorph is a read-only reference.** Anabasis may
copy and adapt first-party code, docs and CI from it, but never modifies the Anamorph repository —
and *"code reuse across the two products is a product-family decision recorded in an ADR, not an
ad-hoc copy."* That sentence makes this ADR mandatory before the first copied file lands, not after.

`DEVELOPMENT_BRIEF.md` §1.2 asks for an explicit assessment of whether the shared UI layer should be
extracted into a module consumed by both products, and registers it as **OQ-005**. `DESIGN.md` §8
answers with a recommendation — copy-and-adapt now, revisit extraction after v0.1.0 — and states in
the same section that the recommendation binds only through this ADR: per `SOURCE_OF_TRUTH.md` a
DESIGN section is Architecture, and an ADR outranks it.

The dependency being settled is much wider than OQ-005's question. `DESIGN.md` leans on sibling
sources in nearly every chapter: the GUI language (§6.1, §6.5), the wrapper and state machinery
(§4.3, §4.4, §7), and — the copies most likely to be questioned at P2 — **DSP sources**: the
published K-weighting coefficients and the Measure+Predict structure of `LoudnessMatch` (§2.7, §2.9),
the `ScopeBuffer` SPSC ring (§2.9), the three-mechanism transition taxonomy of Anamorph's ADR-0004
(§2.8), and the peak-preserving `driveTanh` makeup formulation (§2.4). An ADR that blessed only the
UI copy would leave every one of those as exactly the ad-hoc copy `CLAUDE.md` §3 forbids.

Anabasis has no `src/`. The whole of P1 is the first opportunity to get this wrong, which is why the
rule is written before the code rather than derived from it.

## Problem

Two questions, and the second is the one OQ-005 does not ask.

**(a) Extraction or copy?** A shared `rollytech-ui` module is the tidy answer and the one a reviewer
will expect: one place to fix brand drift, one implementation of the glass language, one `FrameClock`.
Both products are first-party RollyTech code, so there is no licensing obstacle and no third-party
dependency to vet. The reasons it still loses are structural rather than aesthetic, and they need
recording or the next contributor will re-propose it.

**(b) What is in scope?** OQ-005 is phrased about the UI layer — LookAndFeel, About, Settings, Bypass
placement, preset/A-B interaction. But the design's most load-bearing reuse is not UI at all: it is a
block of published filter coefficients, a lock-free ring buffer, a transition taxonomy and a
saturation makeup formulation. Deciding "copy the UI" while leaving the DSP copies undecided would
split one product-family decision across a recorded half and an unrecorded half.

Neither question is obvious. Extraction is the option that a healthy monorepo would take, and the
argument against it is not "copying is better engineering" — it is that this repository pair is not a
monorepo and one half of it has already shipped.

## Options

**On the module question.**

- **A. Extract a shared `rollytech-ui` module now, consumed by both products.** One source of truth
  for the brand layer; drift becomes impossible rather than merely discouraged. **Lost, on three
  independent grounds.** (i) It couples two release cycles: Anabasis's P5 UI work would land in
  Anamorph's build, so every Anabasis brand change becomes a change to a **shipped** product and an
  `ARCHITECTURE_REVIEW_GATE` item *over there*, gating this project's schedule on the other product's
  review. `DEVELOPMENT_BRIEF.md` §1.2 explicitly prioritises shipping on schedule. (ii) It is **not
  unilaterally available**: extraction requires editing Anamorph to consume the module, and
  `CLAUDE.md` §3 forbids Anabasis from modifying that repository at all — so this option is not a
  decision this project can take, only one it can request as a coordinated change to both. (iii) At
  P0 neither side knows what is genuinely common. `DESIGN.md` §6.1 already commits to a different
  accent family, a variable font instead of the platform-default typography, a full accessibility
  layer Anamorph does not have, and a Settings item list that shares only its row grammar; §6.2
  expects P5 to re-derive the window geometry from a maximizer's real control inventory. Extracting
  before the divergences are known produces a module that is mostly configuration points.
- **B. Git submodule or vendored subtree of selected Anamorph files.** A middle path: upstream stays
  visible, the pin is explicit, no coordinated edit to Anamorph is needed. **Lost:** it buys
  extraction's coupling without extraction's benefit. Anabasis still cannot change the upstream files,
  so every adaptation this design requires — the accent swap, `anabasis`/`ANABASIS_INTERNAL`
  identifiers, the inverted compensation polarity, the −70 LUFS gate, the widened `StateSet` — would
  have to live as a patch set layered over vendored sources. A patch set over a pinned tree is
  strictly harder to read and review than an adapted file, and it still delivers no shared fix path.
- **C. Copy-and-adapt into Anabasis's own `src/`, every copied file carrying a provenance header
  naming the Anamorph original; **no shared module for v1**; extraction revisited as a product-family
  decision after v0.1.0 ships. Chosen.**
- **D. Write everything from scratch; no reuse at all.** Maximum independence, zero coupling, no
  inherited assumptions. **Lost:** it discards proven solutions to problems that are already solved
  and whose failure modes were paid for once — the X11 OpenGL use-after-free (Anamorph ADR-0011), the
  raw-exact serialization attribute (its ADR-0013), the clamped A/B slot index that survives
  hand-edited and forward-version blobs, the SPSC ring discipline of one release-store per block. It
  would also mean re-deriving the **published BS.1770-4 K-weighting coefficients** by hand, which is a
  correctness risk with no upside: they are standard-defined values, and the sibling's transcription
  is already verified against the EBU vectors in a shipped product.
- **E. Copy without provenance headers** (a plain, unannotated copy). Cheapest, and superficially the
  same code. **Lost:** it *is* the ad-hoc copy `CLAUDE.md` §3 names. It also destroys the input to the
  post-v0.1.0 extraction review — without headers, "what is actually common" can only be reconstructed
  by reading two codebases side by side — and it lets an inherited assumption travel silently, which
  is a live hazard here: Anamorph's ADR-0009 records "no output clipper anywhere", and Anabasis's
  ADR-0006 deliberately inverts that.

**On the scope question.**

- **F. Restrict this ADR to the UI layer, as OQ-005 literally asks.** Answers the registered open
  question and nothing more. **Lost:** `DESIGN.md` §8 says in terms that this ADR's scope is
  deliberately wider, because the DSP-source adaptations (§2.4, §2.7–2.9) are the copies most likely
  to be challenged at P2 review. Recording only the UI half would make each DSP copy individually
  unrecorded and therefore individually non-compliant with `CLAUDE.md` §3.
- **G. One ADR per copied file or per tier.** Maximum granularity; each copy reviewable on its own.
  **Lost:** it is the "predefined quota" pattern constraint C1 forbids, applied to a single decision.
  The decision — *copy rather than share, with provenance* — is one decision; the file list is its
  scope, not eleven separate rulings.

## Decision

**1. No shared module in v1.** No `rollytech-ui` (or `rollytech-dsp`) library, no submodule, no
vendored subtree, no build-time dependency of either product on the other. Anamorph remains a
**read-only reference**: nothing in this repository, and no work authorised by this ADR, modifies the
Anamorph repository — a standing obligation, not a v1 condition.

**2. Reuse is by copy-and-adapt** into Anabasis's own `src/` tree, under Anabasis namespaces
(`anabasis`, `anabasis::gui`) and Anabasis identifiers (`pid::`, `ANABASIS_INTERNAL`). A copied file
is thereafter an **Anabasis file**: it is maintained here, reviewed here, and diverges here.

**3. Every copied or adapted file carries a provenance header** — a comment block at the top of the
file, before the include guard, with four fields: the Anamorph source path and line range; the commit
read (**`b6a3db8`**, the P0 research revision); whether the content is verbatim or adapted; and, when
adapted, one line naming what changed and why. Illustrative shape:

```
// Provenance: adapted from Anamorph src/dsp/LoudnessMatch.cpp:126-185 @ b6a3db8.
// Adaptation: polarity inverted (wet is always louder), predict is floor-only, silence gate
// referenced to BS.1770 -70 LUFS instead of -60 dBFS mean-square. See ADR-0006, ADR-0009.
// Anamorph is a read-only reference (CLAUDE.md §3) — changes here are never synced back.
```

A block of standard-defined data reused through the sibling (the K-weighting coefficients) cites the
**standard** as well as the sibling, because its authority is ITU-R BS.1770-4, not Anamorph.

**4. Scope — three tiers, all authorised by this ADR.** The list is the P0 inventory, not a closed
set (item 7).

*(a) GUI idioms.* The `AnamorphLookAndFeel` structure and its **componentID-keyed drawing variants**
(`"bypass"`, `"ghost"`, `"icon"`, `"vtoggle"`, `"presetname"` — variants instead of a component
subclass per look); the **`glass::` surface namespace** (`drawEdges` / `fillPanel` /
`drawCircleEdge`, one parameterised depth language for every framed surface); the neutral palette
roles verbatim (`bg`, `bgPanel`, `bgRaised`, `outline`, `text`, `textDim`); **`FrameClock`** with
static-layer caching, atomic meter sources and snapshot repaint gates; the **`Backdrop`** overlay
type (About, Settings, Save-Preset) and the `DimLayer` bypass wash; the top-bar layout grammar with
**Bypass rightmost** and its red pill; the whole-window `setScaleFactor` transform for UI scaling;
Anamorph ADR-0011's platform rule (OpenGL attached on macOS/Windows, **never** on Linux/X11).

*(b) Wrapper and state machinery.* `InternalState` (host-hidden session state, Anamorph ADR-0010);
the **`StateSet` A/B slot + per-slot undo** machinery, here widened to
`{params, presetName, baseline, frozenTrims, detachMask}` (ADR-0007) with per-slot stacks capped at
128 and never serialized; eager slot initialisation; `abCopyToOther` pushing onto the *other* slot's
stack; gesture-gated undo coalescing with host automation folded into the baseline; preset-load undo
bracketing that parses before the bracket opens; `requestDuck()` before every bulk swap; the
sentinel-atomic inject-at-the-duck-bottom pattern **for single-scalar commands only — this
authorisation does not extend to the four-scalar frozen trim vector, whose transport is `OQ-013`
and an AI-agent Hard Stop until an ADR settles it**; `PresetManager` (folder layout, snapped-value
contract, compiled-in factory override tables); **raw-exact serialization** (the additive exact `raw`
attribute, Anamorph ADR-0013) with structural-tolerance read rules; the `pid::` namespace shape,
`ParameterID{id, kVersion}`, the Raw\* exact-normalised discrete classes, the formatter/parser
lambdas, and the single shared exclusion predicate; the **`clampAbSlotIndex` idiom** — a
dependency-free `constexpr` clamp applied to every restored index that feeds a fixed-size array,
guarded by its own headless test.

*(c) DSP-source adaptations.* The **ITU-R BS.1770-4 K-weighting coefficients** and the
**Measure+Predict** structure of `LoudnessMatch` (adapted per ADR-0006: polarity inverted, predict
floor-only, −70 LUFS-referenced silence gate); the **`ScopeBuffer` SPSC ring** (power-of-two ring,
one release-store per block) as the basis of `GrHistoryBuffer` and the two spectrum capture rings;
the **duck / crossfade / warm-monitor transition taxonomy** of Anamorph ADR-0004 — asymmetric
raised-cosine duck for genuine discrete rewires, always-running output crossfades for
bypass/monitoring toggles, smoothed parameters for everything continuous — including the dry-fill
mechanism and the always-running-chain-with-output-crossfade bypass structure; the **`driveTanh`
peak-preserving makeup** formulation and its clean-blend identity property, which is the precedent
for `clipDrive`'s automatic level compensation being exact identity at 0 dB drive; the 20 ms standard
parameter-smoothing constant; `ScopedNoDenormals` at the top of `processBlock` as the single FTZ/DAZ
mechanism; the oversampler-instances-precomputed-at-`prepare()` rule; the single
`setLatencySamples` call site fed by a const `predictLatency(snapshot)`.

**5. Deliberately NOT copied — inherited assumptions that must not travel with the source.** A
provenance header points at code written for a different product; these are the places where the
sibling is wrong *for Anabasis*, and a copy that silently keeps them is a defect:

- **Advanced-mode gating.** Anamorph's snapshot builder gates parameters to neutral by mode, so its
  Advanced toggle can change the sound. Anabasis's mode switch is a pure view change
  (`MODE_AND_ADAPTATION_POLICY.md` inv 1–2, ADR-0005) — the exact opposite contract.
- **"No output clipper anywhere"** (Anamorph ADR-0009). Anabasis's product promise *is* the ceiling;
  ADR-0006 inverts that decision deliberately.
- **The −60 dBFS mean-square freeze threshold** in the level-match path — too high for mastering
  material; ADR-0006 gates against BS.1770 −70 LUFS instead.
- **Structural-only format detection with no version field.** ADR-0007 writes `schemaVersion` = 1
  from day one while keeping the tolerant read rules.
- **`advancedMode` travelling with A/B and undo.** Here it is in the view tier (§4.2), because an A/B
  or undo step that resizes the editor walks into the X11 host-crash path of Anamorph's KI-003.
- **Zero accessibility.** Anamorph has none; brief §8 requires it, so the copied GUI carries a gap to
  be filled at P5, not a standard to be matched.
- **Brand and geometry specifics.** The teal/blue accent pair, the platform-default typography, the
  `940×720` / `940×900` frame constants and the Anamorph Settings item list are carried as *patterns*
  only; §6.1–6.4 replace the values, and §6.2 expects P5 to re-derive the geometry.

**6. Pattern reuse is not source reuse.** Inheriting an *architecture* — the two-layer POD-snapshot
decomposition (ADR-0001), the two-thread no-worker model (ADR-0011), the benchmark procedure — is not
a copy, needs no provenance header, and is governed by the ADR that adopts it. This ADR governs
copied **source**.

**7. Standing authorisation and its conditions.** A new copy from Anamorph during v1 does **not** need
its own ADR, provided all three conditions hold: it carries the provenance header of item 3; the
Anamorph repository is not modified; and the item 5 list is respected. A copy that would import one of
the item 5 assumptions, or any proposal to share code as a module, is outside this authorisation and
requires a new ADR.

**8. Divergence is accepted and one-way.** There is no upstream-sync obligation and no backport path:
a fix made here is not propagated to Anamorph (this project cannot), and a fix made there is not
automatically inherited. Drift is fixed per product.

**9. Revisit after v0.1.0 ships.** Extraction is reconsidered then as a **product-family** decision —
owner-approved, requiring a coordinated change to both repositories, and recorded in a new ADR that
supersedes this one in part. The union of the provenance headers is that review's input: it is the
evidence-backed answer to "what is actually common", which cannot be known at P0.

## Consequences

- **The design's sibling dependencies become legitimate rather than assumed.** Every reuse
  `DESIGN.md` relies on — from the glass namespace to the K-weighting block — is now covered by a
  recorded product-family decision, satisfying `CLAUDE.md` §3 in one ruling instead of eleven.
- **Guaranteed divergence, paid twice.** A brand fix, a LookAndFeel bug, or an improvement to the SPSC
  ring must be made in both products or accepted as drift. This is the explicit price of shipping on
  schedule (`DEVELOPMENT_BRIEF.md` §1.2) and the reason item 9 sets a review point rather than
  declaring the question closed.
- **Anamorph's release cycle is unblocked by Anabasis entirely.** No Anabasis work can force a change
  to a shipped product, and no Anabasis schedule depends on the other project's Architecture Review
  Gate.
- **`DSP_POLICY` invariant 13 stays structurally true.** The DSP core depends only on `juce_dsp` /
  `juce_audio_basics` (ADR-0001, ADR-0008's INTERFACE `AnabasisDSP` target). A shared module would
  have added a dependency to a target whose dependency list is a policy invariant; copying adds none.
- **Provenance headers are a maintenance obligation.** They must be written with the copy, kept when
  the file is edited, and updated (not deleted) when an adaptation deepens — a copied file that has
  been rewritten past recognition says so in its header rather than losing the pointer.
- **Correctness inherited where it is cheapest to inherit.** The published K-weighting coefficients,
  the X11 platform rule, the clamped slot index and the raw-exact attribute arrive already exercised
  by a shipped product; the ≤ 0.1 LU EBU-vector contract (§2.9) still has to be met here.
- **Copied files must be read for the item 5 list, not just compiled.** The ceiling-guarantee
  inversion is the sharp case: a DSP source adapted from a product with no output clipper must not
  carry that assumption into a maximizer.
- **Forecloses for v1:** a shared library, a submodule, a subtree, any build coupling between the two
  repositories, and any unannotated copy. Each would need a superseding ADR. It forecloses nothing
  permanently — item 9 keeps extraction live, better informed and correctly scoped as a decision for
  both products rather than one.
- **Doc-sync obligation:** registration in `ADR_INDEX.md` (`ADR_POLICY.md` rule 1); OQ-005 moves to
  `Resolved` citing this ADR; `DOCUMENTATION_LIFECYCLE_POLICY.md` applies to each copied file's
  arrival, and the P1 skeleton's review checklist gains "provenance header present and accurate" as a
  gate on any file whose origin is Anamorph.

## Related code

None yet — P1 onward. Planned: `src/gui/LookAndFeel.{h,cpp}` (palette, `glass::`, componentID-keyed
variants), `src/gui/FrameClock.h`, `src/gui/PluginEditor.{h,cpp}` (top bar, `Backdrop`, `DimLayer`,
`setScaleFactor`), the visualizer views (`GrHistoryView`, `LoudnessMeterView`, `SpectrumView`,
`CurveView`), `src/InternalState.h`, `src/AbSlotIndex.h` (the `clampAbSlotIndex` idiom),
`src/PluginProcessor.{h,cpp}` (StateSet A/B + per-slot undo, raw-exact serialization, duck-bottom
inject), `src/PresetManager.{h,cpp}`, `src/PluginParameters.{h,cpp}` (`pid::`, Raw\* discrete classes,
formatters, exclusion predicate), `src/MacroEngine.{h,cpp}`, `src/dsp/AnabasisEngine.{h,cpp}`
(transition taxonomy, always-running chain + output crossfades, oversampler prepare rule),
`src/dsp/LoudnessMeter.{h,cpp}` and `src/dsp/LoudnessComp.{h,cpp}` (K-weighting, Measure+Predict),
`src/dsp/GrHistoryBuffer.h` (ScopeBuffer idiom), `src/dsp/ClipSat.{h,cpp}` (peak-preserving makeup),
`src/dsp/EngineParameters.h`.

Evidence [Unverified] — Anabasis has no `src/`, so every claim about Anabasis code above is the
contract the code must satisfy, not an observation (constraint C2: no number here is a measurement):

- Design: `docs/DESIGN.md` §8 (OQ-005 recommendation: copy-and-adapt, provenance headers, no shared
  module, revisit after v0.1.0; and this ADR's deliberately wider scope), §10 row 0009 (mandatory
  under `CLAUDE.md` §3)
- Design (GUI tier): `docs/DESIGN.md` §6.1 (palette roles verbatim, glass language and
  componentID-keyed variants copied, accent/typography/accessibility deltas, `setScaleFactor`
  scaling, OpenGL platform rule), §6.2 (frame constants carried as pattern, geometry re-derived at
  P5), §6.4 (Settings row grammar, own item list), §6.5 (FrameClock, static-layer caching, atomic
  meter sources)
- Design (wrapper/state tier): `docs/DESIGN.md` §4.1 (`pid::` shape, Raw\* discrete classes,
  formatters/parsers), §4.2 (exclusion predicate; the two deliberate departures), §4.3
  (`ANABASIS_INTERNAL`), §4.4 (raw-exact sessions, snapped presets, factory override tables), §7
  (state machinery copied wholesale, widened StateSet, per-slot undo, duck before bulk swaps), §1.3
  (`AbSlotIndex.h` as a copy of the Anamorph idiom)
- Design (DSP tier): `docs/DESIGN.md` §2.4 (`driveTanh` peak-preserving makeup precedent), §2.7
  (Measure+Predict adapted with inverted polarity), §2.8 (three-mechanism transition taxonomy), §2.9
  (K-weighting coefficients reused; ScopeBuffer idiom for GR history and spectrum rings), §2.1 (20 ms
  smoothing constant), §1.4 (`ScopedNoDenormals`, single `setLatencySamples` call site, oversamplers
  prepared up front), §9 (benchmark procedure adopted as a pattern)
- Research: `worklogs/2026-07-30-p0-anamorph-research.md` — the P0 read of Anamorph at commit
  `b6a3db8`, which is the source of the line ranges below and the origin of the copy-vs-extract
  recommendation
- Precedent [Verified]: `Anamorph:src/gui/LookAndFeel.h:8-26` (neutral palette roles),
  `Anamorph:src/gui/LookAndFeel.h:37-52` (`glass::` surface namespace),
  `Anamorph:src/gui/LookAndFeel.cpp:251-414` (componentID-keyed drawing variants),
  `Anamorph:src/gui/LookAndFeel.cpp:332-334` (Bypass red pill)
- Precedent [Verified]: `Anamorph:src/gui/FrameClock.h:10-167`, `Anamorph:src/gui/LevelMeter.cpp:12-73`
  (vblank pacing, static-layer caching), `Anamorph:src/PluginEditor.cpp:1657-1680` (top-bar layout,
  Bypass rightmost), `:1039-1043` (dim-overlay sync), `:1312-1342` (whole-window scale transform),
  `Anamorph:src/PluginEditor.h:295-302` (frame constants kept in one place — the pattern, not the
  numbers)
- Precedent [Verified]: `Anamorph:src/InternalState.h:10-29` (host-hidden state rationale),
  `Anamorph:src/AbSlotIndex.h:15-25` (dependency-free `constexpr clampAbSlotIndex`),
  `Anamorph:src/PluginProcessor.cpp:535-613` (raw-then-value restore, tolerant read rules),
  `:485-491` (sentinel-atomic inject at the duck's silent bottom), `:338-421` (automation folded into
  the baseline, not treated as an edit), `:178-202` and `:33-38,402-421` (gesture/undo bracketing of a
  multi-target commit), `:88-105` (single `setLatencySamples` call site), `:109`
  (`ScopedNoDenormals`)
- Precedent [Verified]: `Anamorph:src/PluginParameters.h:14-88` (`pid::` namespace shape), `:66-88`
  (shared exclusion predicate), `Anamorph:src/PluginParameters.cpp:11-89` (Raw\* exact-normalised
  discrete classes), `:94-95` (`kVersion`), `:97-103,153-194` (formatters/parsers), `:286-389`
  (POD-snapshot builder), `:274-281` (KI-003, the X11 editor-resize crash behind §4.2's
  `advancedMode` departure), `:326-389` (Advanced-mode gating — item 5, deliberately **not** copied)
- Precedent [Verified]: `Anamorph:src/dsp/LoudnessMatch.cpp:16-46` (published BS.1770 K-weighting
  coefficients), `:126-185` (Measure+Predict structure), `:127-128` and
  `Anamorph:src/dsp/AnamorphEngine.cpp:1159` (the −60 dBFS mean-square freeze threshold — item 5,
  deliberately not copied)
- Precedent [Verified]: `Anamorph:src/dsp/ScopeBuffer.h:21-91` (power-of-two SPSC ring, one
  release-store per block), `Anamorph:src/dsp/AnamorphEngine.cpp:597-643` (`driveTanh` makeup + clean
  blend, identity at zero drive), `:58-81` (20 ms smoothing constant), `:44-56` (oversampler
  instances built at `prepare()`), `:777-846,1302-1327` (always-running chain, output-crossfade
  bypass), `:290-307` (dry-fill gating), `:160-209` (bitwise `sameParameters` gate and its
  maintenance rule)
- Precedent [Verified]: Anamorph ADR-0004 (click-free transition taxonomy), ADR-0010 (host-hidden
  `InternalState`), ADR-0013 (additive exact `raw` attribute), ADR-0008 (custom per-A/B-slot undo),
  ADR-0011 (Linux/X11 CPU render), ADR-0007 (Measure+Predict level match), ADR-0009 (*Anamorph's* —
  "no output clipper", the assumption item 5 bars from travelling; not this repository's ADR-0009),
  `Anamorph:docs/architecture/THREAD_MODEL.md` (zero worker threads),
  `Anamorph:docs/architecture/PERFORMANCE_BUDGET.md:207-266` (benchmark procedure adopted as a
  pattern)
- Governing rule: `CLAUDE.md` §3 (Anamorph read-only; cross-product reuse is ADR-recorded);
  `docs/OPEN_QUESTIONS.md` OQ-005 (resolved by this ADR); `DEVELOPMENT_BRIEF.md` §1.2 (shared-module
  assessment requested; shipping on schedule prioritised)
- Depends on: this repository's ADR-0006 (the LoudnessMatch adaptation and the ceiling inversion),
  ADR-0007 (widened StateSet, `schemaVersion`, raw-exact sessions), ADR-0005 (mode-switch contract
  that bars the Advanced-mode gating copy), ADR-0001 and ADR-0008 (the `AnabasisDSP` dependency
  boundary a shared module would have breached)
