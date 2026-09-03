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
