# BRAND_CONSISTENCY_CHECKLIST.md

The **P5 exit criterion**: "Brand-consistency checklist against Anamorph passes item by item"
(`docs/DEVELOPMENT_BRIEF.md` §11). Also the standing guard against brand drift (`FUTURE_RISKS.md`
RISK-007).

Anamorph is a **read-only reference** for every check below. Verification means opening the
Anamorph source/UI and comparing — never editing it.

## How to use this

Each item is checked by a human against a running build of both plugins, or against Anamorph's
source where the property is structural. Record the result and the date. An item that cannot pass
is either fixed, or becomes a deliberate deviation with an ADR and owner sign-off — never a silent
difference.

---

## A. Structural — must match (§1.2 "Inherit")

- [ ] **Overall frame layout** — window proportions, title/top bar, the relationship between the
      header, the main view and the bottom strip.
- [ ] **About page** — how it is opened, what it contains, how it is dismissed. Version + build
      number are shown in the same place and the same format.
- [ ] **Settings page** — the same overlay mechanism, the same ordering discipline, the same
      grouping. Anabasis-specific rows (oversampling factor and **phase mode**, offline-render
      quality, metering options) slot into that organisation rather than replacing it
      (`DEVELOPMENT_BRIEF.md` §7).
- [ ] **Bypass placement** — the same position and the same interaction. Explicitly named in §1.2
      as non-negotiable.
- [ ] **Preset system** — browsing, saving, the dirty marker, factory vs user presets, the preset
      file convention (`.anabasis` mirroring `.anamorph`).
- [ ] **A/B compare** — the same interaction model, the same slot semantics, the same relationship
      to undo.
- [ ] **Window resize / scaling** — the same behaviour, the same persistence of window size, the
      same HiDPI handling.
- [ ] **Tooltips** — the same presentation and the same governing toggle.
- [ ] **UI Animation toggle** — present, and disabling it changes nothing functional.

## B. Visual system — must match

- [ ] **Typography** — the same family and the same scale relationships. JUCE 9 variable-font
      support may be used, but not to introduce a different typeface.
- [ ] **Brand colour system** — the same base palette, the same treatment of background, panel,
      text and disabled states.
- [ ] **Spacing rules** — the same margins, gutters and corner radii.
- [ ] **Control drawing** — knobs, sliders and buttons read as the same family (including the
      reset interaction and its easing).
- [ ] **Iconography** — the same weight and construction.

## C. Deliberate differentiation — must differ (§1.2 "Differentiate")

- [ ] **Main view expresses a maximizer**: one large primary knob as the unambiguous visual focus,
      plus metering visualisation (gain-reduction history, LUFS meters) — **not** a stereo-field
      display.
- [ ] **A distinct accent colour** within the shared design language, so the two products are
      distinguishable at a glance.
- [ ] **Advanced mode is zoned** (Comp / Clip / Limiter / EQ) with a shared metering strip along
      the bottom.
- [ ] The style is **modern, flat, data-visualisation-driven**. Skeuomorphic hardware-panel design
      is explicitly forbidden (§8).

## D. Accessibility and behaviour

- [ ] Complete parameter and automation names.
- [ ] Keyboard operability.
- [ ] **Colour-blind-safe metering palette.**
- [ ] Animation is fluid but restrained, targeting 60 fps, and fully governed by the toggle.
- [ ] Vector drawing throughout; resizable; HiDPI-aware.

## E. What must *not* be inherited

- [ ] No third-party code or asset bound by a copyright or restrictive licence (GPL in
      particular) has been copied from anywhere (§1.2, §13).
- [ ] Competing products were used as **behavioural and visual benchmarks only** — no reverse
      engineering, no copied assets, no copied UI layout.

---

## Result

| Section | Result | Date | Checked by |
|---|---|---|---|
| A Structural | TODO | — | — |
| B Visual system | TODO | — | — |
| C Differentiation | TODO | — | — |
| D Accessibility | TODO | — | — |
| E Exclusions | TODO | — | — |

Deviations approved by ADR: *(none yet)*
