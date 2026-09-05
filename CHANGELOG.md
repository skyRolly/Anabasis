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
notes are written, dated and complete — not that the build shipped. Nineteen such entries now exist
(`[0.1.1]`, `[0.1.2]`, `[0.1.3]`, `[0.1.4]`, `[0.1.5]`, `[0.1.6]`, `[0.2.0]`, `[0.2.1]`, `[0.2.2]`, `[0.2.3]`, `[0.2.4]`, `[0.2.5]`, `[0.2.6]`, `[0.2.7]`, `[0.2.8]`, `[0.2.9]`, `[0.2.10]`, `[0.2.11]`, `[0.2.12]`) and none has been tagged; WHICH version the first annotated
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

## [0.2.12] — 2026-09-05

**The owner's third report on the GR history: 0.2.11 had not removed the visible instability at the
right edge.** The owner's words: *"approximately 1–2 pixels of newly generated content extending to
the right as a short horizontal line … exposes the state while it is still being generated"* — in
the grey level history as much as in the yellow trace. Reproduced frame by frame on the real
processor and the real paint path, and the owner's reading confirmed before anything was changed:
every drawn vertex was frozen and moved rigidly, as 0.2.11 claimed, but the strip between the newest
complete vertex and the plot edge — the lead-out, drawn flat at the last value because the bucket
that belongs there is still collecting — was on screen, and it is the one part of the trace that
changes other than by scrolling. Measured 1.00–2.42 px long at 48 kHz / 512 on the Simple well (up
to 2.90 px at 1024 samples): once per bucket, 23 to 43 times a second, its height jumped to the new
value and the segment to its left — up to a pitch wide, 1.4 px at 48 kHz / 512 on the Simple well —
snapped from flat to sloped, in the GR stroke and the level fill alike, since both run the same
path. The fix is a clip and nothing else: the plot's visible right
boundary is now `floor (right − pitch) − 1`, left of everything the lead-out can touch; the anchor,
the bucket law, the values, the timing, the smoothing, the host-delivery behaviour and the left edge
are untouched, and every column still shown is pixel-for-pixel what 0.2.11 showed there.
Measurement trail: [`worklogs/2026-09-05-gr-history-tip.md`](worklogs/2026-09-05-gr-history-tip.md)
§7.

### Fixed
- **The right edge of the GR history no longer shows the strip that is still being generated.** The
  trace and the level fill stop `ceil (pitch) + 2` columns short of where the newest bucket is
  anchored — four columns for every block size up to 1024 samples at every rate from 44.1 kHz, and
  up to 2048 from 48 kHz up, on either well;
  five at 44.1 kHz / 2048 on the Simple well — and what reaches that boundary is always a segment
  between two complete buckets. The graph is drawn that many columns further right, so the strip
  falls outside the plot rather than inside it and the history still fills the panel's full width. The strip is never wider than half the plot, which matters only
  where one bucket would otherwise span all of it: a host handing over ten seconds of audio in a
  block leaves the 20-second window holding two points, and there the plot keeps its left half
  rather than hiding everything. Measured on the validation harness against 0.2.11 on the same
  frames (real processor, real paint path, 8 s per configuration, on five configurations: the Simple
  well at 48 kHz / 512, 48 kHz / 1024, 44.1 kHz / 512 and 44.1 kHz / 1024, and the Advanced well at
  48 kHz / 512): in the rightmost 24 visible
  columns the translation-compensated movement of the fill's top edge fell from a mean of 0.52 px
  (max 25 px; 1.5 columns per frame moving more than a pixel) to 0.07 px (max 0.44; none), and the
  stroke's from 0.24 px (max 18 px) to 0.04 px (max 0.16) — the floor the interior's content columns
  show; the last visible column carries the trace on every frame, every column beyond it is
  background on every frame, and every column left of the boundary is identical to 0.2.11 in 480 of
  480 frames on every configuration run and in 1560 of 1560 frames of a 26 s run through the settled
  window; frame against frame with no translation model, the three columns inside the boundary show
  no stroke pixel appearing or vanishing on any frame; at 75, 85, 125, 150 and 200 % UI scale the
  strip stays at least 0.89 px clear of the visible range. Evidence: this release. [Verified]

- **The oldest point of the GR history no longer changes after it is drawn.** The twenty-second
  window is a length, so its start fell inside a group of blocks, and the oldest point on screen was
  summarised from only the part of its group still inside the window: as the history scrolled, that
  point lost its earliest blocks one at a time, its value moved, and the segment crossing the plot's
  left border re-shaped. The window now starts at the oldest drawn point's own first block, so every
  point on screen is summarised from all of its blocks for as long as it is visible. Measured on the
  validation harness against the previous build on the same frames, over 3.9 million point readings
  in six configurations: changes to an already-drawn point **0** (before: 340 in 1800 frames at
  48 kHz / 512 — 19 % of frames — up to 1.53 dB, which is 5.9 px on the Simple well and 15.5 px on
  the Advanced; 370 on the Advanced well, 197 at 1024 samples, 264 at 44.1 kHz, 278 at 128 samples,
  and none at all where a group holds a single block, the one geometry that could not have the
  defect). The left-hand eight columns' translation-compensated movement fell from 0.19 px mean and
  5.8 px max to 0.09 and 2.4 — the floor a single-block-per-point configuration shows. The display
  now reaches up to one group of blocks (32 ms at 48 kHz / 512) further back than twenty seconds,
  all of it off the left edge; at host blocks of about 234 samples or fewer the window holds one
  point fewer, so that the buffer can hold every point's blocks. Evidence: this release. [Verified]

- **Switching back from the spectrum no longer shows one frame of the old history.** The GR history
  publishes what it draws once per frame, and it stops publishing while the spectrum has the graph
  well — so the pair describing "where the history is" went stale by one block for every block that
  arrived meanwhile. The host repaints a view the moment it becomes visible, and that repaint can
  reach the screen before the view's first frame callback: when it did, the first visible frame drew
  the history as it was before the switch — the whole trace, and the level fill behind it, shifted
  to the right — and the next frame snapped it back. Measured on the real paint path: with the
  repaint landing first, 148 of 248 switches drew that stale frame, up to 91.7 px out after two
  seconds on the spectrum (2.2 px after 50 ms); the view now re-derives its state the instant it
  becomes visible, before any repaint can read it, and the same 248 switches — including 200 with no
  recovery time between them — draw the current state on the first visible frame every time.
  Evidence: this release. [Verified]

### Changed
- **The history graph is drawn a few columns further right, and shows that much more of the past.**
  The boundary above would otherwise have cost the panel its four rightmost columns, leaving the GR
  history four pixels narrower than the spectrum view of the same well; instead the whole graph is
  placed four columns further right (five where the strip is five), so the boundary lands on the
  plot's own right edge and the plot keeps its full width — 904 columns on the Simple well, 604 on
  the Advanced, the same columns the spectrum draws into. The columns that frees on the left are
  filled with earlier history at the same pitch — the graph is moved, not stretched, and shows
  96 ms more of the past on the Simple well (168 ms on the Advanced) than the nominal twenty
  seconds. The 0.2.11
  entry's "the last pixel column of the GR plot no longer blinks" now holds for the last VISIBLE
  column; the plot's own last columns are no longer drawn at all. Evidence: this release. [Verified]

## [0.2.11] — 2026-09-05

**The owner's second report on the GR history: 0.2.8 had not fixed the part of the line that is
being generated.** The owner's words: *"the newly generated line can have instantaneous changes,
and it also changes while it is moving."* Reproduced frame by frame on the real processor and the
real paint path, and confirmed by the owner against that description. The completed trace glided
rigidly, exactly as 0.2.8 claimed — but the newest vertex was a live estimate: a minimum over a
window that slid with every block, drawn pinned to the right edge while its bucket filled, released
to drift once complete and then re-sprouted. So the last pitch of the line was revised on every
block and re-shaped on every frame, and a value just shown could still rise when the block that set
it left the window. 0.2.8's own claim that its part B had halved the tip's movement was not
reproduced on the real limiter (2.6 px per frame against 0.2.7's 2.9, identical at stride 1, and
more upward pops on the Advanced well). The newest vertex is now created once, when its bucket is
complete, at the value it will keep, and obeys the same rigid law as every other vertex. Nothing on
the audio thread, in the ring or in the smoothed head moved; the scroll timing on hosts that
deliver blocks in bursts, or at a size other than the one they prepared, is unchanged and is filed
as OQ-017. Measurement trail:
[`worklogs/2026-09-05-gr-history-tip.md`](worklogs/2026-09-05-gr-history-tip.md).

