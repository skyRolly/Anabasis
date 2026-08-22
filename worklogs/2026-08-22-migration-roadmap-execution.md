# Worklog — 0.2.0: executing the round-2 migration roadmap (2026-08-22)

Session-local evidence trail for version 0.2.0. Raw investigation material, NOT architecture
documentation — `docs/SOURCE_OF_TRUTH.md`: worklogs are never cited as policy. What is binding is
`CHANGELOG.md`, the three ADRs this round accepted, the code and the tests; this file carries the
measurements, the problems found on the way, and the decisions that are easier to re-argue than to
reconstruct.

Input: `worklogs/2026-08-21-anamorph-migration-audit-round-2.md` and its HTML report — 34 items,
`A2-01` … `A2-34`, in six phases. The owner's directive was to execute the roadmap in one pass, to
implement each item properly rather than as a placeholder, to keep the two products aligned on
C++ standard / toolchain / dependency versions unless there is an Anabasis-specific reason not to,
and to preserve the items the audit marked CANNOT or NOT NEEDED.

**Everything the roadmap marked MIGRATE was implemented. Nothing marked CANNOT or NOT NEEDED was
touched. The three INVESTIGATE items were measured and answered, and none of them was quietly
turned into a port.**

## Phase 1 — make the existing gates trustworthy

| Item | What landed |
|---|---|
| A2-04 | `check-docs.py`: skip filtering relative to the SCAN ROOT, `_deps` in `SKIP_DIRS`, and an empty scan is no longer a pass |
| A2-05 | `check-portability.py`: raw strings, prefixed character literals, line splices inside literals, and the length invariant on an unterminated literal and an unterminated block comment |
| A2-06 | `--self-test` for `check-portability.py` (120 cases) and `check-citations.py` (37 cases), both wired into `source-lint` ahead of the check they vouch for |
| A2-11 | `release.yml`: the three CommonMark fence-closer clauses |
| A2-12 | `run-pluginval.sh`: the crash retry scoped to Linux |
| A2-16 | `scripts/preflight.sh` |
| A2-18 | `TESTING_POLICY.md` rule 5 — a checker must prove it is live |

### A2-04 was a live false green, and the mutants say so

The audit called this out and implementing it confirmed the shape. `markdown_files` tested
`SKIP_DIRS.isdisjoint(path.parts)` on the ABSOLUTE path, and `main()` reported `0 file(s) clean`
with exit 0. Two mutation runs, each killing a **disjoint** set of the new self-test cases:

| Mutant | Killed |
|---|---|
| skip filtering restored to the absolute path | 7 of 67 — every "a checkout under `<name>`" case |
| the empty-scan guard deleted | exactly 1 — "an empty scan set reported a clean run" |

The second is the backstop for the next way of emptying the set, which is why it is a separate
assertion rather than a consequence of the first.

### A2-05: four parser rules, four disjoint mutants

The scanner came across whole from the sibling (it is product-agnostic) and the self-test with it.
Each rule is pinned:

| Mutant | Killed |
|---|---|
| raw strings unrecognised | 7 of 120 |
| the single-character digit-separator test (so `L'a'` blinds the scanner) | 6 |
| line splices inside a literal not preserved | 10 |
| the unterminated-literal length invariant dropped | 4 |

**Exposure today is nil and that is stated rather than hidden**: no raw string and no prefixed
character literal exists in `src/`, `tests/` or `tools/`. The reason to take it anyway is the
failure DIRECTION — every one of these makes the lint go quiet.

### A2-06: the citation self-test could not be copied

The sibling's is 686 lines and tests features this repository's citation gate does not have —
symbol glosses, a template-span parser, a re-aim declaration lifecycle, `--fix` invalidation
reporting. The audit had already ruled that expansion out of scope. So the self-test was WRITTEN
for this gate: 37 cases over recognition (the compound form, the qualifier capture, the lookbehind),
ownership (`classify` against `TRACKED`, including the sibling's `src/PluginEditor.cpp` spelling
which is deliberately not ours), the provenance-block exclusion, the line-map arithmetic, the
right-to-left span rewriting with its dedupe and overlap guard, and the declared-re-aim lifecycle.

