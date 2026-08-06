# ADR-0015 — Pre-ship contract re-freeze: the round-2 defaults, the `int_meterTargets` removal, and the unit the Ceiling is allowed to advertise

**Status:** Accepted (2026-08-06 — owner round-2 directive of 2026-08-05, taken under the standing
autonomous-decision instruction that accompanied it; ⊕ for the post-v0.1.0 fine review like every
decision taken under the v0.1.0 blanket approval)

## Context

Three round-2 changes touched contracts the repository's own rules put behind a decision record,
and they landed in **PR #8** with prose justification only:

| Change | Rule that governs it |
|---|---|
| `ceiling` default **−1.0 → −0.1** | `PARAMETER_COMPATIBILITY_POLICY.md` rule 3 — "ranges, defaults and choice orderings of host-visible params are semantic … requires an ADR" |
| `truePeakMode` default **on → off** | the same rule 3 |
| `int_meterTargets` **removed** from `ANABASIS_INTERNAL` | `SESSION_COMPATIBILITY_POLICY.md` rule 1 — "no field … may be removed or have its meaning changed without an ADR + migration"; `SERIALIZATION_REGISTRY.md`'s own header calls any change to it "a serialization schema change — an AI-agent Hard Stop … enacted only by a superseding ADR" |

`tests/fixtures/parameter_registry.snapshot` was deliberately re-frozen for the first two, and
`SERIALIZATION_REGISTRY.md` §1 was edited from ten `int_*` properties to nine for the third.
`CLAUDE.md` lists "serialization schema change" among the hard stops that **a green build does not
clear**, and `DOCUMENTATION_LIFECYCLE_POLICY.md`'s trigger map names an **ADR** in the row for a
state-serialization schema change. None was written. This ADR is that record.

Writing it after the code is the ordinary shape here, not an exception: ADR-0012, ADR-0013 and
ADR-0014 all ratify mechanisms already in the tree with the tests that pin them, which is why they
carry `Verified` confidence at authoring. What is **not** ordinary — and is recorded rather than
smoothed over — is that the code shipped in a PR before its authority existed. The argument the PR
made ("nothing has shipped, so the cost is zero") is a sound argument for what this ADR should
*say*; it is not a substitute for saying it, because the reader who needs the reasoning is the one
who arrives after the window has closed and finds a default that disagrees with a signed-off design
document.

A fourth item is decided here because it is a direct consequence of the second: with `truePeakMode`
defaulting off, **the shipped default configuration no longer enforces a dBTP ceiling**, while the
`ceiling` parameter's value text still read `" dBTP"` unconditionally.

## Problem

1. **May a frozen default change at all, and what makes it free now and not later?** The rule says
   "requires an ADR" without saying what the ADR must establish.
2. **May a persisted field be deleted outright**, and what is the "migration" rule 1 demands when
   the field is being removed rather than reshaped?
3. **What does the Ceiling advertise** once the mode that makes it a true-peak limit is off by
   default — and is the honest answer a copy change, a units change, or a different default?

## Options

**The window (problems 1 and 2).**

- **A. Refuse the changes; hold them for a human sign-off gate.** The safest reading of the Hard
  Stop list. **Lost:** the owner's round-2 directive *is* the sign-off — it named these changes and
  instructed that architectural decisions be taken autonomously and documented rather than
  escalated — so the gate this option waits at has already opened. It also inverts the cost: every
  day the defaults stay wrong is a day nearer the build that freezes them.
- **B. Take the changes, re-freeze the snapshot, and record the rule that closes the window.**
  **Chosen.** `PARAMETER_COMPATIBILITY_POLICY.md` already anticipates exactly this moment in its
  own §"Getting it right the first time (pre-0.1.0)": *"No parameter has shipped, so none of the
  above has bitten yet. The cheapest possible moment to settle the surface is now."* The obligation
  the rule imposes is the ADR, not abstinence — so the ADR is the deliverable.
