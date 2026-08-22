# 2026-08-22 — CI / toolchain parity audit against the sibling, and what it found

Round A4. The 0.2.1 round aligned the Linux **release topology** (ADR-0032) and added the LTO lane
(ADR-0033). This round asks the harder question the owner's brief poses: **is the migrated workflow
actually equivalent to the sibling's CI architecture**, area by area, or did it migrate structure
while leaving toolchain configuration behind?

It is an audit first. Nothing below was decided from memory of the previous round; every row was
re-read from the two trees.

## Comparison source

| | Revision | Note |
|---|---|---|
| Anamorph | `origin/main` **`2c40a86`** (fetched 2026-08-22) | `.github/` is **unchanged since `feb8991`** — the two rounds since (`100b98c`, `2c40a86`) touched DSP, tests, docs and `check-citations.py` only. The CI baseline has been stable across all three audits. |
| Anabasis | working tree at `43b415f` (0.2.1 + the upload-gate fix) | |

Read in full on both sides: `.github/workflows/{build,codeql,dependency-review,msvc,release}.yml`,
`.github/actions/setup-linux-build/action.yml`, `.github/dependabot.yml`,
`scripts/{setup-linux.sh,setup-llvm-apt.sh}`, and the toolchain half of `CMakeLists.txt`.

## Parity table

Legend: **✓** equivalent · **✗** mismatch · **~** equivalent with a deliberate Anabasis addition.

