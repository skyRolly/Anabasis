# macOS universal ccache — does it actually cache, and whose number justified it

**Date:** 2026-08-22 · **Version:** 0.2.4 · **Branch:** `claude/anabasis-init-migration-mvbbq9`

## The brief

Two review items. The changelog's entry count said "Nine" while listing ten. And, the substantive
one: the `macos` job caches a universal build (`-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`), but
**ccache has historically declined to cache compilations carrying multiple `-arch` values** — so the
claimed critical-path saving might not materialise at all, and the figure justifying it was carried
over from the sibling product rather than measured here. Verify before changing anything.

## The concern was correct, and it is version-bound

It is not folklore. ccache 3.2.5, `ccache.c:1999`:

```c
/* Multiple -arch options are too hard. */
if (str_eq(argv[i], "-arch")) {
    if (found_arch_opt) {
        cc_log("More than one -arch compiler option is unsupported");
        stats_update(STATS_UNSUPPORTED);
        result = false;
        goto out;
```

`NEWS.adoc` for 3.0 records the same: *"Errors when using multiple `-arch` compiler options are now
noted as `unsupported compiler option`."* Support arrived in **3.3** (2016-08-27, *"Added support for
multiple `-arch` options to produce fat binaries"*) and was corrected in **3.3.1** (2016-09-07),
which fixed direct mode failing to distinguish different `-arch` combinations. 4.x inherits that
logic unchanged.

`macos-latest` installs **ccache 4.13.6** — nine years past the fix.

## What the runners actually report

The decisive evidence is the job's own statistics, over three consecutive runs:

| Run | Cache state | Cacheable calls | Hits | Build step |
| --- | --- | --- | --- | --- |
| 32563814120 | **cold** (first cached run) | **182 / 182 (100.0%)** | 26 / 182 (14.29%) | **630.6s** |
| 32565784751 | warm | **182 / 182 (100.0%)** | **174 / 182 (95.60%)** | **233.5s** |
| 32568563583 | warm | **182 / 182 (100.0%)** | 172 / 182 (94.51%) | 370.0s |

`182 / 182` is the whole answer. ccache classifies **every** two-`-arch` compilation as cacheable and
declines none of them; there is **no `Uncacheable` bucket in any of the three logs**, confirmed by
keyword sweep rather than inferred from silence. Cold → warm takes **397s (63%)** off the build step
at an unchanged object count of 182 — the drop is cache hits, not less work.

The third run is higher than the second (370.0s) because one translation unit,
`juce_audio_plugin_client_VST3.mm`, took ~248s on its own that run; it is a miss-side and
link-side effect, not a cache regression. The hit rate is stable at 94–96%.

### Reproduced independently, with a control