- **C. Keep `int_meterTargets` as a reserved, ignored field (a tombstone).** Costs nothing at
  runtime and preserves the letter of "fields are immutable". **Lost:** a reserved field is a
  permanent obligation — it appears in the registry, every future reader has to be told to ignore
  it, and the release checklist has to keep proving it round-trips — bought to protect **zero**
  sessions, because no build has left the repository. The §4.4 read rules already give an old
  developer session the same outcome for free (see the Decision's migration clause).
- **D. Deprecate rather than remove: keep writing the field, stop reading it.** **Lost:** strictly
  worse than C — it keeps the write path, the registry row *and* the obligation, and it makes the
  blob claim a feature the build does not have.

**The Ceiling's advertised unit (problem 3).**

- **E. Leave `" dBTP"` unconditional.** Zero work. **Lost:** it is false in the default
  configuration. `DSP_POLICY.md` invariant 3 says the ceiling *"is interpreted as dBTP when
  true-peak mode is on"*, and ADR-0006 item 3 spells out the other half — with it off, the clamp
  decides on the **sample peak**. A master rendered at defaults can therefore carry inter-sample
  peaks above 0 dBTP while the readout says −0.1 dBTP. Combined with the ceiling moving −1 → −0.1
  this is the least inter-sample-safe default the build has had, so the claim is not merely
  imprecise, it is wrong in the direction that costs the user a failed delivery check.
- **F. Print plain `" dB"` in both modes.** Honest everywhere and needs no plumbing. **Lost:** it
  discards the dBTP signal in the mode that has genuinely earned it, and leaves the meter's
  `dBTP` row sitting beside a ceiling that declines to use the word — inviting the reader to
  conclude they are different quantities.
- **G. The unit follows the mode: `" dBTP"` while true-peak mode is engaged, `" dB"` otherwise.**
  **Chosen.** The readout then states which of the two guarantees is live, which is the thing the
  user actually needs to know and the thing the toggle actually changes.
- **H. Default `truePeakMode` back on instead, and keep the unconditional unit.** Would make the
  claim true again. **Lost:** the owner's round-2 directive set this default deliberately, and it
  was re-affirmed when this finding was raised ("keep the current product decision … unless there
  is a genuine technical reason requiring otherwise"). There is no such technical reason: true-peak
  detection is a *quality* setting the user opts into, the ceiling guarantee itself
  (invariant 4 — the output never exceeds the ceiling) holds in both modes on its own terms, and
  nothing in the chain is unsafe with it off. What was broken was the description, not the DSP.

## Decision

1. **`ceiling` defaults to −0.1 dB** (`src/PluginParameters.cpp:160`, `EngineParameters.h:79`), and
   the one factory table that overrode it to −0.5 no longer needs to
   (`src/PresetManager.cpp`, "EDM Club").

2. **`truePeakMode` defaults to off** (`src/PluginParameters.cpp:218`,
   `EngineParameters.h`), and the true-peak **meter row** likewise (`int_tpMeterOn`,
   `src/InternalState.h:105`). These are independent fields with the same new default and one
   reason: neither should be on in a patch the user has not asked anything of yet.

3. **`int_meterTargets` is removed from the schema**, with the streaming-target display it existed
   for (OQ-008, superseded by the same directive). `ANABASIS_INTERNAL` carries **nine** `int_*`
   properties.

4. **The migration for item 3 is the §4.4 read rules, unchanged.** `InternalState::replaceFrom`
   applies defaults first and then overlays *only properties the schema knows*, so a session
   carrying `int_meterTargets` loads with every other field intact and the unknown one ignored;
   the writer emits the schema, not the input, so the field does not survive a re-save. No
   migration code exists, and none is owed — this is the read rule doing the job it was designed
   for, and it is the reason removal is cheap rather than the reason it is permitted.

5. **The Ceiling advertises the unit it enforces.** Its value text is `" dBTP"` while
   `truePeakMode` is engaged and `" dB"` otherwise. The mechanism is `CeilingUnitSource`
   (`src/PluginParameters.h:93`): the layout is a free function with no processor to ask, so the
   processor owns the holder, **declares it before `apvts`** so the layout's capture of its
   address happens after it is constructed (`src/PluginProcessor.h`), and points it at
   `truePeakMode`'s raw atomic once the APVTS exists (`src/PluginProcessor.cpp:22`). An unwired
   holder falls back to `" dB"` — the **weaker** claim, which is the safe direction for a
   guarantee. The suffix is display-only: `dbFrom` parses the leading float, so `getValueForText`
   is indifferent to which spelling it is handed, and neither the registry snapshot (ID · name ·
   range · default · steps · automatable) nor any serialized value can see it.

   **Correction (2026-08-06, PR #8 review):** an earlier revision of this item — and of the
   comment at the member — argued that the declaration order also made the holder *outlive* the
   lambda. It does not. The APVTS constructor hands every layout parameter to
   `AudioProcessor::addParameter`, so the parameters (and their value-text lambdas) belong to the
   **base** `AudioProcessor` and are destroyed by `~AudioProcessor`, which runs after every
   derived member; `apvts` is destroyed before the holder as well, leaving `truePeakRaw` dangling
   for the remainder of the derived teardown. Declaration order buys the **construction** half
   and nothing more. What makes the arrangement safe is a runtime fact: `getText` is called while
   the processor is live, and nothing in JUCE queries parameter text from a destructor — the
   hazard is latent, not live. Recorded rather than quietly fixed because the wrong version told
   a maintainer the lifetime was proven, and the remedy it implied (keep the two lines in this
   order) is not the remedy the real hazard would need — that would be a handle the parameters
   can own, not an ordering. The ownership arrangement is deliberately unchanged here: nothing
   reads the holder during teardown today, and inventing a shared handle for a display string
   would be a larger change than the risk earns.

6. **The window closes at the first build that leaves this repository.** Until then a default or a
   host-hidden field may be re-frozen under an ADR that records what changed and why. After it,
   `PARAMETER_COMPATIBILITY_POLICY.md` rule 3 and `SESSION_COMPATIBILITY_POLICY.md` rules 1–3 bind
   in full: a default change re-scales nothing but *does* change what an un-edited recall sounds
   like, and a removed field is a read path that must live for ever
   (`COMPATIBILITY_POLICY.md`, with a frozen fixture). The condition is **"has left the
   repository"**, not "is tagged" — a tester build is a shipped build.
   `docs/HANDOVER.md`'s Release Status row is where that fact is recorded; at the time of writing
   it says none has.

7. **No policy amendment.** Rule 3 and rule 1 are exercised, not rewritten: both name an ADR as the
   authority for exactly this, and this is it. `DSP_POLICY.md` invariants 3 and 4 need no
   amendment either, and that is the load-bearing observation behind option G — **both are already
   mode-conditional** ("interpreted as dBTP *when true-peak mode is on*"; "tolerance ≤ 0.1 dBTP *in
   true-peak mode*"), so the DSP was right about its own guarantee the whole time and only the
   user-facing copy over-claimed.

8. **Records amended, not rewritten.** ADR-0006 (Context and option E quote the old `ceiling` and
   `truePeakMode` defaults), ADR-0010 (§Decision's ten-field host-hidden inventory) and
   `DESIGN.md` §4.2/§4.3 carry an amendment banner pointing here and keep their original text.
   They are the record of what was decided on 2026-07-31; `PARAMETER_REGISTRY.md` and
   `SERIALIZATION_REGISTRY.md` are the descriptive ledgers of what the code holds today, and
   `SOURCE_OF_TRUTH.md` already ranks them accordingly.

## Consequences

- **`tests/fixtures/parameter_registry.snapshot` is re-frozen**, which is the normal workflow for a
  deliberate change (rule 2) and is now traceable to an authority instead of to a commit message.
- **The default patch is not inter-sample-safe, deliberately.** At defaults — OS Off, TP off — the
  limiter detects sample peaks and `CeilingClamp` hard-clamps samples at −0.1 dB, so true peaks can
  sit roughly 0.5–1.5 dB above the ceiling on dense programme. Invariant 4 is untouched (the output
  never exceeds the ceiling, on the quantity the mode measures); what changes is that the user must
  engage **TP** to make the number a dBTP number. The manual says so in four places and the readout
  says so continuously — that is the mitigation, and it is a description change because the DSP
  needed none.
- **A live-changing unit string is new behaviour for a host.** Generic editors re-query text on
  demand, so nothing has to be notified; the round-trip test pins that both spellings parse to the
  same value, which is what keeps automation and state indifferent to it.
- **`CeilingUnitSource`'s placement is a CONSTRUCTION rule, and only that.** Moving `ceilingUnit`
  below `apvts` in `PluginProcessor.h` would compile and hand the layout the address of a member
  that has not been constructed yet. It buys nothing at the other end — see the correction in
  item 5 — so a maintainer reading the member comment is told what the order does prove and what
  it does not, rather than being left with a false invariant that would suppress the real question
  if parameter text ever had to be queried during teardown.
- **Forecloses:** further default changes after the first shipped build without a migration story;
  re-introducing `int_meterTargets` under its old name with a different meaning (the ID is spent —
  a future targets feature picks a new one); and any reading of the Hard Stop list under which
  "nothing has shipped" excuses the *record* rather than the *cost*.
- **Doc-sync obligation discharged with this ADR:** registration in `ADR_INDEX.md`
  (`ADR_POLICY.md` rule 1), the amendment banners of item 8, pointer lines in both compatibility
  policies, and the `CHANGELOG.md` round-2 entry cross-linked here
  (`DOCUMENTATION_LIFECYCLE_POLICY.md`'s state-serialization row).

## Related code

- `src/PluginParameters.cpp:160` — `ceiling`, default −0.1, mode-aware value text
- `src/PluginParameters.cpp:218` — `truePeakMode`, default off
- `src/PluginParameters.h:93` — `CeilingUnitSource` (the unit source and its fallback)
- `src/PluginProcessor.h:81` · `src/PluginProcessor.cpp:22` — the holder's placement and wiring
- `src/InternalState.h` — `setDefaults()` (nine `int_*` properties; `tpMeterOn` false), and
  `replaceFrom`'s defaults-first overlay (item 4's migration)
- `src/dsp/EngineParameters.h:79` — the POD's `ceilingDbTp`/`truePeakMode` seeds
- `src/gui/LoudnessMeterView.{h,cpp}` — the TP row's snapshot seed and read fallback, both at the
  shipped default

Evidence [Verified]:
- Source: the files above
- Test: `AnabasisStateTests` `testTheCeilingAdvertisesTheUnitItEnforces` (both modes through the
  host-facing `getCurrentValueAsText`, plus the parse round-trip that pins the suffix as
  display-only); `testRegistrySnapshot` against the re-frozen fixture (defaults);
  `testFactoryPresets` (the un-overridden ceiling sits at −0.1); `testAbToleranceRules` and the
  §4.4 read-rule checks (an absent/unknown `ANABASIS_INTERNAL` property is ignored, item 4);
  `AnabasisTests` `testLimiterTruePeakMode` (which pins its own ceiling explicitly, precisely
  because its stimulus is calibrated against −1 dBTP and the test is about TP-awareness rather
  than about the default)
- Directive: the owner's round-2 instruction of 2026-08-05 and its re-affirmation of the
  `truePeakMode` default when option H was raised in review
