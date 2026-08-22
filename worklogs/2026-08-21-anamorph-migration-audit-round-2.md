# Worklog — Anamorph → Anabasis migration audit, round 2 (2026-08-21)

Session-local evidence trail for the second migration audit. Raw investigation material, NOT
architecture documentation — `docs/SOURCE_OF_TRUTH.md`: worklogs are never cited as policy. Nothing
in this round changes code. What it produces is a set of decisions, each attached to the evidence in
one of the two trees that decided it, so that the implementation rounds that follow can be assigned
by id instead of re-argued.

**No migration was implemented in this round, deliberately.** The instruction was to investigate,
categorise and plan.

---

## STATUS UPDATE — 2026-08-22 (round A3, version 0.2.1): two verdicts re-opened and reversed

This section amends the verdicts below rather than replacing them. The reasoning recorded further
down was correct **under the premises it was given**, and both premises were withdrawn by the
owner's brief for round A3 — so what changed is the question, not the evidence.

| Item | Verdict below | Now | Why the verdict moved |
|---|---|---|---|
| **A2-32** `linux-lto-tests` | INVESTIGATE → *adopt, but not in this round* | **IMPLEMENTED** ([ADR-0033](../docs/architecture/design-decisions/ADR-0033-lto-validation-lane.md)) | The deferral rested on one clause — "deferred only because of CI cost". The brief states that GitHub Actions cost is not a constraint for this project. With that term gone, the verdict's own body ("technically valuable · passes identically") argues **for**. |
| **A2-31** Linux release toolchain | NOT NEEDED — "a release-topology decision with no reported problem behind it here" | **ADOPTED / ALIGNED** ([ADR-0032](../docs/architecture/design-decisions/ADR-0032-linux-release-toolchain.md)) | The verdict tested for a *product-specific reason to change*. The brief replaces that test with its inverse: the two repositories stay aligned on engineering infrastructure unless there is a concrete technical reason to differ. Under the new test "Anabasis has no problem today" stops being a reason to keep the divergence. |
| **A2-34** `setup-linux.sh` dependency profiles | NOT NEEDED (conditional on A2-32) | **NOT NEEDED — unchanged, and the condition it depended on was re-checked** | The profile split exists to stop `build-essential` overwriting a *container's* pinned compiler. A2-32 landed as an ordinary runner job — `g++-14` is packaged in Noble's archive, so no container is needed — so the split still has nothing to serve. |

**What A2-31's implementation actually is.** `linux` builds the shipped VST3 and Standalone with
the pinned Clang; `linux-clang` is deleted and its portability canary, warning-gate self-test +
gate and `AnabasisEngineRepro` move into `linux`; `merge-check` moves to the same compiler and
shares its ccache lineage; GCC is pinned (`ANABASIS_GCC_VERSION`) and keeps a job in
`linux-lto-tests`. The exposure this closes is that ADR-0031 pinned the compiler that gated
*diagnostics* while the one that produced the *binary* floated with the runner image.

**What A2-32's implementation actually is.** `linux-lto-tests`, on every push, two arms: clang-22
`-flto` (the shipped optimization class — INC-004's configuration) and g++-14 `-flto` (the other
major toolchain, which after A2-31 has no other pinned job). One arm more than the sibling's lane,
for a reason specific to this product and recorded in the ADR.

**Measured this round** (Linux x86-64, 4 cores, JUCE prebuilt, ccache cold):

| Build | Time | Result |
|---|---|---|
| Ship class, clang-22, full (VST3 + Standalone + tests + bench + probe) | 24.7 s configure + **539.4 s** build | 301 + 873 checks, 0 failures |
| Suites, clang-22, **no** LTO | 16.2 + **169.0 s** | 301 + 873, 0 failures |
| Suites, clang-22, **`-flto`** | 16.3 + **263.8 s** | 301 + 873, 0 failures — **identical** |
| Suites, g++-14, **no** LTO | 25.4 + **234.2 s** | 301 + 873, 0 failures |
| Suites, g++-14, **`-flto`** | 28.3 + **312.2 s** | 301 + 873, 0 failures — **identical** |

The full evidence trail, including the ABI re-measurement on the Clang artifact and the pluginval
and probe runs against it, is
[`worklogs/2026-08-22-lto-lane-and-linux-toolchain-alignment.md`](2026-08-22-lto-lane-and-linux-toolchain-alignment.md).

