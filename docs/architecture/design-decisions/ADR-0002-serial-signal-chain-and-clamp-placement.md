# ADR-0002 — Fixed serial signal chain; EQ Pre/Post as the only mobility; ceiling clamp always last before dither

**Status:** Accepted (2026-07-31 — owner sign-off on `docs/DESIGN.md`)

## Context

`DEVELOPMENT_BRIEF.md` §3 fixes the processing order as Input Gain → EQ → Comp → Clipper/Sat →
Limiter → Ceiling → Dither → Output, and §4.4 gives the EQ a position switch: "Pre (default) /
Post, i.e. before or after the limiter". `DSP_POLICY.md` invariant 1 restates that chain and makes
any change to stage order or stage placement a DSP signal-order change — an
`ARCHITECTURE_REVIEW_GATE.md` item and an AI-agent Hard Stop (`CLAUDE.md`). Invariant 4 states the
product promise: the output never exceeds the ceiling, under **any** parameter combination, held by
a final safety clamp sitting after the limiter and before dither, tolerance ≤ 0.1 dBTP.

`ADR_POLICY.md` makes an ADR mandatory for DSP signal flow (stage order/placement), and rule 5
makes an ADR the only instrument that can enact a Policy change. `DESIGN.md` §1.2 therefore
deliberately left `DSP_POLICY.md` untouched and deferred the amendment to this ADR, flagging it on
the §11 sign-off checklist so a human ratifies the reading rather than inheriting it from a
diagram.

**This ADR carries a policy amendment.** See *Decision*, item 6.

## Problem

Invariant 1 prints the chain as `… Limiter → Ceiling → Dither` and says the switch moves the EQ
block "before the compressor, or after the limiter". It never says where Post-EQ sits **relative to
the ceiling clamp**, and the two readings are not equivalent:

- `… Limiter → Ceiling → EQ(post) → Dither` — the literal reading of the diagram, since the diagram
  shows the clamp immediately after the limiter and "after the limiter" then means after both.
- `… Limiter → EQ(post) → Ceiling → Dither` — the clamp stays last before dither in both positions.

Under the literal reading the EQ's shelves and bells (±12 dB, §4.2 rows 36/38/40/43) and its tilt
(±3 dB) sit **downstream of the only stage that enforces the ceiling**. A +12 dB post-limiter shelf
re-introduces overshoot that nothing removes, so `testOutputNeverExceedsCeiling` cannot pass in the
Post position and invariant 4 is unsatisfiable by construction — not by a bug, by the stage order
itself. The question is not obvious because both readings are faithful to the brief's sentence; the
brief simply did not distinguish the limiter from the clamp when it wrote "after the limiter".

## Options

- **A. Literal reading — `Limiter → Ceiling → EQ(post) → Dither`.** Faithful to invariant 1's
  printed diagram and needs no policy edit. **Lost:** it makes invariant 4 unsatisfiable in the Post
  position — a +12 dB shelf downstream of the clamp escapes the guarantee entirely, and the ≤ 0.1
  dBTP tolerance becomes untestable rather than merely hard to meet. An invariant that cannot hold
  is the worse reading of an ambiguous pair. It also breaks invariant 12 (dither last before output,
  gain staging unchanged by enabling it) in spirit: the last thing shaping level would be an EQ, not
  the clamp.
- **B. `Limiter → EQ(post) → Ceiling → Dither`; the clamp is always the last stage before dither in
  both positions.** Satisfies the brief's "after the limiter" and keeps invariant 4 satisfiable
  everywhere. Costs one wording amendment to invariant 1, which is a Hard Stop item. **Chosen.**
- **C. Two clamp instances — one after the limiter, one after the Post-EQ.** Would satisfy both
  readings simultaneously. **Lost:** exactly one of the two is inert in each position, so it doubles
  the clamp state, doubles the true-peak estimator taps and doubles the guarded surface for zero
  audible benefit; and it still adds a stage to invariant 1's stage list, so it needs the *same*
  policy amendment plus a larger one.
- **D. Constrain the EQ in the Post position — clamp shelf/bell gains to ≤ 0 dB when
  `eqPosition = Post`.** Removes the overshoot at the source. **Lost:** it makes a parameter's
  effective range depend on another parameter's value, and ranges are frozen contract from v0.1.0
  (`PARAMETER_COMPATIBILITY_POLICY.md` rule 3) — the same saved `eqHighShelfGain` would mean
  different things in the two positions. It also makes the position switch a *value* change (a
  boosting EQ silently flattens on switching to Post), which is the one thing the switch must not
  be.
- **E. Free EQ placement / a third position.** **Lost:** the brief and invariant 1 define exactly
  two positions; every additional position multiplies the chain-order, click-free (invariant 8) and
  hostile-input test matrices, and moves the clamp's estimator tap to a different signal point
  again.