**One structural change was needed to make the arithmetic testable at all.** `build_line_map`
combined a `git diff` subprocess with the offset computation, so the only way to exercise the
computation was to construct a git history. `line_map_from_diff(diff)` is now a pure function and
`build_line_map` is the thin wrapper that feeds it — the same rule the GUI headers state about
expressions reachable only from `paint`.

Two self-test cases were WRONG when first written and the checker was right: an absolute path
produces no match at all (the lookbehind blocks every restart), and a near-miss path like
`xsrc/PluginProcessor.cpp` IS matched but is declined by `classify`. Both were rewritten to assert
what actually holds, which is a more useful pair of cases than the ones intended.

### A2-11 was demonstrated, not assumed

The old tracker recorded a fence opener as `substr($0,1,3)` — never longer than three characters —
and closed on a bare prefix test. Two fixtures were built and run through both forms:

- **Clause 2** (a closer must be at least as long as the opener): a four-backtick block containing a
  three-backtick fence. Old form truncates the entry at the sample heading; new form publishes the
  whole entry.
- **Clause 3** (a line carrying an info string is an opening fence and can never be a closer): a
  three-backtick block opened with the `markdown` info string, containing a three-backtick line
  opened with `cpp`. Same outcome.

And the regression direction: **every existing entry extracts byte-identically** under both forms
(0.1.1 through 0.1.6, md5 compared). The change is for the first entry that adds a nested sample.

**The terminator was deliberately NOT migrated.** The sibling ends on `^## \[`; this repository ends
on `^## `, which cannot leak a non-entry section into a release's notes and needs no separate
documentation contract to hold.

## Phase 2 — realtime enforcement, the portable tiers

### A2-01: the audio path allocates nothing, and that is now measured

`tests/AllocationGuard.h` came across and was adapted. `testTheAudioPathAllocatesNothing` arms it
around `AnabasisEngine::process` over 2 channel counts × 5 oversample factors × 2 phase modes × 4
parameter sets (defaults / pushed / bypass / delta), 24 blocks each, plus a separate case that
drives a **mid-stream oversample rewire with no re-prepare** — the shape a user gets by turning
oversampling up while audio runs, and the one most likely to allocate.

**Result: 2,040 armed `process()` calls across 80 configurations, 0 allocations, both counters
proved live in the same run.**

