# TESTING.md

How to run and interpret the validation suite. Acceptance levels and the hard gate are defined in
`docs/policies/TESTING_POLICY.md`.

> **Status:** the two suites exist and are green (P1 skeleton, 2026-07-31) — `AnabasisTests`
> (DSP acceptance) and `AnabasisStateTests` (state / parameter compatibility). Rows in the
> invariant→test map still marked `TODO (P2+)` are the ones whose DSP does not exist yet; the
> P1 rows are live. The structure below describes the suites as built.

## Headless self-tests


## Realtime enforcement (0.2.0, ADR-0029)

Three tiers, and none of them subsumes another. Run them in this order when touching the audio path:

1. `python3 scripts/check-realtime.py --self-test && python3 scripts/check-realtime.py` — seconds,
   no build, every platform. It reads the branches the suite never executes.

   **It is fail-closed on its own inputs since 0.2.10, and the success line says so.** The gate
   used to report "N ordering requirement(s) met" from `len(REQUIRED_ORDER)` — the number of rules
   it was *asked* to check, not the number it *proved* — so a rule whose file had been renamed or
   deleted passed vacuously: nothing matched, the rule was never evaluated, and the count was
   printed anyway. It now reports "**N of M ordering requirement(s) verified**" from what was
   actually reached, fails when a required rule goes unreached, and refuses an empty input set
   outright (exit 2) rather than reporting "0 file(s) scanned" as success. Four of the self-test's
   cases drive `scan_repo` against real temporary trees for exactly these paths.
2. `scripts/run-tests.sh` — `testTheAudioPathAllocatesNothing` arms `tests/AllocationGuard.h` around
   `AnabasisEngine::process` across the configuration matrix. **Read its two `note:` lines**: they
   say how many calls were armed and which counters were live. A run that skips the assertions says
   so; it never passes them silently.
3. RealtimeSanitizer, which needs the pinned Clang (`scripts/setup-llvm-apt.sh <major>`; the major
   is `ANABASIS_CLANG_VERSION` in `build.yml`):

   ```
   cmake -B build-rt -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
     -DCMAKE_C_COMPILER=clang-<n> -DCMAKE_CXX_COMPILER=clang++-<n> \
     -DCMAKE_C_FLAGS="-fsanitize=realtime -fno-omit-frame-pointer" \
     -DCMAKE_CXX_FLAGS="-fsanitize=realtime -fno-omit-frame-pointer -DANABASIS_RTSAN_LANE=1" \
     -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=realtime" -DANABASIS_BUILD_STANDALONE=OFF
   cmake --build build-rt --target AnabasisTests
   ./build-rt/AnabasisTests_artefacts/RelWithDebInfo/AnabasisTests
   ```

   **Do not set `RTSAN_OPTIONS`.** The default halts on the first violation and exits 43;
   `halt_on_error=false` makes the process print violations and exit 0, which is a gate that cannot
   fail.

**A comparator proves it discriminates before its agreement counts.** `AnabasisChannelProbe
--assert-discriminating` refuses to print a baseline when two configurations produce identical
output and the pair is not declared in its source. A twin-build comparison over a collapsed scenario
set makes any two builds agree while testing nothing — which is exactly the evidence a dependency
bump is judged on.

```bash
scripts/build.sh                 # build (produces AnabasisTests + AnabasisStateTests)
scripts/run-tests.sh             # runs BOTH console apps (fail-closed: a missing binary fails)
```

`run-tests.sh` finds both binaries under `build/` and runs them; it exits non-zero on any failed
`check` **or a missing binary**. The missing-binary case matters: without it, a build that
produced nothing would pass the gate silently.

## Raw scanner output (SARIF artifacts)

Layer 1 of `TESTING_POLICY.md` (static analysis) runs two scanners in CI, and both publish their
findings twice: to GitHub Code Scanning, and — since this change — as an Actions artifact carrying
the scanner's own raw SARIF.

| Analyzer | Workflow | Artifact | SARIF on the runner |
|---|---|---|---|
| CodeQL | `codeql.yml` | `codeql-sarif-<language>-<sha>` | `${{ runner.workspace }}/results/<database-language>.sarif` |
| PREfast (MSVC `/analyze`) | `msvc.yml` | `prefast-sarif-<sha>` | `build\results.sarif` |

**Why the artifact exists.** The Code Scanning alert and check-run annotation APIs are not reachable
from every audit context, so the dashboard could not always be read back. The artifact is the raw
scanner report, retrievable through the ordinary Actions artifact interface.

**Two things to know before reading one.**

- The CodeQL artifact is **strictly richer than the dashboard**. `paths-ignore: build` filters the
  fetched JUCE tree out of the *alerts*, but those results are still present in the raw SARIF — so a
  non-zero result count here does not mean a first-party finding. Check the `uri` of each location:
  anything under `build/_deps/` is third-party JUCE.
- The CodeQL filename is the **database** language, not the matrix language — the `c-cpp` entry
  writes `cpp.sarif`, the `actions` entry `actions.sarif`. The workflow globs `*.sarif` rather than
  hard-coding that mapping.

Retention and failure behaviour are documented under "Artifact safety rules" in `CI_CD.md`: the
upload is gated on `!cancelled()`, not on success, because a report Code Scanning rejects is exactly
when the raw SARIF is most worth keeping.

## Documentation structure lint

Not an audio test, but it is a CI gate — recorded here because
`DOCUMENTATION_LIFECYCLE_POLICY.md`'s trigger map routes CI-workflow changes through this file:

```bash
python3 scripts/check-docs.py --self-test   # the checker's own guarantees; it prints its own case count
python3 scripts/check-docs.py               # whole-repo scan; exit 1 on any finding
```

The `docs` job in `build.yml` runs exactly these two commands on every push, self-test first — a
clean corpus scan is not evidence unless the script's own guarantees were exercised in the same
run. What it checks, and the limits of what it can prove, are stated in the script's docstring.

## Evidence-anchor lint

The other documentation gate, in the `source-lint` job rather than `docs` because it reads SOURCE
as well as prose. Documents of record cite their evidence as `some/file.cpp:695-752`; an edit above such a line
re-aims it silently and the document keeps reading as though it were still correct. (That example
names an untracked path deliberately — an illustration spelled with a tracked one is a citation as
far as the tool is concerned, and gets re-anchored along with the real ones.)

```bash
python3 scripts/check-citations.py --check              # base defaults to origin/main; exit 1 on drift
python3 scripts/check-citations.py --check --base @{u}  # and against what CI will use — see below
python3 scripts/check-citations.py --fix                # re-anchor, then RE-READ what it moved
```

