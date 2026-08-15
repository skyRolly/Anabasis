# ADR-0028 — The JUCE pin moves 9.0.0 → 9.0.1, and stops being the sibling's pin

> **✅ THE ARCHITECTURE REVIEW GATE IS CLEARED (2026-08-16).** `ARCHITECTURE_REVIEW_GATE.md` lists
> "**Build System change** — CMake structure, JUCE version/pin, C++ standard baseline, dependency
> set" among the changes that "must NOT be auto-merged even if CI, the self-tests, and pluginval all
> pass", and `DEPENDENCY_POLICY.md` upgrade rule 1 routes a JUCE bump to that gate **plus an ADR**.
> The owner directed this upgrade explicitly — "Update the project to JUCE 9.0.1", as the first step
> of a planned toolchain modernisation — which is the human decision the gate exists to require.
> This record is the ADR half.
>
> One thing the directive could not authorise, and this record says so rather than letting it pass:
> the bump **breaks the shared pin with Anamorph**, and no work in this repository can repair that,
> because Anamorph is a read-only reference here (`AI_AGENT_POLICY.md` §Cross-repository
> constraint). See §"What this costs the product family".

**Status:** **Accepted — 2026-08-16**, on the owner's directive. Version 0.1.5.

## Context

The pin was decided at P0 as a **product-family** decision, not a per-repository one. OQ-001
(2026-07-30) chose "the same JUCE the sibling product pins: 9.0.0", by the tag's immutable commit
SHA `f8f8864172464b9adf9eba6101e1f784838d1597`, and ADR-0008 wrote it into `CMakeLists.txt` as
`ANABASIS_JUCE_VERSION` + `ANABASIS_JUCE_TAG`. `DEPENDENCY_POLICY.md` states the corollary as a
rule: *"a future bump is a **product-family decision**, not a per-repo one: bumping only Anabasis
re-introduces exactly the divergence this pin removes."*

JUCE 9.0.1 is tagged at `e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8` (`git ls-remote --tags`,
2026-08-16). It is a patch release: **56** upstream commits
(`git rev-list --count f8f8864..e18f7f5`), two entries in `BREAKING_CHANGES.md`, seven headline
items in `CHANGE_LIST.md`.

## Decision

1. **`ANABASIS_JUCE_VERSION` becomes `9.0.1` and `ANABASIS_JUCE_TAG` becomes
   `e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8`.** The SHA pin, `GIT_SHALLOW`, and the
   `ANABASIS_JUCE_PATH` escape hatch are unchanged — this moves the pin, it does not change how the
   dependency is acquired.
2. **No source change is made to accommodate it.** Nothing in `src/`, `tests/` or `tools/` needed
   an edit to compile, link, pass or validate at the new pin; the only C++ edit in this change is a
   comment in `src/dsp/Latency.h` that named the old SHA as the tree its measured table came from.
3. **Neither of the two documented breaking changes reaches this project**, and §"The two breaking
   changes" records why, measured rather than assumed.
4. **The divergence from Anamorph is recorded as an open obligation**, not resolved by silence.

## What actually changed, at file level

The delta was taken as a diff between the two tags rather than read from the release notes, because
the release notes are a summary and the gate is about what compiles into this binary.

Anabasis links **fifteen** JUCE modules once transitive dependencies are counted — fourteen
compile into `libAnabasis_SharedCode.a` (one object directory each under
`build/CMakeFiles/Anabasis.dir/_deps/juce-src/modules/`), plus `juce_audio_plugin_client`, which
compiles into the format targets rather than the shared code. **Six of them are
byte-identical between `f8f8864…` and `e18f7f5…` apart from their module-declaration `version:`
string, and they are the six that decide how this plug-in sounds and what a host sees**:
`juce_dsp`, `juce_audio_basics`, `juce_audio_processors`, `juce_data_structures`,
`juce_audio_utils` and `juce_audio_plugin_client`.

```
$ git diff f8f8864 e18f7f5 -- modules/juce_dsp modules/juce_audio_basics \
      modules/juce_audio_processors modules/juce_data_structures \
      modules/juce_audio_utils modules/juce_audio_plugin_client | grep '^[-+][^-+]'
-  version:            9.0.0
+  version:            9.0.1        (× 6)
```

