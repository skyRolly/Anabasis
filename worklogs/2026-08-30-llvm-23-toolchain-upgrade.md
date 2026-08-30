# 2026-08-30 — LLVM 23: released upstream, not released in our package source

**Directive.** Move every version-pinned LLVM component to **LLVM 23.1.0**, then audit every
*other* pinned toolchain for a newer stable release — preferring the newest stable, **refusing
release candidates, nightlies, development snapshots and unreleased versions** — and verify the
result rather than assert it.

**Outcome, up front.** 23.1.0 is genuinely released. What `apt.llvm.org` ships for major 23 on
noble is **not that build** — it is a `release/23.x` commit that still carries
`LLVM_VERSION_SUFFIX -rc3`, and Debian's packaging drops the suffix so nothing local says so. It
passed every gate in this repository. **The pin holds at 22**, and `setup-llvm-apt.sh` gained the
assertion that makes the difference legible. The 23 measurements are kept: when upstream rebuilds
the suite at the tag, the bump is one line.

---

## 1. Is 23.1.0 actually released? (asked first, because the directive also forbids unreleased versions)

The two clauses of the directive can conflict, and on 2026-08-30 they did — though not in the way
expected. The first measurement was upstream's own record.

| Source | What it says | Verdict |
|---|---|---|
| `git ls-remote --tags llvm/llvm-project` | `refs/tags/llvmorg-23.1.0^{}` → `ea7d852a70e8bdfaf601d6626a760f9771b2c4b4` — an **annotated tag**, cut like every other release | final release |
| `https://releases.llvm.org/` — its `RELEASES` array | `['25 Jul 2026', '23.1.0', …]` listed **above** `['16 Jun 2026', '22.1.8', …]` | final release |
| `https://llvm.org/` front page | current release **`LLVM 23.1.0`**, `23.1.0: Aug 2026`, with *Pre-releases* a separate link | final release |
| `https://apt.llvm.org/noble/dists/` | `llvm-toolchain-noble-23/` published, `Release` dated **2026-08-18** | packaged for our distro |

The two web pages disagree on the date (25 Jul vs Aug 2026) — recorded rather than silently
resolved, and immaterial once the tag is in hand.

**One contrary signal, examined and dismissed.** `apt.llvm.org/llvm.sh` still reads
`CURRENT_LLVM_STABLE=22`. That script is maintained in `opencollab/llvm-jenkins.debian.net` and
lags the release train; it is not upstream's release record, and `scripts/setup-llvm-apt.sh`
deliberately does not consult it. The comment in that script citing its value as corroboration is
one of the two things this round had to retire.

**The apt version string is not, by itself, evidence either way**, and reading it as such is how
the real problem stayed hidden:

```
clang-23           1:23.1.0~++20260818083557+55feb0a3b6b7-1~exp1~20260818083714.47
clang-22           1:22.1.8~++20260714014902+ca7933e47d3a-1~exp1~20260714135019.80
```

The `~++<date>+<sha>` shape is what apt.llvm.org gives **every** build on a release branch —
including the pin this repository has been shipping on for a month. So the suffix distinguishes
nothing. **What does distinguish is the commit each names**, and that is §1b.

## 1b. The finding: apt's `clang-23` is not the release

The trailing hex in each version is the upstream commit the package was built from. Resolve both
against the release tags:

| | package built from | `llvmorg-<version>^{}` | same? |
|---|---|---|---|
| **clang-22** | `ca7933e47d3a` | `ca7933e47d3a3451d81e72ac174dcb5aa28b59d1` | **yes** — a build of the release |
| **clang-23** | `55feb0a3b6b7` | `ea7d852a70e8bdfaf601d6626a760f9771b2c4b4` | **no** |

And the commit apt built for 23 still declares itself a candidate:

