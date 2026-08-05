# OPEN_QUESTIONS.md

Decisions that this repository is **not** allowed to guess at
(`docs/DEVELOPMENT_BRIEF.md` §13). Each entry states the question, why it cannot be answered from
the repository, the options with their consequences, and a recommendation where one exists.

An answered question moves to `Resolved` with the decision, the date, and — if it is an
architecture decision — the ADR that records it. Questions are never deleted.

Status values: `Open` · `Blocking <phase>` · `Resolved`.

---

## OQ-002 — Which JUCE licence tier does Anabasis ship under? · `Blocking commercial release`

**Question.** §2 says to confirm which tier Anamorph ships under and keep this project on the
same tier.

**Why it cannot be guessed.** The Anamorph repository records the tier as an **open owner/legal
decision**, not as a settled fact: it states that the closed-source commercial model rules out
the AGPLv3 arm and that the commercial tier "must be in place before commercial distribution."
There is therefore no answer to copy — this is an owner action, not an engineering one.

**Consequence if unresolved.** Does not block development or internal testing. It blocks
commercial distribution absolutely.

**Related.** Commercial VST3 distribution additionally requires reviewing Steinberg's licensing
terms separately.

---

## OQ-008 — Loudness-penalty reference values · `Open`

**Question.** §6 requires streaming-target lines (Spotify −14, Apple Music −16, YouTube −14,
club/CD) **plus a loudness-penalty estimate in dB per platform**.

**Why it cannot be guessed.** Platform normalisation targets change, and several platforms do not
publish an authoritative figure. Shipping stale numbers as if they were facts is a **C2**
violation and would mislead a mastering decision.

**Action.** Decide the source of these values and how they are dated/updated (hard-coded with a
documented "as of" date, or user-editable), and record the decision plus each value's source in
`DESIGN.md`. Any number that reaches the UI needs a citable origin.

**Mechanism ratified at the 2026-07-31 sign-off** (`DESIGN.md` §2.9): one compiled table,
per-value source citation, "as of" date surfaced in the tooltip, not user-editable in v1,
refreshed each release.

**Values gathered at P5 (2026-08-02), as the mechanism required.** The compiled table
(`src/gui/LoudnessMeterView.h`) carries: **Spotify −14 LUFS** (Normal mode; Loud −11 / Quiet −19),
**Apple Music −16 LUFS** (Sound Check), **YouTube −14 LUFS** — cross-confirmed against
soundplate.com's streaming-loudness table and trackgleam.com's target guide, both current as of
2026-07/08, and surfaced in the panel tooltip with the "as of" date. **What keeps this entry
`Open`:** (1) release-time verification against the FIRST-party pages (Spotify for Artists /
Apple Sound Check / YouTube help) is the per-release refresh the mechanism prescribes — secondary
cross-confirmation was accepted at P5 to avoid shipping an empty panel, and the tooltip's date
makes staleness visible; (2) **club/CD has no published normalization target** — that line from
the brief's list needs an owner-specified reference level or a decision to drop it, and it is
deliberately absent from the table rather than invented.

---

## OQ-009 — Ownership and support contact · `Open`

**Question.** `docs/HANDOVER.md` requires owner/team metadata and a support contact. No such
metadata exists in this repository.

**Action.** Owner supplies. Until then the field stays `TODO` (constraint C7) — it is not
inferred from Anamorph.

---

## OQ-012 — Should macOS/Windows validate the stripped, signed bytes? · `Open (decide at P6)`

**Question.** Only the **Linux** job validates what users actually receive: its strip step runs
before pluginval. On macOS `strip -x` + ad-hoc codesign run in the packaging step *after*
pluginval, and on Windows the public copy is produced after validation too. So a defect introduced
**by stripping or signing** would ship unvalidated on those two platforms.

**Why it is not simply "reorder them".** macOS must codesign **last** — stripping after signing
invalidates the seal — so the pluginval step would have to move after the whole
strip→sign→verify sequence, which changes what a pluginval failure means for artifact gating. On
Windows the same reordering is cheaper but still shifts which step the customer upload depends on.