| Area | Anamorph | Anabasis (before) | Match | Action |
|---|---|---|---|---|
| **GCC version** | **16** (`ANAMORPH_GCC_VERSION`) | **14** | ✗ | **MIGRATE → 16** |
| **GCC acquisition** | `container: gcc:16` + `dependency-profile: headless` | apt `extra-packages: gcc-14 g++-14` | ✗ | **MIGRATE → container** |
| GCC role | compatibility only (`linux-lto-tests`) | compatibility only | ✓ | — |
| **Clang version** | 22 | 22 | ✓ | — |
| **Clang acquisition** | `setup-llvm-apt.sh <major>` · apt.llvm.org · key pinned **by identity** · suite from `/etc/os-release` · installed version asserted | byte-identical logic | ✓ | prose drift only |
| LLVM packages | `clang-N`, `lld-N`, `libclang-rt-N-dev` | identical | ✓ | — |
| AppleClang / Xcode | image default, unpinned | identical | ✓ | — |
| MSVC | auto-detected; **toolset recorded + ABI series asserted** | auto-detected, **no record, no assertion** | ✗ | **MIGRATE** |
| CMake | ≥ 3.22; apt `cmake`; image default elsewhere | identical | ✓ | — |
| Ninja | apt `ninja-build`; brew `ninja \|\| true` + `ninja --version` | same, **version not printed** | ✗ | **MIGRATE** |
| C++ standard | `set(CMAKE_CXX_STANDARD 23)` + REQUIRED + no extensions | identical | ✓ | — |
| ccache install | composite action, non-fatal fallback → `*_COMPILER_LAUNCHER` | identical | ✓ | — |
| **ccache env** | `CCACHE_DIR=$GITHUB_WORKSPACE/.ccache`, `CCACHE_MAXSIZE` per job (400M–1G), `CCACHE_COMPILERCHECK=content` | **none of the three set** | ✗ | **MIGRATE** |
| **ccache path** | `${{ github.workspace }}/.ccache` | `~/.cache/ccache` | ✗ | **MIGRATE** |
| **macOS ccache** | **cached**, both jobs, with the dsymutil question measured and answered | **not cached**, on a rationale the sibling refuted | ✗ | **MIGRATE** |
| LTO, shipped artifact | `juce_recommended_lto_flags` · `-Wl,-object_path_lto` · lld probed for Clang | identical, plus `ANABASIS_NO_LTO` (bisection) | ~ | keep the extra |
| LTO lane shape | one GCC arm | **two arms** (clang + gcc) | ~ | keep the extra arm |
| GCC LTO linker | binutils + GCC LTO plugin; no lld in `headless` | same (no lld for the gcc arm) | ✓ | — |
| **Sanitizer flag set** | `address,undefined,vptr,float-divide-by-zero,implicit-conversion,unsigned-shift-base,local-bounds,nullability` + `-fsanitize-ignorelist` | `address,undefined` | ✗ | **MIGRATE, measured** |
| **UBSan ignorelist** | `scripts/ubsan-ignorelist.txt`, one sub-check, one tree | absent | ✗ | **MIGRATE** |
| **`ASAN_OPTIONS`** | `detect_leaks=1:check_initialization_order=1:strict_init_order=1:strict_string_checks=1` | `detect_leaks=0` | ✗ | **MIGRATE, measured** |
| `UBSAN_OPTIONS` | `print_stacktrace=1:halt_on_error=1` | identical | ✓ | — |
| valgrind | `extra-packages: valgrind`, both suites, `--error-exitcode=1` | identical (+ `-DANABASIS_NO_ALLOC_GUARD`) | ~ | keep the extra |
| Ubuntu runner | `ubuntu-latest` | `ubuntu-latest` | ✓ | — |
| macOS runners | `macos-latest` + `macos-15-intel` | identical | ✓ | — |
| Windows runner | `windows-latest` | identical | ✓ | — |
| **Package installation** | `setup-linux.sh [full\|headless]`; CORE list names `ca-certificates` **and `python3`** explicitly | single list, **no profiles, no `ca-certificates`, no `python3`** | ✗ | **MIGRATE** |
| GitHub Actions versions | checkout `3d3c42e` · cache `55cc834` · upload `043fb46` · download `3e5f45b` · codeql `ff2f1c6` · msvc-analysis `9631532` · dep-review `a1d282b` | **identical, all five workflows** | ✓ | — |
| Permissions | `contents: read` | identical | ✓ | — |
| Concurrency | same group, same tag guard | identical | ✓ | — |
| Triggers | push `**` / PR / dispatch / call | identical | ✓ | — |
| pluginval strictness | 10, one authority | identical | ✓ | — |
| **`timeout-minutes`** | on **every** job (10–60) | on **one** job | ✗ | **MIGRATE** |
| **Cache key naming** | `-sanitizers-clang22-`, `-realtime-clang22-`, `-gcc16-lto-`, `ccache-macos-universal-`, `ccache-macos-intel-` | `-clang22-san-`, `-clang22-rtsan-`, `-<tag>-lto-` | ✗ | **MIGRATE** |
| Artifact flow | stage → self-validate → gated upload | identical + the repro/probe gates | ~ | keep the extra |
| `fuzz` job | present | **absent** | ✗ | **KEEP** — A2-33 declined on measurement |
| `preflight` job | absent | **present** (+ `needs:` on every build job) | ✗ | **KEEP this round**, follow-up |
| GCC warning gate | `check-gcc-warnings.py` + hand-added GCC-only flags + a 3-entry baseline | `check-clang-warnings.py` at **zero**, both compilers | ✗ | **KEEP** — stricter |
| Warning baseline files | two debt lists | none | ✗ | **KEEP** — stricter (A2-22) |
| `TESTS_NO_FTZ` under valgrind | set | absent | ✗ | **KEEP** — no such failure here |

**Twelve mismatches to migrate, five deliberate divergences, and everything else already equivalent.**
The action pins, runner images, triggers, permissions, C++ standard, CMake/Ninja acquisition and the
entire Clang install path were already byte-equivalent — the previous round did migrate those.
What it left behind is **configuration around** the toolchain: ccache, sanitizer depth, package
profiles, timeouts, version reporting, and the GCC major and its acquisition method.

---

## The mismatches, one verdict each

### MIGRATE (12)

