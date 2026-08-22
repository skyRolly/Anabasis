# CI_CD.md

Continuous integration / delivery. Source of truth: `.github/workflows/`.

## Workflows

| Workflow | Purpose |
|---|---|
| `build.yml` | Build + validate on three OSes. The gate. |
| `codeql.yml` | CodeQL analysis (`c-cpp` + `actions`). |
| `msvc.yml` | MSVC `/analyze` → SARIF. |
| `dependency-review.yml` | Dependency Review on PRs to `main`. |
| ~~`cxx23-canary.yml`~~ | **Removed at 0.2.0 (ADR-0030).** C++23 is the baseline, so every job in `build.yml` compiles it on all three platforms as a blocking check and the weekly non-blocking copy became a duplicate build of the baseline. |
| `release.yml` | Tag-triggered draft release. Not present — deferred to the first commercial release by **OQ-007** (resolved 2026-08-02), no longer a P6 item. |

## The pre-P1 preflight guard

`build.yml`, `codeql.yml` and `msvc.yml` each start with a `preflight` job that checks whether
`CMakeLists.txt` exists. While the repository is a P0 scaffold with no build, the gated jobs skip
cleanly with a notice instead of failing on a missing project. The guard becomes a permanent no-op
the moment P1 lands — it needs no removal, but it may be removed once the build is real.

**In `codeql.yml` the guard selects the matrix, rather than conditioning the job.** Only `c-cpp`
needs a project; the `actions` entry analyses `.github/workflows/**`, which exist right now and
need no build. Gating the whole matrix would switch **workflow** security scanning off for the
entire P0 phase — exactly the phase in which these workflow files are being written. So `preflight`
**emits the matrix as JSON** (always `actions`; plus `c-cpp` once `CMakeLists.txt` exists) and
`analyze` consumes it via `strategy.matrix: ${{ fromJSON(needs.preflight.outputs.matrix) }}`. The
`analyze` job carries no `if:` at all: an entry that is not in the matrix simply does not exist, so
nothing is skipped and nothing reports red.

The obvious-looking alternative — a job-level `if: matrix.language != 'c-cpp' || …` — **does not
work**, and silently: `matrix` is not an available context in `jobs.<id>.if` (only `github`,
`needs`, `vars` and `inputs` are). The expression either fails workflow validation or evaluates
`matrix.language` as empty, which makes the test always true, so the `c-cpp` entry runs anyway and
fails on the missing project. `needs` **is** available in `jobs.<id>.strategy`, which is why the
dynamic matrix is the mechanism that actually holds.

## `build.yml` — triggers

`push` to any branch (`"**"`), `pull_request`, `workflow_dispatch`, and `workflow_call` (so a
future `release.yml` can reuse the whole matrix with identical gates — tag pushes do not trigger
`build.yml` directly, the `branches` filter excludes tag events). Permissions: `contents: read`.

## Build matrix

| Job | Runner | Builds | pluginval |
|---|---|---|---|
| **linux** | `ubuntu-latest`, **pinned Clang** (ADR-0032) | VST3 + Standalone (+ tests + bench + probe) — **the shipped Linux artifact**; runs the portability compile canary, both reproductions against the bundle it just built, the ABI floor assertion, and (last, after the uploads) the zero-first-party-warning gate | both modes ×3 — **blocking** |
| **windows** | `windows-latest` (MSVC, multi-config) | VST3 + Standalone (+ tests) | both modes ×3 — **blocking** |
| **macos** | `macos-latest` (Apple Silicon — macOS 26 today) | universal VST3 + **AU** + Standalone (+ tests) | both modes ×3 — **blocking** |
| **docs** | `ubuntu-latest` | nothing — runs `scripts/check-docs.py --self-test` then the full corpus | n/a |
| **source-lint** | `ubuntu-latest` | nothing — runs `scripts/check-portability.py`, then `scripts/check-citations.py --check` against a computed base revision. Checked out with `fetch-depth: 0`, because the second needs history rather than a single commit (seconds) | n/a |
| **macos-intel** | `macos-15-intel` (**native x86_64**) | thin x86_64 VST3 + AU + Standalone (+ tests + probe) | deterministic ×3, VST3 **and** AU — **blocking** |
| **linux-lto-tests** | `ubuntu-latest`, **inside `container: gcc:16`** (ADR-0034) | the two test targets with `-flto` on the compile AND the link, under the **pinned GCC**, then both suites run against that codegen; asserts the container's major before building and gates first-party warnings at zero. Installs the `headless` dependency profile, because `build-essential` would put a distribution GCC over the pinned one | n/a |
| **linux-lto-clang** | `ubuntu-latest`, **pinned Clang** (ADR-0033) | the same two targets under `-flto` with the **shipped optimization class** — INC-004's configuration, which a GCC-only lane cannot reproduce. Separate job rather than a matrix arm because `container:` is a per-job key | n/a |
| **AnabasisChannelProbe** (a step in `linux`, `windows`, `macos`, `macos-intel`) | — | the only check that HOSTS the built bundle instead of recompiling its sources: LTO'd, wrapped, and on macOS run for **both formats × both slices** | n/a |
| **AnabasisEngineRepro** (a step in `linux`, `macos`, `macos-intel`) | — | the same stimulus with NO wrapper, format or host, so a failure names the DSP core directly instead of leaving "engine or wrapper?" to be inferred | n/a |
| **sanitizers** | `ubuntu-latest` (Clang) | the two test targets under ASan + UBSan, plus a plain build for valgrind memcheck over **both** suites | n/a |

**Why five Linux jobs and not one.** They answer five different questions and only one of them
is "does it build here".