**`HEAD~1` is not that base, and reaching for it is how this gate went red twice in a row.** With
the drifting edits still uncommitted, `HEAD` IS the commit you are pushing on top of, so `HEAD~1`
is one too far back — it compares against a revision CI will never look at, passes, and says
nothing about the one it will. `@{u}` is right in both states and is the form to use.

Run `--check` before pushing any change that moves lines in a tracked source file, and `--fix` in
the SAME change set that moved them — that is the repository's re-anchoring rule, and the gate
exists because 0.1.4 proved it does not survive being remembered.

**A repair looks exactly like drift to this tool, so declare it in the same commit.** `--check`
compares the TEXT at the base line against the text at the current line; it has no way to know
that `:330 -> :342` was a correction rather than a slip. So the run AFTER a re-anchor asks for the
re-anchor to be reverted, and the gate is red on the commit that fixed it — unless the new
spelling is added to `DELIBERATE_REAIMS` in that same commit. Re-anchor and declare together;
never in two pushes.

**Run it against BOTH bases, and this is not optional pedantry.** `--check` alone uses
`origin/main`; CI compares against the PREVIOUS PUSH. Those two disagree about which branch the
tool takes: a document whose citation COUNT differs from a base falls to the ordinal-pairing
fallback, which only judges base spellings still present verbatim — so against one base a set of
re-aimed anchors is silently unjudgeable and against the other it is flagged. Round 7 passed
locally on `origin/main` and failed CI on nine anchors for exactly that reason.

```bash
python3 scripts/check-citations.py --check                 # origin/main
python3 scripts/check-citations.py --check --base @{u}     # what CI will use on the next push
```

`@{u}` is the upstream tip — the commit you are pushing ON TOP OF, which is exactly what
`github.event.before` will be. `--base HEAD` is only the same thing while the drifting edits are
still uncommitted; once they are committed it means "compare the tree against itself", which is
clean by construction and tells you nothing.

Three limits are worth knowing before trusting a clean run, all of them stated in the script's
header:

* It proves anchors did not MOVE, never that they were aimed correctly to begin with — and this
  is the limit that matters most, because the tool makes a mis-aimed anchor look MAINTAINED. It
  was first recorded here as "three citations", which was an under-count found by inspecting three;
  a full audit of the governed documents found the majority of anchors in the architecture set had
  been wrong since before the tool existed, each faithfully carried onto the same unrelated code by
  every re-anchoring since. Anchors are therefore spelled with the SYMBOL beside the line number
  wherever the claim names one: that is the half a reader can check, and the half that survives
  the tool being wrong.
* It judges only citations spelled from the repository root and naming one of its tracked files.
  A bare file name, a sibling checkout's path, or a `<rev>:`-pinned anchor is deliberately left
  alone — the ownership test is narrow because every misclassification is a corrupted document.
* Anchors it could not judge (re-spelled or removed since the base) are counted and reported
  separately, so "17 anchors verified" never quietly means "17 of 33".

`--fix` is not a substitute for reading. It preserves the TEXT an anchor named, which is exactly
how a citation that was aimed at the wrong code stays aimed at the wrong code.

## Suite structure

### `tests/dsp_tests.cpp` → `AnabasisTests`

Deterministic DSP acceptance checks using a `check(cond, "what")` counter harness; `main()` calls
every test and exits non-zero on any failure. No test framework, no dependencies.

Planned coverage, one test per `DSP_POLICY.md` invariant (see the invariant → test map there):
chain order; reported latency; true-peak accuracy; **output never exceeds the ceiling**;
oversampling scope; ADAA aliasing measurement; null-with-defaults and bypass-null; click-free
transitions per switchable path; no NaN/Inf/denormals across the feature × oversampling ×
sample-rate matrix; loudness-compensation render neutrality; LUFS against the EBU R128 vectors;
dither placement and default.

**Four of these have a stimulus mandated by an ADR, not left to the implementer.** A test name
alone does not carry the property; these are the cases where the wrong stimulus passes vacuously:

| Test | Mandated stimulus | Source |
|---|---|---|
| `testOutputNeverExceedsCeiling` | Run in **both EQ positions**, and the Post case must include a **+12 dB shelf after the limiter** — the exact signal the clamp placement exists to survive | ADR-0002 |
| true-peak accuracy (≤ 0.1 dB) | The **whole OS matrix** — Off / 2× / 4× / 8× / 16× **× both phase modes** (minimum / linear) — because the estimator's input path differs per setting: its own 4× interpolator, a further ≥ 2×, or the oversampled signal read directly. It must cover **both taps**: the limiter's detector *and* the ceiling clamp's (ADR-0002), which read at different points in the chain. The `Off × linear` cell is knowingly degenerate — no filter is instantiated at Off, so phase cannot reach the estimator; keep it (uniform sweep) but do not hunt for a difference there | ADR-0003 item 9 |
| `testReportedLatencyMatchesImpulse` | The impulse must land at **exactly `maxLookahead + OS` for every lookahead value**, not just at the range ends — the constant-allowance contract is what makes a padding bug a test failure | ADR-0004 |
| click-free transitions | Must include a **lookahead move** — it is the one switchable path with neither a duck nor a latch (`DSP_POLICY.md` invariant 8) | ADR-0004 |

### `tests/state_tests.cpp` → `AnabasisStateTests`

Compiles the **real** plugin sources into its own console target, so it exercises the actual
`AudioProcessor` rather than a mock — **and, since P5, the actual editor**: a growing set of tests
call `createEditor()` and walk the resulting component tree
(`testTheSettingsPanelFollowsAProjectLoad`, `testThePopupShieldActuallyCoversTheEditor`,
`testEveryComboMenuFitsItsControl`, `testTheSavePresetNameFieldIsTaggedForItsFocusGlow`, the R2
tooltip sweep, the knob-position sweep). The tests still run **headlessly** and open no window: the
editor is built, sized and inspected, never shown, and nothing here runs a message loop — which is
why a `juce::Value` change (asynchronous through that loop) and anything requiring a modal pop-up
are outside what this target can reach, and are carried in `DEPENDENCY_POLICY.md`'s JUCE-internals
register instead.

