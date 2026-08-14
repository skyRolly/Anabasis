# Changelog

All notable changes to **Anabasis** are documented here.

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versioning:
`MAJOR.MINOR.PATCH`, pre-1.0 line (< 1.0.0 = pre-release). Maintained per
[`docs/policies/CHANGELOG_POLICY.md`](docs/policies/CHANGELOG_POLICY.md):

- **User-visible changes only.** Refactors, cleanups, formatting and renames are not entries
  unless a commit/PR explicitly states a user-visible impact.
- **No invented history.** Every entry cites an Evidence Source (commit SHA, commit range, PR or
  release tag). An entry that cannot be tied to such evidence is marked
  `[Unverified Historical Reconstruction]`.
- **Renames are `Changed`, not `Removed`** (a display-name change with an unchanged parameter ID).
- Compatibility-affecting entries cross-link the relevant ADR and note any migration.

**No tag has been cut yet, so nothing has left this repository.** A version entry here means its
notes are written, dated and complete — not that the build shipped. Four such entries now exist
(`[0.1.1]`, `[0.1.2]`, `[0.1.3]`, `[0.1.4]`) and none has been tagged; WHICH version the first annotated
`vX.Y.Z` tag cuts is a decision nobody has taken yet, and this file does not presume it.
`release.yml` is what turns a tag into a DRAFT release, and
publishing that draft stays a human action (ADR-0021). The fact lives HERE rather than inside a
version entry because the workflow extracts a version's section VERBATIM as the published release
notes, so a sentence about the tag not existing would ship inside the notes that prove it does.
The compatibility window this keeps open is `COMPATIBILITY_POLICY.md` §"When the contract starts",
which is the copy of record for whether anything has shipped.

**Entry template.** It lives HERE, above the entries, and not at the foot of the file: the
release workflow extracts a version's notes as everything from a version heading to the next
h2 heading — and the oldest entry has none after it, so a template sitting after the entries
was published inside the release notes. **The h2 level is reserved for version entries**: any
other `## ` heading ends a release's notes where it stands, so a section appended to the foot
of this file truncates the oldest entry's notes and a sub-section demoted from `### ` to `## `
truncates its own. Sub-sections stay at `### ` or deeper, which is why the P1–P6 history below
runs in `###` sections. `scripts/check-docs.py` fails the build on both. A fenced block is
read as data, so the sample heading immediately below is not mistaken for structure.

```
## [x.y.z] — YYYY-MM-DD

### Added
- <user-visible change>.
  Evidence: commit 6a24b82 (or PR #NN). [Verified | Partially Verified | Unverified Historical Reconstruction]
```

---

## [0.1.4] — 2026-08-13

**The installer and interaction round.** Everything here is packaging, pop-up/menu interaction or
state bookkeeping: **no audible DSP change**, no parameter added, renamed or removed, and no change
to the serialization schema. Two entries change stored-state *behaviour* within the existing schema
and say so.

### Added
- **The Linux installer offers a per-user install, and it is now the default.** `install.sh` asks
  where to install: the current user (`~/.vst3` and `~/.local/bin`, no root at all — `~/.vst3` is
  the standard per-user VST3 folder that most DAWs scan) or system-wide (`/usr/lib/vst3` and
  `/usr/local/bin`). Running it as root still installs system-wide without asking, and a
  system-wide install elevates only the individual copy/move steps rather than the whole script,
  so a machine without `sudo` gets a clear instruction instead of a failure. A per-user install
  that finds an older system-wide copy still present now says so, names both files and gives the
  command to remove them — otherwise the DAW shows Anabasis twice and may load the older one.
  `uninstall.sh` mirrors the same two modes. Evidence: this release. [Verified]
- **`install.sh --user` / `install.sh --system` answer that question up front.** The prompt is
  gated on stdin being a terminal, so a provisioning script, a CI step or a piped run silently
  took the per-user default and had no way to ask for anything else — the only non-interactive
  route to a system-wide install was to run the whole script under `sudo`, which is a different
  install: it makes the entire transaction root's, where the flag keeps the elevation
  per-operation. `--help` prints the two options; an unrecognised option is refused rather than
  ignored; and `--user` under `sudo` is refused rather than guessed at, because which home
  directory `$HOME` names there depends on the machine's sudoers configuration. Evidence: this
  release. [Verified]
- **The macOS package verifies itself at build time.** The build now fails rather than shipping a
  package whose components are relocatable, version-checked, missing the overwrite action or
  missing their installed-state check — and it first proves those assertions can actually fire, by
  packaging one payload twice and confirming each state appears with the installer defaults and
  disappears when its key is switched off. A count check precedes the per-component loop so an
  empty search cannot pass every assertion by running none of them. Evidence: this release. [Verified]

### Fixed
- **Re-installing on macOS after moving or deleting the app could leave the destination empty.**
  Components were built relocatable and version-checked, so the installer looked the bundle up in
  the system's receipt database and, finding a copy anywhere — including one dragged to the Desktop
  or sitting in the Trash — wrote the payload over *that* copy and reported success while
  `/Applications` stayed empty. Version checking failed the same way from the other side, skipping
  a destination already at or above the package version. Both are now off: every component writes
  its payload to its declared location from the payload alone. Each component also carries a
  fail-closed check, so an install that did not land reports failure instead of success.
  Recorded as **INC-005**. Evidence: this release. [Verified]
- **An interrupted Linux install could leave no working plug-in behind.** The installer deleted the
  installed VST3 and only then copied its replacement, so an interruption between the two left
  nothing. The replacement is now a transaction: the new version is staged, the previous one is
  moved aside rather than deleted, and the old copy is discarded only once the new one is in place.
  Interrupting the run — Ctrl-C, a closed terminal, a logout — leaves a working plug-in, and the
  next run reconciles whatever was left behind. Two limits, stated rather than implied: a signal
  no handler can catch (a kill, a power loss) landing in the two-rename window between moving the
  old bundle aside and moving the new one in leaves the plug-in absent until the installer is run
  again; and the VST3 and the Standalone are replaced one after the other, so an interruption
  between them can leave the new plug-in beside the old Standalone. Staging happens outside the
  folder your DAW scans wherever the filesystem allows it, so a rescan mid-install cannot see a
  half-written bundle. Evidence: this release. [Verified]
- **An interrupted system-wide install now really does put the previous version back.** The
  transaction parks the old plug-in in a staging folder only the administrator can open, and the
  automatic rollback then checked for it *without* administrator rights — so on the normal
  system-wide path (running the installer and choosing "system-wide" rather than launching it with
  `sudo`) the check said "nothing parked" even with the old plug-in sitting right there, and
  interrupting the upgrade at the wrong moment left **no plug-in installed at all** while the only
  good copy stayed unreachable. Reproduced end to end before the fix, on the two-rename window the
  transaction exists to protect. Every check inside that folder is now made with the same rights as
  the writes. The same blindness also stopped a later run from finding the parked copy and from
  printing the warning that says it is there. Evidence: this release. [Verified]
- **A Linux install that refuses to start now names every folder that could be blocking it.** The
  installer declines to stage into a folder it cannot trust — a symlink, one owned by another
  account, one others can write to — and the per-user message named only the first of the two
  places it looks. On a machine where `~` and `~/.vst3` sit on different filesystems the blocker is
  the second one, so the printed instruction removed a folder that was not in the way (and often did
  not exist), and every re-run failed identically. Both candidates are named now, by the message and
  the check reading the same list. Separately, if a previous version was parked in a folder that has
  since become untrustworthy, the run says so instead of passing over it in silence: that copy is
  neither restored nor deleted, and `./uninstall.sh` is what clears it. Evidence: this release.
  [Verified]
- **The uninstaller no longer says "nothing to remove" in the same breath as removing something.**
  Clearing an installer leftover did not count as work done, so a run that found no plug-in but
  did clear staged files printed both lines — and the second is the one you read to decide
  whether anything happened. Evidence: this release. [Verified]
