# ADR-0035 — macOS symbolication is a per-ARCHITECTURE contract, not a per-bundle one

**Status:** Accepted · **Date:** 2026-08-22 · **Supersedes:** nothing. Amends the symbolication
contract established in ADR-0034's macOS bullets and in `docs/procedures/CI_CD.md`.

## Context

ADR-0034 recorded macOS crash symbolication as a *contract*: `-Wl,-object_path_lto` keeps the LTO
codegen object that `dsymutil` needs, and two assertions — "LTO ran and its objects were retained"
and "a validated dSYM was captured" — were said to turn a best-effort capture into a gate.

A review of the 0.2.4 CI logs found that the contract was not being met on **arm64**, and that both
assertions reported success anyway. Every macOS run — cold and warm, all three format bundles —
emits:

```
warning: (arm64) .../build/lto-objects/lto.o unable to open object file: No object file for requested architecture
warning: no debug symbols in executable (-arch arm64)
```

for VST3, AU **and** Standalone, while the job prints `a UUID-matched dSYM with compile units was
captured and uploaded`. On Apple Silicon — now the majority of Mac users, and the population most
likely to hand a developer an OS crash log — the shipped bundle was not symbolicatable at all.

## The two findings

### 1. The gates were architecture-blind, and that is why nobody saw it

Neither assertion was wrong about what it measured. Both measured the wrong granularity.

- **The dSYM UUID check passed** because `dwarfdump --uuid` on a *fat* dSYM lists a UUID for every
  slice regardless of whether that slice carries any DWARF. The UUID **set** matched the binary's
  slices exactly, as required — and told us nothing about DWARF.
- **The compile-unit check passed** because it counted `DW_TAG_compile_unit` across the whole fat
  file. x86_64 contributed 39, arm64 contributed 0, the sum was 39, and `-eq 0` was false. The
  reported "39 compile units" was true and useless: it was 39 for one slice and 0 for the other.
- **The retention assertion passed** because it tested `COUNT -eq 0` on the retained-object
  directory. Its own comment says *"a universal build links once per slice and there are three
  format targets, so several objects are expected"* — six, then. It read **1** and passed.

The general shape is one this repository has hit before and named: a gate that cannot fail is
indistinguishable from a gate that passes. Here the gate *could* fail, but only for a condition
strictly worse than the one that actually occurred.

### 2. The cause is several links sharing one retained-object path — and the comment explaining why that was safe had it backwards

`CMakeLists.txt` passes one `-Wl,-object_path_lto,<dir>` to every LTO-linked target. A universal
build links **once per architecture**, and there are several LTO-linked targets, so a single build
performs many links — every one of them naming the same destination. The evidence that they collide
rather than accumulate is the count: one file where six were expected.

The evidence for *which* link won is in the dSYM warnings. The surviving object is x86_64 (arm64
cannot open it at all), and the symbols x86_64's own dSYM pass cannot find in it include
`juce::DocumentWindow::parentHierarchyChanged` — a **Standalone** symbol, absent from the VST3 and
AU links. So the last link to run overwrote the rest, and every other bundle's debug map now points
at an object belonging to a different target. Both axes collide: architecture and target.

**Why it was believed safe.** `CMakeLists.txt` carried an explicit, confident, and inverted
explanation:

> *"IT MUST BE A DIRECTORY, not a file. [...] Given a directory, ld64 generates a unique name per
> link invocation, so the two slices — and the three format targets that link separately —
> coexist."*

ld64 does the opposite. From `ld/parsers/lto_file.cpp`, the full-LTO path:

```cpp
if ( stat(object_path.c_str(), &statBuffer) == 0 && S_ISDIR(statBuffer.st_mode) )
    object_path += "/lto.o";
```

A directory gets the **fixed** name `lto.o` appended — no architecture in it, no target in it. A
path that is *not* an existing directory is used verbatim as the output file, which is the branch
that actually provides uniqueness. The comment named the right hazard, then chose the one form that
guarantees it.

## Decision

**Symbolication is asserted per architecture slice, for every architecture the shipped artifact
carries.** Concretely:

1. **The dSYM check counts DWARF compile units per slice.** For each architecture reported by
   `lipo -archs` on the binary, `dwarfdump --arch=<A> --debug-info` must yield at least one
   `DW_TAG_compile_unit`. A dSYM that is empty for any slice the binary carries is discarded with a
   warning naming the slice, exactly as a UUID-mismatched dSYM already was. A dSYM that symbolicates
   only some slices of a universal binary is not a usable dSYM.
2. **The retention assertion gates on architecture coverage, not on a count.** It reads the shipped
   bundle's architectures and requires at least one retained LTO object containing each of them. A
   count was rejected deliberately: it tracks the target list, which build options change, and it
   was never the property that broke. Coverage is.