* `source-lint` guards two classes NO build of any kind can see, which is why it is a separate job
  with no `needs:` — a source defect should fail the run on its own terms rather than queue behind
  a toolchain.
  * **Platform-divergent source (`check-portability.py`).** INC-003 was a hard compile error on
    macOS produced by a line that GCC *and* Clang both accept on Linux, because the divergence is
    in the platform's `<cstdint>` typedefs (`size_t` is `uint64_t` here and is not there), not in
    the front end. A lint is the only Linux-runnable guard for that. The same script also compares
    the scratch names `install.sh` CREATES against the ones `uninstall.sh` REMOVES: that pair has
    already diverged once — a `/var/tmp` staging candidate added to one file and not the other, so
    an interrupted install survived a deliberate uninstall — and the two scripts ship as separate
    files in a zip, with no shared library to source and no build step that could generate one, so
    the coupling can only be checked, never removed. Both directions are mutation-verified.
  * **Evidence-anchor drift (`check-citations.py`).** A `file:line` citation in a document of
    record is silently re-aimed by any edit ABOVE it, and the document keeps reading as though it
    were still correct. 0.1.4 showed the re-anchoring rule does not survive being remembered — the
    anchors were re-anchored once, two later commits in the same round moved code again, and 42 of
    71 were stale before anyone noticed. Nothing a compiler or a test suite does can detect it.

  **What the citation step compares against, and what that costs.** `fetch-depth: 0` is there for
  this step alone: it reads the base revision's copy of each tracked source and each document. The
  base is `github.event.pull_request.base.sha` fed through `git merge-base` (a FORK pull request),
  or `github.event.before` (a push). The `merge-base` hop is a no-op on that event and the comment
  in the workflow now says so: `actions/checkout` checks out the generated MERGE commit, whose
  first parent is `base.sha`, so the call returns its input. It is kept as the correct expression
  of the intent — it starts doing work if the checkout is ever pointed at the PR head — but
  `fetch-depth: 0` is justified by something else entirely: `git show <base>:<file>` needs the base
  commit to be PRESENT, and a default depth-1 checkout does not have it. A base that names a commit the repository no longer has —
  after a force-push — falls back to `HEAD~1` with a `::notice::` rather than failing on a question
  it cannot answer. Note the reach honestly: this job carries the same-repo `pull_request` guard the
  `docs` job does, so on a same-repo branch the step runs on the PUSH event only, where the base is
  the branch's previous tip. It therefore checks **one push of drift at a time**, and catches the
  0.1.4 failure mode only because every push is checked; the merge-base path is exercised by fork
  pull requests. Anchors the run could not judge — re-spelled or removed since the base — are
  counted and reported separately rather than netted into the pass total, and on a re-anchoring
  round that is not a footnote: pairing is ORDINAL per path, so a change set that adds or removes
  a citation drops that document to a fallback which checks only base spellings still present
  verbatim. On this very round the tool reported *17 checked, 16 beyond what it could judge* —
  roughly half. Combined with the one-push depth above, that is the mechanism by which a
  re-anchoring round can still ship stale anchors, and it is why `--fix` is not a substitute for
  reading what it moved.

  **Two bounds a green `source-lint` does not cover, stated so the badge is not read as more than
  it is.** A document that did not exist at the base has NOTHING to compare against, so every
  anchor in a NEW record — each ADR added by the change set that introduces it — is unjudged on
  the run that introduces it, and has to be read by hand. And a `--fix` that rewrites a tracked
  SOURCE file changes that line's text, so any anchor elsewhere aimed at that line is thereafter
  measured against wording that moved for a reason unrelated to code movement; the tool now names
  the rewritten lines instead of leaving that silent, but settling it is a human's job.
* `linux` carries the AppleClang DIAGNOSTIC set (`-Wshorten-64-to-32`,
  `-Wimplicit-int-float-conversion`, `-Wshadow-field`, …) that `juce_recommended_warning_flags`
  applies to Clang and not to GCC. Those reached us only from the macOS runner before, minutes
  into a universal build — or not at all, while that job was red for an unrelated reason. It does
  NOT catch the typedef class above; the two gaps are separate, and conflating them is how the
  second one gets dropped. Since INC-004 the **plugin** is built and hosted in the same job that
  gates those diagnostics — the only artefact that carries `juce_recommended_lto_flags` (ADR-0008)
  — and both reproductions run against it. `TESTING_POLICY.md` states that half as non-optional: a
  gate that compiles the sources in a configuration the customer never receives is not a gate on
  the product, which is how a channel-dropping miscompilation shipped for five months behind 1039
  green checks. **Since ADR-0032 this is also the job that SHIPS**, so the configuration those
  gates read and the configuration a user installs are now the same bytes rather than two builds
  argued to be equivalent.
* **`linux-lto-clang` and `linux-lto-tests` are two jobs, not one lane with two arms** (ADR-0033,
  split by ADR-0034). Between them they close the other half of that gap and keep the second
  toolchain alive. The suites do not link the LTO flags the plugin does, so every assertion this
  repository makes used to be made against non-LTO objects — the configuration INC-004 needed in
  order to *not* manifest.
  * `linux-lto-clang` runs them against the **shipped optimization class** — the pinned Clang at
    `-flto`, INC-004's own configuration. This job has no counterpart in the sibling product, and
    that is deliberate: a GCC-only lane cannot reproduce the incident this one is named after.
  * `linux-lto-tests` runs them under the **pinned GCC, inside `container: gcc:16`**, which is
    since ADR-0032 the only place a GCC this repository CHOSE compiles this tree — the image's GCC
    still builds the unsanitized `build-vg` copy in `sanitizers`, but that compiler is incidental to
    a memcheck run and its version is the runner's. It installs the `headless` dependency profile,
    because `build-essential` inside that container would put a distribution GCC over the pinned one.
  * **Why two jobs rather than a matrix:** `container:` is a per-job key, so one containerised arm
    and one bare arm cannot share a `strategy.matrix`. Two jobs are also two independent results — a
    red Clang result cannot hide a GCC one.
  * Each builds the same two targets, which between them compile every first-party translation unit
    — which is why the GCC job is a compatibility guarantee about the source rather than about a
    sample of it.