- **The uninstaller now removes the installer's own leftovers**, by exact name only, so an install
  killed by a signal no handler catches does not survive a deliberate uninstall. Evidence: this
  release. [Verified]
- **A click that dismissed a menu could also operate the control underneath it.** The framework
  re-delivers that click by design, and several controls act on the press itself — the A/B switch,
  the graph-well toggles, a knob's Alt-click reset, and the panel backdrops, where the Save Preset
  panel closing threw away the name being typed. One transparent shield now covers the editor
  whenever any pop-up is on screen and absorbs the dismissing click, including scroll and pinch.
  The residual limitation is registered as **KI-013**: the absorbed click still counts toward the
  system's multi-click run. Evidence: this release. [Verified]
- **A pop-up menu's border could be drawn from unsynchronised state.** ([**ADR-0027**](docs/architecture/design-decisions/ADR-0027-painting-thread-reads-editor-bookkeeping.md)
  — Accepted 2026-08-14; asking the editor a question from the drawing thread is a new cross-thread
  path, which is an Architecture Review Gate item. Review found the path unflagged and the owner
  then cleared the gate. What follows is the defect; the gate was about the path existing at all.) The look-and-feel asks the
  editor whether one of its own menus is open, and that question is answered during PAINTING —
  which, where hardware-accelerated drawing is enabled (macOS and Windows), happens on a different
  thread from the one that opens and closes menus. The counter behind the answer is now atomic, and
  the editor tears the hook down after the drawing thread has stopped rather than before. No
  behaviour change; it removes undefined behaviour that a sanitizer would report.
  Evidence: this release. [Verified]
- **Menus and drop-downs no longer outlive the editor.** Closing the editor with a drop-down open
  left the drop-down on screen over the host; so did switching to another application while the
  pointer rested on the menu. Both are now cancelled. The application-switch check calibrates
  itself when a pop-up opens, so a plug-in hosted in a process that is never the foreground
  application keeps working menus instead of losing every one of them the moment it opens.
  Evidence: this release. [Verified]
- **Leaving the application no longer commits a half-typed value.** A value box being edited is
  abandoned rather than applied when the editor releases keyboard focus on the user's behalf, and
  the focus release itself no longer drags the host window back in front of the application the
  user just switched to. Evidence: this release. [Verified]
- **Turning tooltips off now switches them off.** The setting only lengthened the appear delay,
  which a tooltip already on screen ignores — so a visible tip stayed, and a moving pointer chained
  new ones past the setting indefinitely. Tooltips are now refused at the source. Evidence: this
  release. [Verified]
- **Long menu entries are no longer clipped**, and **disabled menu entries now look disabled.** The
  width the menu asked for was smaller than the width its own row drawing spends, so the longest
  item was measured narrower than it draws; both now read the same constants. A row the menu
  reports as unavailable is dimmed instead of drawing identically to a selectable one.
  Evidence: this release. [Verified]
- **The preset menu no longer splits into two columns** once enough user presets are saved, and no
  longer draws a doubled border. Evidence: this release. [Verified]
- **Re-selecting the preset that is already loaded, with nothing edited, no longer discards your
  redo history** and no longer leaves an undo step that does nothing when pressed. Re-applying a
  preset over an *edited* sound is still a real restore and remains undoable. The apply itself is
  unchanged and is **not** inert: it still re-writes the parameter surface and still takes the
  §2.8 forced duck, so the brief dip is the same as any preset change — what changed is the undo
  bookkeeping afterwards, not the audio. **This works in a project opened from your DAW, which is
  where it matters and where the first cut did not reach**: the check that decides "nothing changed"
  compared a dirty-marker value that a session load deliberately leaves blank, so in any restored
  project it always read "something changed" and did neither half of what this entry promises. It
  now compares what that marker MEANS — was the preset showing as edited before, and is it now —
  rather than the value itself. This changes stored behaviour, not the stored format.
  Evidence: this release. [Verified]
- **A damaged A/B slot can no longer put one session's preset name on another session's sound.**
  ([**ADR-0026**](docs/architecture/design-decisions/ADR-0026-slot-payload-read-rules.md) —
  Accepted 2026-08-14; this is a change to how a stored session is INTERPRETED, so it is an
  Architecture Review Gate item, and the owner cleared the gate after review found it unflagged.)
  A stored slot that survives with its labels but without its parameter payload — hand-edited or
  truncated session data — now resolves to defaults as a whole, the same rule the live surface
  already followed, instead of lending its name and preset identity to whatever was loaded.
  This changes stored behaviour, not the stored format. Evidence: this release. [Verified]

### Changed
- **The Settings tooltip for UI Scale** now names UI scale rather than window size, matching the
  label and the accessibility title beside it; the control scales the whole interface, and on a
  host applying its own DPI scale the window is not the quantity it describes. Display text only —
  no stored setting changed. Evidence: this release. [Verified]

### Known issues
- **KI-013** — the click absorbed by the pop-up shield still counts toward the system's multi-click
  run, so a dismissal immediately followed by a click on the same knob can register as a
  double-click. See `docs/KNOWN_ISSUES.md`.
- **KI-014** — on macOS, holding a letter or digit in the Save Preset name field types one
  character and stops, while punctuation repeats normally. Traced to platform key-repeat handling
  outside this plug-in's control; the fixes considered were rejected as worse than the symptom.
  See `docs/KNOWN_ISSUES.md`.

## [0.1.3] — 2026-08-09

