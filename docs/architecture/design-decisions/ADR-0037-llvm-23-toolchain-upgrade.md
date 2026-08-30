# ADR-0037 — LLVM 23.1.0 is released; what apt ships for it is not, so the pin holds and the install learns to tell the difference

> **✅ THE ARCHITECTURE REVIEW GATE IS CLEARED (2026-08-30).** A **Build System change** — the
> fail-closed install of a pinned compiler gains an assertion — under
> `ARCHITECTURE_REVIEW_GATE.md` §"Compiler and toolchain versions" **rule 1**. Taken on the owner's
> directive to *"update the current project to the latest stable released toolchain versions"*,
> naming LLVM 23.1.0 **and** requiring that release candidates, nightlies, development snapshots
> and unreleased versions be refused. The two clauses conflicted in fact, and this ADR is where
> that was discovered and settled.

**Status:** **Accepted — 2026-08-30.** Version 0.2.7. **Amends [ADR-0031](ADR-0031-clang-toolchain-pin.md)** decision clauses 1 and 2. `ANABASIS_CLANG_VERSION` stays `22`.

## Context

The directive names 23.1.0 and forbids unreleased versions. Both were taken literally, in that
order: **is 23.1.0 released, and is what we would install actually it?**

The first question answers cleanly. `llvmorg-23.1.0` is an annotated tag whose commit is
`ea7d852a70e8…`; `releases.llvm.org` and `llvm.org` both carry 23.1.0 as the current release.
apt.llvm.org publishes `llvm-toolchain-noble-23` with all three packages this repository installs.
Every gate in the tree passes on it — the measurements are in §Evidence and they are not
provisional.

The second question does not.

## Problem

**apt.llvm.org's noble suite for major 23 does not ship the release.** `clang-23` is
`1:23.1.0~++20260818083557+55feb0a3b6b7-1~exp1~…`, and that trailing `55feb0a3b6b7` is the upstream
commit it was built from. That commit is **not** `llvmorg-23.1.0`. It sits on `release/23.x` after
`llvmorg-23.1.0-rc3` (`7196f931f212…`) and before the release commit, and it still carries:

```
$ curl .../llvm-project/55feb0a3b6b7/cmake/Modules/LLVMVersion.cmake
  set(LLVM_VERSION_MAJOR 23)   set(LLVM_VERSION_MINOR 1)
  set(LLVM_VERSION_PATCH 0)    set(LLVM_VERSION_SUFFIX -rc3)
```

against the release commit's `set(LLVM_VERSION_SUFFIX)` — empty.

**And nothing local says so.** Debian's packaging drops the suffix:

```
$ clang-23 --version                 → Ubuntu clang version 23.1.0 (++20260818083557+55feb0a3b6b7-…)
$ clang-23 -dM -E - | grep version   → #define __clang_version__ "23.1.0 (++2026…)"
$ dpkg-query -W -f='${Version}'      → 1:23.1.0~++20260818083557+55feb0a3b6b7-…
```

Three independent reads, all saying `23.1.0`. `setup-llvm-apt.sh`'s
`grep -qE "clang version ${MAJOR}\."` passed. `build.yml`'s `-dumpversion` major assertion passed.
The warning gate, both suites, ASan+UBSan, the LTO lane and RealtimeSanitizer all passed. **A
release-candidate toolchain went through every check this repository has and came out clean**,
because every one of those checks is major-only.

The contrast that proves the mechanism rather than the accident: `clang-22` is
`…+ca7933e47d3a-…`, and `llvmorg-22.1.8` peels to `ca7933e47d3a3451…`. The pin in force **is**
built from its release tag. The scheme is fine; major 23's suite simply has not been rebuilt since
the release.

## Options

- **A. Move to 23 anyway — every gate is green.** Rejected. The directive forbids release
  candidates in as many words, and "all our checks passed" is exactly the evidence that would be
  produced either way. Greenness cannot distinguish the two cases; that is the finding.
- **B. Move to 23 and add the assertion afterwards.** Rejected as incoherent: the assertion's whole
  purpose is to refuse this build, so it would land red.
- **C. Hold at 22, add the assertion, and record that 23 is pre-validated.** **Chosen.**
- **D. Hold at 22 and write a comment.** Rejected — a comment would not have caught this one, and
  the next person to take this directive would repeat the whole investigation.

## Decision

1. **`ANABASIS_CLANG_VERSION` stays `22`.** Not because 22 is newest — 23.1.0 is — but because 22
   is the newest major apt.llvm.org ships **as a release** for this distribution. ADR-0031's
   "upstream stable" is amended to mean exactly that; the ambiguity is what let this through.