That single fact carries most of this record. **No DSP primitive, no filter, no oversampler, no
APVTS, no parameter class and no VST3/AU wrapper source moved**, so a change in rendered audio,
reported latency, parameter behaviour or state ABI is not merely untested-and-hoped-for — it has no
source to come from.

The other **nine** did change. Line counts below are diff lines excluding the two `version:` lines,
restricted to code a Linux/Windows/macOS build compiles:

| Module | Δ | Files | Relevance here |
|---|---|---|---|
| `juce_gui_extra` | 4411 | `juce_WebBrowserComponent.cpp`, `juce_WebControlRelays.h`, the JS→TS package move | Compiled inert: every target sets `JUCE_WEB_BROWSER=0`. Almost all of the 4411 is the relocated TypeScript package, which no compiler in this build reads |
| `juce_core` | 266 | `juce_XmlDocument.cpp`, `juce_Span.h`, `juce_MathsFunctions.h`, `juce_NamedPipe.cpp`, `juce_StandardHeader.h`, `juce_core.h` | **The XML reader is the only change in the whole delta that touches this product's own data** — see below. `Span` gains `Span<T>`→`Span<const T>` conversion and a tightened range constructor; `findNearestValue` is new (no name of ours collides — `grep` over `src/ tests/ tools/` finds neither identifier). `NamedPipe` fixes a unit-test-only collision. `StandardHeader` bumps `JUCE_BUILDNUMBER` 0 → 1 |
| `juce_gui_basics` | 221 | `juce_MouseInputSourceImpl.h`, `juce_XSymbols_linux.h`, `juce_XWindowSystem_linux.cpp`, `juce_Windowing_linux.cpp`, `juce_ComponentPeer.h`, Windows accessibility, `juce_CGMetalLayerRenderer_mac.h` | The Linux input/display fixes below, a gesture-position fix under global scaling, and a doc comment |
| `juce_audio_devices` | 146 | `juce_AudioDeviceManager.cpp`, `juce_CoreAudio_mac.cpp`, `juce_CoreMidi_mac.mm`, `juce_Midi_linux.cpp`, `juce_WASAPI_windows.cpp` | **Standalone-target surface only.** `AudioDeviceManager`'s rate/buffer selection is rewritten around the new `findNearestValue`, and it re-opens the device once if the available buffer sizes changed with the rate. The VST3/AU builds never construct an `AudioDeviceManager`; the DSP suite and state suite never open a device |
| `juce_audio_formats` | 82 | `juce_WavAudioFormat.cpp`, `juce_AiffAudioFormat.cpp`, `juce_FlacAudioFormat.cpp`, `juce_audio_formats.h` | Robustness against malformed WAV/AIFF input, and the new `JUCE_*_INCLUDE_PATH` config macros. **Anabasis never reads an audio file** — presets and sessions are XML — so these codecs are linked but never entered |
| `juce_graphics` | 77 | `juce_PNGLoader.cpp`, `juce_JPEGLoader.cpp`, `juce_graphics.h`, `juce_graphics_libjpg_1.c`, `juce_Fonts_freetype.cpp`, `juce_Direct2DGraphicsContext_windows.cpp` | Vendored-codec include-path configurability (defaults unchanged), a misplaced `#endif` in the libjpeg unity file, weak-linked FreeType symbols so older distros load, and a Direct2D seam fix at fractional display scale (Windows) |
| `juce_audio_processors_headless` | 61 | `juce_AudioUnitPluginFormatImpl.h` | AU **hosting**, macOS-only, and this project hosts nothing in the shipped binary (`AnabasisChannelProbe` hosts, and it loads VST3) |
| `juce_opengl` | 42 | `juce_OpenGLContext.cpp`, `juce_OpenGL_linux.h` | The context is activated before stale state is cleared; the EGL path accepts `EGL_EXT_platform_x11` as well as `EGL_KHR_platform_x11`. The module is linked unconditionally (ADR-0008) but ADR-0011 never attaches a context on Linux, so neither is reachable here |
| `juce_events` | 38 | `juce_Messaging_linux.cpp`, `juce_MessageManager_mac.mm` | The Linux message queue stops draining without bound: the fd callback now breaks after 100 ms so it cannot starve the other `LinuxEventLoop` callbacks — the X event pump among them. One of the three fixes behind CHANGE_LIST's "Fixed unresponsive Linux GUIs" |

