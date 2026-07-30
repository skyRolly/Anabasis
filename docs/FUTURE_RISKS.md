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
**Mitigation:** Level 5 is a **required release precondition** (`RELEASE_POLICY.md` §8), not a
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
**Mitigation:** `docs/BRAND_CONSISTENCY_CHECKLIST.md` is a P5 exit criterion checked item by item;
whether to extract a shared UI module is tracked as OQ-005.
**Owner:** TODO.

---

## Adding a risk

Append the next `RISK-0NN`. State the concrete property of this project that creates the risk — a
risk with no named cause is speculation, and speculation is not documentation (C7). When a risk
materialises, move it to `KNOWN_ISSUES.md` (or `POSTMORTEMS.md` if it caused an incident) and note
here that it did.