**The polish round** (the owner's seven-item 0.1.3 directive), plus two review follow-ups.
Mostly display and naming, and **no audible DSP change**: the one audio-path edit is a
numerical guard in the colour stage (below), argued and tested bit-exact for every signal the
chain can reach. Stated this way deliberately — an earlier draft of this line said "no DSP
behaviour changes anywhere", which the entry's own `Fixed` section contradicts, and
`release.yml` publishes this section verbatim as the release notes.

### Added
- **The AU is validated by pluginval, on every push.** Same strictness, both modes, three
  consecutive passes each — the same gate the VST3 has always had. Until now the release gate ran
  against the VST3 alone on all three platforms, so the AU shipped to Logic users having passed
  no automated validation at all. Evidence Source: PR #14. [Verified]
- **A channel probe that tests the shipped artefact.** Every other check rebuilds the plugin's
  sources into a console app; this one loads the built VST3/AU through a host and asserts both
  channels carry audio across the reported field configurations — so the LTO'd, wrapped binary a
  user actually installs is exercised, on macOS for both formats and both architectures. It is
  what finally reproduced the silent left channel. Opt-in (`-DANABASIS_BUILD_PROBE=ON`) and run
  on all three platforms in CI, alongside a second reproduction that drives the DSP core with no
  wrapper, format or host at all — so a failure says immediately which half of the product is at
  fault. The Clang job now builds the **plugin** with the product's own link-time-optimisation
  flags, the configuration in which the silent-channel defect was the only thing ever built.
  Evidence Source: PR #14. [Verified]
- **Three cross-platform CI gates**, because a green Linux build is not evidence about another
  platform: a source portability lint for the JUCE SIMD-overload hazard (with a compile canary
  that checks the pinned JUCE still has that hazard), a Clang build that fails on any warning in
  first-party sources, and a sanitizer job running ASan + UBSan over both suites plus valgrind
  memcheck over both suites too. `docs/policies/TESTING_POLICY.md` carries what each one does and,
  as importantly, what it does not. Evidence Source: PR #14. [Verified]

### Changed
- **The loudness-compensation toggle reads MATCH, not COMP** — beside a compressor panel
  captioned COMP, the old caption read as a compressor switch. The parameter and its
  automation name ("Loudness Comp") are unchanged. Evidence Source: PR #13 (item 2). [Verified]
- **"Colour" is now "Color" in every user-facing string** — the CLIP / COLOR panel caption,
  the knob tooltips, and the automation names of the three colour-stage parameters ("Color",
  "Color Tone", "Color Depth"; registry snapshot re-frozen per
  PARAMETER_COMPATIBILITY_POLICY rule 2, exactly as 0.1.2's "Limiter Stereo Link"). Parameter
  IDs keep their historical spelling — they are compatibility keys, never shown. Evidence
  Source: PR #13 (item 6). [Verified]
- **The EQ panel is reorganised into one band per row** — bands in ascending frequency order
  (Tilt + low shelf · Bell 1 · Bell 2 · high shelf), each bell reading Q | Freq | Gain across
  the same column grid, and the two shelves framing them in matching columns. The previous
  layout packed the eleven knobs in declaration order, splitting every band across rows. Same
  cell sizes and panel budget, so nothing else moved; parameter identity, ranges and
  automation are untouched. Evidence Source: PR #13, bell column order PR #14 (item 5).
  [Verified]
- **The Sidechain High-Pass tooltip now says what the control does** — it filters the
  **compressor's** detector only; the limiter's detector has been deliberately unfiltered
  since 0.1.2 (ADR-0023), and the tip still described the pre-0.1.2 shared filter. Evidence
  Source: PR #14. [Verified]
- **The Ceiling reads and holds two decimal places** — `-0.10 dB`, not `-0.1 dB`, and the value
  behind the label is quantised to the same 0.01 dB grid rather than merely displayed on it. It is
  the one control set to a number from a delivery spec, so a knob reading `-0.1` while holding
  −0.14 misstated the guarantee at the only place that guarantee is written down. The quantisation
  is in the parameter's range, so it holds for the knob, host automation, typed text, preset and
  session recall and the value the limiter actually compares against — not for the display alone.
  The −0.1 default is unchanged and already on the grid; a stored off-grid value snaps on load
  (ADR-0024, `PARAMETER_COMPATIBILITY_POLICY.md` rule 3 — nothing has shipped, so nothing needs
  migrating). Evidence Source: PR #14. [Verified]
- **The macOS and Windows installers capitalise "Plug-in" and "Application"** in their
  component and folder-prompt wording. Evidence Source: PR #14. [Verified]
- **The RMS statistics row updates at reading pace (~3 Hz)** — the 50 ms windowed measurement
  (ADR-0020) is unchanged and was never wrong; the readout was re-printed at the panel's full
  frame rate (the meter clock paces on vblank, up to ~125 Hz), so the digits churned faster
  than they could be read. The row now adopts a
  fresh value roughly three times a second and holds it between adoptions; a meter reset
  clears it immediately when no audio is running (with audio flowing the same block re-publishes
  a live 50 ms reading, so there is nothing to clear — the rolling windows are not reset, as the
  manual says), and **switching the RMS reference in Settings still moves the
  row on the next frame** — the hold applies to the measurement, not to the reference.
  Evidence Source: PR #13, reference immediacy PR #14 (item 1). [Verified]

### Fixed
- **The left channel is no longer silent.** On macOS — in both AU and VST3, at every sample rate
  and block size, and at the plugin's own defaults as much as at any particular setting — the
  plugin emitted exact digital silence on the left channel while the right played normally.
  Setting Clip Mix to 0, or engaging Bypass, restored it. The cause was in the engine's channel
  bound: it was correct at runtime but not provably in range to the compiler, which made the
  per-sample stereo frames look like possible out-of-bounds writes, and one compiler
  configuration (Clang with link-time optimisation — the one that builds the macOS product)
  acted on that. The bound now states the limit it always had. The corrected build produces
  bit-for-bit the same output as the builds that were never affected, so nothing that sounded
  right has changed. `docs/POSTMORTEMS.md` INC-004 carries the mechanism and why five months of
  green test runs could not see it. Evidence Source: PR #14. [Verified]
- **The clip/saturation stage can no longer poison itself invisibly.** Its dynamic-tame filter
  ran on the unbounded through-signal, so an absurdly hot input (above ~2.2e38, about
  +767 dBFS) could turn the filter's own state into a NaN — **without the plugin producing a
  single bad sample at the time**, because with Clip Drive at 0 the tame is idle and its state
  is written but never read. The corrupted state then survived every block, preset load and
  A/B, and was paid for later: the moment Clip Drive was raised with a non-zero Clip Mix, that
  one channel went digitally silent. The stage's arithmetic bound now covers this third site as
  well as the two it already covered, on the state feed rather than the signal, so a huge
  sample still passes through untouched and no audible sample changes. Not the cause of the
  field reports of a silent left channel — the trigger level is far beyond anything a DAW
  delivers, and that cause is the separate entry above — but a real latch either way.
  Evidence Source: PR #14.
  [Verified]
- **The macOS build works again.** The macOS CI job had failed to COMPILE
  `tests/state_tests.cpp` on every push since 2026-08-08 while Linux and Windows stayed green,
  so no macOS binary was built, validated or packaged for three days. One line did it:
  `juce::jmax<size_t> (...)` instantiates JUCE's `dsp::SIMDRegister` overload of the same name,
  and completing `SIMDRegister<size_t>` needs a `SIMDNativeOps<size_t>` that exists only where
  `size_t` names one of JUCE's ten SIMD element typedefs — true on Linux (`uint64_t` IS
  `unsigned long`), false on macOS (`uint64_t` is `unsigned long long`). Recorded in full as
  `docs/POSTMORTEMS.md` INC-003. Evidence Source: PR #14. [Verified]
- **A sustained, absurdly hot input can no longer silence one channel** (the KI-009
  investigation). The colour stage's harmonic polynomial raises to the fifth power, and with
  Clip Drive at 0 the clipper's own bound is skipped, so a finite-but-astronomical sample
  could overflow it to infinity — after which the engine's per-channel safety substitution
  emitted digital zero on **that channel alone**, for as long as the input persisted, while
  the other channel played normally. The stage's **three** arithmetic sites — that polynomial's
  argument, the drive product, which overflows the same way four hundred dB higher up, and the
  dynamic-tame filter's state feed (the entry above) — are now all bounded at a level some
  120 dB above anything the chain can carry, so no audible sample changes (bit for bit) and the
  stage can no longer produce an infinity from any finite input. This is **not** the mechanism
  behind the field reports of a silent left channel — the owner re-tested with it in the build
  and that issue persisted — and it is worth keeping the two apart: this one needs an input
  around +120 dBFS, while the field fault was a miscompilation that needed no unusual input at
  all and is fixed separately above (`docs/POSTMORTEMS.md` INC-004). Evidence Source: PR #14.
  [Verified]
- **The GR history no longer flashes a vertical accent line at its left edge** while the
  gain-reduction trace there is non-zero. The "unmeasured region" zero-line was also drawn
  for a *full, scrolling* window whenever bucket-expiry phase left a sub-pitch gap at the
  edge, and the trace then dropped vertically from the zero line to its real value at the
  same x — flashing at the bucket-expiry rate. The zero region now draws only while the ring
  is genuinely still filling; a full window extends its oldest bucket to the edge. Evidence
  Source: PR #13 (item 4). [Verified]

### Removed
- **The corner-dot legend appended to the nine macro-managed knob tooltips** ("A corner dot
  means this knob is detached from the macros…"), by owner directive. The detach badge itself
  and Simple view's clickable reset dot are unchanged. Evidence Source: PR #13 (item 7).
  [Verified]

### Investigation (KI-009 — since CLOSED, see Fixed above; KI-012 — still open)

*This section is the round-by-round record of how the silent left channel was hunted, kept
because it is what the next such report starts from. It is written in the present tense of each
round, and the last of those rounds is not the outcome: KI-009 was root-caused and fixed on
2026-08-11 — undefined behaviour in the engine's channel bound that Clang acted on at `-flto` —
and `docs/POSTMORTEMS.md` INC-004 carries the mechanism, the excluded hypotheses and the gates
that now guard it. Where a bullet below says the fault is unfixed, platform-scoped or unreachable
from Linux, INC-004 is what superseded it.*

- **Every KI-009 regression has now run on macOS, and passes.** Restoring the build revealed
  that the broken line sat INSIDE `testClipMixCannotChangeTheDefaultPresetsSound`, so the
  regressions written in the last three rounds — for a fault that reproduces only on macOS — had
  never once executed there. The full macOS job is now green (build → both suites → pluginval ×3
  in both modes → packaging, universal arm64 + x86_64), which makes "does not reproduce" a
  measurement on the reproducing platform rather than an inference from the other one. It is
  still a negative: the reproduction lives outside what the suites drive. Evidence Source:
  PR #14. [Verified]
- **A real defect class was found and pinned in the process.** At Clip Mix 0 the clip/saturation
  stage's output is the untouched dry sample, so a poisoned internal state is invisible to the
  engine's non-finite boundary — and invariant 9's repair is keyed on that boundary. The
  0.1.3 colour-argument bound makes it unreachable in the shipped build;
  `testClipSatCannotHideANonFiniteFromTheBoundary` proves that over 30 poisoning attempts up to
  FLT_MAX, and fails with 32 000 non-finite samples **on ordinary audio** when the bound is
  removed. No audible change — this pins a property the code already had. Evidence Source:
  PR #14. [Verified]
- **The left-channel silence is macOS-only so far.** The owner re-tested on **Linux** and
  cannot reproduce it there, so KI-009 is now explicitly scoped to the macOS build (Windows
  untested by either side). Six rounds of headless work all ran on Linux, so their
  "cannot reproduce" agrees with the owner on a platform where the fault is absent — which
  says nothing about the platform where it is present. The consequence is recorded rather
  than implied: a macOS-only divergence is the only one of the three live hypotheses that
  predicts a platform split, so it leads, and no further Linux-side experiment can move the
  entry. **Superseded** — the platform split was real but the scoping drawn from it was wrong:
  the variable was the compiler, not the operating system, and building the *plugin* with Clang
  on Linux reproduced the fault exactly, which is where it was fixed
  (`docs/POSTMORTEMS.md` INC-004). Evidence Source: PR #14. [Verified]
- **New report — the Linux editor accepting no mouse input (KI-012) — does not reproduce.**
  The built VST3 was loaded into a purpose-built minimal JUCE VST3 host on a real X server and
  driven with **XTEST** pointer events, bare and under a window manager: clicks land (the ADV
  toggle resizes the window), a rotary drag moves Loudness and the parameters its macro map
  drives, and hover repaints against a zero-pixel idle control. `docs/KNOWN_ISSUES.md` KI-012
  records what that excludes, the sibling comparison (every interaction-relevant construct
  identical; the one divergence is off the input path), and the single field datum that would
  settle it — whether the meters move while audio plays. Evidence Source: PR #14. [Verified]
- **The left-channel silence was not fixed by the colour-stage guard.** With that guard in the
  build the owner re-tested on macOS in both AU and VST3: the behaviour was identical across
  formats, unaffected by oversampling or any other setting, removed only by Clip Mix = 0 — and
  **global bypass restored the channel**, which placed the loss inside the processed path rather
  than in host routing. Every one of those observations held and every one of them is explained
  by the real cause, found later in this round and fixed above; `docs/POSTMORTEMS.md` INC-004
  carries the mechanism and why the two faults share a fingerprint. Evidence Source: PR #14.
  [Verified]
- The owner's new per-channel GR observation (comp GR on both lanes, limiter GR on the right
  lane only, both stereo links at 0) localises the left-channel kill to the span between the
  compressor's output and the limiter's detector tap. The exact field configuration is now a
  permanent headless case at both oversampling extremes (both green), and the OS toggle was
  named as the decisive field experiment. That localisation was right and the experiment was
  never needed: the span it named is where the miscompiled channel loop sits
  (`docs/POSTMORTEMS.md` INC-004). Evidence Source: PR #13 (item 3). [Verified]

## [0.1.2] — 2026-08-09

**The field-fix round** (the owner's thirteen-item 0.1.2 directive; the contract-level
decisions are ADR-0023). This entry also releases the preset-identity work that sat in
`[Unreleased]` since PR #12.

### Added
- **A mono→mono I/O layout** — the same plugin at one channel, for dual-mono and multi-mono
  host racks. Stereo→stereo and mono→stereo behave exactly as before; a stereo source into a
  mono output stays unsupported (no downmix rule exists). Evidence Source: PR #13 (ADR-0023
  item 5). [Verified]