- **F. Amend invariant 4 instead of invariant 1 — weaken the guarantee to "the limiter's output
  never exceeds the ceiling", permitting Post-EQ to boost past it.** **Lost:** `DSP_POLICY.md` calls
  invariant 4 the product's core promise and weakening it an Architecture Review Gate item in its
  own right. A maximizer whose output can exceed its own ceiling has no contract to sell; amending
  the diagram is a wording fix, amending the promise is a product change.

## Decision

1. **The chain is strictly serial and fixed.** With `eqPosition = Pre` (row 45, default):

   ```
   Input Gain → EQ → Comp → [OS region: Clip/Sat → Limiter] → Ceiling → Dither → Output
   ```

   With `eqPosition = Post`:

   ```
   Input Gain → Comp → [OS region: Clip/Sat → Limiter] → EQ → Ceiling → Dither → Output
   ```

2. **`eqPosition` is the only mobility in the chain.** No other stage moves under any parameter
   combination, host state, oversampling factor or offline-render setting. The dynamic HF tame
   (`dynTilt`, row 48) is a sub-block *inside* the Clipper/Sat stage, not a chain stage, so it does
   not change this list (`DESIGN.md` §2.2).

   **What that implies for the sample rate it runs at, stated so a P2 author has a sentence to
   cite** *(note added 2026-07-31, same day)*: being inside the Clipper/Sat stage puts `dynTilt`
   inside the oversampled region (ADR-0003), so it runs at the **oversampled** rate. That does not
   conflict with `DSP_POLICY.md` invariant 5's "the EQ … stays at base rate" — invariant 5
   enumerates **chain stages**, and "the EQ" there is the `eqPosition`-mobile EQ *stage*, not every
   filter in the signal path. Invariant 5's named exception (the true-peak estimator) exists because
   that estimator is a stage-external *tap* running at its own rate; `dynTilt` needs no exception
   because it is not a stage at all. A one-band shelf that is part of a nonlinear stage belongs at
   that stage's rate — running it at base rate inside an oversampled block would require rate
   conversion around a filter whose whole purpose is to shape what the clipper then distorts.

3. **The ceiling clamp is always the last stage before dither, in both EQ positions.** It is a
   structurally separate stage (`CeilingClamp.h`), never folded into the limiter. It runs at base
   rate and sits **outside** the oversampled region (§3.1), because it must be downstream of a
   Post-position EQ that is itself outside that region.

4. **The clamp carries its own true-peak estimator tap on its own input.** Once Post-EQ sits between
   the limiter and the clamp, the limiter's detector saw a different signal point, so the clamp
   cannot reuse the limiter's true-peak envelope. In true-peak mode the clamp's gain acts on its own
   TP estimate with a sample-level hard clip as the backstop; sample peak otherwise. Tolerance
   ≤ 0.1 dBTP (invariant 4).

5. **The position switch is a rewire, not a value change.** It is a discrete change routed through
   the §2.8 asymmetric raised-cosine duck (~6 ms out / ~28 ms in), with the forced-duck atomic
   requested *before* the swap. No parameter value moves across the switch. The all-flat skip (§2.2)
   keeps invariant 7 structurally true in both positions.

6. **Policy amendment (this is the Hard-Stop item ratified at sign-off).** `DSP_POLICY.md`
   invariant 1 is amended to print the clamp as always-last-before-dither and to say where Post-EQ
   sits. Its chain block and following sentence become:

   ```
   Input Gain → EQ (pre by default) → Compressor → Clipper + Saturation (colour)
   → Limiter (lookahead + true peak) → [EQ (post)] → Ceiling → Dither → Output
   ```

   > The EQ position switch (Pre/Post) moves the EQ block **only** between the two defined
   > positions — before the compressor, or after the limiter and **before the ceiling clamp**. The
   > ceiling clamp is **always** the last stage before dither, in both positions. No other
   > reordering exists.

   *(Scope of "nothing else changes", recorded 2026-07-31 so a diff of this block against
   `DSP_POLICY.md` does not read an authorised edit as a divergence — the same note ADR-0003 carries
   for its two blocks. The clause above is what this item **prescribes verbatim**. The enacted
   invariant additionally carries (i) a **"Why the clamp placement is part of this invariant"**
   rationale paragraph, which states this ADR's own reasoning — the +12 dB post-limiter shelf
   argument made in the **Problem** statement and in option A's rejection — in the policy rather
   than only here, and (ii) an amended **`Guarded by:`**
   line naming the both-EQ-positions sweep, which is authorised by **item 7 below**, not by this
   item. Neither adds a rule: the chain block and the sentence above are the whole of the
   contract.)*

   Nothing else in invariant 1 changes; invariant 4 is not amended. This is ambiguity being
   resolved, not a reorder being asserted — but resolving it still touches DSP signal order, which
   is an `ARCHITECTURE_REVIEW_GATE.md` item and an AI-agent Hard Stop (`CLAUDE.md`,
   `AI_AGENT_POLICY.md`). It was carried on the `DESIGN.md` §11 sign-off checklist as a Hard-Stop
   line and ratified by the owner on 2026-07-31; `ADR_POLICY.md` rule 5 makes this ADR the
   instrument that enacts it.