**P-01 · GCC major 14 → 16, and the acquisition method with it.**
0.2.1 pinned `g++-14` from Noble's archive and argued that the sibling's `gcc:16` container was
reasoning "about GCC 16 specifically". Re-read against the sibling's own comment, that inverts: the
container exists **because** no apt source ships a *released* g++-16 — Noble stops at 14, and both
`ubuntu-toolchain-r/test` and Ubuntu 26.04 carry trunk snapshots that predate 16.1. The official
image is the only package-managed route to a released 16. So "apt has 14, therefore pin 14" was
choosing the version to fit the acquisition method, which is exactly backwards, and the owner's rule
for this round — *the complete acquisition/configuration method, not only the version number* —
settles it. **Migrate the whole mechanism: `container: gcc:16`, `ANABASIS_GCC_VERSION: 16`,
`dependency-profile: headless`.**

**P-02 · The `headless` dependency profile (audit item A2-34, re-opened).**
NOT NEEDED twice, both times conditional on the GCC lane not being containerised. P-01 makes it
containerised, so the condition is now met and the item flips: `build-essential` in the container
would install a distribution GCC over the pinned one, which is precisely what the profile prevents.
**Migrate `scripts/setup-linux.sh` to `full|headless`, and the action's `dependency-profile` input.**

**P-03 · `ca-certificates` and `python3` in the core package list.** The sibling names both
explicitly; Anabasis names neither. On a GitHub runner they are preinstalled and nothing notices —
in a container, `python3` is what every checker in `scripts/` runs on, and it is present in `gcc:16`
only transitively. A gate that cannot run because its interpreter is absent is the failure that line
prevents. **Migrate.**

**P-04 · ccache environment.** The sibling sets `CCACHE_DIR`, `CCACHE_MAXSIZE` and
`CCACHE_COMPILERCHECK: content` on every caching job; Anabasis sets **none**, so it runs on ccache's
defaults: `~/.cache/ccache`, a **5 GB** ceiling, and `mtime` compiler identity. Three consequences,
all real: the cache the workflow uploads is unbounded in practice; `mtime` can serve an object
compiled by a *different* compiler that happens to share a timestamp (the pin exists to stop exactly
that); and `~` is the wrong place the moment a job runs in a container. **Migrate all three, with
the sibling's per-job budgets.**

**P-05 · ccache path → `${{ github.workspace }}/.ccache`.** Follows from P-04 and is what makes the
containerised GCC lane cache at all.

**P-06 · macOS ccache — and this one is a rationale that was refuted, not merely absent.**
Anabasis's workflow says the macOS jobs are "deliberately NOT cached" because `dsymutil` walks the
debug map back to the object files. The sibling asked the same question and answered it in its own
comment: *"Cached objects are real .o files at the paths the linker recorded, so the packaging step's
`dsymutil` walk is unaffected — it reads the same object files it always read, whoever compiled
them"*, and separately verified that ccache hashes the full `-arch` list so a universal object cannot
be served to a thin build. It also records `macos` as the run's critical path at **29m44s**, of which
**16m40s** is the build step. So Anabasis is paying the longest job in the matrix, in full, on every
push, to avoid a hazard that was measured not to exist. **Migrate: cache both macOS jobs.**

**P-07 · Sanitizer flag set.** See the measurements below — migrated with **one** sub-check
deliberately excluded.

**P-08 · The UBSan ignorelist.** Adopted (`scripts/ubsan-ignorelist.txt`), scope unchanged from the
sibling's: one sub-check, one vendored tree, no first-party path.

**P-09 · `ASAN_OPTIONS`.** `detect_leaks=0` → the sibling's four-option set **including
`detect_leaks=1`**. Anabasis's stated reason for 0 was that JUCE's singletons are torn down at exit
"in ways LeakSanitizer reports". Measured: they are not — see below.

**P-10 · MSVC toolset recorded and asserted.** The sibling reads the toolset out of `CMakeCache.txt`,
prints it, writes it to the job summary and fails if the **ABI series** leaves `14.x` (which is what
decides which Visual C++ redistributable a user needs). Anabasis records nothing: `windows-latest`
floats and MSVC is auto-detected, so today a shipped `.vst3` can be built by a toolset no artifact
names. **Migrate verbatim in behaviour.**

