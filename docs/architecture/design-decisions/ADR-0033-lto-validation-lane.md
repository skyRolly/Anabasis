# ADR-0033 — `linux-lto-tests`: the suites run against shipped-class codegen, on both major toolchains

> **✅ THE ARCHITECTURE REVIEW GATE IS CLEARED (2026-08-22).** A **Build System change** — a new CI
> lane with a pinned compiler — under `ARCHITECTURE_REVIEW_GATE.md` §"Compiler and toolchain
> versions". Taken on the owner's directive for this round, which reopened audit item **A2-32** and
> withdrew the premise it was deferred under.

**Status:** **Accepted — 2026-08-22.** Version 0.2.1. Pairs with **ADR-0032**. **Amended by [ADR-0034](ADR-0034-ci-toolchain-parity.md) (0.2.2)**: the GCC arm moved to `g++-16` in the official `gcc:16` container, and — because `container:` is a per-job key — the two arms became two jobs (`linux-lto-tests` for GCC, `linux-lto-clang` for Clang). The two questions the lane answers are unchanged; §"GCC is pinned from apt, not from a container" below is the clause that was reversed, and it is kept as the record of what this ADR decided.

## Context

By ADR-0008 the two console suites do **not** link `juce_recommended_lto_flags`; the plugin does.
Everything this repository asserts about behaviour — 1,174 checks, the sanitizer builds, valgrind,
RealtimeSanitizer — therefore ran against **non-LTO objects**, while the artifact a user installs is
LTO'd.

That gap has a name here. **INC-004 / KI-009** was undefined behaviour in the engine's channel loop
that only Clang, and only at `-flto`, acted on: for five months every console-target gate was green
while the shipped bundle dropped a channel. `linux` closes half of it — since ADR-0032 it builds,
hosts and validates the LTO'd plugin with the two reproductions aimed at that build. The other half
is the **assertions**, which nothing runs against LTO codegen.

The 0.2.0 audit measured this as **A2-32** and recorded: *technically valuable · passes identically ·
deferred only because of CI cost*, with numbers — the two suites take **172 s** under `clang-22
-flto` against **27 s** without, and the binary falls from 14.07 MB to 5.44 MB. The owner's brief for
this round states that GitHub Actions cost is not a constraint for this project, which removes the
only term in that verdict that argued against.

## Problem

Two questions need a job, and they are not the same question:

1. Do the assertions still hold when the optimiser is allowed to work **across translation units** —
   the configuration INC-004 needed in order to manifest?
2. Does the tree still compile and pass under the **other major toolchain**? After ADR-0032 moved the
   shipped build to Clang, no job in this workflow builds with a GCC this repository chose. (The
   `sanitizers` job's unsanitized `build-vg` copy still uses the image's default `g++`, unpinned and
   incidental to the memcheck run — the same arrangement the sibling has. That is a compiler the
   runner picked, not a compatibility statement this repository can make.) An unbuilt second
   toolchain stops being a compatibility guarantee within one release.

## Options

- **A. One GCC arm, exactly as the sibling has it.** Rejected as insufficient *here*: it answers (2)
  and not (1), and (1) is the incident this repository actually had.
- **B. One Clang arm.** Rejected: answers (1) and drops GCC entirely.
- **C. A two-arm matrix, `clang` and `gcc`, both `-flto`.** Chosen. Each arm answers one question,
  `fail-fast: false` keeps one from hiding the other, and the marginal cost over (A) is one runner on
  a project where runner cost is not a constraint.
- **D. Add `-flto` to the suites everywhere instead of adding a lane.** Rejected. It would make every
  other gate slower (172 s against 27 s on this hardware), and it would DESTROY a property the
  sanitizer and valgrind lanes depend on: those tools want the un-inlined, un-merged code that makes
  their reports name a real function.

## Decision

**A `linux-lto-tests` job, on every push, with two matrix arms.**

| Arm | Compiler | Question |
|---|---|---|
| `clang` | `clang-<ANABASIS_CLANG_VERSION>` `-flto` | do the assertions hold against the **shipped optimization class**? |
| `gcc` | `g++-<ANABASIS_GCC_VERSION>` `-flto` | does the tree compile and pass whole-program under the **other major toolchain**? |

1. `-flto` is set on `CMAKE_C_FLAGS`, `CMAKE_CXX_FLAGS` **and** `CMAKE_EXE_LINKER_FLAGS`. Both halves
   are required and they are not the same switch: the compile flag emits bytecode, the link flag runs
   the optimiser over the whole program.