**Why not decided now.** There is no binary. The honest way to settle this is to measure whether
strip/sign actually perturbs anything on a real build — which is a P6 activity, alongside the
release pipeline.

**Interim.** The asymmetry is stated in the `build.yml` header and in
`docs/procedures/CI_CD.md` rather than glossed over by the "uniform and blocking" wording, which
refers to the pluginval **gate**, not to which bytes it sees.

---

## Resolved

### OQ-006 — Where does the C++23 canary run, and what does it gate? · `Resolved 2026-08-05`

**Decision (2026-08-05, taken while executing the owner's post-v0.1.0 continue-autonomously
directive; ⊕ for review like every decision taken under a standing approval rather than an
item-specific sign-off).** **This entry's own recommendation, adopted unchanged** — which is why
it could be taken without the owner in the loop: the discretionary part (scope and cadence) was
already answered in writing here, and ADR-0008 had already mandated everything else (the job
exists, builds C++23 on all three platforms, never gates). The canary is
`.github/workflows/cxx23-canary.yml`: **DSP core + tests only** — it builds the `AnabasisTests`
target at C++23 (compiling `AnabasisDSP`'s sources plus the JUCE modules they pull in) and then
**runs** the suite, so the weekly signal is "compiles *and still behaves identically* at C++23",
not merely "compiles" — on a **weekly schedule plus `workflow_dispatch`**, on all three
platforms, native-arch on macOS (the 10.13/universal contract is a shipping claim about binaries
this job does not produce). It gates **nothing**: it sits in no `needs:` chain and must never be
made a required status check — ADR-0008: "a red canary is a to-do, not a gate". The C++23
request travels through the `ANABASIS_CXX_STANDARD` cache seam in `CMakeLists.txt` (legal values
20/23 only), which exists because a plain `-DCMAKE_CXX_STANDARD=23` is shadowed by the project's
unconditional `set()` and would be ignored *silently*. The "added at P2" half of the
recommendation was missed — P2 closed 2026-08-01 with the P1 phase summary still reading
"scheduled for P2", and no later phase summary re-raised it — so this resolution lands the job
three phases late rather than re-litigating where it should have landed. Verified on Linux
before landing: every TU of the canary's target compiles at `-std=c++23` with zero diagnostics
and the DSP suite passes in full; the default configuration is untouched (identical `-std=c++20`
command lines, zero recompilation after the CMake edit).
The original entry follows as the record.

**Question.** §2.1 requires a **non-blocking** CI canary job building at C++23 on all three
platforms, whose failure must never block the main pipeline, with its status reported in each
phase summary.

**Open detail.** Whether the canary builds the full target set or only the DSP core + tests, and
whether it runs per-push or on a schedule. Full-matrix per-push roughly doubles CI cost for an
early-warning signal.

**Recommendation.** DSP core + tests only, on a weekly schedule plus `workflow_dispatch`, added at
P2 when there is DSP code for it to compile. Confirm at P2.

---

### OQ-013 — How does the frozen trim vector cross message → audio? · `Resolved 2026-08-02 (ADR-0014)`

**Decision (2026-08-02, under the owner's v0.1.0 blanket approval; ⊕ for the post-v0.1.0 fine
review).** **Inject, freeze-gated, at the duck's silent bottom** — recorded by **ADR-0014**. The
transport is ADR-0012's bounded staged record (second instance:
`AnabasisEngine::restoreFrozenTrims` / `frozenPending`); the product question ADR-0012 left open
is answered *restrictively*: injection is staged **only for a freeze-ON adopted surface** (a
freeze-OFF slot's adapting vector is the truth — there is nothing to restore *to*), and the
pending copy is applied via `AdaptiveEngine::injectTrims` (clamped at the boundary) at the §2.8
duck's silent bottom or the unprimed direct-adopt, after which Freeze holds it exactly. The
capture side follows the ADAPTIVE child's mirror rule: a load→save with no audio between
serialises the loaded copy, not the engine's stale published trims. The Hard Stop banners in
`PluginProcessor.h`, `THREADING_POLICY.md` and `THREAD_MODEL.md` are lifted; guarded by
`AnabasisStateTests` `testFrozenTrimRestore` (every element killed by its own mutant; ADR-0014
enumerates them).
The original entry follows as the record of why this needed deciding.

**Question.** ADR-0007 routes `frozenTrims` through "the engine-side inject-at-the-duck-bottom path,
a sentinel-valued atomic consumed at the forced duck's silent bottom" (the `abMatchGain` pattern).
That phrase is **singular**, but the vector is **four** scalars — release, stereo-link,
sidechain-HPF and dynamic-tilt trims (`DESIGN.md` §5.4, ADR-0005 decision item 10). The precedent it
names carries one float (`Anamorph:src/PluginProcessor.cpp:485-491` [Verified]). So the mechanism is
under-specified exactly where it stops being a copy.

**Why it needs deciding rather than inferring.** The two readings differ in correctness, not style:

- **Four independent sentinel scalars.** Fits the permitted-path table's new single-scalar row four
  times over — but only if the four `exchange`s are guaranteed to be *observed together*. Nothing in
  the accepted set establishes that ordering. A half-consumed vector is not a transient artefact: the
  slot stays half-restored, so a frozen A/B slot renders differently from the slot that was saved,
  defeating the per-slot bit-repeatability `MODE_AND_ADAPTATION_POLICY.md` invariant 3 requires.
- **One release/acquire-gated per-slot POD.** Publishes the four values into a non-atomic per-slot
  struct behind a single ordering-carrying flag. Correct by construction, but it is a **new
  mechanism** — not in the permitted-path table, and not what ADR-0007 says.

Either way the answer changes the thread model, which is an **Architecture Review Gate** item, an
**AI-agent Hard Stop**, and requires an ADR (`ADR_POLICY.md` rule 5) that also amends
`THREADING_POLICY.md`'s table. `ADR-0011` §Consequences names this as the one cross-thread edge its
compliance claim excepts.

**Why not decided now.** Picking the mechanism here would be inventing architecture inside a
documentation pass — the class of change the gate exists to stop. The evidence needed is not textual:
it is what the duck's silent-bottom timing actually guarantees about publish-before-consume, which is
measurable only against real `processBlock` code (constraint C2/C7).

**Half-settled 2026-08-01 by ADR-0012** (taken for OQ-015, the smaller instance of the same
shape): the **mechanism** question is answered — a bounded staged record behind a release/acquire
flag is now a permitted GUI→Audio path, and a four-scalar trim vector fits it, so "one
release/acquire-gated per-slot POD" is no longer a new mechanism. What remains open is the part
ADR-0012 deliberately did **not** decide: whether a restored trim vector may be injected into a
running engine at all, and what that does to the adaptation state machine. **The Hard Stop
stands** — no code may wire the `frozenTrims` restore — but the blocker is now a product question,
not a missing transport.

**Action** *(historical — superseded by the Decision above)*. Settle it at P1 **before** any code wires the `frozenTrims` restore. Everything else in
the P1 skeleton — CMake, parameter surface, POD boundary, pass-through chain, latency — is
independent of it and is not blocked. Until then `THREADING_POLICY.md` carries the gap explicitly
under the command-path table, so the missing mechanism cannot be read as an oversight and
silently filled in.

---

### OQ-014 — Do the MacroEngine guard atomics need a THREADING_POLICY table row? · `Resolved 2026-08-02 (reading 1)`

**Decision (2026-08-02, under the owner's v0.1.0 blanket approval; ⊕ for the post-v0.1.0 fine
review).** **Reading 1 — already blessed; the table had a documentation gap.** The guards
implement the `juce::AsyncUpdater` shape ADR-0005/ADR-0011 mandate, so no new ADR was owed:
`THREADING_POLICY.md` gains an *any thread → message (listener → async drain guard)* row citing
those two ADRs as the enacting authority, and `THREAD_MODEL.md`'s macro-listener row now cites it
instead of this question. KI-003's residual check-then-act window stays recorded there —
resolving the row question does not shrink the window. The original entry follows as the record.

**Question.** The P1 macro layer carries two payload-free atomics that are not rows in
`THREADING_POLICY.md`'s permitted-path table: `mappingPending` (any thread → message thread — the
listener flags, the message-thread drain consumes; `src/MacroEngine.h`) and `restoreDepth`
(restoring thread → message thread — the `ScopedRestore` guard that keeps the drain from applying a
mapping mid-restore). The table's rows all point GUI→Audio or Audio→GUI; an any-thread→message
guard is a direction it does not enumerate, and the policy's own rule is "any path not in this
table is a new cross-thread path → Architecture Review Gate."

