# 2026-09-03 — documentation drift + the first raw-SARIF scanner audit

Chronological. The live register and Roadmap live in
[`docs/reports/2026-09-03-scanner-audit.html`](../docs/reports/2026-09-03-scanner-audit.html); this
file is the trail of how it was arrived at, including the two places the evidence overturned what
the task started from.

## 0. Start state

`main` at `fce94b3` (merge of PR #31), version 0.2.10. PR #31 had just added one
`actions/upload-artifact` step per scanner workflow, which is the only reason this audit was
possible at all: before it, both analyzers wrote SARIF on the runner, uploaded it to Code Scanning
and persisted it nowhere retrievable. The Code Scanning alert and check-run annotation APIs remain
403 from this environment, so **raw SARIF is the only authoritative evidence available**, and this
audit uses nothing else — no PR summaries, no check-run titles, no alert counts.

## 1. Evidence acquisition

Latest completed scanner runs on `main` at `fce94b3`:

| Analysis | Run | Artifact | Results |
|---|---|---|---|
| CodeQL C/C++ | `33798037779` #304 | `codeql-sarif-c-cpp-fce94b3…` | 50 |
| CodeQL Actions | `33798037779` #304 | `codeql-sarif-actions-fce94b3…` | 0 |
| PREfast | `33798037789` #288 | `prefast-sarif-fce94b3…` | 153 |

All three downloaded, extracted and SHA-256'd (hashes in the HTML report's Evidence section).
**203 raw results.** Both CodeQL analyses report `executionSuccessful: true`; the PREfast SARIF is
the merged `results.sarif` the `upload-sarif` step consumed, so it is byte-for-byte what Code
Scanning received.

## 2. The documentation drift — and it was wider than reported

The task carried forward one known drift: `DOCUMENTATION_COVERAGE.md` read "Last updated 0.2.8"
while 0.2.9 and 0.2.10 had shipped. Checking what those commits actually touched, rather than
assuming, changed the shape of the correction.

Both versions had in fact satisfied **most** of their obligations at the time — `SERIALIZATION_REGISTRY.md`,
`HANDOVER.md`, `CHANGELOG.md`, the affected ADRs and a per-version worklog were all updated in the
shipping commits. Exactly two rows were left unsatisfied:

1. **New/changed test.** Both added tests and neither updated `DOCUMENTATION_COVERAGE.md`.
   `TESTING.md` turned out **not** to need an edit for them: its `AnabasisStateTests` section already
   lists "corrupt/foreign-state robustness" as a coverage area, which is the bucket these fall in.
   Checked rather than assumed — the opposite conclusion would have been the easy one to write.
2. **CI workflow.** 0.2.10 rewrote `scripts/check-realtime.py`'s gate semantics — fail-closed on an
   empty input set, unreached rules now fail, the success line counts what was *proved* rather than
   what was *asked* — and left `TESTING.md`'s Realtime-enforcement section describing the old
   behaviour. That is a procedure document misdescribing a live gate, which is the failure mode the
   lifecycle policy's two-file row exists to prevent.

`README.md` is **not** drift despite the "Ship a version" row naming it: its own Project-status
section states it carries a milestone rather than the current version and points at `CMakeLists.txt`
and `HANDOVER.md`'s status row, both current at 0.2.10. Recorded because the row would otherwise
route a future contributor to an edit that should not be made.

Corrected: two dated entries in `DOCUMENTATION_COVERAGE.md` reconstructed from those commits' own
contents, and the `TESTING.md` realtime section. No unrelated history backfilled; no confidence
level upgraded.

## 3. Audit — the shape of the finding set

152 of the 203 results are first-party, and **every one of them is PREfast**. CodeQL contributed no
first-party finding in either language. Five root-cause groups:

| Group | Rule(s) | Raw | Surface | Root cause |
|---|---|---|---|---|
| G1 | `C6385` | 1 | production | bound provable only from the caller |
| G2 | `C6262` | 133 | tests | stack fixtures, summed per function |
| G3 | `C28182`+`C6011` | 9 | tests | analyzer models `dynamic_cast` as identity |
| G4 | `C28252` | 4 | tests | SAL annotation disagrees with the CRT's |
| G5 | `C26498` | 5 | tests | `const` where `constexpr` is available |

133 diagnostics collapsing to one cause is the whole reason the grouping rule matters: counted raw,
C6262 would dominate the report and bury the single production finding underneath it.