---

## Purpose

Determine what has landed in Anamorph since the last audit that Anabasis does not have, decide per
item whether it belongs here, and order the survivors into a plan. The previous audit (2026-08-13)
closed with 56 approved items landing in Anabasis 0.1.4; this one starts where it stopped.

## Scope, and how the baseline was fixed

| Boundary | Value | How it was established |
|---|---|---|
| Anamorph range | `b6a3db8..feb8991` | `b6a3db8` is the commit `docs/DESIGN.md` names as the P0 research baseline; `feb8991` was `origin/main` at audit time |
| Commits in range | 192 (non-merge) | `git log --no-merges b6a3db8..origin/main` |
| Already covered | 74, dated on or before 2026-08-13 | the previous audit's window; re-checked for residue, not re-audited |
| New material | 118, dated 2026-08-14 or later | the actual subject of this round |
| Anabasis side | `facc36a`, version 0.1.6 | the 0.1.6 commit; PR #20 merged it into `main` mid-audit, so the branch was restarted from `origin/main` at `595893c` — which adds that merge and one Dependabot codeql bump, neither of which moves a finding |

**The local Anamorph checkout was 192 commits behind its own `origin/main` when this started.** The
working tree sat at `b6a3db8` (2026-07-30). Every comparison below is against `origin/main` read
through `git -C /home/user/Anamorph show origin/main:<path>`; the sibling's working tree was never
checked out, edited or otherwise touched, per `CLAUDE.md` §3.

Coverage attempted: CI workflows, build system, plugin architecture, DSP, bug fixes, testing,
documentation and process, and reusable modules. The one area with no findings is DSP: nothing in
the range changes an algorithm, and the two engines share idioms rather than code.

## What the range turned out to contain

Nine commits in 192 touch `src/` at all, and five of those are one tooltip/hover round. The delta is
infrastructure, and it clusters:

1. **Mechanical enforcement of the Priority-1 realtime rule** (Anamorph ADR-0029) — three tiers,
   because no single one reaches every shipped toolchain.
2. **Proving the checkers live** — self-tests run in the job that uses them, plus five
   parser-correctness fixes to the portability scanner and two to the documentation scanner.
3. **Making the shipped artifact measurable** — a declared Linux ABI floor; macOS symbolication
   turned from best-effort into a contract.
4. **Toolchain determinism** — a pinned Clang major, a pinned GCC container, and a review-gate
   amendment saying which toolchain versions need an ADR and which cannot.
5. **CI economics and supply chain** — a compiler cache, a composite setup action, a merge-result
   job, commit-SHA pins on every action.

### Direction is not one-way, and assuming it was would have re-imported this repository's own work

Three separate places in the range say so in the sibling's own words:

- `Anamorph:scripts/check-docs.py:4` — "PROVENANCE: adopted verbatim from the sibling product
  Anabasis".
- Anamorph `6e57666` — "ci: adopt the Anabasis validation set; close the AU, Intel-slice and seed
  gaps".
- Anamorph `f967639` — the Linux installers gain "the three groups of behaviour the sibling
  product's copies had grown and these did not".

## Major findings

### Two Anabasis defects, found by comparison (C6 report)

Both are in the documentation gate, and both are false-green shapes.

1. **`scripts/check-docs.py:369` tests the skip set against the ABSOLUTE path.**
   `SKIP_DIRS.isdisjoint(path.parts)` with `parts = path.parts[:-1]`, where `rglob` yields absolute
   paths because `main()` resolves the root. Every ancestor of the checkout is therefore tested: a
   clone under a directory named `build`, `JUCE` or `node_modules` excludes every file in the
   repository.
2. **Nothing catches the result.** `main()` prints `0 file(s) clean` and returns 0. An empty scan is
   currently a pass, so the failure above is silent by construction.

Anamorph carries the repair for both (`75598bb`): resolve `path.relative_to(root)` first, and refuse
to report a clean run over an empty set. Filed as A2-04, first phase.

### One observation that is not a migration item

