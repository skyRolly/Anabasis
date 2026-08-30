# 2026-08-30 — Toolchain maintenance round: LLVM 23 re-checked, nothing moved, and one README claim corrected

**Directive.** Re-check the pinned compiler/toolchain state against the latest stable releases, with
particular attention to LLVM 23.1.0 — *"do not assume that condition is still true"* — upgrade
anything with a newer compatible stable release, verify acquisition and role parity end to end,
sweep for stale references, and **do not create a version bump if nothing was upgraded**.

**Outcome.** No toolchain moved. The LLVM 23 block still holds, for the same reason and re-measured
rather than assumed. Two things did change: a stale README claim about the sibling's JUCE pin, and a
correction to a comment this repository's *previous* round had itself got wrong.

---

## 1. LLVM 23.1.0 — re-checked with the mechanism, not the version string

The directive names the four things not to rely on (package version strings, `clang --version`,
dpkg versions, displayed marketing version) — which is the right list, because all four read a
clean `23.1.0` on a build that is not the release. So the check was the repository's own
release-tag mechanism from ADR-0037, run against the live suite:

```
package  : 1:23.1.0~++20260818083557+55feb0a3b6b7-1~exp1~20260818083714.47
upstream : 23.1.0
built    : 55feb0a3b6b7
tag      : ea7d852a70e8bdfaf601d6626a760f9771b2c4b4     (llvmorg-23.1.0^{})
VERDICT  : NOT A RELEASE (refused)
```

Fetched cache-busted (`Cache-Control: no-cache`), all three packages, plus the suite metadata:

| Fact | Value | Change since ADR-0037? |
|---|---|---|
| Suite `Release` date | `Tue, 18 Aug 2026 15:11:28 UTC` | none |
| `clang-23` / `lld-23` / `libclang-rt-23-dev` | all `…+55feb0a3b6b7-…` | none — still one commit, still that one |
| `llvmorg-23.1.0^{}` | `ea7d852a70e8…` | none |
| Newer 23.x tag (23.1.1)? | none exists | — |

**So the upgrade remains deferred, for exactly the recorded reason**: the noble suite has not been
rebuilt since 2026-08-18, and the commit it was built from is `release/23.x` state after the
`llvmorg-23.1.0-rc3` tag and before the release commit. Nothing about the block is speculative and
nothing about it has aged.

**What state would allow the upgrade.** One condition, checkable in a single command: a
`llvm-toolchain-noble-23` suite whose `clang-23` version string ends in the commit that
`llvmorg-23.1.0^{}` resolves to (today `ea7d852a70e8…`). When that is true,
`scripts/setup-llvm-apt.sh 23` stops exiting 1 by itself — the gate is the test, so no separate
judgement call is required. The validation matrix to re-run before flipping the pin is written out
in §§4–8 of `2026-08-30-llvm-23-toolchain-upgrade.md`; it was executed once already against the rc3
build, so it is a re-run rather than a fresh design.

**No validation matrix was run this round**, deliberately: nothing was upgraded, so a matrix run
would measure the tree against the same compiler that produced the last one and report the same
numbers with a fresh date on them. The lint and self-test set *was* run, because this round does
change files — see §6.

## 2. Every other pinned toolchain

| Component | Pinned now | Newest stable | Verdict |
|---|---|---|---|
| **Clang / LLD / compiler-rt** | major **22** (`build.yml` `ANABASIS_CLANG_VERSION`) | 23.1.0 released; **not shipped as a release** by our package source | **HELD** — §1 |
| **GCC** (compatibility lane) | major **16**, floating `gcc:16` container | **16.2** — `gcc.gnu.org` news lists `GCC 16.2 released`; Docker `library/gcc` publishes `16.2.0`, `16.2`, `16`, and **no 17 tag** | **EQUIVALENT** — already the newest stable major; the floating tag collected 16.1 → 16.2 with no commit |
| **CMake** | no pin. `cmake_minimum_required` 3.22 is a **floor**, not a pin; CI uses the runner/distro build | — | **NO PIN EXISTS** |
| **Ninja** | no pin (runner/distro) | — | **NO PIN EXISTS** |
| **ccache** | no pin (apt on Linux, brew on macOS), and explicitly an optimisation the jobs fall back from | — | **NO PIN EXISTS** |
| **pluginval** | deliberately unpinned (`releases/latest`), recorded as a tracked improvement in `DEPENDENCY_POLICY.md` | — | **KEEP** — pinning it is its own decision, not this round's |
| **JUCE** | 9.0.1 at `e18f7f5…` | a dependency, not a toolchain | out of scope; see §5 |
| **GitHub Actions refs** | every `uses:` SHA-pinned with a version comment | Dependabot's `github-actions` ecosystem, weekly | **KEEP** — owned by a mechanism that does it correctly; a hand-edit would need a SHA this environment cannot resolve (`github.com` is 403 for third-party repos here) |
| **MSVC / AppleClang** | runner-image supplied | — | Architecture Review Gate **rule 2**: a version the image supplies can be detected and recorded, not gated |

