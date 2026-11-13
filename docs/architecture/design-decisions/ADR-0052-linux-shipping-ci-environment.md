# ADR-0052 — Preserve the Linux shipping ABI across runner rollouts

**Status:** **Accepted** — 2026-11-13, by explicit owner approval. The Architecture Review Gate is **APPROVED and CLEARED** under rule 3 for the shipping runner selection. Implementation CI and merge authorization remain separate; no release is authorized.

## Context

ADR-0031 retains the pinned Clang major 22; ADR-0032 makes Clang/lld the shipping Linux compiler and GCC the compatibility compiler. The Anabasis ABI checker caps GLIBC/GLIBCXX/CXXABI at 2.38/3.4.31/1.3.9.

## Problem

The latest main Build & Validate run failed only the shipping Linux job, in Assert the shipped Linux ABI floor. Both actual VST3 and Standalone staged outputs require GLIBC 2.43 on Ubuntu 26.04.1. The previous passing main job used Ubuntu 24.04.5 and both formats required GLIBC 2.38. Clang reports 22.1.8 in both runs, and GLIBCXX and CXXABI stayed unchanged.

## Options

1. Retry with the floating label: a different allocation might pass, but the existing logs show that the newly selected OS can produce unsupported artifacts.
2. Select the previously measured Ubuntu 24.04 family for the shipping job only: the narrow correction that preserves the established ABI contract and existing gates.
3. Introduce a dedicated shipping container or sysroot: could control userspace, but changes more of the release build and packaging than the measured issue requires.
4. Change compiler, symbol linkage or raise the ABI ceilings: not justified by these results; raising the ceiling changes supported systems and is not the requested repair.

## Decision

Accept option 2: change only the shipping `linux` job to `ubuntu-24.04`, the previously measured compatible runner family. The existing sanitizers, realtime, LTO, merge-check and other jobs are not failing for this reason and retain their runner labels. No product source, dependency, compiler pin, ABI checker, CPU flags, packaging or validation body changes.

## Consequences

The runner label fixes the OS family, not a particular image revision, kernel or package patch. Both real rebuilt shipping formats must pass the existing ABI gate, and the complete workflow must pass normally. Preserve the existing CXXABI 1.3.9 ceiling. Revisit the runner selection if support or the measured ABI changes; a green earlier build does not establish current candidate correctness.

## Related code

- `.github/workflows/build.yml`: shipping `linux` job.
- `scripts/check-linux-abi.py`: unchanged shipped-artifact gate.
- `scripts/setup-llvm-apt.sh`, `CMakeLists.txt`: unchanged compiler and build contracts.

## Evidence + confidence

**Verified historical Anabasis job logs:** [passing job](https://github.com/skyRolly/Anabasis/actions/runs/41856527494/job/126795029467) at `f0578b9041214d5bc285f42e61b4fda7873b4c52`; [failed job](https://github.com/skyRolly/Anabasis/actions/runs/43318795973/job/131533800528) at `d4ec10b92ef90f17b3470a95103b70f2bbd8dceb`. [Investigation record](../../../worklogs/2026-11-13-linux-ci-abi-rollout.md) records the comparison and validation boundaries. The initial corrected workflow passed [Build & Validate](https://github.com/skyRolly/Anabasis/actions/runs/43431890792), including the [Linux shipping job](https://github.com/skyRolly/Anabasis/actions/runs/43431890792/job/131907231265) at `9c997e17264205033e7c3429aa778216d6817fd1`: both formats restored GLIBC 2.38, GLIBCXX 3.4.31 and CXXABI 1.3.9. The owner accepted this decision on 2026-11-13. The final documentation head requires its own CI.