```
$ curl .../llvm-project/55feb0a3b6b7/cmake/Modules/LLVMVersion.cmake
  set(LLVM_VERSION_MAJOR 23)   set(LLVM_VERSION_MINOR 1)
  set(LLVM_VERSION_PATCH 0)    set(LLVM_VERSION_SUFFIX -rc3)

$ curl .../llvm-project/ea7d852a70e8.../cmake/Modules/LLVMVersion.cmake   # the release commit
  set(LLVM_VERSION_SUFFIX)
```

It is not the `llvmorg-23.1.0-rc3` tag either (`7196f931f212…`) — it is branch state **after** rc3
and **before** the release commit. An unreleased build, which is exactly what the directive
refuses.

**Nothing on the machine says so.** Three independent reads, all clean:

```
$ clang-23 --version                 → Ubuntu clang version 23.1.0 (++20260818083557+55feb0a3b6b7-…)
$ clang-23 -dM -E - | grep version   → #define __clang_version__ "23.1.0 (++2026…)"
$ dpkg-query -W -f='${Version}'      → 1:23.1.0~++20260818083557+55feb0a3b6b7-…
```

Debian's packaging drops `LLVM_VERSION_SUFFIX`. So `setup-llvm-apt.sh`'s
`grep -qE "clang version 23\."` passed, `build.yml`'s `-dumpversion` major assertion passed, and
**every gate in §§4–8 below passed on a release candidate** — because every one of them is
major-only. That is the finding: greenness could not have told the two cases apart.

The live suite was re-fetched cache-busted on 2026-08-30 to be sure this was not a stale index:
`Date: Tue, 18 Aug 2026 15:11:28 UTC`, same version. The suite has not been rebuilt since the
release.

## 1c. The assertion this produced

`scripts/setup-llvm-apt.sh` now reads the upstream version and commit out of the installed
package's own version string, resolves `llvmorg-<version>` with one `git ls-remote` (no clone, one
ref), and requires them to match. Fail-closed and retried like the script's other network steps;
an unreachable github.com **fails** rather than assuming, because *"could not check"* and
*"checked and it is a release"* must not look alike.

Verified against both real version strings, and end to end:

```
ver=22.1.8   built=ca7933e47d3a   tag=ca7933e47d3a  => RELEASE (accept)
ver=23.1.0   built=55feb0a3b6b7   tag=ea7d852a70e8  => NOT A RELEASE (refuse)

$ ./scripts/setup-llvm-apt.sh 23   → exit 1
setup-llvm-apt: clang-23 is NOT a build of the 23.1.0 release
setup-llvm-apt:   package built from : 55feb0a3b6b7
setup-llvm-apt:   llvmorg-23.1.0 is at: ea7d852a70e8bdfaf601d6626a760f9771b2c4b4
setup-llvm-apt: apt.llvm.org is serving a release-BRANCH build for this major.

$ ./scripts/setup-llvm-apt.sh 22   → exit 0
setup-llvm-apt: clang-22 is the 22.1.8 release (built from ca7933e47d3a)
```

Both directions went through the **real script**, installing for real — not a harness. The accept
path matters as much as the refuse path: a gate that only ever says no is indistinguishable from
one that is broken.

## 2. The acquisition architecture needed no adaptation (and still does not)

Before the assertion above was added, `scripts/setup-llvm-apt.sh 23` ran **unmodified** on this Ubuntu 24.04 (noble) container — the
same distribution `ubuntu-latest` resolves to — and passed its own assertions:

- suite `llvm-toolchain-noble-23` resolved from `/etc/os-release`, as designed;
- the signing-key identity assertion (`6084F3CF…F7421`, exactly one primary key) held — upstream
  has not rotated it;
- **all three packages exist for major 23**: `clang-23`, `lld-23`, `libclang-rt-23-dev`;
- `clang-23 --version` → `Ubuntu clang version 23.1.0`, and the script's own
  `grep -qE "clang version 23\."` passed;
- the `ld.lld-23` existence check passed.

