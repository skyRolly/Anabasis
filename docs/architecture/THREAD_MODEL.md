# THREAD_MODEL.md

The **implemented** thread model, written from **ADR-0011** (Accepted 2026-07-31) with code
citations, as `THREADING_POLICY.md` requires of P1. This file *describes* the code; the binding
rules stay in the policy and the ADR. When this file and the code disagree, the code is drifting or
this file is stale — report it (`SOURCE_OF_TRUTH.md`), do not silently pick one.

**Status: P4 (P1–P4 landed).** The inventory below covers every cross-thread edge that exists in
the tree today, including the P2–P4 additions (the SPSC GR ring, the duck/Learn/restore command
atomics, the meter publish row). Still-planned rows (the P5 spectrum rings, the OQ-013 trim
transport) are listed under **Planned edges** so their absence is visible rather than implied.

## Thread inventory

| Thread | Exists at P1 | Owns |
|---|---|---|
| **Audio** | yes | `AnabasisEngine` state (rings, wedge, envelope, smoothers, crossfade), built-once-per-block `EngineParameters` snapshot |
| **Message/GUI** | yes | Editor, `MacroEngine`, `InternalState` tree, A/B slots + preset/undo bulk swaps, PDC recompute (with the two host-callback exceptions below) |
| **GPU render context** | yes, macOS/Windows only | Component painting when attached — created and driven by JUCE, holds no Anabasis state (`src/gui/PluginEditor.h:616`, attach gated per platform per DESIGN §6.1) |
| Workers | **none** | Adding one is an Architecture Review Gate item + Hard Stop (ADR-0011) |

## Cross-thread edges implemented at P1

Every edge, its mechanism, its direction, and which permitted-path table row (THREADING_POLICY.md)
it instantiates:

| Edge | Mechanism | Table row | Code |
|---|---|---|---|
| Parameters → engine | **45** cached `getRawParameterValue` atomics (`kCachedParamCount` — 44 at P1, +`freeze` at P4) read once per block into the POD snapshot (`CachedParams::toEngine`), never piecemeal | GUI → Audio (automatable params) | `src/PluginParameters.cpp` (`resolve`/`toEngine`), `src/PluginProcessor.cpp` (`processBlock` snapshot build) |
| Host-hidden config → engine | `InternalState` relaxed atomic mirrors (`osMirror`/`phaseMirror`/`offlineMirror`/`lockMirror`), synced on every tree write, read per block | GUI → Audio (host-hidden session state) | `src/InternalState.h:153`, `syncAtomics()` |
| `nonRealtime` → latency predictor | `std::atomic<bool> nonRealtimeFlag`, written in `setNonRealtime()` (a host-thread callback), read by the snapshot build and the predictor | atomic mirror (same class as the row above) | `src/PluginProcessor.h` (`nonRealtimeFlag`), `src/PluginProcessor.cpp` (`setNonRealtime`/`updateLatency`/`processBlock`) |
| Engaged lookahead window → any reader | `std::atomic<int> engagedWindow`, relaxed, written per sample on the audio thread; payload-free diagnostic (the smoothing test reads it) | Audio → GUI (staleness-hint class: monotonic display/diagnostic data) | `src/dsp/AnabasisEngine.h` (`engagedWindowSamples`; line-number cites here went stale twice — symbols only) |
| Forced-duck request → engine | `std::atomic<bool> duckRequested`, set by the wrapper before every bulk swap (A/B, preset, session load), `exchange`-consumed at the block top | GUI → Audio (momentary / transient requests) | `src/dsp/AnabasisEngine.h` (`requestForcedDuck`), `src/PluginProcessor.cpp` (three call sites) |
| Meters → GUI | `pubLufsM/S/I`, `pubDbTpMax`, `pubPlr` (the canonical GATED PLR — still published and still cleared with the set, though since ADR-0020's second amendment the panel's PLR row derives its own from whichever integrated figure it is showing), `pubGrDb`, and since ADR-0020 `pubPeakMaxDb`, `pubRmsDb`, `pubLufsIUngated`, `pubLra` — relaxed atomics, ONE publish per block from `processBlock`, **fed from the engine's §2.9 render tap** (the programme path before the monitor-only delta/comp stages — the buffer itself carries the listening path); plus the engine's `grMinLinear`/`engagedWindow` diagnostic atomics, its `compGrDb` per-stage figure, and since 0.1.2 (ADR-0023 item 10) the per-channel per-stage pair `compGrDbCh[2]`/`limGrDbCh[2]` — the panel meters' L/R lanes and the KI-009 field disambiguator; same relaxed one-store-per-block contract, read through `meterCompGrDbCh`/`meterLimGrDbCh`. The audio thread is the steady-state writer but NOT the only one: `publishSilentMeters()` writes the same set from `prepareToPlay` (host thread) and from `requestMeterReset()` — so from the message thread on the meter panel's click and from whichever thread the host restores on via `setStateInformation` — because a clear that waits for a block top is invisible until audio flows, and both a project open and a meter read ordinarily happen stopped. INDEPENDENT relaxed scalars (ten of them; the LIST in `publishSilentMeters` is the count, deliberately not a number repeated in prose — it was wrong within two commits the first time it was written), no ordering role, so a concurrent load-clear and end-of-block publish are last-writer-wins per scalar and the worst observable outcome is one display frame mixing pre- and post-clear values, overwritten by the next block | Audio → GUI (meters) — plus the non-audio clear writers named here | `src/PluginProcessor.cpp` (the per-block publish), `src/dsp/AnabasisEngine.h` (`outputLoudness`/`outputRms`/`lastRenderTpMax`/`lastRenderPeak`) |
| GR/waveform history → GUI | `GrHistoryBuffer`: 4096-entry power-of-two SPSC ring, entry written FIRST, monotonic index release-stored AFTER, acquire-loaded stateless peeks on the reader side; since the 0.2.8 final review the ring also carries the prepared (rate, block) pair its entries are recorded under — stored by `clear` inside the reset epoch's odd window, read relaxed under the same epoch bracket (`prepared()`) — and a reader closes its batch with `batchIntact` (acquire fence, then the epoch re-read) | Audio → GUI (time series, SPSC ring) | `src/dsp/GrHistoryBuffer.h` |
| Learn start/stop → engine | ONE atomic word `learnCmd` (none / start / commit / commitThenStart), **release**-stored by the writer and `exchange`-consumed with **acquire** at the block top. The writer COMPOSES on its own thread — a start landing on an unconsumed commit becomes one commitThenStart — because ordering information exists only there; two flags with a fixed consumption order silently discarded both commands when a stop and a start fell in the same block. It was a code+flag staged record until 2026-08-02: publishing a two-store record let a consumer whose `exchange` fell between the stores run the new code and leave the writer to re-raise the flag behind it, delivering the same command twice — a repeated commitThenStart commits a pass one block old. A two-bit payload has nothing to stage, so the code IS the flag | GUI → Audio (**single lock-free scalar**; ADR-0012 §Known limits still governs the composing writer) | `src/dsp/AnabasisEngine.h` (`requestLearnStart/Stop`), `src/dsp/AnabasisEngine.cpp` (block top) |
| Learned-target restore → engine | ONE staged record: `pendingLearned` (the learned/never-learned discriminator) + `pendingRefOnset`/`pendingRefTilt` stored relaxed FIRST, then the single `adaptivePending` flag **release**-stored; the block top `exchange`s it with **acquire**, so a block that sees the flag reads that call's whole record, never a torn one, and the LAST restore staged before the block is the one that lands. Two flags with a fixed consumption order could not express last-writer-wins — an un-learned session loaded after a learned one inherited the learned references | GUI → Audio (**bounded staged record**, ADR-0012) | `src/dsp/AnabasisEngine.h` (`restoreLearnedTargets`), `src/dsp/AnabasisEngine.cpp` (block top) |
| Learned state → `getStateInformation` | `AdaptiveEngine::learned` atomic: refs published FIRST, flag **release**-stored; `hasLearned()` **acquire**-loads, so a saver that sees `true` reads the refs that store ordered before it | Audio → GUI (published state read as a unit — the ADR-0012 contract mirrored) | `src/dsp/AdaptiveEngine.h` (`commitLearn`/`hasLearned`), `src/PluginProcessor.cpp` (`getStateInformation`) |
| Macro listener → message-thread mapper | `std::atomic<bool> mappingPending` set from whichever thread APVTS delivers `parameterChanged` on; drained on the message thread by a 30 ms `Timer` (and directly when the callback already runs there); `std::atomic<int> restoreDepth` suppresses the WHOLE drain tick across a restore (`ScopedRestore`) — the guard covers the wrapper's half too, so the tick is not a second concurrent writer of `liveDetachMask` on a host that restores off the message thread; `std::atomic<bool> applying` is the §5.3 discriminator's macro-originated half and is atomic for the same reason `restoreDepth` is — `AnabasisAudioProcessor::parameterChanged` reads it from whichever thread the host chose. **Nothing posts to the message queue from the listener**: `triggerAsyncUpdate()` takes a lock and on some platforms allocates, so a callback delivered on the audio thread would put both inside `processBlock`. `std::atomic<bool> drainStopped` is the one-way TEARDOWN latch: `stopDraining()` sets it and `drainTick` tests it first, so no trigger — timer, posted update, `flushPendingMapping`, `refreshMapping` — can reach `onDrainTick` after the owner has begun destroying the members it calls into. It replaced nulling the `std::function`s, which raced a tick already about to invoke one | Any thread → Message (listener → async drain guard; OQ-014 resolved 2026-08-02, reading 1) | `src/MacroEngine.cpp:28-35,63-66`, `src/MacroEngine.h:92-101,139` |
| §5.3 detach/re-engage bits → the mask | `pendingDetachBits` / `pendingReengage` set from the gesture and APVTS callbacks on whichever thread the host delivers them, drained into `liveDetachMask` on the message thread — directly when the callback is already there, otherwise on the MacroEngine's SAME 30 ms tick via `onDrainTick`. The wrapper used to `triggerAsyncUpdate()` on the off-thread branch, which is the hard red line the row above refuses for exactly the same reason; the `AsyncUpdater` base is gone so the route cannot be re-opened. Cost of the wait: the badge and the serialized mask lag ≤ 30 ms on such a host. The drain is ONE sequence — `MacroEngine::drainTick()`: the wrapper's bits first (they decide the mask), then the mapping (it reads the mask) — and all three triggers call it (the 30 ms timer, the posted `handleAsyncUpdate`, `flushPendingMapping`), because a revision that fixed the order in the timer alone left the posted path mapping against a stale mask. A macro-knob gesture BEGIN raises `pendingReengage` and calls `MacroEngine::armMapping()` (a relaxed store, so it is safe from whichever thread the gesture arrives on): clearing the mask and re-landing the curve are two halves of §5.3 point 3, and a gesture that moves nothing armed neither | Any thread → Message (same row) | `src/PluginProcessor.cpp` (`drainDetachBitsSoon`, `handleAsyncUpdate`), `src/MacroEngine.cpp` (`drainTick`) |
| Frozen-trim restore → engine | `AnabasisEngine::restoreFrozenTrims`: four scalars stored relaxed FIRST, one `frozenPending` flag **release**-stored after; the block top `exchange`s it with **acquire** into a pending copy, which is APPLIED (via `AdaptiveEngine::injectTrims`, finite-checked and clamped at the boundary — round 52: the clamp alone was `juce::jlimit`, whose comparisons are both false for a NaN, so a non-finite property passed through it untouched) at the §2.8 duck's silent bottom or the unprimed direct-adopt — where every restore-driven discontinuity lands. Staged only by the wrapper, only for a freeze-ON adopted surface, and only from a path that also requests the duck (the bottom is the only landing site — `undo`/`redo` were the omission, fixed 2026-08-03); last-writer-wins | GUI → Audio (**bounded staged record**, ADR-0012 — second instance, ADR-0014) | `src/dsp/AnabasisEngine.h` (`restoreFrozenTrims`), `src/dsp/AnabasisEngine.cpp` (block-top consume + the two application sites), `src/PluginProcessor.cpp` (`saveSlotFromLive` capture, `applySlotToLive` + `setStateInformation` stages) |
| Frozen-trim **retention** → the saver | `AdaptiveEngine`'s RETAINED set: four scalars stored relaxed, `retTrimSeq` **release**-stored after them, **acquire**-loaded by `retainedTrimGeneration()`. A COUNTER rather than a flag since round 42, because the wrapper asks it two questions: `!= 0` is "does a retained vector exist?", and `!= slotFrozenBase` is "was it latched since the live surface's frozen ownership last changed?" — the slot scope the engine cannot carry itself, since it latches vectors and knows nothing about A/B. `slotFrozenBase` is recorded (relaxed; a comparand, no payload) by `adoptFrozenMirror`, the single writer of the per-slot mirror. Written only by a MEANINGFUL publication (an audible `finishBlock`, an ADR-0014 `injectTrims`) and never cleared. **The restore MUST be one of the two**, and it is load-bearing rather than incidental: with Freeze ON `finishBlock` publishes nothing, so after `adoptFrozenMirror` re-bases `slotFrozenBase` the injection is the ONLY event that can carry the generation past that base and let the incoming slot answer for its own latch. A future change that made `injectTrims` publish without counting — to keep the counter meaning "measured", say — would leave a freeze-ON slot restored from disk withholding its latch from every save until the next audible block, i.e. for ever on a stopped transport. Whatever the counter comes to mean, it has to include an ADR-0014 restore, because the wrapper reads it to decide which SLOT owns the latch, not where the numbers came from. `testFrozenTrimRestore` case (1) asserts the bump directly — `reset()` leaves it standing, which is what makes it survive a host rate change, while the PUBLISHED set beside it is zeroed with the internal struct because that one describes what the DSP is applying. This is the durable copy of a frozen latch, and it lives here rather than in the wrapper for a threading reason: round 40 kept it in the wrapper's `liveFrozenTrims` `juce::ValueTree` and had `prepareToPlay` — a host callback JUCE does not deliver on the message thread — assign it, opposite the editor's then-continuous `presetDirty()` read of the same member. ThreadSanitizer reports that as a data race on the tree's refcounted pointer. The mirror's REMAINING writer, `adoptFrozenMirror()`, is still reached from `setStateInformation` — a single writer, not a message-thread-only one — so the load-path exposure is the one KI-003 owns, reduced to its pre-round-40 shape rather than removed. Round 51 narrowed the opposing side without touching the thread model: the dirty marker moved off `saveSlotFromLive()` onto `presetShapeFromLive()` (fixed parameter list + atomics, no ValueTree), so the mirror's readers are now the host-initiated ones only — `getStateInformation`, the A/B swap, the §7 undo push — instead of those plus a display timer | Audio → GUI (**publication flag**, THREADING_POLICY release/acquire row) | `src/dsp/AdaptiveEngine.h` (`publishTrims`, `hasRetainedTrims`), `src/PluginProcessor.cpp` (`engineFrozenTrimsIfLive`) |
| §7 undo history → the editor | `historyEpoch`, a relaxed counter bumped by `setStateInformation` (any thread); the message thread reconciles it in `syncHistory()`, which clears the four stacks and the gesture snapshot. The containers themselves — `juce::Array<juce::ValueTree>` and a `ValueTree` — are touched by NO other thread, which is the whole mechanism: the loader announces the session change instead of performing the clear, so there is nothing to race and nothing blocks in the host callback. Every read and write of the history passes through the one reconciliation point | GUI ← host (momentary / transient requests, inverted: the host announces, the GUI thread acts) | `src/PluginProcessor.h` (`historyEpoch`, `syncHistory`), `src/PluginProcessor.cpp` (`setStateInformation`, `undo`/`redo`/`pushUndoStep`, both gesture callbacks) |
| Frozen-trim **application** generation → the saver | `frozenStageSeq` (`fetch_add` by the stager, before the record flag) vs `frozenAppliedSeq` (**release**-stored by the audio thread only after `injectTrims`, **acquire**-loaded by the saver — not the relaxed staleness row, because this counter GATES a read of the four `publishedTrim*` atomics and relaxed would let "settled" be observed before the values it announces — the row THREADING_POLICY now states generally, and which `pubTrimEver`/`retTrimEver` were brought into line with at round 41); `frozenRestorePending()` is their inequality. The consumer samples the generation BEFORE the payload, so a stage landing mid-consume stamps the older number and the record stays pending rather than claiming a vector it did not inject. The record flag alone answers "consumed?", and the save needs "**applied?**" — the ~34 ms between the block top and the duck bottom is a window in which the published trims are still the pre-restore ones, and a save landing there serialised them (until round 51 the editor's ~3 Hz dirty-marker poll reached this path in ordinary use as well, which is how the window was found; the marker no longer enters `saveSlotFromLive` at all, so the reachable landings are `getStateInformation`, the A/B swap and the §7 undo push — rarer, and the gate is required for exactly the same reason) | Audio → GUI (generation / staleness counters) | `src/dsp/AnabasisEngine.h` (`frozenRestorePending`), `src/PluginProcessor.cpp` (`saveSlotFromLive`) |

**How the staged-record row came to exist (ADR-0012).** The two learned-target rows above were
first written here under invented row names ("momentary request + flag-orders-payload"), which read
as if the permitted-path table authorised them. It did not: the restore stages two floats plus a
discriminator behind a separate release-stored flag, and the sentinel row excluded verbatim
anything "wider than one lock-free scalar". The mechanism copied the GR ring's release/acquire
discipline, and Audio→GUI authorisation does not carry over to GUI→Audio. External review caught
it (2026-08-01), it was recorded as OQ-015 rather than redesigned under review pressure, and the
owner ratified the implementation unchanged: **ADR-0012** adds the staged-record row with its six
mandatory conditions, and the rows above now cite it. The implementation did not change; the
authorisation did. **OQ-013 is now closed by ADR-0014 (2026-08-02)** — ADR-0012 gave its trim
vector a permitted transport, and ADR-0014 took the injection decision it deliberately left open;
the frozen-trim row above is the wired result.

**OQ-014, resolved 2026-08-02 (reading 1).** `mappingPending` and `restoreDepth` point
any-thread → message-thread, a direction the table did not enumerate. They implement the shape
ADR-0005/ADR-0011 mandate ("the MacroEngine consumes macro changes solely through an async
message-thread listener" — `juce::AsyncUpdater` is itself an atomic flag plus a message post), so
the owner call took the documentation-gap reading: `THREADING_POLICY.md` now carries a
listener-guard row citing those two ADRs as the enacting authority, and no new ADR was owed. The
residual check-then-act window in the guard remains recorded in `KNOWN_ISSUES.md` KI-003.

## PDC

One `setLatencySamples` call site — `updateLatency()` (`src/PluginProcessor.h`,
`src/PluginProcessor.cpp`) — fed by the `const` race-free predictor in `src/dsp/Latency.h`,
which both the wrapper and the engine call so reported and actual cannot drift. Triggers, per
ADR-0004 item 5: `prepareToPlay`, the three `int_` latency-input callbacks (coalesced to one fire
per bulk read by `InternalState::ScopedLatencyBatch`), and `setNonRealtime()`. The last two are
host callbacks **not** delivered on the message thread — which is why the policy rule reads "off
the audio thread", not "on the message thread" (amended by ADR-0011, second correction). Nothing
recomputes PDC from `processBlock`.

## FTZ/DAZ

`juce::ScopedNoDenormals` at the top of `processBlock` (`src/PluginProcessor.cpp`) is the
single flush-to-zero mechanism; no module carries its own.

## Which context paints

The OpenGL context attaches on macOS/Windows only, never Linux/X11 (`src/gui/PluginEditor.h:616`
and the platform gate around its attach). When attached, JUCE paints components on the GL render
thread; when not, on the message thread. The rule that keeps both safe is the one the policy
already mandates: GUI-side reads of published state are stateless `const` peeks (at P1 the only
such read is `getLatencySamples()` in `paint()`), so the identity of the painting thread carries
no correctness weight. Recorded here per ADR-0011 §Consequences; no policy amendment implied.

**0.1.4 added the first GUI-side cross-thread read that is NOT a stateless `const` peek, and this
row exists because the paragraph above is what made it a defect rather than a choice.**
`AnabasisLookAndFeel::drawResizableFrame` asks the editor whether a parented pop-up is open, through
`isPopupMenuOnScreen`. That override is reached from `PopupMenu::MenuWindow::paintOverChildren` —
i.e. from PAINT — so on macOS/Windows it runs on the GL render thread, while every write to the
counter it reads is on the message thread. As a plain `int` that was a data race and a
sanitizer-reportable one on exactly the two platforms where the context attaches. `presetMenusOpen`
is therefore `std::atomic<int>`, read `memory_order_relaxed`: the value guards nothing but itself,
and a frame that reads a one-tick-stale count draws the border it would have drawn a frame earlier.

**0.2.8 added the second such read, in `GrHistoryView`, and it arrived the same way — as plain
scalars, found by review rather than in the field** ([ADR-0038](design-decisions/ADR-0038-gr-history-display-scalars-cross-the-painting-boundary.md),
**Accepted 2026-09-02; the gate is cleared**). The continuous scroll needs a sub-entry phase, so the
frame-clock tick publishes `shownHead` (the ring index it last observed, and the head the frame
draws) and `smoothHead` (that head advanced at the nominal entry rate) on the message thread, and
`paintHistory` reads both. Both are `std::atomic`, relaxed, `static_assert`ed lock-free, and a third — `publishedEpoch` — says
which ring timeline they belong to, stored `release` last and loaded `acquire` first (ADR-0038
clause 7). That epoch is the boundary's only ordered access and it exists because a phase means
nothing outside the timeline it was measured in: `GrHistoryBuffer::reset()` rewinds the write index,
so a paint landing between a clear and the next tick would otherwise apply the pre-reset offset to
the first frame of the new history. What is new
against ADR-0027 is that there are TWO estimates and the painting thread may pair either one's newer
value with the other's older one: that is safe because every such pairing resolves to
`min (smoothHead, head + 1)` — a position between two frames the ramp itself produces — so the
paint needs no consistency, only the two values (`GrHistoryView::frameFor`). The RING's own
reader contract is unchanged and is a separate thing entirely: the display scalars carry no ring
data, and the ring is still read by stateless `peek`s under its epoch guard — now bounded below by
`readFloor` as well, so a batch cannot reach the slot the audio thread is filling. The TIME BASE
the frame maps through — the prepared (rate, block) pair — is the ring's too since the final
review: `GrHistoryBuffer::prepared()`, published inside the clear and read under the batch's epoch,
replacing `AudioProcessor::getSampleRate()`/`getBlockSize()`, whose plain members the host's
reconfiguring thread writes (`setRateAndBufferSizeDetails`, before `prepareToPlay`) while the tick
and the paint read them. The batch's close is `batchIntact`, an acquire FENCE and a re-read, for
the reason ADR-0011's second 2026-09-02 amendment gives.

**The ring's own payload became atomic in the same round, and for a different reason** (ADR-0011,
amended 2026-09-02). The guards above — the epoch, and `readFloor` — are built to notice that a
batch was overtaken and throw the frame away, and both read synchronised state (`resetEpoch()` and
`available()` are acquire loads). What they were validating was not: `push` stored the pair with a
plain write and `peek` read it with a plain read, so an overtaken batch was a data race, undefined
the moment it happened, and discarding the frame afterwards could not undo it. The fields are
`std::atomic<float>` now, relaxed both ways — the racing read is DEFINED, the guards' job is
unchanged, and the audio-thread store compiles to the same instructions it did.

The same wiring has a LIFETIME half, fixed in the same place: the editor's destructor used to clear
`lnf.isPopupMenuOnScreen` (a `std::function`) before `glContext.detach()`, mutating a callable a live
render thread could still invoke. `detach()` joins that thread, so it now runs FIRST and both hook
assignments are unobserved. The rule to carry forward is the general one: a hook the paint path calls
must be torn down after the painting thread is gone, not before.

## Audio-thread-only state behind a `const` accessor

`LoudnessMeter::integratedLufs()` and `LoudnessMeter::lraLu()` are `const` and, since ADR-0020's
third amendment, **write** `mutable` non-atomic members: each holds its histogram walk between
gating blocks (`integratedCache`/`lraCache` and their validity flags, cleared by `finishSubBlock`
and `clearSessionCumulative`). That is not a cross-thread edge — it is state with exactly **one**
thread, which is why it is recorded here rather than in the table above.

> **The invariant: every caller of `LoudnessMeter::integratedLufs()` and `LoudnessMeter::lraLu()`
> must be on the audio thread.** They are not thread-safe, and they are deliberately not atomic.
> `THREADING_POLICY.md` §"Forbidden cross-thread access" already forbids reaching non-atomic
> shared state from a second thread; this is a case where obeying that rule is what makes the
> cache correct, rather than merely tidy.

It holds by construction today. The only production callers are the meter publish at the end of
`processBlock`, and every writer of the accumulators they cache is on that same thread — the
gating-block commit inside `finishSubBlock`, and the meter-reset consume (`resetMeterHolds` →
`resetIntegrated`) at the block top.

**What makes it worth recording is that the compiler will not defend it.**
`AnabasisEngine::outputLoudness()` returns a public `const LoudnessMeter&`, and a `const` method
that mutates advertises nothing: a GUI-side reader added through that reference would compile
without complaint and race. `const` is exactly the signal a reviewer would otherwise rely on,
which is why the constraint is written down rather than left to be re-derived.

**Nothing on `LoudnessMeter` is safe to read from a second thread**, and the cache did not create
that — `momentaryLufs()`/`shortTermLufs()` walk `subRing` while the audio thread writes it, and
`integratedUngatedLufs()` reads two accumulators the same thread updates. What the cache changed
is the sharpness: those three only READ audio-thread state, while the two cached getters also
WRITE, so a second reader is a write-write race rather than a stale-value one. The invariant above
is stated for the two because they are where a reviewer's `const`-based intuition now fails; the
class-wide rule is the older and broader one.

The accessor is deliberately **not** narrowed. Its other consumer is the engine's own §2.7
compensation, reading `dryMeter`/`wetMeter` from inside `AnabasisEngine::process` — the same
thread — so there is no GUI-side use to remove, and reshaping a public accessor to encode a
constraint no caller currently violates is an API change this record does not require.

**What a GUI-side reader should do instead:** read the published atomics — `meterLufsI()` and
`meterLra()`, the "Meters → GUI" row above. They carry exactly these two figures, refreshed once
per block, and they exist for this purpose. Reaching past them into the meter is the mistake this
section is here to prevent.

**Adding a second reader thread to these two getters is a threading-model change**, and therefore
an Architecture Review Gate item (`CLAUDE.md`'s Hard Stop list): it needs atomics on the cache or
a different mechanism, decided in an ADR and not at the call site.

Recorded per ADR-0020's third amendment; **no policy amendment implied** — the rule that keeps
this safe is one `THREADING_POLICY.md` already states, in the same way "Which context paints"
above records a nuance without amending the ring rule.

## Planned edges (not yet in the tree)

- **Spectrum capture rings — IMPLEMENTED (P5, 2026-08-02)**: two `anabasis::ScopeBuffer`
  instances in the engine (post-input-gain and post-chain/render taps), each filled into
  preallocated scratch during stages A/E and published with ONE release-store per processed
  chunk — the SPSC ring row, same discipline as `GrHistoryBuffer`. The FFT runs GUI-side
  (`SpectrumView`), reading stateless `readLatest` peeks; nothing on the audio thread windows,
  transforms or allocates. Guarded by `testSpectrumRingsCarryTheTaps` (count-per-chunk and
  tap-content equality).
  **`prepare` rewinds both rings, and the rewind is ANNOUNCED on a generation counter**
  (`ScopeBuffer::resetGeneration()`, bumped release-after the index store; the generation-counter
  row). The rewind makes pre-rate-change frames unreachable; the reader owns the smoothed EMA the
  ring cannot reach, so it has to drop that itself, and until round 54 it inferred the reset from
  `writeCount()` going backwards. That predicate holds only while the observed count is still
  below the reader's last one — a single delayed tick lets the producer republish past it, after
  which the reset is missed permanently and the pre-reset analysis is drawn against the new rate's
  bin mapping. `SpectrumView` samples the generation on **both sides** of its analysis batch, the
  same reader contract `GrHistoryBuffer::resetEpoch()` states; it is a plain generation rather than
  that class's odd/even seqlock because `ScopeBuffer::reset()` writes one atomic and touches no
  sample, so there is nothing for a reader to observe half-done.
  `testARewoundSpectrumRingDropsThePreviousTrace` covers both edges, including the
  count-never-dips case the old predicate could not see.
- **Command atomics — the meter-hold reset is now IMPLEMENTED (P5, 2026-08-02)**, joining the
  forced-duck request and the Learn command on the momentary-request row:
  `AnabasisAudioProcessor::requestMeterReset()` → `meterResetPending`, consumed with `exchange`
  at the top of `processBlock`, clearing the session-cumulative display state only — the
  integrated-LUFS histogram (via `LoudnessMeter::resetIntegrated`, which also WATERMARKS the
  gating-block assembly so a block straddling pre-reset material cannot enter the fresh
  histogram and pin the relative gate at the old programme's loudness) and the wrapper's
  `dbTpMaxHold`; PLR follows by derivation, the rolling M/S windows keep running, and the §2.7
  compensation meters are untouched (they are a monitor function, not a display). The two P5
  decisions this edge was holding are TAKEN: **a state load clears the holds**
  (`setStateInformation` stages the same request, so the clear lands at a block top), and
  **the published atomics are cleared by the request itself**, not by its callers: the
  ENGINE half genuinely has to wait for a block top, the DISPLAY half does not, and a request
  that only set the flag was invisible until audio flowed — indefinitely so with the transport
  stopped, which is the ordinary condition for BOTH callers (a project is opened stopped; a
  meter is read stopped). `setStateInformation` carried the display publish beside its request
  from round 33 and the meter panel's click did not, so the same reset worked from a load and
  read as a dead button from the GUI. `requestMeterReset()` now performs both, so a third
  caller inherits the pairing (round 40), and
  **`AudioProcessor::reset()` stays un-overridden** — a transport stop must not clear a
  mastering measurement, an in-flight Learn pass must survive a stop/start (MODE inv 3's
  explicit start/end), and the ≤ 10 ms tail a reset would flush is inaudible; hosts that need a
  flush re-prepare, which reaches everything. Guarded by `testMeterResetClearsSessionHolds`
  (all four halves mutation-verified).
- **GrHistoryBuffer reset epoch — the reader contract, decided here as promised (P5,
  2026-08-02)**: the write index is monotonic BETWEEN resets and MAY REWIND across one;
  `reset()` brackets its host-thread bulk clear with two `release` increments of a
  `resetGuard` epoch (odd = clear in flight), and a reader samples `resetEpoch()` before a batch
  of `peek`s and closes it with `batchIntact` — an acquire fence, then the re-read; an acquire LOAD
  alone does not order the batch's relaxed loads before it on weakly ordered targets — where odd or
  changed means the batch raced a reset and is discarded,
  and the reader re-anchors its cursor to the fresh `available()`, dropping at worst one
  display frame on an event (re-prepare) that already blanks the programme. Readers never
  cache `available()` across an epoch change; within one epoch the SPSC row's contract is
  unchanged. The prepared (rate, block) pair is stored inside the same window since the 0.2.8
  final review (`prepare`/`prepared`). Guarded by `testGrRingResetEpoch` and `grPrepared`.
- **Frozen trim vector transport — IMPLEMENTED (2026-08-02, ADR-0014)**: the Hard Stop this
  bullet carried is lifted. ADR-0012 settled the transport (the staged-record row fits a
  four-scalar vector); ADR-0014 took the injection decision and the edge is now the frozen-trim
  row in the implemented table above, guarded by `testFrozenTrimRestore` (every element killed by its own mutant, each
  killed by a distinct check).

## Verification

`AnabasisStateTests`: `testMacroRestoreDoesNotClobber` and `testDrainInsideRestoreIsSuppressed`
(both mutation-verified) pin the macro edge's restore semantics;
`testLatencyNotifyIsBatchedAcrossARead` pins one PDC fire per bulk read. What cannot be verified
headlessly — an actual off-message-thread host restore, real GL-thread painting — is listed in
KI-003 and `TESTING.md` §What cannot be verified headlessly. `REALTIME_SAFETY_AUDIT.md` (end of
P2) audits allocation/lock freedom on the audio thread; not claimed in advance (C7).
