# Anabasis — Mastering Loudness Maximizer

**Anabasis** (by **RollyTech**) is a stereo mastering loudness maximizer: one large "Push" knob
driving an adaptive chain of compression, clipping/saturation and true-peak limiting, with
loudness-compensated monitoring so *louder* and *better* can be told apart honestly. It is the
second product in the RollyTech line, alongside **Anamorph** (stereo width / stereo-field
expander), and shares that product's frame layout, brand system and engineering standard.

Built with **CMake + JUCE** only — it configures and builds entirely from the command line on a
headless Linux machine, no IDE.

## Project status

- **Version 0.1.0 (pre-release), phase P1 — skeleton.** P0 closed on 2026-07-31 with the owner's
  sign-off of [`docs/DESIGN.md`](docs/DESIGN.md); the eleven ADRs it authorised are Accepted and
  registered in
  [`docs/architecture/design-decisions/ADR_INDEX.md`](docs/architecture/design-decisions/ADR_INDEX.md).
- **The P1 skeleton is in the tree**: `CMakeLists.txt` (ADR-0008's five-target graph), `src/`
  (the 49-parameter surface, the POD engine boundary, a pass-through chain with a basic
  lookahead limiter on the constant 10 ms allowance, schema-v1 state) and `tests/` (223 checks,
  green on Linux together with pluginval L5 in both modes ×3).
- **Decided and frozen from the first build:** the JUCE pin (**9.0.0** at commit `f8f8864…`, the
  same revision Anamorph pins) and the plugin identity (**`RTec` / `Anbs` /
  `com.rollytech.anabasis`**) — both now written into `CMakeLists.txt` as ADR-0008 specifies.
- **Still open** — the JUCE licence tier (OQ-002, blocks distribution not development), the frozen-trim-vector transport
  (OQ-013, blocks that one restore path at P1 and nothing else), and **the remaining entries in
  [`docs/OPEN_QUESTIONS.md`](docs/OPEN_QUESTIONS.md)** — which is the list of record, so this
  sentence cannot drift out of date. None of them may be guessed at. Resolved entries
  stay in that file's `Resolved` section; they are decisions, not choices to revisit.

## Planned scope (see `docs/DEVELOPMENT_BRIEF.md` for the full specification)

- **Signal chain (fixed for v1):** Input Gain → EQ (pre by default) → Compressor →
  Clipper + Saturation → Limiter (lookahead + true peak) → Ceiling → Dither → Output.
- **Simple mode:** one large Loudness/Push knob plus Ceiling, Character and Tone, driven by an
  adaptive engine (short-term LUFS, crest factor, spectral tilt, transient density).
- **Advanced mode:** per-stage control over the same parameter model — switching modes must not
  change the sound.
- **Metering:** LUFS (momentary / short-term / integrated, BS.1770-4 gated), true peak (dBTP),
  PLR, gain-reduction history, input/output spectrum, streaming-target lines with a
  loudness-penalty estimate.
- **Formats:** VST3 (all platforms), **AU** (macOS, for Logic Pro), Standalone (debugging).
  **AAX is not supported.**
- **Platforms:** Linux x86-64 (headless CI build), Windows x86-64, macOS universal (arm64 + x86_64).

## Requirements

- **CMake ≥ 3.22**, a **C++20** compiler, **Ninja** (recommended).
- **JUCE 9.0.0**, fetched automatically by CMake `FetchContent` and pinned to that tag's
  **immutable commit SHA** `f8f8864172464b9adf9eba6101e1f784838d1597` — the same revision the
  sibling product Anamorph pins, so both plugins share one framework baseline
  (`docs/OPEN_QUESTIONS.md` OQ-001, resolved). `docs/policies/DEPENDENCY_POLICY.md` carries the
  version-lock reasoning; changing the pin is an Architecture Review Gate item.
- Linux build deps install via `scripts/setup-linux.sh`. See `docs/procedures/BUILD.md`.

## Quick start (headless Linux) — active from P1 onward

```bash
# 1. Install build dependencies (Ubuntu; safe to re-run)
scripts/setup-linux.sh

# 2. Configure + build (fetches the pinned JUCE commit via CMake FetchContent)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release        # ...or: scripts/build.sh

# 3. Run the headless self-tests (DSP + state compatibility)
scripts/run-tests.sh

# 4. Validate the VST3 with pluginval — strictness escalates by phase (5 → 8 → 10)
scripts/run-pluginval.sh 5 deterministic
scripts/run-pluginval.sh 5 randomise
```

To build without network (JUCE already on disk):
`cmake -B build -DANABASIS_JUCE_PATH=/path/to/JUCE ...`

## Documentation

The full technical documentation lives in **[`docs/`](docs/)**:

- **Start here:** [`docs/DEVELOPMENT_BRIEF.md`](docs/DEVELOPMENT_BRIEF.md) (the product spec) ·
  [`docs/architecture/design-decisions/ADR_INDEX.md`](docs/architecture/design-decisions/ADR_INDEX.md)
  (**the binding decisions** — read before writing code) ·
  [`docs/DESIGN.md`](docs/DESIGN.md) (the signed-off P0 design: architecture, the 49-parameter
  table, macro curves, wireframes) ·
  [`docs/SOURCE_OF_TRUTH.md`](docs/SOURCE_OF_TRUTH.md) ·
  [`docs/REPOSITORY_MAP.md`](docs/REPOSITORY_MAP.md) ·
  [`docs/OPEN_QUESTIONS.md`](docs/OPEN_QUESTIONS.md)
- **Rules (binding):** [`docs/policies/`](docs/policies/) — real-time audio, threading, DSP,
  mode/adaptation, compatibility, AI-agent, testing, release, dependency, code style
- **Architecture & decisions:** [`docs/architecture/`](docs/architecture/) — **eleven Accepted
  ADRs** (ADR-0001…0011, all dated 2026-07-31); the descriptive architecture set lands with P1–P2.
  `DESIGN.md` ranks *below* the ADRs it spawned and loses to them on any disagreement — see
  `docs/SOURCE_OF_TRUTH.md` §"Where `DESIGN.md` sits"
- **How-to:** [`docs/procedures/`](docs/procedures/) (build, development, CI/CD, testing, release)
- **History & status:** [`CHANGELOG.md`](CHANGELOG.md) ·
  [`docs/HANDOVER.md`](docs/HANDOVER.md) · [`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md) ·
  [`docs/FUTURE_RISKS.md`](docs/FUTURE_RISKS.md) · [`docs/POSTMORTEMS.md`](docs/POSTMORTEMS.md)

The documentation falls into **four classes**, kept deliberately separate — Developer, User
(`docs/user/`), Internal/testing, Legal. `worklogs/` sits outside all four: session-local
investigation records that are never cited as policy. See `docs/SOURCE_OF_TRUTH.md`.

## Validation gate

| Level | What | Status |
|---|---|---|
| 1 | Compiler warnings, CodeQL, MSVC `/analyze` | scaffolded, activates at P1 |
| 2–3 | Headless DSP + state-compatibility self-tests | activates at P1 |
| 4 | pluginval — both modes ×3, blocking on all three platforms; strictness 5 → 8 → **10** by phase | activates at P1 |
| 5 | Manual audition in a DAW | required for release sign-off; not headlessly reproducible |

A green build + pluginval pass is **"ready to audition," not final sign-off**
(`docs/policies/TESTING_POLICY.md`).

## Relationship to Anamorph

Anamorph is a **read-only reference** for this project. Anabasis inherits its governance system,
CI/CD shape, testing conventions, build structure and brand system, and may copy and adapt
first-party code from it. Anabasis **never modifies the Anamorph repository**. What is
deliberately shared and what deliberately differs is tabulated in
`docs/DEVELOPMENT_BRIEF.md` §23.

## Licensing

**Anabasis is a closed-source commercial product — it is not open-source software.** The source
being readable here is not a licence: no `LICENSE` file is present, and this repository grants no
right to use, copy, modify or redistribute the code or binaries beyond what written permission
from RollyTech provides (all rights reserved by default).

Anabasis is built on **JUCE**, whose modules are dual-licensed AGPLv3 or commercial; a
closed-source distribution model cannot use the AGPLv3 arm, so the **commercial JUCE tier** must
be in place before commercial distribution. Commercial VST3 distribution requires reviewing
Steinberg's licensing requirements separately. The licence tier this project ships under is an
open decision — `docs/OPEN_QUESTIONS.md` OQ-002.

Contributors and AI agents: read **[`CLAUDE.md`](CLAUDE.md)** and
`docs/policies/AI_AGENT_POLICY.md` before changing code — some changes (parameter IDs,
serialization, threading, DSP order, latency) are hard-stop, human-review-required.
