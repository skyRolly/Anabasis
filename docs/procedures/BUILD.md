# BUILD.md

How to configure and build Anabasis. Headless, command-line only (CMake + JUCE; no IDE/Projucer).

> **Status: the build exists** (P1 skeleton, 2026-07-31). `CMakeLists.txt` implements ADR-0008's
> five-target graph — `AnabasisHardening` + `AnabasisDSP` INTERFACE libraries, the `juce_add_plugin`
> target, and the `AnabasisTests` / `AnabasisStateTests` console apps — and `src/` carries the
> wrapper, the GUI skeleton and the DSP core. Everything below is a description of what the tree
> does, not a specification of what it should do. Verified green on Linux; the Windows and macOS
> legs are confirmed by CI.

## Toolchain

- **CMake ≥ 3.22**, a **C++23** compiler, **Ninja** (recommended generator). The baseline moved
  from C++20 at 0.2.0 ([ADR-0030](../architecture/design-decisions/ADR-0030-cxx23-language-standard.md)),
  so the floor is GCC 13+, Clang 17+, MSVC 19.35+ or AppleClang 15+. It is a hard `set()` in
  `CMakeLists.txt`, not a cache variable: `-DCMAKE_CXX_STANDARD=20` does not take effect, and
  that is deliberate — changing the baseline is an Architecture Review Gate item, so the way to
  change it is an ADR and a commit rather than a flag.
- **The Linux Clang jobs use a PINNED major** ([ADR-0031](../architecture/design-decisions/ADR-0031-clang-toolchain-pin.md)),
  installed by `scripts/setup-llvm-apt.sh <major>`; the value is `ANABASIS_CLANG_VERSION` in
  `.github/workflows/build.yml`. **Since 0.2.1 that major also builds the shipped Linux artifact**
  ([ADR-0032](../architecture/design-decisions/ADR-0032-linux-release-toolchain.md)) — what a user
  installs on Linux is a clang-22 build, and `lld` (installed beside the compiler) is what links its
  LTO. An ordinary local build still needs none of this: the pin matters for the
  zero-first-party-warning gate, for RealtimeSanitizer and for reproducing the shipped codegen, and
  `scripts/preflight.sh` reports the first two as skipped-with-a-note when the pinned compiler is
  absent.
- **GCC is the compatibility compiler, and its major is pinned too** — `ANABASIS_GCC_VERSION` in the
  same file, supplied by the official `gcc:<major>` **container** rather than by apt, because no apt
  source ships a released g++-16 ([ADR-0034](../architecture/design-decisions/ADR-0034-ci-toolchain-parity.md)).
  It builds the two test suites with `-flto` in `linux-lto-tests`
  ([ADR-0033](../architecture/design-decisions/ADR-0033-lto-validation-lane.md)), which is where "the
  tree still compiles under the other major toolchain" is checked. A local GCC build needs none of
  this — any g++ that supports C++23 will do; the pin is about what CI's compatibility claim means.
- **JUCE 9.0.1** is fetched automatically (CMake `FetchContent`, pinned to that tag's **immutable
  commit SHA** `e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8`) — or pointed at a local checkout.
  OQ-001 pinned the sibling product's revision; **ADR-0028 (2026-08-16) moved this repository to
  9.0.1 while Anamorph stayed at 9.0.0**, so the two are a patch apart. See
  `docs/policies/DEPENDENCY_POLICY.md` for the version-lock reasoning.
  A tree configured against the previous pin does **not** pick this up: `ANABASIS_JUCE_TAG` is a
  cache entry, and `set(... CACHE ...)` never overwrites one. Reconfiguring an existing `build/`
  keeps fetching 9.0.0 silently. Delete the build directory, or pass
  `-DANABASIS_JUCE_TAG=e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8` explicitly.

## Linux dependencies (Ubuntu)

```bash
scripts/setup-linux.sh              # `full` (default): a developer's machine
scripts/setup-linux.sh headless     # what compiling + linking needs, and nothing else
```

**Two profiles, because one caller is not a fresh Ubuntu machine.** `full` is the default and what a
developer or a packaging job wants; `headless` is what the `linux-lto-tests` container job installs,
and it drops the host toolchain, the pluginval fetch/display pair, lld and the web-browser binding.
The package lists live in the script so there is one place that knows what a build needs.