**P-11 · `timeout-minutes` on every job.** Without it a hung job burns the 6-hour default. **Migrate
the sibling's values**, and give Anabasis's extra jobs the value of their nearest sibling equivalent.

**P-12 · Toolchain versions printed.** The sibling prints `ninja --version` and `ccache --version` on
macOS and `g++ --version` in the LTO lane; Anabasis prints one. The owner's brief asks for versions
in the logs as a validation requirement. **Migrate, and extend: every Linux job now prints the
compiler it actually resolved.**

Cache-key naming (`-sanitizers-`/`-realtime-`/`-gcc16-lto-`) moves with P-04/P-05 — same lineages,
sibling spelling. A key change starts a fresh lineage once, which is the intended cost.

### KEEP — deliberate, documented divergences (5)

| # | Divergence | Why it stays |
|---|---|---|
| K-01 | **No `fuzz` job** | A2-33, DECLINED on measurement at 0.2.0: gcov puts `setStateInformation` at **100 % of its 74 lines** in the state suite, so libFuzzer would add input diversity over branch combinations rather than reach. Re-evaluate if a state-parsing defect is ever reported, or if that coverage falls. |
| K-02 | **No `check-gcc-warnings.py`, no baseline files** | A2-22, CANNOT: the sibling's baselines key on its paths under its majors. Anabasis gates **both** compilers at **zero** through one script — the stricter contract. Adopting a debt list would *permit* warning classes this repository forbids. |
| K-03 | **No `ANABASIS_TESTS_NO_FTZ`** | The sibling needs it because valgrind does not honour FTZ/DAZ and its denormal assertion fails under emulation. Anabasis's valgrind lane is green without it. Adding an escape hatch for a failure that does not occur would weaken a gate to no purpose. |
| K-04 | **`preflight` job + `needs:` on every build job** | A pre-P1 scaffold guard, now a permanent no-op. Removing it touches every build job's `if:` and interacts with `release.yml`'s `workflow_call` path — orthogonal churn in a toolchain round. **Follow-up, not a keep-forever.** |
| K-05 | **`unsigned-shift-base` excluded from the sanitizer set** | Measured, see below. |

### Anabasis-side extras the sibling does not have (keep, no action)

`ANABASIS_NO_LTO` (the KI-009 bisection switch) · `AnabasisChannelProbe` + `AnabasisEngineRepro` and
the upload gates on them · the second (Clang) arm of the LTO lane · `-DANABASIS_NO_ALLOC_GUARD` on
the valgrind build. Each is this product's own instrument and none has a sibling counterpart to
diverge from.

---

## The sanitizer migration, measured rather than assumed

Adopting the sibling's flag set verbatim was tried first, on the pinned clang-22, RelWithDebInfo,
both suites, with its exact `ASAN_OPTIONS`. **It failed, twice, for two unrelated reasons** — which
is the whole argument for measuring a parity migration instead of transcribing it.

**Finding 1 — `unsigned-shift-base` fires on the dither RNG, once, correctly, on code that is not a
defect.**

```
src/dsp/AnabasisEngine.cpp:1013:38: runtime error: left shift of 2654435769 by 13 places
cannot be represented in type 'uint32_t'
```

That line is the xorshift PRNG the dither stage runs:
`rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;`
Unsigned left shift is **well defined** in C++ — it wraps modulo 2³² — and wrapping is the algorithm,
not an accident. `-fsanitize=unsigned-shift-base` is explicitly a *not-UB* check for shifts that are
*often* unintentional; a xorshift is the canonical case where they are not. The sibling never hit it
because its tree contains no such generator.

The alternatives were weighed against the sibling's own principle (*do not weaken the entire suite
because one path is noisy*): an ignorelist entry would exempt **first-party** code — and would have
to exempt the whole enclosing function, which is `processChunk`, the largest audio-path body in the
tree. Rewriting the generator is out of the question for a different reason: the dither RNG is
frozen by `DSP_POLICY` (offline renders must repeat exactly), so changing it is a hard-stop item, not
a sanitizer accommodation. **Excluding this one sub-check exempts nothing and keeps the other six
instrumenting every translation unit**, first-party included.

