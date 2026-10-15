# LATENCY_MODEL.md

The **reference for what Anabasis reports to the host and why** — the latency authority
`COMPATIBILITY_POLICY.md` §"Where each contract is specified" points at. The deciding
records are **ADR-0004** (the constant lookahead allowance) and **ADR-0003** (true-peak as
a measurement tap); this file is the descriptive map of the implementation. A
**reported-latency change is an AI-agent Hard Stop** (`ARCHITECTURE_REVIEW_GATE.md`) and
requires an ADR.

## The contract in one line

```
reportedLatency = maxLookaheadSamples(10 ms, sr) + osLatency(factor, phaseMode)
```

Two terms, nothing else. `src/dsp/Latency.h` is the **single source of this arithmetic** —
both the engine's actual delay structure and the wrapper's `predictLatencySamples()` call
the same functions, so the reported figure and the real group delay cannot drift apart
without failing `testReportedLatencyMatchesImpulse`.

## Term 1 — the constant lookahead allowance (ADR-0004)

The limiter's lookahead parameter spans 0.5–10 ms with **no zero/off position** (OQ-010),
but the *reported* contribution is the **constant 10 ms maximum**
(`kMaxLookaheadMs`, `src/dsp/Latency.h:25-31`): the limiter reads at a variable offset
inside a fixed 10 ms delay line and the engine pads the difference. Bought with ~8 ms
nobody asked for, deliberately (ADR-0004): everything that carries the `lookahead`
parameter — **moving the knob, browsing presets, A/B switches, undo/redo, session loads** —
changes reported latency by exactly **nothing**, so host PDC never re-syncs mid-session.
This is also why `lookahead` is non-automatable-advisory: the engaged value is a live read
offset, not a PDC input (`PARAMETER_REGISTRY.md` §non-automatable rows).

**In true-peak mode the allowance is shared** (ADR-0041, Accepted 2026-09-27). The ceiling
clamp's true-peak path needs a short delay of its own — `D = attack + 30` samples,
`CeilingClamp::truePeakDelayFor` (41 at 44.1 kHz, 42 at 48 kHz, 54 at 96 kHz, 78 at
192 kHz) — and takes it OUT of the allowance rather than adding it: the region's lookahead
line becomes `maxLookaheadSamples − D`, the clamp delays by `D`, and the sum is the same
allowance. The reported figure is therefore identical in both modes; what TP mode costs is
the top of the engaged range, capped at `10 ms − D` (9.125 ms at 48 kHz). Toggling the mode
changes the line's length, so it is latched at the §2.8 duck's silent bottom exactly like an
oversampling change (`AnabasisEngine::latchOsConfig`), never mid-block. Engaging it while audio
plays enters that bottom at the toggle block rather than after the duck's out-leg (ADR-0041
decision 5, revised in the PR #42 review): the out-leg would still be emitted by the sample-peak composition.
Neither changes the total — the latch moves `D` between the line and the clamp, not the sum.

## Term 2 — the oversampling contribution

A pure function of `(factor, phaseMode)` — no signal-dependent term, and **integer** by
construction (`useIntegerLatency = true` on every `juce::dsp::Oversampling` instance, built
at `prepare`). The per-configuration values live in the `kOsLatMin` / `kOsLatLin` tables in
`src/dsp/Latency.h` and are quoted in **no normative document — this one included**: they
are `getLatencyInSamples()` *measured against the pinned JUCE tree*, so a prose copy is a
stale copy the day the pin moves. (The one other occurrence in the tree is a dated
2026-08-01 coverage-journal entry — a historical record of that day's measurement, which is
what dated entries are for, not a live copy to keep in step.) Structure worth knowing:

- **Off contributes zero.** The chain at OS Off has no oversampler in the path at all.
- The values are group delays **in samples**, which is why the function ignores its
  sample-rate argument — the same cascade at 96 kHz delays the same sample count.
- `kMaxOsLatencySamples` bounds the dry-ring/bypass-alignment headroom, and a
  `static_assert` keeps it ≥ every table entry *at compile time* — the runtime `jassert`
  version compiled out exactly in the builds that ship.
- **A JUCE bump that changes either filter design fails the matrix test.** That is
  RISK-001's tripwire working, not an inconvenience: re-measure, update the tables, and
  the bump review (`DEPENDENCY_POLICY.md` rule 2) carries the change. `prepare()`
  additionally records a per-instance table-vs-`getLatencyInSamples()` comparison flag, so
  drift is caught before the matrix test runs.