**`headless` (both profiles install these):** `cmake git ninja-build pkg-config ca-certificates
python3`, ALSA/JACK/libcurl, FreeType/Fontconfig, X11
(`libx11/xcomposite/xcursor/xext/xinerama/xrandr/xrender`), `libglu1-mesa-dev mesa-common-dev
libegl-dev`.
**`full` adds:** `build-essential`, `curl unzip`, `xvfb`, `lld`, `libwebkit2gtk-4.1-dev libgtk-3-dev`.

**`libegl-dev` is required for JUCE 9** — it creates Linux OpenGL contexts via EGL instead of GLX,
so the EGL headers are a build dependency even if the plugin never attaches a GL context on Linux.

**`libfreetype-dev`, not `libfreetype6-dev`.** The `6` spelling is gone from Debian trixie — the base
of the `gcc:16` container — and survives on Ubuntu only as a `Provides:` on the modern package. The
unsuffixed name is real on both.

**The web-browser binding is a `full`-only extra and nothing here compiles it:** every target sets
`JUCE_WEB_BROWSER=0`, JUCE gates the webkit include on that macro, and JUCE 9.0.1 declares no
`linuxPackages` for `juce_gui_extra`. Where `libwebkit2gtk-4.1-dev` is absent the successor is
`libwebkitgtk-6.0-dev` (Debian trixie and later); `libwebkit2gtk-4.0-dev` is the OLDER name and is
already gone from trixie, so it is not a fallback there.

Three of these serve **pluginval**, not the build: `xvfb` (the editor open/close tests need a
display) and `curl` + `unzip` (`run-pluginval.sh` downloads and extracts the pluginval release).
`curl` the CLI is *not* implied by `libcurl4-openssl-dev`, which is only the development headers —
GitHub-hosted runners preinstall both tools, so a missing one shows up on a fresh machine or a
minimal container rather than in CI.

## Configure + build

```bash
# Recommended (Ninja, Release):
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# ...or exactly what CI ships on Linux (ADR-0032): the pinned Clang, with lld
# linking the LTO. Use this when a result has to match the shipped artifact —
# reproducing a codegen-dependent fault, or re-measuring the ABI floor. The
# major is READ from the one place that holds it rather than pasted here, for
# the reason the pluginval strictness is (see CI_CD.md): a literal in a document
# outlives the pin it was copied from.
CLANG=$(sed -n 's/^  ANABASIS_CLANG_VERSION: *//p' .github/workflows/build.yml)
scripts/setup-llvm-apt.sh "$CLANG"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER="clang-$CLANG" -DCMAKE_CXX_COMPILER="clang++-$CLANG"

# ...or the convenience wrapper (prints the produced .vst3 path):
scripts/build.sh            # scripts/build.sh [Release|Debug]
```

## Build options (the contract P1 implements)

| Option | Default | Effect |
|---|---|---|
| ~~`ANABASIS_CXX_STANDARD`~~ | — | **Removed at 0.2.0** ([ADR-0030](../architecture/design-decisions/ADR-0030-cxx23-language-standard.md)). The baseline moved to C++23 and the seam went with the canary that was its only caller. The standard is now a hard `set()` in `CMakeLists.txt`; `-DCMAKE_CXX_STANDARD=20` is shadowed and ignored *silently*, which is why changing it is an Architecture Review Gate item rather than a flag |
| `ANABASIS_BUILD_BENCH` | OFF | Build `AnabasisBench` (DESIGN §9). ON in the `linux` CI job and nowhere else, so the harness cannot rot unnoticed; never RUN in CI, because a shared runner's numbers are exactly the ones `PERFORMANCE_BUDGET.md` forbids quoting |
| `ANABASIS_BUILD_PROBE` | OFF | Build `AnabasisChannelProbe` (which HOSTS the built bundle) and `AnabasisEngineRepro` (the same stimulus with no wrapper, format or host) |
| `ANABASIS_NO_LTO` | OFF | Drop `juce_recommended_lto_flags` from every target. **Bisection only** — the shipped binary always carries them |
| `ANABASIS_BUILD_TESTS` | ON | Build the `AnabasisTests` + `AnabasisStateTests` console apps |
| `ANABASIS_BUILD_STANDALONE` | ON | Add the Standalone target (debugging convenience) |
| `ANABASIS_JUCE_PATH` | "" | Use a local JUCE checkout instead of fetching |
| `ANABASIS_JUCE_TAG` | `e18f7f5…` (= tag 9.0.1) | JUCE git rev to fetch when no local path is given; `ANABASIS_JUCE_VERSION` (`9.0.1`) carries the human-readable version |
| `ANABASIS_BUILD_NUMBER` | 0 | CI build/dev number shown in the About box (`-DANABASIS_BUILD_NUMBER=${{ github.run_number }}`) |