## 4. G1 — the one production finding, and why the first reading was wrong

`src/dsp/AnabasisEngine.cpp:882`:

```cpp
grMinThisCall = juce::jmin (grMinThisCall, gains[0], gains[nCh - 1]);
```

PREfast: *"the readable size is '8' bytes, but '-4' bytes may be read."* `gains` is
`float[kMaxChannels]` — 8 bytes — so `-4` is the analyzer deriving `nCh == 0` and reading
`gains[-1]`.

The first instinct is that this is a real out-of-bounds read on the audio path. It is not, and the
disproof is two functions away: `process()` returns `false` on `numChannels <= 0` before any chunk
runs, and `processChunk` has exactly one call site — inside `process()`, after that guard. So
`nCh >= 1` always, and `gains[nCh - 1]` is `gains[0]` or `gains[1]`. **False positive against the
code as it stands.** PREfast analyses each function alone and cannot carry a caller's precondition
across.

It was still worth fixing, and not for the diagnostic count. This is the only indexed read of
`gains` outside a `ch < nCh` loop — every such loop is empty-safe at zero — so the entire safety of
the line rests on a precondition established ~640 lines away in a different function, with nothing
local enforcing it. A second caller for `processChunk`, or any weakening of that guard, arms a
genuine out-of-bounds stack read in the audio path with no local signal that anything broke. The
repository already made exactly this argument once, in the comment above `nCh` itself: the
`kMaxChannels` term is documented as **not** removable-as-redundant because *it is the term that
makes the access provably in bounds*. The fix applies the same rule to the lower bound:

```cpp
const int lastCh = juce::jmax (0, nCh - 1);
grMinThisCall = juce::jmin (grMinThisCall, gains[0], gains[lastCh]);
```

No allocation, no lock, no behaviour change for any `nCh >= 1`, and no branch that survives the
optimiser. Realtime guarantees untouched; KI-009's `kMaxChannels` clamp untouched (the diff's only
mention of it is inside a comment).

## 5. G3 — a whole family that is the analyzer being wrong

Nine findings, one modelling error. The pattern is

```cpp
if (auto* b = dynamic_cast<juce::Button*> (c); b != nullptr && …) return b;
if (auto* found = findButtonByText (*c, text)) return found;
```

and the message is *"'c' contains the same NULL value as 'b' did"*. PREfast treats `dynamic_cast` as
identity, infers `b == nullptr ⟹ c == nullptr`, and flags the recursion on `*c`. A failed
`dynamic_cast` says nothing about its operand, and `Component::getChildren()` never yields a null
element. "Fixing" it would mean adding `if (c == nullptr) continue;` against a condition JUCE cannot
produce — defensive noise inserted to satisfy an unsound inference. **DO NOT FIX**, recorded so the
next audit re-confirms rather than re-triages.

## 6. G4 — the one that is genuine and is still not being fixed today

All four `C28252` are the *nothrow* `operator new` / `operator new[]` overloads in
`tests/AllocationGuard.h`, annotated `_Ret_maybenull_` where the MSVC CRT's own prior declaration of
the same operators carries something else. This is a real disagreement, not a false positive — and
the repository's annotation is arguably the semantically correct one, since nothrow `new` does
return null on failure.

It is deferred anyway, and the reason is verification rather than importance: the exact CRT
annotation cannot be read from this environment (no MSVC headers), so any fix is a hypothesis that
only a Windows CI round can confirm, and a wrong guess is churn that leaves the diagnostic standing.
Bundling that speculative loop into a production fix would make this change unreviewable. It goes to
**Next** as its own change, with G5's five `constexpr` edits folded in since the same files are open.

Worth being precise about why it matters more than "4 diagnostics" suggests: `AllocationGuard.h` is
the harness the realtime no-allocation proof runs on. Contradictory annotations degrade PREfast's
reasoning about allocation in exactly the code whose job is to observe allocation.

## 7. G2 — 133 findings, deliberately not fixed

Test functions construct `AnabasisAudioProcessor` (75,840 B) and `AnabasisEngine` (41,216 B) as
stack fixtures; MSVC's C6262 threshold is 16 KB. Distribution: min 37,592 B, median 75,776 B, max
605,988 B; ten functions over 256 KB, one over 512 KB.