On ccache 4.9.1 locally, using a shim that accepts and drops `-arch` (real Linux compilers reject the
flag outright, which masks ccache's decision behind `preprocessor_error`):

```
Executing .../clang -arch x86_64 -E ...     Got result key from preprocessor with -arch x86_64
Executing .../clang -arch arm64  -E ...     Got result key from preprocessor with -arch arm64
Result: cache_miss                          (cold)
Result: direct_cache_hit                    (warm, identical command)

Cacheable calls:      2 /   2 (100.0%)      <- no Uncacheable bucket
```

And the control that proves a decline would have been visible: `-save-temps` on the same shim yields
`Compiler option -save-temps is unsupported` / `Result: unsupported_compiler_option`. Two further
controls varying the arch values both **missed**, confirming the `-arch` set participates in the
cache key — so a universal object cannot be served to a thin build. That claim was previously
inherited from the sibling; it is now verified.

## The number that was not ours

The workflow comment, ADR-0034 and `CI_CD.md` all justified caching with *"the sibling measured the
equivalent job at 29m44s with 16m40s of it in the build step."* Measured here:

| | |
| --- | --- |
| `macos` job wall-clock | **18m43s** — genuinely the longest job in the matrix |
| of which the four pluginval passes | **12m34s (67.1%)** |
| of which the build step ccache acts on | **3m53s (20.7%)** |
| of that build step, actual compilation | **~57s** (ninja reached [178/182] at 57s) |
| the rest of it | **~157s in one AU LTO link**, which ccache cannot cache |

ccache's counters stop moving at `09:46:46` — ~46s into a 233.5s step — corroborating the split from
the other direction. So the critical-path claim **holds** (18m43s is the longest job), but the
magnitude claim did not describe this repository: **16m40s overstates our build step about
fourfold**, and two thirds of the job is validation that no compiler cache can touch.

The decision stands on 397s of real, measured saving on the longest job in the matrix. What changed
is that the justification is now local.

## The measurement gap that made this hard to answer

`macos-intel` is the natural control — it differs from `macos` in exactly one variable, one
architecture instead of two. It could not be used: it restores a ccache and then **never reports what
the cache did**. An audit of every cached job:

| job | ccache env | restore step | statistics step |
| --- | --- | --- | --- |
| `merge-check`, `linux`, `linux-lto-tests`, `linux-lto-clang`, `macos` | yes | yes | yes |
| **`macos-intel`** | yes | yes | **no** |
| `sanitizers`, `realtime` | yes | yes | **no** |

`macos-intel` now has one. A cache whose hit rate is unobservable can be neither defended nor retired
on evidence — the same "a gate that cannot fail is indistinguishable from one that passes" failure
the lint self-tests exist to prevent, applied to a measurement instead of a gate. `sanitizers` and
`realtime` share the gap and were **left alone deliberately**: they are Linux jobs, outside the macOS
question this step was added to answer, and the brief said not to touch unrelated workflow logic.

## Verdict and changes

**ccache works on the universal build. The configuration is kept unchanged.** No build configuration
was touched — no compiler flag, no architecture list, no cache key, no launcher wiring.

| File | Change |
| --- | --- |
| `CHANGELOG.md` | "Nine such entries" → the real count, cross-checked against the `## [0.x.y]` headings |
| `.github/workflows/build.yml` | `macos-intel` gains a read-only `Compiler cache statistics` step; the `macos` comment's imported 29m44s/16m40s replaced with the measured 182/182, 14.29%→95.60%, 630.6s→233.5s |
| `docs/procedures/CI_CD.md` | a row still claiming the macOS jobs were "deliberately not cached" — contradicting two other rows in the same file since 0.2.2 — corrected; the measurement recorded |
| `ADR-0034` | amended with the measurement, and with the honest proportion the imported figure obscured |

## Carried forward from 0.2.3, now closed

Run **32568563583** — the first after the `libxi-dev` fix — completed `linux-lto-tests` end to end
under **g++ 16.2.0**: both LTO links ran, the gate printed `check-clang-warnings: no first-party
warnings (0 in vendored/other paths, not gated)`, and both suites passed against that codegen
(**301 + 873, 0 failures**). The entire job log contains **two** `warning:` lines, both
`lto-wrapper: warning: using serial compilation of N LTRANS jobs` (N=5, N=101) — precisely the
location-less driver form 0.2.3 had just pinned as a non-diagnostic, now observed rather than
hypothesised. 0.2.3's "compile phase measured at 16, link phase measured at 14" no longer applies:
**GCC 16.2.0 is clean through compile and link.**

## Reported, deliberately not fixed

**The arm64 slice of the shipped macOS bundle has no dSYM.** `dsymutil` emits
`warning: (arm64) .../lto-objects/lto.o unable to open object file: No object file for requested
architecture` followed by `warning: no debug symbols in executable (-arch arm64)` — for VST3, AU
**and** Standalone — because the retained LTO object holds only the x86_64 slice. The
"Assert LTO ran and its objects were retained" step cannot detect this: it gates on `COUNT -eq 0`
and the count is 1. `CI_CD.md` describes macOS symbolication as a contract, and on Apple Silicon —
now the majority of Mac users — that contract is not currently met.

This is a shipped-artifact property, outside the scope of a ccache review, and changing the LTO
object-path handling could affect what ships. Filed for the owner rather than altered here.

## Evidence

- **[Verified]** ccache 4.13.6 on `macos-latest`; `Cacheable calls: 182 / 182 (100.0%)` with no
  `Uncacheable` bucket, runs 32563814120 / 32565784751 / 32568563583
- **[Verified]** build step 630.6s cold → 233.5s warm (397s, 63%); hits 14.29% → 95.60%
- **[Verified]** ccache 3.2.5 `ccache.c:1999` refuses multiple `-arch`; 3.3 adds fat-binary support,
  3.3.1 corrects direct-mode arch discrimination (`NEWS.adoc`)
- **[Verified]** reproduced on ccache 4.9.1 locally: two-`-arch` call is `cache_miss` then
  `direct_cache_hit`, `Cacheable calls: 2 / 2 (100.0%)`; `-save-temps` control yields
  `unsupported_compiler_option`; arch-varying controls both miss, so the `-arch` set is in the key
- **[Verified]** job proportions: 18m43s total, 12m34s pluginval, 3m53s build, ~57s compile
- **[Verified]** three cached jobs report no statistics; `macos-intel` now does
- **[Verified]** GCC 16.2.0 clean through the LTO link, run 32568563583
- **[Unverified]** whether an Apple toolchain's per-`-arch` preprocessor outputs diverging in content
  is fully sound in ccache's result-key handling. The local shim produced identical preprocessed text
  for both arches, so that path was not exercised. The CI statistics answer the practical question
  (182/182 cacheable, 95.6% hits, correct binaries through `lipo` and both slices' self-tests)