The header's measured claims were re-taken on this project rather than carried over. `prepare
(48000, 256, 2)` allocates **new=205, malloc=1313** here — the split is what proves the two counters
are two routes rather than one counted twice, and under ASan (where the malloc half compiles out)
the same call prints `new=205 malloc=0`, which is what a genuinely separate route looks like.

**Three problems surfaced and each needed a decision.**

1. **Clang emitted `-Wmissing-prototypes` on four deallocators.** `<new>` declares the SIZED forms
   only when `__cpp_sized_deallocation` is on, and whether it is on by default is a property of the
   compiler version. This repository's Clang gate fails on ANY first-party warning and deliberately
   has no baseline file to absorb it, so the four were FIXED with explicit declarations rather than
   tolerated. Measured: clang 18 emits four; with the declarations, none.
2. **GCC then emitted `-Wredundant-decls`** on exactly those declarations, because libstdc++ has the
   sized forms whatever the flags say. The declarations were narrowed to the four sized forms only
   (the other four are always declared) and the GCC pragma covers the redundancy.
3. **GCC's `-Wmismatched-new-delete` fires on the deallocators and is wrong by construction** — it
   attributes the block to the replaced `operator new` and does not follow through to `rawAlloc`,
   which is `__libc_malloc`. The sibling excluded it from its GCC warning gate; this repository has
   no GCC gate, so an un-suppressed false positive is 13 lines of noise in the shipped Linux build
   log on every push. It is suppressed by a pragma **scoped to the six deallocators** with the
   reasoning attached, so a genuine mismatch anywhere else still reports.

**valgrind had to be handled, and the interaction is real rather than theoretical.** Measured here:
with the guard compiled in, memcheck reports `Mismatched free() / delete / delete []` repeatedly
against the real JUCE-linked suite under the pipeline's exact invocation and `--error-exitcode=1`
fails the step. With `-DANABASIS_NO_ALLOC_GUARD` on that job's build: **0 errors from 0 contexts**,
and the suite discloses the five skipped assertions rather than passing them vacuously (296 checks
instead of 301).

### A2-02: the static lint, scoped to this product's audio path

`scripts/check-realtime.py` came across; the **scope regex was re-derived** by enumerating
definitions in `src/` rather than copied. This chain uses five `process*` spellings, three meter and
adaptive taps, and six `reset*` names. Matching is by EXACT name, which is what keeps
`PresetManager::resetSlotFieldsToDefaults` and `MacroEngine::resetToMacro` — message-thread
functions whose names begin with `reset` — out of the findings.

**Coverage census: 35 audio-path bodies across 15 files.** The lint was proved to reach all three
levels by seeding a violation into real code: `new float[8]` in `AnabasisEngine::process`, a
`vector::resize` in `AnabasisAudioProcessor::processBlock`, and one in `LookaheadLimiter::reset` —
each reported, and the tree clean without them.

The docstring's coverage claim was re-measured for this tree with gcov: **the DSP suite runs 73.7 %
of the 3,156 lines and 63.4 % of the 1,190 branches in `src/dsp`**. That is materially LOWER than
the sibling's figure, which makes the lint's argument stronger here, not weaker: better than a third
of the branches are invisible to both runtime tiers.

## Phase 3 — toolchain determinism, then the sanitizer lane

Order was A2-09 → C++23 → A2-08 → A2-03, and the order is the point: the review-gate amendment
exists before the change it governs, and the Clang pin exists before the lane that needs its
runtime.

### C++23 (ADR-0030) — taken on the owner's directive, against the audit's own verdict

The audit had marked this NOT NEEDED (`A2-25`) because the `ANABASIS_CXX_STANDARD` seam and the
weekly canary already discharged the ADR-0008/OQ-006 mandate. **The owner's directive overrides
that**, in as many words: *"Move Anabasis to C++23 where required by the migration plan… Do not keep
older versions simply because they already work."* Verified before applying, not after: both suites
build and pass at 23 under GCC 13.3 and Clang 18.1.3, and later under the pinned Clang 22.1.8.

The seam and the canary workflow went with it. An `option()` whose only caller is deleted is dead
state, and this repository removes those. `-DCMAKE_CXX_STANDARD=20` now fails to take effect rather
than silently configuring — deliberate, because the unconditional `set()` shadows a same-named cache
entry, so a command-line override was never a working escape hatch.

### A2-08 (ADR-0031) — the pin, and the open decision it closed

The audit left one decision open: keep the zero-first-party-warning gate and fix whatever the pinned
major surfaces, or adopt the sibling's baseline mechanism. **The measurement closed it in favour of
the strict contract.** `scripts/setup-llvm-apt.sh 22` was run on this machine (it works: clang-22.1.8,
lld-22, libclang-rt-22-dev, signing key identity assertion passed), and the full Clang leg — both
suites, the probe, the engine reproduction and the VST3 — built at C++23 with **no first-party
warnings** (2 diagnostics, both in vendored paths, not gated). No baseline file was introduced and
none is needed.

### A2-03 (ADR-0029) — the RTSan lane, exercised rather than described

Because clang-22 could be installed here, the whole lane was run locally rather than written and
hoped for:

- **The annotation changes no code.** `AnabasisEngine.cpp` compiled by clang-22 at `-O3` with the
  project's exact flags, once with `[[clang::nonblocking]]` live and once with the macro forced
  empty, produces a **byte-identical object**: 105,224 bytes, MD5 `3cdcf88c10825bc8c3a130def259210f`.
  That is what permits an annotation on a `DSP_POLICY`-frozen path.
- **The DSP suite runs violation-free under `-fsanitize=realtime`**: 296 checks, 0 failures, exit 0.
- **The liveness canary aborts** with `ERROR: RealtimeSanitizer: unsafe-library-call … Intercepted
  call to real-time unsafe function 'malloc' in real-time context!` at exit **43**.
- **The guard's stand-down is cross-checked from outside the compiler.** With
  `-DANABASIS_RTSAN_LANE=1` and no `-fsanitize=realtime` the build fails with the `#error`; with
  both, it compiles. Both directions run.