2. It builds `AnabasisTests` + `AnabasisStateTests` only, with `ANABASIS_BUILD_STANDALONE=OFF`.
   Between them those two compile **every first-party translation unit**
   (`AnabasisStateTests` takes `ANABASIS_PLUGIN_SOURCES`; both take `src/dsp/*.cpp` through the
   `AnabasisDSP` INTERFACE library), so first-party coverage is complete while the plugin link — the
   slowest in the tree, and already LTO'd and validated in `linux` — is not repeated.
3. **The compiler major is asserted before the build**, from `-dumpversion`, major only: a patch move
   must pass and a wrong major must fail in seconds rather than at the end of a warning gate.
4. **Each arm gets its own ccache lineage** (`ccache-ubuntu-<toolchain><major>-lto-`). `-flto` objects
   are not native objects — GIMPLE bytecode under GCC, LLVM bitcode under Clang — so they share no
   entries with `linux`'s, and one shared lineage would only make the two evict each other.
5. **The first-party warning gate runs on both arms**, at zero, with no baseline file.
   `check-clang-warnings.py` matches the `path:line:col: warning:` shape both compilers emit and
   classifies by resolved path, so the only Clang-specific thing about it is its name. For the Clang
   arm this nearly duplicates `linux`'s gate and is kept because it costs a `tee`; for the GCC arm it
   is the **only** warning gate this repository has.

### GCC is pinned from apt, not from a container — *reversed by ADR-0034*

The sibling pins `gcc:16` through a container image, and its own comment records why: no apt source
ships a released g++-16 — only trunk snapshots, which is the "newest, not stable" a toolchain pin
exists to refuse. **That reasoning is about GCC 16 and does not transfer to a major that is
packaged.** `g++-14` is 14.2.0 in Noble's own archive, so the composite action's existing
`extra-packages` input installs it with no new input, no container, no image tag floating on a major,
and no second dependency-installation path to keep in agreement.

This is also what keeps audit item **A2-34** (the sibling's `dependency-profile` split) NOT NEEDED
for its original measured reason: that split exists to stop `build-essential` installing a
distribution compiler over a container's pinned one, and this lane is not containerised.

> **ADR-0034 reversed both halves of this section (0.2.2).** The 0.2.2 parity audit found the
> argument above inverted: the sibling's container exists *because* no apt source ships a released
> g++-16, so "apt has 14, therefore pin 14" chose the version to fit the acquisition method. The lane
> is containerised now, which also makes A2-34's condition true and brings the `headless` profile
> with it.

## Consequences

- **Measured cost, on this machine (4 cores, cold ccache, JUCE prebuilt via `ANABASIS_JUCE_PATH`):**
  see the worklog's table — the LTO arms are several times the non-LTO build of the same targets, and
  the lane runs in parallel with every other job, so the run's **wall clock** is unchanged as long as
  the lane is not the longest job in the matrix (it is not; `macos` is).
- **ccache helps this lane less than it helps the others**, and that is inherent rather than a
  misconfiguration: ccache caches the *compile* step, and LTO moves most of the work into the *link*.
  The measured hit rate is real but the saving is bounded by the link, which is never cached.
- **A new way for CI to go red that is not a defect in this tree**: an optimiser bug, or a
  latent-UB-dependent transformation, in either compiler. That is the lane working — it is what
  INC-004 looked like from the outside — and the ADR states it so a future reader does not treat a
  red arm as noise.
- **`ANABASIS_BUILD_BENCH` is deliberately NOT set here.** `linux` compiles the bench on every push
  (and, since ADR-0032, under the pinned Clang with the shipped LTO flags), which is the anti-rot
  cover the sibling's LTO lane provides for its own. Building it here too would compile it twice per
  push to answer one question.
- **The lane asserts no timing** and never will: `PERFORMANCE_BUDGET.md` owns why a shared runner's
  numbers must not be quoted.

## Related code
- `.github/workflows/build.yml` (job `linux-lto-tests`; `env.ANABASIS_GCC_VERSION`)
- `.github/actions/setup-linux-build/action.yml` (`extra-packages`)
- `scripts/check-clang-warnings.py`
- `CMakeLists.txt:50-73` (`ANABASIS_BUILD_STANDALONE`, `ANABASIS_JUCE_PATH`)

Evidence [Verified]:
- Source: `.github/workflows/build.yml`
- Test:   both suites built with `clang-22 -flto` and with `g++-14 -flto` and run locally; results
  compared against the same suites without LTO under both compilers
- Worklog: `worklogs/2026-08-22-lto-lane-and-linux-toolchain-alignment.md`