### The one change that touches this product's own data

`XmlDocument::readChildElements` previously skipped only `<!-- comments -->` inside a character
block; 9.0.1 factors comment-and-processing-instruction skipping into one helper and uses it in both
places, so a `<? … ?>` **inside a text block** is now skipped rather than ending the block. The
unterminated-comment error string changes from `"unterminated comment"` to
`"unexpected end of stream"`.

This is a **widening of reader tolerance for a shape no producer emits**, which is the same
argument ADR-0026 made and worth stating in the same terms: `getStateInformation` writes
`ValueTree::createXml()` through `copyXmlToBinary`, and `PresetManager` writes user `.anabasis`
files the same way. `XmlElement::toString` emits no comment and no processing instruction anywhere
inside an element — only the `<?xml …?>` header ahead of the root, which both versions consume in
`skipNextWhiteSpace`. So **no blob this plug-in has ever written reaches the changed branch**, and
nothing that parsed at 9.0.0 fails to parse at 9.0.1. It is not a Serialization Registry change: no
field is added, removed or given a new meaning (`SERIALIZATION_REGISTRY.md` is untouched, and the
raw-exact round-trip and legacy-fixture tests are green).

### The Linux fixes, and why they are named here rather than waved through

Three of them each land on a branch of the decision tree **KI-012** wrote for itself — the field
report that the Linux editor accepts no mouse input, which has never reproduced in this repository:

- **`DynamicLibrary xinputLib { "libXi.so" }` → `{ "libXi.so.6" }`.** Every other X11 helper JUCE
  dlopens is already a SONAME (`libX11.so.6`, `libXext.so.6`, `libXcursor.so.1`,
  `libXinerama.so.1`, `libXrender.so.1`, `libXrandr.so.2`); XInput alone asked for the *unversioned*
  name, which is the symlink the **`-dev` package** ships and an end-user machine does not have.
  9.0.1 also changes `XIQueryVersion`'s stub result from a default-constructed `Status` to
  `BadRequest`, and null-checks `xiQueryDevice`'s return.
- **`vBlankManager.getTimerInterval() != frequencyToUse`** compared a value in **milliseconds**
  against one in **hertz**, and re-started the timer through `startTimerHz`. 9.0.1 converts to a
  period first and compares ms to ms.
- **The message-queue starvation fix** in `juce_events` above.
- (Related: `findDisplays` no longer requires `_NET_WORKAREA` before consulting XRandR, and guards a
  division by a zero `hTotal`/`vTotal`.)

**This ADR does not claim KI-012 is fixed.** The report does not reproduce here, this container is
not the reporter's machine, and no experiment in this change tested the reporter's configuration.
What is recorded — in `KNOWN_ISSUES.md`, at KI-012, with the code-level reasoning — is that three
independent candidate mechanisms which were live at 9.0.0 are closed at 9.0.1, and which
observation would tell them apart if the report recurs.

## The two breaking changes

`BREAKING_CHANGES.md` gained two entries for 9.0.1. Neither reaches this project:

1. **zlib, libjpeg, libpng and libflac build in C language mode.** The project already declares
   `project(Anabasis … LANGUAGES C CXX)` and already compiled the C unity files at 9.0.0
   (`juce_core_zlib.c`, `juce_graphics_libpng.c`, `juce_graphics_libjpg_*.c`,
   `juce_audio_formats_flac_*.c` are all `.c` in both trees). The stated hazard is an ODR/symbol
   collision with a *separately linked* copy of those libraries; Anabasis links none — the four
   arrive only through JUCE modules, as `THIRD_PARTY_LICENSES.md` §2 records — so the escape hatches
   the entry documents (`JUCE_INCLUDE_ZLIB_CODE` and friends, now joined by matching
   `JUCE_*_INCLUDE_PATH` macros) are not needed and are not set.
