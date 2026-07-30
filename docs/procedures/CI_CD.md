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
`CMakeLists.txt` exists and gates every build job on the result. While the repository is a P0
scaffold with no build, the jobs skip cleanly with a notice instead of failing on a missing
project. The guard becomes a permanent no-op the moment P1 lands — it needs no removal, but it may
be removed once the build is real.

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

## Pipeline (per job)

1. **Checkout**.
2. **Configure** — `cmake -B build [-G Ninja] -DCMAKE_BUILD_TYPE=Release
   -DANABASIS_BUILD_NUMBER=${{ github.run_number }}` (the run number becomes the About-box build
   number). Windows uses the default VS generator; macOS adds
   `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` and a deployment target.
3. **Build** — `cmake --build build --config Release`.
4. **Self-tests** — `scripts/run-tests.sh` runs `AnabasisTests` **and** `AnabasisStateTests`
   fail-closed (Linux/macOS); on Windows the runner locates and runs both `.exe`s, propagating the
   first failing exit code.
5. **Symbol handling** — Linux extracts split debug info (`objcopy --only-keep-debug`), strips the
   shipped binaries (`strip --strip-unneeded`, `.gnu_debuglink` embedded) and asserts the VST3
   entry point is still exported. This runs **before** pluginval so the gate validates the exact
   stripped bytes users receive. macOS runs `dsymutil` → `strip -x` → ad-hoc codesign, in that
   order (stripping after signing would invalidate the seal); dSYM capture is best-effort and
   never blocks the customer pipeline. Windows retains the linker PDB into a separate debug
   artifact and purges all debug material from the public copy.
6. **pluginval** — deterministic ×3, then randomise ×3. The randomise step is guarded with
   `if: ${{ !cancelled() }}` so a deterministic failure never *skips* it: both modes always report
   independently, and the job still fails if either fails.
7. **Stage + upload** — customer artifacts (`Anabasis-<OS>`, loose files) and debug artifacts
   (`Anabasis-<OS>-debug`).

## Artifact safety rules (fail-closed)

These are the rules, not incidental details — each blocks a specific way a bad artifact can ship:

- Customer uploads are gated on the self-tests **and** the staging/strip step having **succeeded**
  — never `if: always()`. An unstripped or unvalidated binary cannot reach the public artifact.
- `!cancelled()` (rather than plain `success()`) keeps the beta artifact available when *only*
  pluginval failed, while a failed behavioural gate still blocks it.
- The staging step **self-validates** what it just built: no symbol table, no `.debug`/`.pdb`/
  `.dSYM` in the public copy, and the plugin entry point still exported after the strip.
- Locate everything **before** copying anything; purge debug material from the public copy
  **immediately** after the copy and before any step that can abort — so an abort can never leave
  a symbol-bearing public artifact behind.
- Each locate demands **exactly one** match: zero is a build-layout failure, more than one is
  ambiguity that must not be guessed about.
- Developer `-debug` artifacts are preserved even when a later step fails (they never contain
  customer-facing binaries).

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