**The two readings.**

- **Already blessed.** ADR-0005/ADR-0011 mandate that the MacroEngine "consumes macro changes solely
  through an async message-thread listener". `juce::AsyncUpdater` — the accepted mechanism — is
  itself internally an atomic flag plus a message post; `mappingPending` + the 30 ms drain +
  `restoreDepth` implement that mandated shape (plus its §5.3 restore exception) rather than adding
  a new edge. Under this reading the table has a documentation gap, closed by a row added when
  `docs/architecture/THREAD_MODEL.md` (owed at P1) is written.
- **Needs the Gate.** The table is deliberately exhaustive, a policy change is enacted only by an
  ADR (`ADR_POLICY.md` rule 5), and "it is morally an AsyncUpdater" is an inference, not a
  decision. Under this reading the atomics stand as documented drift until a (small) ADR ratifies
  them and amends the table.

**Where the state is recorded meanwhile.** The mechanism, its mutation-verified tests, and its
residual check-then-act window are documented in `KNOWN_ISSUES.md` KI-003 and
`docs/DOCUMENTATION_COVERAGE.md` (fifth commit entry) — the deviation is visible, not hidden.
Not blocking P1 code (the code ships either way); blocking the **THREAD_MODEL.md** write-up, which
must state one reading or the other.