**Finding 2 — the state suite overflows an 8 MB stack under the deeper instrumentation.**

```
ERROR: AddressSanitizer: stack-overflow
SUMMARY: … tests/state_tests.cpp:2680 in testTeardownAndReengageInvariants()
```

Not a defect either, and not a leak: line 2680 is the function's opening brace. That one function
constructs **four** `AnabasisAudioProcessor` objects, each in its own `{ }` scope. Without
instrumentation the compiler reuses one stack slot for all four; ASan's use-after-scope
instrumentation gives each its own slot plus redzones, so the frame becomes their sum. The extra
sub-checks pushed that sum past 8 MB. `ulimit -s 65536` before the run is the standard remedy and
costs nothing; restructuring the test to heap-allocate is a source change this round has no reason to
make.

**Result with those two corrections** — the sibling's set minus `unsigned-shift-base`, the ignorelist,
the sibling's full `ASAN_OPTIONS` (`detect_leaks=1` included) and a 64 MB stack:

| Suite | Result |
|---|---|
| `AnabasisTests` | **300 checks, 0 failures**, exit 0 |
| `AnabasisStateTests` | **873 checks, 0 failures**, exit 0 |

(300 rather than 301: the allocation guard stands itself down under ASan and *discloses* that it did,
which is the 0.2.0 behaviour and not new here.)

**`detect_leaks=1` passes.** The comment claiming JUCE's singletons would make LeakSanitizer report
at exit is retired by measurement — nothing was reported. That assumption had been carried since the
sanitizer job was written and had never been tested.

**Net coverage change:** the `sanitizers` job goes from `address,undefined` to
`address,undefined,vptr,float-divide-by-zero,implicit-conversion,local-bounds,nullability` with leak
detection and three tightened ASan runtime checks on — six new sub-checks and four new runtime
options, at the cost of one sub-check the sibling has and one `ulimit` line it does not need.

---

## Implementation

| File | Change |
|---|---|
| `.github/workflows/build.yml` | `ANABASIS_GCC_VERSION` 14 → **16**; `linux-lto-tests` becomes a `container: gcc:16` job with `dependency-profile: headless`; **`linux-lto-clang`** is split out (the Clang arm ADR-0033 added for INC-004 — `container:` is a per-job key, so a matrix could not carry one containerised arm); `CCACHE_DIR`/`MAXSIZE`/`COMPILERCHECK` on every caching job and the cache path moved to `${{ github.workspace }}/.ccache`; **the two macOS jobs cached**, with the launcher wired into their configure and a stats step; the sanitizer flag set deepened behind the ignorelist, `ASAN_OPTIONS` replaced and the run given a 64 MB stack; **the MSVC toolset recorded and its ABI series asserted**; `timeout-minutes` on all twelve jobs; cache keys re-spelled to the sibling's. |
| `.github/actions/setup-linux-build/action.yml` | New `dependency-profile` input, passed through to `setup-linux.sh`; a printed record of the toolchain the run resolved (cmake, ninja, default `c++`, `ld`). |
| `scripts/setup-linux.sh` | `full` / `headless` profiles with a validated argument; `ca-certificates` and `python3` named in the core list. |
| `scripts/ubsan-ignorelist.txt` | New — one sub-check, one vendored tree. |
| `scripts/setup-llvm-apt.sh` | Prose only: it still described `linux-clang`, deleted at 0.2.1. |
| Records | **ADR-0034** new; **ADR-0033** amended inline (its "pinned from apt, not from a container" section is the clause this round reverses, kept as the record of what it decided); `ADR_INDEX.md`. |
| Docs | `CI_CD.md` (job table, artifact-safety note, a 0.2.2 section), `TESTING_POLICY.md` (Level-1b/1c), `REPOSITORY_MAP.md`, `BUILD.md`, `DOCUMENTATION_COVERAGE.md`, `HANDOVER.md`, `CHANGELOG.md`. |

**Job count 11 → 12** (`linux-lto-clang` added). No first-party source file was touched.

