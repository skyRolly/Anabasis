# GCC 16 warning-gate validation — the container lane's first run

**Date:** 2026-08-22 · **Version:** 0.2.3 · **Branch:** `claude/anabasis-init-migration-mvbbq9`

## The brief

`linux-lto-tests` failed on its first real run. The lane runs GCC 16 and treats first-party
warnings as errors, but [ADR-0034](../docs/architecture/design-decisions/ADR-0034-ci-toolchain-parity.md)
recorded a measured zero-warning baseline only for **14.2.0** — 16 was unmeasured, and the lane was
kept strict anyway on the ADR-0031 rule that a pin which surfaces diagnostics gets them *fixed*
rather than baselined. So the question was the obvious one: did GCC 16 introduce diagnostics this
tree does not satisfy, and is the zero-warning policy still honest? With the standing constraint —
**do not weaken the gate without evidence**.

## What actually failed, which was not a warning

Run **32565784751** (push, `367fc446`). Exactly one job of twelve failed. The step that failed was
`Build the suites (LTO), gating on first-party warnings`, and it failed **10 seconds** in — a full
two-suite LTO build takes minutes, so it died early rather than completing and failing the gate at
the end. The reason:

```
juce_gui_basics.h:393:13: fatal error: X11/extensions/XInput2.h: No such file or directory
```

in three vendored translation units — `juce_gui_basics.cpp`, `juce_gui_extra.cpp` and
`juce_audio_processors.cpp`. `set -e` aborted the step before the gate ran at all. **The gate never
executed. There were no warnings to weaken a gate over.**

### Root cause

JUCE 9.0.1 defaults `JUCE_USE_XINPUT` to 1 (`juce_gui_basics.h:132`), so the include at line 393 is
unconditional in practice. `dpkg -S` puts that header in **`libxi-dev`** — a package
`scripts/setup-linux.sh` never named.

It reached the `full` profile anyway, transitively: `libgtk-3-dev` carries `Depends: libxi-dev`.
That is why the Ubuntu runners, and this development machine, compiled it without complaint for the
entire life of the script. 0.2.2 moved the gtk/webkit pair to `full`-only on correct evidence —
every target sets `JUCE_WEB_BROWSER=0`, JUCE gates its webkit include on that macro, and JUCE 9.0.1
declares no `linuxPackages` for `juce_gui_extra` — and in doing so took the X-input **headers** out
of `headless` along with the toolkit nothing here compiles.

Depending on a GUI toolkit we do not compile to supply a header we do compile is exactly the
accident an explicit package list exists to prevent, so `libxi-dev` is now explicit on **both**
profiles.

### The methodological finding

0.2.2's evidence block verified that **every package name resolves** on Debian trixie and Ubuntu
noble. Whether the declared set is **sufficient** is a different question, and it was never asked.
The check that asks it:

```sh
grep -rhoE '<(X11|GL|EGL|freetype2?|fontconfig|alsa|xcb)/[^>]+>' modules/ | sort -u   # what the tree includes
dpkg -S /usr/include/<header>                                                          # what owns each one
```

Against JUCE 9.0.1 that is **fifteen headers over eight packages**. Every one but `libxi-dev` was
already named explicitly; `x11proto-dev` (which owns `X11/Xmd.h`) is a hard `Depends:` of
`libx11-dev` and so is guaranteed. One omission, and it was the one that fired.

## The warning answer: GCC 16 is clean

The failing run is itself the measurement, because ninja got 36 of 56 objects built before it
stopped — and **every first-party translation unit in both suites was among them**. All 13:

| Target | Translation units compiled by g++ 16.2.0 |
| --- | --- |
| `AnabasisTests` | `tests/dsp_tests.cpp`, `src/dsp/AnabasisEngine.cpp` |
| `AnabasisStateTests` | `tests/state_tests.cpp`, `src/PluginProcessor.cpp`, `src/PluginParameters.cpp`, `src/MacroEngine.cpp`, `src/PresetManager.cpp`, `src/dsp/AnabasisEngine.cpp`, and all six `src/gui/*.cpp` |

Flags as the log records them: `-O3 -flto -std=c++23` plus the full
`juce_recommended_warning_flags` set (`-Wall -Wextra -Wpedantic -Wsign-conversion -Wshadow
-Wfloat-equal -Woverloaded-virtual -Wzero-as-null-pointer-constant …`). **Not one of the thirteen
emitted a diagnostic.** Two majors and a change of distribution, and the zero-warning policy did not
move.

### The objection that has to be answered

43 of that run's 47 cacheable compiles were **ccache hits**, so "the log is silent" is only evidence
if a hit still prints the stored warning. Measured directly rather than assumed:

```
$ ccache g++-14 -Wall -Wextra -c warn.cpp -o warn.o     # miss
warn.cpp:1:20: warning: unused variable 'unusedVar' [-Wunused-variable]
$ ccache g++-14 -Wall -Wextra -c warn.cpp -o warn.o     # hit
warn.cpp:1:20: warning: unused variable 'unusedVar' [-Wunused-variable]
```

ccache replays stored stderr on a hit, and `CCACHE_COMPILERCHECK=content` means a hit requires the
byte-identical compiler — so a replayed diagnostic is still g++ 16.2.0's. A warning would have
appeared either way.