---

### OQ-016 — Does the §5.4 release trim apply while the limiter is in AUTO release? · `Resolved 2026-08-02 (ADR-0013)`

**Decision (2026-08-02, under the owner's v0.1.0 blanket approval; ⊕ for the post-v0.1.0 fine
review).** **Option 2 — the trim scales the auto poles**, as this entry recommended: recorded by
**ADR-0013**. `LookaheadLimiter::setAutoReleaseScale` scales `kAutoFastMs`/`kAutoSlowMs` by the
same `2^octaves` factor the manual path already applies (clamped to [0.5, 2.0], the trim's own
±1-octave bound), preserving the fast/slow ratio and therefore the two-stage character. All four
adaptive behaviours are now audible at factory defaults; `MODE_AND_ADAPTATION_POLICY.md`'s scope
note is deleted by that ADR. Guarded by `AnabasisTests` `testAutoReleaseFollowsTheTrimScale`
(mutation-verified against the fixed-constant alphas). The original entry follows as the record.

**Question.** The release trim is applied to the effective `limReleaseMs`
(`AnabasisEngine::process`), which `LookaheadLimiter` consumes **only in manual-release mode**.
`limAutoRelease` defaults to **on**, and the auto path steps its two envelopes with the fixed
`kAutoFastMs = 40` / `kAutoSlowMs = 600` constants. So in the factory state the release trim is
computed, published, displayed as an overlay, latched by Freeze and serialized — and changes
nothing about the sound. The other three trims reach their stages in the same state.

**Why it cannot be guessed.** Both readings are supported by the documents this repository is
governed by:

- **Working as specified.** `DESIGN.md` §5.4 defines trims as "bounded deltas applied inside the
  engine **around the current parameter values**", and `MODE_AND_ADAPTATION_POLICY.md` states that
  every trim "is inert while its host stage is inert" — the property the invariant-7 null rests
  on. Under auto release the manual release parameter *is* the inert stage, so an inert trim is
  the rule working, exactly as the scHpf trim is inert while the detector sits on its 20 Hz skip.
- **Not delivering the intent.** The same policy advertises four adaptive behaviours as live at
  P4, and the point of a release trim is to open up on sparse material and clamp down on dense
  material — which the auto path, being fixed, cannot do. Under this reading the trim should scale
  the two auto poles by the same 2^octaves factor (preserving their ratio, so the two-stage
  character is unchanged) and the current wiring is an incomplete implementation.

**Why it is not settled here.** Scaling the auto poles changes what the plugin sounds like **at
factory defaults** — the trim converges away from zero on ordinary programme material — which is a
product decision, not a bug fix. It is also the first case of a trim targeting a value that is not
a user-facing parameter, which is a precedent for the other three.

**Consequence if unresolved.** No code is blocked: the trim is bounded, inert and harmless in the
current wiring. What is blocked is any claim that all four adaptive behaviours are audible at
defaults — `MODE_AND_ADAPTATION_POLICY.md`'s "Current implementation" section and `HANDOVER.md`
now state the scope explicitly instead.

**Recommendation.** Option 2 (scale the auto poles), decided by the owner and recorded in an ADR
before P5, since the §5.4 overlay in the Advanced view will otherwise display a trim the user
cannot hear.


### OQ-007 — Does the release pipeline ship installers at P6? · `Resolved 2026-08-02`

**Question.** Anamorph ships an Inno Setup installer (Windows), a `.pkg` (macOS) and shell
installers inside the Linux zip, plus a tag-triggered draft-release pipeline. Anabasis's §11 P6
says only "presets, performance optimisation, pluginval L10, DAW matrix, documentation."

**Decision (2026-08-02, under the owner's v0.1.0 blanket approval; ⊕ for the post-v0.1.0 fine
review).** **v0.1.0 ships as plain zips** — the CI build artifacts, stripped and (on macOS)
ad-hoc-signed, zipped per platform, exactly what the existing `build.yml` produces. The full
installer/packaging set (Inno Setup, `.pkg`, tag-triggered draft-release pipeline) is a
well-understood port from Anamorph and is deferred to the first commercial release, alongside
OQ-002 (licence tier) and OQ-012 (validate-the-shipped-bytes), which gate that release anyway.
Nothing in the v0.1.0 tree changes: this resolution records scope, not code.

### OQ-015 — Does the learned-target restore need a THREADING_POLICY row, and which shape? · `Resolved 2026-08-01 (P4)`

**Decision: option 1 — ratify the existing implementation.** `ADR-0012` adds a
**GUI → Audio bounded staged record** row to `THREADING_POLICY.md`'s permitted-path table, with six
mandatory conditions (bounded and fixed at compile time · one writer, one consumer · payload
relaxed then flag release-stored, consumed `exchange(acquire)` at a block top · last-writer-wins,
never queued · the writer may acquire-load the flag to test consumption · the consumer only
adopts). The learned-target restore is ratified **unchanged** as the first instance;
`AdaptiveEngine::learned` is ratified as its Audio→GUI mirror. `THREAD_MODEL.md`'s two rows now
cite ADR-0012 instead of this question.

Rejected: re-expressing the pair as sentinel-valued slots (trades a documented mechanism for an
undocumented tearing window across two session loads), and deferring the restore to P5 (a
user-visible regression that returns the same question later).

**OQ-013 is not resolved by this.** ADR-0012 removed its *mechanism* objection — a four-scalar
trim vector now has a permitted transport — but whether a restored trim vector may be injected
into a running engine, and what that does to the adaptation state machine, is still open. No code
may wire that path.


### OQ-011 — What is the macOS deployment target? · `Resolved 2026-07-31 (P1)`

