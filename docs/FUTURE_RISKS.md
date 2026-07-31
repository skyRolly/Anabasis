# FUTURE_RISKS.md

Forward-looking risks: things that have **not** gone wrong yet but plausibly will, with what would
trigger them and what would reduce them. A risk that has materialised moves to
`KNOWN_ISSUES.md`; a risk that caused an incident moves to `POSTMORTEMS.md`.

Risks are evidence-backed, not imagined (constraint C7): each one names the concrete property of
this project that creates it.

## Entry format

```
## RISK-0NN — <one-line summary>

**Likelihood:** Low | Medium | High   **Impact:** Low | Medium | High
**Trigger:** <what would turn this into a real problem>
**Mitigation:** <what reduces it, and what is already in place>
**Owner:** <role, or TODO>
```

---

## RISK-001 — JUCE version bump moves DSP, latency or state behaviour silently

**Likelihood:** Medium **Impact:** High
**Trigger:** Any change to the JUCE pin — including one taken "just to get a fix".
**Why it exists here:** JUCE provides the oversampling, the filters, the APVTS parameter/state
layer and the plugin wrappers. A bump can change numerical output, reported latency, or the
editor/host-embedding path without failing a compile.
**Mitigation:** immutable-SHA pinning; a bump is an `ARCHITECTURE_REVIEW_GATE` Build System change
requiring an ADR; `DEPENDENCY_POLICY.md` rule 2 requires the full suite + pluginval both modes ×3
on all three OSes **plus** a Level-5 audition after any bump. The strongest available check is a
**twin-dump comparison** — render a fixed scenario matrix under both versions and diff the output
bit-for-bit, including reported latencies.
**Owner:** TODO (OQ-009).

---

## RISK-002 — The parameter surface is frozen before it is understood

**Likelihood:** Medium **Impact:** High
**Trigger:** Shipping a build to a tester with a parameter table that P2–P4 later shows to be
wrong (a range too narrow, a macro curve that must change, a parameter that should have been two).
**Why it exists here:** parameter IDs, ranges and the macro mapping become permanent contract the
moment a build leaves the repository (`COMPATIBILITY_POLICY.md`), and the adaptive engine's macro
mapping is exactly the part of this design that is least knowable in advance.
**Mitigation:** settle the full parameter table in `DESIGN.md` **before** writing the layout
(`DEVELOPMENT_BRIEF.md` §24); prefer generous ranges chosen once; freeze the registry snapshot as
soon as the layout exists; keep pre-0.1.0 builds inside the team for as long as possible.
**Owner:** TODO.

---

## RISK-003 — The ceiling guarantee is asserted rather than proven

**Likelihood:** Medium **Impact:** High
**Trigger:** A true-peak overshoot on material the test suite never generated — inter-sample peaks
after saturation, an automation sweep at audio rate, an oversampling-factor switch mid-transient,
or a sample rate the suite does not cover.
**Why it exists here:** "output never exceeds the ceiling" is the product's core promise
(`DSP_POLICY.md` invariant 4), and a limiter that holds only on well-behaved music satisfies a
green test suite while failing a real master.
**Mitigation:** hostile-input testing is a policy requirement, not an optional extra
(`TESTING_POLICY.md` rule 5); a final safety clamp sits after the limiter regardless of what the
limiter did; the tolerance (≤ 0.1 dBTP) is stated numerically so a near-miss is a failure.
**Owner:** TODO.

---

## RISK-004 — The adaptive engine becomes audible as modulation

**Likelihood:** Medium **Impact:** Medium
**Trigger:** Time constants tuned on one kind of programme material; hysteresis too small; a
feature (crest factor, transient density) that reacts to a section change faster than the ear
forgives.
**Why it exists here:** §5.2 requires second-scale adaptation with *no audible modulation
whatsoever*, which is a much stronger claim than "it sounds fine on the test track".
**Mitigation:** `MODE_AND_ADAPTATION_POLICY.md` invariant 3 states the constraint as binding, with
a Freeze control as the escape hatch and a static-input convergence test; the loudness-matched
listening test across ≥ 5 genres (`TESTING_POLICY.md`) is where this actually gets caught.
**Owner:** TODO.

---

## RISK-005 — Manual-only validation for audio quality and host behaviour

**Likelihood:** High **Impact:** Medium
**Trigger:** Any release. Levels 1–4 are automated; audio quality, GUI appearance and real-DAW
behaviour are not, and never will be.
**Mitigation:** Level 5 is a **required release precondition**
(`RELEASE_POLICY.md` §"Preconditions"), not a
nice-to-have; the DAW smoke-test matrix (Reaper/Windows, Logic Pro/macOS AU) is named explicitly
so "we tested it" has a definition.
**Owner:** TODO.