- **Per-channel gain-reduction meters** in the COMP and LIMITER zones — each meter is two
  lanes, L above R, identical at 100 % stereo link and diverging below it. (Beyond the
  display, this is the field instrument for the KI-009 investigation: it distinguishes a
  per-channel gain collapse from a kill outside the dynamics stages at a glance.) Evidence
  Source: PR #13 (ADR-0023 item 10). [Verified]
- **Preset identity** (carried from PR #12, previously unreleased; ADR-0022, the
  product-family port of Anamorph's ADR-0024): a factory preset is identified by an immutable
  internal id and a user preset by its file on disk, so a user preset saved under a factory
  preset's name is now the one selected — the drop-down marks exactly one row, `‹ ›` steps
  from the row that was actually loaded, and the selection survives undo, A/B, Copy and a
  session reload. A stored identity that no longer resolves (a preset deleted, renamed or
  moved; a file loaded from outside the preset folder) selects **no** row rather than a
  same-named substitute; sound restoration is unaffected in every such case. The identity is
  three additive per-slot strings in the **session** only — user `.anabasis` preset files are
  byte-for-byte unchanged, and sessions saved before this change load exactly as before.
  Evidence Source: PR #12. [Verified]

### Changed
- **No gain reduction below the engagement level — the item-2 field fix, three mechanisms
  deep (ADR-0023):** the compressor's knee now softens the onset *above* the threshold
  (zero gain at or below it; the old centred knee computed real gain from −3 dBFS up at the
  0 dBFS default), the limiter's detector is *unfiltered* (the shared SC HPF both under-read
  bass overs into the safety clamp and over-read low-frequency transients by up to ~6 dB of
  filter overshoot — reduction on material that never crossed the ceiling), and the
  compressor's filtered detector magnitude is clamped to a raw-magnitude ceiling — a peak
  envelope of the input, so a sidechain HPF may only deafen the detector, never sharpen it,
  and never re-couples it to the bass the control exists to ignore. The all-defaults null now holds for **any**
  sub-ceiling input, and Delta at the default preset on such material is exact digital
  silence. Peaks above the −0.1 dB Ceiling still draw their by-definition reduction. **SC
  HPF is therefore a compressor-side control now**; a bass-heavy over is limited rather than
  hard-clipped whatever it is set to. Evidence Source: PR #13. [Verified]
- **The GR history draws at a fixed scale** — no startup zoom: a fresh trace grows from the
  right edge at the settled pitch, and the unmeasured region to its left stays empty (level
  0, GR 0 — nothing estimated, interpolated or stretched). **Pause/resume continues the
  timeline**: the history now survives a transport-start re-prepare and clears only when the
  sample rate or block size actually changes. Evidence Source: PR #13 (ADR-0023 item 6).
  [Verified]
- **The graph well opens on the GR history** (was the spectrum), and its GR|SPEC switch
  moved to the bottom-left corner — the old top-right position covered the newest reduction,
  the data being watched — with GR as the left segment and **the whole pill acting as one
  toggle**: clicking it always switches to the other view, so no press on it is a silent
  no-op (clicking the active SPEC segment previously did nothing). Stored sessions keep
  whichever view they saved. Evidence Source: PR #13 (ADR-0023 item 7). [Verified]