`.github/workflows/build.yml:251` — the `preflight` job is the pre-P1 scaffold guard. Its own comment
says it "becomes a permanent no-op" once `CMakeLists.txt` exists and "may then be deleted". It has
been a no-op since P1 and still costs a runner and a `needs:` edge on every push. Recorded here
rather than acted on: deleting a CI job is not an audit's job.

### The single best-fitting item is one Anabasis already asked for

`docs/architecture/REALTIME_SAFETY_AUDIT.md:70` reads: "No sanitizer/RT-checker run: this audit is
inspection, not instrumentation. A malloc-interposition run (e.g. an RT-safety checker under the DSP
suite) would upgrade the allocation claims from Verified-by-inspection to machine-verified; tracked
for P6's gate." P6 closed and the gate never landed. `Anamorph:tests/AllocationGuard.h` is literally
that instrument, and its `operator new` half is standard C++ and therefore reaches MSVC, which is the
one shipped toolchain no sanitizer lane can cover. `AnabasisEngine::process` (`src/dsp/AnabasisEngine.h:84`)
has the same shape as the entry point the sibling arms around.

## Migration decisions

34 items, `A2-01` … `A2-34`. The full text, with per-item evidence, adaptation and risk, is in the
HTML report produced with this round. Summary:

| Verdict | Count | Ids |
|---|---|---|
| Need to migrate | 18 | A2-01 … A2-18 |
| Cannot migrate | 6 | A2-19 … A2-24 |
| Not needed | 7 | A2-25 … A2-31 |
| Investigate before deciding | 3 | A2-32 … A2-34 |

### Need to migrate — the recommended set, in the order the roadmap runs them

| Id | Item | Phase |
|---|---|---|
| A2-04 | check-docs: root-relative skip filter + empty-scan guard | 1 |
| A2-05 | check-portability: five scanner-correctness fixes | 1 |
| A2-06 | `--self-test` for check-portability and check-citations | 1 |
| A2-11 | CHANGELOG fence tracker: the two missing CommonMark closer rules | 1 |
| A2-12 | pluginval crash-retry scoped to Linux | 1 |
| A2-16 | `scripts/preflight.sh` | 1 |
| A2-18 | TESTING_POLICY: a checker must prove it is live | 1 |
| A2-01 | Allocation guard armed around `process()` | 2 |
| A2-02 | Static realtime lint over audio-path bodies | 2 |
| A2-09 | Review-gate amendment: which toolchain versions are gated | 3 |
| A2-08 | Pin the Clang major | 3 |
| A2-03 | RealtimeSanitizer lane + entry-point annotation | 3 |
| A2-07 | Declared Linux glibc/libstdc++/CXXABI floor, gated | 4 |
| A2-10 | macOS symbolication: `-Wl,-object_path_lto` + two assertions | 4 |
| A2-17 | Twin-run self-discrimination for the existing probes | 4 |
| A2-14 | ccache + `setup-linux-build` composite action | 5 |
| A2-13 | `merge-check` job — build the merge result | 5 |
| A2-15 | SHA-pin every action + Dependabot semver split | 5 |

### Cannot migrate — and why, from Anabasis evidence

| Id | Item | Reason |
|---|---|---|
| A2-19 | Hover-occlusion term (`cursorIsOverOpenPopup`, KI-024) | The sibling derives hover geometrically from `getMouseXYRelative()`, which cannot express occlusion. Anabasis derives it from `Component::isMouseOver(true)`, and `src/gui/PluginEditor.cpp:2552-2570` carries the measured chain by which raising the shield re-points `componentUnderMouse` — "So both readers see false and the lift eases out." The question is already answered here |
| A2-20 | Idle-gate lit latch (`microLit`, KI-025) | Anabasis's micro-anim driver is a lean adaptation with no idle gate (`src/gui/PluginEditor.h:596`; `stepMicroAnims` at `src/gui/PluginEditor.cpp:2895` has only an `isShowing()` early return). There is no motion latch to seal |
| A2-21 | `stepVal` step-vs-distance landing repair | Anabasis's ease already tests distance only (`src/gui/PluginEditor.cpp:2907-2918`) and never had the step test the sibling deleted |
| A2-22 | The Clang and GCC warning baseline FILES | They key on the sibling's paths under the sibling's pinned majors. Importing one would permit warning classes Anabasis currently forbids outright. The mechanism is separable; the data is not migratable |
| A2-23 | Linux installer hardening (`f967639`) | Direction reversed — verified present here, including `--discard-parked` (`packaging/linux/uninstall.sh:29-45`) and the `\.probe\b` scratch name (`scripts/check-portability.py:197`) the sibling had to add back |
| A2-24 | check-docs CHANGELOG heading widened to `^## [` | Anabasis's narrower pattern deliberately exempts `## [Unreleased]`, which by definition carries no date. The wider form would demand one |