**Decision.** `CMAKE_OSX_DEPLOYMENT_TARGET=10.13`, kept **deliberately** rather than inherited
blindly. Evidence: the pinned JUCE 9 tree documents its deployment floor as **macOS 10.11**
(README §"Minimum System Requirements → Deployment Targets" [Verified from the fetched pin]), so
10.13 sits above the framework floor; it also matches the sibling product, keeping one support
claim across the family. The value governs the **x86_64 slice** only — the arm64 slice floors at
**11.0** by toolchain regardless — so the user-visible claim is: **macOS 10.13+ (Intel), 11.0+
(Apple Silicon)**. Restated in `COMPATIBILITY_MATRIX.md` when that document lands (P2). The first
macOS CI run is the warning-free check on the value; the `build.yml` comment carries the decision
at the point of use.

### OQ-010 — Does the limiter lookahead get an explicit 0 / off position? · `Resolved 2026-07-31`

**Question.** §4.3 specifies the limiter lookahead as **0.5–10 ms** (default ≈ 2 ms). On that
range the plugin **always reports non-zero latency to the host**; there is no zero-latency
configuration.

**Why it cannot be deferred.** Parameter ranges are semantic and become contract at the first
shipped build (`PARAMETER_COMPATIBILITY_POLICY.md` rule 3): widening the range later to add a 0
position re-scales every saved session's normalised value and is an `ARCHITECTURE_REVIEW_GATE`
item. It also determines what `DSP_POLICY.md` invariant 2 and the release checklist can assert —
"with lookahead 0 and oversampling off, reported latency is 0" is not testable on a 0.5–10 ms
range, so the invariant is phrased against the lookahead **allowance** instead (`DESIGN.md` §3.3
makes the reported contribution a constant 10 ms, so the figure is not a function of the engaged
value at all).

**Trade-off.** An off position enables a genuinely zero-latency mode (useful while tracking, and
in hosts with poor PDC), at the cost of a limiter that cannot catch transients ahead of time — a
markedly different, and worse, sound. Several mastering limiters deliberately omit it for exactly
that reason.

**Action** *(historical — superseded by the Decision below)*. Decide in `DESIGN.md` before `createAnabasisLayout` exists. If the answer is "no off
position", say so explicitly in the parameter table so it reads as a decision rather than an
oversight.

**Recommendation** *(2026-07-30, as it stood before the decision)*. `DESIGN.md` §3.4: **no zero/off position** —
keep 0.5–10 ms exactly. A 0 ms limiter degenerates into a clipper (the chain already has a
better one); the zero-latency tracking use case is out of this product class; and narrowing
never breaks sessions while widening later would. Stated in the §4.2 parameter table
(`lookahead`, row 27, footnote ⁶; non-automatable because the engaged value is a read offset into a live delay line, not because it moves PDC — under `DESIGN.md` §3.3 the reported figure is a constant allowance). Becomes part of ADR-0004
on sign-off; the DESIGN sign-off is the decision event that clears this entry's `Blocking P1`.

**Decision (owner sign-off, 2026-07-31).** **No zero/off position** — the range stays 0.5–10 ms
exactly as briefed. Recorded by **ADR-0004**, together with a second decision this entry did not
anticipate: reported latency is the **constant 10 ms lookahead allowance**, not the engaged value,
so the plugin's PDC no longer moves with the parameter at all. `DSP_POLICY.md` invariant 2 was
amended accordingly by that ADR — the invariant is now phrased against the *allowance*, and the
latch sentence **drops the lookahead**, leaving the oversampling factor and phase mode (both feed
`osLatency`, so both stay latched).

---

### OQ-005 — Extract a shared `rollytech-ui` module? · `Resolved 2026-07-31`

**Question.** §1.2 asks for an assessment of whether the shared UI layer (LookAndFeel, About page,
Settings page, Bypass placement, preset/A-B interaction) is worth extracting into a shared module
consumed by both products, versus copy-and-adapt.

