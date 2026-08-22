# ADR-0034 — CI/toolchain parity with the sibling: the GCC container, the ccache contract, and the sanitizer depth

> **✅ THE ARCHITECTURE REVIEW GATE IS CLEARED (2026-08-22).** A **Build System change** — a compiler
> major, its acquisition method, and the configuration around both — under
> `ARCHITECTURE_REVIEW_GATE.md` §"Compiler and toolchain versions" rules 1 and 3. Taken on the
> owner's directive for this round: *Anabasis should inherit the complete CI/toolchain setup from
> Anamorph unless there is a concrete reason why Anabasis must differ.*

**Status:** **Accepted — 2026-08-22.** Version 0.2.2. Amends **ADR-0033**; pairs with **ADR-0032**.

## Context

0.2.1 migrated the Linux release *topology* (ADR-0032) and added the LTO lane (ADR-0033). This round
asked whether the migrated workflow is actually **equivalent** to the sibling's CI architecture, area
by area. It is not a version bump: the audit read both `.github/` trees, both composite actions, both
`setup-*.sh` scripts and the toolchain half of both `CMakeLists.txt`.

**What was already equivalent** (so the previous rounds did land it): every pinned GitHub Action SHA
across all five workflows, all runner images, triggers, permissions, concurrency, the C++ standard
block, CMake/Ninja acquisition, the entire Clang install path (`setup-llvm-apt.sh` differs only in
prose), the shipped-artifact LTO configuration and the lld probe.

**What was not**: the configuration *around* the toolchain — twelve mismatches, listed in
`worklogs/2026-08-22-ci-toolchain-parity-audit.md` with a verdict each. This ADR records the four
that are decisions rather than transcriptions.

## Decision

### 1. GCC moves 14 → 16, **through the official image**, with the `headless` profile

`linux-lto-tests` runs `container: gcc:16`; `ANABASIS_GCC_VERSION: 16` is the authority and the job
asserts the container's major against it from `-dumpversion` before building.

**0.2.1's reasoning is reversed here, explicitly.** It pinned `g++-14` from Noble's archive and
argued that the sibling's container was "reasoning about GCC 16 specifically". Re-read, the sibling's
comment says the opposite: no apt source ships a *released* g++-16 — Noble stops at 14, and both
`ubuntu-toolchain-r/test` and Ubuntu 26.04 carry trunk snapshots predating 16.1 — so the image is the
only package-managed route to a released 16. Choosing 14 because apt has 14 is choosing the version
to fit the acquisition method.

`dependency-profile: headless` comes with the container and is not optional: inside `gcc:16`,
`build-essential` would install a distribution GCC over the pinned one and silently un-pin the lane.
This **re-opens and reverses audit item A2-34**, which was NOT NEEDED twice on the condition that the
GCC lane was not containerised — a condition this decision removes.

**The tag is literal (`gcc:16`) because `container:` cannot read the `env` context.** The assertion
step is what keeps the two in agreement; a tag edited without the env fails in seconds.

### 2. The LTO lane becomes two jobs

