# Linux shipping CI ABI rollout — 2026-11-13

## Baseline and direct evidence

Main: `d4ec10b92ef90f17b3470a95103b70f2bbd8dceb`. The full [failed Build & Validate run](https://github.com/skyRolly/Anabasis/actions/runs/43318795973) has one failed job: linux, at Assert the shipped Linux ABI floor. Docs, source-lint, preflight, Windows, macOS, macOS Intel, Linux LTO Clang/GCC, realtime and sanitizers succeeded; merge-check and macos-review-selections were skipped. CodeQL and Microsoft C++ Code Analysis also succeeded on that main SHA.

| Direct job-log observation | [Previous pass](https://github.com/skyRolly/Anabasis/actions/runs/41856527494/job/126795029467) | [Current failure](https://github.com/skyRolly/Anabasis/actions/runs/43318795973/job/131533800528) |
|---|---|---|
| Source | f0578b9041214d5bc285f42e61b4fda7873b4c52 | d4ec10b92ef90f17b3470a95103b70f2bbd8dceb |
| Image family / OS | ubuntu-24.04 / 24.04.5 LTS | ubuntu-26.04 / 26.04.1 LTS |
| Clang reported release | 22.1.8 | 22.1.8 |
| VST3 GLIBC / GLIBCXX / CXXABI | 2.38 / 3.4.31 / 1.3.9 | 2.43 / 3.4.31 / 1.3.9 |
| Standalone GLIBC / GLIBCXX / CXXABI | 2.38 / 3.4.31 / 1.3.9 | 2.43 / 3.4.31 / 1.3.9 |
| Standalone ALSA version reference | 1.2.10 | 1.2.13 |
| ABI gate | pass | exit 1, both formats rejected |

The logs measure real staged shipping outputs via the production checker. Individual symbol-level imports were not independently extracted.

## Decision and validation

Change only the shipping runner line, as the failure is at the ABI gate. Retain the CXXABI 1.3.9 ceiling, all flags, checks, tools and dependency pins. The alternatives and consequences are recorded in [Accepted ADR-0052](../docs/architecture/design-decisions/ADR-0052-linux-shipping-ci-environment.md). No product version or release operation.

The candidate must pass complete hosted Build & Validate, including both rebuilt stripped artifacts' ABI checks, the existing full suites, probes, pluginval, warning and staging gates, plus the normal PR checks. A historical pass is not candidate evidence. Independent artifact-level symbol inspection and native/host manual checks are not represented as newly executed. Owner Architecture Review was explicitly approved on 2026-11-13; merge and release authorization remain separate.


## Corrected workflow evidence

The initial corrected source `9c997e17264205033e7c3429aa778216d6817fd1` passed [Build & Validate (push)](https://github.com/skyRolly/Anabasis/actions/runs/43431890792), [Build & Validate (PR)](https://github.com/skyRolly/Anabasis/actions/runs/43431912132), [CodeQL](https://github.com/skyRolly/Anabasis/actions/runs/43431911949) and [Dependency review](https://github.com/skyRolly/Anabasis/actions/runs/43431911764). The [Linux shipping job](https://github.com/skyRolly/Anabasis/actions/runs/43431890792/job/131907231265) ran on Ubuntu 24.04.5 with Clang 22.1.8, passed 795 DSP and 1,638 State checks (zero failures), and measured GLIBC 2.38 / GLIBCXX 3.4.31 / CXXABI 1.3.9 for both staged formats. The ABI checker returned success. The final documentation head requires its own hosted CI; this evidence is scoped to the tested earlier source.