2. **The WebBrowserComponent JS interop package moved to
   `native/typescript/webview-interop`.** `JUCE_WEB_BROWSER=0` on every target; nothing here imports
   that package. The one build-system consequence is invisible: `juce_add_module` now reads that
   package's `package.json` with `string(JSON …)` to define `JUCE_WEBVIEW_INTEROP_LIBRARY_VERSION`,
   which needs CMake ≥ 3.19 — the project already requires 3.22.

## What this costs the product family

The shared pin bought four things (`DEPENDENCY_POLICY.md` §Version-lock reasoning). This bump keeps
one of them and suspends three:

| Property | After this bump |
|---|---|
| Builds are reproducible from an immutable revision | **Kept** — the mechanism is unchanged, only the revision moved |
| One dependency audit covers both products | **Suspended** — `THIRD_PARTY_LICENSES.md` was re-derived here against `e18f7f5…`; Anamorph's remains a `f8f8864…` document |
| A Level-5 audition of one is a baseline for the other | **Suspended** — formally. Bounded in practice by the byte-identity of every audio-path module above: an audible difference between the products cannot be JUCE-attributable |
| A JUCE-attributable behaviour difference is impossible by construction | **Suspended** — it is now merely *unlikely*, and it cannot be an audio, parameter or state difference at all: the six modules that decide those are byte-identical. What is left is the nine in the table above — GUI, platform-native, file-format and web-view |

**The obligation this leaves open:** re-converge the two pins. That is a decision for the sibling's
repository and cannot be taken from this one — `CLAUDE.md` and `AI_AGENT_POLICY.md` both make
Anamorph read-only from here, so a change set that "fixed" the divergence by editing Anamorph would
violate a standing constraint to satisfy a policy sentence. The honest resolution is to record the
divergence where a reader of either repository's dependency documentation will meet it, which is
what `DEPENDENCY_POLICY.md`, `HANDOVER.md` §Critical dependencies, `COMPATIBILITY_MATRIX.md` and
`README.md` now do, and to leave the sibling's bump to whoever owns that repository.

## Alternatives

- **Stay at 9.0.0.** Rejected: the owner directed the upgrade, and it is the first step of a stated
  toolchain modernisation. Worth recording anyway that the *technical* case for 9.0.1 on its own
  merits is not neutral — the three Linux fixes above are on the exact surface KI-012 lives on.
- **Bump both products together, as `DEPENDENCY_POLICY.md` prescribes.** Not available from this
  repository at all; see above. This is the alternative that would have been correct and is
  foreclosed by a different rule, which is why it is written down rather than omitted.
- **Wait for Anamorph to bump first, then follow.** Rejected: it inverts the directive, and it
  leaves the decision waiting on a repository this work has no authority over. The divergence is
  the same size in either order.
- **Bump and say nothing about the sibling.** Rejected explicitly. The pin's whole documented
  purpose was that it was shared; a bump that leaves `README.md` saying "the same revision the
  sibling product Anamorph pins" is a false statement in the first file a contributor reads.

## Verification

Run at the new pin on Linux x86-64 (GCC 13.3, Release, Ninja), against a **fresh** build tree — see
the note in `BUILD.md`: `ANABASIS_JUCE_TAG` is a cache entry, so reconfiguring an existing `build/`
keeps fetching the old SHA silently.

- **Both suites green, at the same counts as 9.0.0**: `AnabasisTests` 296 + `AnabasisStateTests`
  845 = **1141 checks, 0 failures**. Same source, same project version, one variable changed.
