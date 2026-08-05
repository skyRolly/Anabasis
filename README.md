# Anabasis — Mastering Loudness Maximizer

**Anabasis** (by **RollyTech**) is a stereo mastering loudness maximizer: one large "Push" knob
driving an adaptive chain of compression, clipping/saturation and true-peak limiting, with
loudness-compensated monitoring so *louder* and *better* can be told apart honestly. It is the
second product in the RollyTech line, alongside **Anamorph** (stereo width / stereo-field
expander), and shares that product's frame layout, brand system and engineering standard.

Built with **CMake + JUCE** only — it configures and builds entirely from the command line on a
headless Linux machine, no IDE.

## Project status

- **Version 0.1.0 (pre-release) — CODE COMPLETE** (2026-08-02, under the owner's blanket
  approval; what remains is the post-v0.1.0 human fine review — the brand pass, the DAW matrix
  audition and the listening pass over every constant marked ⊕). P0 closed on 2026-07-31 with the
  owner's sign-off of [`docs/DESIGN.md`](docs/DESIGN.md); the Accepted ADRs registered in
  [`docs/architecture/design-decisions/ADR_INDEX.md`](docs/architecture/design-decisions/ADR_INDEX.md)
  govern the implementation. **No count is written here on purpose** — the same rule the pluginval
  strictness follows, and for the same reason: this file is the first one a contributor opens, so
  it is the copy whose staleness costs most. Two numerals in this document disagreed with each
  other for exactly one round; the index is the registry.
  [`docs/HANDOVER.md`](docs/HANDOVER.md) carries the status of record.
- **In the tree**: `CMakeLists.txt` (ADR-0008's five-target graph) and `src/` — the 49-parameter
  surface, the full §2 processing chain (EQ · glue compressor · ADAA clipper/saturation ·
  true-peak lookahead limiter · oversampling to 16× · dither) behind the POD engine boundary on
  the constant 10 ms allowance, the §2.8 click-free transition layer, BS.1770-4 metering with the
  §2.7 loudness-compensated monitor, the §5.4 adaptive engine with Learn **including the ADR-0014
  frozen-trim restore**, the P5 editor (Simple and Advanced views, meters, spectrum, curve
  display) and the P6 per-slot undo / 13-preset factory bank (Default + 12) / performance bench — verified by
  `tests/` (**645 checks** — re-count from the suites' own output when editing, the same rule
  HANDOVER's status row carries — green on Linux, together with pluginval at the strictness
  `.github/workflows/build.yml` sets, in both modes ×3; see
  [`docs/TEST_REPORT.md`](docs/TEST_REPORT.md) for the measured numbers and
  `docs/policies/TESTING_POLICY.md` for the gate, which is deliberately stated in one place).
- **Decided and frozen from the first build:** the JUCE pin (**9.0.0** at commit `f8f8864…`, the
  same revision Anamorph pins) and the plugin identity (**`RTec` / `Anbs` /
  `com.rollytech.anabasis`**) — both now written into `CMakeLists.txt` as ADR-0008 specifies.
- **Still open** — the JUCE licence tier (OQ-002, blocks distribution not development) and
  **the remaining entries in
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
  PLR, gain-reduction history, input/output spectrum. (The brief's streaming-target lines were
  removed by owner directive 2026-08-05 — platforms normalise; a master is pushed against the
  ceiling, not a platform figure.)
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

# 4. Validate the VST3 with pluginval, at the strictness CI ENFORCES.
#    Read out of the one place that holds it rather than pasted: the literal
#    here was `5`, and stayed 5 through two raises, under a comment claiming it
#    was current — so the front page told contributors to validate at a weaker
#    bar than the build actually requires. `docs/procedures/CI_CD.md` carries
#    this same snippet with the full reasoning (POSIX `sed`, not `grep -oP`,
#    which BSD grep rejects; the `^  ` anchor pins the match to the `env:`
#    assignment so the job steps' references cannot supply a second value).
STRICTNESS=$(sed -n 's/^  ANABASIS_PLUGINVAL_STRICTNESS:[[:space:]]*\([0-9][0-9]*\).*/\1/p' \
             .github/workflows/build.yml)
: "${STRICTNESS:?could not read ANABASIS_PLUGINVAL_STRICTNESS from build.yml}"
scripts/run-pluginval.sh "$STRICTNESS" deterministic
scripts/run-pluginval.sh "$STRICTNESS" randomise
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
- **Architecture & decisions:** [`docs/architecture/`](docs/architecture/) — the Accepted ADRs,
  enumerated in
  [`ADR_INDEX.md`](docs/architecture/design-decisions/ADR_INDEX.md) and **nowhere else**. This
  bullet used to spell the list out ("twelve Accepted ADRs: ADR-0001…0011 plus ADR-0012") and went
  stale the moment ADR-0013 and ADR-0014 were accepted, contradicting the status section of this
  same file two screens up — in the one document that tells a contributor which decisions bind
  them. The index is the registry; this line points at it.
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
| 4 | pluginval — both modes ×3, blocking on all three platforms, editor under `xvfb`; strictness is `ANABASIS_PLUGINVAL_STRICTNESS` in `.github/workflows/build.yml`, which carries the phase ladder and is the only copy | activates at P1 |
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

Third-party attribution: [`NOTICE`](NOTICE) carries the notices that must accompany a binary
distribution, and [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) is the complete verified
inventory (component, purpose, licence, and the exact file each licence was read from). CI
copies both into every customer artifact (`docs/policies/RELEASE_POLICY.md` §"Third-party
attribution").

Contributors and AI agents: read **[`CLAUDE.md`](CLAUDE.md)** and
`docs/policies/AI_AGENT_POLICY.md` before changing code — some changes (parameter IDs,
serialization, threading, DSP order, latency) are hard-stop, human-review-required.
