# Anabasis — User Manual

*The plug-in's exact version and build number are shown on the About screen (click the
**ANABASIS** title).*

**New here? Go straight to [§2 Quick start](#2-quick-start).**

### Contents

1. [Introduction](#1-introduction) — what Anabasis is and the idea behind it
2. [Quick start](#2-quick-start) — install, first launch, your first loudness push
3. [The interface](#3-the-interface) — every panel and control
4. [The adaptive engine, Learn and Freeze](#4-the-adaptive-engine-learn-and-freeze)
5. [Simple mode and Advanced mode](#5-simple-mode-and-advanced-mode)
6. [Signal flow and latency](#6-signal-flow-and-latency)
7. [Presets and A/B](#7-presets-and-ab)
8. [Workflow examples](#8-workflow-examples)
9. [FAQ & troubleshooting](#9-faq--troubleshooting)

Installation is covered separately in the **[Installation guide](INSTALLATION.md)**. If
something is wrong and this manual doesn't answer it, **[KNOWN_ISSUES.md](../KNOWN_ISSUES.md)**
lists every confirmed limitation with its current status.

---

## 1. Introduction

### What Anabasis is

Anabasis is a **mastering loudness maximizer** by RollyTech: one large **Loudness** knob
driving an adaptive chain of compression, clipping/saturation and true-peak limiting, with
**loudness-compensated monitoring** built in — so you can hear whether a push actually sounds
*better*, not merely *louder*.

It is a stereo audio *effect* (no MIDI): VST3 on all platforms, Audio Unit on macOS, plus a
Standalone application. Three I/O layouts: **stereo→stereo**, **mono→stereo** (a mono source
is duplicated to both channels before mastering; since 0.1.1) and **mono→mono** (the same
processing on one channel — for dual-mono and multi-mono racks; since 0.1.2).

### Who it is for

Mastering and mixing engineers who need competitive loudness without losing punch or tone;
producers who want a finished-sounding master bus with one honest control.

### The concept in one paragraph

The **Loudness** knob does not map to a single gain — it drives the whole chain through an
**adaptive engine** that listens to your programme material (short-term loudness, crest
factor, spectral tilt, transient density) and continuously, *slowly* trims how the stages
share the work: light compression and transparent limiting low on the dial, the clipper
absorbing transients in the middle, saturation color and a dynamic high-frequency tame at
the top. The **Ceiling** is a limit the output never exceeds — a sample-peak limit as shipped,
and a *true-peak* one once you engage **TP**, at sample rates of 12 kHz or more (§3.2). **MATCH**
plays the processed signal back at matched loudness so the level increase cannot flatter
you, and **Delta** lets you listen to exactly what the processing is removing.

---

## 2. Quick start

### 2.1 Install

Follow the **[Installation guide](INSTALLATION.md)** — v0.1.x ships as plain per-platform
ZIPs (no installers yet), so there are two or three copy commands per platform, plus a
one-time security-warning step on Windows/macOS.

**What you need.** A 64-bit machine — Windows x86-64, macOS (Apple Silicon or Intel; the
build targets macOS 10.13 and later on Intel, 11.0 and later on Apple Silicon), or x86-64
Linux — and a VST3 or (on macOS) AU host. Anabasis follows whatever sample rate and buffer
size your host uses, makes no network connections, and needs no account or activation.

### 2.2 First launch

**Rescan your plug-ins.** Most DAWs only look for new plug-ins on demand:

| DAW | Rescan |
|---|---|
| REAPER | *Options → Preferences → Plug-ins → VST → Re-scan* |
| Ableton Live | *Preferences → Plug-Ins → Rescan* |
| Cubase / Nuendo | *Studio → VST Plug-in Manager → Rescan All* |
| FL Studio | *Plugin Manager → Find installed plugins* |
| Logic Pro / GarageBand | validates AUs automatically on launch |
| Bitwig | *Settings → Locations → Plug-in Locations* |
| Ardour | *Preferences → Plugins → Scan for Plugins* |

Anabasis is **64-bit only**, and it is an audio **effect** — look under effects/audio FX,
not instruments. On macOS, Logic Pro and GarageBand load the **AU** (`.component`); every
other DAW uses the **VST3**.

### 2.3 Load it on the master

Insert Anabasis on a **stereo** track — normally the master bus, or a mix bus. (A mono
source also works: it is duplicated to both channels; the *output* is always stereo.)
You'll see the **Simple** view: the
top bar, the big **Loudness** knob with **Ceiling**, **Character** and **Tone** around it, a
toggle row, and the metering strip.

### 2.4 Your first push

1. Play the loudest section of your track.
2. Raise **Loudness** until the gain-reduction history shows steady, musical work — watch
   the **out LUFS** readout climb. (The graph well opens on the **GR history** — the
   GR|SPEC pill in its bottom-left corner toggles to the spectrum and back — §3.4.)
3. Switch **MATCH** (loudness-compensated monitoring) on. The level jump disappears; what
   you hear now is the *sound* of the processing at matched loudness. If it still sounds
   better, keep going; if it only sounded better because it was louder, you just found out.
4. Press **DELTA** to hear exactly what is being removed — transient tops, mostly. Short,
   dry ticks are healthy; tone or vocal body in the delta means you are pushing too hard.
5. Check the **Ceiling** (default −0.1 dB) against your delivery spec — and if that spec is
   written in **dBTP**, engage **TP** beside it, which is what makes the number mean dBTP
   at 12 kHz and up (§3.2). The readout's unit follows the switch and the sample rate, so it
   always says which one you have: `dBTP` only where TP holds the Ceiling in dBTP, `dB`
   otherwise — below 12 kHz even with **TP** on.
6. **Bypass** in the top bar A/Bs against the untouched signal — with MATCH on, that
   comparison is loudness-matched too: MATCH brings the processed signal down to the
   input's loudness and Bypass plays the input as it is (since 0.2.14 — before that, MATCH
   turned the bypassed signal down by the same amount, so the jump stayed). MATCH errs
   slightly low: the matched signal settles a fraction of a LU under the input on typical
   material, up to about 1.7 LU on very hot material ([`KNOWN_ISSUES.md`](../KNOWN_ISSUES.md)
   KI-023). MATCH and DELTA are listening aids only — an offline bounce or export never
   includes them.

### 2.5 The Standalone application

Anabasis also runs as a **standalone app** (`Anabasis.exe` on Windows, `Anabasis.app` on
macOS, `Anabasis` on Linux) — the same plug-in with its own audio device, useful for
checking a file or a live input without a DAW. Pick your audio hardware in the standalone's
audio settings on first launch. The Windows build does not include ASIO (the ASIO SDK is not
redistributable); Windows uses the system audio backends, Linux uses ALSA/JACK/PipeWire.
There is no host, so there is no automation and no session to save into; presets are shared
with the plug-in versions ([§7.2](#72-saving-and-managing)).

### 2.6 Where to go next

- What the adaptive engine is doing under the knob, and how to lock it: [§4](#4-the-adaptive-engine-learn-and-freeze).
- Per-stage control (compressor, clipper, limiter, EQ): press **ADV** — [§5](#5-simple-mode-and-advanced-mode).
- A starting point rather than a blank slate: thirteen factory presets — [§7](#7-presets-and-ab).
- Concrete recipes: [§8](#8-workflow-examples).
- Something not working? [§9](#9-faq--troubleshooting).

---

## 3. The interface

Anabasis has two views: **Simple** (the big knob and its satellites) and **Advanced**
(per-stage zones over the same sound). Toggle with **ADV** in the top bar — switching views
**never changes the sound** ([§5](#5-simple-mode-and-advanced-mode)).

Universal gestures:

- **Knobs**: drag to change; **double-click or Alt/Option-click to reset** to default (one
  undoable step).
- **Value boxes** (the number under a knob): **drag vertically** to change, double-click to
  type. Typed entry speaks the shorthand you would say out loud: `2k`/`2kHz` means 2000 Hz, and in
  a **percent** box a number between 0 and 1 is a fraction — `0.5` is 50 %, `1` is 100 %. Add the
  `%` when you mean the literal percent, which is how you reach values below one: `0.1%` is a tenth
  of a percent.
- **Tooltips**: every control has one, but they are **off by default** — enable them in
  Settings.

### 3.1 Top bar

| Control | What it does |
|---|---|
| **ANABASIS** title | Opens the About overlay (version, build number). Click anywhere to close. |
| **‹ Preset name ›** | Steps through presets (wraps at the ends). Clicking the name opens the preset menu (§7). An `*` after the name means the sound no longer matches the loaded preset. |
| **A / B** | Two independent sound slots for comparing settings; click to switch (§7.4). |
| **Copy** | Copies the current slot's sound into the other slot. |
| **↶ / ↷** | Undo / Redo — kept **per A/B slot**. Covers sound parameters, preset loads, Copy (on the destination slot) and the ADV view switch; bypass and the monitor toggles are never recorded. |
| **Settings (gear)** | Opens the Settings overlay (§3.5). |
| **ADV** | Switches Simple ↔ Advanced view (§5). |
| **BYPASS** | Click-free bypass: plays your input untouched, at its own level. With **MATCH** on, the comparison is loudness-matched, because MATCH brings the processed signal to the input's loudness (since 0.2.14). Hosts also see this as the standard bypass parameter. |

### 3.2 Simple view

| Control | Range | What it does |
|---|---|---|
| **Loudness** | 0 … 100 | The big knob: how hard the adaptive chain pushes (§4). At 0 it applies no push — but the Ceiling still holds, so anything already hotter than it is still limited. |
| **Ceiling** | −20 … 0 dB, default −0.1 | The output limit — nothing leaves the plugin above it. Two toggles sit beside it. **TP** decides what the number *means*: off (the default) the limit is on **sample peaks** and the readout says `dB`; on, the limiter detects true peaks and the final ceiling clamp holds the waveform *between* the samples too, at every Oversampling setting — inter-sample peaks are caught and the readout says `dBTP`. A Ceiling move glides over 20 ms, and every sample is held to the value in force at the moment it leaves the plugin, with TP on as with TP off (since 0.2.15; before, TP mode lagged a lowered Ceiling by under a millisecond and could overshoot it by up to ~3 dB during the glide). TP holds at every sample rate from 12 kHz up (since 0.2.16; before, a constructed worst case could read up to about 0.2 dB over below 44.1 kHz). The dBTP ceiling needs 12 kHz or more. Below that — 8 and 11.025 kHz included — the Ceiling holds sample peak, not dBTP, whatever the **TP** switch says, and its readout says `dB` even with **TP** on (the **TP** and Ceiling tooltips name the 12 kHz boundary there instead of promising dBTP); inter-sample peaks can then exceed it, and the STATISTICS **TP** row (§3.4) shows by how much. With **TP** on at Oversampling Off or 2× the limiter still reacts to inter-sample peaks there — a best effort, not a guarantee. It is the same parameter as the Advanced limiter zone's TP switch (§3.3). **LOCK** keeps the ceiling's *value* fixed while you browse presets — the value only: a factory preset still turns TP (and Dither) off (§7.3). |
| **Character** | 0 … 1 | Clean ↔ Color: how much of the push is done with saturation character rather than clean limiting. |
| **Tone** | −1 … +1 | Dark ↔ bright tilt of the overall result. |

The toggle row underneath: **MATCH** (loudness-compensated monitoring), **DELTA**
(difference monitoring — solo what the processing removes), **FREEZE** and **LEARN** (§4),
plus the live **out LUFS** readout. A small **edited dot** appears when Advanced edits have
detached parameters from the macros — clicking it returns everything to the macro sound
(one undoable step; §5).

### 3.3 Advanced view

Four zones over the same parameter model — **COMP**, **CLIP / COLOR**, **LIMITER**,
**EQ** — plus the utility row and the metering well. (Until 0.1.2 a read-only mirror of the
three macro knobs sat between them; it was display-only and has been removed — the macros
live in the Simple view, and the accent detach dots on the zone knobs still show which
controls Advanced edits have taken off the macro curves.) Each stage's controls (ranges are
the registry's, shown on each control; inside a zone the captions drop the stage prefix —
the automation lane keeps the full name, so "Ratio" here is "Comp Ratio" to your DAW):

- **COMP** — the mastering glue compressor: Ratio (1.1–4:1), Threshold, Attack (5–100 ms),
  Release (50–1000 ms) with **AUTO** (program-dependent two-stage release), Knee (softens
  the onset **above** the threshold — at or below it the compressor computes nothing, so
  the default threshold of 0 dBFS means no reduction on any legal level), RMS/Peak
  detector, Mix (parallel compression), **Stereo Link** ("Comp Stereo Link" in automation —
  how much both channels share one gain; full link keeps the image stable, lower lets each
  channel breathe on its own; since 0.1.1), and its gain-reduction meter — **two lanes
  since 0.1.2, L above R**, which read identically at full link and diverge below it.
- **CLIP / COLOR** — the transient-absorbing clipper and the color stage: Shape
  (hard ↔ soft, with a live transfer-curve display), Drive (level-compensated), Mix,
  **Color** model (Clean / Tape / Tube / Transistor), Odd/Even harmonic balance, Color
  Tone, Color Depth, and **Dynamic Tame** — a programme-dependent high-frequency softener.
- **LIMITER** — the true-peak lookahead limiter: Gain ("Limiter Gain" in automation — the
  push into it), Lookahead (0.5–10 ms; with **TP** on, the longest settings engage a little less —
  9.0 ms at the top at 48 kHz, less at lower rates, down to 6.2 ms at 12 kHz — because the ceiling's true-peak detection takes its share of the
  fixed 10 ms, so the reported latency never changes; below 12 kHz the whole 10 ms goes to the limiter, **TP** on or off; §6), Release ("Lim Release") with **AUTO**, **Style**
  (Transparent / Punchy / Loud), Stereo Link ("Limiter Stereo Link"), Transients (transient
  preservation), **TP** (true-peak mode — **off by default**; on, the limiter detects true peaks
  and, at sample rates of 12 kHz or more, the final ceiling clamp holds inter-sample peaks at
  every Oversampling setting, so the Ceiling becomes a dBTP limit; below 12 kHz the Ceiling holds
  sample peak, not dBTP, §3.2),
  and its two-lane L/R gain-reduction meter. The **SC HPF** (20–300 Hz) keeps low-frequency
  energy from pumping the **compressor's** detector; since 0.1.2 the limiter's detector is
  deliberately unfiltered — its job is the Ceiling, so it always sees the true peak, and a
  bass-heavy over is limited rather than left to the safety clamp.
- **EQ** — Tilt (±3 dB around ~700 Hz), low shelf, high shelf, two bells (Freq/Gain/Q),
  and the **Pre / Post** position switch (before the compressor, or after the limiter —
  either way the ceiling still holds), with a live response curve.
- **Output** — Input Gain (−12…+24 dB) and **Dither** (Off / 16-bit / 24-bit TPDF, with
  optional noise **SHAPE**) for final exports.

### 3.4 The metering strip

Always along the bottom:

The **STATISTICS** panel — the same eight readings in both Simple and Advanced:

| Row | What it is |
|---|---|
| **M** | Momentary loudness, the newest 400 ms (BS.1770). |
| **S** | Short-term loudness, the last 3 s. |
| **I** | Integrated loudness over the whole measurement. Which revision it follows is a Settings choice (§3.5). |
| **TP** | True peak in dBTP, max hold. It always measures true peak, whether or not the limiter's TP mode is engaged — so it is the honest check on a sample-peak ceiling, and it turns red above your Ceiling. With **TP** on, at 12 kHz and up, it can turn red while it reads the Ceiling itself: the hold is then a few thousandths of a dB above it, well inside the 0.1 dB tolerance ([`KNOWN_ISSUES.md`](../KNOWN_ISSUES.md) KI-020). Below 12 kHz the Ceiling holds sample peak, not dBTP (§3.2), so there a red **TP** reading means inter-sample peaks really are over the Ceiling, **TP** on or off. |
| **SP** | Sample peak in dBFS, max hold. Read it against TP: the gap between them **is** the inter-sample overshoot. |
| **RMS** | RMS level over a 50 ms Hann window. The reference is a Settings choice (§3.5). |
| **LRA** | Loudness Range in LU (EBU R128 / Tech 3342) — how much the loudness moves across the programme. A steady master reads near 0; a dynamic one reads 8–15. |
| **PLR** | Peak-to-loudness ratio: TP minus the **I** row above it — so it follows the same BS.1770 revision that row does (§3.5). A crest/dynamics at-a-glance number. |
- **The graph well** — one panel, two switchable views, in both Simple and Advanced:
  - **Spectrum** — the input/output spectrum overlay (input dim, output in the accent).
  - **GR history** — a scrolling trace of recent gain reduction, the fastest way to see
    how hard and how often the limiter is working.

  A fresh instance opens on the **GR history** (since 0.1.2). The GR|SPEC pill in the
  graph's bottom-left corner shows both views with the active one lit; clicking the pill —
  anywhere on it — toggles to the other view. The choice is session state, saved with your
  project, so it reopens on whichever you left it. The history draws at a fixed scale: a
  fresh instance grows its trace from the right edge, and the region to the left stays
  empty until twenty seconds of audio have actually been measured — nothing is estimated
  or stretched. Twenty seconds means twenty seconds of audio whatever size of buffer your
  host hands the plugin, and whether or not that size stays the same: the timeline follows
  the audio rather than the callback rate (since 0.2.12). What decides whether the full
  twenty seconds fit is how many buffers a second your host runs — sample rate divided by
  buffer size — because the history records one point per buffer of processed audio. Up to
  about **13 000 buffers a second** you get the whole twenty seconds, and every ordinary
  configuration is far below that: 48 kHz at 8 samples is 6000, 192 kHz at 16 is 12 000,
  and 384 kHz at 32 is 12 000 (since 0.2.12 round 19 — it was half that before, so
  192 kHz at 16 and 384 kHz at 32 used to show about eleven seconds). Past that point the
  history holds as much as it can rather than twenty seconds, shortening smoothly in
  proportion — around eleven seconds at 384 kHz with 16-sample buffers — and the trace
  still draws the audio it holds, to scale, with nothing estimated or stretched. Anabasis
  does not refuse or alter any sample rate to keep this promise; the promise is stated in
  buffers a second so that it stays true at rates faster than any of these. The trace scrolls continuously — it advances a fraction of a pixel with
  every processed block rather than stepping once per bucket (since 0.2.8). Each point of
  the trace is drawn once, when every block it summarises has been processed, and is never
  redrawn: a new point enters at the right edge as each group of blocks completes, and what
  is already on screen only moves (since 0.2.11). The trace fills the graph edge to edge, the same
  width the spectrum view uses: the group of blocks still being collected is held just off the
  right-hand end rather than shown incomplete, and each completed group enters the visible trace
  from there (since 0.2.12). Pausing and
  resuming continues the timeline; it restarts only when the
  sample rate or block size changes. Vertically the trace hangs from the top of the graph
  and reaches the bottom at **24 dB** of reduction — the same span the COMP and LIMITER
  panels' own GR meters use, so the two read against each other directly.
- **The limiter's reduction as a number** (since 0.2.14) — under **out LUFS** in Simple, and in
  the foot of the LIMITER panel in Advanced: **lim GR** is the limiter's gain reduction now, in
  dB (the deepest over the last 0.3 s), and **GR max** the deepest the GR history graph shows —
  every point of it, the oldest at its left edge included (since 0.2.15; it used to stop up to
  ~0.2 s short of the graph's left edge).
  It is the same figure the GR history draws — the **limiter's** reduction only; the
  compressor's shows on its own COMP meter, and the clipper's is not measured. It works in
  either graph view. **lim GR** reads "-" when nothing has been processed yet or when your
  host has stopped sending audio for more than half a second (the max stays, with the history
  it describes); "0.0" means the limiter is doing nothing. While **BYPASS** is on it keeps
  showing the limiter's work on the processed signal, which you are not hearing.

**RESET**, on the STATISTICS header line, clears the session figures: the integrated
measurement (**I**), the loudness range (**LRA**), both peak holds (**TP**, **SP**) — and **PLR**
with them — plus the session length beside the header. Do it after changing the section you
are judging. Nothing else resets them except loading a project and the host re-preparing the
plug-in (a sample-rate or buffer-size change); an A/B switch, a preset and BYPASS do not.
Clicking anywhere else on the panel does nothing (since 0.2.14 — until then the whole panel was
the reset, so a stray click, right-click or drag discarded the session). The rolling windows (M,
S, RMS) are not reset — they measure the last few seconds and have nothing session-scoped in
them — but every reading blanks for an instant at a reset and returns with the next audio; with
the transport stopped they stay blank until audio flows. The new session starts at the moment
of the reset: nothing played before it reaches the new figures — not even a peak that was still
on its way through the meter when you pressed RESET (since 0.2.16; before, a reset straight after
a loud passage could bring its last peak back into the new TP hold, and the first moments of
silence after it could show a loudness carried over from before). Up to the first 0.2 s after
a reset is left out of **I**, for the same reason.

**What the session covers.** The time beside the header (`m:ss`, whole seconds) is how much
programme the session figures describe. It stops while you listen to **BYPASS** during
playback: an audition of your input is not part of the master you are measuring, so it is left
out of I, LRA and the peak holds (since 0.2.14 — before, a bypass comparison could raise the
peak holds above your Ceiling and pull I toward the input). The time and the peak holds pick up
again with the first moment after the audition, keeping what they held before it; **I** and
**LRA** leave a little more out. As
after a reset, up to the first 0.2 s after the audition is not in **I**, and nor is up to 0.1 s
before it — so less than half a second between two auditions adds to the time but nothing to
**I** — and **LRA** moves again about 3 s after it. The rolling readings still follow what you
hear, bypass included. In an **offline bounce** the bypassed sections are part of the file, so
they are measured. If your host stops sending audio to the plug-in (some hosts do when
a track is disabled, frozen or bypassed by the host itself), nothing is measured, the time stops,
and the rolling readings hold their last values.

### 3.5 Settings (gear)

Session state — saved with your DAW project, never in presets, invisible to automation:

| Setting | Options | Notes |
|---|---|---|
| **Oversampling** | Off / 2× / 4× / 8× / 16× | For the nonlinear stages and true-peak accuracy. Higher = cleaner at higher CPU cost; adds host-compensated latency (§6). **At Off, any Clip Drive above 0 dB also softens the top octave** — the clipper's antialiasing costs about −2 dB at 10 kHz and −12 dB at 20 kHz at 48 kHz (more at 44.1 kHz), whatever the drive amount, and the Loudness knob engages the clipper above 30 %. 4× cuts that to under 0.6 dB at 20 kHz ([`KNOWN_ISSUES.md`](../KNOWN_ISSUES.md) KI-005). |
| **Phase** | Minimum / Linear | Minimum phase = lowest latency; linear phase = symmetric ringing, more latency. |
| **Offline Render** | Follow Online / Force Max | Force Max renders your bounce at maximum oversampling regardless of the live setting; Follow Online uses whatever the live setting is. |
| **UI Scale** | XS / S / M / L / XL | Five steps; **M** is the original size, everything scales in proportion. |
| **UI Animations** | on/off | Default on. Off never changes behaviour, only motion. |
| **Tooltips** | on/off | Hover hints on every control — what it does, in a line. Default off. |
| **Integrated** | BS.1770-2+ / BS.1770-1 | Which revision the **I** row follows. **-2 onward** (default) gates quiet passages out of the average, which is what every modern delivery spec means by "integrated LUFS". **-1** is the original ungated definition — the plain average, dragged down by silence. |
| **RMS Reference** | AES-17 / Mathematical | What the **RMS** row calls 0 dB. **AES-17** (default) reads a full-scale sine as 0 dBFS, the mastering convention. **Mathematical** reads the same sine as −3.01, the literal root-mean-square. The two differ by exactly 3.01 dB and never by anything else. |

(The Spectrum/GR choice is *not* here: switch it with the chip on the graph itself — §3.4.)

The **Ceiling LOCK** lives next to the Ceiling knob itself, not here — but like these
settings it is session state, so browsing presets never moves a locked ceiling.

---

## 4. The adaptive engine, Learn and Freeze

The **Loudness** knob drives a macro mapping into the real stage parameters, and around
that mapping the **adaptive engine** applies small, bounded trims based on what it hears —
release times open up on sparse material and tighten on dense material, stereo linking,
the compressor's detector high-pass and the dynamic tame all follow the programme. Adaptation is
deliberately **slow** — second-scale, with hysteresis — so it never sounds like
modulation; it is the difference between a setting that is right for the chorus and one
that is right for the whole song.

- **FREEZE** locks the current adaptive state exactly — also when the host re-prepares the
  plug-in (a sample-rate or buffer-size change, or in some hosts pressing play or starting a
  bounce). Use it when the engine has settled
  on the sound you want and you don't want a quiet bridge to re-open it. A frozen state is
  **saved and restored with your session** — reopening the project brings back exactly the
  trims you froze, not a re-adapted approximation. (One A/B case does not agree with the save
  yet: switching into a slot that has FREEZE on but no frozen sound of its own keeps playing the
  sound of the slot you left — [`KNOWN_ISSUES.md`](../KNOWN_ISSUES.md) KI-007, item 10.)
- **LEARN** is an explicit calibration pass: press it, play a *representative* section
  (at least 5 seconds — the button counts), press it again. The engine fixes its internal
  reference targets to that material, so subsequent adaptation is judged against *your*
  track rather than a generic assumption. The learned reference is saved with the session.

Neither is required — the engine adapts sensibly without them. Freeze is for
repeatability; Learn is for material the generic references misjudge (very dark mixes,
very sparse arrangements, spoken word).

---

## 5. Simple mode and Advanced mode

Simple is a **macro layer over the Advanced parameters** — one parameter model, two views,
and **switching views never changes the sound**.

When you edit a stage parameter in Advanced that the macros manage, that parameter
**detaches** from the macro (it shows a corner-dot badge) and keeps your value — returning
to Simple moves nothing. The next time you move a macro knob, detached parameters
**re-engage** and glide back under macro control; that gesture is the "the macros are in
charge again" moment, and it is smooth, not a jump. The **edited dot** in Simple view is
the summary indicator — click it to re-land everything on the macro sound at once
(undoable).

Two things the corner dot is **not**: it is not an "edited since the preset" mark (that is
the `*` after the preset name), and it never appears on knobs the macros don't manage —
only the nine managed parameters can detach, because only they have a macro curve to
detach *from*. Consequently, manually turning a detached knob back to its old value does
**not** clear the dot: the knob is still off macro control, holding *your* value. Moving a
macro, clicking Simple's edited dot, or loading a preset is what re-attaches it. (The knob
hover hints carried this legend until 0.1.3, when it was removed by owner directive — the
explanation lives here, in the manual, and the dot itself is unchanged.)

---

## 6. Signal flow and latency

```
Input Gain → EQ (Pre position) → Compressor → Clipper + Color
           → Limiter (lookahead, true peak) → EQ (Post position) → Ceiling
           → Dither → Output
```

- The **ceiling clamp is always last before dither** — whatever you do upstream
  (including a boosted Post EQ), the output does not exceed the ceiling: its sample peaks with
  **TP** off, and at any sample rate below 12 kHz; its true peaks with **TP** on at 12 kHz and up,
  to within 0.1 dB on the plug-in's own dBTP meter and on the BS.1770 reference filter — the two
  meters the promise is defined on. True-peak meters
  differ in the top few percent below Nyquist, so another meter can read a little more on very
  bright, heavily clipped material: up to ~0.2 dB on libebur128-based meters and up to ~1 dB on a
  long-kernel reference in the tests. For a delivery checked on such a meter, set the ceiling about
  1 dB below the spec for that kind of material, or use oversampling
  ([`KNOWN_ISSUES.md`](../KNOWN_ISSUES.md) KI-020).
- **Latency is constant by design**: Anabasis reports a fixed lookahead allowance
  (10 ms) plus the current oversampling filter latency. Moving the Lookahead knob,
  browsing presets, switching A/B — none of it changes reported latency, so your DAW's
  delay compensation never re-syncs mid-session. Oversampling factor and phase mode *do*
  change it, and take effect at a click-free moment.
- **Click-free by construction**: preset loads, A/B switches, undo/redo and engine
  rewires (EQ position, color model, oversampling changes, the **TP** switch) duck the output
  briefly instead of clicking. The short dip *is* the mechanism working. (Below 12 kHz the **TP**
  switch changes only what the limiter listens to, so it does not dip.) Switching **TP on** while
  audio plays at 12 kHz or more is the one that starts differently: the programme stops at once and its last value
  fades away over a few milliseconds, so the true-peak ceiling already holds from the moment you
  press it (a fade of the audio itself would still carry inter-sample peaks); the sound then comes
  back in TP mode as usual.
- **Self-healing**: if a hostile upstream signal ever overflows a filter, the engine
  detects and repairs it within the block instead of going silent.

---

## 7. Presets and A/B

### 7.1 Loading

Click the preset name for the menu — **FACTORY** and **USER** sections — or step with the
**‹ ›** arrows (wrap-around). "Load Preset…" opens a file chooser for `.anabasis` files
anywhere on disk. Loads are click-free and form **one undo step**.

Thirteen factory presets ship built in: *Default* (the plug-in's opening state — re-apply
it to get back to a clean slate), then *Transparent Master, Loud Pop, EDM Club, Vocal
Forward, Tape Glue, Rock Punch, Hip-Hop Low End, Acoustic Warmth, Classical Dynamics,
Podcast Voice, Cinematic Wide, Lo-Fi Crush*.

### 7.2 Saving and managing

"Save Preset…" opens a name dialog. Saving over an existing name overwrites it, without a
confirmation — including a name that only becomes an existing one once characters a file name
cannot hold are removed ([`KNOWN_ISSUES.md`](../KNOWN_ISSUES.md) KI-022). User
presets are plain XML files with the `.anabasis` extension, stored per user:

| OS | Folder |
|---|---|
| Windows | `%APPDATA%\RollyTech\Anabasis\Presets` |
| macOS | `~/Library/RollyTech/Anabasis/Presets` |
| Linux | `~/.config/RollyTech/Anabasis/Presets` |

There is no in-plugin rename/delete — manage the files in that folder (the menu picks up
changes, sorted alphabetically).

### 7.3 What a preset contains

A preset changes **sound parameters only**. Deliberately left alone: Bypass, the
monitoring toggles (**MATCH** — your host's automation lane still calls this parameter
"Loudness Comp" — and **DELTA**), the Simple/Advanced view state, **Freeze**, and
everything in Settings. A **locked Ceiling** is skipped entirely — browsing presets never
moves it. The `*` edited marker compares exactly what a preset can carry, so toggling
monitoring or resizing the window never marks a preset as edited.

**TP, Dither and Noise SHAPE are sound parameters here, so a preset sets them.** Every factory
preset turns all three **off** (none of them states an intent for them), and a user preset
restores what it was saved with. LOCK holds the ceiling's *value*, not TP — so after browsing,
re-engage TP (and Dither) if your delivery needs them, or Undo back to the state you locked
([`KNOWN_ISSUES.md`](../KNOWN_ISSUES.md) KI-021).

Presets are **forward-compatible**: parameter identities are frozen and regression-tested
in CI, so `.anabasis` files and DAW sessions from older versions keep loading in newer
ones. (A *session* restores anything an old file doesn't mention to its default; a
*preset* applies exactly the values it lists.)

### 7.4 A/B compare

**A/B** switches between two complete, independent sound setups; **Copy** pushes the
current one into the other slot. Each slot keeps its own preset name, edited state and
undo history — and the Copy itself is an **undo step on the destination**: switch to the
copied-into slot and press Undo to revert the Copy, then keep undoing through that slot's
own earlier history. Switching is click-free and is not itself an undo step. Both slots
travel with your DAW session.

---

## 8. Workflow examples

### A transparent master

1. Start from *Transparent Master* (or defaults). Ceiling to your delivery spec (the −0.1
   default suits most deliveries; lock it). If the spec is written in dBTP, engage **TP**
   as well — that is what makes the ceiling hold inter-sample peaks, at sample rates of 12 kHz
   or more — and engage it *after*
   choosing the preset, because a preset turns it off (§7.3). On bright material, once
   Loudness passes 30 %, set Oversampling to 4× so the clipper does not soften the top octave
   (§3.5).
2. Play the loudest chorus; raise **Loudness** until the GR history shows steady work
   (the graph well opens on the GR history — if it is showing the spectrum, the GR|SPEC
   pill in its bottom-left corner switches back).
3. **MATCH on.** Judge at matched loudness. Use **DELTA** to check what is being lost —
   dry transient ticks only.
4. Compare candidates with **A/B** + **Copy**, judge PLR, and undo freely — history
   is per slot.

### Competitive loudness (pop/EDM)

1. Start from *Loud Pop* or *EDM Club*. Push **Loudness** well up the dial — the clipper
   absorbing transients before the limiter is what keeps it clean at this range.
2. Add **Character** for density and glue; **Tone** to taste against harshness.
3. In Advanced, try Limiter **Style → Loud**, and raise **Oversampling** (4× and up) —
   at heavy clipping it audibly cleans the top end. **Offline quality → Force Max**
   renders the bounce at 16× regardless.
4. Watch **DELTA** for pumping or vocal damage.

### Spoken word / podcast

1. Start from *Podcast Voice*. Modest **Loudness**; the compressor does most of the work.
2. **LEARN** on a representative minute of the actual voice, then let it settle and
   **FREEZE** — one consistent sound for the whole episode, saved with the session.
3. Aim for your distributor's integrated-loudness spec rather than maximum loudness;
   press **RESET** on the STATISTICS panel, play the episode through, and read **I**.

---

## 9. FAQ & troubleshooting

### Installing and loading

**The plug-in doesn't appear in my DAW.**
Work through these in order:

1. **Rescan** — see the table in [§2.2](#22-first-launch). This alone fixes most cases.
2. **Right format?** Logic Pro and GarageBand load **AU only**; everything else uses the
   **VST3**. Anabasis is 64-bit only.
3. **Right place / permissions?** Check the install paths — and on Linux/macOS the
   `chmod` steps — in the [Installation guide](INSTALLATION.md). If you copied by hand,
   make sure you moved the *whole* `Anabasis.vst3` **folder**.
4. **Right kind of track?** Stereo→stereo, mono→stereo and mono→mono are all offered
   (since 0.1.2); the one shape refused is a stereo source into a mono output — there is
   no downmix rule, so such a slot will not offer the plugin.
5. **Blocklisted from an earlier failed scan?** Clear the host's plug-in cache/blocklist
   entry and scan again — common after a macOS quarantine problem: the first scan fails,
   the host remembers, and never retries on its own.

**macOS says it "cannot be opened", or it won't load after copying.**
Gatekeeper — the binaries are not notarized yet. Run the `xattr` de-quarantine commands in
the [Installation guide](INSTALLATION.md), restore the executable bits, and rescan.

### Latency and sound

**My DAW shows ~10 ms (or more) of plug-in delay. Is something wrong?**
No — that is the design. Anabasis is a lookahead limiter, so it always reports a fixed
10 ms lookahead allowance, plus the oversampling filters when oversampling is on. What you
get in exchange: the reported figure is **constant** — browsing presets, moving Lookahead,
switching A/B never changes it, so delay compensation never re-syncs mid-session. Your DAW
compensates automatically; on a master bus this is inaudible by definition.

**Clicks or a brief dip when switching presets, A/B or undoing?**
The short dip is deliberate — parameter jumps and engine rewires execute in a masked
moment instead of clicking.

**It gets louder but not better.**
That is exactly what **MATCH** exists to reveal. Judge with it on; use **DELTA** to hear
the cost; back off Loudness or shift work to Character. If the top end hardens, raise
**Dynamic Tame** (Advanced) or darken **Tone** slightly.

**How much CPU, and how do I reduce it?**
**Oversampling** is by far the largest cost, and 16× is deliberately expensive — use
**Offline quality → Force Max** to get maximum quality on the bounce while running lighter
live. Closing the editor window removes the GUI's share.

### Automation and sessions

**Can I automate the controls?**
Every stage parameter (compressor, clipper/color, limiter, EQ, gains) is a host parameter
and can be automated as usual. A few are host parameters that are deliberately **not
offered as automation targets**: the three macro knobs (Loudness, Character, Tone —
automating a macro that itself writes other parameters would fight the host), the
Simple/Advanced view toggle (**ADV**), Freeze, Lookahead, True Peak mode and the two dither
controls. Hosts differ in whether they hide those; writing them still behaves sanely.
The **Settings** overlay's items are a different thing again — they are not host parameters
at all, so they never reach an automation list; they are session state saved with your
project.

**Will my old sessions and presets still work after I update?**
Yes — parameter identities are frozen and regression-tested in CI ([§7.3](#73-what-a-preset-contains)).

### Presets

**Where are my presets stored? How do I share one?**
Per-user folder, exact paths in [§7.2](#72-saving-and-managing). The `.anabasis` files are
portable across platforms — copy them out, or load any file directly with *Load Preset…*.

**Browsing presets changed my ceiling.**
Engage the **LOCK** next to the Ceiling knob — a locked ceiling is skipped by every preset
apply. LOCK holds the number only: a factory preset still switches **TP**, Dither and Noise
SHAPE off, so check them before a delivery render (§7.3).

**Anything else?**
[`docs/KNOWN_ISSUES.md`](../KNOWN_ISSUES.md) lists every confirmed limitation with its
current status.

---

*Anabasis is © 2026 RollyTech. All rights reserved.*
