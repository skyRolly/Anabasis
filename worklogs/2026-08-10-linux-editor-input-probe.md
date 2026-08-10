# Worklog — Linux editor input probe (2026-08-10)

Session-local evidence trail for `docs/KNOWN_ISSUES.md` KI-012 (owner report: on Linux no
control responds to a click and hovering produces no reaction). Raw investigation material, NOT
architecture documentation — `docs/SOURCE_OF_TRUTH.md`: worklogs are never cited as policy.

The point of recording it is negative coverage. Every probe below **passed**, so none of it
belongs in the suites — a test that pins behaviour the fault never exhibits pins nothing. What
it is good for is stopping the next round from re-running it.

---

## Harness

Three temporary programs, none of them in the build:

1. **`Xvfb :91 -screen 0 1600x1200x24`** — a real X server, so the pointer, the window tree and
   the damage/expose machinery are the real ones. Repeated **without** a window manager and
   under **`twm`**, so both the bare case and the reparenting-frame case are covered.
2. **A minimal JUCE 9.0.0 VST3 host** — `juce::AudioPluginFormatManager` +
   `juce::VST3PluginFormat`, the plug-in's editor in a `juce::DocumentWindow`. This is the point
   of the exercise: it puts the editor through the **same XEmbed path a Linux DAW uses**, which
   an in-process `getComponentAt` probe cannot reach. It polls the host-side parameter values
   every 300 ms and prints the ones that move, which is an oracle *outside* the plug-in.
3. **XTEST injectors** — `XTestFakeMotionEvent` / `XTestFakeButtonEvent`, i.e. events
   manufactured by the X server itself, not `ComponentPeer::handleMouseEvent` calls. Plus an
   `XGetImage` pixel-differ, so "did it repaint" is answered from the framebuffer.

The X11 window tree under `twm`, which is itself a result — it shows the embedding actually
happening, and it shows there is exactly one plug-in window:

```
0x200141  942x767  @327,224   twm frame
0x600004  942x748  @0,19      host DocumentWindow
0x600005  940x720  @1,27      host-side XEmbed container
0x40000d  940x720  @0,0       the plug-in's own X11 window   <- no GL child under it
```

## Results

| Probe | Expectation if the report holds | Measured |
|---|---|---|
| Standalone, click the ADV toggle at editor (809, 23) | nothing | content height 720 → 822 |
| VST3 in the host, no WM, same click | nothing | same resize |
| VST3 in the host, no WM, rotary drag at (300, 120), Δy = −60 | nothing | `Loudness` 0.000 → 0.228; `Comp Ratio`, `Comp Threshold`, `Limiter Gain` follow through the macro map |
| VST3 in the host **under `twm`**, same drag | nothing | identical |
| Pointer parked in a corner, two `XGetImage` grabs 1 s apart | — | 0 pixels changed (correct: no audio, so the meters are still — this is the control that proves the differ is not just reporting noise) |
| Pointer moved onto a knob, grab again | 0 | **26 861** pixels changed |

Clicks land, drags reach the parameters, hover repaints. The reported symptom is absent from
every configuration reachable here.

## What the earlier in-process probe had already excluded

Before the XEmbed harness, a probe inside the state-test binary built the editor, made it
visible and walked `getComponentAt` over a grid: no visible child larger than half the frame,
and every sampled point resolved to a real control (`juce::TextButton`, the editor's `Knob`,
`juce::ToggleButton`, `LoudnessMeterView`, `GrHistoryView`, `juce::Label`). That excluded the
z-order / `setInterceptsMouseClicks` / overlay class, which is why this round went to the peer
and embedding layer instead.

## Sibling comparison (the owner's explicit ask)

Read-only pass over `/home/user/Anamorph` (ADR-0009 governs reuse; the repository is never
modified). Interaction-relevant constructs, both editors:

| Construct | Anabasis | Anamorph |
|---|---|---|
| OpenGL attach | `#if JUCE_MAC \|\| JUCE_WINDOWS` (`src/gui/PluginEditor.cpp`) | `#if ! (JUCE_LINUX \|\| JUCE_BSD)` (`src/PluginEditor.cpp`) — ADR-0011 / INC-006 / KI-003 |
| `juce::TooltipWindow` | `{ nullptr, 600 }`, `setOpaque (false)` under `JUCE_MAC` | identical |
| Editor opacity | `setOpaque (true)` | identical |
| UI scale | `setSize` then `setTransform (scale (hostScale × uiScale))` | identical |
| `setScaleFactor` override | `hostScale = newScale > 0 ? newScale : 1; applyUiScale();` | identical |
| `EDITOR_WANTS_KEYBOARD_FOCUS` | `FALSE` | `FALSE` |
| Hardening / LTO / warning flag targets | `juce_recommended_{config,lto,warning}_flags`, `--gc-sections`, `relro`, `now`, `noexecstack` | identical |
| Full-frame overlays | `dimOverlay` never intercepts; three `Backdrop`s are `addChildComponent`, shown only on an explicit click | same shape |

**One divergence, and it is not on the input path:** the sibling declares its
`juce::OpenGLContext` member on every platform and gates only `attachTo`; this editor compiles
the member out of the Linux build entirely. Both therefore run Linux without GL — and the window
tree above confirms it at runtime rather than in the preprocessor, since the plug-in owns one
X11 window with no GL child beneath it.

## Where this leaves the report

The fault is real for the reporter and absent in every harness here, so on this evidence the
difference is environmental, not in the component tree, the hit-testing, the overlay z-order or
the GL gate. The discriminating datum is named in KI-012: **do the meters move while audio
plays?** On Linux JUCE drives `dispatchDeferredRepaints()` from the same vblank timer that feeds
`juce::VBlankAttachment` (`juce_gui_basics/native/juce_Windowing_linux.cpp`, `onVBlank()`), so a
dead timer takes the whole editor's repaint with it and would read as "nothing reacts" even
while clicks are landing. Moving meters with dead controls is the opposite fault — input
routing — and points somewhere else entirely.