- **`AnabasisChannelProbe` against the built VST3 bundle**, 33 configurations across
  {48 kHz, 44.1 kHz} × {512, 64} plus the macro-with-editor scenario, per-channel output printed to
  nine decimal places: **identical at both pins, digit for digit.** This is the DEPENDENCY_POLICY
  rule-2 concern ("a JUCE change can move DSP/latency/editor behaviour invisibly to the headless
  gate") answered with the shipped bundle rather than with the console apps.
- **pluginval** at the `build.yml` strictness, both modes × 3, editor under `xvfb`.
- **valgrind memcheck on both suites** (`--track-origins=yes --error-exitcode=1`, the CI
  invocation), rebuilt unsanitized at the new pin: **0 errors from 0 contexts** on each. This is
  the pipeline's only uninitialised-read detector, and it is the one gate whose result could not be
  predicted from the diff — `juce_core`, `juce_graphics` and `juce_gui_basics` all changed, and the
  state suite is what drives the real wrapper `processBlock` and constructs the editor.
- **The Clang leg, which is the one that matters for KI-009's class of defect.** `build-clang`
  (Clang 18, Release, the CI job's target set including the LTO'd `Anabasis_VST3`) builds with
  **no first-party warning** — `check-clang-warnings.py` gates by resolved path and its self-test
  passes first, so its silence is meaningful — the portability compile canary still finds the
  `SIMDRegister` hazard it guards in the 9.0.1 tree, both suites are green from that build, and the
  probe run against the **Clang-LTO'd bundle** agrees with the GCC one digit for digit. INC-004 is
  the reason this leg exists at all: a JUCE bump changes what the optimiser sees, and the
  configuration that ships on macOS is Clang at `-flto`.
- **The rule-7 register was re-walked**, and every one of its rows is intact for a reason stronger
  than re-reading them: `juce_PopupMenu.cpp`, `juce_ComboBox.cpp`, `juce_TextEditor.cpp`,
  `juce_Component.cpp`, `juce_MouseInputSource.cpp`, `juce_ModalComponentManager.cpp`,
  `juce_TooltipWindow.cpp`, `juce_Button.cpp`, `juce_LookAndFeel_V2.cpp` and
  `juce_LookAndFeel_V4.cpp` are **unchanged between the two tags**, so every assumption those rows
  record — and every `juce_*.cpp:line` citation in `src/` and `docs/` — still points at the same
  bytes. The one file in that neighbourhood that did change, `juce_MouseInputSourceImpl.h`, changed
  only `getTargetForGesture`, which is on the magnify-gesture path and not on
  `setComponentUnderMouse`, the function the `isMouseOver` row turns on.
- **Third-party attribution re-verified** per rule 6, by the procedure `THIRD_PARTY_LICENSES.md`
  prescribes rather than by inspection: JUCE's `LICENSE.md` and every cited licence file are
  byte-identical across the two tags, and the symbol probes were re-run against the new build's
  object files. The only licence files added anywhere belong to JUCE's own new npm package, which
  no compiler in this build reads.
- **`RELEASE_COMPATIBILITY_CHECKLIST.md` re-run** per rule 3 for everything the headless gate can
  answer — registry snapshot, schema round-trip, latency across the lookahead × oversampling
  matrix, ceiling, metering accuracy, pluginval. Its four manual items (host matrix, automation
  playback, cross-version session reload, "presets sound identical") are unchanged in status: they
  were owed before this bump and are owed after it, and rule 2's **Level-5 audition** is owed
  specifically *because* of it.

## Consequences

- **`ANABASIS_JUCE_TAG` is a CACHE entry.** Anyone with an existing build tree keeps building
  against 9.0.0 until they delete it or override the variable, and nothing warns them. Recorded in
  `BUILD.md` beside the pin.
- **`DEPENDENCY_POLICY.md`'s compliance log stops being empty.** It was written as "*(empty — no
  dependency bump has occurred. Every future bump is recorded here with its ADR and the rule-2
  verification result.)*" This is the first entry, and the rule-2 result it carries is
  deliberately partial: the headless half is done and the audition half is not.
- **The 3-OS half of rule 2 is CI's**, not this machine's. Everything above is Linux; the Windows
  and macOS suites and pluginval runs land with this commit's CI run, and the two platform-specific
  changes in the delta (Direct2D fractional-scale seam, `CGMetalLayerRenderer` nil guards) are on
  their side of that line.

## Related code
- `CMakeLists.txt:80-81` (the two cache variables)
- `src/dsp/Latency.h` (the OS latency table's provenance comment)

Evidence [Verified]:
- Source: `CMakeLists.txt:80-81`
- Test:   `AnabasisTests` + `AnabasisStateTests` (1141 checks) and `AnabasisChannelProbe`, both at
  the new pin; the probe's 33-configuration output compared digit-for-digit against the same
  binary built at the old pin