## Validation

| Gate | Result |
|---|---|
| All five workflows + the composite action + `dependabot.yml` | parse |
| Semantic diff of the parsed `build.yml` vs `HEAD` | every difference intended and enumerated below |
| `scripts/setup-linux.sh` | `bash -n` clean; a bad profile exits **2** with usage |
| `scripts/setup-llvm-apt.sh` | `bash -n` clean |
| Both suites, **the new sanitizer set** (clang-22, `detect_leaks=1`, 64 MB stack) | **300 + 873, 0 failures**, exit 0 |
| Both suites, ordinary Release build | **301 + 873 = 1174, 0 failures** |
| `scripts/preflight.sh` | **rc = 0** |
| Six checker self-tests | 134 · 67 · 120 · 37 · 19 · 15 |
| `check-citations --check` | 50 anchors unmoved |
| `check-linux-abi.py` on the built artifacts | within the declared floor |

**What could NOT be validated here, stated rather than implied:** the `gcc:16` container lane. This
environment has no container runtime (`docker info` fails), so the lane's YAML, its `headless`
profile and its assertion step are reviewed and parsed but never executed. The first push is the
measurement, and the pin is one env line plus one image tag if 16 rejects something 14 accepted.
That is the compatibility lane doing its job rather than a surprise.

## Follow-ups

1. **The container lane's first run.** See above.
2. **The GCC warning gate is measured at 14, not 16.** It gates first-party warnings at zero. Under
   `juce_recommended_warning_flags` that measured zero at g++-14.2.0; 16 is unmeasured. Kept strict
   on purpose — ADR-0031's rule is that a pin which surfaces diagnostics gets them fixed.
3. **`preflight` (K-04)** is still Anabasis-only and still a permanent no-op. Deleting it touches
   every build job's `if:` and interacts with `release.yml`'s `workflow_call` path; it is a round of
   its own, not a rider on a toolchain one.
4. **Windows and macOS compilers still float** (review-gate rule 2). This round closed the *record*
   half for Windows; whether either should be pinned is a separate question per platform, and
   neither is installable the way apt.llvm.org's Clang is.
5. Carried forward unchanged from 0.2.0/0.2.1: the `DEPENDENCY_POLICY` rule-2 Level-5 JUCE audition,
   re-converging the JUCE pin with the read-only sibling, and CI's own first green `realtime`,
   MSVC/AppleClang C++23 and macOS dSYM demonstrations.

---

## Review round (same day): the doc drift, and a container package that would have killed the lane

Two findings, both fixed. The owner has reviewed and signed off the remaining migration decisions —
the sanitizer set and its one exclusion, the fuzz and preflight verdicts, the warning gates and the
GCC-16 strategy — so those are **approved as they stand** and are not re-opened here.

### 1. `CI_CD.md` still described a two-arm lane that no longer exists

The job table was updated when the lane split; the prose bullet below it was not, and still read
"Its `clang` arm … and its `gcc` arm". One more copy had the same fault — the "What 0.2.2 changed"
row still said "`linux-lto-tests`, two arms".

Both now describe the actual structure: two jobs, what each answers, and **why** they are two rather
than a matrix (`container:` is a per-job key, so one containerised arm and one bare arm cannot share
a `strategy.matrix`). The "why four Linux jobs" heading became five, and the bullet gained the
`headless` profile and the container.

**Asserted rather than eyeballed:** a check parses `build.yml`, confirms every one of the twelve jobs
is named somewhere in `CI_CD.md`, and confirms that every surviving mention of an "arm" is a sentence
saying the arms are separate jobs. `linux-clang` still appears once, as history, which is correct.

### 2. `libfreetype6-dev` does not exist in the container's distribution

The reviewer asked whether the `headless` profile's Ubuntu-named packages are valid inside a Debian
container. Investigated properly, and the answer is **yes for the packages they named and no for one
they did not**.

**What the container actually is:** `gcc:16` is `FROM buildpack-deps:trixie` — Debian 13, read from
the official image's Dockerfile, not assumed.

