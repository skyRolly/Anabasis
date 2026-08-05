# BUILD.md

How to configure and build Anabasis. Headless, command-line only (CMake + JUCE; no IDE/Projucer).

> **Status: the build exists** (P1 skeleton, 2026-07-31). `CMakeLists.txt` implements ADR-0008's
> five-target graph — `AnabasisHardening` + `AnabasisDSP` INTERFACE libraries, the `juce_add_plugin`
> target, and the `AnabasisTests` / `AnabasisStateTests` console apps — and `src/` carries the
> wrapper, the GUI skeleton and the DSP core. Everything below is a description of what the tree
> does, not a specification of what it should do. Verified green on Linux; the Windows and macOS
> legs are confirmed by CI.

## Toolchain

- **CMake ≥ 3.22**, a **C++20** compiler, **Ninja** (recommended generator).
- **JUCE 9.0.0** is fetched automatically (CMake `FetchContent`, pinned to that tag's **immutable
  commit SHA** `f8f8864172464b9adf9eba6101e1f784838d1597`) — or pointed at a local checkout. Same
  revision as the sibling product (`docs/OPEN_QUESTIONS.md` OQ-001, resolved). See
  `docs/policies/DEPENDENCY_POLICY.md` for the version-lock reasoning.

## Linux dependencies (Ubuntu)

```bash
scripts/setup-linux.sh     # safe to re-run; installs build + X11/audio/GTK deps + xvfb
```

Installs: `build-essential cmake git ninja-build pkg-config`, `curl unzip`, ALSA/JACK/libcurl,
FreeType/Fontconfig, X11 (`libx11/xcomposite/xcursor/xext/xinerama/xrandr/xrender`),
`libglu1-mesa-dev mesa-common-dev libegl-dev`, `libwebkit2gtk-4.1-dev libgtk-3-dev`, and `xvfb`.

**`libegl-dev` is required for JUCE 9** — it creates Linux OpenGL contexts via EGL instead of GLX,
so the EGL headers are a build dependency even if the plugin never attaches a GL context on Linux.
If `libwebkit2gtk-4.1-dev` is unavailable on your release, try `libwebkit2gtk-4.0-dev`.

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

# ...or the convenience wrapper (prints the produced .vst3 path):
scripts/build.sh            # scripts/build.sh [Release|Debug]
```

## Build options (the contract P1 implements)

| Option | Default | Effect |
|---|---|---|
| `ANABASIS_CXX_STANDARD` | 20 | Which C++ standard the tree compiles at. **20** is the ADR-0008 baseline and the only value anything ships from; **23** exists solely so the OQ-006 canary can ask "does tomorrow's baseline still compile?". Any other value refuses at configure. **Set the standard through this option, never through `-DCMAKE_CXX_STANDARD`** — the project assigns `CMAKE_CXX_STANDARD` unconditionally, so a direct override is shadowed and ignored *silently*. It is a cache entry, so a tree configured at 23 stays at 23 until it is reset or reconfigured fresh |
| `ANABASIS_BUILD_TESTS` | ON | Build the `AnabasisTests` + `AnabasisStateTests` console apps |
| `ANABASIS_BUILD_STANDALONE` | ON | Add the Standalone target (debugging convenience) |
| `ANABASIS_JUCE_PATH` | "" | Use a local JUCE checkout instead of fetching |
| `ANABASIS_JUCE_TAG` | `f8f8864…` (= tag 9.0.0) | JUCE git rev to fetch when no local path is given; `ANABASIS_JUCE_VERSION` (`9.0.0`) carries the human-readable version |
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
