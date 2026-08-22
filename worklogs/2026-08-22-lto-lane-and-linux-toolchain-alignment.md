# 2026-08-22 — the LTO validation lane, and the Linux release toolchain aligned with the sibling

Round A3. Two decisions, both **re-opened** against verdicts the 0.2.0 audit had already taken:

| Item | 0.2.0 verdict | This round |
|---|---|---|
| **A2-32** `linux-lto-tests` — run the suites against shipped-class codegen | INVESTIGATE → *adopt, but not in this round* (deferred on CI cost) | **IMPLEMENTED** |
| **A2-31** ADR-0030's Linux release toolchain (the sibling ships Clang) | NOT NEEDED — "a release-topology decision with no reported problem behind it here" | **ADOPTED / ALIGNED** |

Both verdicts were correct **as reasoning under their premises**, and both premises were withdrawn by
the owner in this round's brief:

- A2-32's deferral rested on one sentence — "deferred only because of CI cost". The brief states that
  GitHub Actions cost is not a constraint for this project. With the cost premise gone, the verdict's
  own body ("technically valuable · passes identically") is an argument FOR, not against.
- A2-31's verdict rested on "no reported problem behind it here", i.e. on the absence of a
  product-specific reason to change. The brief replaces that test with its inverse: **the two
  repositories stay aligned on engineering infrastructure unless there is a concrete technical reason
  to differ.** Under the new test, "Anabasis has no problem today" stops being a reason to keep the
  divergence and becomes an argument that the divergence costs nothing to remove.

This entry records the investigation, the decisions, the implementation and the measurements.

---

## 1. Audit — what the two repositories actually do

Comparison source: **`Anamorph:origin/main` at `100b98c`** (fetched 2026-08-22), which is the merge of
PR #127. Nothing from any development branch or open PR was read, per the brief. `Anabasis` is at
`origin/main` `8b5f98a` — the merge of PR #21, i.e. 0.2.0 including its review round.

**One relevant fact about the sibling's delta since the last audit (`feb8991..100b98c`, 7 commits):
`.github/workflows/build.yml` is not in it.** The CI architecture compared below is the same one the
0.2.0 audit read; what changed in that range is `check-citations.py`, a Velvet-decorrelation DSP
change, `PERFORMANCE_BUDGET.md` and three performance worklogs. So this round re-decides two verdicts
against an unchanged comparison source — the decisions moved, not the evidence.

### 1.1 Linux toolchain topology

| Question | Anamorph (`100b98c`) | Anabasis (`8b5f98a`, before this round) |
|---|---|---|
| Linux **release** compiler | **clang-22**, pinned via `ANAMORPH_CLANG_VERSION` and installed from apt.llvm.org by `setup-llvm-apt.sh` | **whatever `ubuntu-latest` resolves `g++` to** (13.3.0 today), unpinned |
| Second Linux compiler | **gcc:16** container, in `linux-lto-tests` only | **clang-22** in `linux-clang`, a second near-complete Release build |
| Which build ships | the Clang one | the GCC one |
| Which build carries the first-party warning gate | the shipping one (`linux`), run **late**, after the artifact uploads | the non-shipping one (`linux-clang`) |
| Which build carries the portability compile canary | `linux` | `linux-clang` |
| Which build runs the plugin-hosting reproduction | `linux` (`AnamorphDspDump` is in the LTO lane; its probe equivalent is the pluginval + staging pair) | **both** — `linux` hosts the GCC bundle, `linux-clang` hosts the Clang one |
| LTO on the shipped plugin | `juce_recommended_lto_flags` | identical (`ANABASIS_LTO_FLAGS`, ADR-0008) |
| LTO on the **test suites** | `-flto` in `linux-lto-tests` | **nowhere** |
| Linker for Clang+LTO | `lld`, version-matched by `setup-llvm-apt.sh` | identical (`CMakeLists.txt` probes `-fuse-ld=lld`, warns and falls back) |
| ABI floor gate | `check-linux-abi.py` over the **stripped, staged** binaries | identical, and Anabasis measured its own floor rather than importing the sibling's |
| Jobs | 11: `merge-check` `docs` `source-lint` `linux` `sanitizers` `windows` `macos` `macos-intel` `linux-lto-tests` `realtime` `fuzz` | 11: `merge-check` `docs` `source-lint` `preflight` `linux` `linux-clang` `sanitizers` `realtime` `windows` `macos` `macos-intel` |
| `merge-check` compiler | clang-22, sharing `linux`'s ccache lineage | `g++`, sharing `linux`'s (GCC) lineage |