### Not needed

`A2-25` C++23 as the default standard — the `ANABASIS_CXX_STANDARD` switch and `cxx23-canary.yml`
already discharge the ADR-0008 / OQ-006 mandate. `A2-26` JUCE 9.0.1 — Anabasis arrived first
(ADR-0028, 0.1.5). `A2-27` the `macos-latest` runner — done in `a36149e`. `A2-28` AU gate, native
Intel job, Rosetta slice and the nonzero deterministic seed — these came from Anabasis. `A2-29`
`tests/dsp_dump.cpp` as a file — `tools/channel_probe.cpp` and `tools/engine_repro.cpp` cover both
levels and ADR-0028 used the former for exactly the twin-build comparison; only the
self-discrimination property is worth taking, which is A2-17. `A2-30` `tests/bench.cpp` — Anabasis
has its own from P6. `A2-31` ADR-0030's Linux release toolchain — a release-topology decision with
no reported problem behind it here.

### Deferred pending measurement

`A2-32` an LTO test lane, `A2-33` libFuzzer over `setStateInformation`, `A2-34` `setup-linux.sh`
dependency profiles. Each needs a coverage comparison Anabasis has to take; A2-34 is conditional on
A2-32 and has nothing to serve otherwise.

A2-32 is the one worth restating, because the argument for it is this repository's own history:
INC-004 — the silent left channel — was undefined behaviour that manifested only under Clang at
`-flto`, the configuration ADR-0008 builds the plugin in and no console target uses, and every
console-target gate was green throughout. What is not yet known is whether an LTO-built suite reaches
anything `AnabasisChannelProbe` against the Clang-LTO'd bundle does not.

## Governance boundaries this plan touches

Nothing in the recommended set renames or removes a parameter id, changes the serialization schema,
changes the threading model, changes DSP signal order, or changes reported latency.

- **A2-03** reaches `ARCHITECTURE_REVIEW_GATE.md` twice — a Build System plus validation change, and
  an annotation on a `DSP_POLICY`-frozen path. It needs an ADR and a byte-identical-object proof for
  the annotation before it is acceptable.
- **A2-08** reaches the same gate as a pinned compiler version, which is why A2-09 lands first: the
  amendment is what says so.
- **A2-15** reverses a decision recorded in `docs/policies/DEPENDENCY_POLICY.md:28` ("Floating majors
  are a deliberate trade"). The policy rows change in the same commit or the repository contradicts
  itself.
- **A2-07** must measure Anabasis's own artifact. Copying the sibling's floor constants would be the
  invented number `AI_AGENT_POLICY.md` C2 forbids.

## What this round deliberately did NOT do

- **No code, workflow, script or policy was changed.** The only file added is this worklog.
- **The sibling repository was not modified, and was not checked out.** `git fetch` and
  `git show origin/main:<path>` only.
- **The sibling's citation-gate expansion is not an item.** `Anamorph:scripts/check-citations.py`
  grew from 816 to 2,099 lines in this range — symbol glosses checked against the anchor's contents,
  a template-span parser, a `DELIBERATE_REAIMS` declaration lifecycle, `--fix` invalidation
  reporting. It is real work and it is out of scope: Anabasis's citation gate has a more basic
  problem first, which is that it has never been proven live (A2-06).
- **Anamorph's coverage-audit prose is not an item.** `docs/DOCUMENTATION_COVERAGE.md` gained 4,849
  lines in the range; it is that repository's record of its own rounds.
- **No verdict was taken on the sibling's newer ADRs as ADRs.** Anabasis numbers its own from
  ADR-0001 (`ADR_POLICY.md`); anything migrated that needs a decision record gets an Anabasis number.

## Deliverables

- The HTML report — investigation summary, the full 34-item checklist, the four categories, the
  six-phase roadmap, the dependency and risk tables, and the per-phase validation requirements.
- This worklog.
