# ADR-0013 — The §5.4 release trim scales the limiter's AUTO release poles

**Status:** Accepted (2026-08-02 — owner decision on `OPEN_QUESTIONS.md` OQ-016, option 2, under
the v0.1.0 blanket approval of 2026-08-02; flagged ⊕ for the post-v0.1.0 fine review like every
decision taken under that approval)

## Context

The §5.4 adaptive engine computes a release trim in **octaves** (±1, `AdaptiveEngine::Trims`)
and `AnabasisEngine::process` applies it to the effective `limReleaseMs` by `2^octaves`. But
`LookaheadLimiter` consumes `limReleaseMs` **only in manual-release mode**, and `limAutoRelease`
defaults to **on**: the auto path stepped its two envelopes with the fixed `kAutoFastMs = 40` /
`kAutoSlowMs = 600` constants. So in the factory state the release trim was computed, published,
displayed as the Advanced view's overlay, latched by Freeze and serialized — and changed nothing
about the sound. OQ-016 recorded the two defensible readings ("working as specified — a trim is
inert while its host stage is inert" vs "not delivering the intent — the point of a release trim
is to adapt the release") and required an owner call, since the answer changes what the plugin
sounds like at factory defaults.

## Problem

Does the release trim reach the AUTO release path, and if so, how — without changing the
two-stage character of the auto release or widening the trim's authority?

## Options

- **A. Leave the trim manual-only.** Zero risk to the factory sound; but the §5.4 overlay
  displays a trim the user cannot hear in the default state, and the "four adaptive behaviours"
  claim stays scoped to three. Rejected by the owner call.
- **B. Scale both auto poles by the same `2^octaves` factor.** The fast/slow **ratio** (40/600)
  is preserved, so the two-stage character is unchanged; the trim's ±1-octave bound maps to a
  0.5×–2× pole scale, the same authority it already has over the manual time. **Chosen.**
- **C. Map the trim onto a blend or programme-dependent curve of its own.** More expressive, but
  it invents a second adaptation mechanism inside the limiter — new surface, new tests, and a
  precedent for trims growing private behaviours. Rejected.

## Decision

`LookaheadLimiter::setAutoReleaseScale (float factor)` — factor clamped to **[0.5, 2.0]**
(exactly the trim's ±1-octave bound) — scales **both** auto constants at every alpha
recomputation site (`prepare`, `setRate`, and the setter itself):
`aRelFast = onePoleMs (40 · s)`, `aRelSlow = onePoleMs (600 · s)`.
`AnabasisEngine::process` computes **one** factor `2^t.releaseOctaves` per block and feeds both
paths — the manual `limReleaseMs` multiply and `setAutoReleaseScale` — so whichever release mode
is engaged, the §5.4 behaviour is audible. A positive trim (sparse material) slows both poles by
the same proportion; a negative trim (dense material) quickens them; the ratio — and therefore
the dual-stage envelope shape — never moves.

This is the first trim whose target is not a user-facing parameter. The precedent it sets is
deliberately narrow: a trim may reach an **internal constant of its host stage** only by scaling
it inside the trim's existing bound, never by acquiring a new bound or a new state machine.

## Consequences

- The factory-default sound now adapts: on ordinary programme material the release trim converges
  away from zero and the auto envelope genuinely follows it. This is the owner-approved intent,
  and it is why the decision could not be taken as a bug fix.
- `MODE_AND_ADAPTATION_POLICY.md`'s scope note ("the release trim is inert at factory defaults")
  is deleted by this ADR — all four adaptive behaviours are audible at defaults.
- The invariant-7 null is untouched: with the adaptive engine converged at zero trim the factor
  is exactly `2^0 = 1` and both alphas recompute to their old values.
- Freeze semantics are unchanged — the latched vector holds, and through this path it now holds
  the auto envelope too.

## Related code

- `src/dsp/LookaheadLimiter.h:192-196` — `setAutoReleaseScale` (clamp + both alphas);
  `:77-78`, `:125-126` — the `prepare`/`setRate` recompute sites; `:429` — `autoScale`
- `src/dsp/AnabasisEngine.cpp:518-520` — one `2^octaves` factor feeding both release paths

Evidence [Verified]:
- Source: the two files above
- Test: `AnabasisTests` `testAutoReleaseFollowsTheTrimScale` — a burst into one slow-pole time
  constant of quiet shows scale 1 recovered vs scale 2 clearly behind (calibrated at 600 ms,
  where the two curves are furthest apart); mutation-verified — restoring the fixed-constant
  alphas kills both checks
