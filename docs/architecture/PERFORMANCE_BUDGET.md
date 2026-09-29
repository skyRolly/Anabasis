# PERFORMANCE_BUDGET.md

The DESIGN §9 budget and its measurements. Target (brief §10): **≈5 % of one modern desktop core
at 48 kHz stereo 4× OS**. The ⊕ allocation below is DESIGN §9's draft; per-stage attribution
needs a profiler pass and is NOT claimed by the whole-engine numbers here.

## Allocation (DESIGN §9 targets), now with measured standalone costs

Measured with the `AnabasisBench` per-stage section (same machine as the matrix below; each module
standalone at 48 kHz base rate in its working configuration, median of 5×1 s runs).
**Re-measured 2026-08-03 after a method fix, and the previous figures were wrong in the unsafe
direction**: the per-stage helper generated its stimulus — an LCG step and a `std::sin` per sample
— INSIDE the timed region, while the header line and the matrix section both promise that stimulus
generation is outside the stamps. A `sinf` is comparable to, and for the cheap stages larger than,
the stage being measured, so every row carried an unlabelled constant overhead in a table used to
argue each allocation row is inside budget. The stimulus is now pre-generated per run outside the
stamps (regenerated each run, because the stage callbacks mutate their frame in place), leaving one
indexed call per sample inside the timed region. EQ moved 0.16 → 0.10 %, Compressor 0.15 → 0.10 %,
Metering 0.18 → 0.10 %; Clipper and Limiter barely moved, which is itself the corroboration — their
own cost dominated the harness. **No verdict changed.** **Standalone cost is not in-chain attribution** — cache locality and inlining differ
inside the running engine, and the region stages (clipper, limiter) execute at the OS rate — so
the whole-engine matrix stays the budget authority; this table answers "is any single stage out
of line with its allocation", and none is:

| Stage | §9 allocation | measured standalone (48 kHz) | verdict |
|---|---|---|---|
| EQ (six sections engaged) | ≤0.3 % | 0.10 % | inside |
| Compressor (RMS + HPF) | ≤0.3 % | **0.24 %** (re-measured 2026-08-09, same bench/machine, after ADR-0023's detector work — the overshoot ceiling adds a compare and a one-pole per sample per channel, running unconditionally so the off→on filter edge cannot start it cold) | inside |
| Clipper/ADAA + colour + tame | ≤0.8 % | 0.21 % (×OS rate in-chain) | inside |
| Limiter + TP detector | ≤1.5 % | 0.37 % (×OS rate in-chain) — re-measured 2026-09-27 | inside |
| Ceiling clamp, true-peak path (ADR-0041, TP mode only) | charged to the row above (ADR-0006: "a second true-peak estimator instance") | **0.51 %** at base rate (107 ns/sample standalone, 2026-09-27) | **over the row in TP mode at ≥ 4×**: limiter ≈ 0.37 % × the OS rate in-chain plus 0.51 % exceeds 1.5 %; the whole-budget case stays inside (3.60 %, below) |
| Metering + features | ≤0.5 % | 0.10 % for one meter + TP + adaptive; the chain runs three meters — ≈0.3 % worst case | inside |
| OS resampling | ≤1.5 % | not separable standalone — the matrix difference (4× working − Off working ≈ 1.5 %) BUNDLES the region stages' rate multiplication, so this row is bounded, not isolated | inside by the bundle bound |
| Headroom | ≥0.1 % | the budget case totals 3.0 % of the ≈5 % target | ample |

## Measured (2026-09-27) — whole engine, `AnabasisBench`

**Re-measured 2026-09-27 under the refresh rule**: the chain gained a stage (ADR-0041's
true-peak path in the ceiling clamp), and the matrix gained a third mode, `working+TP` — the
`working` configuration with `truePeakMode` on, which also engages the limiter's own true-peak
detector below 4× (ADR-0003 item 6). The `working`/`defaults` rows re-measured within the noise of
the 2026-08-02 run (the budget case reads 3.00 % in both).

**Machine:** Intel(R) Xeon(R) Processor @ 2.10 GHz (4 cores), gcc 13.3.0, Linux, Release with the
**bench target's** flag set — see "Build configuration" immediately below, which is NOT identical
to the shipped plugin's and used to be described here as though it were. **Method** (the procedure
Anamorph prescribes and DESIGN §9 commits to): the
OFF-by-default `AnabasisBench` target (`-DANABASIS_BUILD_BENCH=ON`) compiles the engine sources
directly; 5 runs per cell of 1 s audio each (220 Hz tone + noise at ≈−12 dBFS); **ns/sample is
the median over runs of the per-block timed region** (`process()` only — stimulus generation is
outside the stamps); worst block is the maximum single `process()` call; "% of realtime" =
ns/sample × SR / 10⁷. **"Worst block" is the maximum single `process()` call across ALL FIVE
runs**, not the worst block of the run that supplied the median — deliberately the conservative
figure, since a dropout is caused by the worst block that ever happens rather than a typical one,
and correspondingly ~5× more exposed to the scheduler noise the caveat below describes. `working` = the §5.5 macro at loudness ≈ 50 with EQ and colour engaged
(every stage off its exact-skip path); `defaults` = the factory null path.