- **Editor captions drop the stage prefix** inside the captioned zones — Ratio, Threshold,
  Attack, Release, Knee, Mix and Stereo Link in COMP; Gain, Release and Stereo Link in
  LIMITER — while automation names keep it, and the limiter's automation name gains its
  prefix: **"Stereo Link" → "Limiter Stereo Link"** (a display-name rename, ID unchanged,
  snapshot re-frozen per PARAMETER_COMPATIBILITY_POLICY rule 2; beside 0.1.1's "Comp Stereo
  Link" the bare name was the ambiguous lane of the pair). Screen readers keep announcing
  the full automation names. Evidence Source: PR #13 (ADR-0023 item 8). [Verified]
- **The Advanced view's read-only macro row is removed** and the window tightens to
  940×822 — the three Loudness/Character/Tone mirrors were display-only; the macros live in
  the Simple view and the per-control detach dots remain. The Dither caption now sits on the
  same baseline as Input Gain and SC HPF. Evidence Source: PR #13 (ADR-0023 item 9).
  [Verified]

### Fixed
- **KI-009 (left channel silent) — narrowed, instrumented, and its confound removed.** The
  0.1.2 report's Delta observation proves the left input pin is live and the loss sits in
  the processed leg — a fingerprint two independent audits could not express in the current
  source with intact state (KNOWN_ISSUES KI-009, 0.1.2 addendum, carries the analysis). What
  ships: the false GR that contaminated the Delta diagnosis is gone (above), the per-channel
  GR lanes disambiguate the remaining hypotheses in one field glance, the channel-symmetry
  battery gained the six diagnostic configurations the report was observed under (Delta
  engaged, loudness-comp on, limiter link 0 %, shaped 16-bit dither, true peak, 44.1 kHz),
  and mono→mono closes the last layout-negotiation surface. Evidence Source: PR #13.
  [Verified]
- A user preset sharing a factory preset's name no longer shows the selection mark on **both**
  menu rows, and saving over a factory preset's name no longer leaves the mark on the factory
  row (carried from PR #12, previously unreleased). Evidence Source: PR #12. [Verified]

---

## [0.1.1] — 2026-08-07

**The first release entry this repository writes.** v0.1.0 was declared code-complete on
2026-08-02 but was never tagged: the post-v0.1.0 review rounds landed first, and the owner cut
the release one patch level on. This entry therefore carries the whole P1–P6 development *and*
the 0.1.1 round — the sections below run newest first.

### Added
- **Waveform Statistics panel** — the metering panel now shows seven readings, identically in
  Simple and Advanced: momentary (400 ms), short-term (3 s), integrated, **true peak**,
  **sample peak**, **RMS** (50 ms Hann window) and **loudness range** (LRA, EBU Tech 3342),
  plus PLR. Two new Settings choose which STANDARD two of them follow: *Integrated*
  (BS.1770-2+ gated, the default, or BS.1770-1 ungated) and *RMS Reference* (AES-17, the
  default, or mathematical). The true peak is no longer hideable — the *True-Peak Meter*
  toggle and the field behind it are gone. Reading TP against the new SP row gives the
  inter-sample overshoot directly. ADR-0020; session field `int_tpMeterOn` removed,
  `int_integratedStd` / `int_rmsRef` added (an old session's value for any of the three is
  handled by the schema's read rules — no migration needed).
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]
- **Comp Stereo Link** — the glue compressor gains the adjustable stereo link the limiter
  already had (0–100 %, default 100 %). At the default it is bit-for-bit the fully linked
  detector it always was; lower values let each channel breathe on its own. Named apart from
  the limiter's *Stereo Link* so the two automation lanes cannot be confused. The parameter
  surface is now 50. ADR-0019.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]
- **Installers, and a release pipeline that builds them** — a Windows Inno Setup installer, a
  macOS `.pkg` with component selection (VST3 / AU / Standalone), and Linux
  `install.sh`/`uninstall.sh` inside the zip, each with an `INSTALL.txt`. A tag now drives a
  validate → build → draft-release pipeline that archives the per-platform trees, attaches the
  installers, publishes SHA256SUMS and a build manifest, and takes its release notes from this
  file. ADR-0021; closes the OQ-007 deferral.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]

### Changed
- **Copy (A/B) is undoable and no longer destroys history** — copying one slot's sound into the
  other now records an undo step on the DESTINATION slot and keeps that slot's earlier history
  beneath it, so one Undo reverts the Copy and further Undos walk back through the slot's own
  edits. **The ADV view switch is undoable too.** Neither travels with an A/B compare: switching
  slots still never resizes the editor. ADR-0018.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]
- **Frequency text entry speaks mastering shorthand** — on the high shelf (1–20 kHz) a bare
  number up to 20 is read as kHz, so `8` lands 8 kHz; on the full-range bells (20 Hz–20 kHz) the
  pivot is the knob's own 20 Hz floor, so `19` lands 19 kHz while `20` stays 20 Hz. `8k`,
  `8 kHz` and `8000` all mean the same thing everywhere.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]
- **Graph-well switch, GR history and zone layout** — the single-name corner chip is replaced by
  a two-segment SPEC | GR switch that shows both modes and floats translucently above the trace
  (the old chip was masked by the GR zero-line); the GR history no longer shimmers while
  scrolling (fixed-identity decimation buckets and a filled waveform path instead of a
  re-phasing comb of 1 px bars); the four zone combos span their panel, so *Transparent* no
  longer clips; the dither combo gains its caption.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]
- **About panel** — a shorter description in the Version line's colour, and the URL sits
  directly above the copyright, matching the family layout.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]
- **The knob corner dot explains itself** — each of the nine macro-managed knobs carries the
  detach legend in its tooltip, and the manual states what the dot is not: not an
  edited-since-preset mark, and not cleared by turning the knob back, because the knob is still
  off macro control.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]
- **Artifact contents match the sibling product's** — `NOTICE` and `THIRD_PARTY_LICENSES.md` are
  no longer copied inside the archives; they ship as version-named assets on the release page,
  where they accompany every distribution route including the installers. ADR-0021 amends
  `RELEASE_POLICY.md` accordingly.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]

### Fixed
- **The left channel could fall silent on a mono source** (KI-009). The plugin refused the
  mono→stereo layout the sibling accepts, so a host with a mono source had to negotiate
  stereo→stereo and feed the signal on one input pin and silence on the other; this chain is
  strictly dual-mono, so the silent pin produced a silent output channel in both modes. Mono
  input is now accepted and duplicated before the chain. The headless stereo battery gained the
  case that reproduces it.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]
- **A Bypass or monitor-toggle click no longer eats an Undo press** — those clicks used to mint
  an undo step whose restore changed nothing. ADR-0018.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]

### Removed
- **The Settings *True-Peak Meter* toggle** and its session field, with the statistics panel that
  makes it meaningless (see Added). ADR-0020.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]

---

### Added
- **P1 skeleton — the plugin exists** (VST3 / AU-on-macOS / Standalone; JUCE 9.0.0 pinned):
  the 49-parameter surface of `DESIGN.md` §4.2 (IDs frozen, registry snapshot under
  `tests/fixtures/`), a pass-through chain with a basic lookahead limiter and the final ceiling
  clamp, constant reported latency (10 ms lookahead allowance — browsing presets never moves
  host PDC, ADR-0004), Simple-mode macro knobs mapped by the §5.5 draft curves, schema-v1
  session state with A/B slots, and a placeholder editor. No EQ/compressor/clipper processing,
  no oversampling, no metering yet — those are P2–P5.
  Evidence Source: **PR #4** (`skyRolly/Anabasis`) — commits `91ece13` (skeleton), `e0c24d5`
  (limiter alignment + state restore fixes), `79fd781` (control smoothing), `d6fa408`
  (macro mapping off the audio thread, defaults-first read rules) and `0190aee` (LF snapshot
  pin, restore guard). The PR is the stable citation: it keeps resolving after the branch is
  deleted at merge, which a branch-relative "the commit that follows" does not
  (`CHANGELOG_POLICY.md` rule 2). [Verified]