Offline build (no network) with a local JUCE:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DANABASIS_JUCE_PATH=/path/to/JUCE
```

## Structure the P1 `CMakeLists.txt` must have

Inherited from Anamorph (`DEVELOPMENT_BRIEF.md` §18) — these are not stylistic preferences, each
one exists because of a specific failure mode:

1. **`AnabasisDSP` as an `INTERFACE` library**, not `STATIC`. Its sources compile into each final
   target. A static library here would duplicate JUCE module object code and break linking.
2. **`AnabasisHardening` as an `INTERFACE` target**, linked `PUBLIC` so the flags propagate to
   every format target's compiles *and* its final link, and to the test targets — so the
   self-tests validate the shipped flag configuration.
   - MSVC: `/guard:cf`; Release `/Zi` + `/DEBUG /OPT:REF /OPT:ICF`; `/DYNAMICBASE /NXCOMPAT`
     pinned explicitly so a toolchain change cannot silently drop them.
   - GCC/Clang: `-fstack-protector-strong -ffunction-sections -fdata-sections`, Release `-g`;
     link `--gc-sections` + `-z relro -z now -z noexecstack` (Linux) / `-dead_strip` (macOS).
   - **Deliberately absent:** `-O3`, `-ffast-math`, extra LTO — numerics-affecting flags are
     frozen by `DSP_POLICY.md`. Stripping happens in CI packaging only; local builds stay
     debuggable.
3. **One wrapper/GUI source list, two consumers** — the plugin target and `AnabasisStateTests`
   both read the same CMake variable, so a new file can never be added to only one of them. Both
   also carry the **same `PRIVATE` link set**, `juce::juce_opengl` included: the editor attaches an
   `OpenGLContext` on macOS/Windows (`DESIGN.md` §6.1), and the state test compiles the GUI sources,
   so omitting the module there breaks the link rather than the render (ADR-0008).
4. `juce::juce_recommended_config_flags` + `_lto_flags` + `_warning_flags`, warning-free.

## Formats produced

`VST3` everywhere; `+ AU` additionally on macOS (Logic Pro loads only AU); `+ Standalone` when
`ANABASIS_BUILD_STANDALONE` is ON. **AAX is Not Supported** by decision.

## Plugin identity (frozen — `juce_add_plugin` arguments)

| Field | Value | Note |
|---|---|---|
| `COMPANY_NAME` | `RollyTech` | |
| `BUNDLE_ID` | `com.rollytech.anabasis` | |
| `PLUGIN_MANUFACTURER_CODE` | **`RTec`** | The **vendor** code — identical in every RollyTech plug-in. Anamorph moved from `Anmf` to `RTec` in its 0.9.1 for this reason (its ADR-0023). |
| `PLUGIN_CODE` | **`Anbs`** | Per-product, unique. `Anmr` is Anamorph's. |
| `PRODUCT_NAME` | `Anabasis` | |
| `VST3_CATEGORIES` | `"Fx" "Dynamics" "Mastering"` | |

These are **host-facing identity**, not cosmetics: the manufacturer code is the AU component's
manufacturer field, and JUCE derives the VST3 class UID from the manufacturer code + plugin code +
plugin name. Change one after a build has left this repository and every saved session reports the
plugin as **missing** — the host cannot match it at all. They are frozen from the first build
(`docs/policies/COMPATIBILITY_POLICY.md`; decided in `docs/OPEN_QUESTIONS.md` OQ-003).

## Compile definitions (part of the build contract)

`ANABASIS_VERSION_STRING`, `ANABASIS_BUILD_NUMBER`, `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`,
`JUCE_VST3_CAN_REPLACE_VST2=0`, `JUCE_DISPLAY_SPLASH_SCREEN=0`, `JUCE_REPORT_APP_USAGE=0`,
`JUCE_STRICT_REFCOUNTEDPOINTER=1`.

## Symbols

Local Release builds carry full debug info (`-g` / `/Zi` via the hardening flags) and are **never
stripped locally** — debugging a local build works out of the box. Stripping, with debug-info
retention as separate `Anabasis-<OS>-debug` artifacts, happens only in CI packaging
(`CI_CD.md`).

## Network domains the build needs (restricted sandboxes)

- Ubuntu apt mirrors (`archive.ubuntu.com` / `ports.ubuntu.com`) — `setup-linux.sh`.
- `github.com` — JUCE source (pinned commit SHA via `FetchContent`).
- `github.com` — pluginval release (only for `scripts/run-pluginval.sh`).
