# Third-party components in Anabasis

Complete inventory of third-party software that Anabasis compiles, links or redistributes,
with the licence of each and the exact file the licence was read from.

**This is a factual record, not legal advice.** No licensing or legal determination is made
here; open decisions are listed under [Open licensing decisions](#open-licensing-decisions)
and tracked in [`docs/OPEN_QUESTIONS.md`](docs/OPEN_QUESTIONS.md) (OQ-002). The short
attribution notices that must accompany a binary distribution are reproduced in
[`NOTICE`](NOTICE); `docs/policies/RELEASE_POLICY.md` §"Third-party attribution" makes
carrying both files with every binary distribution a release requirement.

Copy-and-adapt provenance (ADR-0009): the structure and verification protocol of this
inventory are adapted from `Anamorph:THIRD_PARTY_LICENSES.md`. The **findings are not
copied**: every row below was re-verified against this repository's own pinned JUCE tree and
its own build objects, exactly because "an inventory copied from another project is not
evidence" (`RELEASE_POLICY.md`, constraint C7). That the resulting component set matches the
sibling's is the expected consequence of both products pinning the same JUCE commit — it is a
result, not an assumption.

## How this inventory was produced

Anabasis has exactly one declared dependency: **JUCE**, fetched by CMake `FetchContent` and
pinned to an immutable commit (`CMakeLists.txt` — `ANABASIS_JUCE_TAG`; ADR-0008, OQ-001).
Every third-party component below therefore arrives *inside the JUCE source tree* — nothing
else is vendored, no package manager is used, and the plugin embeds no typefaces or other
assets of its own (`src/` contains no `BinaryData` and registers no custom `Typeface`).

The inventory was verified against the pinned tree and a local Release build (2026-08-05),
not from memory:

| Question | How it was answered |
|---|---|
| Which components exist? | `LICENSE.md` at the root of the fetched JUCE checkout — JUCE's own authoritative dependency list — plus a walk of the compiled translation units for components that reach the build transitively (see the FreeType/stb note below) |
| What licence does each carry? | the component's real licence file inside the JUCE tree (paths cited per row) |
| Which are actually compiled into Anabasis? | `nm` symbol probes on the **per-TU object files** of a Release build (e.g. `FLAC__stream_decoder_init_stream` in `juce_audio_formats_flac_2.c.o`, `SBAlgorithmCreate` in `juce_graphics_Sheenbidi.c.o`, `jcopy_block_row` in `juce_graphics_libjpg_1.c.o`, `png_create_read_struct` in `juce_graphics_libpng.c.o`, `hb_buffer_create` in `juce_graphics_Harfbuzz.cpp.o`, `PVG_FT_*` in `juce_graphics_lunasvg.c.o`, `vorbis_synthesis` in `juce_audio_formats.cpp.o`). Probing the **linked** `.so` is unreliable here — this project builds with LTO and `--gc-sections` (`juce_recommended_lto_flags`, `AnabasisHardening`), which internalise or drop most static C symbols from the final image — so the objects, not the image, are the evidence |
| Which are present but *not* built? | the compile-time gate that excludes them (`#if` guard, platform guard, or a `FORMATS` value Anabasis does not build), confirmed by the absence of their symbols from the same objects (e.g. `juce_audio_processors_headless_lv2_libs.cpp.o` contains zero `lilv_`/`serd_`/`sord_`/`sratom_`/`lv2_` symbols) |

To re-verify after a JUCE bump, repeat exactly that: read the new `LICENSE.md`, then re-run
the symbol probes against a fresh Release build's object files. See
[`docs/policies/DEPENDENCY_POLICY.md`](docs/policies/DEPENDENCY_POLICY.md).

Pinned version at the time of writing: **JUCE 9.0.0**, commit
`f8f8864172464b9adf9eba6101e1f784838d1597`. Paths below are relative to that checkout
(`build/_deps/juce-src/` in a local build) unless stated otherwise.

---

## 1. Framework

### JUCE

| | |
|---|---|
| **Purpose** | The entire application framework: DSP primitives (`dsp::Oversampling`, FIR/IIR filters, `dsp::AudioBlock`), the parameter system (APVTS), GUI, and the VST3/AU/Standalone format wrappers |
| **Origin** | Raw Material Software Limited — <https://juce.com> |
| **Licence** | **Dual: AGPLv3 *or* the commercial JUCE 9 licence** |
| **Licence file** | `LICENSE.md` (JUCE checkout root) |
| **Shipped** | Yes — statically compiled into every Anabasis binary |

JUCE's own words: *"The JUCE Framework modules are dual-licensed under the AGPLv3 and the
commercial JUCE licence."*

**The product model is closed-source commercial** (`README.md` §Licensing). That model cannot
satisfy the AGPLv3 arm, so the commercial JUCE 9 licence must be in place before commercial
distribution; obtaining it (and which tier) remains an open owner action — **OQ-002**, which
blocks commercial distribution and nothing else. The repository declares no licence of its
own.

---

## 2. Compiled into the shipped binaries

Everything in this table produces object code in the Anabasis VST3 / AU / Standalone builds.
All of it arrives via JUCE modules; none of it is separately vendored by this repository.

| Component | Purpose in Anabasis | Licence | Licence file (in the JUCE tree) |
|---|---|---|---|
| **Steinberg VST 3 SDK** | The VST3 plug-in interface Anabasis implements | MIT (Steinberg Media Technologies GmbH, 2025) — but see §3 | `modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt` |
| **HarfBuzz** | Text shaping for every label and readout in the GUI | "Old MIT" (HarfBuzz's own term) | `modules/juce_graphics/fonts/harfbuzz/COPYING` |
| **SheenBidi** | Unicode bidirectional text ordering for the GUI | Apache License 2.0 — © 2016-2025 Muhammad Tayyab Akram | `modules/juce_graphics/unicode/sheenbidi/LICENSE` |
| **LunaSVG** | SVG rasterisation in JUCE's `Drawable` path | MIT — © 2020-2025 Samuel Ugochukwu | `modules/juce_graphics/drawables/lunasvg/LICENSE` |
| **PlutoVG** | 2-D vector rasteriser used by LunaSVG | MIT — © 2020-2025 Samuel Ugochukwu | `modules/juce_graphics/drawables/lunasvg/plutovg/LICENSE` |
| **FreeType** (vendored *inside* PlutoVG) | Scanline rasteriser, path stroker and fixed-point maths — `plutovg-ft-raster.c`, `plutovg-ft-stroker.c`, `plutovg-ft-math.c`, all `#include`d by `juce_graphics_lunasvg.c` | FreeType Project Licence (FTL) — © 1996-2002, 2006 David Turner, Robert Wilhelm, Werner Lemberg | `.../lunasvg/plutovg/source/FTL.TXT` |
| **stb_truetype / stb_image / stb_image_write** (vendored inside PlutoVG) | TrueType glyph extraction (`plutovg-font.c`) and image encode/decode (`plutovg-surface.c`) inside PlutoVG | MIT **or** public domain (Unlicense), at the recipient's choice — © 2017 Sean Barrett | licence text at the end of `.../plutovg/source/plutovg-stb-truetype.h` (same in the other two headers) |
| **libpng** | PNG image decoding | PNG Reference Library License v2 | `modules/juce_graphics/image_formats/pnglib/LICENSE` |
| **libjpeg (IJG)** | JPEG image decoding | Independent JPEG Group licence — **carries a mandatory acknowledgement** (see below) | `modules/juce_graphics/image_formats/jpglib/README` §LEGAL ISSUES (the condition-(2) wording sits at lines 129-131) |
| **zlib** | Deflate/inflate used by JUCE's zip and PNG paths (compiled as `juce_core_zlib.c`) | zlib licence — © 1995-2026 Jean-loup Gailly and Mark Adler | `modules/juce_core/zip/zlib/README` (§"Copyright notice") |
| **FLAC** | Audio-file reading via `juce_audio_formats` | BSD 3-clause — © Josh Coalson / Xiph.Org Foundation | `modules/juce_audio_formats/codecs/flac/Flac Licence.txt` |
| **Ogg Vorbis** | Audio-file reading via `juce_audio_formats` (compiled inside the module's unity TU) | BSD 3-clause — © 2002-2020 Xiph.org Foundation | `modules/juce_audio_formats/codecs/oggvorbis/libvorbis-1.3.7/COPYING` |
| **GLEW / Mesa / Khronos OpenGL declarations** | The OpenGL entry points in `juce_gl.h`. `juce_opengl` is linked **unconditionally on every platform** (ADR-0008); only the context *attach* is gated at runtime — macOS/Windows yes, Linux/X11 never (`DESIGN.md` §6.1) | BSD (GLEW), MIT (Mesa), MIT (Khronos) | `modules/juce_opengl/opengl/juce_gl.h`, between the `BEGIN_GLEW_LICENSE` / `END_GLEW_LICENSE` markers |
| **AudioUnitSDK** | The AU wrapper — **macOS builds only** (the `APPLE` gate in `CMakeLists.txt` is what adds `AU` to `FORMATS`) | Apache License 2.0 | `modules/juce_audio_plugin_client/AU/AudioUnitSDK/LICENSE.txt` |

FLAC and Ogg Vorbis reach the binary because `JUCE_USE_FLAC` and `JUCE_USE_OGGVORBIS` both
default to `1` (`modules/juce_audio_formats/juce_audio_formats.h:69-85`) and Anabasis does not
override them. Anabasis itself never reads audio files (presets are XML text); the codecs come
along with the module.

### Notices that are *mandatory*, not courtesy

Three of the above impose an attribution obligation on binary distribution. All three are
discharged by [`NOTICE`](NOTICE). Nothing has ever been distributed from this repository
(`docs/HANDOVER.md` Release Status), so no obligation has yet been triggered — the machinery
is in place *before* the first artifact leaves: `NOTICE` and this file are copied into every
CI customer artifact (`build.yml`, all three staging steps), and
`docs/policies/RELEASE_POLICY.md` §"Third-party attribution" requires them with every binary
distribution. v0.1.0 ships as plain zips (OQ-007), so the in-artifact copies are the carrier;
anyone redistributing the binaries further must carry both files along:

- **libjpeg (IJG)** — condition (2): *"If only executable code is distributed, then the
  accompanying documentation must state that 'this software is based in part on the work of the
  Independent JPEG Group'."* The IJG licence also forbids using an IJG author's or company name
  in advertising.
- **FLAC** and **Ogg Vorbis** (BSD 3-clause) — *"Redistributions in binary form must reproduce
  the above copyright notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution."* The third clause also
  forbids using the Xiph.Org Foundation name to endorse the product.
- **HarfBuzz** requires the copyright notice and its two disclaimer paragraphs to appear in all
  copies; **SheenBidi**'s Apache-2.0 terms require the licence and attribution notices to be
  carried along.

- **FreeType** (FTL) does not mandate a specific form, but §"Legal Terms" asks distributors of
  binaries to credit it and supplies the wording — *"Portions of this software are copyright ©
  &lt;year&gt; The FreeType Project (www.freetype.org). All rights reserved."* — which
  `NOTICE` reproduces with the `<year>` placeholder filled (2000-2014, the span the vendored
  `plutovg-ft-*` file headers carry), exactly the substitution the FTL's own text instructs.

zlib and libpng ask for acknowledgement but explicitly do *not* require it; it is given anyway.
**stb** is dual MIT/public-domain, so attribution is optional; it is listed for completeness.

> **Two of these are not in JUCE's own `LICENSE.md` dependency list.** FreeType and stb reach the
> build *transitively*, vendored inside PlutoVG rather than by JUCE directly, so reading JUCE's
> list alone would have missed both. This is why the verification step above walks the actual
> compiled translation units (`juce_graphics_lunasvg.c` `#include`s the three `plutovg-ft-*.c`
> files plus `plutovg-font.c` and `plutovg-surface.c`, which in turn include the three stb
> headers; the compiled object carries the `PVG_FT_*` symbols) instead of trusting an upstream
> manifest. Repeat that walk after any JUCE bump.

---

## 3. Steinberg VST 3 — separate review required

The VST 3 SDK **source code** bundled with JUCE 9.0.0 is under the **MIT licence**
(`.../VST3_SDK/LICENSE.txt`, "Copyright (c) 2025, Steinberg Media Technologies GmbH").

The MIT grant covers the code. It does **not** cover the "VST" name and logo, or the terms on
which VST 3 plug-ins may be developed and distributed. Evidence in the pinned tree:

- `.../VST3_SDK/VST3_Usage_Guidelines.pdf` ships alongside the SDK.
- `.../VST3_SDK/README.md` states that the full VST 3 SDK obtained from Steinberg contains
  *"the **Steinberg VST 3 Plug-In SDK Licensing Agreement** that you have to sign if you want to
  develop or host **VST 3** plug-ins."*

> **Commercial VST3 distribution requires reviewing Steinberg's licensing requirements
> separately.** This repository makes no determination about which agreements apply, whether one
> must be signed, or how the VST trademark may be used. That review is an owner action, recorded
> beside OQ-002 in `docs/OPEN_QUESTIONS.md` and in `README.md` §Licensing.

---

## 4. Present in the JUCE tree but NOT built into Anabasis

Listed for completeness so a future audit does not have to re-derive the exclusions. Each was
confirmed excluded by the stated gate *and* by the absence of its symbols from this build's
objects.

| Component | Licence (per JUCE's `LICENSE.md` / its own file) | Why it is not in Anabasis |
|---|---|---|
| **JUCE MP3 decoder** | JUCE's own terms, with an explicit patent/IP disclaimer | `JUCE_USE_MP3AUDIOFORMAT` defaults to **0** and Anabasis does not enable it, so the decoder's body is `#if`-ed out (zero MP3-named symbols in the build). JUCE's disclaimer warns the code is *"NOT guaranteed to be free from infringements of 3rd-party intellectual property"* — Anabasis therefore ships no MP3 decoder. |
| **LV2 SDK** (lv2, lilv, serd, sord, sratom) | ISC | `juce_audio_processors_headless_lv2_libs.cpp` is compiled but its content is behind `#if JUCE_INTERNAL_HAS_LV2`; the object contains no `lv2_`/`lilv_`/`serd_`/`sord_`/`sratom_` symbols. Anabasis neither builds an LV2 plug-in nor hosts plug-ins. |
| **AAX SDK** | Proprietary Avid AAX licence / GPLv3 | AAX is **Not Supported** (`docs/policies/COMPATIBILITY_POLICY.md`, `DEVELOPMENT_BRIEF.md` §2); it is not in Anabasis's CMake `FORMATS`. |
| **Steinberg ASIO SDK** | Proprietary Steinberg ASIO licence / GPLv3 | Only the licence file and headers are present (`modules/juce_audio_devices/native/asio/`); `JUCE_ASIO` is not enabled (no `ASIOInit` in the build). |
| **Oboe** | Apache License 2.0 | Android audio backend; Anabasis targets Linux/Windows/macOS only. |
| **CHOC (incl. QuickJS)** | ISC (CHOC), MIT (QuickJS) | Lives in `juce_javascript`, a module Anabasis does not link. |
| **Box2D** | zlib | Lives in `juce_box2d`, a module Anabasis does not link. |
| **pslextensions** | Public domain | Presonus VST3 extension headers; not referenced by Anabasis. |
| **ARA** | — | No ARA SDK is present in the pinned tree; the ARA translation unit compiles empty. |
| **reaper-sdk, Projucer icons, Android Gradle wrapper** | zlib / MIT / Apache 2.0 | JUCE examples, bundled apps and build tooling — not part of a plug-in build. |

---

## 5. Dynamically linked system libraries

These are **not redistributed** by Anabasis — they are provided by the operating system or the
user's distribution and resolved at load time. Recorded because a downstream packager may need
to know. Two lists, because they answer different questions — both read from a Release Linux
VST3 build on 2026-08-05:

**Direct `DT_NEEDED` entries** (`objdump -p` on the `.so`):
`libfontconfig`, `libfreetype`, `libstdc++`, `libgcc_s`, `libm`, `libc`, the dynamic loader.

**Loaded at runtime rather than linked**: the X11/OpenGL stack (`libX11`, `libGL`, …) is
*absent* from `DT_NEEDED` — JUCE 9 resolves it dynamically when a display is present (the
editor runs under `xvfb` in every pluginval gate, which is what exercises this path
headlessly). A packager should treat those libraries as required for GUI use even though the
linker does not record them.

On Windows and macOS the equivalents are OS frameworks (Core Audio / Audio Units, Direct2D,
etc.) shipped with the operating system.

---

## 6. Build- and CI-only tools

Used to produce or validate builds; **never redistributed inside an artifact**, so no notice
obligation attaches to the shipped product.

| Tool | Where | Note |
|---|---|---|
| **pluginval** (Tracktion) | `scripts/run-pluginval.sh` / `.ps1` — downloaded at validation time | The validation gate; not packaged. Its version is not pinned — recorded as a tracked improvement in `docs/HANDOVER.md` §Critical dependencies. |
| **xvfb** | `scripts/setup-linux.sh` / the Linux CI jobs | Virtual display for the editor-open pluginval requirement; a system tool, not shipped. |
| **GitHub Actions** (`actions/checkout`, `actions/upload-artifact`, `actions/dependency-review-action`, `github/codeql-action`, `microsoft/msvc-code-analysis-action`) | `.github/workflows/` | CI only. |

Installer tooling (Inno Setup, `pkgbuild`) is deliberately absent: v0.1.0 ships plain zips and
the packaging pipeline is deferred to the first commercial release (**OQ-007**).

---

## Open licensing decisions

These require an **owner/business decision** and are deliberately left open here. None of them
is resolved by this document.

1. **Obtaining the commercial JUCE 9 licence tier** — **OQ-002**. JUCE 9 modules are AGPLv3
   *or* commercial; the closed-source commercial product model cannot satisfy the AGPLv3 arm,
   so as a factual consequence a commercial JUCE licence must be in place before commercial
   distribution. Which tier, and its acquisition, remain owner/legal actions; nothing in this
   repository records that purchase.
2. **Anabasis's own licence.** The repository root has **no `LICENSE` file**, so the terms under
   which Anabasis's own source and binaries are offered are undeclared. [`NOTICE`](NOTICE) and
   `README.md` §Licensing state the product model; neither grants anything.
3. **An end-user licence agreement (EULA)** for the distributed binaries, if the product is to
   be sold. **None exists — not even a draft** (unlike the sibling product, which carries an
   explicitly unapproved one). Product-legal wording is owner-supplied, never invented here
   (constraint C8).
4. **Steinberg VST 3 requirements** for commercial distribution and trademark use — see §3.

All four are owner actions gated with OQ-002 in `docs/OPEN_QUESTIONS.md`; they are **not**
engineering tasks and cannot be closed by a code change.