* `sanitizers` catches, on Linux, defects that only MANIFEST elsewhere: memory this OS hands back
  zero-filled is arbitrary on macOS, so an uninitialised read is benign here and poisonous there
  while the defect itself is platform-independent.

**Known coverage boundaries, named rather than left to be rediscovered.** Each of these is a real
limit of the matrix as it stands; none is a defect in it.

* **macOS-hosted runners are not a DAW.** The probe and pluginval exercise the real wrapper and
  the real binary, which is as close as automation gets, but neither is Logic, Ableton or Reaper:
  host-specific buffer arrangements, parameter automation patterns, sample-rate/blocksize changes
  mid-stream and plugin rescan behaviour remain outside CI. A green macOS matrix is evidence
  about the plugin, not a substitute for the DAW audition `TESTING_POLICY.md` Level 5 requires.
* **No CI gate runs the SUITES' assertions against LTO'd code** — but the channel probe and the
  engine reproduction do, and that is why they exist. The plugin target links
  `juce::juce_recommended_lto_flags`; both test targets deliberately do not (ADR-0008), so the
  DSP and state assertions always run un-LTO'd. This boundary is not theoretical: INC-004 was
  undefined behaviour that only Clang at `-flto` acted on, so it was invisible to every suite,
  every sanitizer and valgrind while the shipped bundle dropped a channel. What closes it is
  running an ORACLE against the LTO'd binary — pluginval's conformance checks on all platforms,
  plus `AnabasisChannelProbe` and `AnabasisEngineRepro`, whose oracle is the product's own
  behaviour rather than a conformance rule.
* ~~**The x86_64 macOS slice runs under Rosetta or not at all.**~~ **CLOSED** — the `macos-intel`
  job (`runs-on: macos-15-intel`) builds a thin x86_64 product on native Intel hardware and runs
  the suites, both reproductions, the probe against VST3 **and** AU, and pluginval there. It
  asserts `uname -m` and `sysctl.proc_translated` and FAILS if either says otherwise, because a
  green tick from the wrong architecture would read as "Intel is fine". The `macos` job's Rosetta
  step is retained as a second, cheaper signal on the universal binary; it still warns rather than
  fails when the image has no Rosetta, and that is now a redundancy rather than the only coverage.
* **A same-repo `pull_request` event reports a GREEN "Build & Validate" with ZERO build jobs.**
  `docs`, `source-lint` and `preflight` all skip that event on purpose (the `push: ["**"]`
  trigger already built the SHA), and `preflight` skipping means every build job's `needs` is
  unsatisfied. That is correct for duplicate avoidance and dangerous for branch protection:
  making this workflow a required check only works if the protection treats a skipped conclusion
  as passing. See "Before enabling branch protection" below.
* **On macOS and Windows pluginval validates PRE-strip, PRE-codesign bytes** (OQ-012). Only Linux
  validates the exact shipped bytes.
* **`MALLOC_PERTURB_` is glibc-only**, so the hostile-allocator proxy on the Linux self-test steps
  has no equivalent on the macOS or Windows jobs — libmalloc and the Windows CRT ignore it.
  Setting it there would read as coverage that does not exist, so it is deliberately absent.

The **docs** job is deliberately outside the `preflight` gate and outside every build job's `needs`:
it must run while the repository is still a pre-P1 scaffold (the phase in which the documentation
*is* the deliverable), and a prose defect should fail the run without skipping a binary that is
otherwise fine. `--self-test` runs first because it is the load-bearing half — the checker once
reported the repository clean while an unclosed fence exempted 1382 lines of the largest document
from every check, so a zero exit is not evidence unless the script's own guarantees were exercised
in the same run.

Validation is **uniform and blocking on every platform**: there is no `continue-on-error`, so a
non-zero pluginval exit fails the job everywhere.

## Strictness escalates by phase — in one place

That place is the `env:` block at the top of `.github/workflows/build.yml`
(`ANABASIS_PLUGINVAL_STRICTNESS`), and this section deliberately does **not**
quote its current value. It used to, and the copy went stale the moment the
build raised the bar for P6 — in the very file whose heading promises one place
only. Read the number there; raising it stays a one-line edit.

The sentence that used to close this section sent readers to
`docs/policies/TESTING_POLICY.md` as "likewise the only document that states
it", which was a circle: that policy does **not** state the number — it
explicitly refuses to, and points back here at `build.yml`. Three files each
claiming to be the single source is how the value gets copied a fourth time.
The division of labour, stated once:

| Question | Answered by |
|---|---|
| What number is in force, and what it was per phase | `.github/workflows/build.yml` (`env:` block) — **the only source** |
| What the gate *requires* — suites, modes, passes, platforms | `docs/policies/TESTING_POLICY.md` |
| How the pipeline is wired to meet it — jobs, step order, artefacts, retries | this document |

## Pipeline

**The step order differs by platform, and the difference is deliberate**, so the numbering below
is per-platform rather than a single list.

Common to all three:

1. **Checkout**.
2. **Configure** — `cmake -B build [-G Ninja] -DCMAKE_BUILD_TYPE=Release
   -DANABASIS_BUILD_NUMBER=${{ github.run_number }}` (the run number becomes the About-box build
   number). Windows uses the default VS generator; macOS adds
   `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` and a deployment target. **Linux additionally passes
   `-DANABASIS_BUILD_BENCH=ON`** — the bench is OFF by default so it cannot be run casually and
   quoted without its machine (`PERFORMANCE_BUDGET.md`), but OFF everywhere meant `tests/bench.cpp`
   was compiled by no job at all and its DSP calls would rot silently. One platform compiling it
   makes that rot a red build; `build.yml`'s own comment records the two residuals (only this
   platform, and compiling is not running).