### 1.2 What the difference costs, stated precisely

The divergence is **not** "Anabasis validates less". Both repositories compile the whole first-party
tree under both major toolchains on every push. What differs is **which compiler's output a user
installs**, and that has three consequences here:

1. **The shipped Linux binary is built by an unpinned compiler.** `ANABASIS_CLANG_VERSION` pins the
   compiler that gates warnings; nothing pins the one that produces `Anabasis.so`. A runner-image
   bump changes the shipped codegen with no commit in this repository — the same class of problem
   ADR-0031 fixed for diagnostics, left open for the artifact. `check-linux-abi.py` bounds the
   *symbol floor* of that artifact, not its optimiser.
2. **The reproduction gates are aimed at the non-shipping build.** `AnabasisEngineRepro` and the
   `--assert-discriminating` channel probe run against `build-clang`, which is thrown away.
   INC-004/KI-009 was a defect that only Clang at `-flto` acted on, so aiming those two instruments
   at Clang was right — but under the current topology they validate a binary nobody receives, and
   the binary users DO receive is validated by pluginval and the probe in the `linux` job only.
3. **Two full Release builds of the same tree** run on every push, differing only in compiler, and
   the sibling deleted exactly that duplication when it made the same move (its `linux` job carries
   the note: "ABSORBED FROM THE FORMER `linux-clang` JOB").

### 1.3 Is the difference still justified?

No. It has no product-specific cause: Anabasis's `linux` job is GCC because that is what a bare
`cmake` picks on `ubuntu-latest`, not because a decision was taken. `ADR-0031` pinned Clang for
diagnostics and for RTSan; it explicitly did not consider the shipping question, and A2-31's verdict
("no reported problem behind it here") is the record of it being left alone rather than settled.

---

## 2. The plan

### 2.1 Release compiler (Goal 2 — A2-31)

- `linux` builds the shipped VST3 + Standalone with **clang-22**, the already-pinned major, installed
  by the existing composite action.
- `linux-clang` is **deleted**, and the three things that were genuinely its own move into `linux`:
  the portability compile canary, the warning-gate self-test + first-party warning gate, and
  `AnabasisEngineRepro`. The channel probe is already in `linux` and now hosts the Clang bundle.
- The warning gate moves to the **end** of the job, after the uploads — a warning finding must not
  withhold a beta artifact whose behavioural gates passed, which is the rule the ABI gate already
  follows here and the placement the sibling uses.
- `merge-check` moves to clang-22 as well, so the merge-result build uses the shipping compiler and
  shares one ccache lineage with `linux` (`ccache-ubuntu-clang22-release-`) instead of keeping a
  GCC lineage alive for one job.
- GCC does not disappear: it becomes the **compatibility** compiler, in the new lane below. That is
  the sibling's topology exactly.

### 2.2 The LTO validation lane (Goal 1 — A2-32)

`linux-lto-tests`, on every push, reusing the composite action and the ccache infrastructure; no new
workflow file.

**Two arms, and the second one is this product's own reason.** The sibling's lane has a single GCC
arm, because for the sibling the lane's job is "the other toolchain still compiles and passes".
Anabasis needs that too — after §2.1 no job builds with a GCC this repository chose, the
`sanitizers` job's unpinned valgrind copy aside — but it also has INC-004: undefined
behaviour in the engine's channel loop that **only Clang, and only at `-flto`**, acted on, green
across every console-target gate for five months. A lane that runs the suites under GCC's LTO would
not have caught it. So:

| Arm | Compiler | What it answers |
|---|---|---|
| `clang` | clang-22 `-flto` | do the 1,174 assertions still hold against the **shipped optimization class**? (INC-004's configuration) |
| `gcc` | g++-14 `-flto` | does the tree still compile and pass under the **other major toolchain**, whole-program? |

**GCC is pinned to 14 from apt, not to a `gcc:16` container.** The sibling's own comment records why
it needed the container — no released g++-16 exists in any apt source, only trunk snapshots — and
that reasoning does not transfer to a major that IS packaged: `g++-14` (14.2.0) is in Noble's
archive, so the existing `extra-packages` input installs it with no new action input, no container,
and no image tag floating on a major. This also keeps **A2-34** (the sibling's `dependency-profile`
split) NOT NEEDED for its original measured reason — the split exists to serve a containerised job,
and this lane is not one.

The lane builds only `AnabasisTests` + `AnabasisStateTests` (`ANABASIS_BUILD_STANDALONE=OFF`): between
them they compile every first-party translation unit, and the plugin is already built LTO'd in
`linux`. The bench is **not** duplicated here — Anabasis already compiles it in `linux`
(`ANABASIS_BUILD_BENCH=ON`), which is the anti-rot cover the sibling's lane provides for its own.

### 2.3 ADRs

- **ADR-0032** — the Linux release toolchain: Clang ships, GCC validates.
- **ADR-0033** — the LTO validation lane, its two arms and its pins.
- **ADR-0031** amended: its "three jobs use it" list and its `linux-clang` references.

### 2.4 Documentation

`docs/procedures/CI_CD.md` (job table, the lane, the compiler map), `docs/policies/TESTING_POLICY.md`
(Level-1b row, the job names, the checker list), `docs/procedures/BUILD.md`,
`docs/REPOSITORY_MAP.md`, `docs/architecture/COMPATIBILITY_MATRIX.md`, `docs/POSTMORTEMS.md`
(INC-003/INC-004 both name `linux-clang` as the gate that now lives in `linux`),
`docs/DOCUMENTATION_COVERAGE.md`, `docs/HANDOVER.md`, `CHANGELOG.md`, `README.md` if it names the
Linux compiler.

### 2.5 Validation required before this is called done

Every gate the 0.2.0 round ran, plus: both suites under **each of the four** (clang/gcc × LTO/non-LTO)
with the results compared; pluginval and the channel probe against the **Clang-built** shipped bundle;
the ABI floor re-measured on the Clang artifact (a different compiler can raise the symbol floor);
the first-party warning count under clang-22 AND g++-14; and the CI-time and ccache figures the brief
asks for.

---

## 3. Implementation

### 3.1 `.github/workflows/build.yml`

| Job | Before | After |
|---|---|---|
| `merge-check` | image `g++`; lineage `ccache-ubuntu-gcc-release-` | **clang-22**; lineage `ccache-ubuntu-clang22-release-`, shared with `linux` |
| `linux` | image `g++`; build → strip → suites → probe → pluginval ×2 → stage → uploads → ABI | **clang-22**; + portability compile canary, + warning-gate self-test, + `tee clang-build.log`, + engine reproduction before the probe, + the first-party warning gate **last, after the uploads** |
| `linux-clang` | pinned Clang; second full Release build | **deleted** (154 lines) |
| `linux-lto-tests` | — | **new**: matrix `toolchain: [clang, gcc]`, `fail-fast: false`, `-flto` on compile and link, both suites built and run, warning gate on both arms |
| `env` | `ANABASIS_CLANG_VERSION: 22` | + `ANABASIS_GCC_VERSION: 14` |

Job count is unchanged at **11** — one deleted, one added.

**Three implementation details worth their own line, because each is a decision rather than a
transcription:**

1. **The matrix arms take their compiler from `env`, not from the matrix.** GitHub does not expose
   the `env` context inside `strategy.matrix`, so the matrix carries only the toolchain NAME and each
   step resolves the major through `env` where that context IS available (`steps.*.with`,
   `steps.*.run`). This is what keeps `ANABASIS_CLANG_VERSION` / `ANABASIS_GCC_VERSION` the single
   authority instead of re-spelling `22` and `14` in the matrix.
2. **The compiler major is asserted from `-dumpversion`, major only.** `clang++-22 -dumpversion`
   answers `22.1.8` and `g++-14` answers `14`, so the comparison takes `${GOT%%.*}`: a patch move
   passes, a wrong major fails in seconds.
3. **The warning gate moved to the end of `linux`.** In `linux-clang` it ran immediately after the
   build, because that job produced no artifact. In `linux` it would have gated the uploads, so it
   now runs last — a compatibility or diagnostic FINDING must not withhold a beta artifact whose
   behavioural gates passed. It still fails the job. Same rule the ABI assertion already followed.

### 3.2 `.github/actions/setup-linux-build/action.yml`

No interface change. `extra-packages` — which already existed — is what installs the pinned GCC, and
the header now says so; the count in its "why this exists" note moves from four jobs to five, and a
paragraph records why the sibling's `dependency-profile` input is deliberately still absent.

### 3.3 Records

`ADR-0032` (Linux release toolchain) and `ADR-0033` (the LTO lane) are new and Accepted; `ADR-0031`
is amended inline at decision clauses 3 and 5, the second of which this round **reverses** — it said
"`linux` still builds and ships the Linux artifact with the image's GCC". `ARCHITECTURE_REVIEW_GATE.md`
gains the worked example: this is rule 3's first application, and GCC moved from rule 2 (the image
chooses) to rule 1 (this repository chooses).