**Every name checked against the trixie archive**, with the check validated in both directions first
(the naive "does the page 200" test says yes for `definitely-not-a-real-package-xyz`, and a
"Package not available" grep says ABSENT for `libgtk-3-dev`, which is present — both were discarded
before the real sweep):

| Package | trixie | Note |
|---|---|---|
| `libwebkit2gtk-4.1-dev` | **present** (2.52.5) | the reviewer's example — fine as it stands |
| `libgtk-3-dev` | **present** (3.24.49) | fine |
| every X11 `-dev`, `libasound2-dev`, `libjack-jackd2-dev`, `libcurl4-openssl-dev`, `libegl-dev`, `libglu1-mesa-dev`, `mesa-common-dev`, `libfontconfig1-dev`, `cmake`, `ninja-build`, `pkg-config`, `ca-certificates`, `python3`, `git` | **present** | fine |
| **`libfreetype6-dev`** | **ABSENT** | neither real nor virtual in trixie |
| `libfreetype-dev` | **present** (2.13.3) | and present on noble (2.13.2) |

**So the lane would have failed at dependency install** — on a package nobody flagged. It passes on
the runners only because Ubuntu noble's `libfreetype-dev` carries `Provides: libfreetype6-dev`; noble
has no real `libfreetype6-dev` either (`apt-cache policy` → `Candidate: (none)`). The `6` spelling
has been a compatibility shim for a while, and one distribution has now dropped the shim.

**Fixed by naming `libfreetype-dev`**, which is a real package on both — not by adding a fallback for
a name that is obsolete on both.

**And the web-browser binding moved `CORE` → `FULL_EXTRA`, on evidence rather than to dodge a name.**
It is not a compile dependency of this project at all: every target sets `JUCE_WEB_BROWSER=0`
(`CMakeLists.txt`), JUCE gates its webkit include on that macro (`juce_gui_extra.cpp:123`), and JUCE
9.0.1 declares **no `linuxPackages` for `juce_gui_extra`** — only `alsa`, `freetype2 fontconfig` and
`egl gl`, in three other modules. It is also the heaviest entry in the list and the most volatile
name in it (`4.0` already gone from trixie, successor `libwebkitgtk-6.0-dev`). `full` keeps it, so a
developer flipping the macro does not face a second dependency hunt; `headless` — which builds two
console targets and no browser — no longer installs it. **That is the profile split doing its job,
not a weakening of it.**

The script now also prints the distribution it resolved the names on. A container lane makes the
package list a portability surface, and review-gate rule 2 asks for detection and record where a
version cannot be pinned.

### Validation of the review round

| Gate | Result |
|---|---|
| `docs/procedures/CI_CD.md` vs `build.yml` | asserted from the parsed workflow: all **12** jobs documented, no surviving two-arm claim, container and profile both described |
| `bash -n` on both setup scripts | clean; a bad profile still exits **2** with usage |
| Profile contents | asserted: `headless` is webkit/gtk-free and carries the portable freetype name; `full` = `headless` + 7 |
| `apt-get install -s` on Ubuntu noble | **both profiles resolve, rc=0**, no "Unable to locate" |
| Debian trixie archive | every name in both profiles present (table above), discriminator validated in both directions |
| All five workflows + composite action + `dependabot.yml` | parse |
| `scripts/preflight.sh` | **rc = 0** |
| Suites | **301 + 873 = 1174 checks, 0 failures** (no C++ source touched) |
| Checker self-tests | 134 · 67 · 120 · 37 · 19 · 15 · 50 anchors unmoved |

### Signed off, no change required

The owner's manual review confirms the remaining migration decisions, so these are **acknowledged
and closed** rather than carried as open items: the sanitizer set with `unsigned-shift-base`
excluded and the 64 MB stack; `detect_leaks=1`; the absent `fuzz` job (A2-33) and the retained
`preflight` job (K-04); both warning gates at zero with no baseline file; and the GCC-16 container
strategy. The one open item that is **not** a decision remains open on its own terms: the container
lane has still never executed, and its first CI run is the measurement.