3. **Build** — `cmake --build build --config Release` (Linux therefore also builds `AnabasisBench`,
   which is never RUN in CI — shared-runner timings are exactly the numbers that must not be
   quoted).

Then:

| | **Linux** | **Windows / macOS** |
|---|---|---|
| 4 | **Symbol handling** — `objcopy --only-keep-debug`, `strip --strip-unneeded`, `.gnu_debuglink` embedded, VST3 entry point re-asserted | **Self-tests** |
| 5 | **Self-tests** | **pluginval** |
| 6 | **pluginval** | **Stage / package** (incl. symbol handling) |
| 7 | **Stage + upload** | **Upload** |

**On Linux the strip runs before the self-tests and pluginval on purpose:** the release gate then
validates the exact stripped bytes users receive, not a differently-linked intermediate. It also
means the strip step is what the customer-artifact staging gates on, so a strip failure blocks the
public upload while leaving the developer debug artifact — reason enough for the order to be
stated accurately rather than tidily.

On macOS the packaging step runs after validation and orders itself `dsymutil` → `strip -x` →
ad-hoc codesign (stripping after signing would invalidate the seal); dSYM capture is best-effort
and never blocks the customer pipeline. Windows purges all debug material from the public copy,
validates it, and only then retains each shipped image's linker PDB into a separate debug
artifact — see the checkpoint rule below for why that order matters.

> **Known asymmetry: only Linux validates the shipped bytes.** Because macOS and Windows strip
> (and, on macOS, sign) *after* pluginval, a defect introduced **by stripping or signing** would
> ship unvalidated there. "Uniform and blocking" above describes the pluginval **gate** — same
> strictness, same two modes ×3, failing the job on every platform — not which bytes it sees.
> Whether to reorder is `docs/OPEN_QUESTIONS.md` **OQ-012**, deliberately deferred to P6 when
> there is a binary to measure rather than a guess to make.

**pluginval** runs deterministic ×3, then randomise ×3. **Both** steps carry the **same** explicit
guard — neither relies on implicit skipping — so a **deterministic** failure never *skips* the
randomise step: both modes always report independently, and the job still fails if either fails.
A failed **build** skips both, because there is no plugin to validate and a second red step about
a missing `.vst3` only obscures the real cause.

| Job | Guard on **both** pluginval steps |
|---|---|
| `windows`, `macos` | `if: ${{ !cancelled() && steps.build.outcome == 'success' }}` |
| `linux` | `if: ${{ !cancelled() && steps.build.outcome == 'success' && steps.strip.outcome == 'success' }}` |

The extra Linux term is load-bearing, not decoration: Linux strips **before** validating, and
`steps.build.outcome` stays `success` when the *strip* step fails — so without it the gate would
validate a partially-stripped binary while this document claims Linux validates the exact bytes
users receive. Weakening it re-opens exactly that hole.

Both modes pass a strictness and a mode to `scripts/run-pluginval.sh` (`.ps1` on Windows);
deterministic mode pins a **nonzero** `--random-seed` — `0` is pluginval's "generate a random seed"
sentinel and pins nothing (`docs/procedures/TESTING.md`).

**Uploads** produce customer artifacts (`Anabasis-<OS>`, loose files) and debug artifacts
(`Anabasis-<OS>-debug`).

## Duplicate-build avoidance

`push: ["**"]` builds every branch, and `pull_request` would rebuild the same SHA once a PR is
open — two full 3-OS matrix runs per commit. The `preflight` job therefore skips **same-repo**
pull_request events, since the push event already covered that SHA. Fork PRs still run: their
push happens in the fork, so the `pull_request` event is the only trigger that sees them.

`codeql.yml` and `msvc.yml` do **not** need the same guard: both are `branches: [main]`-only, so a
feature-branch push cannot double up with its PR.

**Latent hazard for `workflow_call` (re-check at P6).** Inside a reusable workflow
`github.event_name` reflects the **caller's** event, and every build job gates on
`needs.preflight.outputs.ready`. So a future caller invoked on a same-repo `pull_request` — a
release rehearsal wired to PRs, say — would skip `preflight`, get **zero** build jobs, and still
report success. The planned caller (`release.yml`) is tag-push-triggered, so this is latent rather
than broken; the condition needs re-reading when `release.yml` lands.

## `msvc.yml` is doubly inert until P1 — rehearse it on purpose

Its path filters are `src/**`, `tests/**`, `CMakeLists.txt` — none of which exist yet — **and its
own file**. So a change to `msvc.yml` itself on `main` *does* start it (this bootstrap change
included); what makes it a no-op in that case is `preflight`, not the filters. Everything else
(the weekly schedule, `workflow_dispatch`) is likewise started and then stopped by `preflight`.

The conclusion still holds — the workflow is inert until P1 — but the pinned third-party analysis
action has consequently **never executed** here. **Run it once via `workflow_dispatch` at P1**,
rather than discovering an incompatibility inside the P1 build PR.

## The C++23 canary — retired (ADR-0030)

This section described a weekly, non-blocking job that built the DSP suite at C++23 on three OSes to
answer "does tomorrow's baseline still compile?". **At 0.2.0 the baseline moved to C++23**, so the
question is answered by every job in `build.yml` on every push, as a blocking check, and the canary
workflow was deleted along with the `ANABASIS_CXX_STANDARD` seam it drove.

The reasoning is kept here rather than removed: the canary was correct while it stood, and the
condition it was waiting for — the baseline catching up — is what retired it. `OPEN_QUESTIONS.md`
OQ-006 carries the same supersession.

## What 0.2.0 added to the pipeline