Risk of leaving: bounded and self-announcing. Tests only, never shipped; the Windows default
main-thread stack is 1 MB and the worst function sits at 606 KB with the Windows lane green; the
Linux sanitizer lane already raises `ulimit -s 65536` for ASan's redzone inflation. Cost of fixing:
heap-converting 133 sites across two large, densely-commented suites — wide churn, real regression
risk, zero behavioural gain. The repository already applies the targeted remedy where it mattered:
`testGrHistoryReaderStaysInsideTheRingAndSeesEveryReset` heap-allocates its fixtures explicitly
"for MSVC PREfast's summed-locals C6262".

**Monitor**, with a trigger rather than a schedule: any single function past ~768 KB, or a Windows
stack overflow in CI, and that function alone gets converted.

## 8. Third-party

51 results (CodeQL's 50 plus one PREfast `C26495`) are in the FetchContent'd JUCE tree. Verified
rather than assumed: across primary locations, related locations **and** code flows, CodeQL's 50
results reference 25 files over 72 locations and **none** lies outside `build/`. Meanwhile the same
run extracted 2,071 artifacts including 42 first-party `src/` and `tests/` files — so first-party
code was genuinely analysed and produced nothing, which is what makes "0 first-party CodeQL
findings" a result rather than an absence.

Not modified (`DEPENDENCY_POLICY.md`), and excluded from every first-party count. Note the raw SARIF
is strictly richer than the dashboard here: `paths-ignore: build` filters these out of the *alerts*,
not out of the SARIF.

## 9. Verification

Local, on this commit, after the G1 fix:

| Check | Result |
|---|---|
| `scripts/build.sh` | exit 0 |
| `scripts/run-tests.sh` | exit 0 — **1345 checks, 0 failures** (`AnabasisTests` 316 + `AnabasisStateTests` 1029) |
| `check-docs` | 109 file(s) clean |
| `check-citations` | 54 anchors unchanged against `origin/main` |
| `check-portability` | 48 files, 0 violations |
| `check-realtime` | 40 files, 0 violations, 1 of 1 ordering requirement verified |
| Gate self-tests | all six pass — docs 67, citations 37, portability 120, realtime 145, linux-abi 19, clang-warnings 18 |
| `git diff --check` | clean |
| Diff review | the entire production change is **+2 / -1 lines**; no unrelated modification |

1345 matches `HANDOVER.md`'s recorded figure exactly, which is the non-regression statement: the fix
changes no behaviour, and the suites agree.

**Scanner confirmation — G1 is closed on evidence.** PREfast run `33801797950` on `a7112c4`,
diffed against the audited run at `fce94b3` over the same analyzer:

| | before | after |
|---|---|---|
| total results | 153 | **152** |
| resolved | — | exactly one: `C6385 src/dsp/AnabasisEngine.cpp:882` |
| newly introduced | — | **none** |
| `src/` surface | 1 | **0** |

The delta is exactly the finding and nothing else, which is the statement worth having: the fix
landed, and it did not trade one diagnostic for another. This is also the only way G1 could be
confirmed — it is a local-provability change, so the suites cannot see it, there having been no
reachable defect for them to catch.

**Not run here:** macOS, Windows, MSVC `/analyze` locally, CodeQL locally, pluginval, ASan/UBSan —
all CI lanes.

## 10. Unresolved

No blocked decisions. Nothing in this audit needs an owner, architecture or compatibility gate: G1
changes no behaviour and touches no gated surface, G2/G3/G5 are test-only, G4 is deferred for
verification logistics, T1/T2 are settled by policy.

Standing limitation, restated because it bounds every claim here: the Code Scanning **alert-store**
state (open / fixed / dismissed) is unavailable from this environment. This audit is built entirely
from raw SARIF and asserts nothing about it.

---

# Round 2 (2026-09-03, later) — PR review closure + the deferred G4/G5

Starting state: `main` at `1c16654` (PR #32 merged). Round 1 closed as recorded above — drift
corrected, artifact persistence in place, G1 fixed and scanner-confirmed, CodeQL first-party surface
clean, G2/G3/T1/T2 accepted. Nothing below reopens any of them; no new evidence was found that would
justify it.

Two new review findings arrived against `src/PluginProcessor.cpp`, plus the G4/G5 Roadmap items this
round was always going to pick up.

## R1. Review finding #1 — "Adopted snapshots can miss first save" (`PluginProcessor.cpp:1199`)

> *If adoption finishes after `programMailbox.take()` but before generation loads, the save selects
> its old snapshot. It combines current sound with stale metadata.*

**Disproved. No code changed.**

The named machinery does not exist. `programMailbox`, `applyResolved`, `Mailbox` and `take()` have
**zero occurrences** anywhere in `src/` or `tests/`. Line 1199 is inside a comment block, not code.
The plugin has no program state machine at all to hold a mailbox: `getNumPrograms()` returns 1,
`getCurrentProgram()` returns 0, and `setCurrentProgram(int)` is an empty override.

Names being wrong is not by itself a disproof, so the analogous mechanism was traced. There **is** a
staged-vs-published selection in the save path, and it is the only one: `getStateInformation`'s
ADAPTIVE block picks between the staged mirrors and the engine's published values on
`engine.adaptiveRestorePending()`. That is exactly the shape the finding describes, so it is the
thing that had to be proved sound.

It is sound, for two independent reasons.

1. **The two actors are ordered.** The stager (`setStateInformation`) writes `stagedRefOnset`,
   `stagedRefTilt`, `stagedAdaptiveLearned` and *then* calls `engine.restoreLearnedTargets(...)`,
   which does `adaptivePending.store(true, release)`. The reader takes
   `adaptivePending.load(acquire)` before reading the mirrors. The release/acquire pair publishes the
   three stores ahead of the flag, so a reader that sees the flag up sees the values that set it —
   on the message thread, where both live, and equally from any other thread a host might call
   `getStateInformation` on.
2. **The mirrors are never consumed.** The audio thread's
   `adaptivePending.exchange(false, acquire)` clears only the *flag*. The three staged values are
   written by the staging site alone and persist until the next `setStateInformation`. So the
   interleaving the finding describes — the flag flipping between the two reads — resolves either
   way to the same numbers: if `restoreStaged` reads true the mirrors hold the loaded truth, and if
   it reads false the engine has already applied *those same values* and its published pair is that
   truth. Sound and metadata cannot come from different generations because there is only ever one
   generation in flight.

Worth recording that the repository already reasoned about this hazard class: the comment above the
staging site warns that a stager which sets the flag without pairing the stores would make
`getStateInformation` "serialize the stale one", and instructs future stagers to route through that
site or pair the stores in a helper. The invariant the finding worries about is not accidental — it
is written down and enforced at the one place that can break it.

## R2. Review finding #2 — "Newer restores can lose synchronous settings" (`PluginProcessor.cpp:1069`)

> *When a second restore overlaps adoption of the first, `applyResolved` can overwrite the newer
> oversampling atomic. Activation uses the previous setting until another adoption.*

**Disproved. No code changed.**

Again the named machinery is absent — no `applyResolved`, and 1069 is a comment line. And again the
analogue exists and had to be checked: there **is** an oversampling atomic,
`InternalState::osMirror`, read by the audio thread through `oversampleFactor()`.

The finding requires a stale value to be *in flight* — something captured earlier and written later.
There is none, by construction. `osMirror` has exactly two writer paths and both are `syncAtomics()`
(the constructor, and `valueTreePropertyChanged` via the listener). `syncAtomics()` takes its value
from `tree.getProperty(iid::oversample)` **at the instant it runs**; it carries no argument and no
captured payload. A late, "older" call therefore cannot install an older value — it re-reads the
current tree and installs the current truth. The mirror is a derived cache, not a delivered message,
and a derived cache has no stale generation to lose.

The ordering question the finding raises does not arise either: `setStateInformation` and
`InternalState::replaceFrom` are synchronous message-thread code, and `ValueTree::setProperty`
dispatches the listener synchronously on the same thread, so restores are serialised rather than
overlapped. "Newer restore wins" holds because the newer restore is the one that ran last, and the
mirror reflects whatever the tree holds when it did.

**Neither finding required a compatibility, serialization or architecture gate**, since neither
produced a change.

## R3. G4 — `C28252 ×4`, and the diagnostic named its own fix

The four findings correlate exactly with the four `ANABASIS_GUARD_RET_MAYBENULL` uses; the four
`ANABASIS_GUARD_RET_NOTNULL` uses are clean. That one-to-one split is the evidence that located the
fault in the macro rather than in any individual overload.

The message says the prior instance has `SAL_success(return!=0)`. MSVC's `<vcruntime_new.h>` declares
the nothrow operators `_Ret_maybenull_ _Success_(return != NULL) _Post_writable_byte_size_(_Size)`,
and declares the throwing forms with no `_Success_` at all — a throwing `new` returns or throws, it
never fails by returning. The repository's MAYBENULL macro carried `_Ret_maybenull_` and
`_Post_writable_byte_size_` but **not** `_Success_`, so its definitions contradicted the prior
declaration; the NOTNULL macro had nothing to contradict, which is precisely why only four of the
eight fired.

Fix: add `_Success_(return != 0)` to the MAYBENULL macro. This aligns the annotation with the CRT
contract rather than removing one. The tempting alternative — dropping `_Ret_maybenull_` — would have
been the wrong repair: nothrow `new` genuinely can return null, and modelling that is the one thing
this guard exists for.

Verification of this fix is **necessarily remote**, and the levels are worth keeping apart because
only the last one is evidence for the claim:

* **Local** — the change compiles and the suites stay green, but nothing local can see it. `_Success_`
  is SAL, and the macros expand to nothing off MSVC, so on this machine the edit is a no-op by
  construction.
* **Windows CI execution** — the `msvc.yml` lane builds and runs `/analyze` on Windows. That the job
  goes green says the annotation is well-formed, not that C28252 stopped firing.
* **Windows PREfast scanner confirmation** — the raw SARIF for the pushed commit showing the four
  findings gone. This is the only level that settles it, because the CRT headers cannot be read from
  this environment: the `_Success_` clause was DERIVED from the diagnostic text plus the
  four-MAYBENULL / four-NOTNULL correlation, so it is a hypothesis until the analyzer answers.
* **Remaining limitation** — even confirmation is confirmation on ONE toolset. It says this MSVC's
  CRT declares the nothrow operators the way the fix assumes; a future toolset that changed that
  declaration would re-open the diagnostic, which is why the reasoning is recorded here rather than
  just the edit.

At the time this section was written the run had not happened, so nothing above was yet established.
**The result is in §R5**, which reports it.

## R4. G5 — `C26498 ×5`

Bundled into the same change, as the round-1 Roadmap said it should be: `tests/` was already open and
the Windows verification loop was already running. Five locals initialised from `constexpr` members
(`max()`, `quiet_NaN()`, `infinity()`) changed `const` → `constexpr`. No behavioural or codegen
consequence; no other test touched.

## R5. Verification

Local, after G4/G5: build exit 0; **1345 checks green** (316 + 1029); `check-docs` 109;
`check-citations` 54 anchors; `check-portability` 48/0; `check-realtime` 40 files, 1 of 1 verified;
all six gate self-tests; `git diff --check` clean. Six functional lines changed, no production code.

**Windows PREfast — G4 and G5 confirmed, first iteration.** Run `33807395028` on `b8bace4`, diffed
against the round-1 end state at `a7112c4`:

| | before | after |
|---|---|---|
| total results | 152 | **143** |
| `C28252` | 4 | **0** |
| `C26498` | 5 | **0** |
| newly introduced | — | **none** |
| `src/` surface | 0 | **0** |

The delta is exactly the nine fixed findings and nothing else. No iteration loop was needed: the
`_Success_` hypothesis was derived from the diagnostic text plus the exact four-and-four correlation,
and it held on the first Windows run.

## R6. Closing state

143 PREfast results remain, and **every one carries a standing disposition**: `C6262` ×133 (G2,
accepted, monitored on a trigger), `C28182` ×8 + `C6011` ×1 (G3, analyzer modelling error), `C26495`
×1 (T2, third-party JUCE). **Zero actionable first-party findings remain**, and the production
surface is clean.

Nothing was left unresolved. The two review findings are disproved with source-level evidence rather
than deferred; G4 and G5 are scanner-confirmed rather than argued.


---

# Round 3 (2026-09-03) — audit-record consistency closure

No source, test, scanner or CI change. `docs/` and `worklogs/` only.

## C1. The coverage record was stale for round 2

The audit obligation applies to *every* documentation-affecting change, and round 2 changed two audit
records without touching `DOCUMENTATION_COVERAGE.md`. Added one entry for round 2 and one for this
round. Nothing else backfilled, no historical obligation invented.

The round-2 entry deliberately does **not** claim the **New/changed test** row. `AllocationGuard.h`
and `dsp_tests.cpp` were edited, but no test was added, removed, or had its behaviour changed — a SAL
annotation is analysis-only, and `const` → `constexpr` on locals initialised from `constexpr` members
changes neither semantics nor codegen. The suites reporting the same 1345 checks either side is the
evidence for that rather than an assertion of it.

## C2. "Windows-CI-verified" was four claims compressed into one

§R3 asserted G4 "Windows-CI-verified" in a narrative section written *before* the run, while the
report at that moment still said pending. The compression was the defect: that phrase ran together
local verification (a no-op off MSVC by construction), Windows CI execution (a green job proves the
annotation is well-formed, not that the diagnostic stopped firing), PREfast scanner confirmation (the
raw SARIF — the only level that settles it, the `_Success_` clause having been *derived* rather than
looked up), and the standing limitation that confirmation is confirmation on **one** toolset. §R3 now
separates them and defers the result to §R5. The report needed no change on this point; `bdd75b2` had
already closed its side.

## C3. The consistency sweep, and what it caught in my own work

Seven independent lenses were run across the report, the worklog, `DOCUMENTATION_COVERAGE.md` and
`HANDOVER.md` — statuses, counts, provenance, pending-vs-confirmed, roadmap, remaining work, handover.
Counts and provenance came back clean. The other five converged, independently and repeatedly, on
**four self-contradictions inside the HTML report** — all of them mine, all introduced by updating one
part of the document when G4/G5 closed and not the others:

1. **"Open decisions": *"G4 is deferred for verification logistics, not for a decision."*** Present
   tense, in a file that declares itself the live state, three sections after the same file records
   G4 as `CLOSED — Windows-PREfast confirmed`. Flagged by four lenses.
2. **Roadmap preamble: *"the one open verification claim rises to the top."*** True when written;
   false once the verification closed and G2 took the top slot. The item that now leads says in its
   own body that *everything actionable is done*.
3. **G4 "Cost / complexity" row** still named *"drop the annotation on the nothrow overloads"* as the
   likely fix — the approach the Fix row four rows below explicitly rejects — and described
   verification as an unconfirmed hypothesis awaiting CI.
4. **R1/R2 filed in two Roadmap buckets at once**, Monitor *and* Closed, so the Roadmap did not say
   which state it assigned them.

All four corrected. The lesson is worth writing down: a live document updated in place accumulates
exactly this class of defect, because the parts that need changing are the ones that do not mention
the thing that changed. A sweep that asks "what does this document now contradict?" finds them;
re-reading the parts you edited does not.

## C4. Drift found in `HANDOVER.md` — reported and corrected

The handover lens found the Test Status row claiming **"Five checkers now ship `--self-test`"** while
*listing six*, with two stale figures: `check-realtime 90` and `check-clang-warnings 15`. Neither is
this round's doing — 0.2.10 grew the realtime self-test by the four `scan_repo` cases against real
temporary trees, and `CI_CD.md` already records the clang-warnings gate going 15 → 18 — so the row
had been contradicting the tree for two versions.

Corrected from a measured run of all six on this commit: docs 67, citations 37, portability 120,
realtime 145, linux-abi 19, clang-warnings 18. Recorded here and in the final report rather than
changed quietly, which is what C6 asks: drift is to be reported, and the objection is to *silent*
correction, not to correction.

## C5. Verification

`check-docs` 109 clean · `check-citations` 54 anchors · `check-portability` 48/0 · `check-realtime`
40 files, 1 of 1 verified · all six gate self-tests · HTML well-formed · `git diff --check` clean ·
diff is `docs/` and `worklogs/` only.

## C6. Closing state

Every completed disposition stands unchanged — G1, G4, G5 fixed and scanner-confirmed; G2 monitored;
G3 and the two review findings recorded disproofs; T1/T2 third-party; CodeQL clean on both languages.
**Zero actionable first-party findings remain**, and the records now agree with each other on that.

## C7. The sweep also caught two residues in this round's own output

Worth recording, because they are the same defect class as §C3 and they survived a first pass. The
synthesis pass re-read the four files after the corrections and found (a) the worklog claiming the
`HANDOVER.md` drift was recorded "here **and in the final report**" while the report carried no
round-3 record at all, and (b) this round's `DOCUMENTATION_COVERAGE.md` entry still describing the
round as *one wording fix* after it had grown to six, and not listing `HANDOVER.md` among the records
it engaged. Both fixed: the report now carries the round-3 closure section, and the coverage entry
matches the round it describes.

The pattern is exact. Each was written when it was true, then the round grew past it. That is the
whole lesson of §C3 recurring inside the fix for §C3 — which is the argument for sweeping a document
set against itself at the END of a round rather than trusting that each edit was complete when made.
