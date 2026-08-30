# Parity audit round 2 — the sibling stood still; both trees had moved

**Date:** 2026-08-30 · **Version:** 0.2.6 · **ADR:** ADR-0036 · **Baselines:** Anamorph
`origin/main` `fd78c3b` vs Anabasis `f3dc48e` (0.2.5)

## The first finding is about the baseline

Anamorph gained six commits since ADR-0034's audit (`2c40a86`), and its CI surface gained
**nothing**: `.github/` (tree object `6b34680`), `setup-linux.sh`, `setup-llvm-apt.sh` and
`CMakeLists.txt` are byte-identical git objects at both commits. The new commits are DSP/perf work
(A7 rounds). So this audit's divergences are, by construction, either misses of the first audit or
Anabasis's own 0.2.3–0.2.5 movement — and the fresh area-by-area read (seven lanes over both
trees, 200 rows: 95 same, 105 not) found plenty of both.

## The brief's named checks

| Check | Verdict |
| --- | --- |
| "GCC 13.x in at least one LTO path" | **False.** Both LTO arms pinned: `gcc:16` container (major asserted in-job) and `clang-22` (asserted at install and in-job). The one unpinned compiler use is the sanitizers job's valgrind build — runner default, non-LTO — and the **sibling's valgrind build is unpinned identically**, so that is parity. Anabasis alone records the resolved version per job (composite action's toolchain group). |
| GCC role | Compatibility only, both sides; ships nothing. |
| Clang acquisition | `setup-llvm-apt.sh` functionally identical: same key fingerprint pinned by identity, same `clang-N lld-N libclang-rt-N-dev`, same asserts. |
| Runners / Actions / permissions / triggers | Same five workflows, same images, same SHA pins, same permissions/concurrency. codeql cron sits on a different weekday (harmless stagger, kept). |
| C++ standard / CMake / Ninja | Parity (C++23 REQUIRED, no extensions). |

## Migrated this round (9 items, each verified before landing)

1. **`MALLOC_PERTURB_` 255 → 1** in `linux` and both LTO lanes, plus new on `merge-check`.
   Measured here: glibc fills **fresh** allocations with `perturb ^ 0xff` — under 255 a
   never-written buffer reads `00 00 00 00` (float 0.0, benign, same as unset); under 1 it reads
   `0xFE` bytes (−1.69e38, loud). Our comment's "0xFF fills a float buffer with NaN" described
   freed-block reads, not the uninitialised-fresh reads KI-009 implicates. The sibling's own sweep
   also shows no value can produce NaN fresh fills. Suites pass under the new value.
2. **`ANABASIS_BUILD_NUMBER` → `set_source_files_properties(src/gui/PluginEditor.cpp …)`**,
   removed from both `PUBLIC` blocks. The 0.2.4 round measured the hazard (58 direct / 116
   preprocessed hits) and deferred the fix behind the review gate; the sibling carries the
   identical arrangement with its own number (84.4% of compile time cache-missing per run).
   Verified via `compile_commands.json`: exactly 2 of 103 compile commands carry the define
   (PluginEditor.cpp in both consumers), value delivered; 101 command lines are now run-invariant.
3. **macOS validates the shipped bytes** — resolves OQ-012's macOS half by adoption. Packaging
   (dsymutil → `strip -x` → codesign) moved before pluginval; all four gates read `dist/` via
   `ANABASIS_PLUGINVAL_BUNDLE` (already supported by `run-pluginval.sh`, unused until now); the AU
   registry-install copies from `dist/`. Codesign-last preserved (packaging completes, seal
   included, before validation). The engine-repro and channel-probe steps keep their build-tree
   provenance — which post-packaging is the same stripped, signed bytes, since the packaging step
   strips and signs in place before copying.
4. **`tests_x86_64` gates the macOS uploads.** The Rosetta step had no `id`; a failing Intel slice
   reddened the job while the universal artifacts uploaded. Now named in both upload conditions.
5. **MSVC toolset assert → last step of the windows job.** The sibling's comment documents
   abandoning exactly the pre-build slot we had it in: an ABI-series move there kills the run
   before any evidence exists to judge the move by.
6. **ccache zero+print on 8/8 cached jobs**: `--zero-stats` added to `merge-check`, `linux`,
   `sanitizers`, `realtime` (their stats were lineage-cumulative); stats steps added to
   `sanitizers` and `realtime` (cached with zero reporting — the 0.2.4 recommendation, now done).
7. **`macos-intel`: thin-slice `lipo` assertion** (`id: thin`, gating pluginval — the job's
   "native Intel" label was previously earned by the configure line alone) **and randomise arms**
   for VST3 + AU (sibling's rationale: fresh-seed values through Intel codegen on an Intel FPU is
   an architecture × value space nothing else reaches; our universal job's randomise arms execute
   arm64). 6 Intel pluginval gates → matches the sibling's 12-pass shape at our step count.
8. **Three GCC-only diagnostics** — `-Wduplicated-cond -Wduplicated-branches -Wlogical-op` — join
   the GCC LTO lane's CXX flags **under the zero gate** (no baseline). Measured first: over both
   suites they fire 4 times, all vendored (`juce_NamedPipe_posix.cpp`, `-Wlogical-op` on
   `EWOULDBLOCK || EAGAIN` — equal values on Linux), zero first-party. The lane now sees every
   diagnostic the sibling gates, under the stricter contract. (`-Wshadow`,
   `-Wmisleading-indentation` already reach the build via JUCE's flags / `-Wall`.)
9. **Small adoptions:** Windows PE truncation guard +24 → +26 (the +24 bound admitted a file
   truncated at the optional-header Magic and threw a raw IndexOutOfRangeException); the Windows
   job's uncached state now carries its reason (/Zi-PDB artifact vs ccache's /Z7-only MSVC mode);
   both macOS jobs remove the registry-installed AU after its last consumer. Plus one
   Anabasis-internal repair this audit surfaced: the sanitizers job header still said
   `detect_leaks=0` while the step has set 1 since 0.2.2.

## Kept different — re-affirmed item by item

| Divergence | Why it stays |
| --- | --- |
| Zero-warning gate, no baseline files | 0.2.0/0.2.2 decision, owner-signed; stricter than the sibling's no-NEW-warnings debt lists. Corollary accepted: no baseline→major coupling, so the majors are asserted in-job instead. |
| `unsigned-shift-base` excluded | Measured: one hit, the frozen xorshift dither RNG, wrap IS the algorithm (DSP_POLICY). |
| No `fuzz` job | A2-33 declined on coverage; owner-confirmed "do not change fuzz decisions". Sibling's remains release-blocking there. |
| `preflight` job | Owner: removal is its own round. |
| No `TESTS_NO_FTZ` | Valgrind lane green without it; a relaxation for a failure that does not occur weakens the gate. |
| CXXABI floor 1.3.9 vs sibling's 1.3.14 | Ours is the measured requirement; the sibling declares the runtime-family ceiling. Documented in `check-linux-abi.py` on both sides. |
| `linux-lto-clang` | INC-004's lane; the sibling's own comment concedes its suites never execute under the shipping compiler's LTO. |
| Engine repro + channel probe (+ Linux upload gating) | Instruments the sibling lacks entirely. |
| 0.2.5 per-(target, arch) LTO objects, per-slice dSYM gates | The sibling still has the collision and the aggregate-count blindness. |
| 0.2.3 package set | `libfreetype-dev` + explicit `libxi-dev` + webkit out of `headless` + distro record. |
| Windows pluginval runs GUI tests | Green here across every run; KI-007 was the sibling's GPU-runner evidence, not ours. |
| macOS ccache `MALLOC…` placement / bench-in-linux / no DspDump / targeted merge-check / staged-bytes ABI assert / `NO_LTO` / release.yml rules / preflight-guarded codeql & msvc | Each documented in ADR-0034/0.2.x rounds; the audit found no reason to reopen any. |

## Recorded about the sibling (read-only from here)

- Its container lane installs `libfreetype6-dev`; Debian trixie ships no such package, so the lane
  dies at dependency install on its next run.
- Its universal build shares one `-object_path_lto` path across six links; its retention and dSYM
  checks are the count-only/aggregate forms ADR-0035 documents as unable to see the resulting
  empty arm64 dSYMs.
- Its webkit fallback advice names `libwebkit2gtk-4.0-dev`, gone from trixie.
- **Its JUCE pin re-converged with ours** (9.0.1 `e18f7f5`), lifting ADR-0028's suspension —
  recorded in `DEPENDENCY_POLICY.md`.

## Validation

| Gate | Result |
| --- | --- |
| Both suites, GCC 13.3 Release, with the three new diagnostics | **301 + 873, 0 failures**; 4 vendored warnings, 0 first-party, gate exit 0 |
| DSP suite under `MALLOC_PERTURB_=1` | **301, 0 failures** |
| State suite under `MALLOC_PERTURB_=1` | **873, 0 failures** |
| Build-number scoping | 2 of 103 compile commands carry the define; value `777` delivered in a probe configure |
| Fill-byte measurement | fresh: `0x00` under 255, `0xFE` under 1; freed: complement — both directions |
| All five workflows + composite action | parse; job/step structures read back correct |
| Self-tests | check-docs 67 · citations 37 · portability 120 · realtime 134 · clang-warnings 18 |
| Repo lints | portability 0 violations · realtime 0 violations · docs 97 files clean |

**Unverified locally, stated:** the reordered `macos`/`macos-intel` jobs and the Windows guard
end to end — no Apple or Windows toolchain in this environment. CI's next run is the measurement;
the per-slice dSYM gates and the thin assert are among what will report.