| Job / mechanism | Why it exists |
|---|---|
| **`merge-check`** | The ONLY job that runs on a same-repo pull request. Every other job is skipped there because the push trigger already built that SHA — the TIP, not the merge — so `refs/pull/N/merge`, the tree the merge button produces, was never compiled. Build and self-tests only: what a moved base breaks is compilation and behaviour, and both are platform-independent. |
| **`realtime`** | RealtimeSanitizer over the DSP suite (ADR-0029), in its own job because the Clang driver refuses to combine `-fsanitize=realtime` with the `sanitizers` job's set. A liveness canary runs first and the job fails if it does NOT abort. Also carries the compile-only `-Wfunction-effects` gate over the JUCE-free leaf layer, compiled twice so a dead diagnostic cannot pass. |
| **The pinned Clang** | `ANABASIS_CLANG_VERSION` in `build.yml` is the single authority; `scripts/setup-llvm-apt.sh` installs it fail-closed. Four jobs use it, and since ADR-0032 one of them SHIPS with it. Without it the zero-first-party-warning gate has no stable reference point and the RTSan runtime does not exist (ADR-0031). |
| **The composite action + ccache** | `./.github/actions/setup-linux-build` carries the setup and the "ccache is an optimization, never a requirement" fallback ONCE for five jobs; the per-job cache lineage stays in the workflow because that is the part that differs. **The macOS jobs are cached too** (0.2.2, measured 0.2.4) — this row said they were "deliberately not cached" on the `dsymutil` argument long after 0.2.2 refuted it and cached them, contradicting two other rows in this same file. |
| **The Linux ABI floor** | `scripts/check-linux-abi.py` on the STRIPPED bytes, last in the `linux` job so a compatibility finding never withholds an artifact whose behavioural gates passed. |
| **macOS symbolication as a contract** | `-Wl,-object_path_lto` keeps the object `dsymutil` needs; two assertions — LTO ran and its objects were retained, and a UUID-matched dSYM was captured — turn a best-effort capture into a gate. Both run after the uploads. |
| **Self-tests beside their checks** | `TESTING_POLICY.md` rule 5. Five checkers ship `--self-test` and each runs in the job that uses it, ahead of the use. |

## What 0.2.1 changed in the pipeline

| Change | Why |
|---|---|
| **`linux` builds with the pinned Clang and SHIPS that build** (ADR-0032) | The artifact users install was previously produced by whatever `g++` the runner image carried — the one part of the toolchain nothing pinned. The reproductions aimed at INC-004 (`AnabasisEngineRepro`, `AnabasisChannelProbe --assert-discriminating`) now run against the bundle that ships rather than against a second Clang build that was thrown away. |
| **`linux-clang` deleted** | Its three own steps — the portability compile canary, the warning-gate self-test + gate, and the engine reproduction — moved into `linux`. What is gone is a second full Release compile of the same tree, not a check. |
| **The warning gate runs last, after the uploads** | A DIAGNOSTIC finding must not withhold a beta artifact whose behavioural gates passed. It still fails the job. Same rule the ABI assertion follows. |
| **`merge-check` moved to the same compiler** | A merge-check on a different compiler from the one that will build the merge result is a check of a tree nobody ships; it also shares `linux`'s ccache lineage, and two lineages in one budget evict each other. |
| **`linux-lto-clang` + `linux-lto-tests`, two jobs** (ADR-0033, split by ADR-0034) | The suites do not link the LTO flags the plugin does, so every assertion was made against non-LTO objects — the configuration INC-004 needed in order to stay hidden. `linux-lto-clang` runs them against the shipped optimization class; `linux-lto-tests` keeps the second major toolchain compiling this tree on every push, in its container. They are separate jobs because `container:` is a per-job key. |
| **The pinned GCC** | `ANABASIS_GCC_VERSION` in `build.yml`, asserted against `g++ -dumpversion` inside `container: gcc:16` — the image supplies the compiler, so the workflow's job is to CHECK the major rather than to install it (ADR-0034; it came from the distribution archive while the pin was 14). Pinned for the same reason the Clang major is: a compatibility compiler is only a compatibility statement while WHICH compiler is a fact of this file rather than of a runner image. **Measured at 16.2.0: zero first-party warnings across all 13 translation units in the two suites** — see the 0.2.3 row below. |

## What 0.2.3 changed in the pipeline — the GCC 16 warning gate, measured

0.2.2 shipped the `gcc:16` container lane without ever having run it — this environment has no
container runtime, and the ADR said so. The first push measured it, and the measurement is the
point of this row.

| Change | Why |
| --- | --- |
| **`libxi-dev` added to `CORE_PACKAGES`, on BOTH profiles** | The lane died in three JUCE translation units at `fatal error: X11/extensions/XInput2.h: No such file or directory`. JUCE 9.0.1 defaults `JUCE_USE_XINPUT` to 1, so `juce_gui_basics.h:393` includes that header unconditionally in practice, and it belongs to `libxi-dev` — a package the list never named. It reached `full` transitively, via `libgtk-3-dev`'s `Depends: libxi-dev`, which is why no runner and no developer machine ever noticed; `headless` drops the gtk/webkit pair on purpose and dropped the X-input headers with it. |
| **The gate's failure message no longer says "Clang"** | Three lanes feed `check-clang-warnings.py` and two of them are GCC. `--compiler` is now passed by each caller (`clang++-22`, `g++-16`, `clang++-22`), defaulting to a neutral `the compiler` rather than to the wrong one. Attribution only, no change to what the gate accepts or rejects. |
| **Three self-test cases for driver-level warnings** | Every GCC LTO link emits `lto-wrapper: warning: using serial compilation of N LTRANS jobs`. It carries no source location, so it is correctly not a diagnostic this gate can attribute — and now that is pinned rather than incidental, alongside the `cc1plus:` and `ld:` forms. 15 → 18 cases. |

