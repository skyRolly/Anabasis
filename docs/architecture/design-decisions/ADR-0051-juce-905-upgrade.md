# ADR-0051 — Propose JUCE 9.0.4 → 9.0.5

**Status:** Proposed — 2026-11-04 (Asia/Shanghai). **Pending Human Architecture Review;
implementation has not begun.** The assignment selects JUCE 9.0.5 and permits investigation
and separate PR preparation subject to governance; it explicitly does not waive architecture
approval. Accepted ADR-0047 remains the current dependency decision.

## Context

Verified remote main: `3eeaef2872cf1dbaecaa1bb9ee7b3fb3f03d8504`; initially clean working tree,
no open PRs before the assignment. Application version **0.2.17** stays unchanged.

| Identity | Version | Immutable commit |
|---|---|---|
| Current, Accepted ADR-0047 | 9.0.4 | `3fb5cbc0340758596dac39f7c8de47d562b4896e` |
| Proposed target | 9.0.5 | `d60b7a78030661574a615b586d9325ba7a7fa7dd` |

Official lightweight tags, clean detached source trees, project/module versions and the
2026-11-02 official release corroborate the target. Evidence is retained independently in
[`worklogs/2026-11-04-juce-905-review.md`](../../../worklogs/2026-11-04-juce-905-review.md)
and its [ledger](../../../worklogs/juce905-review/evidence.json).

## Problem and architecture gate

Upgrade without altering the serial graph, true-peak ceiling, macro semantics, state/parameter
contracts, real-time safety, latency, identity, supported platforms or compiled codec set.
Default CACHE values alone do not migrate existing 9.0.4 builds. New parser/API and GUI changes
require analysis beyond the release number.

The gate names **“Build System change — CMake structure, JUCE version/pin, C++ standard baseline,
dependency set”** and states **“Detecting any gated change is an AI Agent Hard Stop — the agent
stops and requests human review rather than proceeding.”** Required approval is a human reviewer
with DSP/audio context approving this proposal's implementation scope under
[`ARCHITECTURE_REVIEW_GATE.md`](../../policies/ARCHITECTURE_REVIEW_GATE.md), Procedure 2–3.
ADR-0047's earlier approval is not approval of this proposal.

## Options

1. Retain 9.0.4: safe baseline/fallback if approval or validation fails; does not meet the target.
2. Fetch mutable tag/branch: rejected; violates ADR-0008's immutable dependency contract.
3. Only replace defaults or force cached values: rejected; silently retains old dependencies or
   overwrites deliberate custom pins.
4. Preserve the existing FetchContent/local-path model and codec definition list, extend stale
   default rejection, and pin the exact target commit: **proposed**. Keep `GIT_SHALLOW` unless a
   supported configuration demonstrates a failure requiring a separately reviewed correction.

## Proposed decision and implementation scope

- After approval, update `ANABASIS_JUCE_VERSION` and `ANABASIS_JUCE_TAG` to the target pair.
- Add both 9.0.4 fields to the top-level cache rejection, retaining 9.0.0/9.0.1 protection,
  two-field reset instructions, custom pins and `ANABASIS_JUCE_PATH`. Update the existing
  dependency-boundary regression for old/reused/partial/mixed caches and intentional local
  comparisons. Verify the actual source HEAD consumed by fresh builds.
- Preserve `ANABASIS_JUCE_CODEC_DEFINITIONS`: **`JUCE_USE_MP3AUDIOFORMAT=0`, `JUCE_USE_OPUS=0`,
  `JUCE_USE_WEBP=0`** on plugin, both suites, benchmark, channel probe and engine reproduction,
  including propagation to generated formats. Source inclusion is present on all six consumers;
  generated commands/objects remain an implementation verification requirement.
- Preserve CMake ≥3.22, C++23, existing toolchain/FP/ISA/LTO settings and target architecture.
  No new library, SDK, plugin format, supported platform, shared module or cross-repository
  production modification is proposed.
- Preserve identity, DSP order, ceiling/latency/oversampling contracts, Simple/Advanced neutrality,
  macro/adaptation mappings, parameter IDs/ranges/defaults, serialization and real-time behavior.
  Any demonstrated JUCE-induced source correction must be separately identified and reviewed;
  none is implemented or demonstrated necessary by this source-only review.
- When implementation is approved, synchronize current dependency/test/build/CI/architecture
  records, live JUCE citations, `NOTICE`, `THIRD_PARTY_LICENSES.md` and the changelog according to
  its evidence policy. Keep historical 9.0.4 records and actual current attributions intact until
  the pin changes. Do not change a product version, tag, release or published asset.

## Upstream compatibility and DSP risks

- `juce_dsp`, `juce_audio_basics`, `juce_events` and `juce_opengl` contain only module-version
  changes. Oversampling/filter maths and MessageManagerLock implementation are unchanged.
  APVTS is byte-identical. This supports investigation; it does not prove numerical equivalence.
