# BUILD.md

How to configure and build Anabasis. Headless, command-line only (CMake + JUCE; no IDE/Projucer).

> **Status: the build does not exist yet.** There is no `CMakeLists.txt` and no `src/` — those
> land at P1 (`docs/DEVELOPMENT_BRIEF.md` §11). This document specifies the build contract the P1
> work must implement, and becomes a description of reality at that point. The Linux dependency
> setup below is real and usable today.

## Toolchain

- **CMake ≥ 3.22**, a **C++20** compiler, **Ninja** (recommended generator).
- **JUCE 9.x** is fetched automatically (CMake `FetchContent`, pinned to the release tag's
  **immutable commit SHA**) — or pointed at a local checkout. The exact tag and SHA are resolved
  at P0 (`docs/OPEN_QUESTIONS.md` OQ-001) and recorded in `CMakeLists.txt` and `README.md`. See
  `docs/policies/DEPENDENCY_POLICY.md` for the version-lock reasoning.

## Linux dependencies (Ubuntu)

```bash
scripts/setup-linux.sh     # safe to re-run; installs build + X11/audio/GTK deps + xvfb
```

Installs: `build-essential cmake git ninja-build pkg-config`, ALSA/JACK/curl, FreeType/Fontconfig,
X11 (`libx11/xcomposite/xcursor/xext/xinerama/xrandr/xrender`), `libglu1-mesa-dev
mesa-common-dev libegl-dev`, `libwebkit2gtk-4.1-dev libgtk-3-dev`, and `xvfb`.

**`libegl-dev` is required for JUCE 9** — it creates Linux OpenGL contexts via EGL instead of GLX,
so the EGL headers are a build dependency even if the plugin never attaches a GL context on Linux.
If `libwebkit2gtk-4.1-dev` is unavailable on your release, try `libwebkit2gtk-4.0-dev`.

`xvfb` is needed for pluginval's editor open/close tests, not for the build itself.

## Configure + build

```bash
# Recommended (Ninja, Release):
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# ...or the convenience wrapper (prints the produced .vst3 path):
scripts/build.sh            # scripts/build.sh [Release|Debug]
```

## Build options (the contract P1 implements)

| Option | Default | Effect |
|---|---|---|
| `ANABASIS_BUILD_TESTS` | ON | Build the `AnabasisTests` + `AnabasisStateTests` console apps |
| `ANABASIS_BUILD_STANDALONE` | ON | Add the Standalone target (debugging convenience) |
| `ANABASIS_JUCE_PATH` | "" | Use a local JUCE checkout instead of fetching |
| `ANABASIS_JUCE_TAG` | *(the pinned SHA)* | JUCE git rev to fetch when no local path is given; `ANABASIS_JUCE_VERSION` carries the human-readable version |
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
   both read the same CMake variable, so a new file can never be added to only one of them.
4. `juce::juce_recommended_config_flags` + `_lto_flags` + `_warning_flags`, warning-free.

## Formats produced

`VST3` everywhere; `+ AU` additionally on macOS (Logic Pro loads only AU); `+ Standalone` when
`ANABASIS_BUILD_STANDALONE` is ON. **AAX is Not Supported** by decision.

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