**The warning finding, stated plainly: GCC 16 introduced no new diagnostics in this tree.** Run
32565784751 compiled all **13 first-party translation units** of the two suites under g++ **16.2.0**
at `-O3 -flto -std=c++23` with the full `juce_recommended_warning_flags` set, and none emitted a
warning. The zero-warning policy holds at 16 exactly as it held at 14.2.0, so nothing about the gate
was weakened — the lane's failure was a missing header, not a diagnostic. That run aborted before the
**LTO link**, where `-Wodr` and `-Wlto-type-mismatch` fire; **run 32568563583 closed that cell**
(0.2.4). The fixed lane succeeded end to end, both links ran, the gate printed `no first-party
warnings`, and the only two `warning:` lines in the whole job were the `lto-wrapper` LTRANS notices
the 0.2.3 self-test had just pinned as non-diagnostics. Both suites passed against that codegen:
301 + 873, 0 failures. **GCC 16.2.0 is clean through compile and link.**

**The methodological finding is the one worth carrying forward.** 0.2.2 verified that every package
name *resolves* on trixie and on noble. Whether the declared set is *sufficient* is a different
question, and it was never asked. The check that asks it: enumerate the system headers the vendored
tree includes, map each to its owning package with `dpkg -S`, and diff against the declared list.
Against JUCE 9.0.1 that is fifteen headers over eight packages, and `libxi-dev` was the only
omission.

## What 0.2.2 changed in the pipeline — the parity audit

Every row here came from reading the sibling's tree, not from memory of the previous round. The full
table, including the areas that were **already** equivalent, is in
`worklogs/2026-08-22-ci-toolchain-parity-audit.md`.

| Change | Why |
|---|---|
| **GCC 14 → 16, in `container: gcc:16`** (ADR-0034) | No apt source ships a *released* g++-16, so pinning 14 was choosing the version to fit the acquisition method. The image is the only package-managed route to a released 16. `dependency-profile: headless` comes with it — `build-essential` inside that container would un-pin the lane. |
| **The LTO lane is two jobs** | `container:` is a per-job key, so one containerised arm and one bare arm cannot share a matrix. `linux-lto-tests` is GCC; `linux-lto-clang` is the Clang arm ADR-0033 added for INC-004. |
| **ccache: `CCACHE_DIR`, `CCACHE_MAXSIZE`, `CCACHE_COMPILERCHECK=content`** | None of the three were set, so the cache ran on defaults: unbounded in practice, `mtime` compiler identity (which can serve an object built by a different compiler that shares a timestamp — defeating the pin), and a home-relative path that is wrong inside a container. |
| **The macOS jobs are cached** | See the artifact-safety note above; it is also the run's critical path (18m43s, the longest job in the matrix). **Measured on the universal build, not assumed** (0.2.4): ccache 4.13.6 reports `Cacheable calls: 182 / 182 (100.0%)` — it declines none of the two-`-arch` compilations — and hits go 14.29% cold → 95.60% warm. Quote the phase, not the step total: the **compile phase** — the only part ccache acts on — falls **501s → 46–86s across two warm runs (415–455s, 83–91%)**. The step *total* says 261–398s (41–63%), a far wider spread, because it also carries an LTO link that drifted 130s → 187s → 284s between runs on a phase ccache never sees. |
| **Sanitizers: six more sub-checks, `detect_leaks=1`, a 64 MB stack** | Measured, not transcribed — including the one sub-check deliberately left out and why (ADR-0034 §4). |
| **The MSVC toolset is recorded, its ABI series asserted** | `windows-latest` floats and MSVC is auto-detected, so a shipped `.vst3` was built by a toolset no artifact named. A record plus one narrow assertion, not a pin — MSVC cannot be installed the way apt.llvm.org's Clang can. |
| **`timeout-minutes` on every job** | Without it a hung job burns the 6-hour default. |
| **Toolchain versions printed** | The composite action prints cmake, ninja, the default `c++` and `ld` once per Linux job; macOS prints ninja and cmake; the LTO lanes assert and print their compiler; `setup-llvm-apt.sh` already asserted the Clang major. What the review gate cannot pin, it requires to be detected and recorded. |
| **`ca-certificates` + `python3` named explicitly** | Preinstalled on a runner, not promised in a container — and every checker in `scripts/` is Python. |

## Reproducing CI locally

`scripts/preflight.sh` runs the checkers with their self-tests and then the suites, in CI's own
order. It states what it CANNOT run rather than skipping quietly: the Clang warning gate needs a
build log from the pinned compiler, and the ABI floor needs linked artifacts (it runs for real when
a local Release build has produced them). A green preflight means "the checkers and suites pass",
not "CI will be green".

It runs the citation gate against **three** bases — `origin/main`, the branch's merge base, and
`HEAD~1`. The third is what CI actually compares on a push and is the only one that reads the change
since the last push on a branch with more than one commit.

## Before enabling branch protection — read this

Three trigger designs here interact with **required status checks**, and all bite only once
protection is switched on. None is a defect; all are traps if configured blindly.

1. **`build.yml` on same-repo PRs.** The jobs are skipped by the `preflight` guard above, so they
   report a *skipped* conclusion on the PR event rather than running. GitHub treats a skipped
   required check as satisfied, so this is expected to be fine — but if a required check ever sits
   in a "waiting" state on internal PRs, this guard is the first thing to look at. The build that
   actually validates the commit is the one on the **push** event for the same SHA.

   **What that costs:** the push build validates the *branch head*; the PR-event build would have
   validated the *merge commit*. Those differ, and the difference is exactly what catches a semantic
   merge conflict — code that is fine on both sides but broken once combined. Today nothing merges
   without a fresh push, so the gap is theoretical. It stops being theoretical the moment a merge
   queue or "require branches to be up to date" is enabled, and the dedup should be revisited then.

