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
- **The GCC arm's warning gate is measured at 14, not at 16.** It gates first-party warnings at zero
  through `check-clang-warnings.py`. Under `juce_recommended_warning_flags` for GCC that measured
  zero at 14.2.0; 16 is unmeasured. Kept strict deliberately: ADR-0031's rule is that a pin which
  surfaces diagnostics gets them **fixed**, not baselined.
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
- **Unverified locally:** the `gcc:16` container lane (no container runtime in this environment)
- Worklog: `worklogs/2026-08-22-ci-toolchain-parity-audit.md`