`ANABASIS_CLANG_VERSION` is the only input; nothing in the script encodes a major.

## 3. Version-coherence of the linker (no mixed majors)

`CMakeLists.txt` selects `-fuse-ld=lld` unversioned, so the question is which `ld.lld` a
`clang++-23` driver actually reaches:

```
$ clang++-23 -fuse-ld=lld -print-prog-name=ld.lld
/usr/lib/llvm-23/bin/ld.lld
```

The driver resolves to its **own** major's lld, not to a distro `ld.lld`. `check_linker_flag`
agreed: `ANABASIS_HAVE_LLD:INTERNAL=1` in the 23 configure. No mixed-major LLVM path exists.

## 4. RealtimeSanitizer — the lane that could not exist without the pin

Both tiers of the `realtime` job, run verbatim against `clang++-23`:

- **Runtime canary** (`tests/realtime_canary.cpp`, `-fsanitize=realtime`): compiled, and the
  binary **aborted at exit 43** with `ERROR: RealtimeSanitizer: unsafe-library-call — Intercepted
  call to real-time unsafe function 'malloc' in real-time context!`. The job's two assertions
  (non-zero exit; the report string present) both hold, so the lane is still able to fail.
- **Compile-time tier** (`-Werror=unknown-warning-option -Werror=function-effects`): the clean
  compile of `tests/realtime_effects.cpp` was **silent**, and the seeded canary compile failed with
  `error: function with 'nonblocking' attribute must not call non-'nonblocking' function … 
  [-Werror,-Wfunction-effects]`. Neither flag was renamed in 23.

## 5. What Clang 23 changed that this tree could feel

Read from `https://releases.llvm.org/23.1.0/tools/clang/docs/ReleaseNotes.html` before building,
so the build was a test of a hypothesis rather than a fishing trip:

- **`Removed Compiler Flags` is empty.** No flag this repository passes was withdrawn.
- **Codegen:** *"Clang now more aggressively optimizes away stores to objects after they are dead"*
  (`-fno-lifetime-dse` disables). The most consequential item for a DSP tree: it makes latent
  lifetime UB more likely to change behaviour, which is what the sanitizer and RTSan lanes are for.
- **UBSan widened:** null/alignment/array-bounds checks now run on **aggregate** copies in C, and on
  trivial copy/move in C++. A genuine chance of *new* runtime reports in `sanitizers`.
- **Sanitizer special-case lists** gained format v4, with a deprecation warning for
  non-canonicalised `./`-prefixed path matches. `scripts/ubsan-ignorelist.txt` declares no version
  header and its one rule is `src:*juce-src/*` — no leading `./` — so v1 semantics still apply.
- Language-level breaks (`break`/`continue` inside a loop's own condition; unevaluated-string
  parsing of line/module directives; `_BitInt(N)` deducing `N` as `size_t`; nested local classes in
  a different block scope) — none of which this tree or JUCE 9.0.1 was expected to hit, and the
  full build is the check.
- **LLD 23** removed the symbol-partition feature, changed `MEMORY`-region address-expression
  precedence, and renamed the default time-trace extension. This project uses none of the three.

## 6. The zero-warning contract, re-measured rather than carried over

> **What §§4–8 actually measured.** Every number below was produced by the compiler apt ships for
> major 23 — `55feb0a3b6b7`, release-branch state shortly before the release commit. It is *close
> to* 23.1.0, not 23.1.0. So these results say "this tree is ready for 23" with high confidence and
> "23.1.0 exactly was measured" not at all; the difference is the commits between `55feb0a3b6b7`
> and `ea7d852a70e8`. They will be re-run when the suite rebuilds, which is cheap because the
> recipe is written down here.


This repository has **no warning-baseline file** — the sibling's `clang-warning-baseline.txt`
was rejected in the 0.2.0 audit — so a major bump is a measurement, not an edit. The `linux` job's
own recipe, run against `clang-23`:

```
$ cmake -B build23 -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang-23 -DCMAKE_CXX_COMPILER=clang++-23 \
    -DANABASIS_BUILD_BENCH=ON -DANABASIS_BUILD_PROBE=ON
-- The C compiler identification is Clang 23.1.0
-- The CXX compiler identification is Clang 23.1.0
$ cmake --build build23 --config Release 2>&1 | tee clang23-build.log        # 170/170, exit 0
$ python3 scripts/check-clang-warnings.py --self-test
check-clang-warnings: self-test passed (18 cases).
$ python3 scripts/check-clang-warnings.py --log clang23-build.log --root . \
    --build-dir ./build23 --compiler clang++-23
check-clang-warnings: no first-party warnings (426 in vendored/other paths, not gated).
```

**Zero first-party diagnostics on Clang 23.1.0.** The 426 are all under `build23/_deps/juce-src`
— dominated by `-Wunused-template` on `juce_ElementComparator.h`'s `sortArray` /
`findInsertIndexInSortedArray` and `juce_WaveShaper.h`'s `CreateWaveShaper`, one instance per
including TU. The gate excludes `_deps`, and the 0.2.0 decision not to import a baseline file is
what makes that exclusion the *only* leniency in the contract.

Both suites, against that Release build:

```
PASS: 301 checks, 0 failure(s)          # AnabasisTests
PASS: 873 checks, 0 failure(s)          # AnabasisStateTests
```

The allocation guard reported **0 allocations over 2,040 armed `process()` calls across 80
configurations, with both counters live** — the ADR-0029 evidence, reproduced under the new major.

## 7. The linker, and why "unversioned `-fuse-ld=lld`" is not a loose end

The `linux` job's final link is the one that most deserved a look, because `CMakeLists.txt` asks for
`-fuse-ld=lld` with no major. Observed mid-link:

```
/usr/lib/llvm-23/bin/ld.lld … -plugin-opt=O3 --no-undefined --gc-sections
  -o Anabasis_artefacts/Release/VST3/Anabasis.vst3/Contents/x86_64-linux/Anabasis.so
```

The driver resolves `ld.lld` from **its own** resource directory, so `clang++-23` gets `lld-23`
without anything naming the major twice. `check_linker_flag` set `ANABASIS_HAVE_LLD:INTERNAL=1`,
and the LTO plugin link — the one ADR-0032's fallback warning exists for — completed.

## 8. The two Clang 23 risks, tested rather than reasoned about

### 8.1 UBSan's widened aggregate-copy checking — no new reports

The `sanitizers` job's exact flag set (`address,undefined,vptr,float-divide-by-zero,
implicit-conversion,local-bounds,nullability`), its ignorelist, its
`ASAN_OPTIONS=detect_leaks=1:check_initialization_order=1:strict_init_order=1:strict_string_checks=1`
and its `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`:

```
SAN_BUILD_RC=0
PASS: 300 checks, 0 failure(s)   # AnabasisTests — 300, not 301: under ASan the malloc half of the
                                 # allocation guard is compiled out and says so at the skip
PASS: 873 checks, 0 failure(s)   # AnabasisStateTests
```

`halt_on_error=1` is what makes that silence mean something: a single new aggregate-copy diagnostic
would have aborted the run rather than printed and continued. Nothing was reported, and
LeakSanitizer stayed quiet under `detect_leaks=1`.

### 8.2 Special-case-list format v4 — the deprecation warning does not apply here

Clang 23 warns on `-fsanitize-ignorelist` rules that match only non-canonicalised paths, and v5
will drop that compatibility. Measured across the sanitized configure and build:

```
--- special-case-list / ignorelist diagnostics ---
san-build.log:0
san-configure.log:0
```

`scripts/ubsan-ignorelist.txt` declares no `#!special-case-list-v4` header and its single rule is
`src:*juce-src/*` under `[implicit-conversion]` — no leading `./`, so nothing in it depends on the
behaviour v4 deprecates. **Recorded as a future item, not a present one:** if that file ever gains a
version header or a `./`-prefixed rule, v5 is where it breaks.