**Trade-off.** Extraction gives one place to fix brand drift, but couples two release cycles and
makes any change to Anamorph's UI a change to a *shipped* product — which is an
`ARCHITECTURE_REVIEW_GATE` item over there. Copy-and-adapt ships faster (the brief explicitly
prioritises shipping on schedule) at the cost of guaranteed divergence.

**Note.** Anabasis must not modify the Anamorph repository, so extraction is not unilaterally
available to this project in any case — it would require a coordinated change to both.

**Action** *(historical — superseded by the Decision below)*. Give a recommendation in `DESIGN.md` (§1.2 requires one). Do not extract without
owner approval.

**Recommendation** *(2026-07-30, as it stood before the decision)*. `DESIGN.md` §8: **copy-and-adapt now**, with
provenance headers pointing at the Anamorph originals; revisit extraction as a product-family
ADR after Anabasis v0.1.0 ships, when both UI layers are stable enough to see what is actually
common. **Becomes ADR-0009 on sign-off** (`DESIGN.md` §10), whose scope is wider than the UI
layer — `CLAUDE.md` §3 requires *every* cross-product copy, including the DSP-source adaptations,
to be ADR-recorded.

**Decision (owner sign-off, 2026-07-31).** **Copy-and-adapt, no shared module for v1.** Recorded
by **ADR-0009**, whose scope is deliberately wider than the UI layer: `CLAUDE.md` §3 makes *every*
cross-product copy ADR-recordable, so the DSP-source adaptations (K-weighting coefficients, the
Measure+Predict structure, the `ScopeBuffer` ring, the transition taxonomy) are covered too.
Extraction is revisited as a product-family decision after v0.1.0 ships — that revisit will be a
new ADR, not a reopening of this entry.

---

### OQ-004 — Simple ⇄ Advanced coexistence strategy · `Resolved 2026-07-31`

**Question.** §5.3 requires a decision: when the user has edited parameters manually in Advanced
and then returns to Simple, how do macro values and manual values coexist? The brief names two
candidate directions (macro takes precedence with a clear notice; or offer a "carry over" option)
and requires the strategy to be **argued in the design document before implementing**.

**Hard constraint regardless of the answer.** Switching modes must not change the sound *at the
moment of the switch* (`MODE_AND_ADAPTATION_POLICY.md`). Any strategy that fails that is
excluded.

**Action** *(historical — superseded by the Decision below)*. Argue and decide in `DESIGN.md`; record as an ADR before P4 implementation.

**Recommendation** *(2026-07-30, as it stood before the decision)*. Argued in `DESIGN.md` §5.3:
**macro-latch with re-engage on touch** — returning to Simple moves nothing (invariant 2 holds by
construction); manually edited parameters are *detached* from the macro and badged; the next
macro-knob gesture re-engages them through the normal rate-limited glide, which is the "clear
notice" moment. Carry-over offsets were rejected (history-dependent, untestable mapping;
compounds with adaptation). Becomes ADR-0005 on sign-off.

**Decision (owner sign-off, 2026-07-31).** Macro-latch with re-engage on touch, as recommended.
Recorded by **ADR-0005**, which also fixes the macro-write/manual-edit discriminator (a
message-thread re-entrancy flag **and** a gesture bracket) that the rule depends on.

---

### OQ-001 — Which JUCE 9.x point release do we pin? · `Resolved 2026-07-30`

**Question.** §2 requires pinning the newest stable JUCE 9.x point release by its tag's
**immutable commit SHA**, and recording the tag in `README.md`.

**Decision (owner, 2026-07-30).** Pin **the same JUCE the sibling product pins: 9.0.0**, by the
tag's immutable commit SHA `f8f8864172464b9adf9eba6101e1f784838d1597`.

**Rationale.** A single shared framework version across the product family keeps the Level-5
audition baseline comparable between the two plugins and means one dependency audit, one set of
JUCE-attributable behaviour, and one bump decision for both products. Anamorph has already
verified this exact commit headlessly (its ADR-0022 records a 32-scenario twin dump proving
engine output bit-identical 8.0.14 → 9.0.0, including reported latencies), so Anabasis inherits a
framework revision with evidence behind it rather than an unexercised newer one.

