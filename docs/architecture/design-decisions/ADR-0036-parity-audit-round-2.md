# ADR-0036 — Parity audit round 2: the sibling's measured fixes, adopted; ours, kept

**Status:** Accepted · **Date:** 2026-08-30 · **Amends:** ADR-0034 (the first parity audit).
Resolves the macOS half of OQ-012.

## Context

The owner asked for a complete re-audit of CI/toolchain parity against the sibling's current
`main`, on the standing rule that engineering infrastructure stays aligned unless a product has a
concrete reason to differ. The comparison baseline moved from `2c40a86` (ADR-0034's) to `fd78c3b` —
and the first finding is that the move is empty on the sibling's side: its `.github/` tree,
`setup-linux.sh`, `setup-llvm-apt.sh` and `CMakeLists.txt` are **byte-identical git objects** at
both commits. Everything below is therefore either something ADR-0034's audit missed, or something
Anabasis changed since (0.2.3–0.2.5). The fresh area-by-area read produced 200 comparison rows: 95
equivalent, 105 not.

The brief's specific worry — "GCC 13.x in at least one LTO path" — is false: both LTO arms are
pinned (`gcc:16` container, `clang-22`) and both majors are asserted in-job. The one unpinned
compiler use in Anabasis CI is the sanitizers job's valgrind build, which floats on the runner
default — **exactly as the sibling's does**, so it is parity, not drift; Anabasis additionally
records the resolved version per job through the composite action's toolchain-versions group.

## Adopted from the sibling (each verified here before adoption)

1. **`MALLOC_PERTURB_` moves 255 → 1, and the old comment was wrong.** glibc fills *fresh*
   allocations with `perturb_byte ^ 0xff` and only *freed* blocks with the byte itself. Measured
   directly this round: under 255, a never-written allocation reads `0x00000000` (float `0.0`) —
   the exact benign pattern the variable is set to defeat, indistinguishable from leaving it
   unset; under 1 it reads `0xFEFEFEFE` (`-1.69e38`, loud and wrong-signed). The previous
   comment's claim that "0xFF fills a float buffer with NaN" was true only of *freed*-block reads,
   not the uninitialised-fresh reads KI-009 implicates. The sibling reached the same conclusion by
   its own sweep, which additionally showed **no** value of this variable can produce NaN/Inf/
   denormal fresh fills. Also added to `merge-check`'s self-tests (the sibling has it there); the
   LTO lanes keep it (broader placement than the sibling's — ours, kept).
2. **`ANABASIS_BUILD_NUMBER` becomes a source-file property on `src/gui/PluginEditor.cpp`.** The
   0.2.4 ccache round measured the hazard (58 direct vs 116 preprocessed hits — every TU carrying
   the run-varying `PUBLIC` define was a guaranteed direct-mode miss) and filed the fix as
   proposed-but-gated. The sibling carries the identical arrangement with its own measurement
   (84.4% of compile time was missing its cache per run). Verified here via
   `compile_commands.json`: the define now reaches exactly two compile commands —
   `PluginEditor.cpp` in the plugin target and in `AnabasisStateTests` — and the other 101 TU
   command lines are run-invariant. Both suites pass unchanged (301 + 873, 0 failures).
3. **macOS validates the shipped bytes** (OQ-012's macOS half, resolved in the sibling's proven
   direction): packaging — `dsymutil`, `strip -x`, ad-hoc codesign — runs **before** pluginval;
   all four macOS pluginval gates read `dist/` (`ANABASIS_PLUGINVAL_BUNDLE`, a mechanism
   `run-pluginval.sh` already carried, unused); the AU is registry-installed **from `dist/`**. The
   codesign-last constraint is preserved because the whole packaging step completes before
   validation begins. Windows still validates pre-public-copy bytes; that half of OQ-012 stays
   open.
4. **The x86_64 Rosetta self-test gates the macOS customer uploads** (`id: tests_x86_64`, named in
   the plugins and installer upload conditions). Before, a failing Intel slice reddened the job
   while the universal artifacts it ships in uploaded anyway — the sibling closed exactly this.
5. **The MSVC toolset assert is the job's last step.** Same script both sides; the sibling's
   comment documents abandoning the pre-build slot Anabasis still had it in, where a toolset moved
   off the 14.x series failed the run *before* any build/test/upload evidence existed to judge the
   move by.
6. **ccache observability, 8/8:** `--zero-stats` at configure in the four jobs that lacked it
   (`merge-check`, `linux`, `sanitizers`, `realtime` — their printed stats were lineage-cumulative,
   not per-run) and `--show-stats` steps in the two jobs that cached with no reporting at all
   (`sanitizers`, `realtime`).
7. **`macos-intel` asserts its bundles are thin x86_64** (`lipo`, gating pluginval) and **runs the
   randomise arms** for both formats — the sibling's stated reason holds unchanged: fresh-seed
   values through Intel codegen on an Intel FPU is an (architecture × value) space nothing else
   reaches. The universal job's randomise arms execute arm64; its Rosetta step is
   deterministic-only.
8. **Three GCC-only diagnostics join the GCC LTO lane under the zero gate:** `-Wduplicated-cond
   -Wduplicated-branches -Wlogical-op` — the sibling's gated extras, which our default flag set
   never enabled. Measured before adoption: over both suites they fire 4 times, all in vendored
   JUCE (`juce_NamedPipe_posix.cpp`, `-Wlogical-op` on the EAGAIN/EWOULDBLOCK idiom), zero
   first-party — so the gate stays at zero while the lane now sees every diagnostic the sibling's
   does, under the stricter contract. (`-Wshadow` and `-Wmisleading-indentation`, the sibling's
   other two, already reach this build via JUCE's flags and `-Wall`.)
9. **Smaller adoptions:** the Windows PE staging guard's truncation bound moves +24 → +26 (covers
   the optional-header Magic read; the sibling documents the exact failure), the Windows job
   records *why* it is uncached (/Zi-PDB artifact vs ccache's /Z7-only MSVC mode — decision was
   indistinguishable from omission), and both macOS jobs remove the registry-installed AU after
   its last consumer.

## Kept different — each deliberate, each documented

The zero-warning contract with no baseline files (ADR-0034; the sibling runs a no-NEW-warnings
debt list) · `unsigned-shift-base` excluded (measured; the RNG wrap IS the algorithm) · no fuzz
job (A2-33, owner-confirmed) · `preflight` (owner: its own round) · no `TESTS_NO_FTZ` (no such
failure here) · CXXABI floor 1.3.9 measured vs the sibling's 1.3.14 family ceiling ·
`linux-lto-clang` (INC-004's lane; the sibling's suites never execute under its shipping
compiler's LTO) · the engine-repro/channel-probe instruments and their Linux upload gating · the
0.2.5 per-(target, arch) LTO objects and per-slice dSYM gates · the 0.2.3 package set
(`libfreetype-dev`, explicit `libxi-dev`, webkit out of `headless`) · Windows pluginval running
the GUI tests (green here; KI-007 was the sibling's GPU evidence) · the toolchain-versions record
· `ANABASIS_NO_LTO` · release.yml's no-literal-strictness rule and any-h2 terminator · the
staged-bytes ABI assert · `merge-check`'s targeted build.

## Recorded, not fixed (the sibling is read-only from here)

Its `setup-linux.sh` still names `libfreetype6-dev`, which Debian trixie — the base of its own
`gcc:16` container — does not ship, so its container lane fails at dependency install on next run;
its universal build still shares one `-object_path_lto` path across six links (the ADR-0035
defect class: its arm64 dSYMs are presumably empty today, and its count-only retention and
aggregate compile-unit checks cannot see it); its webkit fallback advice names a package gone from
trixie. And one welcome observation: **the JUCE pins have re-converged** — the sibling's `main`
now pins the same 9.0.1 commit `e18f7f5`, lifting the suspension ADR-0028 recorded
(`DEPENDENCY_POLICY.md` updated).

## Consequences

- The hostile-allocator coverage becomes real: fresh-allocation reads now see a poisoned pattern
  on every glibc lane. A first-party read-before-write that 255 masked would surface as a new
  failure — that is the gate working, not a regression of this change.
- ccache direct-mode hit rates should rise materially on every lane that compiles the plugin
  sources; the stats steps this round completes are what will show it.
- macOS pluginval failures now mean "the shipped bytes failed", and a strip/sign-introduced defect
  fails the gate instead of shipping. pluginval remains deliberately absent from the customer
  upload conditions on every platform (the beta-artifact policy, unchanged).
- **What this does NOT change:** no DSP algorithm, parameter, serialization schema, threading
  model or reported latency; no sanitizer flag; no compiler or optimization flag on any shipped
  artifact. The only first-party file touched outside `.github/` and docs is `CMakeLists.txt`
  (the build-number scoping, output bytes identical).

## Related code
- `.github/workflows/build.yml` (`merge-check`, `linux`, `sanitizers`, `linux-lto-tests`,
  `realtime`, `windows`, `macos`, `macos-intel`)
- `CMakeLists.txt` (`set_source_files_properties` for the build number)
- `docs/OPEN_QUESTIONS.md` (OQ-012), `docs/policies/DEPENDENCY_POLICY.md` (re-convergence)

Evidence [Verified, except where stated]:
- `MALLOC_PERTURB_` fill bytes measured in both directions on this machine (fresh 0x00 under 255,
  0xFE under 1; freed complement)
- Build-number scoping verified via `compile_commands.json` (2 of 103 commands carry the define,
  value delivered); both suites **301 + 873, 0 failures**, including under `MALLOC_PERTURB_=1`
- The three GCC diagnostics measured over both suites: 4 vendored hits, 0 first-party, gate exit 0
  (g++ 13.3 locally — every first-party TU compiled; the `gcc:16` zero-gate in CI is the
  authoritative confirmation on the pinned major)
- All five workflows and the composite action parse; the `macos`/`macos-intel`/`windows` step
  reorderings validated structurally (step lists and gate expressions read back)
- **Unverified locally:** the reordered macOS jobs and the Windows guard end to end (no Apple or
  Windows toolchain here) — CI's next run is the measurement, the posture ADR-0034 set
- Worklog: `worklogs/2026-08-30-parity-audit-round-2.md`