A view's own ARITHMETIC is reached a different way, and 0.1.6 is the case that shows why both are
needed. `GrHistoryView` publishes the parts of its draw that carry a correctness argument as pure
statics — `windowEntries`, `buckets`, `bucketX`, `drawsZeroRegion`, since 0.1.6 `grY`, and since
0.2.8 `entryPeriod`, `smoothedHead`, `phaseOf`, `parked`, `paintHead`, `frameFor`, `readFloor` and
`bucketReads` (0.2.8's `tipFirst`, the trailing window the newest vertex read, was removed in
0.2.11: the newest drawn bucket reads its own span like every other), since 0.2.12 `visibleRight`
(whose sweep pins BOTH of the requirements it holds apart — the uncapped bound wherever a window
has three or more buckets, and a non-empty clip for every window and every plot at least two
columns wide, the property whose absence blanked the plot at a ten-second host block),
and `firstDrawn` (the oldest bucket a frame may draw once the ring's floor is taken into account),
and `hiddenColumns`/`leadBuckets` (the columns the boundary hides, which is how far right the frame
is drawn, and the buckets of earlier history that shift needs on the left),
plus the ring's own
`GrHistoryBuffer::prepare`, `prepared` and `batchIntact` (pinned by `grPrepared`, through the ring
and through the wrapper) — because an expression reachable only from `paint` is one no test can pin
and no mutant can kill; the GR trace's vertical mapping under-reported reduction past 12 dB for
three rounds while it sat inline, and its horizontal geometry stepped a non-integer pitch once per
bucket for six (0.1.2 → 0.2.8) while the pinned property was only *where* buckets land, never *how*
they move — the 0.2.8 walk in `testGrHistoryWindowNeverAsksForTheHeadSlot` now holds the per-entry
motion at every head across three buckets, which is the assertion the stepped form fails, and since
0.2.11 holds that only complete buckets are drawn, that every drawn bucket's read set is a constant
of its index, and that a completing bucket appears at the edge as a new vertex, and since 0.2.12
that the visible boundary sits left of the newest drawn vertex at every head and both ends of the
phase. The properties of that view no static can carry — where the stroke's cap lands, and what a
clip leaves on screen — are pinned through a RENDERED snapshot at every fill of the newest bucket
(`grPaint`, 0.2.11; extended at 0.2.12 to the visible boundary: the last visible column lit on every
fill, every column beyond it untouched on every fill), because the last plot column had been
blinking for three rounds while every static was green, and the strip beyond the newest vertex had
then been on screen for one more. A THIRD rendered pin was added by the 0.2.12 review
(`testGrHistorySurvivesAHostBlockOfTenSeconds`): a clip rectangle is not a number either, and a
boundary computed one column left of the plot blanked the whole history while every arithmetic
static stayed green. The review's second finding needed neither a render nor the editor: a REAL
`GrHistoryBuffer` walked past 400 heads with a pattern whose minimum sits on the first entry of
every bucket makes a one-entry truncation move a drawn value by 11 dB, so
`testTheOldestDrawnBucketKeepsItsValueUntilItLeaves` can assert the invariant itself — no drawn
bucket changes value while it is drawn — rather than a pixel consequence of it. The view-switch
defect needed a render again, and a THIRD frame to compare against:
`testTheGrHistoryIsCurrentTheFrameItBecomesVisible` snapshots the view before the spectrum takes the
well, again the instant it gets it back, and once more from a view that has never been stale, and
holds the middle one to the last rather than the first — the defect was a frame that was *valid*,
just not current, so only a comparison between two states can see it. The review finding under that
fix moved the same question OFF the pixels, because its amplitude is sub-pixel: what a reveal has to
get right is the published pair, so `GrHistoryView::drawnFrame` was made public (the reason `tick`
is) and `testTheGrHistoryDoesNotResumeAnExpiredRamp` asserts the pair directly — hidden mid-ramp
with the transport stopped, the reveal must publish the parked value, and nothing may move for
thirty frames after it. `testTogglingTheGraphWellNeverMovesTheGrHistoryBackwards` pins the direction
the other candidate repair would have broken (`head + phase` may only grow across fifty switches).
The same round pins the spectrum's half of the lifecycle through its own state rather than a render
too, `analysedInDb()` being public for the same reason: a bin that was falling must have decayed by
the seconds the view was away (`testTheSpectrumIsCurrentTheFrameItBecomesVisible`), a view that came
back to rings nothing was written to must hold its trace bit-identically
(`testTheSpectrumHoldsItsTraceWhenNothingArrivedWhileHidden`), and a re-prepare during the switch
must reach the first visible frame as the floor
(`testARePrepareWhileHiddenDoesNotReachTheFirstVisibleSpectrumFrame`).

The 0.2.12 review round added one more thing the suite could not previously see: a defect that is
not in either trace but in the PAIR. `SpectrumView` draws two traces from two rings the audio thread
publishes with one release-store each, and the question — do the two traces describe the same span
of audio? — is invisible to a test that can read one of them, so `analysedOutDb()` joins
`analysedInDb()`. `testTheSpectrumsTwoTracesAlwaysDescribeTheSameSpan` then constructs the split
state directly rather than racing for it: a chunk pushed into one ring and not the other IS the
state the producer holds between its two publications, and it is reached with a `const_cast` on the
processor's own ring, the test standing in for the producer. Its two halves are pinned separately
because the repair has two — the READ cases advance the pair first so the tick actually runs and the
read is what decides, and the GATE case leaves the EMA mid-fall so that "the gate held" is
distinguishable from "the gate opened and the analysis landed on the same numbers". The ring's own
new entry point is pinned in the DSP suite beside the other `specSync` checks, the clamp included:
an end past this ring's head must read the head's own window, which is what makes a stale index from
the OTHER ring safe. The review round that followed added the other half — a window ending inside the
ring can still BEGIN outside it once a chunk longer than `capacity − kSize` has run past the
endpoint — and it is pinned BY VALUE rather than by count: the ring is filled with a ramp whose
sample IS its absolute index, so a frame the producer took back reads as something else and the
assertion sees it. `testTheSpectrumNeverDrawsAFrameTheProducerTookBack` then drives the same
boundary through the real analyser at four host block sizes (at the threshold, past it, a whole
ring, and far past it) in both publication orders and three times each, asserting that the two
traces agree and that a block leaving no common span holds the last coherent pair rather than
drawing half of one.

The round after that had to pin something the suite had never had to pin: a property that only
exists WHILE another thread runs. The analyser chooses its window from a snapshot of two rings and
then copies from both, and the defect was that a producer publishing in between made only ONE copy
come back short. That is pinned in two halves, which is the shape to copy for anything similar. The
DECISION is a pure static (`SpectrumView::onePairOneSpan`) with a seven-row truth table — both reads
whole, either one short, either floor past the window's start, and the empty span that is coherent by
definition — so the rule is mutation-killable without a thread. The STIMULUS is a thread:
`testTheSpectrumsPairSurvivesAProducerRunningDuringTheFrame` runs a producer publishing 13000-frame
chunks flat out beside 5000 drawn frames, with IDENTICAL audio in both rings, so that a frame whose
two traces differ at all is a frame whose two windows were not the same span — one assertion, one
direction, and no schedule to depend on: the fixed code cannot fail it and the unfixed code fails it
on every run measured. A thread is what this needed and a sleep is not: the property is "no
interleaving produces a mismatched pair", which more interleavings can only test harder.

The round after THAT had to reach one layer further out, and it changed what the suite is allowed
to look at. The two defects were in the hand-over from `tick` to `paint` — a boundary that is TWO
THREADS on macOS and Windows and one on Linux — so `SpectrumView::readPublishedFrame` and
`paintedWindow()` are public for the same reason `analysedOutDb()` is: the property at stake is what
a SECOND thread can see, and a test that can only read the message thread's own state cannot say it.
`testTheSpectrumsRendererNeverSeesHalfOfTwoFrames` pins the deterministic half first (what a tick
publishes is the frame it committed; an idle tick disturbs nothing; a paint takes the published
window and follows it across publications) and then runs the threaded half with a MARKER made of the
audio itself: both rings get identical blocks, so a coherent frame's two traces are bit-identical,
and the tone alternates over a whole 4096-frame window per tick, so a frame assembled from two ticks
disagrees across the spectrum. Any inequality at all is therefore a mixed frame and there is no
threshold to argue about — 0 of 306 485 reads on the fixed tree, 1 161 778 of 1 321 607 with the
renderer reading the tick's working vectors. `testAReconfiguredSpectrumNeverKeepsThePreviousMapping`
pins the other defect across the ring-capacity boundary (8192, 13000, 16384, 32768 samples, with and
without new audio, re-preparing 48 kHz → 96 kHz so the same tone moves from bin 512 to bin 256):
where a window survives, the first frame after the change is the new configuration's; where none can
(at or above a whole ring, where none ever will) the view publishes the empty frame rather than
leaving the old rate's spectrum under the new rate's axis. The reader-side in-flight reserve that
replaced the withdrawn ring-side one is pinned by `specReserve` — the arithmetic on its own, and
that a drawn frame's window obeys it at the block the host prepared.

Round 11 added the third fact a frame has to carry, and it is not in the trace at all: the SAMPLE
RATE its bins are read through. `testASpectrumFrameCarriesTheRateItsBinsAreReadThrough` uses the
audio as its own marker — 6 kHz is bin 512 at 48 kHz and bin 256 at 96 kHz, so the peak bin of a
published trace says which configuration produced it — and its load-bearing case is the one that
needs no thread at all: reconfigure and refill with NO tick in between, so the processor has already
moved to the new rate while the newest frame any renderer can pick up is still the old one, then
paint. The frame must be drawn through the rate that produced it. `paintedFrame().rate` is what makes
that observable: `paint` writes the rate it actually resolved back into the frame it drew, so a
renderer reading the processor behind the frame's back is visible from outside rather than only in
the pixels. `testNoFrameARendererPicksUpEverMixesTwoConfigurations` then runs the same marker past a
reading thread while the main thread churns 48 kHz ⇄ 96 kHz, which is what kills a rate published on
the tick's schedule rather than the frame's — the deterministic cases cannot see that one, because
single-threaded there is no window between the two publications to observe.

**THE PREMISE OF A THREADED TEST IS ESTABLISHED, NOT ASSERTED — and CI taught this file the same
lesson twice.** `testTheFrozenLatchNeedsNoThreadCrossing` learned it on 2026-08-14 (run
31801408265). `testTheSpectrumsRendererNeverSeesHalfOfTwoFrames` learned it on 2026-09-06, in the
same `sanitizers` job and in the same shape: the suite reported 1248 checks / 1 failure while
memcheck reported 0 errors from 0 contexts, and the one failure was the premise — `distinct > 1`,
i.e. the reading thread got exactly ONE productive turn in 4000 ticks. The mechanism is valgrind's
serialised scheduler: while the reader holds the CPU the published frame cannot move, so every
iteration of a reader quantum returns the same frame and `distinct` counts reader quanta that
straddled a publication rather than reads. MEASURED under memcheck pinned to one CPU, 500 ticks: 481
distinct frames idle, 221 under 8 competing spin loops, 219 under 24 — the reader's share of frames
is the scheduler's to decide. The fix is the one the older test already prescribes, "remove the
dependency rather than tune it": wait for the reader to be RUNNING and to have taken a whole frame
before the measured section begins (guaranteed, because nothing publishes during that wait, so the
counter is even and stable), and keep publishing and yielding until it has taken a second, different
one. A stronger stimulus than the original, not a weaker one — the frame count the property is
measured over becomes a floor rather than a hope. Re-run under memcheck on one CPU: 1261 checks, 0
failures, 0 errors from 0 contexts.

**AND CI TAUGHT IT A THIRD TIME, in the round that quoted the lesson.** The repair above was applied
to `testTheSpectrumsRendererNeverSeesHalfOfTwoFrames` and NOT to
`testNoFrameARendererPicksUpEverMixesTwoConfigurations`, written in the same round: that one ran a
bounded churn and then asserted that the reading thread had seen a frame at both rates. The
`sanitizers` job failed on it — 1261 checks, 1 failure, memcheck itself reporting 0 errors from 0
contexts — for exactly the reason the paragraph above gives. Two things follow, and both are now in
force across every threaded test in this file. **A premise is established, never asserted after the
fact**: each rate is confirmed by waiting for the reader to have seen it, and the wait terminates
because the published frame is that rate's and stays it while the wait spins. **And every such wait
is BOUNDED**, because a wait that can only end when the property holds turns a LIVENESS defect into a
CI timeout, which reports nothing — bounded, the same defect fails the premise and names itself. The
caps are far above what any working scheduler needs. A conjunction is also split into one check per
conjunct, so a failure says which half broke; the round-10 form did not, and its log could not
distinguish "the reader never ran" from "the reader never saw the second rate".

**THE TEST THAT ACTUALLY PAINTS.** `specPaint`
(`testAPaintThatLosesTheRaceKeepsTheFrameItAlreadyHad`) is the only test in the tree that calls
`SpectrumView::paint` from a thread that is not the one ticking, and it exists because nothing else
could see what a renderer does with a read it LOSES. `readPublishedFrame` cannot preserve its output
on failure — the 4096-bin copy has already happened by the time the bracket can be checked — so a
caller that reads into its drawing buffers and ignores the result draws a mixture of two
publications. That is a broken invariant with no race in it, so ASan, UBSan and memcheck are all
silent on it by construction, and it took a test that paints. **The marker has to be
position-independent, and the first one was not.** A torn copy is a PREFIX of one publication and a
SUFFIX of another, and where the two sweeps cross depends on their relative speed, so two marker
tones catch only a tear that falls between them: measured, with tones at bins 21 and 1707 the defect
was caught in some runs and missed in others. The two publications therefore differ in EVERY bin —
one is white noise, the next is digital silence — so a clean frame has its first and last bins both
lit or both at the −120 floor and a torn one has one of each. `lit(first) != lit(last)` is the
detector, the crossing can be anywhere in the array, and there is no threshold to argue about, since
noise reads tens of dB above the floor and silence reads exactly the floor. `dt = 1 s` drives the
EMA's decay to 0.9998 and one whole window per tick keeps the analysed span from being a blend of
the two. The premises are established rather than assumed: the run must have painted, and must have
drawn a fully lit frame AND a fully floored one, before "no painted frame was torn" means anything.
Measured on the shipped build: 6395 paints, 2966 fully lit frames, 3429 fully floored ones, 0 torn.

**THE TWO TESTS THAT BUILD A SPLIT RESET.** `specGen`
(`testNoSpectrumFrameEverPairsTwoConfigurationGenerations`) and `specStraddle`
(`testAResetThatLandsInsideATickNeverReachesTheScreen`) pin the round-13 half of the frame
invariant: one frame is one span AND one configuration generation. Neither can be built through
`processBlock`, which publishes to both taps with one `num` and so can never put one ring a
configuration ahead of the other, so both reach past it — the `const_cast` handles on
`AnabasisAudioProcessor::spectrumInRing()`/`spectrumOutRing()` that `specLap` already uses.

`specGen` is single-threaded and exact. It settles both traces on one configuration, rewinds ONE
ring and refills BOTH — which is precisely the state a reader is in when it has accounted for one of
`AnabasisEngine::prepare`'s two back-to-back rewinds and not the other, since a rewind sends the
producer back to slot 0 and the frames it writes next overwrite exactly the slots the shared window
reads. The assertion is made at the bin the PREVIOUS configuration's marker occupied, which the new
configuration's audio has nothing at: the two traces must agree there, and must agree that it is
gone. Before the repair they were 104.3 dB apart at a 512-frame block and 69.0 dB at 4096. Three
markers at three bins (5 kHz at 48 kHz = 427, 7 kHz at 96 kHz = 299, 12 kHz at 96 kHz = 512) mean a
trace says in its own numbers which configuration it belongs to, with no threshold to argue about,
and the test walks the four required cases, both split directions, the reset edge at a whole-ring
block, one and eight blocks after a reset, and a 48 → 96 → 48 → 44.1 → 88.2 → 44.1 sweep.

`specStraddle` covers the half a single thread cannot reach: a rewind that becomes visible AFTER the
tick sampled the two generations and BEFORE it re-samples them, so the reset edge is silent and the
post-batch re-read is the only guard. **Round 18 made that interleaving ESTABLISHED rather than
searched for.** Until then the producer swept the rewind's landing point with a doubling spin and a
yield count and `guardFired` counted the hits — a feedback signal that only goes non-zero after the
search has already succeeded, which converged natively on the first rounds and never converged at
all under valgrind's cooperative scheduler: 3 261 238 ticks, 6000 rewinds, zero straddles on a
GitHub runner (KI-019). The window is now ENTERED: the ticking thread blocks inside it at
`SpectrumView::whileBatchAnalysed` while a second thread performs the two real
`ScopeBuffer::reset` calls and the refill, so one straddle happens on every run on every scheduler,
and the frame it produces — full span, both traces at the floor — is asserted directly. The sweep,
the six-thousand-round hunt and `guardFired` as a pass condition are gone; what remains after the
forced straddle is sixty rounds of stress, still paced by the reader's own publications through a
condition variable, checking the invariants that need no particular interleaving (a floored trace
never appears beside a lit one, no trace ever holds two markers, no identity ever spans two
configurations). Both markers complete a whole number of cycles in one pushed chunk (96000 / 512 = 187.5 Hz;
6937.5 = 37 × 187.5 and 12000 = 64 × 187.5), because a repeated chunk that does not is a pulse train
whose splatter puts real energy in the other marker's bin — that mistake made the mixture detector
count the stimulus, at 59 frames a run, before it was fixed. Measured on the shipped build across
round 18's forty-run battery (twenty native, ten pinned to one core, ten under twelve competing spin
loops): the forced straddle occurs exactly once in every run, and the sixty-round stress that follows
it gives 60 reconfigurations, 234-246 lit frames, 0-2 of them floored by the guard — the guard
firing is now a diagnostic about the machine and not a pass condition — 0 lopsided, 0 mixed and 0
identity switches, with the premise counter `starvedAt` zero in all forty.

