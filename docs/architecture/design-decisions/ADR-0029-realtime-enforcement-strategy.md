# ADR-0029 — Realtime-safety enforcement: three tiers, and what deliberately stays out

> **✅ THE ARCHITECTURE REVIEW GATE IS CLEARED (2026-08-22).** Two items apply.
> **Build System change** — a new CI job, a pinned sanitizer runtime and a new compile-time flag on
> a test translation unit. And a **source change on a `DSP_POLICY`-frozen audio path**: a type
> attribute on `AnabasisEngine::process`. The owner directed this round explicitly — "execute the
> Anamorph → Anabasis migration roadmap", of which the two portable realtime tiers are phase 2 and
> the sanitizer lane is phase 3 — which is the human decision the gate exists to require. This
> record is the ADR half. The annotation's admissibility rests on the object comparison in
> §Evidence, not on the directive.

**Status:** **Accepted — 2026-08-22.** Version 0.2.0.

## Context

`REALTIME_AUDIO_POLICY.md` is this repository's **Priority-1** policy. Its rule is unconditional —
the audio thread must be deterministic, lock-free and allocation-free — and it names a hard red line
of forbidden operations (`new`/`malloc`, any container resize, mutex/lock, blocking wait, file or
network IO, `sleep`, thread creation, `std::async`, exceptions thrown on the path).

Until this ADR, **nothing in CI could detect a violation of it.** The enforcement was human review
plus `docs/architecture/REALTIME_SAFETY_AUDIT.md`, a hand-written per-module audit. Neither is a
gate. The dynamic tools already in the pipeline answer different questions and none of them asks
*where* an allocation happened: ASan finds out-of-bounds and lifetime bugs, UBSan finds undefined
behaviour, valgrind memcheck finds uninitialised reads. A `malloc` added to
`AnabasisEngine::process` is, to every one of them, perfectly correct code.

The audit itself recorded the gap and asked for the fix by name: *"A malloc-interposition run (e.g.
an RT-safety checker under the DSP suite) would upgrade the allocation claims from
Verified-by-inspection to machine-verified; tracked for P6's gate."* P6 closed at 0.1.0 and the gate
never landed; the request stood through four more versions.

## Problem

Adopting a realtime checker is not one decision but five, and getting any of them wrong produces
either a gate that cannot fail or noise that trains people to ignore it:

1. **What is annotated** — the annotation is a source change on a `DSP_POLICY`-frozen audio path.
2. **Whether the compile-time diagnostic (`-Wfunction-effects`) comes with it.**
3. **How a sanitizer lane relates to the existing `sanitizers` job.**
4. **What the lane's failure semantics are.**
5. **What covers the platforms a Clang sanitizer cannot reach** — two of the three shipped binaries
   are built by MSVC and AppleClang.

## Options

- **A. Do nothing; keep review plus the manual audit.** Rejected. The Priority-1 policy stays the
  only binding constraint in the repository with no mechanical check, and the audit is a document
  that rots.
- **B. One tier only — RealtimeSanitizer.** Rejected on reach. RTSan is Clang-only and
  Linux/macOS-only. The shipped Windows binary is MSVC's, and a policy enforced on two of three
  platforms is a policy with a hole exactly where nobody is looking.
- **C. One tier only — a static lint.** Rejected on power. A text scan cannot follow a call into
  JUCE, through a template, or into a helper; it would report a clean tree while `AudioBuffer::setSize`
  ran per block.
- **D. Fold RTSan into the existing `sanitizers` job.** **Impossible, not merely undesirable:** the
  Clang driver rejects `-fsanitize=realtime` combined with `address`, `undefined`, that job's actual
  `address,undefined` set, or `thread`. RTSan requires its own binary.
- **E. Three complementary tiers, each covering what the others structurally cannot.** **Chosen.**

## Decision

### 1. The properties being protected

Exactly those `REALTIME_AUDIO_POLICY.md` already binds. **This ADR adds no new constraint**; it adds
the first mechanical detectors for the constraint that already exists. Where the two could ever
disagree, the Policy governs.

### 2. Tier 1 — the allocation guard, the portable one

`tests/AllocationGuard.h`, compiled into `AnabasisTests` and armed only around
`AnabasisEngine::process` by a scope guard. Replaceable `operator new`/`delete` (standard C++, and
therefore available on **every** conforming implementation including MSVC) plus glibc malloc
interposition where that exists. It reports the two counts **separately** because they are two
routes: JUCE's `AudioBuffer`/`HeapBlock` take the raw-malloc path, so `operator new` alone would
miss most of what actually happens.

