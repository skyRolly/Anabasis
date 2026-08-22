# COMPATIBILITY_MATRIX.md

What Anabasis is claimed to run on, at what confidence, with the evidence per claim — and,
for the rows no headless gate can prove, the target list the **post-v0.1.0 DAW-matrix
audition** records its results against (`HANDOVER.md` Pending Tasks item (b); this document
existing is that audition's precondition, which is why it lands before it).

Status taxonomy per `docs/policies/COMPATIBILITY_POLICY.md` §"Status taxonomy": **Verified**
(provable from build/CI/code) · **Partially Verified** (README/CI claim, not fully provable
here) · **Unverified** (could work, no evidence in repo) · **Not Supported** (deliberate,
evidence-backed exclusion).

Copy-and-adapt provenance (ADR-0009): structure from
`Anamorph:docs/architecture/COMPATIBILITY_MATRIX.md`; every status and citation below was
re-derived from **this** repository.

## Plugin formats

| Format | Status | Evidence |
|---|---|---|
| **VST3** | **Verified** | Built on Linux/Windows/macOS; the primary target; pluginval gates it on all three platforms. `CMakeLists.txt` (`ANABASIS_FORMATS` opens with `VST3`); `build.yml`, all build jobs |
| **AU (Audio Unit)** | **Verified (build)** / **Unverified (host)** | Built on macOS only as a universal `.component` (`if(APPLE) list(APPEND ANABASIS_FORMATS AU)` — the brief requires AU for Logic Pro, §2). Real Logic/GarageBand loading has never been observed from this repository — that is audition row A2 below |
| **Standalone** | **Verified** | Built on all three OSes; `ANABASIS_BUILD_STANDALONE` defaults ON. `CMakeLists.txt` §3 |
| **AAX** | **Not Supported** | Deliberate, evidence-backed exclusion: `docs/policies/COMPATIBILITY_POLICY.md` carries the Not Supported entry; the brief's §2 format list has no AAX row (exclusion by construction, not by a sentence — its §14.3 uses AAX as the canonical Not Supported *example*), and `README.md` states it outright. Not in `ANABASIS_FORMATS`. The DSP core is wrapper-agnostic (ADR-0001), so a future AAX wrapper is low-cost — but it is explicitly not built and not claimed |

## Platforms / architectures

pluginval runs **both modes ×3, blocking, on every platform**, at the strictness held in one
place — `ANABASIS_PLUGINVAL_STRICTNESS` in `.github/workflows/build.yml` (no number is quoted
here on purpose; `docs/procedures/CI_CD.md` §"Strictness escalates by phase").

| Platform | Status | Evidence |
|---|---|---|
| **Linux x86-64** | **Verified (blocking gate)** | CI builds VST3 + Standalone; headless pluginval under `xvfb`, run against the **stripped** bytes (the strip step precedes validation on Linux only — CI_CD.md §Pipeline). `build.yml` `linux` job |
| **Windows x86-64** | **Verified (blocking gate)** | MSVC multi-config build; pluginval both modes ×3, no `continue-on-error`. `build.yml` `windows` job |
| **macOS universal (arm64 + x86_64)** | **Verified (blocking gate)** | `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`; the packaging step **asserts** both slices with `lipo` rather than printing them; pluginval both modes ×3. `build.yml` `macos` job |

**Supported-OS floor (macOS)** — the claim OQ-011 resolved and directed this document to
restate: **macOS 10.13+ (Intel), 11.0+ (Apple Silicon)**. `CMAKE_OSX_DEPLOYMENT_TARGET=10.13`
governs the x86_64 slice (deliberate — above JUCE 9's documented 10.11 floor, matching the
sibling product); the arm64 slice floors at 11.0 by toolchain regardless. Evidence: the
`build.yml` macOS configure step and its comment, `docs/OPEN_QUESTIONS.md` OQ-011.
**Supported-OS floor (Linux) — MEASURED AND GATED since 0.2.0.** This paragraph previously read
"no Windows or Linux OS floor is claimed — none has been decided or measured", which was accurate
and was the problem: the artifact had a floor all along, nobody had chosen it, and nothing reported
it. `scripts/check-linux-abi.py` now measures the shipped VST3 and Standalone on every push and
fails the run that raises the requirement. **This document quotes no number of its own and defers
to that file**, for the same reason `CLAUDE.md` quotes no pluginval strictness: the copy that rots
is the one nobody edits. What the floor means in user terms is stated there too — the families
gated, why `CXXABI` is one of them, and which systems fall below.

**No Windows OS floor is claimed** — none has been decided or measured, and inventing one here
would be a C7 violation. CI builds on `windows-latest`, which proves buildability there and nothing
about older systems. The Linux equivalent of that sentence stood for five versions before the
measurement above replaced it; the Windows one is the same shape and is a known gap rather than a
claim.

## I/O layouts

| Layout | Status | Evidence |
|---|---|---|
| **stereo → stereo** | **Verified** | The default layout: bus declaration `src/PluginProcessor.cpp:12-14`, predicate `isBusesLayoutSupported` (stereo out always). The whole test suite runs this layout |
| **mono → stereo** | **Verified (headless)** | Accepted since 0.1.1 (KI-009 fix): `isBusesLayoutSupported` admits a mono main input; `processBlock` duplicates it into channel 1 before the engine, exactly as the sibling does. The refusal this replaces forced hosts with mono sources to negotiate stereo→stereo and feed one live pin + one silent pin — the KI-009 one-channel-silent mechanism. Evidence: the battery's `mono in` case (`tests/state_tests.cpp`, `testBothChannelsCarryAudioThroughTheWrapper`) |
| **anything → mono / other** | **Not Supported** | Deliberate: Anabasis is a **stereo mastering** maximizer (`DEVELOPMENT_BRIEF.md` §0); the output main must be stereo, and inputs other than mono/stereo are rejected |

## DAW hosts — the audition target list

No host has ever been observed loading Anabasis from this repository. pluginval (both modes
×3, blocking, editor open under `xvfb`) is the **proxy** for host conformance — a strong one,
and not a substitute for real hosts. Every row below is therefore **Unverified**, and stays so
until the post-v0.1.0 DAW-matrix audition records evidence against it. **Do not mark any host
Verified without that evidence** (constraint C7); what each audition pass must exercise —
loading, automation, offline render, state restoration, and the recorded per-host checks (the
preset-step host-notification burst is a named checklist line — its bound lives there, not
here, the same single-place treatment the strictness gets above) — is owned by
`docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md`.

| Row | Host (format) | Status | Note |
|---|---|---|---|
| A1 | **REAPER, Windows (VST3)** | **Unverified** | One of the two hosts the brief's §10 acceptance criteria name for the smoke pass. REAPER's list-all-parameters behaviour is also the documented reason the host-hidden fields live outside the parameter tree (`src/InternalState.h` header) — worth eyeballing that Settings state is indeed absent from its parameter list |
| A2 | **Logic Pro, macOS (AU)** | **Unverified** | The second brief-named host, and the only AU coverage — the AU wrapper has build evidence only |
| A3 | Ableton Live, Cubase, Studio One, Bitwig, GarageBand, Pro Tools†, … | **Unverified** | No evidence in repo. † Pro Tools requires AAX — **Not Supported** — so Pro Tools can only ever appear here as "not loadable, by design" |

Rows A1–A2 are the brief's minimum (§10: "at least one full pass each"); A3 is discretionary
depth for the audition. Results, when they exist, belong in this table with per-host evidence
(host version, OS, what was run, what was observed), not as a bare status flip.

## Toolchain / dependency pins

| Dependency | Pin | Status | Evidence |
|---|---|---|---|
| JUCE | **9.0.1** — immutable commit `e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8` (FetchContent + `GIT_SHALLOW`). **No longer the sibling's pin**: Anamorph stays at 9.0.0 / `f8f8864…` (ADR-0028) | **Verified** | `CMakeLists.txt` (`ANABASIS_JUCE_TAG`); ADR-0028, and ADR-0008/OQ-001 for the original pin |
| C++ standard | **C++20** baseline (`ANABASIS_CXX_STANDARD`, legal values 20/23 — 23 exists only for the OQ-006 canary and is never a shipping configuration) | **Verified** | `CMakeLists.txt`; ADR-0008 decision B5; `cxx23-canary.yml` |
| pluginval | latest release, downloaded at validation time — **not pinned** | **Verified (that it is unpinned)** | `scripts/run-pluginval.sh`; recorded as a tracked improvement in `HANDOVER.md` §Critical dependencies |

See `docs/policies/DEPENDENCY_POLICY.md` for the version-lock reasoning; changing the JUCE pin
is an Architecture Review Gate item.