- **P2 DSP core (in progress)** — the chain stages now process audio: the §2.2 EQ (tilt pair,
  shelves, two bells, Pre/Post with the clamp-last guarantee), the §2.3 glue compressor
  (log-domain, soft knee, RMS/Peak, two-pole auto release, sidechain HPF, parallel mix), and the
  §2.4 clipper/saturation (hard↔soft knee morph with first-order ADAA — measured 14.8/10.4 dB
  alias reduction at OS Off — colour models with odd/even balance and tone, the dynamic HF tame,
  drive with level compensation — and the limiter's push sits AFTER it, so raising loudness does
  not drive the clipper harder), and the §2.5 limiter upgrades (true-peak mode with the 4×
  ADR-0003 measurement tap — the ceiling is dBTP-aware — stereo link, Transparent/Punchy/Loud
  styles, transient preservation, two-pole auto release, shared sidechain HPF), §3
  oversampling (Off/2×/4×/8×/16× × min/linear phase wrapping Clipper→Limiter, all instances
  built at prepare, integer-latency mode so reported PDC is exact across the whole matrix,
  Force-Max offline honoured) and §4.5 dither (TPDF 16/24-bit + first-order noise shaping,
  deterministic, after the clamp), and the §2.8 click-free transition layer (asymmetric
  raised-cosine duck ~6 ms out / ~28 ms in: engine rewires — EQ position, colour model, OS
  factor/phase — execute only at the silent bottom; A/B, preset and session-load bulk swaps
  request the duck before swapping — closes KI-001 → POSTMORTEMS INC-001). All-defaults remains
  a bit-exact null; bypass stays a bit-exact null at every oversampling factor.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`). [Verified]

- **P3 metering engine (in progress)** — BS.1770-4 LUFS M/S/I with the two-stage gate
  (calibrated to the standard's compliance vector ≤ 0.1 LU), dBTP max-hold off the shared
  estimator, PLR, per-block GR published with a 43 s history ring, and the §2.7 monitor layer:
  loudness-compensated monitoring with loudness-matched bypass (Measure + Predict, monitor-only —
  the offline render is bit-identical either way) and delta monitoring. The Loudness Comp and
  Delta toggles now work (KI-002 → POSTMORTEMS INC-002).
  Evidence Source: **PR #5** (`skyRolly/Anabasis`). [Verified]

- **P4 adaptive engine (core)** — audio-thread feature extraction (crest, tilt, transient
  density), of which **transient density and tilt** drive the §5.4 bounded trim vector around
  release / stereo link / sidechain HPF / dynamic tame (crest is published for the UI and
  reserved for a future mapping), second-scale slew with hysteresis, Freeze latching the vector
  exactly, and the
  mode-switch invariant pinned sample-identically (`testModeSwitchIsSoundNeutral`). The trims are
  engine-internal — the host, automation and undo never see them — and the bit-exact null holds
  with adaptation live. Learn (core) is in: analyse → commit fixes the
  reference targets, serialized in the global ADAPTIVE child ("absent = never learned").
  The OQ-013-gated frozen-trim restore remains the one blocked path.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`). [Verified]

### Fixed
- **An extreme input level could silence the plugin permanently.** A finite but astronomical
  sample (the kind a broken upstream plugin emits, not one a DAW produces) could overflow a stage
  that carries gain — an EQ biquad, the compressor's squaring RMS detector, the clipper's colour
  polynomial, the oversampler's filters — and the stage then held a NaN for ever. The engine's
  boundaries substituted `0.0f` for the non-finite value on the way out, so the output was silence
  and nothing signalled that anything had happened: only re-opening the session (or any host
  action that re-prepared the plugin) brought the sound back. Those boundaries now record the
  substitution and the affected stage's filter state is cleared, so the engine recovers by itself
  within the block. A hostile input buffer is unchanged: non-finite input is still zeroed before
  any state sees it, at no cost.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) —
  `testExtremeLevelDoesNotSilencePermanently`, **five** stimuli (counted from the test's own
  `run(...)` cases, not from an earlier entry — the Post-EQ case was added a round later), each
  mutation-verified against the matching half of the fix. [Verified]

- **…including with oversampling on, where the recovery was incomplete.** The first fix repaired
  the engine's own stages but not the oversampler, whose default (minimum-phase) filters are
  recursive: one infinite state fed itself and every later sample stayed non-finite, so an extreme
  input still silenced the plugin permanently at any oversampling factor. The oversampler is now
  reset when it is the stage that produced the value. The repair of the other stages also stopped
  being a blanket reset — it clears only the values that are actually non-finite, so recovering
  from a poisoned detector filter no longer snaps the compressor's gain reduction to unity.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) — the oversampler case of the same test, now at
  `FLT_MAX` (the level the polyphase filters overflow at), mutation-verified. [Verified]

- **…and with the EQ in the Post position, where the same silence survived two rounds of fixing.**
  The Post EQ sits after the limiter but before the ceiling clamp, and the limiter's *attack* — not
  the ceiling — is what bounds its input: at a short lookahead setting the gain has only fallen to
  ~0.29 by the time an extreme peak plays, and a fully boosted EQ multiplies by ~3.4, so the
  biquad overflows and the plugin goes silent for the rest of the session. Stage E now has the two
  boundaries it was missing (the decimation filters' output and the Post EQ's own), so both stages
  are repaired like the others. Separately, the limiter's detector high-pass is now checked once
  per block rather than on the recovery flag: its corruption produces no non-finite output at all
  (a `NaN` level compares false against the ceiling), so the limiter would have passed everything
  at unity gain for ever without anything to notice.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) — the Post-position case of the same test,
  calibrated against the limiter attack and mutation-verified. [Verified]

- **Meters and Simple-mode adaptation no longer break for the session on one extreme sample.**
  Both are fed signals that are finite but unbounded, and both overflow on a legal float — the
  loudness meters in their K-weighting filter, the adaptive feature extractor when it squares its
  band split. Neither emits audio, so nothing in the engine could notice: the readings became NaN,
  every gate that compares them turned false, and the result was a loudness/true-peak readout
  stuck at silence, a loudness compensation that stopped tracking, an integrated reading that
  stopped accumulating, and a Simple-mode trim vector frozen at a plausible-looking value until
  the plugin was re-prepared. Both are now checked once per block, and a Learn pass that
  accumulated a broken feature is cancelled rather than committing it into the session.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) —
  `testExtremeLevelDoesNotBreakTheMetersOrAdaptation`, one stimulus per stage (Nyquist at full
  scale for the extractor, the bypass leg for the meter), each mutation-verified. [Verified]

- **A double Learn press can no longer save a reference measured from one block.** The Learn
  command was published as a code plus a separate "pending" flag, so an engine that picked the
  command up between those two writes could have it re-raised behind it and carry it out twice —
  and a "finish and start again" press carried out twice finishes the pass it just started, one
  block old. The command is now a single value the engine takes whole.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) — `AnabasisEngine::learnCmd`; the interleaving
  itself is not headlessly reproducible, and `testStopThenStartInOneBlockKeepsBoth` pins the
  composed semantics across the change. [Partially Verified]

- **A Learn analysis that measured through an extreme sample is no longer saved as the reference.**
  Ending a Learn pass on the block after such a sample stored a broken measurement as the learned
  reference, which froze Simple-mode adaptation for good — every trim is derived from that
  reference — and was then written into the project file, so reloading did not clear it. The
  commit now refuses a measurement that overflowed, with the same outcome as an empty pass (the
  previous reference stays), and a restore that carries one reads as never-learned.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) —
  `testALearnPassThatOverflowedIsNotCommitted`, both writers mutation-verified. [Verified]