Nothing in that table is an older version retained because it happens to work. The two real pins are
both already on the newest thing their source ships as a release.

## 3. Acquisition and configuration parity — the whole chain, not the number

Checked link by link, because "the version moved but something older is still being selected" is the
failure this section exists to exclude:

| Link | State |
|---|---|
| Authority | `ANABASIS_CLANG_VERSION: 22` / `ANABASIS_GCC_VERSION: 16`, `build.yml` env — one place |
| Install | `scripts/setup-llvm-apt.sh <major>`: suite read from `/etc/os-release`, signing key pinned **by identity** (exactly one primary fingerprint), three packages in one transaction, `--version` asserted, **and the release-tag assertion** |
| Composite action | `clang-version` input; empty means the job does not use Clang; **fail-closed when set**; values bound as `env` so they are data, not a command position |
| Compiler selection | every `-DCMAKE_CXX_COMPILER` in a Clang job interpolates the env value; the GCC lane's compiler is the container's, with the major asserted from `-dumpversion` |
| Linker | `-fuse-ld=lld` is unversioned by design and resolves to the driver's own major — measured in the previous round: `clang++-23 -print-prog-name=ld.lld` → `/usr/lib/llvm-23/bin/ld.lld`. No mixed-major path |
| Sanitizer runtimes | ASan/UBSan **and** RTSan all come from the one `libclang-rt-<major>-dev` the same script installs |
| Cache | all six Linux lineages put the compiler major in **both** `key:` and `restore-keys:` (five `clang${…}`, one `gcc${…}`). The two macOS keys do not name a compiler — and cannot serve foreign objects anyway, because **all eight cached jobs set `CCACHE_COMPILERCHECK: content`**. Windows is uncached, with its reason recorded |
| LTO | Clang arm `-flto` on compile **and** link, linked by the version-matched lld; GCC arm the same inside the container |

**No link was found where a version could move while an older compiler, runtime or package stayed
selected.**

## 4. Release vs validation roles — read from `build.yml`, not from the docs

| Job | Compiler | Uploads a release artifact? |
|---|---|---|
| `linux` | **pinned Clang** | **yes** — `Anabasis-Linux`, `Anabasis-Linux-debug` |
| `linux-lto-tests` | `g++` in `container: gcc:16` | no |
| `linux-lto-clang` | pinned Clang, `-flto` | no |
| `merge-check`, `sanitizers`, `realtime` | pinned Clang | no |
| `windows` | MSVC (runner) | **yes** — installer, payload, debug |
| `macos` | AppleClang (runner) | **yes** — installer, payload, debug |
| `macos-intel` | AppleClang (runner) | no — thin-x86_64 validation |

So the architecture the directive asked to confirm holds exactly: **Clang ships Linux, GCC is
compatibility-only, and the LTO design is two jobs.** That last point is worth stating precisely
because it is easy to get wrong: `linux-lto-tests` is the **GCC** job and `linux-lto-clang` is the
Clang one — two jobs rather than one matrix because `container:` is a per-job key. ADR-0033 decided
a two-arm matrix and **carries ADR-0034's amendment inline** recording the split; `CI_CD.md`
describes both jobs correctly in four places. Nothing there needed changing.

## 5. Stale references

Two found, one of them mine.

