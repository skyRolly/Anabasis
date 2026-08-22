# ADR-0032 — Clang builds the shipped Linux artifact; GCC becomes the compatibility compiler

> **✅ THE ARCHITECTURE REVIEW GATE IS CLEARED (2026-08-22).** A **Build System change** — which
> compiler produces a shipped binary — under `ARCHITECTURE_REVIEW_GATE.md` §"Compiler and toolchain
> versions". Taken on the owner's directive for this round, which reopened audit item **A2-31** and
> replaced the test it was decided under: *the two products stay aligned on engineering
> infrastructure unless there is a concrete technical reason to differ.*

**Status:** **Accepted — 2026-08-22.** Version 0.2.1.

## Context

Until this ADR the Linux topology was:

| Job | Compiler | What it produced |
|---|---|---|
| `linux` | whatever `ubuntu-latest` resolved `g++` to (13.3.0) | **the artifact users install** |
| `linux-clang` | clang-22, pinned by ADR-0031 | a second Release build, thrown away |

Nothing decided that. `linux` is GCC because that is what a bare `cmake` picks on the runner image;
`linux-clang` was added in 0.1.3 for the AppleClang diagnostic set (INC-003) and grew the warning
gate, the portability canary and both reproductions around it.

The 0.2.0 migration audit looked at the sibling's opposite arrangement — Clang ships, GCC validates
— and recorded **A2-31: NOT NEEDED**, on the grounds that it was "a release-topology decision with
no reported problem behind it here". That was a correct answer to the question as posed. This round
poses a different one.

## Problem

Three consequences of the old topology, in descending order of how much they matter:

1. **The shipped Linux binary was built by an unpinned compiler.** ADR-0031 pinned the compiler that
   *gates diagnostics*; nothing pinned the one that produced `Anabasis.so`. A runner-image bump
   changed the shipped codegen with no commit here — the exact exposure ADR-0031 exists to close,
   left open on the artifact. `check-linux-abi.py` bounds that artifact's **symbol floor**, which is
   a different property from its optimiser.
2. **The two instruments aimed at INC-004 validated a build nobody receives.**
   `AnabasisEngineRepro` and the `--assert-discriminating` channel probe ran against `build-clang`.
   Pointing them at Clang was right — INC-004 (KI-009) was undefined behaviour only Clang at `-flto`
   acted on — but under the old topology they proved it about a discarded binary, while the binary
   users install was covered by pluginval and the probe in `linux` alone.
3. **Two full Release builds of the same tree on every push**, differing only in compiler.

## Options

- **A. Keep GCC shipping and pin it.** Rejected. It closes (1) and nothing else, keeps the duplicate
  build, and leaves the reproductions aimed at a discarded artifact. It also diverges from the
  sibling for no reason anyone could state.
- **B. Ship Clang, delete `linux-clang`, move GCC to a compatibility lane.** Chosen. It closes all
  three: the artifact is built by the already-pinned major, the reproductions land on the shipped
  bundle, and the second full build becomes a suites-only lane that answers a question the first one
  cannot (`ADR-0033`).
- **C. Ship both.** Rejected: two Linux artifacts is a support surface, not a validation strategy,
  and nothing has asked for it.

## Decision

**Clang builds the shipped Linux artifact. GCC keeps a job as the compatibility compiler.**

1. `linux` installs the pinned Clang through the composite action and configures with
   `-DCMAKE_C_COMPILER=clang-<n> -DCMAKE_CXX_COMPILER=clang++-<n>`. It builds, strips, self-tests,
   reproduces, hosts, validates (pluginval ×3 in both modes), stages, uploads and asserts the ABI
   floor of that build.
2. `linux-clang` is **deleted**. Its three genuinely-own steps move into `linux`: the portability
   compile canary, the warning-gate self-test + first-party warning gate, and `AnabasisEngineRepro`.
3. **The warning gate runs LAST**, after the artifact uploads. A diagnostic finding must not withhold
   a beta artifact whose behavioural gates passed; it still fails the job. Same rule the ABI
   assertion already follows.
4. `merge-check` moves to the same compiler and shares one ccache lineage
   (`ccache-ubuntu-clang<n>-release-`) with `linux`. A merge-check on a different compiler from the
   one that will build the merge result is a check of a tree nobody ships — and a second lineage
   inside one cache budget only evicts the first.
5. **GCC is not dropped.** It is pinned (`ANABASIS_GCC_VERSION`) and becomes the compatibility
   toolchain in `linux-lto-tests` (ADR-0033). A second major toolchain is worth keeping *because* it
   disagrees — it accepts, rejects and warns about different code — and that value survives only
   while it is still compiled on every push.

## Consequences

- **The shipped Linux codegen is now pinned**, and moving it is an Architecture Review Gate change
  like any other compiler pin.
- **`lld` becomes load-bearing on the release path.** `setup-llvm-apt.sh` installs `lld-<n>` beside
  the compiler, and `CMakeLists.txt` probes `-fuse-ld=lld` for Clang; the plugin's
  `juce_recommended_lto_flags` link is the one that needs it (a one-pass GNU-ld archive scan against
  post-scan LTO codegen is the failure this avoids). The probe still falls back with a warning rather
  than refusing to configure.
- **The ABI floor is a property of the new compiler and was re-measured, not assumed.** Measured on
  the clang-22 build of this tree: `GLIBC 2.38 · GLIBCXX 3.4.31 · CXXABI 1.3.9` — the same three
  floors `check-linux-abi.py` already declares, so the file needed no change and the compatibility
  claim in `COMPATIBILITY_MATRIX.md` is unchanged.
- **One fewer full Release build per push**, replaced by a suites-only lane (ADR-0033).
- **GCC's coverage narrows from "the whole product" to "the whole first-party source".** The two
  suites compile every first-party translation unit between them, so nothing in `src/`, `tests/` or
  `tools/` stops being GCC-compiled; what GCC no longer builds is the plugin *link* and the
  standalone wrapper. Stated rather than glossed: a GCC-only defect in the JUCE plugin-client layer
  would now be invisible here. Judged acceptable because that layer is a dependency's, not this
  project's, and because nothing this repository ships is built with it any more.
- **What this does NOT change:** the parameter surface, the serialization schema, the threading
  model, DSP signal order and reported latency are all untouched, and the artifact layout, staging
  self-checks and upload gating are identical.

## Related code
- `.github/workflows/build.yml` (`env.ANABASIS_CLANG_VERSION`, `env.ANABASIS_GCC_VERSION`; jobs
  `merge-check`, `linux`, `linux-lto-tests`)
- `.github/actions/setup-linux-build/action.yml`
- `CMakeLists.txt:196-208` (the `-fuse-ld=lld` probe)
- `scripts/setup-llvm-apt.sh`, `scripts/check-clang-warnings.py`, `scripts/check-linux-abi.py`

Evidence [Verified]:
- Source: `.github/workflows/build.yml`
- Test:   both suites, `AnabasisEngineRepro`, `AnabasisChannelProbe --assert-discriminating`,
  pluginval ×3 in both modes and `check-linux-abi.py`, all run locally against the clang-22 build
- Worklog: `worklogs/2026-08-22-lto-lane-and-linux-toolchain-alignment.md`