---

## 4. Validation

Linux x86-64, JUCE 9.0.1 `e18f7f5…`, C++23, 4 cores. **clang 22.1.8** (apt.llvm.org, the CI pin) and
**g++ 14.2.0** (Noble's archive, the CI pin).

### 4.1 The suites, five ways — the LTO/non-LTO agreement the round is named after

| Build | Checks | Result |
|---|---|---|
| Ship class, clang-22 (the new `linux`) | 301 + 873 = **1174** | 0 failures |
| Suites, clang-22, no LTO | 301 + 873 | 0 failures |
| Suites, clang-22, **`-flto`** | 301 + 873 | 0 failures — **identical** |
| Suites, g++-14, no LTO | 301 + 873 | 0 failures |
| Suites, g++-14, **`-flto`** | 301 + 873 | 0 failures — **identical** |

**The allocation guard survives LTO, which was not a given.** It replaces global `operator new` and
interposes the malloc family; whole-program optimisation is exactly the setting in which a compiler
may inline or devirtualise around such a replacement. All five builds report the same two lines:
`allocation guard armed over 2040 process() calls across 80 configurations (new counter live, malloc
counter live)` and `prepare(48k, 256, 2) allocates new=205 malloc=1313` — the same counts under both
compilers, with and without LTO.

### 4.2 The shipped artifact, now Clang-built

| Gate | Result |
|---|---|
| First-party warnings, clang-22 ship log | **0** (2 diagnostics, both JUCE's `_deps` splash-screen pragma) |
| First-party warnings, clang-22 `-flto` log | **0** (1 vendored) |
| First-party warnings, g++-14 `-flto` log | **0** — the only two GCC warnings are `using serial compilation of N LTRANS jobs`, which carry no source path and are the LTO driver's, not the tree's |
| `AnabasisEngineRepro` | both cases kept both channels (`L/R = 0.01 dB` at defaults and at the field settings) |
| `AnabasisChannelProbe --assert-discriminating` | **112 pairs compared, 4 declared collapses, 0 undeclared**; every configuration kept both channels — the same table the GCC-built bundle produced at 0.2.0 |
| pluginval, strictness from `build.yml`, editor under xvfb | **deterministic ×3 and randomise ×3, PASSED** |
| `check-linux-abi.py` on the Clang artifacts | `CXXABI_1.3.9, GLIBC_2.38, GLIBCXX_3.4.31` on both binaries — **within the declared floor, and equal to it**. The compiler changed; the floor did not, so no supported system moves |
| `scripts/preflight.sh` end to end | **rc = 0** |
| Self-tests | check-realtime 134 · check-docs 67 · check-portability 120 · check-citations 37 · check-linux-abi 19 · check-clang-warnings 15 |
| `check-citations --check` | 50 anchors unmoved against `origin/main` |

**One difference from CI, stated rather than glossed:** locally the bundle validated by pluginval was
**not** stripped first. CI strips before validating (that ordering is deliberate and documented in
`build.yml`), so the CI gate sees the exact stripped bytes and this local run saw the unstripped
ones. `strip --strip-unneeded` removes symbol tables, not code, and the ABI check above was run on
both forms at 0.2.0 with the same answer — but the local run is the weaker of the two and should not
be quoted as if it were the gate.

### 4.3 CI cost — measured here, with what that does and does not transfer

Wall-clock on this machine (4 cores), JUCE prebuilt, ccache **cold**:

| Build | Configure | Build | Δ vs its non-LTO twin |
|---|---|---|---|
| Ship class, clang-22 (VST3 + Standalone + tests + bench + probe) | 24.7 s | **539.4 s** | — |
| Suites, clang-22, no LTO | 16.2 s | **169.0 s** | — |
| Suites, clang-22, `-flto` | 16.3 s | **263.8 s** | **+94.8 s (+56 %)** |
| Suites, g++-14, no LTO | 25.4 s | **234.2 s** | — |
| Suites, g++-14, `-flto` | 28.3 s | **312.2 s** | **+78.0 s (+33 %)** |

ccache behaviour, measured properly — **same build directory, object files deleted, cache populated
by the previous pass**, which is what `restore-keys` reproduces for a lane across runs:

| Lane | Cold | Warm | Warm hit rate |
|---|---|---|---|
| Ship (clang-22) | 528.5 s | **169.6 s** | **154 / 158 = 97.5 %** |
| LTO suites (clang-22) | 263.8 s | **164.3 s** | **51 / 52 = 98.1 %** |

**Read those two rows together, because they say different things.** The ship lane falls **3.1×**:
its work is mostly compiling, and ccache caches compiling. The LTO lane falls only **1.6×** at a
HIGHER hit rate — 98.1 % of its compiles were served from cache and it still took 164 s, because
what remains is the **link**, and the whole-program optimiser runs there. That is inherent to the
lane rather than a misconfiguration, and ADR-0033 states it so nobody later tries to tune it away.

**A false start, recorded because it would otherwise look like a measurement.** The first attempt at
the warm numbers compared two builds in DIFFERENT directories with different `ANABASIS_JUCE_PATH`
values, and reported 15.8 % hits and 528 s — no better than cold. ccache hashes the preprocessed
output, which carries the include paths, so a moved JUCE tree is a different compilation. (The
version bump in this same round did the same thing through `-DANABASIS_VERSION_STRING`.) Neither
affects CI, where a lane's paths are constant across runs and the version changes on the commit that
is supposed to invalidate the cache — but it does mean the only honest warm measurement is one taken
in a fixed directory, which is what the table above is.

**What does NOT transfer to GitHub's runners:** absolute times (different hardware, 4 cores here) and
the JUCE fetch, which is prebuilt here and is a real per-job cost there. What DOES transfer is the
shape: the LTO arms cost roughly a third to a half more than their non-LTO twins, ccache removes most
of the compile and none of the link, and the two arms run **in parallel** with each other and with
every other job — so the run's critical path is unchanged as long as this lane is not the longest job
in the matrix, which it is not (`macos` is).

**Net runner time is close to flat, and it is worth saying why rather than claiming a win.** The
round DELETES `linux-clang`, a full Release build of the plugin plus four targets, and ADDS two
suites-only arms. Two suites-only builds cost less than one plugin build plus its four targets on
this machine; on GitHub's runners the balance depends on their relative core counts. The honest claim
is "no material increase", not "cheaper".

---

---

## 4b. Review round (same day): the customer upload was not gated on the two reproductions

One finding, fixed. `Upload Linux artifacts` named only `steps.tests` and `steps.stage`, so a
bundle that failed the engine reproduction or the channel probe was still staged and **still
uploaded as the customer artifact** — only the job's conclusion turned red, after the fact.

**Pre-existing for the probe, and made to matter by this round.** While those two instruments lived
in `linux-clang` their outcome could not sensibly gate an upload from a different job: they read a
build that was discarded. ADR-0032 moved them onto the bundle that ships and gave `linux` the
upload, which is exactly the arrangement in which "the probe reports a lost channel and the artifact
ships anyway" becomes reachable. Leaving the gate at two names would have made the absorption
cosmetic — the instruments would have been *pointed* at the shipped bytes without being *able to
stop them*.

The two steps gained ids (`repro`, `probe`) and the upload names all four:

```yaml
if: >-
  ${{ !cancelled()
      && steps.tests.outcome == 'success'
      && steps.repro.outcome == 'success'
      && steps.probe.outcome == 'success'
      && steps.stage.outcome == 'success' }}
```

**Three properties, asserted rather than asserted-to:**

- **A skipped instrument does not pass.** `outcome` is `skipped` when an earlier failure skips the
  step, and `skipped != 'success'`, so the upload is skipped too. The gate asks "did this pass",
  never "did this not fail" — which is the direction `!cancelled()` alone does not give.
- **`stage` is deliberately unchanged.** It still runs on `strip` alone, still self-validates the
  tree it assembles, and still produces the `dist/` copy the ABI floor assertion reads — a
  compatibility FINDING is worth reporting even on a build that failed a behavioural gate. Nothing
  ships from it while the upload is skipped, which is the property that matters. `pluginval` stays
  out of the list, unchanged: a pluginval-only failure still yields beta artifacts on purpose.
- **Nothing else moved.** The parsed workflow was diffed against `HEAD` job by job and step by step:
  **exactly three semantic differences** — two added `id:` keys and the one `if:`. The GCC arm of
  `linux-lto-tests`, raised in the same review as *safe as written* (GCC's `-flto` links through
  binutils with the LTO plugin; `CMakeLists.txt:196-208` forces `-fuse-ld=lld` for Clang only, and
  probes even then), was deliberately not touched.

**Validated:** all five `.github` YAML files parse; the four named ids exist in the job, are each
compared against `'success'`, and all four run *before* the upload step (asserted from the parsed
document, not read by eye); `preflight.sh` rc=0 with 301 + 873 checks and the six self-tests
unchanged. No C++ source was touched, so no rebuild was required.

The prose that describes the gate moved with it: the workflow's own header block and
`CI_CD.md` §"Artifact safety rules (fail-closed)".

## 5. What this round deliberately did NOT do

- **No DSP algorithm, parameter, serialization, threading-model or latency change.** No first-party
  source file was touched at all: the diff is CI, records and documentation. `tests/fixtures/parameter_registry.snapshot` is unchanged and deliberately not re-frozen.
- **No `check-gcc-warnings.py`, and no warning baseline file.** The sibling carries both; this
  repository gates BOTH compilers at zero through the one script it already has, which is the
  stricter contract and the one the 0.2.0 audit chose when it rejected importing the sibling's debt
  lists (A2-22, CANNOT). Measured first, adopted second: g++-14.2.0 emits zero first-party warnings
  on this tree, so the zero gate is a fact about the tree rather than a hope.
- **No `gcc:16` container, and therefore no `dependency-profile` input** (A2-34 stays NOT NEEDED —
  see ADR-0033 for why the sibling's reasoning is about GCC 16 specifically and does not transfer).
- **`AnabasisBench` is not built in the LTO lane.** `linux` already compiles it on every push, now
  under the pinned Clang with the shipped LTO flags. Duplicating it would compile it twice per push
  to answer one question.
- **The `sanitizers` job's `build-vg` copy still uses the image's default `g++`**, exactly as the
  sibling's does. It is a compiler the runner picked, incidental to a memcheck run, and it is NOT
  the compatibility statement this repository makes — that one is `linux-lto-tests`' pinned arm.
- **`preflight` (the pre-P1 scaffold guard) is untouched.** It is a permanent no-op now that the
  build exists, and removing it is a separate change with its own reasoning.
- **`fuzz` was not migrated.** A2-33 was DECLINED on measurement at 0.2.0 (`setStateInformation` is
  at 100 % line coverage in the state suite) and nothing in this round's brief re-opened it.

## 6. Follow-ups this round leaves open

1. **CI has never run any of this.** Everything above is a local measurement on 4 cores. The first
   push is what proves the two arms install their compilers, that `steps.cc.outputs.*` reaches the
   cache key, and that the matrix expands as written. Nothing here can substitute for that.
2. **Three obligations carried forward from 0.2.0, unchanged:** the `DEPENDENCY_POLICY.md` rule-2
   Level-5 manual audition for the JUCE bump; re-converging the JUCE pin with the read-only sibling
   (Anabasis 9.0.1, Anamorph 9.0.0); and the first green `realtime` job, C++23 under MSVC/AppleClang
   and the macOS dSYM contract, all of which are CI's to demonstrate.
3. **The GCC major will need re-taking when the runner image moves.** `g++-14` is Noble's newest
   RELEASED g++; when `ubuntu-latest` becomes 26.04 the archive's newest changes and this pin should
   be re-argued (a Build System change under the review gate, like any other). The sibling's `gcc:16`
   container becomes the better answer the moment a released 16 is packaged for the runner's
   distribution — that is a reason to revisit, not a reason to pre-empt.
4. **Windows and macOS still float.** Their compilers are chosen by the runner image (review-gate
   rule 2), and this round changed only Linux. Whether the same argument — "the compiler that
   produces the shipped bytes should be one this repository chose" — should reach them is a real
   question that needs an answer per platform, and MSVC/AppleClang are not installable the way
   apt.llvm.org's Clang is.