- XML hardening is **directly applicable** to `getXmlFromBinary`/`ValueTree::fromXml` and
  `PresetManager::loadPreset`'s file parser. Generated schema-v1 trees are shallow and do not
  require external DTDs. Target limits reject pathological/deep/entity-dependent documents;
  verify legitimate presets/session fixtures rather than weakening those limits or claiming
  that every upstream fix was exploitable in Anabasis.
- Binary ValueTree/var readers and JSON limits are not used by the first-party persistence
  paths. ZIP/OSC/JavaScript changes have no identified product input path. Image/audio parsers
  require transitive reachability evidence; linking a module is not an external-input finding.
- Windows VST3 DPI handling and shared native window association, mouse dispatch, PopupMenu
  transparency, tooltips, Direct2D and macOS repainting change. Re-walk the dependency-internals
  register: parented preset menus still bind the default look-and-feel before parent insertion,
  append above the shield and enter modal state; completion still owns/deletes its window.
  The GL render-frame message try-lock still spans component painting. Native interaction and
  OpenGL teardown must be auditioned; source checks do not close those gates.
- HarfBuzz 14.2.1 → 15.0.0 is shipped text-shaping code. Reviewed licence texts are unchanged;
  verify compiled components/text layout on the actual target build. Codec exclusions and
  commercial JUCE/product-distribution decisions stay in force.
- Projucer deployment defaults and Android requirements do not control this CMake desktop build.
  Xcode 27's inability to target the existing older macOS floor must be escalated if encountered;
  changing that floor/toolchain is outside this proposal.

## Test requirements and observed evidence

Existing six cache regressions and source-policy checker controls run. An isolated acquisition
probe using the target SHA and `GIT_SHALLOW` verifies exact git HEAD with CMake 3.22.6 and 3.31.6. Both
9.0.4 and 9.0.5 product configurations fail building juceaide on missing `ft2build.h`; no DSP,
state, channel-probe or pluginval success is inferred. Full shipping CI evidence currently
belongs to 9.0.4, not to the proposed target.

Acceptance requires fresh/reused/local cache and actual-source provenance checks; full DSP and
state suites at both pins from identical first-party source/settings; the existing discriminating
33-scenario channel probe against the LTO VST3 plus engine reproduction; the retained cross-version
session driver with byte-identical re-save, parameter/metadata equality and six render hashes/
latencies. Retain Class-A identities and explicitly permitted tolerances without re-freezing
snapshots or widening tolerances. Include rate/buffer/OS/phase/Force-Max, ceiling automation,
bypass/Mix, resets, stereo/mono input, non-finite recovery and metering.

Run the existing Linux/Windows/macOS Apple Silicon/native Intel matrix, VST3/AU where applicable,
Standalone where enabled, pluginval both modes ×3 at the workflow strictness, ASan/UBSan,
realtime/memcheck, LTO/warning/ABI/source/docs/analysis/dependency and packaging/symbol gates.
Anabasis has no production TSan job or fuzz target: do not invent such a CI pass; any additional
experiment must be reported with its scope. Existing KI-003/024/026/028 remain dated limitations.

**Pending Human Acceptance:** Level-5 loudness-matched blind listening across ≥5 genres;
Reaper/Windows and Logic Pro/macOS AU load, automation, offline render and state restore;
latency/PDC, native editor/menu/scaling/accessibility/OpenGL and applicable installation checks.
The worklog contains the exact worksheet. Old approvals do not accept JUCE 9.0.5.

## Consequences and rollback

This proposal changes no current dependency or production behavior. A human may approve the
scoped implementation independently of manual acceptance, integration and release decisions.
Only after approval/implementation will this amend ADR-0047's current pin and ADR-0008's pin
reference; the remaining Accepted contracts retain their force.

If a compatibility/ceiling/latency/macro contract differs, stop the affected implementation at
its gate and preserve evidence. Before integration, withdraw or revert the branch implementation.
After a future integration, revert this product's upgrade commit(s), restore the 9.0.4 pin and
matching guards/tests/docs, reset both cached fields, verify the old source HEAD and rebuild/
revalidate. Do not roll release tags/assets or touch the sibling's source.

## Related code; evidence and confidence

`CMakeLists.txt`, `tests/juce_cache_tests.py`, `src/PresetManager.cpp`, `src/PluginProcessor.cpp`,
`src/dsp/Latency.h`, `src/gui/PluginEditor.cpp`, `src/gui/LookAndFeel.h`, `tools/channel_probe.cpp`,
`.github/workflows/build.yml`; Accepted ADR-0002/0003/0004/0005/0006/0007/0008/0009/0011/0014/
0028/0030–0037/0041–0048. **Verified:** remote/tag identities, source/attribution comparisons,
definition sites and recorded tool exits. **Unverified:** target numerical/state/host equality,
full native target matrix and human acceptance. Anamorph is a read-only reference from this
repository; its independently owned ADR-0071 is a sibling proposal, not this ADR's authority.