---

## RISK-006 — Licensing is unresolved

**Likelihood:** High **Impact:** High (for distribution only)
**Trigger:** Any distribution beyond internal testing.
**Why it exists here:** the closed-source commercial model rules out JUCE's AGPLv3 arm, so the
commercial tier must be in place; the product also has no licence terms of its own, and VST3
distribution has separate Steinberg requirements. All three are owner/legal actions, not
engineering ones.
**Mitigation:** tracked as `OPEN_QUESTIONS.md` OQ-002 and as `RELEASE_POLICY.md` precondition 9,
so it cannot be passed over by a green build.
**Owner:** project owner.

---

## RISK-007 — Brand drift between Anabasis and Anamorph

**Likelihood:** Medium **Impact:** Low
**Trigger:** Independent UI evolution in either product — a LookAndFeel tweak, a Settings-page
reorganisation, a different Bypass placement.
**Why it exists here:** the two products deliberately share a frame, an About page, a Settings
page, a Bypass placement and a preset/A-B interaction model, but the code is (for now) copied
rather than shared, and Anabasis may not modify Anamorph.
**Mitigation:** `docs/BRAND_CONSISTENCY_CHECKLIST.md` is a P5 exit criterion checked item by item.
The shared-module question is **settled, not open**: **ADR-0009** (Accepted 2026-07-31) chose
copy-and-adapt with provenance headers and no shared module for v1, and schedules the extraction
revisit as a product-family decision **after v0.1.0 ships** — so this risk is accepted for one
release, with a named review point rather than an open question (OQ-005 is `Resolved`).
**Owner:** TODO.

---

## RISK-008 — The measurement-tap latency contract rests on an unverified detector-delay bound

**Likelihood:** Low **Impact:** High
**Trigger:** The first impulse-response latency measurement at P2 (`testReportedLatencyMatchesImpulse`).
**Why it exists here:** `DESIGN.md` §3.2 resolves `DSP_POLICY.md` invariants 2/5 by making
true-peak detection a **measurement tap**, which yields "with oversampling off, reported latency
is exactly the lookahead allowance and the detector adds nothing" — but only while the estimator's
group delay fits *inside* the 0.5 ms **minimum engaged** lookahead. The design arithmetic (BS.1770-4 Annex 2, 48 coefficients, 4 phases →
(48−1)/2 = 23.5 upsampled ≈ 5.9 base samples ≈ 0.122 ms at 48 kHz) says it fits with room to
spare, and because lookahead is specified in **milliseconds** the margin holds across sample
rates. It is nonetheless arithmetic about an unwritten module, not a measurement (C2), and the
whole latency contract plus ADR-0003/0004 sit on it.
**Mitigation:** verify with the first impulse test at P2, *before* anything else is built on the
claim. §3.3's constant-allowance decision makes the fallback cheap: the detector's delay is
absorbed by raising the *minimum engaged* read offset inside the fixed 10 ms line, so the
**reported** figure never moves and no ADR amendment or Architecture Review is triggered. The
residual exposure is to the accuracy contract (invariant 11), not the latency one.
**Owner:** TODO.

---

## RISK-009 — The variable-font direction depends on a licence that does not exist yet

**Likelihood:** Medium **Impact:** Low
**Trigger:** P5 UI work reaching typography with no approved font licence.
**Why it exists here:** `DEVELOPMENT_BRIEF.md` §2 and §8 ask Anabasis to use JUCE 9's variable-font
support, and `DESIGN.md` §6.1 proposes embedding one variable font — but any third-party asset
needs its licence stated and owner approval **before** adoption (brief §13), and no font has been
proposed or cleared. Anamorph offers no precedent: it embeds nothing and uses the platform default
sans-serif [Verified], which is also the stated fallback.
**Mitigation:** raise the licence question early enough that P5 is not blocked waiting on it; the
platform-default path (plus Anamorph's fit-to-width drawing idioms) is a complete fallback, so the
risk is schedule and brand consistency, not feasibility. Track alongside OQ-002's licence work.
**Owner:** TODO.

---

## Adding a risk

Append the next `RISK-0NN`. State the concrete property of this project that creates the risk — a
risk with no named cause is speculation, and speculation is not documentation (C7). When a risk
materialises, move it to `KNOWN_ISSUES.md` (or `POSTMORTEMS.md` if it caused an incident) and note
here that it did.