### The limit of that evidence, stated plainly

The build aborted at 36/56, so the **LTO link never ran** — and the link is where GCC emits `-Wodr`
and `-Wlto-type-mismatch`, the cross-translation-unit class this lane exists for. That phase is
measured locally instead, at **GCC 14.2.0**, both suites, `-flto` on compile and link, gate run over
the complete log:

```
$ python3 scripts/check-clang-warnings.py --log gcc-lto-local.log --root . --build-dir ./build-gcc16val
check-clang-warnings: no first-party warnings (0 in vendored/other paths, not gated).
```

The only two `warning:` lines in that entire log are `lto-wrapper: warning: using serial compilation
of 5 / 99 LTRANS jobs`. Both suites then pass against that codegen: **301 + 873 checks, 0
failures**, with the allocation guard armed over 2,040 `process()` calls.

So: **compile phase measured at 16, link phase measured at 14.** The first green run of the fixed
lane closes the last cell.

> **SUPERSEDED 2026-08-22 (0.2.4).** That green run happened: **32568563583**. `linux-lto-tests`
> passed end to end under g++ 16.2.0 — both LTO links completed, the gate printed
> `check-clang-warnings: no first-party warnings (0 in vendored/other paths, not gated)`, and both
> suites passed against that codegen (301 + 873, 0 failures). The whole job log carries exactly two
> `warning:` lines, both `lto-wrapper: warning: using serial compilation of N LTRANS jobs` (N=5,
> N=101) — the location-less driver form this same round had just pinned as a non-diagnostic,
> observed in the wild rather than hypothesised. **The link phase is measured at 16 too; the
> "measured at 14" qualifier above no longer applies.**

## The gate itself, validated rather than assumed

The script is called `check-clang-warnings.py` and two of its three callers are now GCC, so its
behaviour on GCC output was checked rather than inferred.

**Format coverage.** GCC's link-time diagnostics were generated from purpose-built two-TU cases to
see what the matcher would face:

| Diagnostic | Form GCC emits | Matched? |
| --- | --- | --- |
| `-Wlto-type-mismatch` | `b.c:1:8: warning: type of 'x' does not match original declaration` | yes |
| `-Wodr` | `c1.cpp:1:8: warning: type 'struct S' violates the C++ One Definition Rule` | yes |
| `-Wmissing-profile`, `#pragma GCC warning` | `pg.cpp:1:23: warning: …` | yes |
| `lto-wrapper`, `cc1plus`, `ld` | no path, no line, no column | correctly not a diagnostic |

Every GCC form that names first-party code carries the `path:line:col:` shape the matcher requires.
The location-less forms are driver-level: there is no file to attribute them to, and
`lto-wrapper`'s LTRANS-parallelism notice is a property of the machine, not of this tree.

**End to end, not only the synthetic self-test.** A fixture carrying a GCC `-Wunused-variable`, a
GCC LTO-time `-Wodr` and a vendored diagnostic classifies **2 first-party / 1 vendored** and exits
1.

## Changes

| File | Change |
| --- | --- |
| `scripts/setup-linux.sh` | `libxi-dev` added to `CORE_PACKAGES` — both profiles — with the transitive-via-gtk accident recorded |
| `scripts/check-clang-warnings.py` | `--compiler` argument, so a GCC lane's failure is not attributed to Clang; default is a neutral `the compiler`. Three self-test cases pin the driver-level forms. 15 → 18 cases |
| `.github/workflows/build.yml` | the three gate call sites pass the compiler they ran |
| `ADR-0034` | GCC 16.2.0 recorded as measured; the package-sufficiency finding recorded as a method, not just a fix |
| `docs/procedures/CI_CD.md` | the stale "installed from the distribution archive" pinned-GCC row corrected to the container; 0.2.3 section |
| `CMakeLists.txt`, `CHANGELOG.md`, `docs/HANDOVER.md` | 0.2.3 |

**Not changed, deliberately:** the gate's strictness (nothing surfaced that would justify relaxing
it), the sanitizer decisions, the fuzz/preflight decisions, and the GCC 16 migration strategy. No
first-party C++ source file was touched, and no DSP algorithm, parameter, serialization schema,
threading model or reported latency moved.

## Evidence

- **[Verified]** GCC 16.2.0, 13/13 first-party TUs, 0 diagnostics — CI run 32565784751,
  `linux-lto-tests`
- **[Verified]** ccache replays stderr on a hit, measured in both directions
- **[Verified]** GCC LTO compile **and link**, both suites, 0 first-party diagnostics; 301 + 873
  checks, 0 failures — local, `g++-14 14.2.0`
- **[Verified]** `libxi-dev` owns `X11/extensions/XInput2.h` (`dpkg -S`); present in Debian trixie
  and Ubuntu noble, checked with known-absent negative controls
- **[Verified]** `check-clang-warnings.py --self-test` — 18 cases; and the gate run against real GCC
  output, not only synthetic paths
- **[Unverified]** the `gcc:16` container lane end to end. This environment has a `docker` client
  but no reachable daemon, and no apt source here ships a released g++-16 — which is the same
  reason ADR-0034 moved the lane into the image. The fixed lane's first run is the measurement