`container:` is a per-job key, so a matrix cannot carry one containerised arm and one bare arm.
`linux-lto-tests` is the GCC container job (the sibling's job, matched in name, shape and steps);
`linux-lto-clang` is the Clang arm ADR-0033 added for INC-004, unchanged in purpose. Two jobs are
also two independent results.

### 3. The ccache contract is adopted whole, and macOS is cached

`CCACHE_DIR=${{ github.workspace }}/.ccache`, `CCACHE_MAXSIZE` per job (400M–1G) and
`CCACHE_COMPILERCHECK=content` on every caching job. Anabasis previously set **none** of the three
and cached `~/.cache/ccache`, i.e. ran on ccache's defaults: an effectively unbounded cache, `mtime`
compiler identity — which can serve an object built by a *different* compiler that shares a
timestamp, defeating the pin — and a home-relative path that is the wrong place inside a container.

**The macOS jobs are now cached, and the objection that kept them uncached was refuted rather than
outvoted.** This workflow claimed `dsymutil`'s debug-map walk made caching unsafe. A cached object is
a real `.o` at the path the linker recorded, so the walk reads the same files either way; the sibling
verified that, and verified that ccache hashes the full `-arch` list so a universal object cannot be
served to a thin build. It also measured the equivalent job as the run's critical path — 29m44s, of
which 16m40s is the build step.

*(Amended 2026-08-22, 0.2.4 — the universal build's cache is now measured HERE.)* A review asked the
question this ADR should have asked itself: ccache has historically **refused** to cache a
compilation carrying more than one `-arch`, and `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"` produces
exactly that — one clang invocation with both, not two invocations. Had that still held, the cache
would have been inert and the restore/save pure cost. It does not hold. `macos-latest` installs
**ccache 4.13.6**, and the job's own statistics say so across three consecutive runs:

| Run | Cache state | Cacheable calls | Hits | Build step |
| --- | --- | --- | --- | --- |
| 32563814120 | cold (first cached run) | **182 / 182 (100.0%)** | 26 / 182 (14.29%) | **630.6s** |
| 32565784751 | warm | **182 / 182 (100.0%)** | 174 / 182 (95.60%) | **233.5s** |
| 32568563583 | warm | **182 / 182 (100.0%)** | 172 / 182 (94.51%) | 370.0s |

`182 / 182` is the whole answer: ccache classifies **every** two-`-arch` compilation as cacheable and
declines none. There is no `Uncacheable` bucket in any of the three logs — an absence confirmed by
keyword sweep, not inferred from silence. Cold → warm is **397s (63%)** off the build step at an
unchanged object count of 182, so the drop is cache hits rather than less work.

Two honesties the amendment adds. First, **ccache reaches only the compile half**: its counters stop
moving ~46s into the 233.5s warm step, and the remainder is the LTO link, which ccache does not
cache. Second, **the 29m44s / 16m40s figures above are the SIBLING's job**, and this ADR quoted them
as if they sized the local one. Measured here, `macos` is genuinely the run's critical path at
**18m43s** — but **12m34s (67%)** of that is the four pluginval passes and only **3m53s (20.7%)** is
the build step ccache acts on. 16m40s overstates this repository's build step by roughly 4×. The
decision stands on 397s of real saving on the longest job in the matrix; the *magnitude* argument
that carried it here was imported, and is now replaced by local measurement.

**The measurement gap that made this hard to answer, and is now closed for macOS.** `macos-intel` —
the single-arch control, differing from `macos` in exactly one variable — restored a ccache and then
never reported what it did, so the obvious comparison could not be made from the logs. It now has a
`Compiler cache statistics` step. `sanitizers` and `realtime` have the same gap and were left alone:
they are Linux jobs, outside the question that prompted this. A cache whose hit rate is unobservable
can be neither defended nor retired on evidence, which is the "a gate that cannot fail is
indistinguishable from one that passes" failure applied to a measurement.

### 4. The sanitizer set is adopted **minus one sub-check**, measured

`sanitizers` goes from `address,undefined` to
`address,undefined,vptr,float-divide-by-zero,implicit-conversion,local-bounds,nullability` behind
`scripts/ubsan-ignorelist.txt` (one sub-check, one vendored tree, no first-party path), with the
sibling's `ASAN_OPTIONS` including **`detect_leaks=1`**.

**`unsigned-shift-base` is deliberately excluded.** Measured, it fires exactly once, on the dither
RNG's xorshift (`rngState ^= rngState << 13` on a `uint32_t`, `AnabasisEngine.cpp:1013`). That shift
is *well defined* — unsigned left shift wraps modulo 2³² — and the wrap **is** the algorithm. The
check is explicitly a not-UB check for shifts that are *often* unintentional. The alternatives were
worse: an ignorelist entry would exempt first-party code and would have to exempt the whole enclosing
`processChunk`; and the generator is frozen by `DSP_POLICY` (offline renders repeat exactly), so
rewriting it is a hard-stop item, not a sanitizer accommodation.

**The sanitized run raises the stack to 64 MB**, and that is part of the gate rather than a
workaround: `testTeardownAndReengageInvariants` constructs four `AnabasisAudioProcessor` objects in
four scopes, and ASan's use-after-scope instrumentation gives each its own slot plus redzones instead
of reusing one. Measured: 8 MB overflows, 64 MB passes.

**`detect_leaks=1` retires an assumption.** The previous `detect_leaks=0` carried a comment about
JUCE singletons being reported at exit. Measured: nothing is reported.

## Consequences

- **The compatibility compiler is two majors newer** and is now a *released* one rather than the
  newest an archive happened to carry. It is also a **different distribution** (Debian, in the
  image) — which widens what the lane can catch, and is the point of having it.
- **CI has never run the container lane**, and it cannot be run here: this environment has no
  container runtime. The first push is the measurement. If `g++-16` rejects something this tree
  compiles under 14, that is the lane reporting a real portability finding — and the pin is one env
  line plus one tag to move back.
- **The container's DISTRIBUTION is part of this decision, and the review round found a defect in
  it.** `gcc:16` is `FROM buildpack-deps:trixie` — Debian 13, not Ubuntu — and package names are not
  a constant across the two. `scripts/setup-linux.sh` installed **`libfreetype6-dev`**, which trixie
  ships neither as a real package nor as a virtual one; the lane would have died at dependency
  install. It worked on the runners only because Ubuntu noble's `libfreetype-dev` carries
  `Provides: libfreetype6-dev` — a compatibility name one distribution has already dropped. The
  scripts now name `libfreetype-dev`, real on both. Two consequences worth keeping: a container lane
  makes the package list a **portability surface**, and the script therefore prints the distribution
  it resolved the names on (review-gate rule 2, detect and record).
- **The web-browser binding moved to the `full` profile**, on evidence rather than to dodge a name:
  every target sets `JUCE_WEB_BROWSER=0`, JUCE gates its webkit include on that macro, and JUCE
  9.0.1 declares no `linuxPackages` for `juce_gui_extra` (only `alsa`, `freetype2 fontconfig` and
  `egl gl`, in three other modules). It is not a compile dependency of this project, it is the
  heaviest entry in the list, and it is the most volatile name in it — `libwebkit2gtk-4.0-dev` is
  already gone from trixie and the successor there is `libwebkitgtk-6.0-dev`. `full` keeps it so a
  developer flipping the macro does not face a second dependency hunt.
- **The GCC arm's warning gate is now measured at 16 — the pin held, and the gate was never the
  problem.** *(Amended 2026-08-22, after the container lane ran.)* The lane was kept strict on the
  reasoning below even though 16 was unmeasured; run **32565784751** measured it. Every one of the
  **13 first-party translation units** in the two suites — `tests/dsp_tests.cpp`,
  `tests/state_tests.cpp`, `src/dsp/AnabasisEngine.cpp` (in both targets), `src/PluginProcessor.cpp`,
  `src/PluginParameters.cpp`, `src/MacroEngine.cpp`, `src/PresetManager.cpp` and all six
  `src/gui/*.cpp` — was compiled by **g++ 16.2.0** under the full
  `juce_recommended_warning_flags` set at `-O3 -flto -std=c++23`, and **not one emitted a
  diagnostic**. Two majors, a different distribution, and the zero-warning policy did not move. It
  stays strict: ADR-0031's rule is that a pin which surfaces diagnostics gets them **fixed**, not
  baselined, and there was nothing to fix.

  The evidence survives the obvious objection, which is worth stating because 43 of that run's 47
  compiles were ccache hits. **ccache replays stored stderr on a hit** — measured directly here, the
  same `-Wunused-variable` printed on the miss and on the hit — and `CCACHE_COMPILERCHECK=content`
  means a hit requires the byte-identical compiler, so a replayed diagnostic is still g++ 16.2.0's.
  A warning would have appeared either way.

  **The link phase is now measured at 16 as well — the remaining cell is closed.** *(0.2.4.)* The
  0.2.3 statement of this limit was that the failing run aborted at 36/56 before the **LTO link**,
  where GCC emits `-Wodr` and `-Wlto-type-mismatch` — the cross-TU class this lane exists for — so
  the link was measured only locally, at 14.2.0. Run **32568563583**, the first run after the
  `libxi-dev` fix, completed the lane: `linux-lto-tests` **succeeded**, both links ran ([54/56],
  [55/56]), and the gate printed `check-clang-warnings: no first-party warnings (0 in
  vendored/other paths, not gated)`. The entire job log contains **two** `warning:` lines, both
  `lto-wrapper: warning: using serial compilation of N LTRANS jobs` (N=5, N=101) — precisely the
  location-less driver form 0.2.3 pinned as a non-diagnostic, now observed in the wild rather than
  hypothesised. Both suites then passed against that codegen: **301 + 873, 0 failures**. So GCC
  16.2.0 is clean through **compile and link**, and the zero-warning policy needs no qualifier.
- **A missing header, not a warning, is what actually failed the lane — and the previous round's
  package verification could not have caught it.** `juce_gui_basics.h:393` includes
  `<X11/extensions/XInput2.h>` whenever `JUCE_USE_XINPUT` is set, and JUCE 9.0.1 defaults it to 1,
  so the include is unconditional in practice. That header belongs to **`libxi-dev`**, which
  `CORE_PACKAGES` never named. On `full` it arrived anyway, as a transitive dependency of
  `libgtk-3-dev` (`Depends: libxi-dev`) — the very package this ADR moved to `full` on the grounds
  that nothing here compiles webkit. Correct on its own terms, and it took the X-input headers with
  it: three JUCE translation units died at `fatal error: X11/extensions/XInput2.h: No such file or
  directory`.

  The methodological finding is the durable part. This ADR's evidence block verified that **every
  package name resolves** on trixie and on noble. That is a different question from whether the
  **set is sufficient**, and only the first was asked. The check that answers the second is the one
  now on record: enumerate every system header the vendored tree includes
  (`grep -rhoE '<(X11|GL|EGL|freetype2?|fontconfig|alsa|xcb)/[^>]+>' modules/`), map each to its
  owning package with `dpkg -S`, and diff that against the declared list. Run against JUCE 9.0.1 it
  yields fifteen headers across eight packages, of which `libxi-dev` was the single omission —
  every other one was already named explicitly, and `x11proto-dev` (which owns `X11/Xmd.h`) is a
  hard `Depends:` of `libx11-dev`. `libxi-dev` is now explicit on **both** profiles: depending on a
  GUI toolkit we do not compile to supply a header we do compile is exactly the accident an
  explicit list exists to prevent.
- **macOS caching changes the critical path**, which is the largest CI-time effect in this round —
  and it changes nothing a user receives: the `lipo` slice assertion and the dSYM contract are
  unmoved.
- **The sanitizer job is materially stronger**: six new sub-checks and four new ASan runtime options,
  at the cost of one sub-check the sibling has and one `ulimit` line it does not need.
- **What this does NOT change**: no DSP algorithm, parameter, serialization schema, threading model
  or reported latency; no first-party C++ source at all.

## Related code
- `.github/workflows/build.yml` (`env.ANABASIS_GCC_VERSION`; `linux-lto-tests`, `linux-lto-clang`,
  `sanitizers`, `macos`, `macos-intel`, `windows`)
- `.github/actions/setup-linux-build/action.yml` (`dependency-profile`)
- `scripts/setup-linux.sh` (`full` / `headless`), `scripts/ubsan-ignorelist.txt`

Evidence [Verified, except where stated]:
- Source: `.github/workflows/build.yml`
- Test:   both suites under the adopted sanitizer set — **300 + 873, 0 failures, exit 0**, with
  `detect_leaks=1` and a 64 MB stack; the excluded sub-check and the stack limit each measured in
  both directions
- **GCC 16.2.0 warning baseline: 0 first-party diagnostics over 13/13 translation units**, from CI
  run 32565784751's `linux-lto-tests` job (compile phase). ccache stderr replay verified locally in
  both directions, so cache hits do not weaken the reading
- **GCC LTO link phase: 0 first-party diagnostics**, both suites, `g++-14 14.2.0` with `-flto` on
  compile and link, gate run over the complete log
- The gate itself re-verified against real GCC output, not only its synthetic self-test: a fixture
  carrying a GCC `-Wunused-variable`, a GCC LTO-time `-Wodr` and a vendored diagnostic classifies
  2 first-party / 1 vendored and exits 1. `-Wodr` and `-Wlto-type-mismatch` were generated from
  purpose-built two-TU cases to confirm GCC's link-time diagnostics carry the `path:line:col:`
  form the matcher requires — they do
- **Verified in CI, still not locally:** the `gcc:16` container lane ran end to end and **passed** in
  run 32568563583 — build, LTO link, warning gate and both suites (301 + 873, 0 failures). It
  remains unrunnable in this environment (a `docker` client with no reachable daemon), so the
  measurement is CI's, not a local one. Its PACKAGE SET is verified two ways: every name in
  both profiles resolves against the Debian trixie and Ubuntu noble archives (with known-absent
  negative controls), and the declared set is now checked for SUFFICIENCY as well as resolvability
  by the header→package enumeration described above
- Worklog: `worklogs/2026-08-22-ci-toolchain-parity-audit.md`