### 8.3 Vendored warning volume moved, and it is outside the gate

426 warnings in `_deps/juce-src` on the full Release build, 152 on the sanitized one (which
builds only the two test targets) — almost entirely
`-Wunused-template` on JUCE's header-inline templates (`sortArray`,
`findInsertIndexInSortedArray`, `absoluteTolerance`, `relativeTolerance`, `CreateWaveShaper`) plus
HarfBuzz's `hb-meta.hh`/`hb-iter.hh` functors, one instance per including TU. **First-party count
is 0 in both builds**, which is the only number the gate reads. Noted because a reviewer comparing
raw build logs across the bump will see the total move and should know where it comes from.

## 8.4 The LTO lane, both gates, on 23

`linux-lto-tests`' Clang arm verbatim — `-flto` on the compile **and** the link, then the suites run
against that codegen, under the `MALLOC_PERTURB_=1` the 0.2.6 round measured:

```
LTO_BUILD_RC=0
check-clang-warnings: no first-party warnings (152 in vendored/other paths, not gated).
PASS: 301 checks, 0 failure(s)     # AnabasisTests, LTO codegen
PASS: 873 checks, 0 failure(s)     # AnabasisStateTests, LTO codegen
```

The linker that resolved those two links, asserted directly rather than inferred:

```
$ clang++-23 -fuse-ld=lld -Wl,--version -xc++ /dev/null -o /dev/null
Ubuntu LLD 23.1.0 (compatible with GNU linkers)
```

This is the arm ADR-0033 added because GNU ld's single archive scan cannot resolve members that
LTO codegen produces afterwards — the failure mode `CMakeLists.txt`'s fallback warning describes.
It links clean on 23, with a version-matched lld.

## 8.5 The full RealtimeSanitizer suite, not just the canary

§4 proved the lane can still fail. This is the lane doing its actual job — the DSP suite built with
`-fsanitize=realtime -DANABASIS_RTSAN_LANE=1` and run with no `RTSAN_OPTIONS`, so a violation
anywhere is fatal:

```
RT_BUILD_RC=0
note: allocation guard is compiled out in this configuration
      (new=0 malloc=0 aligned=0 mallocCompiledIn=0) -- skipping the audio-path allocation assertions
PASS: 296 checks, 0 failure(s)
```

296, not 301: under RTSan the allocation guard stands down by design — its definitions would shadow
RTSan's own interceptors — and it **says so at the skip** rather than passing silently, which is the
ADR-0029 behaviour, reproduced unchanged on the new major.

## 8.6 The first-party lints

```
check-portability: self-test passed (120 cases).   check-portability: 48 file(s) scanned, 0 violation(s).
check-realtime:    self-test passed (134 cases).   check-realtime:    40 file(s) scanned, 0 violation(s).
check-docs:        self-test passed (67 cases).    check-docs:        100 file(s) clean.
check-citations:   self-test passed (37 cases).    check-citations:   50 anchor(s) unchanged vs origin/main.
check-clang-warnings: self-test passed (18 cases).
```

Every self-test ran, so none of these silences is the silence of a dead checker.

## 9. Every other pinned toolchain, audited

The directive's §2 asks the same three questions of everything else that carries a version. The
answer is short because ADR-0031's design put each version in exactly one place.