- **The `-Wfunction-effects` tier is live.** `tests/realtime_effects.cpp` compiles clean over this
  product's genuinely JUCE-free leaf layer — `CeilingClamp.h`, `ScopeBuffer.h`, `Latency.h`,
  `EngineParameters.h`, identified by inspection rather than copied from the sibling's list — and
  fails with the expected diagnostic under `-DANABASIS_EFFECTS_CANARY`.

## Phase 4 — the artifact's properties become measurements

### A2-07: the Linux ABI floor, measured here and NOT copied

`scripts/check-linux-abi.py` came across; **the floor constants did not**. Measured on this
repository's own shipped VST3 and Standalone: **GLIBC 2.38, GLIBCXX 3.4.31, CXXABI 1.3.9** — both
binaries identical. The sibling declares `CXXABI_1.3.14` because that is what ITS artifact needs;
adopting that number would have left five ABI versions of headroom in which a real raise passes
unnoticed. The self-test fixtures were re-aligned to the declared floor for the same reason.

`COMPATIBILITY_MATRIX.md` previously said no Linux OS floor "has been decided or measured", which
was accurate and was the problem. It now defers to the script and quotes no number of its own.

### A2-10: the decision the workflow had been deferring

`build.yml` named this change in its own comment — *"Restoring full macOS symbolication needs the LTO
object persisted via `-Wl,-object_path_lto` — a linker-flag change that is a deliberate future
decision, not a silent addition."* It is taken, as ADR-0029's round rather than slipped in beside
the packaging code, with the per-slice reasoning that makes it correct (a directory, not a file, and
created at configure time so a missing path fails the link).

Two assertions turn the best-effort capture into a contract, and both run LAST so a debug-capture
finding never withholds customer artifacts whose behavioural gates passed.

### A2-17: the check found something on its first run

`--assert-discriminating` compares every configuration pair within a (rate, block size) group and
refuses to print a baseline over a collapsed scenario set. **It immediately found that
`field: mix 100, links 0` and `field: mix 100, links 100` produce IDENTICAL output to nine decimal
places, at every rate and block size.**

That is a property of the STIMULUS, not a defect: the probe drives 220 Hz left and 330 Hz right at
the same amplitude, so both detectors see the same level and a linked gain computer computes what an
unlinked one computes. The link axis has no purchase on equal-level material.

**It is declared rather than engineered away**, and the reason is a genuine conflict of purposes:
making the channels differ in LEVEL would give the link axis something to bite on and would trip this
probe's primary oracle, the `> 6 dB apart` skew test that detects a half-lost channel (KI-009). The
two purposes want opposite stimuli. So the collapse is recorded where a reader of the evidence will
see it — including the reader of ADR-0028's "33 configurations agree digit for digit", which is
really 32 distinct experiments per rate/block plus one run twice.

Measured: **112 configuration pairs compared, 4 collapsed, 0 undeclared.** Mutation-verified by
making a third configuration identical to another: the undeclared collapse is reported by name and
the probe refuses.

## Phase 5 — CI economics and supply chain

- **A2-14**: `.github/actions/setup-linux-build` replaces the setup block in four jobs, with the
  ccache fallback policy written ONCE; per-job cache lineage stays in the workflow because that is
  the part that genuinely differs. The macOS jobs are deliberately **not** cached: `dsymutil` walks
  the debug map back to the object files, which is the one place "same bytes, different provenance"
  could be observable, and A2-10 had just made that contract load-bearing.
- **A2-13**: `merge-check` builds `refs/pull/N/merge` — the tree the merge button produces, which no
  push build ever compiles because every other job is skipped on a same-repo PR. Build and self-tests
  only; it shares the `linux` job's GCC cache lineage, which is what keeps it nearly free.
- **A2-15**: every action ref SHA-pinned with a version comment; Dependabot split by semver impact.
  This **reverses a decision `DEPENDENCY_POLICY.md` recorded**, so that section was rewritten with
  the argument that changed it — JUCE is SHA-pinned so it cannot move under a re-pointed tag, and
  JUCE never sees a token while every action runs with the job's credentials — rather than left
  contradicting the workflows.

## Phase 6 — the three investigations, and what they measured