2. **`scripts/setup-llvm-apt.sh` asserts the RELEASE TAG.** It reads the upstream version and
   commit out of the installed package's own version string, resolves `llvmorg-<version>` with a
   single `git ls-remote` (no clone, one ref), and requires the package's commit to be the tag's
   commit. Fail-closed like the rest of the script, retried like its other network steps, and
   unreachable-github fails rather than assuming — *"could not check"* and *"checked and it is a
   release"* must not look the same.
3. **The 23 measurements are recorded, with their exact provenance.** They were taken on
   `55feb0a3b6b7` — release-branch state shortly before the release commit — so they are strong
   evidence *this tree is ready for 23* and are **not** a measurement of 23.1.0. When the suite
   rebuilds at the tag, re-run them against the real release and then flip the pin; the recipe is
   written down, which is what makes that cheap.
4. **The sibling parity argument is demoted, not invoked.** Anamorph also pins 22, and this round
   deliberately does **not** lean on that: had 23 been shipped as a release, this repository would
   have moved alone and recorded the divergence. Matching is a consequence here, not a reason.

## Consequences

- **Every Clang job now depends on github.com** for one `git ls-remote`. That is a new network
  domain for `setup-llvm-apt.sh`, declared in its header. A github outage now fails the Clang jobs
  — the same fail-closed trade the script already makes for apt.llvm.org, and the same reasoning:
  proceeding on an unverified compiler is the worse outcome.
- **A future major bump costs one extra check and cannot silently take an RC.** The failure mode
  this ADR found is invisible to every other check in the repository, so nothing else covers it.
- **This will fire again**, and that is intended: any major whose suite is mid-release-cycle is
  refused with a message naming both commits, until upstream rebuilds.
- **The directive was not fully satisfiable, and the residual is stated rather than papered over.**
  The pin did not move. What moved is the repository's ability to know whether it should.

## Related code
- `.github/workflows/build.yml` (`env.ANABASIS_CLANG_VERSION`, the pin-rationale block above the jobs)
- `scripts/setup-llvm-apt.sh` (the release-tag assertion at the foot; header rationale)
- `docs/policies/DEPENDENCY_POLICY.md` (the Clang row), `docs/procedures/BUILD.md`

## Evidence [Verified]

Measured on Ubuntu 24.04 (noble) — the distribution `ubuntu-latest` resolves to. Full trail:
[`worklogs/2026-08-30-llvm-23-toolchain-upgrade.md`](../../../worklogs/2026-08-30-llvm-23-toolchain-upgrade.md).

- **23.1.0 is released.** `git ls-remote --tags` on `llvm/llvm-project`:
  `refs/tags/llvmorg-23.1.0^{}` → `ea7d852a70e8bdfaf601d6626a760f9771b2c4b4`. `releases.llvm.org`
  lists 23.1.0 above 22.1.8; `llvm.org` carries it as the current release. (The two pages disagree
  on the date — `25 Jul 2026` vs `Aug 2026` — which is recorded and immaterial: the tag settles it.)
- **What apt ships for 23 is not that.** Suite `Release` dated 2026-08-18 (re-fetched
  cache-busted); `clang-23` built from `55feb0a3b6b7`; that commit's `LLVM_VERSION_SUFFIX` is
  `-rc3`; `llvmorg-23.1.0-rc3^{}` is `7196f931f212…`, a different commit again, so the build is
  post-rc3 branch state.
- **The 22 pin passes the same test**: built from `ca7933e47d3a`, and `llvmorg-22.1.8^{}` is
  `ca7933e47d3a3451…`.
- **The assertion discriminates, through the real script, in both directions.**
  `./scripts/setup-llvm-apt.sh 23` exits **1** naming both commits;
  `./scripts/setup-llvm-apt.sh 22` exits **0** with
  `clang-22 is the 22.1.8 release (built from ca7933e47d3a)`. Both were real installs. The accept
  path is part of the evidence: a gate that only ever refuses is indistinguishable from a broken one.
- **23 itself is sound, and this is why the hold is about packaging and not about the compiler.**
  On `clang-23.1.0` (the rc3 build): warning gate **zero** first-party (426 vendored, ungated);
  suites **301 + 873**; ASan+UBSan **300 + 873** under `detect_leaks=1` and `halt_on_error=1` with
  **zero** special-case-list diagnostics; LTO lane **301 + 873** linked by `Ubuntu LLD 23.1.0`;
  RTSan canary aborts at exit 43, `-Wfunction-effects` canary fails by name, full DSP suite
  **296, 0 failures**. Lints: portability 120/0 · realtime 134/0 · docs 67/100 clean · citations 37
  · clang-warnings 18.
- **GCC examined and not moved:** `gcc.gnu.org` (16.2 newest released, 17.0 in development);
  Docker Hub `library/gcc` (`16.2.0`, `16.2`, `16`; no 17 tag).
- **Not measured here (residual):** the `gcc:16` container arm (no Docker in this environment; its
  compiler did not move), and the assertion's behaviour on a CI runner, which the first run proves.