It proves it can count before it reports zero. `selfCheck()` performs one known allocation of each
kind and reports which halves moved; a half that is not live is **disclosed and skipped**, never
silently passed. The configurations genuinely differ: under ASan the malloc half is compiled out (an
executable-defined `malloc` fights ASan's allocator and the process dies), and under RTSan and
valgrind the whole guard is compiled out — the first because its definitions would take precedence
over RTSan's own interceptors and blind the lane, the second because memcheck tracks allocator
families separately and reports every guarded delete as a mismatched free.

### 3. Tier 2 — RealtimeSanitizer at one annotated entry point

`AnabasisEngine::process` carries `ANABASIS_NONBLOCKING` (`src/dsp/RealtimeAnnotations.h`). It is the
engine's audio-thread entry point and the entire serial DSP chain runs inside it, so **one
annotation places the whole chain under enforcement** — the sanitizer follows real calls at runtime
and needs no annotation on the callees.

The annotation is a **type attribute**, written after the parameter list and after `noexcept`; the
prefix spelling is a hard compile error, not a warning. It is guarded by
`__has_cpp_attribute(clang::nonblocking)` and expands to nothing elsewhere, because three of the four
toolchains that build this product are not the pinned Clang.

A new **`realtime`** job builds `AnabasisTests` with `-fsanitize=realtime` on the pinned Clang
(ADR-0031) and runs it. Three properties are load-bearing:

- **Its own build directory**, forced by option D's driver restriction.
- **Default `RTSAN_OPTIONS`.** The job sets none. RTSan halts on the first violation by default and
  exits 43; the one setting that would break the gate is `halt_on_error=false`, under which the
  process prints violations and still exits 0.
- **A liveness canary runs first** (`tests/realtime_canary.cpp`) and the step fails if it does **not**
  abort. It asserts two things — a non-zero exit *and* the sanitizer's own `ERROR: RealtimeSanitizer`
  report signature — because the first alone is satisfied by a canary that died for an unrelated
  reason. The canary's own failure message deliberately avoids that token.

Only `AnabasisTests` runs under RTSan. `AnabasisStateTests` drives the wrapper, but its audio-path
coverage is a handful of blocks; the DSP suite is where the chain is exercised across the whole
feature matrix, and a runtime tool's value is proportional to path coverage.

### 4. `-Wfunction-effects` is scoped to the JUCE-free leaf layer, not to the audio path

Clang can only infer a callee's effects from a **visible definition**, and JUCE 9.0.1 carries no
annotations of its own (measured: zero occurrences of `clang::nonblocking`, `clang::nonallocating` or
`__rtsan` in the pinned checkout). Enabling the flag over any translation unit that calls into JUCE
therefore produces warnings about correct code.

The rule this ADR sets: **the annotation marks entry points for the runtime tool; the compile-time
diagnostic waits for the dependency** wherever the dependency is in the call graph. That rule has a
boundary and the boundary is measurable rather than a matter of judgement. This engine has a
genuinely JUCE-free leaf layer — `CeilingClamp.h`, `ScopeBuffer.h`, `Latency.h` and
`EngineParameters.h`, four headers that include no JUCE module at all — and over that layer the flag
has nothing opaque to complain about. `tests/realtime_effects.cpp` is a compile-only translation unit
(`-fsyntax-only`, no link, no run) whose annotated driver calls those leaves in the order the audio
path calls them. It proves those bodies effect-clean **before any test executes them**, which is the
one thing a runtime tool structurally cannot do.

**That step compiles the file twice.** A clean compile is its entire output and is also what a dead
gate prints: Clang treats an unrecognised `-Werror=<name>` as a `-Wunknown-warning-option` *warning*,
so a renamed or dropped `function-effects` would leave the step exiting 0 while checking nothing.
`-Werror=unknown-warning-option` makes the renamed case fail by name on the first compile; the second
compile adds `-DANABASIS_EFFECTS_CANARY`, which seeds an allocating unannotated helper and a call to
it into the same file, and the step fails unless that compile fails **with** a `-Wfunction-effects`
diagnostic. Seeding into the gated TU rather than a separate canary file is deliberate — it exercises
the exact include set and flags the gate uses.

### 5. Tier 3 — a static lint over audio-path bodies

`scripts/check-realtime.py`, in `source-lint` with its own `--self-test`. Both runtime tiers see only
the code the suite executes; this one reads the branches it never takes, on every platform, with no
build. **Measured on this tree with gcov: the DSP suite covers 73.7 % of the 3,156 lines and 63.4 %
of the 1,190 branches in `src/dsp`** — so better than a third of the branches are invisible to both
runtime tiers.

It is **function-scoped**, which is the whole design. A file-wide token scan is unusable: all three
`setSize` calls in `AnabasisEngine.cpp` and every `.assign` in the modules sit inside `prepare()`,
where allocation is not merely allowed but required. The scan is bounded to the bodies of the
functions the policy names — matched by **exact name**, never by prefix, so
`PresetManager::resetSlotFieldsToDefaults` and `MacroEngine::resetToMacro` stay out — plus the
same-file transitive closure of what those bodies call. There is deliberately no separate exemption
list: a name that is not in the scope regex is never scanned, so a second list could only drift out
of agreement with the first.

### 6. Known limitations, stated so they are not rediscovered as surprises

- **Runtime coverage is what the suite executes.** An unexercised branch containing a `malloc` is
  invisible to tiers 1 and 2. Tier 3 is the answer, and it is a text scan.
- **Optimizer elision.** At `-O2`/`-O3` Clang can delete a non-escaping `malloc`/`free` pair before
  the RTSan pass sees it, so a *synthetic* canary of that shape can pass. Both canaries here escape
  their allocation through a volatile sink for that reason.
- **Weakening overrides are not diagnosed.** Clang accepts an override that drops `nonblocking` from
  an annotated virtual without any diagnostic.
- **Platform reach.** RTSan: Linux and macOS, Clang only. The shipped Windows (MSVC) and macOS
  (AppleClang) binaries are never built by that lane — which is precisely why tier 1 exists.
- **Third-party frames.** A violation inside JUCE is reported at the JUCE frame. That is correct — it
  is still a violation on this project's audio path — but the fix may be a call-site change here.

### 7. Suppressions are not used

`RTSAN_OPTIONS=suppressions=` exists and this repository deliberately does not use it: a suppression
file is where a real regression eventually hides. The response to a future report is to investigate
the source. If one is ever genuinely required it is a change to this ADR.

## Consequences

- One new CI job, one new pinned dependency (the sanitizer runtime, which arrives with ADR-0031's
  install), and one type attribute on a frozen audio path.
- `REALTIME_SAFETY_AUDIT.md`'s allocation claims move from Verified-by-inspection to
  **machine-verified**, and its first Gaps entry is closed by name.
- `TESTING_POLICY.md` gains rule 5 (a checker must prove it is live), which this round's three
  canaries and five `--self-test`s are the first cohort of.
- A future maintainer who adds an audio-path allocation now fails a job rather than waiting for a
  reviewer to notice.

## Evidence

**Verified.** Linux x86-64, Clang 22.1.8 from apt.llvm.org, C++23, JUCE 9.0.1 `e18f7f5…`.

- **The annotation changes no code.** `src/dsp/AnabasisEngine.cpp` compiled by clang-22 at `-O3`
  with the project's exact flag set, once with the attribute live and once with the macro forced
  empty, produces a **byte-identical object**: both 105,224 bytes, MD5
  `3cdcf88c10825bc8c3a130def259210f`. That is what permits the annotation on a `DSP_POLICY`-frozen
  path at all.
- **The DSP suite runs violation-free under RealtimeSanitizer:** 296 checks, 0 failures, exit 0.
  (296 rather than 301 because the allocation guard correctly stands itself down in that build and
  the suite **discloses** the five skipped assertions rather than passing them vacuously.)
- **The RTSan lane can fail.** `tests/realtime_canary.cpp` aborts with
  `ERROR: RealtimeSanitizer: unsafe-library-call … Intercepted call to real-time unsafe function
  'malloc' in real-time context!` and exit **43**.
- **The guard's stand-down is cross-checked from outside the compiler.** With
  `-DANABASIS_RTSAN_LANE=1` and no `-fsanitize=realtime`, the build fails with the `#error` naming
  both possible causes; with both, it compiles.
- **The compile-time tier is live.** `tests/realtime_effects.cpp` compiles clean under
  `-Werror=function-effects`; with `-DANABASIS_EFFECTS_CANARY` it fails with *"function with
  'nonblocking' attribute must not call non-'nonblocking' function
  '(anonymous namespace)::canaryAllocatingHelper' [-Werror,-Wfunction-effects]"*.
- **The allocation guard finds nothing on the audio path and proves it was looking:** 2,040 armed
  `process()` calls across 80 configurations (both channel counts × five oversample factors × two
  phase modes × four parameter sets), plus a mid-stream oversample rewire driven with no re-prepare
  — **0 allocations**, both counters proved live in the same run. For scale, the same guard reports
  `new=205 malloc=1313` for one `prepare (48000, 256, 2)`.
- **Under ASan** the same test reports `new=205 malloc=0` and skips one assertion, which is the
  documented state; **under valgrind** with `-DANABASIS_NO_ALLOC_GUARD` memcheck reports **0 errors
  from 0 contexts** (with the guard compiled in it reports repeated `Mismatched free() / delete /
  delete []` and fails the step, which is why that define is on that job's build).
- **The static lint reaches real bodies:** 35 audio-path bodies across 15 files; a seeded `new` in
  `AnabasisEngine::process`, a seeded `resize` in `AnabasisAudioProcessor::processBlock` and a
  seeded `resize` in `LookaheadLimiter::reset` are each reported, and the tree is clean without them.
  `--self-test`: 90 cases.

## Related code

- `tests/AllocationGuard.h` · `tests/realtime_canary.cpp` · `tests/realtime_effects.cpp`
- `src/dsp/RealtimeAnnotations.h` · `src/dsp/AnabasisEngine.h` · `src/dsp/AnabasisEngine.cpp`
- `scripts/check-realtime.py` · `.github/workflows/build.yml` (`realtime`, `source-lint`)
- Test: `testTheAudioPathAllocatesNothing` (`tests/dsp_tests.cpp`)
- Worklog: `worklogs/2026-08-22-migration-roadmap-execution.md`