- **P5 groundwork — meter holds now reset on demand and on session load.** The integrated-LUFS
  figure and the true-peak max-hold are session-cumulative by design; until now only re-preparing
  the plugin cleared them, so loading a different project kept the previous programme's true-peak
  maximum on the meter. A reset request (wired to the P5 meter panel) clears both at the next
  audio block, and loading a session issues it automatically. Resetting during playback measures
  only post-reset material — a reset cannot be pinned at the old programme's loudness by a
  measurement window that straddles it. A transport stop still clears nothing: stop/start must
  not cancel a Learn pass or a mastering measurement (that decision is now recorded, not open).
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) — `testMeterResetClearsSessionHolds`,
  `testGrRingResetEpoch`, all halves mutation-verified. [Verified]

- **P5 — the interface exists.** The full Simple and Advanced views in the RollyTech family frame
  (Anamorph's top bar, Backdrop overlays, glass language and thin-arc controls, with Anabasis's
  gold/amber accent): the one large Loudness knob with Character / Tone / Ceiling (+lock) and the
  monitor toggles; the four Advanced zones (Comp · Clip/Colour · Limiter · EQ) over the whole
  per-stage parameter surface with a live clip transfer curve, an EQ response curve and
  per-stage GR meters; the loudness panel (LUFS M/S/I, streaming-target lines with the
  loudness-penalty estimate, dBTP, PLR — click resets the session holds); the scrolling
  GR/waveform history; the dismissible input/output spectrum overlay; Settings (oversampling,
  phase, offline quality, UI scale 80–200 %, animation, tooltips, metering options) and About;
  Learn with a minimum-pass countdown and an empty-pass readout; macro detach badges with
  reset-to-macro; accessibility names on every control. Editing a macro-managed parameter by
  hand now detaches it from the macro until the next macro gesture (or reset-to-macro) — restores
  and automation never detach.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) — suites + pluginval L8 ×3 both modes with the
  editor open under xvfb; GUI appearance itself is Level-5 manual validation. [Verified]

- **P6 (in progress) — undo, factory presets, and the measured budget.** Per-slot undo/redo in
  the top bar (one drag = one step; automation never pollutes the history; undoing an edit also
  restores its macro-detach state; preset applies undo as one step). Five factory presets from
  the brief's list (Transparent Master, Loud Pop, EDM Club, Vocal Forward, Tape Glue — settings
  are drafts until the listening pass) in a FACTORY menu section, with the preset name showing a
  dirty mark once the state is edited. A locked ceiling is never moved by any preset. The
  performance budget is now measured, not asserted: the 48 kHz / 4× working case runs at 3 % of
  one core on the recorded machine (`docs/architecture/PERFORMANCE_BUDGET.md`), and pluginval
  passes at strictness 10 locally in both modes.
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) — `testUndoIsPerSlotGestureCoalescedAndMaskWide`,
  `testFactoryPresets`, `AnabasisBench`. [Verified]

- **v0.1.0 completion (2026-08-02, under the owner's blanket approval — every item ⊕ for the
  post-v0.1.0 fine review).** The adaptive release trim now reaches the AUTO release path
  (ADR-0013): with auto release on — the factory default — the limiter's two release poles scale
  by the trim's `2^octaves` factor with their ratio preserved, so all four §5.4 adaptive
  behaviours are audible at defaults instead of three. **Freeze memory now restores**
  (ADR-0014): a frozen slot saved and reloaded — session load or A/B switch — reproduces its
  latched trim vector exactly (applied at the transition duck's silent bottom, so the restore is
  click-free), instead of re-latching from whatever the engine happened to hold. The factory
  bank grows from five to the brief's **twelve** presets (adding Rock Punch, Hip-Hop Low End,
  Acoustic Warmth, Classical Dynamics, Podcast Voice, Cinematic Wide, Lo-Fi Crush — draft values
  until the listening pass). CI's pluginval gate rises to strictness 10 on all three platforms.
  Also fixed: the Windows CI state-suite crash (stack overflow from two 128 KB inline capture
  buffers per engine instance; storage is heap-allocated at construction now, and both test
  suites print unbuffered so a crash can no longer swallow its own output).
  Evidence Source: **PR #5** (`skyRolly/Anabasis`) — `testAutoReleaseFollowsTheTrimScale`,
  `testFrozenTrimRestore`, `testFactoryPresets`; ADR-0013/ADR-0014;
  OQ-007/OQ-013/OQ-014/OQ-016 → Resolved. [Verified]

### Fixed

- **Undo and redo are click-free, and they restore a frozen slot's remembered adaptation**
  (2026-08-03). An undo step is a bulk swap like a preset load or an A/B switch and now dips
  through the same silent transition; before this it stepped, and on a frozen slot the remembered
  adaptation was not restored at all — the values could instead surface later, at the next A/B
  switch, changing the sound of a different setting.
- **Saving a project just after loading a frozen setting keeps the right remembered values**
  (2026-08-03). A save landing in the short window between the load being picked up and taking
  effect wrote the previous values and discarded the loaded ones permanently; the editor's own
  preset-dirty polling reached that window in ordinary use.
- **The meter reset no longer lets a slice of the previous, louder material into the fresh
  integrated reading** (2026-08-03): the measurement window that was in progress at the moment of
  the reset is now excluded, which is what the reset was always documented to do.
- **The Settings drop-downs select what they say, and keep showing it** (2026-08-03).
  Oversampling, phase and offline-render quality were each off by one — picking "Off" turned
  oversampling on, "Minimum" gave linear phase, "Follow" forced maximum quality — and all three
  opened blank until touched. They also now follow the stored setting, so a project loaded with
  the Settings panel open no longer leaves them showing the previous project's choices.
- **Automating Bypass with the window open no longer risks an audio dropout** (2026-08-03): the
  window asked the message queue for work from inside the audio callback, which can take a lock.
- **The true-peak readout turns red against YOUR ceiling** (2026-08-03), not against a fixed
  −1 dBTP: at any other ceiling — including the −0.5 the "EDM Club" preset sets — it warned at the
  wrong level, staying neutral while genuinely over or lighting up while legal.
- **A knob is no longer wrongly marked as "taken off the macro" after loading a preset or project**
  (2026-08-03). A knob edit arriving moments before the load could be applied to the freshly
  loaded settings, leaving that knob stuck at the old value and no longer following
  Loudness/Character/Tone — and the wrong state was saved with the session. Related: grabbing a
  macro knob now re-engages the whole set within the same update, instead of leaving one
  parameter reading as re-engaged while it still held the old value.
- **Factory presets change the sound** (2026-08-03). A factory preset expresses itself through the
  three macro knobs, and the translation from those positions to the compressor / clipper /
  limiter / EQ settings was being cancelled as the preset loaded, so every factory preset left the
  processing at its defaults. User presets were never affected (they carry every value themselves)
  and still land exactly as saved.
- **The preset name's edited mark tells the truth after a project load or an A/B switch**
  (2026-08-03). The record of "what this preset landed" is now kept per A/B slot and dropped on a
  load, so a freshly loaded preset no longer shows as edited and slot A is no longer marked
  against a preset applied in slot B.
- **Double-clicking a knob to reset it now behaves exactly like alt-clicking it** (2026-08-03): it
  is undoable, and on a Simple-mode macro parameter it detaches from the macro the same way.
- **Automating Loudness, Character or Tone no longer takes back a knob you had just taken off the
  macro** (2026-08-03). When the host moved a macro while a knob edit was still being picked up,
  that knob was written from the macro curve and only afterwards marked as detached — so it stuck
  at the macro's value instead of yours, and the session was saved that way. Every route into the
  macro update now applies your edits first.
- **The Settings panel follows a project load completely** (2026-08-03). The oversampling, phase
  and offline-quality drop-downs were fixed earlier in this branch; the three streaming-target
  checkboxes were not, so a panel left open across a load kept showing the previous project's
  targets while the loudness meter already drew the new project's target lines — and the first
  click on a checkbox then read as toggling the wrong one.
