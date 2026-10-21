# ADR-0047 — Upgrade JUCE 9.0.1 to 9.0.4 and preserve the compiled codec set

**Status:** Accepted — on the owner's 2026-10-21 directive to safely upgrade to JUCE 9.0.4,
resolve resulting issues, synchronize documentation and create a PR. This authorizes the
Build System change under `ARCHITECTURE_REVIEW_GATE.md`; it is not release sign-off or
authorization to merge. Manual audition remains owed under `DEPENDENCY_POLICY.md` rule 2.

## Context

Anabasis pins JUCE 9.0.1 at `e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8` (ADR-0028).
The requested 9.0.4 tag resolves to `3fb5cbc0340758596dac39f7c8de47d562b4896e`, corroborated
by `git ls-remote`, a clean detached checkout, JUCE's project version and module declarations.
The read-only Anamorph reference already pins that commit.

## Problem

A default-only bump leaves existing CMake caches on 9.0.1. JUCE 9.0.2 also enables its MP3
decoder by default, and 9.0.3 adds enabled-by-default Opus and WebP codecs. None is used by
Anabasis; accepting those defaults would expand the compiled component set unnecessarily.
The intervening releases also change XML formatting, message locks, OpenGL, platform windows
and some plugin wrappers, requiring source and runtime checks rather than an API-only build.

## Options

- **Retain 9.0.1:** rejects the requested framework update.
- **Change defaults only:** rejected; existing build caches silently retain the old dependency,
  and fresh builds compile extra codecs.
- **Force cache values:** rejected; would silently replace deliberate custom dependency pins.
- **Pin the immutable 9.0.4 commit, reject previous default caches and explicitly exclude the
  unused codecs:** chosen. Retains custom revisions and the local-checkout escape hatch.

## Decision

1. Set `ANABASIS_JUCE_VERSION` to `9.0.4` and `ANABASIS_JUCE_TAG` to the full commit above.
   FetchContent, `GIT_SHALLOW` and `ANABASIS_JUCE_PATH` retain ADR-0008's acquisition contract.
2. Before FetchContent, reject either cached field from the previous Anabasis defaults
   (9.0.0/9.0.1), including mixed or partially reset pairs. Print the two-field `-U` reset
   command with quoted source/build paths. Preserve custom pins. A local JUCE path permits an
   intentional old/new comparison without this guard.
3. Set `JUCE_USE_MP3AUDIOFORMAT=0`, `JUCE_USE_OPUS=0` and `JUCE_USE_WEBP=0` through one shared
   definition list on all six consumers: plugin, DSP/state suites, benchmark, channel probe
   and engine reproduction. This also propagates to the generated plugin-format targets.
   FLAC/Ogg Vorbis, desktop OpenGL and all other existing definitions retain their behavior.
4. Add a dependency-boundary CMake regression test to the Linux CI job. Its local stand-in
   fails deliberately after recording the selected pin; reaching it with a stale default is
   a test failure. Cases include reconfiguration, both historical defaults, one-field resets,
   mixed pairs, custom pins, local comparisons and paths containing spaces.
5. Re-verify the third-party inventory against `JUCE.spdx.json`, component licences and build
   objects. Update the relocated Ogg/Vorbis paths. Record disabled new components separately
   from shipped attribution. `juceaide` is a build helper with its own defaults, not a shipped
   Anabasis consumer.
6. Re-check the dependency-internals register and refresh its source citations. No first-party
   DSP algorithm, parameter, schema, latency, thread model or UI wording is changed.
7. Keep application version **0.2.17**. Record the framework change under Unreleased; create no
   tag or release. Submit the PR with execution evidence and remaining manual checks disclosed.

This supersedes ADR-0028's current pin, and amends ADR-0008's pin and compile-definition
contract only. The immutable acquisition mechanism, target graph, identity and C++23 baseline
(ADR-0030) remain in force. `DEPENDENCY_POLICY.md`'s current row and compliance log implement
this decision without weakening any upgrade requirement.
`TESTING_POLICY.md`'s Linux gate gains the cache regression from decision 4; all existing
runtime, platform and manual validation requirements remain in force.

## Consequences

Old default caches now stop with an actionable error instead of building an older framework.
Deliberate old-pin comparisons use the documented local-checkout path. Anabasis and Anamorph
again use the same framework commit, with no changes to the sibling repository.

Linux measurements cannot establish macOS AU/native GL behavior, Windows behavior or listening
quality. Existing platform CI and Level-5 audition remain required. In particular, the
pre-existing KI-028 Intel AU/pluginval failure is not declared fixed by this bump.

## Related code

- `CMakeLists.txt`: pin, stale-cache guard and shared codec exclusions.
- `tests/juce_cache_tests.py`; `.github/workflows/build.yml`: regression and invocation.
- `src/dsp/Latency.h`, `src/dsp/RealtimeAnnotations.h`, `src/gui/LookAndFeel.h`, `src/gui/PluginEditor.cpp`,
  `src/gui/SpectrumView.h`: refreshed dependency commentary only.

Evidence [Verified for pin/source audit; runtime evidence and limits recorded separately]:
- `CMakeLists.txt`, the 9.0.1 → 9.0.4 upstream diff and `tests/juce_cache_tests.py`.
- `worklogs/2026-10-21-juce-904-upgrade.md`: build, comparison, compatibility and attribution
  evidence, including unperformed checks.