**THE TWO TESTS THAT PIN THE HISTORY'S CADENCE (round 14, OQ-017 fix 1).**
`testGrHistoryEntriesFollowThePreparedBlock` (`dsp_tests.cpp`) drives the real engine with a real
`GrHistoryBuffer` sink through `setGrHistorySink`, so the property is asserted where it is produced;
`testTheGrHistoryScrollsAtThePreparedBlock` (`state_tests.cpp`) asserts the same thing through the
wrapper's ring and `GrHistoryView`'s own `entryPeriod` / `windowEntries`, which is where the display
reads it.

The engine-level test's spine is **schedule-invariance asserted bit for bit**: the same audio is run
through six delivery schedules — 64, 128, 512, 1024, 4096 and a variable one whose seven sizes
(1, 3, 17, 63, 512, 1024, 1964) average the prepared block and none of which is a multiple of it —
and every entry of every run must be bit-identical to the reference. That one statement is "no
sample is lost", "none is counted twice" and "the statistics describe the entry's own span", and it
needs no tolerance to argue about. Beside it: cadence over thirty rate × block × D/B configurations
(44.1 / 48 / 96 kHz, 512 and the AU's 1156, D/B from 0.25 to 8); each entry's peak derived in CLOSED
FORM from the input and `groupDelaySamples()`, which is available because defaults on sub-ceiling
material are a bit-exact delay-aligned copy (`testNullWithDefaults`) — with a negative control
asserting that neighbouring entries differ, so a delivered block assigned wholesale to one entry
would fail it; a 64-sample transient inside a 4096-sample delivery landing in exactly ONE entry at
the index the grid and the delay put it at; the remainder walked sample by sample across call
boundaries; and D == B asserted BIT-IDENTICAL to the pre-0.2.12 wrapper expression itself
(`gainToDecibels (lastBlockMinGain(), -60)` and `lastRenderPeak()`), not to a remembered number.

**Two passes exist because mutation testing found the suite blind without them.** Deleting the
per-chunk reset of `grMinChunk` turns it into a running GLOBAL minimum — which is STILL
schedule-invariant (entry boundaries fall at the same absolute samples in every schedule) and STILL
agrees with `lastBlockMinGain()` (the per-call fold reads the same running value), so the cadence,
split and identity passes all stayed green while the GR trace would latch at the deepest reduction of
the session and never recover. Only a stimulus whose reduction GOES AWAY sees it, which is what the
loud-then-silent pass is for: it reads −3.62 dB in the passage and −3.62 dB after it with the reset
deleted, against better than −0.02 dB with it. And the re-prepare pass passed for the wrong reason
until it fed the pipeline first: `process` works IN PLACE, so re-using one buffer feeds the engine
its own delayed output and three calls of that is digital silence — a partial entry that had
survived a re-prepare would have carried silence either way. It now refills before every call and
asserts the loud render is really in the accumulator before the re-prepare decides its fate.

**Round 15 inverted half of that pass, and added the matrix behind it.** The PR review found that
`AnabasisEngine::prepare` dropped the partial on EVERY re-prepare while the ring keeps its entries
at an unchanged `(rate, block)` pair — so a transport start lost up to a prepared block of
already-rendered audio from a timeline that went on running, which is what ADR-0023 item 6 and
`USER_MANUAL.md` promise it will not. The partial now follows the ring's own gate.
`testTheHistorySurvivesASameConfigurationRePrepare` is the matrix: every pass is written against
SAMPLE CONSERVATION — the audio delivered is an exact multiple of the prepared block, so
`entries × B == samples` is an equality with no floor to hide in — and against a MARKER burst whose
render falls inside the partial, so which entry received those samples is a fact rather than an
inference. It covers a partial of 0, 1, 100, 300 and `B − 1`, forty pause/resume cycles, a
sample-rate change, a prepared-block-size change and an explicit `reset()` — which does NOT drop it, because `reset()` clears nothing in the ring and so has nothing to discard from the ring's timeline — and it closes with the
JOINT statement — the partial reaches an entry exactly when the ring kept its timeline — because the
defect was those two decisions disagreeing. `testTheGrHistoryScrollsAtThePreparedBlock` asserts the
same conservation through eight `prepareToPlay` cycles on the real wrapper. Ten checks fail against
the pre-round-15 engine.

**Round 16 replaced the decision those passes rest on, and `testTheHistoryTimelineIsTheRingsTimeline`
pins the replacement.** Round 15 had the engine MIRROR `GrHistoryBuffer::prepare`'s clear-on-change
comparison; that agreed for `prepare` and for nothing else, so a `GrHistoryBuffer::reset()` restarted
the ring's timeline while a partial from the old one survived into it — measured at 48 kHz / 512 with
511 samples in flight, the new timeline's first entry closed on ONE post-reset sample and carried the
previous timeline's peak. The engine now reads the ring's reset epoch instead.

**The measurement every pass makes is structural, and deliberately not the peak.** An engine that is
not reset keeps rendering pre-break audio out of its lookahead line quite legitimately, so a peak
alone cannot tell a leak from ordinary audio continuity. What can is **how many POST-break samples
the first entry after the break stands for**: `B` for a new timeline, `B − carried` for a continued
one — exact, and independent of what the pipeline holds. The test feeds post-break audio one sample
at a time to read that number off directly. Cases: a break on an entry boundary (five kinds, all
identical); a ring reset at 1, 100, 300 and `B − 1` in flight; the statistics assertion taken where
the engine is reset too, so the pipeline cannot supply the marker; a reset straight after a
publication; a same-configuration re-prepare; an engine reset that clears nothing; a configuration
change; attaching a sink to an engine that had already accumulated without one; and a
five-transition sequence. Twelve checks fail against round 15's engine, and all nine mutants of the
new mechanism are killed.

**One of those nine is worth its own paragraph, because it survived until a test was written for the
WRAPPER rather than the engine.** Deleting the sync from `AnabasisEngine::prepareHistoryTimeline`
passed both suites: every engine-level pass calls the timeline API itself, so none of them can see
the one production site forgetting it. `testTheGrHistoryScrollsAtThePreparedBlock` now re-prepares
the real processor at a changed block size with a partial in flight and measures the first new
entry's span through `processBlock`. It leaves 100 samples in flight rather than 300 deliberately: a
partial LARGER than the new block is dropped by `prepare`'s span guard — a well-formedness rule, not
a timeline decision — which would have masked the question the pass exists to ask.

**Round 17 pins what the ring's SIZE means, which is a duration rather than an entry count**
([ADR-0040](../architecture/design-decisions/ADR-0040-gr-history-ring-capacity-is-a-duration.md)).
Because an entry is one prepared block, `kSize` decides `kSize · block / rate` seconds — and no test
in the tree had ever asserted a number of seconds, so a ring sized against a 512-sample block passed
everything while retaining 0.6825 s at 192 kHz / 32.
`testTheHistoryWindowKeepsItsSecondsAcrossThePreparedPairs` sweeps twenty-six prepared pairs and
asserts the quantity the contract is written in — retained entries × prepared block ÷ sample rate.
Its bounds are **derived from `kSize`** rather than quoted, so the sweep pins the SHAPE of the
contract at any capacity (the whole window wherever the ring can hold one, never below §2.9's floor
down to the ring's own bound, and the exact proportion past it) while the named cases — 192 kHz / 32,
48 kHz / 64, 48 kHz / 8, and 192 kHz / 16 and / 8 either side of the boundary — pin its VALUE. It
closes on the real ring: a whole window's entries pushed at 192 kHz / 32, and the frame's own
`Buckets::first` used to find the oldest of them.
`testTheRingKeepsASecondOfEntriesAtTheSmallestPreparedBlock` adds the producer's half in the DSP
suite, where the engine can be driven: one second of audio at 192 kHz / 32 is 6000 entries, half
again what the whole 4096-entry ring held, and the marker in the first six blocks is looked for at
`groupDelaySamples / B` — where the engine's own latency puts it — so finding it there is what says
index 0 has not been re-used. Nine checks fail on the 4096-entry ring.

**`specFrame` places its reader instead of hoping it lands (round 18).** Its concurrent half used to
run a renderer thread against four thousand publications and assert that nothing it accepted was
mixed — and not one assertion in the function required the reader to have overlapped a publication
even once, so two hundred thousand reads could pass without entering the state the bracket exists
for. The reader is now put at all four states a publication has — before it, inside it with the
counter odd, across it, and after it — each through a rendezvous inside the production function,
each counted in the branch that verified that placement's own observable, and all four asserted
non-zero. The odd marker's real payoff is asserted directly rather than implied: the refusal happens
BEFORE the copy, so a reader that arrives with the payload torn still holds the last coherent frame
in its own buffers and the mixed pair is unreadable rather than read and discarded. What remains of
the free-running phase is a bounded stress — two hundred publications, twenty thousand reads, two
marker bins — whose value is coverage and not the proof.

**`specFrame` gained the premise it had always rested on.** Its marker is that identical audio in
both spectrum rings analyses to bit-identical traces, so any inequality a reading thread sees is a
mixed frame; nothing checked that, and a failure could therefore point at the wrong side of the
thread boundary. The writer's own pair is now compared on every tick, on the thread that produced
it, and the test prints its counts — reads, distinct frames, mixed frames, working-pair splits, and
the first offending bin with both values — so a failure carries its own evidence rather than needing
a second run.

**The hot pass runs FROZEN, and that is a measured property of the chain rather than a
convenience.** §5.4's `adaptiveEngine.finishBlock` runs once per `process()` CALL and its trims are
adopted for that whole call, so the DELIVERED size — not the chunking — sets the adaptation cadence:
a host running 64 adapts eight times as often as one running 512. That was true before this round and
is untouched by it. With the trims live the six schedules part by at most **0.0032 dB** of GR and
**0.00035** linear of peak, which a following pass asserts as a bound; frozen, they are identical,
which is what isolates the accumulation. The same pair of results is what re-measured the chunk
loop's transparency claim: moving a chunk boundary is bit-transparent WITH THE LIMITER ENGAGED, and
`AnabasisEngine.cpp`'s note now says so on that evidence rather than on a run that never engaged it.

**What the suite cannot see here, stated rather than implied.** `repaint()` is what carries a
published frame to the screen, and a headless suite has no repaint region to inspect: the tests pin
the published state and the painter's copy of it, so a mutant that deletes the `repaint()` call while
leaving the publication survives. The same limit applies to the data race itself, which is argued
from the memory model rather than measured (ADR-0038 records it the same way).

**The two tests that sleep, and why they are the only ones.** `testTheGrHistoryDoesNotResumeAnExpiredRamp`
and `testTheSpectrumIsCurrentTheFrameItBecomesVisible` each block for 40 ms between hiding a view and
showing it again. The quantity under test IS real elapsed time — the seconds a view's frame clock was
stopped for, which the views measure from the wall clock because neither the ring (it stops with the
transport) nor the `FrameClock` (its pacing state is reset on restart, deliberately) can report it —
so no injected `dt` can stand in for it without bypassing the code under test. They stay
deterministic because only a LOWER bound is asserted and `sleep_for` guarantees exactly that: it
blocks for at least the requested duration, 40 ms is 3.75 entry periods at 48 kHz / 512, and more
elapsed time only saturates the same clamp harder. A test that needs an UPPER bound on elapsed time
would not be admissible here.
`testGrHistoryAndTheMeterLanesShareOneReductionSpan` pins that mapping through the statics **and**
renders a standalone `GrMiniMeter` into an image (`createComponentSnapshot`, no editor and no
window) to check the OTHER readout of the same quantity independently — a test that quoted the
shared constant twice would pass with the meter dividing by anything.

This paragraph said "never instantiated" until 2026-08-13, having gone stale at P5 —
`TESTING_POLICY.md`'s harness-conventions bullet was corrected on the same point at 0.1.1 and this
copy was missed. An under-described coverage claim is not harmless: it invites the next contributor
to add a test that already exists.

Planned coverage: serialized-schema shape; the **parameter-registry snapshot**; raw-exact
save → load → save round-trip (byte-identical) and its fixed-point precondition
(`testRawRoundTripIsIdempotent`); the §4.4 structural-tolerance read rules — a valid root that omits
`ANABASIS` or `ANABASIS_INTERNAL` reads as *defaults*, never as "keep the live values"
(`testMissingChildrenReadAsDefaults`, which also pins the same rule at **PARAM granularity**: a
missing individual child resets that one parameter — behaviour supplied by the pinned JUCE's
reconnection fallback, not by our code, which is exactly why it is pinned); every legacy read path
via a frozen fixture;
corrupt/foreign-state robustness; user-preset round-trip + exclusion rules; A/B and view-param
preservation; **`testMacroDefaultIsFixedPoint`** — the macro mapping at the default position must
equal every managed parameter's declared default (ADR-0005, `MODE_AND_ADAPTATION_POLICY.md`
invariant 1); **`testModeSwitchIsSoundNeutral`** (invariant 2).

### The registry snapshot — how it is used

`tests/fixtures/parameter_registry.snapshot` freezes the parameter surface (IDs, names, order,
ranges, automation flags). The test fails on any change. Re-freezing is an explicit, deliberate
act:

```bash
AnabasisStateTests --write-snapshot     # ONLY for an INTENTIONAL parameter change
```

Re-freezing to turn a red test green is a compatibility break in disguise
(`PARAMETER_COMPATIBILITY_POLICY.md`). This test is what automates the "Parameter IDs unchanged"
release-checklist item.

**The fixture is pinned to LF by `.gitattributes`, and the comparison normalises line endings
anyway.** Git for Windows defaults to `core.autocrlf=true` — including on the GitHub-hosted
`windows` runner — so without the pin the fixture is checked out with CRLF there and all 49 lines
mismatch: a Windows-only red reporting a frozen-parameter-surface break that did not happen. Both
defences are kept because they fail differently: the pin fixes new checkouts, the normalisation
covers a clone made before the pin existed. On a real mismatch the test now prints the **first
differing line** and both sides — a bare `FAIL` costs a whole CI round to diagnose.

**Two defaults in the snapshot are knowingly off by ulps, and that is recorded here rather than
"fixed".** The dump writes `range.convertFrom0to1 (param->getDefaultValue())` — the value that
survives the *normalised* round trip, which is what a host actually restores — so a log taper's
`exp(log(x))` shows through: `limRelease` reads **100.000015** (declared 100 ms) and `eqBell2Freq`
reads **2999.999756** (declared 3000 Hz). The declared defaults in `PluginParameters.cpp` are the
round numbers; these are their images under the taper, correct to ~1e-7 relative and inaudible.
Rounding the dump to hide them would make the snapshot stop detecting a real taper change, which is
the one thing it exists to catch. `testRawRoundTripIsIdempotent` pins the property that actually
matters — that one save→load→save pass is a *fixed point*, so byte-identity holds — and it, not the
snapshot, is where a taper change is diagnosed.

## Writing a test

Use the existing harness and add the call in `main`. DSP behaviour → `dsp_tests.cpp`;
state/serialization/preset behaviour → `state_tests.cpp`.

**One documented exception to that split.** `testMacroDefaultIsFixedPoint` and
`testModeSwitchIsSoundNeutral` are *behavioural* guards but live in `state_tests.cpp`, because only
`AnabasisStateTests` compiles the wrapper sources (ADR-0008's target graph) and both need the APVTS
and the MacroEngine, which the DSP core deliberately cannot see (ADR-0001). Placement follows what
the target can link, not what the test measures — do not "fix" it by moving them.

**Every bug fix ships a regression test** that fails on the old code and passes on the fix
(`TESTING_POLICY.md` rule 1). A fix without one is not finished.

For this product specifically, a test that only uses well-behaved musical material is not a test.
Include hostile inputs: full-scale square waves, inter-sample-peak-heavy signals, DC, silence, and
parameters automated at audio rate.

## pluginval (VST3 conformance)

```bash
# The GATE value comes out of the one place that holds it, never pasted — same
# extraction README.md and CI_CD.md use, and for the reason CI_CD documents
# against itself: a literal here would go stale on the next raise while the
# comment beside it still claimed to be current.
STRICTNESS=$(sed -n 's/^  ANABASIS_PLUGINVAL_STRICTNESS:[[:space:]]*\([0-9][0-9]*\).*/\1/p' \
             .github/workflows/build.yml)
: "${STRICTNESS:?could not read ANABASIS_PLUGINVAL_STRICTNESS from build.yml}"
scripts/run-pluginval.sh "$STRICTNESS" deterministic   # fixed nonzero seed (gate, mode A)
scripts/run-pluginval.sh "$STRICTNESS" randomise       # --randomise x3    (gate, mode B)

scripts/run-pluginval.sh                    # no argument: the SCRIPT's own default (8) — a
                                            # convenience for a quick local pass, NOT the gate
```

The strictness ladder and the current value live in `ANABASIS_PLUGINVAL_STRICTNESS` at the top of
`.github/workflows/build.yml`, which carries the phase→strictness rows; `TESTING_POLICY.md` owns
what the gate REQUIRES and deliberately restates no number. This page used to spell the ladder out
as a third copy — correct at the time of writing, which is precisely how the README's copy survived
two raises. Each mode runs **3 consecutive** passes; both modes must pass on all three platforms at
the phase strictness. Windows uses `run-pluginval.ps1`.

The randomise mode exercises state restoration under randomised test order and an unpinned,
per-run seed — defects a fixed seed reproducibly misses.

**Do not "simplify" the deterministic mode's seed to 0.** pluginval treats `--random-seed 0` as
*"generate a random seed"* (`Source/PluginTests.h`), so 0 makes the deterministic mode identical to
the randomise mode minus the shuffle. The scripts pin a nonzero constant
(`PLUGINVAL_SEED` / `$PluginvalSeed`), and the same value on all three platforms. **Nothing
enforces that the two constants stay equal** — each script's comment names the other; that is the
whole mechanism.

**Reproducing a randomise-only failure.** pluginval logs the seed it drew as
`Random seed: 0x…` at the top of every run. Take that value from the failing CI log and replay it:

```bash
.tools/pluginval --strictness-level 10 --randomise --random-seed 0x4aeacb4 \
                 --validate build/…/Anabasis.vst3 --timeout-ms 600000
```

Test *order* is shuffled per repeat, so a pinned seed reproduces the draw, not necessarily the
interleaving of a 3-pass run.

**Copy the logged value verbatim — do not uppercase it.** pluginval accepts the `0x…` form
(`CommandLine.cpp`: `if (seedString.startsWith ("0x")) return seedString.getHexValue64();`) and
round-trips it exactly — verified against 1.0.4: `--random-seed 0x4aeacb4` logs
`Random seed: 0x4aeacb4`. But the character whitelist it is checked against
(`containsOnly ("x-0123456789acbdef")`) is **case-sensitive**, so `0X4AEACB4` is rejected with
*"Invalid random seed argument!"* and exit `-1` — which, per the retry table above, both scripts
misclassify as an abnormal termination and retry three times before failing. Decimal works too
(`78248628` logs `0x4a9fab4`), and is what the scripts themselves pass.

The script downloads pluginval if absent, finds the built `Anabasis.vst3`, and runs under
`xvfb-run` when available (Linux editor tests need a display).

### Crash retry — what it is and is not

An **abnormal termination** of the validator is retried up to 3 times; a **real validation
failure** fails immediately and is never retried. The retry exists to absorb host-side validator
crashes, not plugin defects — a real plugin defect crashes deterministically and still fails after
the retries.

**The boundary is platform-specific** (`docs/policies/TESTING_POLICY.md` rule 3 is the binding
statement):

| | abnormal termination → retried | real failure → immediate |
|---|---|---|
| **Linux / macOS** | `exit ≥ 128` (128 + signal number) | `exit < 128` |
| **Windows** | Win32 exception code (`≥ 256`), negative, or no code at all | **`1…255`, including 128…255** |

pluginval's own exit code is only ever **0 or 1** (`Source/CommandLine.cpp` funnels every failure
through `exitWithError`, which returns 1; the failure count goes to the log, not the exit code), so
anything larger comes from the OS. Windows has no signals — nothing the OS reports lands in 1…255
there — so a code in that range came from pluginval and is a *real* failure, **including 128…255**,
which on Linux/macOS would read as a crash. `run-pluginval.ps1` therefore classifies differently
from `run-pluginval.sh` by design.

The one code neither script classifies correctly is a **malformed command-line argument**:
pluginval exits `-1` (255 on POSIX), which both scripts read as an abnormal termination and retry
three times before failing. Both scripts construct their own arguments, so that code means the
script itself is broken — it still fails, just noisily.

On Windows, `run-pluginval.ps1` launches pluginval via `System.Diagnostics.Process` and
`WaitForExit()` rather than the call operator: pluginval is a **GUI-subsystem** app, so `& $pv`
returns immediately with a `$null` `$LASTEXITCODE`, which both false-greens the step and (with a
retry loop) spawns concurrent background validators. The exit code is the only trustworthy signal,
and it is only trustworthy after an explicit wait.

## What cannot be verified headlessly

- **Audio quality.** Transparency, punch retention, distortion onset and tonal shift — the entire
  point of the product — need a DAW, loudness-matched comparison, and ears.
- **GUI appearance.** Layout, animation smoothness, colour rendering, HiDPI.
- **Real-host behaviour.** Automation recording, offline render, plugin rescan, session
  restoration in an actual DAW.

These are Level 5 (`TESTING_POLICY.md`) and are a **required release precondition**, not an
optional extra. A green build + pluginval pass means "ready to audition," not "shipped."