**Recorded in.** `README.md`, `docs/policies/DEPENDENCY_POLICY.md`, `docs/procedures/BUILD.md`,
`docs/HANDOVER.md`. **Recorded by ADR-0008** (Accepted 2026-07-31) — the P0 build-decision ADR this
entry's standing obligation named, so that half of the obligation is **discharged**. What remains is
that it must be written into `CMakeLists.txt` (`ANABASIS_JUCE_VERSION "9.0.0"` +
`ANABASIS_JUCE_TAG "f8f8864…"`) when that file is created at P1.

**Evidence [Verified]:** the version + SHA are read from the sibling repository's
`CMakeLists.txt:36-38` and its ADR-0022. **Not verified from this repository** — Anabasis has no
build yet, so "this pin configures and builds" becomes Verified only at P1.

**Standing obligation.** This is now a pin, so `DEPENDENCY_POLICY.md` applies in full: any later
change is an `ARCHITECTURE_REVIEW_GATE` Build System change requiring an ADR plus the rule-2
verification. Also: §2 requires checking for a newer stable 9.x (and reporting rather than
adopting a JUCE 10) — that check is now a *deliberate deferral*, not an oversight. Re-run it if
9.0.0 turns out to lack something this project needs.

---

### OQ-003 — Plugin identity codes and bundle ID · `Resolved 2026-07-30`

**Question.** `juce_add_plugin` needs `PLUGIN_MANUFACTURER_CODE`, `PLUGIN_CODE`, `BUNDLE_ID`.
These are **permanent host-facing identity**: the manufacturer code is the AU component's
manufacturer field, and JUCE derives the VST3 class UID from the manufacturer code + plugin code +
plugin name. A host that recorded the old identity in a session does not load a *changed* plugin —
it reports the plugin as **missing**.

**Decision (owner, 2026-07-30).**

| Field | Value | Rationale |
|---|---|---|
| `COMPANY_NAME` | `RollyTech` | same brand |
| `PLUGIN_MANUFACTURER_CODE` | **`RTec`** | The manufacturer code identifies the **vendor**, so it is shared by every RollyTech plug-in. `Anmf` was rejected: it abbreviates *Anamorph*, the first product, which does not survive a product line. **Anamorph is changing to `RTec` in its 0.9.1** so the two products agree from the start (Anamorph ADR-0023). Also considered and rejected: `Roll` (a common English word — higher chance of colliding with another vendor's registered code), `RolT`, `RlyT`. AU requires ≥ 1 uppercase character; `RTec` satisfies it. |
| `PLUGIN_CODE` | **`Anbs`** | Per-product and must be unique; `Anmr` is Anamorph's. |
| `BUNDLE_ID` | **`com.rollytech.anabasis`** | Matches the sibling product's pattern. |
| `VST3_CATEGORIES` | **`"Fx" "Dynamics" "Mastering"`** | A maximizer, not a spatial effect (Anamorph uses `"Fx" "Spatial" "Stereo"`). |

**Consequence — none, and that is the point.** Anabasis has never built, so it adopts the final
vendor code before it can ever have an identity to break. Anamorph pays a one-time disruption
(its KI-016) precisely so that this repository does not.

**Standing obligation.** From the first build that leaves this repository these values are frozen
(`COMPATIBILITY_POLICY.md`). Anamorph has already spent the "before the first release" exception
for the manufacturer code; there is no comparable exception available here.

**Recorded in.** `docs/procedures/BUILD.md` §Plugin identity. Must be written into
`CMakeLists.txt` when it is created at P1. **Recorded by ADR-0008** (Accepted 2026-07-31).

**Evidence [Unverified].** No build exists, so "these values register correctly in a host" is
unproven — it becomes Verified at the first Level-5 check (P1).