| Toolchain / tool | Pinned now | Newest stable | Source checked | Verdict |
|---|---|---|---|---|
| **Clang / LLD / compiler-rt** | major **22** (unchanged) | **23.1.0** — released, tag `ea7d852a70e8…` | `git ls-remote`, `releases.llvm.org`, `llvm.org`, `apt.llvm.org/noble/dists/` | **HELD** — the release exists; our package source ships a pre-release build of it (§1b). Moves when the suite rebuilds |
| **GCC (compatibility lane)** | major **16**, floating `gcc:16` | **16.2** released; **17.0 in development** | `gcc.gnu.org` news list; Docker Hub `library/gcc` tags | **EQUIVALENT** — already newest stable major; `gcc:16` resolves to `16.2.0` |
| **C++ standard** | **C++23** (`CMAKE_CXX_STANDARD 23`) | C++23 is the newest *published* standard | ADR-0030 | **EQUIVALENT** — and not a toolchain pin; raising it is its own ADR |
| **CMake** | none — runner image / distro apt | n/a | `.github/actions/setup-linux-build/action.yml` | **NO PIN TO MOVE** (gate rule 2); already printed in CI |
| **Ninja** | none — runner image / distro apt | n/a | same | **NO PIN TO MOVE**; already printed in CI |
| **ccache** | none — distro apt, behind a non-fatal fallback | n/a | same | **NO PIN TO MOVE**; an optimisation, never a requirement |
| **pluginval** | deliberately unpinned (latest release) | n/a | `DEPENDENCY_POLICY.md` "Current dependencies" | **UNCHANGED** — pinning it is a tracked improvement with its own decision, not this round's |
| **MSVC toolset** | none — `windows-latest` supplies it | n/a | `build.yml`'s "Record + assert the MSVC toolset" step | **CANNOT BE PINNED HERE** (gate rule 2) — detected and asserted instead |
| **AppleClang / Xcode** | none — `macos-latest` supplies it | n/a | ADR-0031 §Context (the `macos-14` → `macos-latest` precedent) | **CANNOT BE PINNED HERE** (gate rule 2) |
| **JUCE** | **9.0.1**, SHA `e18f7f5…` | out of scope | `DEPENDENCY_POLICY.md`, ADR-0028 | **UNCHANGED** — a dependency, not a toolchain; a bump has its own seven-rule procedure |
| **GitHub Actions refs** | every `uses:` SHA-pinned with a version comment | tracked by Dependabot | `DEPENDENCY_POLICY.md` §"Action refs" | **UNCHANGED** — not a toolchain, and it already has an owner. Demonstrated mid-round: PR #24 landed on `main` while this work was in flight, bumping `github/codeql-action` v4.37.7 → v4.37.8 by SHA. Touching those refs by hand would fight the mechanism that is already working. |

**The two "no pin to move" rows are not evasions.** `ARCHITECTURE_REVIEW_GATE.md` rule 2 says a
version the runner image supplies *cannot* be gated, because GitHub re-points `ubuntu-latest` with
no commit here; what the rule requires instead is detection and record, and the composite action
already prints `cmake --version` and `ninja --version` on every Linux job.

## 10. Cache invalidation would have been automatic — checked, and now moot

The pin did not move, so nothing was invalidated. Recorded because it was the first risk checked
and because it is what makes the eventual bump cheap. Every Clang ccache key embeds the major:

```
key: ccache-ubuntu-clang${{ env.ANABASIS_CLANG_VERSION }}-release-${{ github.run_id }}
key: ccache-ubuntu-clang${{ env.ANABASIS_CLANG_VERSION }}-lto-${{ github.run_id }}
key: ccache-ubuntu-sanitizers-clang${{ env.ANABASIS_CLANG_VERSION }}-…
key: ccache-ubuntu-realtime-clang${{ env.ANABASIS_CLANG_VERSION }}-…
```

so a `22 → 23` move would put every lineage in a namespace with no entries: cold once on four jobs,
warm after, and **no object compiled by 22 could ever be served into a 23 link.** This is the
property `CCACHE_COMPILERCHECK=content` would enforce anyway, arriving one layer earlier. Nothing
here needs changing when the bump lands.

## 11. Pre-existing drift found while auditing, and deliberately NOT fixed here

Reported rather than repaired — none of it is caused by or affected by this bump, and the directive
says to make no unrelated changes:

| Site | Says | Reality since |
|---|---|---|
| `src/dsp/RealtimeAnnotations.h:25` | "GCC builds the shipped Linux binary" | ADR-0032 (0.2.1) — Clang does |
| `tests/AllocationGuard.h:278` | "compiled at Release in the `linux` job (GCC) and in `linux-clang`" | `linux` is the Clang job; `linux-clang` was deleted at 0.2.1 |
| `tests/AllocationGuard.h:390` | "`linux-clang` fails on ANY …" | same |
| `scripts/preflight.sh:48` | "(CI: linux-clang)" | same |
| `scripts/check-portability.py:814` | "runs in `linux-clang`" | same |

`build.yml`'s many `linux-clang` mentions are explicitly historical ("the deleted `linux-clang`
job", "ABSORBED FROM") and are correct as written. The five above are present-tense claims about a
job that no longer exists.

## 12. What actually changed in the tree

| File | Change |
|---|---|
| `scripts/setup-llvm-apt.sh` | **The release-tag assertion** (§1c), plus the header rationale for it and the new `github.com` domain. Separately, the `llvm.sh` `CURRENT_LLVM_STABLE` citation is retired |
| `.github/workflows/build.yml` | `ANABASIS_CLANG_VERSION` **unchanged at 22**; the pin-rationale block rewritten to record why 23 waits and what the assertion covers; the measured-baseline sentence corrected from the stale `g++-14.2.0` to `clang-22.1.8 and g++-16.2.0` |
| `CMakeLists.txt` | `project(... VERSION 0.2.7)` — nothing else |
| `docs/policies/DEPENDENCY_POLICY.md` | Clang row: "the newest major apt.llvm.org ships as a RELEASE for noble", with ADR-0037's finding in the Status cell |
| `ADR-0031` | Amendment banner; clause 1's *"upstream stable"* sharpened and its sibling-parity half demoted; clause 2 gains the release-tag requirement |
| `ADR-0037` (new), `ADR_INDEX.md` | The decision and its registry row |
| `CHANGELOG.md`, `HANDOVER.md`, `DOCUMENTATION_COVERAGE.md` | 0.2.7 entry, status of record, coverage scope |

**Deliberately not changed:** `docs/procedures/BUILD.md` still says a `clang-22` build, which is
now correct rather than stale. Every ADR/worklog sentence that records a *measurement* taken on
clang-22 (`ADR-0029` §Evidence, `ADR-0030`'s warning table, `ADR-0032`'s ABI floor, `ADR-0033`'s
`clang-22 -flto`, `check-realtime.py`'s two "compiles clean under clang-22 and gcc-13" notes) is
untouched — those are dated records, not claims about the current pin.

## 13. Residuals — stated so this is not read as full cover

1. **The directive was not fully satisfiable.** It asked for 23.1.0 and forbade unreleased builds;
   our package source offers only the second kind for major 23. The pin did not move. What moved is
   the repository's ability to know the difference.
2. **§§4–8 measured the rc3-era branch build, not 23.1.0.** See the note at §6. They are strong
   evidence the tree is ready for 23 and are not a measurement of the release.
3. **The assertion adds `github.com` to a fail-closed script.** A github outage now fails the Clang
   jobs. That is the same trade the script already makes for apt.llvm.org, made deliberately: an
   unverifiable compiler must not install silently. Cost: one `git ls-remote`, no clone.
4. **The assertion has not run on a CI runner yet** — only here, in both directions. The first CI
   run is what proves it there.
5. **Only the Linux Clang lanes were exercised.** Windows (MSVC) and macOS (AppleClang) do not use
   this pin; the `linux-lto-tests` GCC arm runs in a `gcc:16` container this environment has no
   Docker for, and its compiler did not move.
6. **When the suite rebuilds**, re-run §§4–8 against the real 23.1.0 before flipping the pin — the
   recipe is in this file, and the assertion will refuse the bump until upstream is ready anyway.