### Fixed
- **The right-hand end of the GR trace no longer changes after it is drawn.** A bucket is drawn
  only once all of its blocks have arrived, at its final value, and the strip between it and the
  edge holds that value flat until the next bucket completes. Measured on the validation harness —
  real processor, real paint path, 8 s per configuration, Simple and Advanced wells, 44.1 and
  48 kHz, 512 to 2048-sample blocks, 1× and 2× scale — against the pre-fix code on the same
  frames: revisions of an already-drawn vertex 0 (pre-fix 24–36 per second, up to 22 px on the
  Simple well and 57 px on the Advanced), re-sloped segments 0 (pre-fix 30–45 per second),
  ledge-to-spike collapses 0, and the completed trace still translates at one uniform step per
  frame with zero deviation on a steady host. The newest value reaches the panel up to `stride − 1`
  blocks later than before — 21 ms at 48 kHz / 512 on the Simple well, 32 ms on the Advanced —
  which is the price of never revising a drawn vertex. Evidence: this release. [Verified]
- **The last pixel column of the GR plot no longer blinks.** The flat lead-out ended on the anchor,
  the left boundary of the last column, with a butt cap, so that column was lit only by the spill
  of a steep segment ending there — dark on 52 % of frames at 48 kHz / 512 (37 % in 0.2.7). It now
  runs to the clip edge and the column is lit on every frame. Evidence: this release. [Verified]

### Changed
- **The first bucket after a reset appears when it completes, not before.** For the first
  `stride − 1` blocks the zero line spans the whole panel; 0.2.8 drew one bucket from the first
  block and revised it as it filled. Evidence: this release. [Verified]

## [0.2.10] — 2026-09-03

**0.2.9's guard was too wide, and it guarded only half the door.** Review of that change found both,
and both are fixed here. The guard tested `! std::isfinite`, which declines an infinity as well as a
NaN — but an infinity is not an unusable number, it is an out-of-range one, and every clamp on the
path answers it with the endpoint. So a session asking for the rail got the `value` attribute
instead, silently, and a preset asking for one left the control where it stood. The predicate is now
`std::isnan`, which is the value that actually needs it: NaN is the only number a comparison-based
clamp cannot reject. Separately, 0.2.9 guarded the `raw` OVERLAY and left the `value` it falls back
to unguarded — a document with no usable `raw` and a NaN `value` still poisoned all 50 controls and
the next save still wrote every one of them back. That fallback is now read under the same rule.

### Fixed
- A stored ±infinity restores the control to its endpoint again, as it did before 0.2.9, on both the
  session and the preset path. Evidence: PR #29. [Verified]
- A session file whose `value` is unusable and which carries no usable `raw` no longer leaves the
  control in that state, and can no longer be written back by the next save: the unreadable `value`
  is dropped and the parameter restores to its default, which is the schema's rule for a value that
  cannot be read. Evidence: PR #29. [Verified]
- A negative sample rate can no longer reach a buffer allocation as a negative length and throw out
  of `prepareToPlay`. Not reachable from a conforming host — VST3 and AU both specify the rate
  positive — and nothing changes for any rate one can supply. Evidence: PR #29. [Verified]
- `AnabasisBench` no longer throws on a `/proc/cpuinfo` whose `model name` field is empty, and no
  longer misreads one that has no colon or no space after it. Evidence: PR #29. [Verified]

## [0.2.9] — 2026-09-02

**A session or preset file can no longer put a control into a state it cannot get out of.** Found by
a security-and-quality audit of the repository's Code Scanning surface, not by a scanner: no analyzer
reports it, because the code looks like it already clamps. It does not. Every clamp the value meets
is comparison-based — `juce::jlimit` on the session path, `NormalisableRange::snapToLegalValue` on
the preset path, and Steinberg's own `Parameter::setNormalized` last of all — and **every comparison
against a NaN is false**, so a NaN passes all three untouched. JUCE's number reader returns one for
the literal `nan`, which is all a hand-edited or half-written document needs to carry. Measured on
the code before the fix: a crafted session document left **all 50 parameters** holding an unusable
value, **31 readouts printing "nan"**, and the next save wrote every one of them back — so the damage
propagated with the file. A crafted `.anabasis` preset reached 15. Infinities were always clamped
correctly and still are; NaN was the only value that got through. No schema field is added, removed
or re-meant: an unusable `raw` now takes the same fallback the registry already gives an ABSENT one,
which is the read rule `AdaptiveEngine`'s trims and learned targets have applied since ADR-0014.
Audio was never affected — the engine's own hygiene absorbed it, verified across 140 blocks in five
configurations — so nothing about how the plugin sounds changes here. Measurement trail:
[`worklogs/2026-09-02-code-scanning-audit.md`](worklogs/2026-09-02-code-scanning-audit.md).

### Fixed
- A corrupt, truncated or hand-edited session file can no longer leave a parameter unusable: a
  non-finite `raw` is declined and the `value` attribute is used instead, exactly as when `raw` is
  absent. Affected every parameter, every readout that prints one, and every subsequent save.
  Evidence: PR #28. [Verified]
- The same for `.anabasis` preset files, which are the more widely shared of the two formats: a
  non-finite `value` is skipped like an unrecognised parameter id, leaving the control where it was.
  Evidence: PR #28. [Verified]

## [0.2.8] — 2026-09-01

**One field report: the GR history scrolled in lurches.** The owner's words: *"the newly drawn
portion of the GR history to the right of the yellow line is jittery."* The yellow line is the
trace's own flat zero-reduction run (the only yellow in the well is the trace's accent), and the
part to its right is the only part that CAN show horizontal motion — which is where the renderer
had been stepping a non-integer pitch once per decimation bucket since 0.1.2. A second review
round then found six correctness defects in that fix — four of them races — and they are repaired
in the same version; one of them widens a threading decision, which went to architecture review and
was approved
([ADR-0038](docs/architecture/design-decisions/ADR-0038-gr-history-display-scalars-cross-the-painting-boundary.md),
Accepted 2026-09-02). No DSP algorithm, parameter, serialization schema or reported-latency change, and
nothing on the audio thread moved: the ring's producer and its published index are byte-identical,
its host-thread clear gained only the two stores of the time base it now owns, and every other
change here is on the reading side. Measurement trail:
[`worklogs/2026-09-01-gr-history-scroll-jitter.md`](worklogs/2026-09-01-gr-history-scroll-jitter.md).

### Fixed
- **The GR history scrolls continuously instead of standing still and lurching.** Every vertex
  of the trace and the waveform used to move only when a decimation bucket completed — once every
  `stride` blocks — and then by a whole pitch, which is not an integer (1.447 px at 48 kHz / 512
  on the Simple well, 1.929 px at 1024). Modelled at a 60 Hz display, 48 % of frames drew no
  motion and the rest a 1.45 px jump, each jump landing every vertex on a new sub-pixel phase so
  the anti-aliased stroke re-rasterised at every step; a horizontal segment is invariant under a
  horizontal shift, which is why the flat gold zero line looked steady and everything sloped to
  its right did not. The geometry now places each bucket by the newest ENTRY's position inside
  its bucket, and between two entries by a head the frame clock smooths at the nominal entry rate
  (held to within one entry of the real head — never behind the data, never more than one entry
  ahead of it), so the trace advances `pitch / stride` per processed block and one uniform step
  per frame: the same model gives 0 % motionless frames and a per-frame spread (σ) of 0.000 px on
  a steady host where it was 0.72.
  The left edge scrolls the same way: the oldest drawn point now sits on or just beyond the
  edge with its segment clipped, where it used to sit up to a pitch inside behind a flat run and
  jump a whole pitch outward every time a bucket expired. Bucket identity, every completed
  bucket's value, the fixed pitch, the right anchor, the zero-data unmeasured region and the clear
  rule are exactly as ADR-0023 item 6 decided (amended in place, dated). Evidence: this release.
  [Verified]
