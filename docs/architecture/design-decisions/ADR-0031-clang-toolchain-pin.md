# ADR-0031 — The Linux Clang major is pinned, and the gate it feeds keeps its zero-warning contract

> **✅ THE ARCHITECTURE REVIEW GATE IS CLEARED (2026-08-22).** A **Build System change** — a pinned
> compiler version, installed from a third-party package source — under the rule this same round
> added to `ARCHITECTURE_REVIEW_GATE.md` §"Compiler and toolchain versions": *a version this
> repository pins is gated.* Taken on the owner's migration directive, which asked in as many words
> to "align toolchain versions and dependency versions with Anamorph".

**Status:** **Accepted — 2026-08-22.** Version 0.2.0. **Amended by [ADR-0032](ADR-0032-linux-release-toolchain.md) (0.2.1)**, which moved the shipped Linux build onto this pin and pinned GCC beside it; decision clauses 3 and 5 carry the amendment inline.

## Context

`linux-clang` gates on **zero first-party warnings** — a stricter contract than the sibling
product's, which absorbs a debt list in `clang-warning-baseline.txt`. Until this ADR the compiler
that defined that bar was whatever `ubuntu-latest` resolved to that week.

That is a real exposure, not a theoretical one. Which diagnostics Clang emits is a property of the
**major version**: `-Wshadow-field`, `-Wsign-conversion`, `-Wmissing-prototypes` and
`-Wunused-but-set-variable` have all moved between majors. A runner-image bump could therefore turn
the gate red on a push that changed nothing in the tree — or, worse, quietly green with fewer
diagnostics. This repository has already lived the same shape once on the other platform, when
`macos-14` floated to `macos-latest` and AppleClang moved from 15 to 21.

ADR-0029 adds a second, harder requirement: RealtimeSanitizer arrived in Clang 20, and Ubuntu's
archives for `noble` — what `ubuntu-latest` resolves to — stop at clang-20 with no
`libclang_rt.rtsan` for a newer major. Without a pin there is no `realtime` job at all.

## Problem

Ubuntu's archive boundary is a fact about the distribution, not about this project, and the upstream
stable major is two ahead of it. Following the image means holding the warning gate and the
sanitizer host behind a packaging decision nobody here made.

## Options

- **A. Stay on the image's default `clang`.** Rejected: the zero-warning gate has no stable
  reference point, and the `realtime` lane cannot exist.
