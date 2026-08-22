# ADR-0030 — The language standard moves C++20 → C++23, and the canary that asked for it is retired

> **✅ THE ARCHITECTURE REVIEW GATE IS CLEARED (2026-08-22).** `ARCHITECTURE_REVIEW_GATE.md` lists
> "**Build System change** — CMake structure, JUCE version/pin, **C++ standard baseline**, dependency
> set" among the changes that must not be auto-merged however green CI is. The owner directed this
> explicitly as part of the migration round: *"Move Anabasis to C++23 where required by the migration
> plan… Do not keep older versions simply because they already work."* This record is the ADR half.

**Status:** **Accepted — 2026-08-22**, on the owner's directive. Version 0.2.0.
**Supersedes:** ADR-0008 decision B5's *standard* (C++20). B5's other half — **no modules** — is
reaffirmed, not moved. **Closes:** OQ-006.

## Context

ADR-0008 fixed the baseline at C++20 and admitted 23 through exactly one seam: an
`ANABASIS_CXX_STANDARD` cache variable, legal values 20 and 23, default 20, whose **only** caller was
the weekly `cxx23-canary.yml` workflow. That job asked "does tomorrow's baseline still compile?" on
three OSes, non-blocking, never a required check. OQ-006 recorded the question it was answering.

Two things changed. The sibling product raised its own standard to 23 (its ADR-0027) and ships it on
all three platforms, so the family's engineering standard moved. And the pinned Clang this round
introduces (ADR-0031) is a compiler for which 23 is unremarkable rather than forward-looking.

## Problem

The canary's answer had been "yes" for every run since it landed, which makes it a job that cannot
usefully fail. Keeping it while raising the baseline would be worse: it would compile the baseline
weekly, off to one side, in a job nobody is required to read.

Meanwhile the seam's value depends entirely on having a caller. An `option()` no build uses is dead
state, and this repository removes dead state rather than leaving it to imply a configuration
somebody supports (the same call round 53 took on two unarmed `LookAndFeel` subclasses).

## Options

- **A. Keep C++20 and keep the canary.** Rejected on the owner's directive, and on the substance
  behind it: the two products would compile the same shared idioms under different standards, which
  makes a difference between them harder to attribute — the same argument ADR-0028 makes about the
  JUCE pin.
- **B. Raise the default to 23, keep the seam so 20 stays buildable.** Rejected. The seam would then
  have no caller in CI and no consumer in the build; its only remaining use would be manual
  bisection, which `-DCMAKE_CXX_STANDARD` cannot express here anyway (the unconditional `set()`
  shadows a same-named cache entry, so the flag fails *silently*). An option that exists to be
  guessed at is worse than none.
- **C. Hard `set(CMAKE_CXX_STANDARD 23)`, seam removed, canary retired.** **Chosen.**

## Decision

1. `CMakeLists.txt` sets `CMAKE_CXX_STANDARD 23` unconditionally, with
   `CMAKE_CXX_STANDARD_REQUIRED ON` and `CMAKE_CXX_EXTENSIONS OFF` unchanged.
2. The `ANABASIS_CXX_STANDARD` cache variable and its validation branch are **removed**.
3. `.github/workflows/cxx23-canary.yml` is **deleted**. Its question is now answered by every job in
   `build.yml` on all three platforms, on every push, as a blocking check.
4. **Modules remain unused** — ADR-0008 decision B5's other half stands. C++23 *library* features are
   admitted only behind feature-test macros and a thin first-party abstraction, because the three
   shipped standard libraries do not implement them on the same schedule. That rule is unchanged; it
   was already the rule for 23 features under the canary.
5. Changing the baseline again is this same gate plus an ADR. The hard `set()` is deliberate: it
   makes a command-line override fail rather than configure, so the way to change the standard is a
   commit.

## Consequences

- The minimum toolchains rise to those that implement C++23: GCC 13+, Clang 17+, MSVC 19.35+,
  AppleClang 15+. Every CI image already exceeds all four, and `DEPENDENCY_POLICY.md`'s row is
  updated to match.
- OQ-006 closes. `DEVELOPMENT_BRIEF.md` §2.1's forward-compatibility policy is discharged rather than
  amended: it asked for a canary until the baseline moved, and the baseline has moved.
- One weekly non-blocking job disappears from the Actions page. Nothing that was checked stops being
  checked; it is checked more often and blockingly.
- No source changed to accommodate the standard. This is a compiler-mode change, not a port.

## Evidence

**Verified.** Linux x86-64, JUCE 9.0.1 `e18f7f5…`, both suites built and run at 23:

| Compiler | Build | Suites |
|---|---|---|
| GCC 13.3, Release | clean — no first-party diagnostics | `AnabasisTests` 301, `AnabasisStateTests` 873, 0 failures |
| Clang 18.1.3, Release | clean — the only diagnostic is a `#pragma message` from vendored JUCE | 301 / 873, 0 failures |
| Clang 22.1.8 (the ADR-0031 pin), Release | **zero first-party warnings** under the full gate, 2 diagnostics in vendored paths | 301 / 873, 0 failures |

The Windows and macOS legs are CI's half and land with this commit's run.

## Related code

- `CMakeLists.txt` (the standard block)
- `.github/workflows/cxx23-canary.yml` — deleted
- `docs/policies/DEPENDENCY_POLICY.md` · `docs/policies/CODE_STYLE.md` · `docs/OPEN_QUESTIONS.md`
- Worklog: `worklogs/2026-08-22-migration-roadmap-execution.md`