- **The newest point of the trace no longer pops at every bucket start.** It aggregated only the
  entries its bucket had collected so far — a single block's value the frame a bucket began, then
  deepening — so the tip flicked at bucket rate (modelled 0.99 dB mean movement between frames).
  It now aggregates the trailing `stride` entries, the same filter length as every completed
  bucket, coinciding with the bucket the instant it completes (0.55 dB). Evidence: this release.
  [Verified]
- **A cleared history is drawn immediately, even when it refills to exactly the same length.**
  The renderer decided "nothing has changed" from the number of blocks it held, so a sample-rate or
  buffer-size change that cleared the history and then refilled it to the same count left the OLD
  trace on screen — one frame while audio keeps playing, and indefinitely if the transport stops
  there, which is exactly when a host re-prepares. It now keys on the history's identity rather than
  its length, and the scroll phase restarts with the timeline. Evidence: this release. [Verified]
- **The oldest end of the trace can no longer read a block the audio thread is overwriting.**
  At the shortest buffer sizes the drawn window spans the whole history buffer, leaving one block of
  margin between the oldest point drawn and the block being written. Drawing a frame from the
  position the display last observed spent that margin on the delay, so a single block arriving
  between the two put the oldest read exactly on the block being written. The margin is measured
  against the live write position now, and a frame that loses the race anyway is dropped rather than
  drawn from overwritten data. Evidence: this release. [Verified]
- **The scroll state is published across the render-thread boundary properly.** On macOS and
  Windows the editor composites on a GPU render thread, which read the two scroll values while the
  frame clock wrote them — a data race, and undefined behaviour, whatever the compiled code happened
  to do. They are atomic now; the display behaviour is unchanged. This widens the threading decision
  ADR-0027 took, so it went to architecture review and was **approved**
  ([ADR-0038](docs/architecture/design-decisions/ADR-0038-gr-history-display-scalars-cross-the-painting-boundary.md),
  Accepted 2026-09-02). Evidence: this release. [Verified]
- **The first frame after the history restarts is drawn where it belongs.** A sample-rate or
  buffer-size change clears the history and restarts its timeline; the scroll offset from *before*
  the clear could still be applied to the first frame drawn after it, putting that frame one step
  ahead of where the new history actually starts. The offset now travels with the identity of the
  history it was measured in, so a restarted timeline is drawn from its beginning and ordinary
  scrolling resumes on the very next frame. Evidence: this release. [Verified]
- **The history buffer's stored values are read and written atomically.** The display's guards
  already noticed when the audio thread had overtaken a frame's read of the history and threw that
  frame away — but noticing is not enough: reading a block while the audio thread writes it was
  undefined behaviour the moment it happened, whatever the guard did next. The stored values are
  atomic now, so such a read is defined (each value is one of the two, never nonsense) and the guard
  keeps doing exactly what it did. **The audio thread is unaffected**: its store compiles to the same
  instructions it always did, verified against the generated code, and a build on any target where
  that would not hold now fails rather than shipping. Evidence: this release. [Verified]
- **A spectrum reset clears the old trace as soon as any part of the reset is visible.** Clearing
  the analyser after a sample-rate or buffer-size change waited on one of the two things the reset
  publishes; if the display noticed the other one first it kept showing the previous spectrum and
  then, having recorded that it was up to date, stopped looking — so the old trace could sit there
  for as long as the machine took to get round to it. The display now acts on either signal, and
  also clears the moment a read comes back empty, which is itself only possible after a reset.
  Nothing else about the analyser changed. Evidence: this release. [Verified]
- **The GR meters take their one-or-two-lane layout from a value the host cannot change under them.**
  The editor asked the plugin for its live channel count from a timer while the host could be
  rewriting it — undefined behaviour — and it was also the wrong question: the lanes show
  per-channel measurements and must be drawn in the layout those measurements were taken in. The
  count is published now, at every point the layout is actually decided. Evidence: this release.
  [Verified]
- **A history frame the audio thread has already overwritten is no longer drawn.** The display
  checks, after building a frame, whether the audio thread has run far enough ahead to have
  overwritten the oldest history it just read — and throws such a frame away. That check could
  silently fail: the audio thread published the history values and the position separately, so the
  display could read overwritten values and still see an old position, conclude nothing had
  happened, and draw them. It now publishes them in an order the check can rely on, so either the
  frame's data is intact or the frame is dropped — there is no third case. Nothing about the picture
  changes on a healthy machine, and the audio thread pays nothing measurable: the same instructions
  on Intel and Apple's x86 machines, one extra ordering instruction per buffer on Apple silicon.
  Evidence: this release. [Verified]
- **The spectrum and EQ displays read the sample rate from a source the host cannot change under
  them.** Both took it from a value the host rewrites during a reconfiguration while the display was
  reading it — undefined behaviour, on every platform, and on the EQ curve from two different
  threads. They now read the same published copy the history display has used since the previous
  round. The only visible difference is during a reconfiguration itself, where the EQ curve may show
  the previous rate's response for that moment instead of the new one. Evidence: this release.
  [Verified]
- **The spectrum analyser's captured audio is read and written atomically too.** The two capture
  rings behind the spectrum display had the same defect the history buffer did: the audio thread
  wrote their samples while the display read them, and if the audio thread got far enough ahead of a
  display frame those were undefined reads, whatever the compiled code did. The stored samples are
  atomic now, so such a read is defined — at worst one analyser frame mixes older and newer audio,
  which fades out on the display's own ~120 ms smoothing. Nothing about the picture changes. **The
  audio thread pays a measured 0.005 % of one buffer's time** for it, verified against the generated
  code and against the timing of the capture itself, and a build on any target where that store
  would take a lock now fails rather than shipping. Evidence: this release. [Verified]
- **The display's time base can no longer be torn by a host reconfiguration.** The sample rate and
  buffer size the history maps its 20-second window and scroll rate through were read from values
  the host's thread rewrites during a reconfiguration, while the display was reading them — a data
  race, and undefined behaviour, whatever happened to be drawn. The pair now travels with the
  history it describes: it is published inside the same clear that restarts the history, so a frame
  that read it while a reconfiguration ran through is discarded and the next is drawn from the
  settled values. Under a stable configuration nothing moves — the same window and the same scroll
  rate, verified identical from either source. Evidence: this release. [Verified]

### Changed
- **The view repaints while the smoothed head is still moving.** Between two entries the trace
  now drifts on the frame clock, so up to one entry period of frames is drawn after the last
  arrival; once the smoothed head parks one entry ahead of the real one, the pre-0.2.8 gate —
  repaint on new data only — is back in force, so a stopped transport costs no per-vblank paints.
  Each frame draws the history up to the entry its phase was computed for, so an entry landing
  between the frame tick and the paint waits one frame rather than skipping ahead of the ramp.
  The plot is clipped to its own columns, which costs the 0.7 px of stroke end-cap that used to
  spill into the panel margin. Evidence: this release. [Verified]

---

## [0.2.7] — 2026-08-30

**LLVM 23.1.0 is released; what `apt.llvm.org` ships for major 23 is not — so the Clang pin HOLDS
at 22 and the install learns to tell the difference (ADR-0037).** The directive behind this round
asked for the newest stable released toolchain *and* forbade release candidates; those turned out
to be different instructions. Investigating which one applied is the round.
No DSP algorithm, parameter, serialization schema, threading model or reported latency change; no
first-party C++ or JUCE source file touched. Full measurement trail:
[`worklogs/2026-08-30-llvm-23-toolchain-upgrade.md`](worklogs/2026-08-30-llvm-23-toolchain-upgrade.md).