2. **`codeql.yml`'s check *names* change between phases.** Because the matrix is emitted by
   `preflight`, the job `Analyze (c-cpp)` **does not exist at all** before `CMakeLists.txt` is
   committed — only `Analyze (actions)` does. A required check named after the c-cpp entry would
   therefore block every pre-P1 PR on a check that cannot report. Require `Analyze (actions)` now if
   you want one; add the c-cpp name only once P1 has landed. (This is the dynamic matrix working as
   intended — it is the *naming* that needs care, not the mechanism.)

3. **`codeql.yml` on docs-only PRs — the sharpest one.** Its `paths-ignore` means the workflow is
   **not created at all** for a docs-only PR, so a required `Analyze (c-cpp)` / `Analyze (actions)`
   check has nothing to report and the PR blocks forever. This is a documented GitHub behaviour,
   not a repository bug, and it matters here because docs-only PRs are most of this repository's
   traffic during P0. The standard workaround is a companion no-op workflow declaring jobs with
   the **same names** and the inverse path filter. Add it when — and only when — CodeQL is made
   required; adding it earlier is dead weight.

4. **`cxx23-canary.yml` must never be in the required set at all.** Not a skip/naming subtlety
   like the three above — a standing prohibition: ADR-0008 defines the canary as early warning
   whose failure "must never block the main pipeline", and it runs on a schedule rather than on
   PR events, so requiring it would block every PR on a check that cannot report there even when
   green. If the canary is red, the to-do is a code or toolchain fix (or a deliberate, ADR-gated
   baseline decision) — never "make the check required so someone has to look at it".

## Artifact safety rules (fail-closed)

These are the rules, not incidental details — each blocks a specific way a bad artifact can ship:

- Customer uploads are gated on the self-tests **and** on the public copy having been assembled,
  purged and validated — never `if: always()`. An unstripped or unvalidated binary cannot reach the
  public artifact.
- **The macOS jobs are cached** (0.2.2), and the universal build's cache is **measured** (0.2.4).
  The claim that `dsymutil`'s debug-map walk made caching unsafe was refuted rather than outvoted: a
  cached object is a real `.o` at the path the linker recorded, so the walk reads the same files
  either way. The second objection — that ccache historically REFUSED compilations carrying more
  than one `-arch`, which is exactly what `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"` produces — was
  settled by reading the runner's own statistics rather than the sibling's: ccache **4.13.6** reports
  `Cacheable calls: 182 / 182 (100.0%)` with no `Uncacheable` bucket at all, over three consecutive
  runs. Hit rate goes **14.29% cold → 95.60% warm** with the object count unchanged at 182 — only
  `.ccache` is restored, never `build/`, so ninja issues all 182 invocations every time and the drop
  is cache hits rather than an incremental build. On timing, quote the phase rather than the step
  total: the **compile phase** — the only part ccache acts on — falls **501s → 46–86s across two warm runs (415–455s, 83–91%)**. The step *total* says 261–398s (41–63%), a far wider spread, because it also carries an LTO link that drifted 130s → 187s → 284s between runs on a phase ccache never sees. The `lipo` slice assertion is the backstop against a thin object
  reaching a universal artifact.
- **On Linux the gate also names the two instruments that read the shipped bundle** (0.2.1):
  `AnabasisEngineRepro` and `AnabasisChannelProbe --assert-discriminating`. Before ADR-0032 they ran
  in `linux-clang` against a build that was thrown away, so their outcome could not sensibly gate an
  upload from a different job; now they run against the exact bundle staged into `dist/`. Without
  this, a bundle that dropped a channel — the KI-009 / INC-004 defect these two exist to catch —
  would still have been uploaded, with only the job conclusion turning red afterwards. A *skipped*
  instrument does not satisfy the gate either: the condition asks "did this pass", not "did this not
  fail". Staging itself is deliberately **not** gated on them, so the ABI floor assertion still gets
  a `dist/` to read and still reports; nothing ships from it while the upload is skipped.
- **Two checkpoints, not one step outcome (Windows).** The staging step emits `public_ok=true` as
  soon as the public copy passes its leak check, *before* the PDB retention that follows it, and
  the customer upload gates on that output rather than on the step's overall outcome. `$GITHUB_OUTPUT`
  is read after the step regardless of how it ended, so a later failure cannot retract the
  checkpoint. Without this, a purely **developer-side** symbol problem — no CodeView record, or the
  recorded PDB not uniquely locatable — would also withhold the Windows beta artifact even though
  the public copy was already finished and clean. macOS treats the same class of failure as
  best-effort, so the two platforms otherwise had opposite policies for it.
- **PDB retention stays strict, and that is deliberate.** On macOS a missing dSYM is an *expected*
  consequence of Release+LTO; on Windows `/DEBUG` guarantees a PDB, so its absence means the build
  or the retention logic is wrong and must be seen. The strictness now costs only the `-debug`
  artifact, which is what it is actually protecting.
- `!cancelled()` (rather than plain `success()`) keeps the beta artifact available when *only*
  pluginval failed, while a failed behavioural gate still blocks it.
- The staging step **self-validates** what it just built — but **not equally on the three
  platforms**, and the difference is worth knowing before trusting the phrase:

  | Platform | What the staging check actually proves |
  |---|---|
  | **Linux** | Independent property check: reads ELF section headers to assert no `.symtab` survives, and asserts `nm -D` still shows `GetPluginFactory` — i.e. the *stripped* plugin is still loadable |
  | **macOS** | Independent property check: asserts both `arm64` and `x86_64` slices are present (`lipo`), plus the `.dSYM` leak scan |
  | **Windows** | **Delete-confirmation only**: it re-lists the very extensions the purge just removed, so it can fire only if `Remove-Item` silently failed. Nothing asserts the shipped `.vst3` still exports its entry point |

  Closing the Windows gap is a **P1** item (`TODO(P1)` in `build.yml`): it needs a real binary to
  assert against, and the honest option is the PE export table — not a byte-string search for the
  symbol name, which proves only that the name appears somewhere in the file.