1. **`README.md` §Requirements said the sibling had not followed the JUCE pin** — *"ADR-0028 …
   moved Anabasis to 9.0.1 and Anamorph has not moved, so the two are one patch release apart until
   the sibling follows."* It has followed. Verified in the read-only sibling:
   `ANAMORPH_JUCE_VERSION "9.0.1"`, `ANAMORPH_JUCE_TAG "e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8"`
   (`/home/user/Anamorph/CMakeLists.txt:52-53`) — the same commit this repository pins.
   `DEPENDENCY_POLICY.md` recorded the re-convergence during the 0.2.6 audit; the README was the
   copy that did not get the message, which is the exact failure its own "no count is written here
   on purpose" rule exists to limit. Corrected.
2. **`tests/AllocationGuard.h` said "both arms of `linux-lto-tests`"** — a claim introduced by the
   *previous* round's own stale-reference fix, and wrong for the reason §4 gives. It now names both
   jobs. This is the second time this one comment has been wrong about the LTO lane; it now carries
   the `container:` reason so the next reader does not have to re-derive it.

**Deliberately not touched:** ADR-0033's Decision line ("two matrix arms"), which was correct when
taken and sits under an inline amendment; every dated measurement (`measured on g++ 13.3`,
`clang-22.1.8`); `check-clang-warnings.py`'s CI-run receipt; past CHANGELOG entries; `build.yml`'s
historical `linux-clang` mentions. Rewriting any of those would destroy a record to tidy a string.

## 6. Version bookkeeping — the bump was withdrawn

The comment corrections were first landed as **0.2.8**. That was wrong, and the review that caught
it is right on the policy:

- `CHANGELOG_POLICY.md` rule 3: *"Repository scaffolding and **documentation passes are not
  entries**."* A comment pass is precisely that.
- `CHANGELOG.md`'s own preamble: *"A version entry here means its notes are written, dated and
  complete."* In this repository **a version IS its CHANGELOG entry** — the preamble enumerates the
  versions by listing them. So an entry that policy forbids implies a version that does not exist.
- `RELEASE_POLICY.md` frames its version-bump precondition as one of the things that must hold
  *"before a version ships"*. Bumping `project(... VERSION ...)` for a comment pass asserts a
  release the policy says is not there.

So `CMakeLists.txt`, `CHANGELOG.md`, `HANDOVER.md` and `DOCUMENTATION_COVERAGE.md` were restored to
their merged state byte-for-byte (0 diff lines against `main` each), and the round carries **no
version bump and no CHANGELOG entry**. The record lives in worklogs, which is what worklogs are for.

**The README needed no version edit** — and this is a convention, not an omission.
`DOCUMENTATION_LIFECYCLE_POLICY.md`'s **Ship a version** row does list `README.md (status/version)`,
but that row is not triggered when no version ships; and the README's status section states in its
own words that its `v0.1.0 CODE COMPLETE` line is *"a MILESTONE, not the current version: the tree
has moved on through later rounds, `CMakeLists.txt` carries the version of record and
`HANDOVER.md`'s Release Status row carries whether anything has shipped."* The delegation is the
design. The README edit this round did make is §5's factual correction, which is a different
obligation entirely.

## 7. Validation

Nothing was upgraded, so no compiler matrix was re-run (§1). What this round changes is comments,
one README paragraph and one worklog, and that is what was validated:

| Check | Result |
|---|---|
| `check-docs.py --self-test` / full | 67 cases · **101 files clean** |
| `check-citations.py --self-test` / full | 37 cases · 54 anchors verified against `origin/main` |
| `check-portability.py --self-test` / full | 120 cases · 48 files, **0 violations** |
| `check-realtime.py --self-test` / full | 134 cases · 40 files, **0 violations** |
| `check-clang-warnings.py --self-test` | 18 cases |
| `check-linux-abi.py --self-test` | 19 cases |
| Workflow + composite-action YAML parse | all 5 workflows + the action |
| `bash -n scripts/*.sh` | clean |
| `git diff --check` | clean |
| Edited headers, **pinned** `clang++-22`, `-Werror=function-effects` | compile **silent** |
| Edited headers, `g++ -Wall -Wextra` | compile **silent** |
| `-Wfunction-effects` canary | still fails **by name** — the gate over those headers is live |

## 8. Follow-up

One item, and it is a watch rather than a task: **re-run the §1 check when
`llvm-toolchain-noble-23`'s `Release` date moves off 2026-08-18.** The assertion in
`setup-llvm-apt.sh` makes this safe to get wrong in only one direction — it refuses, it never
accepts an unreleased build — so the cost of not noticing promptly is a delayed upgrade, not a bad
one.