## What adds nothing: the true-peak measurement tap (ADR-0003)

True-peak detection runs as a **measurement tap** off the signal path — a 4-phase
polyphase estimator whose group delay fits inside the 0.5 ms *minimum engaged* lookahead
with margin — so dBTP metering and the limiter's true-peak detection contribute **zero** to
reported latency at every oversampling setting, including Off. The ceiling clamp's
true-peak path (ADR-0041) does delay its audio, by `D`, and contributes zero to the
REPORTED figure for the reason given under Term 1: its delay is inside the allowance. The design-time arithmetic
behind this was RISK-008; the entry records its measured resolution (the estimator's
reporting lag is bounded well inside the minimum window, and the margin *grows* with
sample rate). Residual exposure is accuracy (`DSP_POLICY.md` invariant 11's meter), not
latency.

## When the figure recomputes

**One call site** — `updateLatency()` (`src/PluginProcessor.cpp:896-914` — the definition through its
`setLatencySamples` call, the only
`setLatencySamples` caller; it no-ops when the figure is unchanged). Reached from five
triggers (ADR-0004 item 5):

| Trigger | Path |
|---|---|
| `int_oversample` change | `InternalState::onLatencyInputChanged` |
| `int_osPhase` change | same callback |
| `int_offlineQuality` change | same callback |
| `prepareToPlay` | `src/PluginProcessor.cpp:813` (`prepareToPlay`, its `updateLatency()` call) |
| `setNonRealtime` | `src/PluginProcessor.cpp:886-894` (`setNonRealtime`) |

A **session load is one latency event, not six**: `InternalState::replaceFrom` batches the
whole read behind `ScopedLatencyBatch`, so the reported figure never walks through the
default (Off) value mid-load (`src/InternalState.h` — the batch's own comment; pinned by
`testLatencyNotifyIsBatchedAcrossARead`). `setStateInformation` ends with one further,
deliberately redundant `updateLatency()` (`src/PluginProcessor.cpp:2018`, the last statement of
`setStateInformation`) — belt-and-braces
for the rest of the restore body, a no-op because `setLatencySamples` skips an unchanged
figure, and documented at the site as exactly that: the host still sees at most one PDC
change per load.

## The offline rule

`effectiveFactor()` (`src/dsp/Latency.h:96-99`): with **Offline quality = Force Max** and
`isNonRealtime()`, the engine renders — and the wrapper reports — the **forced 16×**
factor regardless of the live setting. This is why `setNonRealtime` is a recompute
trigger. The transition edges are asymmetric by design and both pinned: a
realtime→offline flip does **not** duck the render (`testOfflineFlipDoesNotDuckTheRender`
— the bounce's first samples are not a fade), while the return edge **is** ducked
(`testReturnFromOfflineIsDucked`).

## Verification map

| Property | Test (`tests/dsp_tests.cpp`) |
|---|---|
| Impulse lands at exactly the reported figure, across lookahead values — both true-peak modes | `testReportedLatencyMatchesImpulse` |
| The TP-mode composition: window capped at `allowance − D`, group delay unchanged | `testTruePeakModeCapsTheWindowNotTheLatency` |
| A true-peak toggle is latched at the duck's bottom, never stepped | `testDuckWrapsTruePeakLatch` |
| Engaging TP mid-stream holds the ceiling from the toggle on (the latch at the toggle block) | `testTruePeakEngagementHoldsTheCeiling` |
| Measured impulse vs reported figure across the full `(factor × phase)` matrix, both true-peak modes — **exact** for linear phase (a symmetric FIR's peak *is* its group delay) and at OS Off; **within ±1 sample** for min-phase (an IIR cascade's group delay is frequency-dependent by design; the test's comment records the measured split) | `testOsLatencyMatrix` |
| Offline force / duck edges | `testOfflineFlipDoesNotDuckTheRender`, `testReturnFromOfflineIsDucked` |
| Bypass alignment at every factor (the dry ring uses the same tables) | the OS bypass-null checks |

## Changing anything here

| Change | Cost |
|---|---|
| Reported figure (either term's meaning) | **Hard Stop** + ADR (this is a host-facing contract; sessions remember PDC behaviour) |
| The os tables after a JUCE bump | Expected: re-measure under `DEPENDENCY_POLICY.md` rule 2; the matrix test is the gate |
| A new latency input | ADR + a new trigger row here + the batch reviewed |