- Locate everything **before** copying anything; purge debug material from the public copy
  **immediately** after the copy and before any step that can abort — so an abort can never leave
  a symbol-bearing public artifact behind.
- Each locate demands **exactly one** match: zero is a build-layout failure, more than one is
  ambiguity that must not be guessed about.
- Developer `-debug` artifacts are preserved even when a *later* step fails (they never contain
  customer-facing binaries) — but each is gated on a `debug_artifacts` output written **last** by
  the step that produces the symbols, not on "that step was not skipped". The debug directory is
  created at the top of those steps, so the weaker gate would fire the upload against an empty
  directory whenever the step aborted part-way, failing a second time on `if-no-files-found` and
  burying the real error.

## Security scanning

| Workflow | Convention |
|---|---|
| `codeql.yml` | `c-cpp` uses build-mode **manual**, not `none`: JUCE arrives via `FetchContent` at configure time, so a bare checkout has no framework headers and a no-build analysis would resolve almost no includes. `paths-ignore: build` keeps the fetched JUCE tree out of the alerts — it is pin-locked and review-gated, so alerts there are unactionable. Docs-only changes skip the workflow. `actions` is analysed with build-mode `none`. |
| `msvc.yml` | A real build is **required** (juceaide generates files the plugin TUs consume). JUCE is treated as external (`ignoredIncludePaths` / `ignoredTargetPaths`). Path-filtered triggers — a full build + `/analyze` pass on a docs change is pure cost. Analyses **Release**, the shipped configuration, so it sees the `NDEBUG` state customers get. |
| `dependency-review.yml` | PRs to `main` only; `comment-summary-in-pr: on-failure` — most PRs change no manifest and an unconditional comment is noise. |
| `dependabot.yml` | Weekly **grouped** `github-actions` bumps (one PR, one CI run per week). JUCE is **not** covered: CMake is not a Dependabot ecosystem, and a JUCE bump is deliberately a manual, review-gated Build System change. |

## Reproducing CI locally

**Two blocks, because the shells genuinely differ.** The POSIX one covers Linux and
macOS; Windows gets its own, since the CI job there drives `run-pluginval.ps1` from
PowerShell and neither `sed` nor `${VAR:?}` is available in a stock `cmd`/PowerShell
environment. Round 43 fixed the strictness lookup for macOS and then described the
result as running "on all three gate platforms", which moved the same defect one
platform along rather than removing it. Both blocks read the number from the same
single authority, `.github/workflows/build.yml`, and neither restates it.

**Linux / macOS:**

```bash
scripts/setup-linux.sh          # Linux only — macOS needs no dependency step
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
scripts/run-tests.sh
# $STRICTNESS is ANABASIS_PLUGINVAL_STRICTNESS, read from the ONE place that
# holds it (.github/workflows/build.yml) rather than pasted: the literal used to
# be `5` here and stayed 5 through two raises, under a comment telling the reader
# it was current.
#
# POSIX `sed`, not `grep -oP`: `-P` and `\K` are GNU extensions that BSD grep —
# /usr/bin/grep on macOS, one of the three platforms this gate is REQUIRED on —
# rejects outright. The failure was silent rather than loud: STRICTNESS came out
# empty and the two commands below ran with no strictness argument at all, so the
# local gate did not match CI. The `^  ` anchor pins the match to the `env:`
# assignment, so the `${{ env.ANABASIS_PLUGINVAL_STRICTNESS }}` references in the
# job steps cannot contribute a second value.
STRICTNESS=$(sed -n 's/^  ANABASIS_PLUGINVAL_STRICTNESS:[[:space:]]*\([0-9][0-9]*\).*/\1/p' \
             .github/workflows/build.yml)
: "${STRICTNESS:?could not read ANABASIS_PLUGINVAL_STRICTNESS from build.yml}"
scripts/run-pluginval.sh "$STRICTNESS" deterministic
scripts/run-pluginval.sh "$STRICTNESS" randomise
```

**Windows** (PowerShell — what the `windows` job itself runs):

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
# There is no `run-tests.ps1`: the `windows` job inlines its binary discovery so
# it can REFUSE an ambiguous multi-config match rather than guess (see the job's
# own comment). Locally the paths are fixed, so run them directly.
build/AnabasisTests_artefacts/Release/AnabasisTests.exe
build/AnabasisStateTests_artefacts/Release/AnabasisStateTests.exe
# Same single authority, read with PowerShell's own regex rather than sed. The
# `^  ` anchor pins the match to the `env:` assignment, so the
# `${{ env.ANABASIS_PLUGINVAL_STRICTNESS }}` references in the job steps cannot
# contribute a second value — the same reasoning as the POSIX block above.
# The match is TESTED before it is indexed. `(Select-String …).Matches[0]`
# dereferences a `$null` when the pattern misses — a renamed env var, a moved
# workflow — and PowerShell then throws a property-not-found error before the
# guard below can say what is actually wrong. Same class of failure the POSIX
# block's `${VAR:?}` exists to prevent.
$m = Select-String -Path .github/workflows/build.yml `
       -Pattern '^  ANABASIS_PLUGINVAL_STRICTNESS:\s*(\d+)'
if (-not $m) { throw 'could not read ANABASIS_PLUGINVAL_STRICTNESS from .github/workflows/build.yml' }
$Strictness = $m.Matches[0].Groups[1].Value
scripts/run-pluginval.ps1 -Strictness $Strictness -Mode deterministic
scripts/run-pluginval.ps1 -Strictness $Strictness -Mode randomise
```