### Fixed
- **A release-candidate compiler could pass every check in this repository, and did.**
  `apt.llvm.org`'s noble suite builds `clang-23` from `release/23.x` commit `55feb0a3b6b7`, which
  sits after the `llvmorg-23.1.0-rc3` tag and before the release commit and still carries
  `LLVM_VERSION_SUFFIX -rc3`. Debian's packaging drops that suffix, so `clang-23 --version`,
  `__clang_version__` and the dpkg version **all** report a clean `23.1.0`. Installed and measured,
  it cleared the warning gate, both suites, ASan+UBSan, the LTO lane and RealtimeSanitizer — because
  every one of those checks is major-only. `scripts/setup-llvm-apt.sh` now reads the upstream commit
  out of the installed package's own version string and asserts it **is** the commit
  `llvmorg-<version>` points at, fail-closed, with one `git ls-remote` and no clone.
  Evidence: the assertion accepts `1:22.1.8~++…+ca7933e47d3a` (`llvmorg-22.1.8^{}` =
  `ca7933e47d3a3451…`) and refuses `1:23.1.0~++…+55feb0a3b6b7` (`llvmorg-23.1.0^{}` =
  `ea7d852a70e8…`); `./scripts/setup-llvm-apt.sh 23` exits 1 naming both commits. [Verified]
- **`setup-llvm-apt.sh` cited `apt.llvm.org/llvm.sh`'s `CURRENT_LLVM_STABLE` as evidence of what
  upstream had released.** That variable read `22` on a day when 23.1.0 was released *and*
  packaged — so the citation, left standing, argued for never upgrading. The header now names
  `releases.llvm.org` / `llvm.org` as the release record, and the tag as what settles it.
  [Verified]
- **The pinned-compiler comment block claimed a warning baseline measured on `g++-14.2.0`**, two
  majors behind the GCC pinned since 0.2.2. It now reads `clang-22.1.8 and g++-16.2.0`. [Verified]

### Changed
- **ADR-0031's "upstream stable" is sharpened to "the newest major this distribution's package
  source ships as a RELEASE".** The ambiguity between that and "the newest release" is precisely
  what let an RC through. Its sibling-parity clause is demoted to a consequence: Anamorph also pins
  22, but this round deliberately does not lean on that — had 23 shipped as a release, this
  repository would have moved alone and recorded the divergence. [Verified]

### Unchanged, and checked rather than assumed
- **The Clang pin stays at major 22**, which apt builds from its release tag `llvmorg-22.1.8`
  exactly — the contrast that shows the packaging scheme is sound and only major 23's suite is
  behind. **23 was exercised end to end** — on the build apt ships (`55feb0a3b6b7`, branch state shortly
  before the release commit, so evidence the tree is ready for 23 rather than a measurement of
  23.1.0): warning gate zero first-party warnings (426 vendored, ungated), suites **301 + 873**,
  ASan+UBSan **300 + 873** under `halt_on_error=1` with zero special-case-list diagnostics, the
  `-flto` lane **301 + 873** linked by `Ubuntu LLD 23.1.0`, and RealtimeSanitizer's full DSP suite
  **296, 0 failures** with its canary still aborting at exit 43. Re-run those against the real
  release when the suite rebuilds, then the bump is one line. [Verified]
- **GCC stays at major 16.** `gcc.gnu.org` lists 16.2 as newest released with 17.0 still in
  development, and Docker Hub's `library/gcc` publishes `16.2.0` with no 17 tag — so
  `ANABASIS_GCC_VERSION: 16` is already the newest stable major, and its floating `gcc:16`
  container already collected 16.1 → 16.2 with no commit here. [Verified]
- **CMake, Ninja and ccache carry no version pin to move** — runner-image and distribution
  supplied, already printed in CI by the composite action, and covered by the Architecture Review
  Gate's rule 2 (a version the image supplies can only be detected and recorded, not gated).
  pluginval remains deliberately unpinned, as `DEPENDENCY_POLICY.md` records. GitHub Action refs
  are SHA-pinned and owned by Dependabot, which bumped `github/codeql-action` to v4.37.8 mid-round;
  v4.37.9 (26 Aug 2026) is one patch further and is left to the same mechanism. [Verified]

## [0.2.6] — 2026-08-30

**Parity audit round 2 (ADR-0036): the sibling's CI surface is byte-identical to the last audit's
baseline, so this round's mismatches are what that audit missed plus what 0.2.3–0.2.5 changed
here.** Nine adoptions, each verified before landing; the kept-different list re-affirmed item by
item. No DSP algorithm, parameter, serialization schema, threading model or reported latency
change; no sanitizer, compiler or optimization flag on any shipped artifact moved. Full 200-row
audit: [`worklogs/2026-08-30-parity-audit-round-2.md`](worklogs/2026-08-30-parity-audit-round-2.md).

### Fixed
- **`MALLOC_PERTURB_` was set to a measured no-op.** glibc fills *fresh* allocations with the
  complement of the byte, so the previous `255` wrote `0x00` into every never-written buffer —
  the exact benign pattern the variable exists to defeat. Now `1` (fresh = `0xFE`, −1.69e38,
  loud), matching the sibling's measured choice, on `linux`, both LTO lanes, and now
  `merge-check` too. The old comment's NaN claim described freed-block reads, not fresh ones.
  Evidence: fill bytes measured in both directions; both suites pass under the new value. [Verified]
- **`ANABASIS_BUILD_NUMBER` no longer perturbs 101 translation units per run.** The run-varying
  `PUBLIC` define became a source-file property on its only reader, `src/gui/PluginEditor.cpp` —
  the fix the 0.2.4 ccache round measured and deferred, now landed with family precedent (the
  sibling scopes its build number identically, on an 84.4%-of-compile-time measurement).
  Evidence: `compile_commands.json` — 2 of 103 commands carry the define; suites 301 + 873, 0
  failures. [Verified]
- **The Windows PE truncation guard covers the Magic read** (+24 → +26); at exactly +24/+25 the
  old bound admitted the file and the parser threw a raw `IndexOutOfRangeException` instead of
  the diagnosable error the guard exists for. [Verified]

### Changed
- **macOS validates the shipped bytes** — OQ-012's macOS half resolved by adopting the sibling's
  arrangement: packaging (dsymutil → `strip -x` → ad-hoc codesign) runs before pluginval, all
  four macOS gates read `dist/` via `ANABASIS_PLUGINVAL_BUNDLE`, and the AU is registry-installed
  from `dist/`. A defect introduced by stripping or signing now fails the release gate instead of
  shipping. The Windows half of OQ-012 stays open. [Verified structurally; CI's next run is the
  behavioural measurement]
- **The x86_64 Rosetta self-test gates the macOS customer uploads** (`id: tests_x86_64`); a
  failing Intel slice no longer ships inside a green-looking universal artifact. [Verified]
- **The MSVC toolset assert is the windows job's last step**, so a toolset that leaves the 14.x
  ABI series fails the run *after* its build/test/upload evidence exists rather than destroying
  it. [Verified]
- **ccache observability is complete (8/8 cached jobs)**: per-run `--zero-stats` added to the four
  jobs whose printed stats were lineage-cumulative, and stats steps added to `sanitizers` and
  `realtime`, which cached with no reporting at all. [Verified]