- **Tooltips are drawn in the product's own style** (2026-08-03): the tooltip window was created
  without being told which look-and-feel to use, so it fell back to the JUCE default and the
  designed capsule never appeared.
- **Opening a project resets the meters immediately, not at the next note** (2026-08-03). Loading
  a session clears the integrated loudness and true-peak hold — but the clear only took visible
  effect once audio started, so a project opened with the transport stopped showed the previous
  session's readings for as long as it stayed stopped.
- **The preset name's edited mark updates immediately after undo, redo, A/B, a preset change or a
  save** (2026-08-03), instead of keeping the previous state's mark for up to a third of a second.
- **Knobs are drawn where they are the moment the window opens** (2026-08-03), instead of sweeping
  up from their minimum over the first few frames.
- **Dragging a knob's number behaves like dragging the knob** (2026-08-03): the edit is now
  undoable, and on a Simple-mode macro parameter it takes that control off the macro the same way.
- **Grabbing a macro knob puts every knob you had taken off it back on the curve** (2026-08-03),
  even when the grab moves nothing. Previously a click-and-release re-attached them in name only:
  they stopped showing as taken-off but kept the values you had dialled in.
- **Copying one A/B slot over the other starts that slot's undo history fresh** (2026-08-03).
  Before, the first undo after switching to the copied-into slot restored a state from before the
  copy — silently discarding both the copy and that slot's last edit.
- **Undoing a preset change restores the edited mark too** (2026-08-03): the name reverted but the
  " *" kept comparing against the preset that had just been applied.
- **"Reset to macro" can be undone** (2026-08-03). It re-attaches every knob to the macro and
  re-lands nine values at once, and it was the only change of that size the user could not take
  back.
- **An un-frozen A/B slot no longer saves a Freeze memory it does not hold** (2026-08-03), and a
  saved Freeze memory is no longer overwritten with empty values by a plugin that has been opened,
  or whose sample rate the host has just changed, before any audio has played through it again.
- **The spectrum display no longer shows the wrong frequencies right after a sample-rate change**
  (2026-08-03): the analysis it had already collected at the old rate stayed on screen for about a
  tenth of a second, drawn against the new one.
- **Switching A/B mid-drag no longer puts an undo step on the wrong slot** (2026-08-03). The step
  described the slot you left, so undoing on the slot you arrived at restored values it never had.
- Smaller: a host that reports control gestures across threads can no longer split one drag into
  the wrong undo steps, and a macro knob grabbed right after such an edit re-engages it as
  documented; a well-formed preset file from another plugin is refused without costing an undo
  step; the transfer/EQ curve display follows a host sample-rate change; the oldest column of the
  gain-reduction history can no longer be drawn from an entry the audio thread is still writing;
  closing the plugin can no longer race its own background housekeeping.
  Evidence Source: **PR #6** (`skyRolly/Anabasis`) — `testUndoRequestsDuck`,
  `testFrozenTrimRestore` (undo + save-window cases), `testMeterResetIgnoresTheStraddlingSubBlock`,
  `testAGestureEndWithoutACountedBeginIsIgnored`,
  `testThePostedDrainAlsoTakesTheWrapperBitsFirst`,
  `testGrHistoryWindowNeverAsksForTheHeadSlot`,
  `testTheSettingsPanelFollowsAProjectLoad`,
  `testTheWholeTickIsSuppressedInsideARestore`,
  `testMeterResetClearsSessionHolds` (the no-audio-at-all case),
  `testTeardownAndReengageInvariants`, `testStateReplacementAndHistoryConsistency`,
  `testPreparedStateAndSlotOwnership`; all mutation-verified. [Verified]

- **Changed under the same PR:** the "Transparent Master" and "Classical Dynamics" factory presets
  now select the **Clean** colour model explicitly. Both are described as untouched, and both were
  inheriting the parameter's Tape default — inaudible at their settings, but only because the
  colour depth the macro layer writes happens to be ~0 there. ⊕ with the rest of the factory values,
  pending the listening pass.
  Evidence Source: **PR #6** (`skyRolly/Anabasis`). [Verified]

### Changed (round-2 owner directive, 2026-08-05; recorded by **ADR-0015**)
- **Defaults:** every true-peak switch (limiter mode and the true-peak meter row) now defaults
  **off**; the Ceiling defaults to **−0.1** everywhere (parameter, engine POD, and the one factory
  override that pinned −0.5 is gone). The registry snapshot was deliberately re-frozen — nothing
  has shipped, so the change is compatibility-free by the contract's own terms, and
  [`ADR-0015`](docs/architecture/design-decisions/ADR-0015-pre-ship-contract-refreeze.md) records
  both the rationale and the condition that closes that window.
- **The Ceiling now says which limit it is holding.** With true-peak mode off — the new default —
  the ceiling is a **sample-peak** limit, so its readout prints `dB`; engaging **TP** moves
  detection to the oversampled rate and the readout becomes `dBTP`. It used to print `dBTP`
  unconditionally, which claimed an inter-sample guarantee the default configuration does not
  make. The number, the automation and the saved state are unaffected — only the unit shown.
- **A "Default" preset** opens the plugin: factory index 0 with zero intents, the bank now
  Default + the brief's 12. A fresh instance reads clean and stars on the first edit.
- **Streaming-platform analysis removed outright:** the loudness-target lines, penalty rows,
  target checkboxes and their session field (`int_meterTargets`) are gone — platforms normalise; a
  master is pushed against the ceiling, not a platform figure. Old sessions carrying the field
  load unaffected: the defaults-first read rules ignore it and a re-save does not write it back.
  The schema removal is recorded by
  [`ADR-0015`](docs/architecture/design-decisions/ADR-0015-pre-ship-contract-refreeze.md).
- **One graph well, two views:** the spectrum and the scrolling GR history are switchable modes
  of the same panel in both Simple and Advanced, toggled by the chip on the graph's own corner
  ("GR" / "SPEC" — it names the view you switch to); the Settings toggle is gone. The spectrum's
  low end no longer staircases (the sibling's two-regime column read: Catmull-Rom under 1.5
  bins/column, bin averaging above).
- **A highly visible TP switch** joins the Ceiling in the Simple view (stacked above LOCK) —
  the same parameter as the Advanced limiter zone's TP.
- **The sibling's UI grammar lands:** circle-arrow undo/redo icons, its About layout (left-aligned
  link, no underline) with a generated one-sentence product description, its shorter A/B oval, the
  XS/S/M/L/XL five-step UI scale (M = original), Title-Case Settings terminology ("Offline
  Render", "UI Scale", "UI Animations", "Follow Online"), and Save left / Cancel right in the
  save dialog.
- **A complete tooltip set** in the sibling's voice — one terse line per parameter, held in one
  table; accessibility titles stay the registry names. The graph-well chips explain themselves;
  Bypass deliberately carries no tip.
- **Layout coherence, verified against rendered snapshots:** every Advanced zone's mode combo
  sits in the zone header (the truncated "AUTO"/"TP"/"LOCK" labels now fit their cells), Input
  Gain and SC HPF ride the utility strip as faders, and the Settings panel shrinks to its
  content.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]

### Known issue reports under investigation
- **A field report of the left channel falling silent** could not be reproduced headlessly: a
  six-configuration wrapper battery (defaults, driven macros, editor alive, 16× linear
  oversampling, factory preset, session round-trip) measures both channels carrying audio within
  6 dB in every case, and remains in the suite as a permanent guard. `docs/KNOWN_ISSUES.md`
  KI-009 records what the probes exclude and the environment details needed to proceed.
  Evidence Source: **PR #8** (`skyRolly/Anabasis`). [Verified]

These P1–P6 sections belong to the `[0.1.1]` entry above: the release the tag cuts is the first
one this repository has produced, so its notes are the whole development, not a delta against a
predecessor that was never published.