- **B. Pin, and adopt the sibling's warning baseline file to absorb whatever the newer major says.**
  Rejected twice over. That file keys on the sibling's paths and flags (`-Wfloat-equal
  src/dsp/VelvetNoise.cpp`, `-Wswitch-enum src/dsp/AnamorphEngine.cpp`) — none of which exist here —
  and importing the *mechanism* would **permit** warning classes this gate currently forbids
  outright. The 0.2.0 migration audit rejected it on exactly that reasoning (item A2-22).
- **C. Pin the upstream stable major from apt.llvm.org and keep the zero-warning contract, fixing
  whatever the pin surfaces.** **Chosen** — and the measurement below is what made it possible to
  choose: the pin surfaces nothing.

## Decision

1. `ANABASIS_CLANG_VERSION: 22` in `.github/workflows/build.yml`, beside
   `ANABASIS_PLUGINVAL_STRICTNESS`, as the single authority for the major. **22 is the current
   upstream stable and is the major the sibling product pins**, which keeps the two products'
   diagnostics comparable — the point of sharing an engineering standard across a family.
2. `scripts/setup-llvm-apt.sh <major>` installs exactly three packages — `clang-<major>`,
   `lld-<major>`, `libclang-rt-<major>-dev` — from apt.llvm.org, upstream's own channel for this
   distribution. It is **fail-closed**: the signing key is pinned **by identity** (primary
   fingerprint `6084F3CF…`, asserted to be the *only* primary key in the keyring, so a concatenated
   blob cannot satisfy it), the suite name is read from `/etc/os-release` rather than hard-coded, and
   the installed compiler's version is asserted at the end. A partial install stops the job rather
   than letting it proceed on the image's default compiler.
3. Three jobs use it: `linux-clang` (build + warning gate + portability canary), `sanitizers`
   (ASan/UBSan; the same package carries those runtimes) and `realtime` (RTSan, ADR-0029).
   **Amended by ADR-0032 (0.2.1):** `linux-clang` is gone, and the pin's callers are now `linux`
   (which builds the shipped artifact with it and carries the warning gate and the canary),
   `merge-check`, `sanitizers`, `realtime` and the Clang arm of `linux-lto-tests`.
4. **No warning baseline file is introduced.** The gate stays at zero first-party warnings. If a
   future major surfaces diagnostics, they get **fixed**; re-taking that decision requires amending
   this ADR.
5. The GCC leg is untouched. `linux` still builds and ships the Linux artifact with the image's GCC,
   which is a rule-2 toolchain under the review gate's new §"Compiler and toolchain versions" — the
   repository does not pin it, so it is covered by detection and record rather than by review.
   **Superseded by ADR-0032 (0.2.1).** That decision reversed exactly this clause: Clang ships,
   GCC moves to `linux-lto-tests` as the compatibility compiler, and its major is pinned
   (`ANABASIS_GCC_VERSION`) — so it is a rule-1 toolchain now, not a rule-2 one. The sentence is
   kept rather than rewritten because it is the record of what this ADR decided, and the reason it
   changed is worth a reader's attention: the exposure it accepted ("the repository does not pin
   it") was on the SHIPPED artifact, which is the one place the argument does not hold.

## Consequences

- Each Clang job gains one apt transaction against a third-party host. If apt.llvm.org is
  unreachable the job fails saying so, which is the honest outcome — falling back to a different
  compiler would change the bar the gate measures against and read as a project problem.
- The pin is now a thing to maintain. It is deliberately not on Dependabot's radar (no ecosystem
  covers it); `DEPENDENCY_POLICY.md` records what maintains it instead.
- A local developer without clang-22 still builds and tests everything except the warning gate and
  the realtime lane, both of which `scripts/preflight.sh` reports as **skipped with a note** rather
  than passing quietly.

## Evidence

**Verified.** `scripts/setup-llvm-apt.sh 22` run on the `ubuntu-noble` image this session:
installed `clang-22`, `lld-22`, `libclang-rt-22-dev`; `clang-22 --version` reports **22.1.8**; the
key-identity assertion passed.

With that compiler, at C++23, building `AnabasisTests`, `AnabasisStateTests`,
`AnabasisChannelProbe`, `AnabasisEngineRepro` and the VST3:

- **`check-clang-warnings.py`: no first-party warnings** (2 diagnostics, both in vendored paths, not
  gated). The zero-warning contract holds at the pinned major with no baseline and no source change.
- Both suites green: `AnabasisTests` 301, `AnabasisStateTests` 873, 0 failures.
- `libclang_rt.rtsan-x86_64.a` is present at `/usr/lib/llvm-22/lib/clang/22/lib/linux/`, which is
  what makes ADR-0029's lane possible; `-fsanitize=realtime` links and runs.
- `__has_cpp_attribute(clang::nonblocking)` answers true, which the guarded macro in
  `src/dsp/RealtimeAnnotations.h` depends on.

The Windows and macOS toolchains are unaffected by this ADR and are rule-2 cases.

## Related code

- `.github/workflows/build.yml` (`env.ANABASIS_CLANG_VERSION`; `linux`, `merge-check`,
  `sanitizers`, `realtime`, `linux-lto-tests` — `linux-clang` until ADR-0032)
- `scripts/setup-llvm-apt.sh`
- `docs/policies/DEPENDENCY_POLICY.md` · `docs/policies/ARCHITECTURE_REVIEW_GATE.md`
- Worklog: `worklogs/2026-08-22-migration-roadmap-execution.md`