**Build configuration — where the bench differs from the shipped plugin, stated because the row
above quoted "the shipped flag set" and that was not exact.** `AnabasisBench` links
`AnabasisHardening` + `juce::juce_recommended_config_flags` + `juce::juce_recommended_warning_flags`
(`CMakeLists.txt`), which is the same set the two test apps use. The `Anabasis` plugin target links
one more: **`juce::juce_recommended_lto_flags`**. So these figures come from a Release build
**without LTO**, and additionally **with debug info** — `AnabasisHardening` adds `-g` in Release on
GCC/Clang and `/Zi` + `/DEBUG` on MSVC, which is binary hygiene rather than a codegen change and
does not alter the optimisation level.

The difference is deliberately NOT closed by changing the target, because that would change the
measured results rather than the documentation, and the numbers above would then need re-measuring
on a recorded machine (C2). What it means for reading them: the whole DSP under `src/dsp` is
header-only and reaches `bench.cpp` through `AnabasisEngine.h`, so it is already instantiated inside
the bench's single translation unit — most of what LTO buys is cross-TU inlining the optimiser can
therefore do here anyway. The residual gap between this configuration and the shipped one is
**unmeasured**, so treat these as the bench target's figures rather than as the plugin binary's, and
re-state the flag set if a future round ever adds LTO to the bench.

