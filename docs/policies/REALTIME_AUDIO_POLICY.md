# REALTIME_AUDIO_POLICY.md

**Priority: 1 (highest).** System Policy. Binding constraint on `processBlock`, the engine's
`process`, and every DSP module's `process`/`reset`.

Inherited unchanged from Anamorph — this is a framework-level invariant, not a product-specific
one.

## Rule

The audio thread must be **deterministic, lock-free, and allocation-free**. Every operation on
the audio path must be O(1) or bounded/deterministic in time.

## Forbidden on the audio thread (hard red line)

`new` / `delete` · `malloc` / `free` · any heap allocation or container resize (`std::vector`
resize/`push_back`, `juce::AudioBuffer::setSize`) · `mutex` / `lock` / `condition_variable` ·
blocking waits · filesystem IO · network IO · `sleep` · C++ exceptions thrown on the path ·
`future` / `promise` / `std::async` · thread creation · `system()` / `fork()` / subprocess.

## Permitted

- Reads/writes of pre-allocated buffers and scalar state.
- Atomic loads/stores (relaxed for published meters; release/acquire for a scope ring).
- In-place IIR coefficient recompute — bounded.
- `std::fill` over a pre-sized buffer (no resize) — e.g. `reset()` and lookahead-buffer flush.
- Transcendental functions (`tanh`, `sin`, `log10`, `pow`) — bounded, no allocation.
- `juce::ScopedNoDenormals` (**required**; active for the whole block — §2 mandates FTZ/DAZ
  denormal protection).

## Anabasis-specific consequences

These follow from the chain in `DEVELOPMENT_BRIEF.md` §3–§5 and are called out because they are
the places this project is most likely to violate the rule:

1. **Lookahead buffers are sized in `prepare()` for the maximum lookahead (10 ms) at the maximum
   sample rate and the maximum oversampling factor** — a lookahead or oversampling-factor change
   must never reallocate on the audio thread.
2. **Oversampling factor changes are latched**, applied only at a reset / a silent or crossfaded
   boundary, so no allocation and no latency change happens mid-block.
3. **LUFS/BS.1770 gated integration keeps bounded state.** An integrated-loudness measurement over
   an unbounded history must be implemented as a fixed-size accumulator (running block statistics),
   never a growing container.
4. **Feature extraction for the adaptive engine runs on pre-allocated state.** Crest factor,
   spectral tilt/centroid and transient density are computed into fixed buffers; any FFT is
   pre-planned in `prepare()`.
5. **Nothing in the metering or GUI path allocates on the audio thread.** Meters publish through
   atomics; scope/GR-history data crosses through a pre-allocated lock-free ring
   (`THREADING_POLICY.md`).
6. **Denormals**: the limiter's release envelope and the compressor's detector decay toward zero
   and are classic denormal generators. `ScopedNoDenormals` is mandatory, and envelope state
   should additionally be flushed to zero below a threshold rather than relied on to underflow.

## Current compliance

**Audited (section updated 2026-08-05 — it had read "TODO (no code yet)" since P0 and stayed TODO
for three phases after the deliverable it scheduled actually landed; the flip was owed at
end-of-P2 and is applied late, not backdated).** The module-by-module review with evidence
citations is `docs/architecture/REALTIME_SAFETY_AUDIT.md`; **its audited revision is the P2
transition-layer commit (2026-08-01)**, stated there rather than glossed. Audio-thread code added
after that revision — the P3 meters and their rings, the §5.4 feature extractor, the spectrum
taps — entered through the `THREADING_POLICY.md` permitted-path table (each addition is a named
row with its ADR) and the round-41/42 two-threaded stimulus + ThreadSanitizer passes recorded in
`HANDOVER.md`, but the audit *document* has not been re-baselined against the v0.1.0 tree. That
re-baseline is a recorded gap (`DOCUMENTATION_COVERAGE.md` §Known coverage gaps), not a claim
(constraint C7).

## Enforcement

- Any change touching an audio path is reviewed against the forbidden list.
- Buffer sizing must happen in `prepare()`. If a feature needs more scratch, grow it in
  `prepare()`, never in `process()`.
- A change that could introduce an unbounded or per-block allocation triggers the
  **Architecture Review Gate** and an **AI Agent Hard Stop** (`AI_AGENT_POLICY.md`).
- Changing this policy requires an ADR.
