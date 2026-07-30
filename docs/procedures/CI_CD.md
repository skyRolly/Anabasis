# CI_CD.md

Continuous integration / delivery. Source of truth: `.github/workflows/`.

## Workflows

| Workflow | Purpose |
|---|---|
| `build.yml` | Build + validate on three OSes. The gate. |
| `codeql.yml` | CodeQL analysis (`c-cpp` + `actions`). |
| `msvc.yml` | MSVC `/analyze` → SARIF. |
| `dependency-review.yml` | Dependency Review on PRs to `main`. |
| `release.yml` | **[P6]** tag-triggered draft release. Not present yet. |

## The pre-P1 preflight guard

`build.yml`, `codeql.yml` and `msvc.yml` each start with a `preflight` job that checks whether
`CMakeLists.txt` exists. While the repository is a P0 scaffold with no build, the gated jobs skip
cleanly with a notice instead of failing on a missing project. The guard becomes a permanent no-op
the moment P1 lands — it needs no removal, but it may be removed once the build is real.

**In `codeql.yml` the guard is per matrix *entry*, not per job.** Only `c-cpp` needs a project; the
`actions` entry analyses `.github/workflows/**`, which exist right now and need no build. Gating
the whole matrix would switch **workflow** security scanning off for the entire P0 phase — exactly
the phase in which these workflow files are being written. So `actions` always runs and `c-cpp`
waits for `CMakeLists.txt`.

## `build.yml` — triggers

`push` to any branch (`"**"`), `pull_request`, `workflow_dispatch`, and `workflow_call` (so a
future `release.yml` can reuse the whole matrix with identical gates — tag pushes do not trigger
`build.yml` directly, the `branches` filter excludes tag events). Permissions: `contents: read`.

## Build matrix

| Job | Runner | Builds | pluginval |
|---|---|---|---|
| **linux** | `ubuntu-latest` | VST3 + Standalone (+ tests) | both modes ×3 — **blocking** |
| **windows** | `windows-latest` (MSVC, multi-config) | VST3 + Standalone (+ tests) | both modes ×3 — **blocking** |
| **macos** | `macos-14` (Apple Silicon) | universal VST3 + **AU** + Standalone (+ tests) | both modes ×3 — **blocking** |

Validation is **uniform and blocking on every platform**: there is no `continue-on-error`, so a
non-zero pluginval exit fails the job everywhere.

## Strictness escalates by phase — in one place

```yaml
env:
  ANABASIS_PLUGINVAL_STRICTNESS: 5   # P1–P2: 5 · P3–P5: 8 · P6/release: 10
```

Raising the bar is a one-line edit to `build.yml`. The release gate is **10**
(`docs/policies/TESTING_POLICY.md`).

## Pipeline

**The step order differs by platform, and the difference is deliberate**, so the numbering below
is per-platform rather than a single list.

Common to all three:

1. **Checkout**.
2. **Configure** — `cmake -B build [-G Ninja] -DCMAKE_BUILD_TYPE=Release
   -DANABASIS_BUILD_NUMBER=${{ github.run_number }}` (the run number becomes the About-box build
   number). Windows uses the default VS generator; macOS adds
   `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` and a deployment target.
3. **Build** — `cmake --build build --config Release`.

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

**pluginval** runs deterministic ×3, then randomise ×3. The randomise step is guarded with
`if: ${{ !cancelled() && steps.build.outcome == 'success' }}` so a **deterministic** failure never
*skips* it — both modes always report independently, and the job still fails if either fails —
while a failed **build** does skip it, because there is no plugin to validate and a second red step
about a missing `.vst3` only obscures the real cause.

**Uploads** produce customer artifacts (`Anabasis-<OS>`, loose files) and debug artifacts
(`Anabasis-<OS>-debug`).

## Duplicate-build avoidance

`push: ["**"]` builds every branch, and `pull_request` would rebuild the same SHA once a PR is
open — two full 3-OS matrix runs per commit. The `preflight` job therefore skips **same-repo**
pull_request events, since the push event already covered that SHA. Fork PRs still run: their
push happens in the fork, so the `pull_request` event is the only trigger that sees them.

`codeql.yml` and `msvc.yml` do **not** need the same guard: both are `branches: [main]`-only, so a
feature-branch push cannot double up with its PR.

## `msvc.yml` is doubly inert until P1 — rehearse it on purpose

Its path filters (`src/**`, `tests/**`, `CMakeLists.txt`, its own file) match nothing buildable
yet, so push/PR events cannot start it; the weekly schedule and `workflow_dispatch` do start it and
are then stopped by `preflight`. Two independent no-ops is consistent, but it means the pinned
third-party analysis action has never executed in this repository. **Run it once via
`workflow_dispatch` at P1**, rather than discovering an incompatibility inside the P1 build PR.

## Before enabling branch protection — read this

Two trigger designs here interact with **required status checks**, and both bite only once
protection is switched on. Neither is a defect; both are traps if configured blindly.

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

2. **`codeql.yml` on docs-only PRs — the sharper one.** Its `paths-ignore` means the workflow is
   **not created at all** for a docs-only PR, so a required `Analyze (c-cpp)` / `Analyze (actions)`
   check has nothing to report and the PR blocks forever. This is a documented GitHub behaviour,
   not a repository bug, and it matters here because docs-only PRs are most of this repository's
   traffic during P0. The standard workaround is a companion no-op workflow declaring jobs with
   the **same names** and the inverse path filter. Add it when — and only when — CodeQL is made
   required; adding it earlier is dead weight.

## Artifact safety rules (fail-closed)

These are the rules, not incidental details — each blocks a specific way a bad artifact can ship:

- Customer uploads are gated on the self-tests **and** on the public copy having been assembled,
  purged and validated — never `if: always()`. An unstripped or unvalidated binary cannot reach the
  public artifact.
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
- The staging step **self-validates** what it just built: no symbol table, no `.debug`/`.pdb`/
  `.dSYM` in the public copy, and the plugin entry point still exported after the strip.
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

```bash
scripts/setup-linux.sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
scripts/run-tests.sh
scripts/run-pluginval.sh 5 deterministic     # use the current phase strictness
scripts/run-pluginval.sh 5 randomise
```