| SR | block | OS | mode | ns/sample (median) | worst block (us) | % of realtime |
|---|---|---|---|---|---|---|
| 44100 | 64 | Off | defaults | 252.3 | 91.0 | 1.11% |
| 44100 | 64 | Off | working | 292.1 | 312.3 | 1.29% |
| 44100 | 64 | Off | working+TP | 477.4 | 671.4 | 2.11% |
| 44100 | 64 | 4x | defaults | 499.3 | 831.6 | 2.20% |
| 44100 | 64 | 4x | working | 645.7 | 789.6 | 2.85% |
| 44100 | 64 | 4x | working+TP | 771.3 | 830.6 | 3.40% |
| 44100 | 64 | 16x | defaults | 1341.1 | 449.6 | 5.91% |
| 44100 | 64 | 16x | working | 1944.2 | 744.8 | 8.57% |
| 44100 | 64 | 16x | working+TP | 2040.4 | 506.8 | 9.00% |
| 44100 | 512 | Off | defaults | 234.5 | 194.9 | 1.03% |
| 44100 | 512 | Off | working | 278.7 | 221.9 | 1.23% |
| 44100 | 512 | Off | working+TP | 452.3 | 410.0 | 1.99% |
| 44100 | 512 | 4x | defaults | 476.4 | 307.9 | 2.10% |
| 44100 | 512 | 4x | working | 625.9 | 506.6 | 2.76% |
| 44100 | 512 | 4x | working+TP | 864.6 | 2480.0 | 3.81% |
| 44100 | 512 | 16x | defaults | 1326.1 | 1856.5 | 5.85% |
| 44100 | 512 | 16x | working | 1935.6 | 1472.9 | 8.54% |
| 44100 | 512 | 16x | working+TP | 2022.5 | 1316.2 | 8.92% |
| 48000 | 64 | Off | defaults | 243.3 | 90.0 | 1.17% |
| 48000 | 64 | Off | working | 282.1 | 120.5 | 1.35% |
| 48000 | 64 | Off | working+TP | 463.7 | 367.7 | 2.23% |
| 48000 | 64 | 4x | defaults | 491.6 | 85.7 | 2.36% |
| 48000 | 64 | 4x | working | 644.1 | 104.0 | 3.09% |
| 48000 | 64 | 4x | working+TP | 776.2 | 805.6 | 3.73% |
| 48000 | 64 | 16x | defaults | 1341.2 | 720.3 | 6.44% |
| 48000 | 64 | 16x | working | 1943.9 | 1707.3 | 9.33% |
| 48000 | 64 | 16x | working+TP | 2038.4 | 602.1 | 9.78% |
| 48000 | 512 | Off | defaults | 234.1 | 279.8 | 1.12% |
| 48000 | 512 | Off | working | 278.2 | 458.4 | 1.34% |
| 48000 | 512 | Off | working+TP | 452.4 | 406.2 | 2.17% |
| 48000 | 512 | 4x | defaults | 485.7 | 714.8 | 2.33% |
| 48000 | 512 | 4x | working | 624.2 | 709.5 | 3.00% |
| 48000 | 512 | 4x | working+TP | 750.3 | 551.9 | 3.60% |
| 48000 | 512 | 16x | defaults | 1338.2 | 1707.2 | 6.42% |
| 48000 | 512 | 16x | working | 1952.8 | 2168.6 | 9.37% |
| 48000 | 512 | 16x | working+TP | 2094.7 | 2653.9 | 10.05% |
| 96000 | 64 | Off | defaults | 250.2 | 148.0 | 2.40% |
| 96000 | 64 | Off | working | 284.2 | 99.9 | 2.73% |
| 96000 | 64 | Off | working+TP | 462.8 | 127.2 | 4.44% |
| 96000 | 64 | 4x | defaults | 502.5 | 1678.8 | 4.82% |
| 96000 | 64 | 4x | working | 700.2 | 2981.6 | 6.72% |
| 96000 | 64 | 4x | working+TP | 775.4 | 151.6 | 7.44% |
| 96000 | 64 | 16x | defaults | 1347.8 | 1059.8 | 12.94% |
| 96000 | 64 | 16x | working | 1975.3 | 1560.0 | 18.96% |
| 96000 | 64 | 16x | working+TP | 2230.7 | 2219.0 | 21.41% |
| 96000 | 512 | Off | defaults | 256.9 | 256.4 | 2.47% |
| 96000 | 512 | Off | working | 305.5 | 293.4 | 2.93% |
| 96000 | 512 | Off | working+TP | 460.3 | 704.0 | 4.42% |
| 96000 | 512 | 4x | defaults | 477.8 | 467.5 | 4.59% |
| 96000 | 512 | 4x | working | 625.8 | 782.5 | 6.01% |
| 96000 | 512 | 4x | working+TP | 752.9 | 1313.8 | 7.23% |
| 96000 | 512 | 16x | defaults | 1311.5 | 934.6 | 12.59% |
| 96000 | 512 | 16x | working | 1886.4 | 1355.8 | 18.11% |
| 96000 | 512 | 16x | working+TP | 1998.7 | 1395.5 | 19.19% |


## Verdict against the target

- **The budget case — 48 kHz · 512 · 4× · working — measures 3.0 % of one core** on this 2.1 GHz
  server-class Xeon; the target is ≈5 % on a *modern desktop* core, so it holds with margin and
  would improve on the reference hardware. **With true-peak mode on it measures 3.6 %** (2026-09-27,
  ADR-0041): still inside the target, and the one cell where the §9 per-stage row is exceeded (see
  the allocation table). TP mode is off by default (ADR-0015). The TP-mode cost is per BASE sample,
  so it doubles with the rate: +1.5 % at 96 kHz / OS Off. CI-class variance applies: these are wall-clock stamps
  on a shared machine — re-measure before quoting anywhere externally.
- 16× is the deliberate quality extreme, not the budget case: 9.2 % at 48 kHz, 20.2 % at
  96 kHz/512/working. Usable, and honest to state.
- The defaults column is the null path: ≈1–1.3 % at 48 kHz — the bit-exact identity chain plus
  metering/features. The per-stage table above supersedes the earlier "unclaimed" note: every
  allocation row now has a measured standalone figure under it.
- Worst-block figures include scheduler noise (a 64-sample block stamped at 1.8 ms on a shared
  Xeon is a preemption, not DSP) — treat the median column as the load-bearing one.

## Refresh rule

Re-run `AnabasisBench` and replace the table whenever the chain gains a stage, an OS mode
changes, or before any release claim; the machine line travels with the table (C2 — a number
without its machine and method is not a measurement).