3. **Each `(target, architecture)` gets its own retained-object path.**
   `$<TARGET_PROPERTY:NAME>` separates the targets; `-Xarch_<arch>` separates the slices, forwarding
   the option to only that architecture's link. Three properties of that form are load-bearing and
   are recorded because a careless edit re-creates the bug silently:
   - `-Xarch_<arch>` forwards exactly **one** following token — clang raises
     `err_drv_invalid_Xarch_argument_with_args` on more — so the linker argument must be the single
     comma-joined `-Wl,-object_path_lto,<path>`, never an `-Xlinker` pair.
   - CMake's `SHELL:` prefix is **required**. `target_link_options` de-duplicates and recombines
     otherwise, which would separate `-Xarch_` from the argument it governs.
   - It assumes **full LTO**. ThinLTO's branch in the same ld64 file `mkdir`s its argument and names
     members `<n>.o`, still target- and arch-agnostic; `-flto=thin` would reintroduce this bug, and
     assertion 2 is what would catch it.

Both keep the existing division of labour that ADR-0034 established and this ADR does not disturb:
debug-capture problems never withhold customer artifacts. The dSYM validity tests degrade to a
`::warning::` and discard, packaging continues, the customer pipeline (`strip -x` → `codesign` →
verify) is untouched, and the hard assertion runs **after** the uploads.

## Consequences

- **The arm64 slice's dSYM becomes a gated property**, on the platform where crash logs actually
  arrive. The `macos` job fails while the underlying collision is unfixed — which is the gate
  working, not a regression introduced by it.
- **`macos-intel` is unaffected.** A single-architecture build has one slice; the per-slice loop
  reduces to the aggregate test it replaces. Verified across the case matrix below.
- **The success message now reports per-slice counts** (`arm64=39 x86_64=39`) rather than a sum, so
  the log states the property that was asserted instead of one that merely correlates with it.
- **What this does NOT change:** no compiler flag, no optimization setting, no sanitizer flag, no
  DSP algorithm, parameter, serialization schema, threading model or reported latency. The universal
  build is preserved exactly — same architectures, same `-flto`, same shipped bytes.

## Related code
- `.github/workflows/build.yml` — `macos` job: `Assert LTO ran and its objects were retained`,
  `Package macOS plugins`, `Assert a validated dSYM was captured`
- `CMakeLists.txt:116-164` — `AnabasisHardening`, the `-object_path_lto` link option

Evidence [Verified, except where stated]:
- Source: `.github/workflows/build.yml`
- **The defect**, from CI: `retained LTO objects: 1` where the step's own comment expects one per
  (target × architecture); `warning: no debug symbols in executable (-arch arm64)` for all three
  bundles in runs 32565784751 and 32568563583; the surviving object's unresolved symbols are
  Standalone's, in a VST3/AU dSYM pass
- **The gate logic**, exercised against stub `lipo`/`dwarfdump` over six cases: healthy universal
  keeps; **arm64-empty and x86_64-empty both discard where the previous aggregate logic kept**;
  single-arch healthy keeps (so `macos-intel` is unaffected); single-arch degenerate discards;
  unreadable slices discard
- Both changed `run:` blocks parse under `sh -n`; all five workflows and the composite action parse
- **The fix's CMake half, verified on Linux** by generating and reading the link lines: three
  targets x two architectures produce **six distinct** `-object_path_lto` paths where one exists
  today; `SHELL:` keeps each `-Xarch_<arch>` adjacent to its argument; `$<TARGET_PROPERTY:NAME>`
  expands per consuming target; the single-architecture and no-architecture-list shapes degrade to
  one path per target. The real `CMakeLists.txt` still configures
- **ld64's behaviour, from source rather than from this machine:** the `S_ISDIR` -> `"/lto.o"` branch
  in `ld/parsers/lto_file.cpp`; the per-`-arch` link + `lipo` decomposition in clang's
  `Driver::BuildUniversalActions`; `-Wl,` carrying no `NoXarchOption` and `ToolChain::
  TranslateXarchArgs` handling `LinkerInput` per bound architecture
- **Unverified locally:** anything requiring a macOS toolchain. This environment is Linux with no
  Apple linker, `lipo`, `dwarfdump` or `dsymutil`, so neither the gates nor the fix are exercised
  against a REAL universal link here — CI is the measurement, the same posture ADR-0034 took for the
  `gcc:16` container. Xcode 15+ defaults to `ld-prime`, which Apple has not open-sourced; the
  `lto.o` naming is confirmed in classic ld64 and corroborated by the observed single `lto.o`
- Worklog: `worklogs/2026-08-22-macos-arm64-dsym.md`
