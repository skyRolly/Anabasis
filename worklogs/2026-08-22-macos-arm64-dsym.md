# The arm64 slice had no dSYM, and three checks said it did

**Date:** 2026-08-22 · **Branch:** `claude/anabasis-init-migration-mvbbq9` · **ADR:** ADR-0035

## The brief

Two review items. A stale sanitizer sub-check count, and — the substantive one — the shipped macOS
universal bundle carries an arm64 slice with no matching dSYM while x86_64 symbol information is
retained. Inspect the packaging and symbol flow, confirm *why*, and add validation that verifies
**both** slices have usable dSYM output. Preserve the universal build; do not touch compiler flags
or optimization settings.

## Item 1 — the sanitizer count, and it was in four places

The comment claimed six UBSan sub-checks beyond `address,undefined` and then named five. The flags
are the arbiter:

```
-fsanitize=address,undefined,vptr,float-divide-by-zero,implicit-conversion,local-bounds,nullability
```

Five beyond the pair — `vptr`, `float-divide-by-zero`, `implicit-conversion`, `local-bounds`,
`nullability`. (The C flags carry four; `vptr` is C++-only, which is correct, not a sixth.) A
leftover from the sibling's set before `unsigned-shift-base` was dropped on measurement.

The review named two files. A sweep found **four**: `build.yml` twice ("Six sub-checks…" and "the
other six…"), ADR-0034 §Consequences, the ADR index row for ADR-0034, and the 0.2.2 worklog. All
corrected. The `-fsanitize` flags themselves are untouched.

## Item 2 — why arm64 has no dSYM

### The symptom

Every macOS run, cold and warm, all three bundles:

```
warning: (arm64) .../build/lto-objects/lto.o unable to open object file: No object file for requested architecture
warning: no debug symbols in executable (-arch arm64)
```

and the job nonetheless prints `a UUID-matched dSYM with compile units was captured and uploaded`.

### Why three checks missed it

`dsymutil` does not read DWARF out of the linked binary. It reads the **debug map** (`N_OSO` stabs),
walks back to each object file the linker consumed, and pulls DWARF from there. Under `-flto` ld64
does codegen itself and deletes the temporary — which is why `-Wl,-object_path_lto` exists here at
all (ADR-0034). The flag is present and working. What was wrong is that **every link writes to the
same destination**.

A universal build links **once per architecture**, and there are several LTO-linked targets, so one
build performs many links — each naming the same path. They overwrite rather than accumulate. The
retention step's own comment says so without noticing: *"a universal build links once per slice and
there are three format targets, so several objects are expected"* — six, then.

**CI reports `retained LTO objects: 1`.**

Which link won is readable from the warnings. The survivor is x86_64 (arm64 cannot open it), and the
symbols the *x86_64* dSYM pass cannot find in it include
`juce::DocumentWindow::parentHierarchyChanged` — a **Standalone** symbol, absent from the VST3 and AU
links. So the last link overwrote the rest, and every other bundle's debug map now points at an
object belonging to a different target. **Both axes collide: architecture and target.**

Then three checks reported success anyway:

| check | why it passed |
| --- | --- |
| dSYM UUID set matches the binary's slices | `dwarfdump --uuid` on a **fat** dSYM lists a UUID for every slice whether or not that slice carries DWARF. The set matched exactly, as required — and said nothing about DWARF. |
| dSYM has ≥ 1 DWARF compile unit | counted across the **whole fat file**: 39 from x86_64 + 0 from arm64 = 39, so `-eq 0` was false. "39 compile units" was true and useless. |
| LTO objects were retained | tested `COUNT -eq 0`. Read **1** where six were expected, and passed. |

None was wrong about what it measured. All three measured the wrong granularity.

## What was changed

**Both gates now work per architecture slice.**

1. **dSYM DWARF, per slice.** For each architecture from `lipo -archs` on the binary,
   `dwarfdump --arch=<A> --debug-info` must yield at least one `DW_TAG_compile_unit`. A dSYM empty
   for any slice the binary carries is discarded with a warning naming the slice — a dSYM that
   symbolicates only part of a universal binary is not a usable one. The success line now reports
   per-slice counts (`arm64=39 x86_64=39`) instead of a sum, so the log states the property that was
   actually asserted.
2. **Retention, on architecture coverage.** It reads the shipped bundle's architectures and requires
   at least one retained LTO object containing each. A count was rejected deliberately: it tracks
   the target list, which build options change, and it was never the property that broke.

The existing division of labour is untouched: debug-capture problems still degrade to a
`::warning::` and never withhold customer artifacts; `strip -x` → `codesign` → verify is unaffected;
the hard assertion still runs after the uploads.

### Proven against the case matrix

No Apple toolchain exists in this environment, so the logic was exercised against stub `lipo` and
`dwarfdump`:

| case | new gate | old aggregate gate |
| --- | --- | --- |
| universal, both slices have DWARF | KEEP | KEEP |
| **universal, arm64 empty — the observed defect** | **DISCARD** | **KEEP** |
| universal, x86_64 empty | DISCARD | KEEP |
| single-arch, healthy (`macos-intel`) | KEEP | KEEP |
| single-arch, degenerate | DISCARD | DISCARD |
| `lipo` cannot read the binary | DISCARD | — |

Row two is the whole point: the new gate rejects exactly what the old one accepted. Row four
confirms `macos-intel` is unaffected — with one slice the per-slice loop reduces to the test it
replaces.

Both changed `run:` blocks parse under `sh -n`; all five workflows and the composite action parse.

## The collision itself, fixed

The belief that made this safe was written down, and it was inverted. `CMakeLists.txt` said:

> *"IT MUST BE A DIRECTORY, not a file. [...] Given a directory, ld64 generates a unique name per
> link invocation, so the two slices — and the three format targets that link separately —
> coexist."*

ld64's own source says otherwise — `ld/parsers/lto_file.cpp`, full-LTO path:

```cpp
if ( stat(object_path.c_str(), &statBuffer) == 0 && S_ISDIR(statBuffer.st_mode) )
    object_path += "/lto.o";
```

A directory gets the **fixed** name `lto.o`. A path that is *not* an existing directory is used
verbatim as the output file — that is the branch which actually gives uniqueness. The comment named
the right hazard and then picked the one form that guarantees it.

**The fix** names a file per `(target, architecture)`: `$<TARGET_PROPERTY:NAME>` separates targets,
`-Xarch_<arch>` separates slices. Three properties are load-bearing, and are in the code comment
because a careless edit re-creates the bug silently:

- `-Xarch_<arch>` forwards exactly **one** token (clang raises
  `err_drv_invalid_Xarch_argument_with_args` otherwise), so the argument must be the single
  comma-joined `-Wl,-object_path_lto,<path>` — never an `-Xlinker` pair.
- CMake's `SHELL:` prefix is **required**; `target_link_options` de-duplicates and recombines
  otherwise, splitting `-Xarch_` from the argument it governs.
- It assumes **full LTO**. ThinLTO `mkdir`s its argument and names members `<n>.o` — still
  target- and arch-agnostic — so `-flto=thin` would reintroduce the bug. The coverage assertion is
  what would catch that.

### Verified on Linux, by reading the generated link lines

The CMake half needs no Apple toolchain to check — generate, then read `build.ninja`:

| shape | generated paths |
| --- | --- |
| `arm64;x86_64`, 3 targets | **6 distinct** — `AlphaVST3-arm64.o`, `AlphaVST3-x86_64.o`, `BetaAU-arm64.o`, `BetaAU-x86_64.o`, `GammaStandalone-arm64.o`, `GammaStandalone-x86_64.o` |
| `x86_64` only (`macos-intel`) | 3, one per target |
| no architecture list | 3, one per target, no `-Xarch_` emitted |

Six where one exists today — exactly the count the retention step's own comment always expected.
`SHELL:` held each `-Xarch_` adjacent to its argument, and `$<TARGET_PROPERTY:NAME>` expanded per
consuming target. The real `CMakeLists.txt` still configures.

### What remains unverified

ld64's runtime behaviour. This environment is Linux with no Apple linker, `lipo`, `dwarfdump` or
`dsymutil`, so the *effect* of these paths on a real universal link is CI's measurement, not this
machine's — the posture ADR-0034 took for the `gcc:16` container lane. Two specific caveats:

- Xcode 15+ defaults to **`ld-prime`**, which Apple has not open-sourced. The `lto.o` naming is
  confirmed in classic ld64 and corroborated by the observed single `lto.o`, but not read from the
  linker that actually ran.
- N_OSO stabs record each object's **mtime** and `dsymutil` validates it. With one path per link
  that should be stable; if mtime rejections ever appear, `dsymutil --no-object-timestamp` is the
  documented escape, and it weakens a real check so it is named here rather than pre-emptively set.

The per-slice gates are what will confirm or refute the fix on the next run — which is the point of
adding them in the same round.

## Evidence

- **[Verified]** five sub-checks, not six — read off `-fsanitize=` in `build.yml`; four stale
  occurrences corrected
- **[Verified]** `retained LTO objects: 1` against six expected; `warning: no debug symbols in
  executable (-arch arm64)` for VST3, AU and Standalone in runs 32565784751 and 32568563583
- **[Verified]** the surviving object is x86_64 and belongs to Standalone — its unresolved symbols
  in a VST3/AU pass are `juce::DocumentWindow` members
- **[Verified]** gate logic over the six-case matrix above; `sh -n` on both changed blocks; all
  workflows parse
- **[Unverified]** the gates against a real universal dSYM. No Apple toolchain here; CI is the
  measurement, the same posture ADR-0034 took for the `gcc:16` container lane