**A2-32, the LTO test lane: ADOPT, but not in this round.** Built both suites with `clang-22 -flto`
and ran them: **301 + 873, 0 failures**, identical to the non-LTO build. So it finds nothing today
and its value is preventive — it is the configuration INC-004 required to manifest, and the existing
coverage of shipped-class codegen is the channel probe's oracle (channel presence and per-channel
RMS over 33 configurations), not the 1,174 assertions. Cost, measured: rebuilding the two suites
takes **172 s under LTO against 27 s without**, and the binary goes from 14.07 MB to 5.44 MB, so LTO
is doing real work rather than being nominal. Recommended for a following round, with the numbers.

**A2-33, libFuzzer over `setStateInformation`: DECLINE, on measurement.** Ran gcov over
`AnabasisStateTests` and read per-function coverage: **`setStateInformation` is at 100 % of its 74
lines**, `getStateInformation` 100 % of 27, `applySlotToLive` 97.5 % of 40. `PluginProcessor.cpp`
overall is 94.6 % of lines / 55.2 % of branches and `PresetManager.cpp` 91.2 / 63.4. A fuzzer would
therefore add input DIVERSITY over branch combinations rather than reach, on top of a suite that
already carries explicit corrupt-state and legacy-migration tests. Re-evaluate if a state-parsing
defect is ever reported from the field, or if that function's line coverage falls.

**A2-34, `setup-linux.sh` dependency profiles: NOT NEEDED, now for a measured reason.** The profile
split exists to serve a containerised job. A2-32's verdict does not require a container — the lane
would be an ordinary runner job with the pinned Clang and `-flto` — so there is nothing for the split
to serve. The composite action was written without the input for that reason.

## Validation

Linux x86-64, JUCE 9.0.1 `e18f7f5…`, C++23.

| Gate | Result |
|---|---|
| `scripts/preflight.sh` | **rc=0** end to end |
| `AnabasisTests` / `AnabasisStateTests` (GCC 13.3, Release) | **301 + 873 = 1174 checks, 0 failures** |
| Same, Clang 18.1.3 | 301 + 873, 0 failures |
| Same, pinned Clang 22.1.8 | 301 + 873, 0 failures; **no first-party warnings** across the whole Clang leg |
| Same, `clang-22 -flto` | 301 + 873, 0 failures (A2-32 investigation) |
| ASan + UBSan (clang) | clean; guard's malloc half compiled out and DISCLOSED (300 checks) |
| valgrind memcheck (`-DANABASIS_NO_ALLOC_GUARD`) | **0 errors from 0 contexts** |
| RealtimeSanitizer (clang-22) | **violation-free**, exit 0 (296 checks, guard stood down and disclosed) |
| RTSan liveness canary | aborts with the RTSan report, exit **43** |
| `-Wfunction-effects` leaf layer | clean; canary fails with the expected diagnostic |
| pluginval, strictness from `build.yml`, editor under xvfb | deterministic ×3 **and** randomise ×3, PASSED |
| `AnabasisChannelProbe --assert-discriminating` | 112 pairs, 4 declared collapses, 0 undeclared; every configuration kept both channels |
| `check-linux-abi.py` | within the declared floor; self-test 19 cases |
| check-docs / check-portability / check-realtime / check-citations / check-clang-warnings self-tests | 67 / 120 / 90 / 37 / 15 cases |
| `check-citations --check` | 50 anchors unmoved against `origin/main` |

## What this round did NOT do

- **No DSP algorithm changed.** The only edit to `src/dsp/AnabasisEngine.cpp` is a type attribute on
  one declaration, proved to produce a byte-identical object.
- **No parameter added, renamed or removed**; no range, default, choice ordering or automation flag
  moved; `tests/fixtures/parameter_registry.snapshot` is unchanged and deliberately not re-frozen.
- **No serialization schema, threading-model or reported-latency change.**
- **Nothing the audit marked CANNOT was migrated** — the hover-occlusion term, the `microLit` idle
  latch, the `stepVal` landing repair, the warning baseline FILES, the Linux installer hardening
  (which flowed the other way) and the widened CHANGELOG heading pattern are all untouched, and the
  reasons stand as the audit recorded them.
- **Nothing the audit marked NOT NEEDED was migrated except C++23**, which the owner's directive
  re-took explicitly and which carries ADR-0030 as its record.
- **No INVESTIGATE item was implemented.** All three produced verdicts with measurements.