7. **Guards.** `testOutputNeverExceedsCeiling` runs its hostile-input sweep **across both EQ
   positions**, with the Post case including a full +12 dB shelf boost. The chain-order /
   transfer-order test asserts both orders explicitly. `eqPosition` gets its own click-free path
   test under invariant 8.

## Consequences

- **Invariant 4 becomes satisfiable in every reachable state**, which is the whole point: no
  parameter combination — including a +12 dB post-limiter shelf on top of a limiter already at the
  ceiling — can push audio past the clamp.
- **The clamp is a safety stage, not a quality stage.** Boosting EQ after the limiter drives the
  clamp hard and trades fidelity for the guarantee. That is the correct priority for a safety clamp
  and is accepted, not mitigated.
- **A second true-peak estimator instance exists** (limiter detector + clamp tap). Its cost is
  charged against §9's ≤ 1.5% limiter + TP-detection allocation, and the ≤ 0.1 dB accuracy test
  (invariant 11) must cover the clamp's tap as well as the limiter's, because their input paths
  differ.
- **The oversampling region is bounded by this decision, not only by §3.1's aliasing argument.**
  The clamp cannot be pulled inside the OS region without putting it upstream of a Post-position EQ,
  so "clamp at base rate" is now load-bearing for invariant 4 rather than a convenience.
- **Deliberate inversion of the sibling product.** Anamorph has no output clipper at all — a
  headroom decision recorded in its ADR-0009. Anabasis inverts it on purpose: a maximizer's contract
  *is* the ceiling. Any code adapted from Anamorph under ADR-0009 (this repository's) must not carry
  the "no output clamp" assumption across.
- **Forecloses:** any stage after the clamp other than dither; free EQ placement or a third
  position; per-position EQ range differences; folding the clamp into the limiter as a shared
  gain stage. Each of those re-enters the Architecture Review Gate.
- **Invariant 7 is unaffected but stays conditioned as `DESIGN.md` §4.2 states it:** with all
  defaults the EQ is flat and skipped and the clamp is inert below the ceiling, but near-full-scale
  material engages the default ⊕ −1.0 dBTP ceiling by design, which `testNullWithDefaults`'s
  stimulus level must respect.
- **Doc-sync obligation:** the invariant 1 edit above lands in `DSP_POLICY.md` with this ADR, and
  the ADR is registered in `ADR_INDEX.md` (`ADR_POLICY.md` rule 1); the architecture chain diagram
  and `procedures/TESTING.md`'s entry for `testOutputNeverExceedsCeiling` (both-positions stimulus)
  follow per `DOCUMENTATION_LIFECYCLE_POLICY.md`.

## Related code

None yet — P1 onward. Planned: `src/dsp/AnabasisEngine.{h,cpp}` (chain owner, position rewire,
transitions), `src/dsp/CeilingClamp.h` (final clamp, invariant 4), `src/dsp/TiltEq.h`,
`src/dsp/LookaheadLimiter.{h,cpp}`, `src/dsp/ClipSat.{h,cpp}`, `src/dsp/MasteringComp.{h,cpp}`,
`src/dsp/TruePeak.h` (shared BS.1770-4 estimator; the clamp instantiates its own tap),
`src/dsp/Dither.h`, `src/dsp/EngineParameters.h` (POD snapshot carrying `eqPosition`),
`src/PluginParameters.{h,cpp}` (`pid::eqPosition`, row 45).

Evidence [Unverified] — Anabasis has no `src/`, so every runtime claim above is the contract the
code must satisfy, not a measurement (constraint C2):

- Design: `docs/DESIGN.md` §1.2 (chain diagram, EQ mobility, Hard-Stop callout), §2.6 (ceiling
  clamp as a separate always-last stage, own TP tap, ≤ 0.1 dBTP), §2.8 (duck for discrete rewires),
  §3.1 (OS region excludes the clamp), §4.2 rows 34–45 (EQ surface, ±12 dB gains, `eqPosition`
  Pre/Post default Pre), §10 (this ADR's scope), §11 (sign-off checklist, Hard-Stop line)
- Research: `worklogs/2026-07-30-p0-anamorph-research.md`
- Policy amended by this ADR: `docs/policies/DSP_POLICY.md` invariant 1 (`ADR_POLICY.md` rule 5)
- Precedent [Verified]: `Anamorph:src/dsp/AnamorphEngine.cpp:777-846,1302-1327` — always-running
  chain with an output crossfade for bypass, the structure the mobile-EQ rewire plugs into
- Precedent [Verified]: `Anamorph:docs/architecture/design-decisions/ADR-0009-nan-selfheal-nyquist-clamp.md`
  — "no output clipper anywhere", the decision Anabasis deliberately inverts
- Precedent [Verified]: `Anamorph:docs/architecture/design-decisions/ADR-0004-clickfree-transition-strategy.md`
  — the three-mechanism duck/crossfade taxonomy the position switch is routed through