- **`macos-intel` earns its "native Intel" label**: a thin-slice `lipo` assertion now gates
  pluginval, and the randomise arms run for both formats (fresh-seed values through Intel codegen
  on an Intel FPU — a space the universal job's arms, which execute arm64, never reach).
  [Verified structurally]
- **The GCC lane gates three more diagnostics at zero**: `-Wduplicated-cond`,
  `-Wduplicated-branches`, `-Wlogical-op` — the sibling's gated extras our default flags never
  enabled. Measured before adoption: 4 vendored hits, 0 first-party.
  Evidence: both suites built with the flags, gate exit 0. [Verified]
- **Both macOS jobs remove the registry-installed AU** after its last consumer; the Windows job
  records why it is uncached (/Zi-PDB artifact vs ccache's /Z7-only MSVC support). [Verified]

### Notes
- **The JUCE pins have re-converged.** The sibling's `main` now pins the same 9.0.1 commit
  `e18f7f5`, lifting the suspension ADR-0028 recorded; `DEPENDENCY_POLICY.md` carries the update.
- **Kept different, re-affirmed**: the zero-warning contract, the excluded sanitizer sub-check,
  no fuzz job, `preflight`, no `TESTS_NO_FTZ`, the measured CXXABI floor, `linux-lto-clang`, the
  repro/probe instruments, the 0.2.5 dSYM gates, the 0.2.3 package set, Windows GUI-inclusive
  pluginval — each with its rationale in ADR-0036.
- **Recorded about the sibling** (read-only from here): its container lane's `libfreetype6-dev`
  cannot resolve on Debian trixie; its universal build still has the shared-`lto.o` collision and
  the aggregate dSYM checks that cannot see it.

---

## [0.2.5] — 2026-08-22

**The arm64 slice of the shipped macOS bundle had no debug symbols, and three checks said it did.**
Every macOS run emitted `warning: no debug symbols in executable (-arch arm64)` for VST3, AU and
Standalone while the job reported a validated dSYM. On Apple Silicon — the population most likely to
hand a developer an OS crash log — the bundle was not symbolicatable. No DSP algorithm changed, no
parameter was added, renamed or removed, no serialization schema, threading model or reported
latency moved, and **no first-party C++ source file was touched**. Analysis, citations and
verification: [`worklogs/2026-08-22-macos-arm64-dsym.md`](worklogs/2026-08-22-macos-arm64-dsym.md),
[ADR-0035](docs/architecture/design-decisions/ADR-0035-macos-per-architecture-symbolication.md).

### Fixed
- **Each `(target, architecture)` now gets its own retained LTO object.** A universal build links
  once per architecture and several targets link with LTO, so one build performed six links — all
  naming the same `-Wl,-object_path_lto` destination, five of them overwritten. The surviving object
  was one target's x86_64 codegen, so every bundle's arm64 debug map pointed at a file with no arm64
  slice, and the two non-surviving bundles' x86_64 maps pointed at another target's symbols. The
  `CMakeLists.txt` comment asserting a directory made ld64 generate unique names had it backwards:
  ld64 appends the fixed name `lto.o` to a directory (`ld/parsers/lto_file.cpp`), while a
  non-directory path is used verbatim — which is the branch that actually gives uniqueness.
  `$<TARGET_PROPERTY:NAME>` now separates targets and `-Xarch_<arch>` separates slices.
  Evidence: generated link lines show **6 distinct object paths** for 3 targets × 2 architectures
  where 1 exists today; single-arch and no-arch shapes degrade to one per target. [Verified]
- **The sanitizer comment claimed six UBSan sub-checks and named five.** The flags carry five beyond
  `address,undefined` — `vptr`, `float-divide-by-zero`, `implicit-conversion`, `local-bounds`,
  `nullability` (the C flags carry four; `vptr` is C++-only). A leftover from the sibling's set
  before `unsigned-shift-base` was dropped. Corrected in all **four** places it appeared: `build.yml`
  twice, ADR-0034 §Consequences, the ADR index row and the 0.2.2 worklog. No flag changed.
  Evidence: `.github/workflows/build.yml` `-fsanitize=` lines. [Verified]

### Changed
- **The macOS symbolication gates are per architecture slice.** Both were blind in the same way. A
  fat dSYM lists a UUID for every slice whether or not that slice carries DWARF, so the UUID-set
  match passed; and compile units were counted across the whole fat file, so 39 from x86_64 plus 0
  from arm64 cleared `-eq 0` and printed "validated dSYM captured (UUID-matched, 39 compile units)".
  The retention assertion tested only `COUNT -eq 0` and read 1 where its own comment expects one per
  (target × slice). Now: compile units are counted per slice with `dwarfdump --arch <A>`, and
  retention gates on every shipped architecture having a retained LTO object — coverage rather than
  a count, because a count tracks the target list and was not the property that broke. The success
  line reports per-slice counts instead of a sum.
  Evidence: gate logic exercised over six cases against stub `lipo`/`dwarfdump` — arm64-empty and
  x86_64-empty both discard where the aggregate logic kept; single-arch keeps, so `macos-intel` is
  unaffected. [Verified]

### Notes
- **The universal build is preserved exactly** — same architectures, same `-flto`, same shipped
  bytes. `-object_path_lto` controls only *where* ld64 writes the LTO temporary; no compiler flag,
  optimization setting or sanitizer flag changed anywhere in this round.
- **ld64's runtime behaviour is CI's measurement, not this machine's.** This environment is Linux
  with no Apple linker, `lipo`, `dwarfdump` or `dsymutil`. The CMake half is verified here by
  reading generated link lines; the effect on a real universal link is verified by the new gates on
  the next macOS run — the posture ADR-0034 took for the `gcc:16` container lane. Xcode 15+ defaults
  to `ld-prime`, which Apple has not open-sourced, so the `lto.o` naming is read from classic ld64
  and corroborated by the observed single `lto.o`.

---

## [0.2.4] — 2026-08-22

**The macOS universal build's compiler cache, measured rather than inherited.** A review asked
whether ccache really caches an `arm64;x86_64` build — older ccache refused compilations carrying
more than one `-arch` — and whether the CI saving claimed for it was this repository's number or the
sibling's. Both questions are now answered from this repository's own logs. No DSP algorithm
changed, no parameter was added, renamed or removed, no serialization schema, threading model or
reported latency moved, and **no first-party source file was touched**. Measurements:
[`worklogs/2026-08-22-macos-ccache-validation.md`](worklogs/2026-08-22-macos-ccache-validation.md).

### Fixed
- **The changelog's own entry count said nine while listing ten.** 0.2.2 and 0.2.3 each added an
  entry; the count word advanced once. Cross-checked against the `## [0.x.y]` headings rather than
  the parenthetical beside it.
  Evidence: `CHANGELOG.md`. [Verified]
- **`docs/procedures/CI_CD.md` still asserted the macOS jobs were "deliberately not cached"** in one
  row while two other rows in the same file said they were cached — stale since 0.2.2 enabled them.
  Evidence: `docs/procedures/CI_CD.md`. [Verified]

### Added
- **`macos-intel` reports compiler-cache statistics.** It is the single-arch control — it differs
  from `macos` in exactly one variable — yet it restored a cache and never said what the cache did.
  The step is read-only and `|| true`; it reports, it cannot fail the job. **Added, not yet run**,
  so it has produced no measurement so far; it is also not load-bearing, since the decisive control
  is cold-vs-warm on the universal lane itself.
  Evidence: `.github/workflows/build.yml` (verified as an edit, not as an observation). [Verified]

### Changed
- **The macOS caching rationale now quotes figures measured here.** The workflow comment, ADR-0034
  and `CI_CD.md` justified caching with "the sibling measured the equivalent job at 29m44s, 16m40s
  of it building". Measured locally, `macos` is genuinely the run's critical path at **18m43s** —
  but **12m34s (67%)** is the four pluginval passes and only **3m53s (20.7%)** is the build step
  ccache acts on, so 16m40s overstates this repository's build step about fourfold. The decision is
  unchanged; the magnitude argument is now local.
  Evidence: `.github/workflows/build.yml`, ADR-0034, `docs/procedures/CI_CD.md`. [Verified]

### Notes
- **ccache does cache the universal build, completely.** `macos-latest` installs ccache **4.13.6**,
  and across three consecutive runs the job reports `Cacheable calls: 182 / 182 (100.0%)` with **no
  `Uncacheable` bucket in any log**. Hits go **14.29% cold → 95.60% warm** at an unchanged object
  count of 182 — only `.ccache` is restored, never `build/`, so ninja issues all 182 invocations
  every run and the drop is cache hits, not an incremental build. On timing, the honest figure is
  the **compile phase**, which is all ccache acts on: **501s cold → 46–86s across two warm runs
  (415–455s, 83–91%)**. The step *total* spans 261–398s (41–63%) only because it also carries an
  LTO link that drifted 130s → 187s → 284s on a phase ccache never sees. The review's concern was historically correct and is version-bound: ccache 3.2.5
  bailed out with "More than one -arch compiler option is unsupported"; 3.3 added fat-binary support
  and 3.3.1 corrected direct-mode arch discrimination, nine years before the version in use.
  Evidence: runs 32563814120 / 32565784751 / 32568563583; ccache `ccache.c` v3.2.5 and `NEWS.adoc`;
  reproduced locally on ccache 4.9.1 against a control that shows what a real decline looks like.
  [Verified]
- **What the cache does not reach:** ccache's counters stop moving ~46s into the 233.5s warm build
  step. The remainder is the LTO link, which ccache does not cache — the saving is almost all of the
  compile time and none of the link time. This is why the step total is the wrong number to quote:
  across the two warm runs the uncached link alone varied by 97s.
  Evidence: run 32565784751 (`Stats updated: 09:46:46`). [Verified]
- **Reported, not changed — a run-varying `PUBLIC` define is costing direct-mode cache hits.**
  `CMakeLists.txt:280` (and `:336`) put `ANABASIS_BUILD_NUMBER="${ANABASIS_BUILD_NUMBER}"` on the
  target as `PUBLIC`, and CI passes the run number, so the value changes every run. ccache's direct
  mode hashes the command line, so every translation unit carrying that define misses direct mode
  and can only hit via preprocessed mode — which on this lane means running the preprocessor once
  per `-arch`. It matches what the logs show: 58 direct vs 116 preprocessed hits, with the direct
  hits concentrated in `AnabasisTests`, the one target that does not define it. The only consumer is
  `src/gui/PluginEditor.cpp:210`. Narrowing it to that source file would leave output bytes
  unchanged, but it is a Build System change under the Architecture Review Gate, so it is proposed
  rather than applied.
  Evidence: `CMakeLists.txt:74,280,336`; `src/gui/PluginEditor.cpp:116,210`; run 32565784751 stats.
  [Verified]
- **GCC 16 is now clean through the LTO link, not only the compile.** 0.2.3 had to record "compile
  phase measured at 16, link phase measured at 14" because the failing lane aborted before linking.
  Run **32568563583** — the first after the `libxi-dev` fix — completed it: `linux-lto-tests`
  succeeded, both links ran, the gate printed `no first-party warnings`, and both suites passed
  (301 + 873, 0 failures). The only two `warning:` lines in the job are the `lto-wrapper` LTRANS
  notices 0.2.3 had just pinned as non-diagnostics.
  Evidence: run 32568563583. [Verified]
- **Reported, deliberately not fixed — the arm64 slice of the shipped macOS bundle has no dSYM.**
  `dsymutil` emits `warning: (arm64) … No object file for requested architecture` and `warning: no
  debug symbols in executable (-arch arm64)` for VST3, AU and Standalone, because the retained
  `lto-objects/lto.o` holds only the x86_64 slice. The "Assert LTO ran and its objects were
  retained" step cannot see it: it gates on `COUNT -eq 0`, and the count is 1. This is a
  shipped-artifact symbolication contract and outside the scope of this round, so it is filed for
  the owner rather than changed here.
  Evidence: run 32568563583, `macos` job. [Verified]

---

## [0.2.3] — 2026-08-22

**The GCC 16 warning gate, measured — and the missing header that stopped it being measured.** The
`gcc:16` container lane shipped in 0.2.2 without ever having been run (ADR-0034 said so: no
container runtime here, the first push is the measurement). It ran, and it failed — not on a
warning but on `fatal error: X11/extensions/XInput2.h: No such file or directory`, in three JUCE
translation units. No DSP algorithm changed, no parameter was added, renamed or removed, no
serialization schema, threading model or reported latency moved, and **no first-party source file
was touched**. Measurements and reasoning:
[`worklogs/2026-08-22-gcc16-warning-gate-validation.md`](worklogs/2026-08-22-gcc16-warning-gate-validation.md).

### Fixed
- **`libxi-dev` is now an explicit dependency on both `setup-linux.sh` profiles.** JUCE 9.0.1
  defaults `JUCE_USE_XINPUT` to 1, so `juce_gui_basics.h:393` includes `<X11/extensions/XInput2.h>`
  unconditionally in practice; that header belongs to `libxi-dev`, which the package list never
  named. It reached the `full` profile transitively — `libgtk-3-dev` carries `Depends: libxi-dev` —
  which is why the Ubuntu runners and every developer machine compiled it anyway. The `headless`
  profile drops the gtk/webkit pair deliberately (nothing here compiles webkit) and took the
  X-input headers with it, killing the container lane at compile.
  Evidence: run 32565784751, job `linux-lto-tests`; `dpkg -S`, and the package present in the
  Debian trixie and Ubuntu noble archives alike. [Verified]

### Changed
- **The first-party warning gate no longer attributes its findings to Clang when GCC produced
  them.** `check-clang-warnings.py` takes `--compiler`, and the three lanes that call it pass the
  compiler they ran; the default is a neutral `the compiler` rather than a wrong name. What the
  gate accepts and rejects is unchanged.
  Evidence: `scripts/check-clang-warnings.py`, `.github/workflows/build.yml`. [Verified]
- **Three self-test cases pin the driver-level warning forms.** Every GCC LTO link emits
  `lto-wrapper: warning: using serial compilation of N LTRANS jobs`, which carries no source
  location and therefore cannot be attributed to a file; that it is not treated as a diagnostic is
  now asserted rather than incidental, alongside the `cc1plus:` and `ld:` forms. 15 → 18 cases.
  Evidence: `python3 scripts/check-clang-warnings.py --self-test`. [Verified]

### Notes
- **GCC 16 introduced no new diagnostics in this tree, and the zero-warning policy is unchanged.**
  All **13 first-party translation units** of the two suites compiled under **g++ 16.2.0** at
  `-O3 -flto -std=c++23` with the full `juce_recommended_warning_flags` set and none emitted a
  warning — the policy measured at 14.2.0 holds at 16 across two majors and a change of
  distribution. The failing run aborted before the LTO **link**, where `-Wodr` and
  `-Wlto-type-mismatch` fire; that phase is measured locally at GCC 14.2.0 over both suites in the
  same `-flto` configuration and is likewise clean of first-party diagnostics.
  Evidence: run 32565784751; ccache stderr replay verified in both directions, so the run's 43
  cache hits do not weaken the reading. [Verified]

---

## [0.2.2] — 2026-08-22

**The CI/toolchain parity round: an audit of the sibling's build pipeline area by area, and the
twelve places the previous migration had moved structure without moving configuration.** No DSP
algorithm changed, no parameter was added, renamed or removed, no serialization schema, threading
model or reported latency moved, and **no first-party source file was touched** — the change set is
CI, records and documentation. The patch bump is for one user-relevant fact and one contributor-
relevant one: the sanitizer gate the shipped code passes is materially deeper, and the compatibility
compiler moved two majors. Audit, verdicts and measurements:
[`worklogs/2026-08-22-ci-toolchain-parity-audit.md`](worklogs/2026-08-22-ci-toolchain-parity-audit.md).

### Changed

- **The `sanitizers` gate deepened from `address,undefined` to seven sub-checks, with leak detection
  on** ([ADR-0034](docs/architecture/design-decisions/ADR-0034-ci-toolchain-parity.md)). Added:
  `vptr`, `float-divide-by-zero`, `implicit-conversion`, `local-bounds`, `nullability`, behind a
  narrowly scoped ignorelist (one sub-check, one vendored tree). `ASAN_OPTIONS` gains
  `detect_leaks=1` and three tightened runtime checks — the previous `detect_leaks=0` rested on an
  assumption about JUCE's singletons that measurement retired. `unsigned-shift-base` is deliberately
  **not** adopted: measured, it fires once, on the dither RNG's xorshift, where the wrap is the
  algorithm and the shift is well defined. Both suites pass the new set: **300 + 873, 0 failures**.
  Evidence: ADR-0034 §Evidence. [Verified]
- **The compatibility compiler moved from `g++-14` (apt) to `g++-16` (the official `gcc:16`
  container)**, with the `headless` dependency profile that a containerised toolchain requires. No
  apt source ships a released g++-16, so 0.2.1's pin had chosen the version to fit the acquisition
  method. The LTO lane is now two jobs, because `container:` is a per-job key.
  Evidence: ADR-0034 §Decision; `.github/workflows/build.yml`. [Verified — except the container
  lane itself, which no local environment here can run]

### Added

- **The Windows toolset is recorded and its ABI series asserted.** `windows-latest` floats and MSVC
  is auto-detected, so until now a shipped `.vst3` could be built by a toolset no artifact named.
  The job prints the toolset, writes it to the run summary, and fails if the ABI series leaves
  `14.x` — which is what decides the Visual C++ redistributable a user needs.
  Evidence: `.github/workflows/build.yml` (`windows`). [Verified]
- **The macOS jobs are cached**, and every job gained a timeout and a printed record of the
  toolchain it resolved. The `dsymutil` objection that had kept macOS uncached was refuted, not
  outvoted: a cached object is a real `.o` at the path the linker recorded.
  Evidence: ADR-0034 §3. [Verified]

## [0.2.1] — 2026-08-22

**The sibling-alignment round: the Linux binary you install is now a Clang build, and the test
suites are finally run against the optimization class that ships.** No DSP algorithm changed, no
parameter was added, renamed or removed, no serialization schema, threading model or reported
latency moved, and the frozen parameter-registry snapshot needed no re-freeze. The patch bump is
for one user-visible fact — **which compiler produces the Linux artifact** — and the measured
consequence of that change is that the declared ABI floor is unchanged, so a system that could load
0.2.0 can load this. Investigation and measurements:
[`worklogs/2026-08-22-lto-lane-and-linux-toolchain-alignment.md`](worklogs/2026-08-22-lto-lane-and-linux-toolchain-alignment.md).

### Changed

- **The shipped Linux VST3 and Standalone are built by the pinned Clang**, not by whatever `g++`
  the CI image carried
  ([ADR-0032](docs/architecture/design-decisions/ADR-0032-linux-release-toolchain.md)). ADR-0031
  pinned the compiler that *gated diagnostics*; nothing pinned the one that produced the binary, so
  a runner-image bump could change the shipped codegen with no commit in this repository. The same
  move points the two instruments written for INC-004 — the bare-engine reproduction and the
  channel probe that HOSTS the bundle — at the build that ships rather than at a second Clang build
  that was discarded. **The ABI floor was re-measured on the new artifact and is unchanged**
  (GLIBC 2.38 / GLIBCXX 3.4.31 / CXXABI 1.3.9), so no supported system moves.
  Evidence: ADR-0032 §Evidence; `.github/workflows/build.yml`. [Verified]

### Added

- **`linux-lto-tests`: both suites are built with `-flto` and re-run against that codegen**, on the
  pinned Clang and on a pinned GCC
  ([ADR-0033](docs/architecture/design-decisions/ADR-0033-lto-validation-lane.md)). By ADR-0008 the
  suites do not link the LTO flags the plugin does — which is exactly the configuration INC-004
  needed in order to stay hidden behind five months of green console gates. The Clang arm closes
  that gap; the GCC arm keeps the second major toolchain compiling this tree on every push now that
  it no longer builds the artifact. Both arms gate first-party warnings at zero.
  Evidence: ADR-0033 §Evidence; `.github/workflows/build.yml`. [Verified]

## [0.2.0] — 2026-08-22

**The engineering-standard round: the Priority-1 realtime policy becomes a gate, the toolchain
stops moving under the build, and the shipped artifact's properties become measurements.** No DSP
algorithm changed, no parameter was added, renamed or removed, no serialization schema, threading
model or reported latency moved, and the frozen parameter-registry snapshot needed no re-freeze.
The minor bump is for the C++ baseline and the toolchain pin, both of which change what a
contributor needs installed. Executed from the migration audit in
[`worklogs/2026-08-21-anamorph-migration-audit-round-2.md`](worklogs/2026-08-21-anamorph-migration-audit-round-2.md);
the execution record is
[`worklogs/2026-08-22-migration-roadmap-execution.md`](worklogs/2026-08-22-migration-roadmap-execution.md).

### Added

- **`REALTIME_AUDIO_POLICY` is now enforced mechanically, in three tiers**
  ([ADR-0029](docs/architecture/design-decisions/ADR-0029-realtime-enforcement-strategy.md)). An
  allocation guard armed around the engine's audio entry point and compiled into the DSP suite (the
  tier that reaches Windows, where no sanitizer runs); a **RealtimeSanitizer** job behind a liveness
  canary; and a static lint over audio-path bodies for the branches neither runtime tier executes.
  The audit document had asked for the first of these since P2. Measured: **0 allocations over 2,040
  armed `process()` calls across 80 configurations**, and the DSP suite runs violation-free under
  RealtimeSanitizer.
  Evidence: ADR-0029 §Evidence; `tests/AllocationGuard.h`, `scripts/check-realtime.py`. [Verified]
- **The shipped Linux binaries have a declared, gated ABI floor** — `scripts/check-linux-abi.py`.
  Until now the compatibility document said no Linux OS floor "has been decided or measured"; the
  artifact had one all along and nothing reported it. Measured on this repository's own build:
  GLIBC 2.38, GLIBCXX 3.4.31, CXXABI 1.3.9. The run that raises it is now the run that fails.
  Evidence: `scripts/check-linux-abi.py`; `docs/architecture/COMPATIBILITY_MATRIX.md`. [Verified]
- **macOS crash symbolication actually works.** Under Release + LTO, ld64 deleted the object
  `dsymutil` needs, so the `-debug` artifact was never produced on the one platform whose users hand
  developers OS crash logs. `-Wl,-object_path_lto` retains it, and two assertions turn the
  best-effort capture into a contract.
  Evidence: `CMakeLists.txt` (Apple link options); `.github/workflows/build.yml` `macos` job. [Verified]
- **`scripts/preflight.sh`** — the lint gates and the suites in one local command, with no silent
  skips, and the citation gate run against all three bases including the push predecessor CI
  actually compares.
  Evidence: `scripts/preflight.sh`. [Verified]

### Changed

- **The language baseline moves C++20 → C++23**
  ([ADR-0030](docs/architecture/design-decisions/ADR-0030-cxx23-language-standard.md)), superseding
  ADR-0008 decision B5's standard and closing OQ-006. A C++23 compiler is now required to build:
  GCC 13+, Clang 17+, MSVC 19.35+, AppleClang 15+. Modules remain unused and C++23 *library*
  features remain admissible only behind feature-test macros. The `ANABASIS_CXX_STANDARD` seam and
  the weekly `cxx23-canary` workflow are removed — every job on all three platforms now compiles
  the baseline as a blocking check.
  Evidence: ADR-0030 §Evidence. [Verified]
- **The Linux CI Clang major is pinned**
  ([ADR-0031](docs/architecture/design-decisions/ADR-0031-clang-toolchain-pin.md)). The
  zero-first-party-warning gate had no stable reference point, and RealtimeSanitizer does not exist
  in the major Ubuntu's archives stop at. **No warning baseline file was introduced** — the gate
  stays at zero, and it holds at the pinned major with no source change.
  Evidence: ADR-0031 §Evidence. [Verified]
- **Every GitHub Actions ref is pinned to a commit SHA**, and Dependabot is split by semver impact.
  This reverses a decision `DEPENDENCY_POLICY.md` previously recorded; that policy section is
  rewritten with the argument that changed it rather than left contradicting the workflows.
  Evidence: `.github/dependabot.yml`; `docs/policies/DEPENDENCY_POLICY.md`. [Verified]
- **pluginval's crash-retry is scoped to Linux.** The retry exists for one documented X11/XEmbed
  host-side flake; on macOS it was giving a genuine intermittent crash three chances to disappear.
  Evidence: `scripts/run-pluginval.sh`. [Verified]

### Fixed

- **The documentation gate could report a clean run over an empty set.** `check-docs.py` tested its
  skip list against the ABSOLUTE path, so a checkout living anywhere under a directory named
  `build`, `JUCE` or `node_modules` excluded every file in the repository — and `main()` then printed
  `0 file(s) clean` and exited 0. Both halves are fixed and both are pinned by new self-test cases.
  No document's content changed; what changed is that the gate now reads them.
  Evidence: `scripts/check-docs.py`; `--self-test` (67 cases). [Verified]
- **The portability lint could be silently blinded** by a raw string, a prefixed character literal,
  a line splice inside a literal, or an unterminated literal or block comment — each of which
  desynchronises its scanner and turns findings below it into false negatives or wrong line numbers.
  No such construct exists in the tree today; the failure direction is what made it worth fixing.
  Evidence: `scripts/check-portability.py`; `--self-test` (120 cases). [Verified]
- **Published release notes could be truncated by a nested code fence.** The extractor recorded a
  fence opener as three characters and closed on a bare prefix, so a three-backtick line closed a
  four-backtick block, and a fence carrying an info string closed the block it was nested inside —
  after which the next `##` heading ended the notes early. The closer now follows CommonMark: same
  character, at least as long, nothing but whitespace after the run. Every existing entry extracts
  byte-identically; the fix is for the first entry that adds a nested sample.
  Evidence: `.github/workflows/release.yml`. [Verified]

---

## [0.1.6] — 2026-08-21

**Two field reports: the GR history and the percent value boxes.** The gain-reduction history was
under-reporting the plug-in's own work — its trace stopped at half the panel height and went flat
for anything deeper, while the limiter's own GR meter beside it read further — and a fraction typed
into a percent box landed a hundredth of where it was meant to. No DSP change, no parameter added,
renamed or removed, no change to the serialization schema, and no change to reported latency.

### Fixed

- **The GR history trace uses the whole panel again, at the same 24 dB span as the GR meters.**
  It divided by 12 dB and then spent only half the panel height getting there, so every reduction
  past 12 dB drew the same horizontal line across the middle of the graph — the display's ceiling,
  read as the limiter's. Everything that was already visible is drawn at exactly the height it was
  before (12 dB still lands at half the panel); what changed is the reduction that used to be
  flattened out of sight. The trace and the per-stage GR meters now read one shared span, so the
  two cannot drift apart again. Evidence: this release. [Verified]

### Changed

- **A percent box reads a bare number between 0 and 1 as a fraction: `0.5` is 50 %, `1` is 100 %.**
  Typed into a 0–100 % control, `0.5` used to land on half of ONE percent — a value
  indistinguishable from zero at the knob, in the readout and in the sound. A number carrying an
  explicit `%` is the literal percent, which is how the sub-1 % region is reached: `0.1%` is a
  tenth of a percent, and `1 %` is one percent. This affects the seven percent controls (Loudness,
  Comp Mix, Comp Stereo Link, Clip Mix, Color Depth, Limiter Stereo Link, Transients); no
  parameter range, default, ID or automation behaviour changed. Evidence: this release. [Verified]
- **A percent box holding a fractional value shows its decimal** — `0.1 %` where it used to round
  to `0 %`. Whole percents print exactly as before (`50 %`). Without it the box denied the value it
  had just accepted, and any path that re-read its own displayed text — the round-trip a host
  performs — turned that value back into zero. Evidence: this release. [Verified]

---

## [0.1.5] — 2026-08-16

**The framework bump.** Anabasis moves from JUCE 9.0.0 to **JUCE 9.0.1**. Nothing in this
repository's own DSP, parameter surface, state format or GUI code changed to accommodate it, and
nothing needed to: the audio-path modules are byte-identical between the two upstream tags. What
the release carries is upstream's own patch-release fixes, three of which are on Linux and land on
exactly the surface a standing field report lives on.
([**ADR-0028**](docs/architecture/design-decisions/ADR-0028-juce-901-pin.md) — Accepted
2026-08-16; a JUCE pin change is an Architecture Review Gate item and the owner directed it.)

### Changed

- **The plug-in is now built against JUCE 9.0.1** (commit
  `e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8`), replacing 9.0.0 (`f8f8864…`). **No audible change is
  expected or intended, and this is measured rather than asserted**: `juce_dsp`,
  `juce_audio_basics`, `juce_audio_processors` and the VST3/AU wrapper sources are identical
  between the two upstream tags apart from a version string, and the shipped bundle's per-channel
  output over 33 sample-rate / block-size / parameter configurations is the same to nine decimal
  places built either way. Reported latency, the parameter surface and the session format are
  unchanged. Evidence: this release. [Verified]
- **On Linux, three upstream defects that could leave the editor unresponsive are fixed.** JUCE
  9.0.0 loaded the X11 input extension by a filename that only exists on machines with development
  packages installed, so on an ordinary end-user machine it silently registered for no pointer
  events at all; it also restarted the redraw timer on every display query because the guard
  compared a millisecond period against a refresh rate in hertz, and let a busy message queue
  starve the X event pump. All three are upstream fixes carried by the bump, not changes made here.
  **This does not close the outstanding report** ([KI-012](docs/KNOWN_ISSUES.md)) — it has never
  reproduced in this repository, so nothing here could test the fix against the configuration that
  shows it — but each of the three was a live candidate at 9.0.0 and none is at 9.0.1.
  Evidence: this release. [Verified]

### Fixed

- **A hand-edited or foreign session file containing an XML processing instruction inside element
  text now loads** instead of being cut short at that point. No file this plug-in writes has ever
  contained one, so no session, preset or A/B slot saved from Anabasis is read differently than
  before — this widens what the reader tolerates and narrows nothing. Carried by the JUCE bump.
  Evidence: this release. [Verified]

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
  per-operation. `--help` prints the two options, and three things are refused rather than
  guessed at: an unrecognised option, the two flags given together (they differ in destination
  *and* in privilege, so there is no intent to infer — less than there is behind a typo), and
  `--user` under `sudo`, because which home directory `$HOME` names there depends on the
  machine's sudoers configuration. Repeating one option is not a conflict and passes. **`uninstall.sh`
  takes the same two flags**, which is the half that would otherwise have been left in the worse
  place: a script that can INSTALL system-wide without a terminal but can only REMOVE per-user.
  Evidence: this release. [Verified]
- **The macOS package verifies itself at build time.** The build now fails rather than shipping a
  package whose components are relocatable, version-checked, missing the overwrite action or
  missing their installed-state check — and it first proves those assertions can actually fire, by
  packaging one payload twice and confirming each state appears with the installer defaults and
  disappears when its key is switched off. A count check precedes the per-component loop so an
  empty search cannot pass every assertion by running none of them. Evidence: this release. [Verified]

### Fixed
- **A successful per-user Linux install could report failure and print nothing.** The last step of
  the transaction discards the copy it set aside, and on the per-user branch that `rm -rf` was the
  one command after the point of no return with no failure tolerance — so under `set -e` an
  unremovable parked bundle (an immutable attribute, a read-only remount, a mount point inside it)
  aborted the script *after* both files were correctly in place but *before* the traps were
  cleared: exit 1, the rollback handler fired, and none of the confirmation, the PATH note or the
  duplicate-install warning was printed. Reproduced end to end. The system-wide branch had already
  guarded its copy of that line; both now also SAY what they could not remove instead of
  discarding the message with the exit status, because what survives is a full plug-in bundle
  sitting in the user's tree. Evidence: this release. [Verified]
- **The Linux uninstaller deleted the one copy of your plug-in that only the installer could
  restore.** An install interrupted in the two-rename window parks the working bundle as
  `Anabasis.vst3.prev` inside the installer's scratch directory; `install.sh` puts it back, and
  nothing else does. `uninstall.sh` swept that directory away with the rest of the scratch —
  printing a note first that named exactly what was being lost, which made it worse rather than
  better: the script established that it knew the copy was valuable and destroyed it in the same
  breath, and the user this reaches is the one whose plug-in has just vanished mid-install and who
  reaches for the uninstaller to tidy up. The copy is now KEPT and named, with the command that
  restores it; `--discard-parked` deletes it for anyone who wants the directory gone. Evidence:
  this release. [Verified]
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
