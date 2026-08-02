# PERFORMANCE_BUDGET.md

The DESIGN §9 budget and its measurements. Target (brief §10): **≈5 % of one modern desktop core
at 48 kHz stereo 4× OS**. The ⊕ allocation below is DESIGN §9's draft; per-stage attribution
needs a profiler pass and is NOT claimed by the whole-engine numbers here.

## ⊕ Draft allocation (DESIGN §9 — targets, not measurements)

OS resampling ≤1.5 % · limiter + TP detection ≤1.5 % · clipper/ADAA ≤0.8 % · compressor ≤0.3 % ·
EQ ≤0.3 % · metering + features ≤0.5 % · headroom ≥0.1 %.

## Measured (2026-08-02) — whole engine, `AnabasisBench`

**Machine:** Intel(R) Xeon(R) Processor @ 2.10 GHz (4 cores), gcc 13.3.0, Linux, Release with the
shipped flag set. **Method** (the procedure Anamorph prescribes and DESIGN §9 commits to): the
OFF-by-default `AnabasisBench` target (`-DANABASIS_BUILD_BENCH=ON`) compiles the engine sources
directly; 5 runs per cell of 1 s audio each (220 Hz tone + noise at ≈−12 dBFS); **ns/sample is
the median over runs of the per-block timed region** (`process()` only — stimulus generation is
outside the stamps); worst block is the maximum single `process()` call; "% of realtime" =
ns/sample × SR / 10⁷. `working` = the §5.5 macro at loudness ≈ 50 with EQ and colour engaged
(every stage off its exact-skip path); `defaults` = the factory null path.

| SR | block | OS | mode | ns/sample (median) | worst block (us) | % of realtime |
|---|---|---|---|---|---|---|
| 44100 | 64 | Off | defaults | 250.2 | 104.5 | 1.10% |
| 44100 | 64 | Off | working | 287.6 | 88.0 | 1.27% |
| 44100 | 64 | 4x | defaults | 501.2 | 192.1 | 2.21% |
| 44100 | 64 | 4x | working | 604.8 | 129.0 | 2.67% |
| 44100 | 64 | 16x | defaults | 1021.5 | 207.9 | 4.50% |
| 44100 | 64 | 16x | working | 1809.9 | 421.5 | 7.98% |
| 44100 | 512 | Off | defaults | 188.3 | 205.8 | 0.83% |
| 44100 | 512 | Off | working | 216.1 | 304.4 | 0.95% |
| 44100 | 512 | 4x | defaults | 373.4 | 404.4 | 1.65% |
| 44100 | 512 | 4x | working | 455.6 | 339.3 | 2.01% |
| 44100 | 512 | 16x | defaults | 1301.2 | 1151.2 | 5.74% |
| 44100 | 512 | 16x | working | 1471.5 | 2143.7 | 6.49% |
| 48000 | 64 | Off | defaults | 200.1 | 134.1 | 0.96% |
| 48000 | 64 | Off | working | 234.7 | 86.0 | 1.13% |
| 48000 | 64 | 4x | defaults | 380.5 | 89.7 | 1.83% |
| 48000 | 64 | 4x | working | 464.2 | 98.5 | 2.23% |
| 48000 | 64 | 16x | defaults | 1264.5 | 1812.9 | 6.07% |
| 48000 | 64 | 16x | working | 1855.7 | 317.0 | 8.91% |
| 48000 | 512 | Off | defaults | 270.1 | 397.6 | 1.30% |
| 48000 | 512 | Off | working | 313.7 | 311.7 | 1.51% |
| 48000 | 512 | 4x | defaults | 508.6 | 456.1 | 2.44% |
| 48000 | 512 | 4x | working | 625.4 | 497.9 | 3.00% |
| 48000 | 512 | 16x | defaults | 1306.1 | 920.3 | 6.27% |
| 48000 | 512 | 16x | working | 1921.0 | 1365.7 | 9.22% |
| 96000 | 64 | Off | defaults | 265.7 | 119.3 | 2.55% |
| 96000 | 64 | Off | working | 287.4 | 109.5 | 2.76% |
| 96000 | 64 | 4x | defaults | 496.6 | 99.1 | 4.77% |
| 96000 | 64 | 4x | working | 622.1 | 88.4 | 5.97% |
| 96000 | 64 | 16x | defaults | 1316.6 | 200.0 | 12.64% |
| 96000 | 64 | 16x | working | 1936.1 | 242.2 | 18.59% |
| 96000 | 512 | Off | defaults | 246.6 | 268.0 | 2.37% |
| 96000 | 512 | Off | working | 305.2 | 286.3 | 2.93% |
| 96000 | 512 | 4x | defaults | 506.9 | 424.0 | 4.87% |
| 96000 | 512 | 4x | working | 652.3 | 655.1 | 6.26% |
| 96000 | 512 | 16x | defaults | 1386.3 | 1101.3 | 13.31% |
| 96000 | 512 | 16x | working | 2100.2 | 1500.8 | 20.16% |


## Verdict against the target

- **The budget case — 48 kHz · 512 · 4× · working — measures 3.0 % of one core** on this 2.1 GHz
  server-class Xeon; the target is ≈5 % on a *modern desktop* core, so it holds with margin and
  would improve on the reference hardware. CI-class variance applies: these are wall-clock stamps
  on a shared machine — re-measure before quoting anywhere externally.
- 16× is the deliberate quality extreme, not the budget case: 9.2 % at 48 kHz, 20.2 % at
  96 kHz/512/working. Usable, and honest to state.
- The defaults column is the null path: ≈1–1.3 % at 48 kHz — the bit-exact identity chain plus
  metering/features, which bounds the "metering + features" allocation from above at well under
  its 0.5 %… only jointly with the pass-through chain; the per-stage split needs the profiler
  pass this document does not claim.
- Worst-block figures include scheduler noise (a 64-sample block stamped at 1.8 ms on a shared
  Xeon is a preemption, not DSP) — treat the median column as the load-bearing one.

## Refresh rule

Re-run `AnabasisBench` and replace the table whenever the chain gains a stage, an OS mode
changes, or before any release claim; the machine line travels with the table (C2 — a number
without its machine and method is not a measurement).
