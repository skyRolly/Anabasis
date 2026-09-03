# 2026-09-03 — PR #28 review + the actionable post-audit findings

Six findings, all closed. Two audit observations, both dispositioned without change.

## 1-2. What 0.2.9 got wrong (both faults, and they are different)

**Too wide.** The guard tested `! std::isfinite`. An infinity is not an unusable number — it is an
out-of-range one, and every clamp on the path already answers it with the endpoint: `jlimit` with 1
and 0, `NormalisableRange::snapToLegalValue` with the range ends, and the `value` path with the same.
0.2.9 declined the endpoint along with the NaN, so a session asking for the rail silently got the
`value` attribute instead and a preset asking for one left the control where it stood. The comment
sitting beside the code said *"Infinities already clamp correctly; NaN is the only value that needs
the test"* — the intent was right and the predicate did not implement it. `std::isnan` does.

**Too narrow.** 0.2.9 guarded the `raw` OVERLAY and left the `value` it falls back TO unguarded.
Measured on that code: with no usable `raw` and a NaN `value`, 50 of 50 controls held a NaN and the
next save wrote every one of them back. The probe that found the original defect only ever poisoned
`raw`, which is exactly why it missed this — the same blind spot the regression test had.

The repair for the second is `adoptParamsTree` dropping an unreadable `value` from the tree before
`replaceState`. That is not a special case bolted on beside the mechanism, it IS the mechanism:
`AudioProcessorValueTreeState` reads
`tree.getProperty (valuePropertyID, getDenormalisedDefaultValue())`
(`juce_AudioProcessorValueTreeState.cpp:413`), so an absent `value` restores the parameter's DEFAULT
— which is precisely `SERIALIZATION_REGISTRY.md`'s rule for a value that cannot be read, reached
through JUCE's own default path rather than a second one.

Measured before → after, all 50 parameters, via a probe linked against the real plugin:

| document | 0.2.9 | now |
|---|---|---|
| `raw=inf`, finite `value` | 7 at 1.0 (fell back) | **50 at 1.0** |
| `raw=-inf`, finite `value` | 7/18 (fell back) | **50 at 0.0** |
| `raw=nan`, finite `value` | falls back | falls back (unchanged) |
| no `raw`, `value=nan` | **50 non-finite, 50 re-saved** | **0 and 0** |
| no `raw`, `value=±inf` | 50 at the endpoint | unchanged |
| finite control | 0.5 | unchanged |

## 3. Why the old test could not see either fault

It corrupted `raw` only and asserted only that nothing was non-finite. The fallback 0.2.9 substituted
for a declined infinity is *also* finite, so no finiteness test could tell an endpoint from it; and
nothing ever poisoned `value`, so the second fault had no path to the assertion at all.

The extension pins the endpoint **per parameter and exactly**, and adds the fallback document
(`raw` removed, `value` NaN) with three separate consequences: the control is finite, it holds the
DEFAULT rather than whatever was live, and a save after it propagates nothing.

Mutation-tested rather than merely run:

| mutation | fails |
|---|---|
| whole 0.2.9 state code restored | 5 checks (2 endpoint + 3 fallback) |
| only the `raw` predicate widened back | exactly the 2 endpoint checks |
| only the fallback repair removed | exactly the 3 fallback checks |

## 4. check-realtime.py failed open, two ways

Reproduced on scratch trees before and after:

| tree | before | now |
|---|---|---|
| rule's file DELETED | `exit=0`, "1 ordering requirement(s) met" | `exit=1`, 1 unreachable |
| rule's file RENAMED | `exit=0`, "1 ordering requirement(s) met" | `exit=1`, 1 unreachable |
| `src/` present but EMPTY | `exit=0`, "0 file(s) scanned" | `exit=2`, refuses an empty set |
| the real repository | `exit=0` | `exit=0`, "1 of 1 verified" |

`scan_required` already caught a rule whose FUNCTION was renamed — it is reading the file the rule
names. It could not catch a rule whose FILE was renamed: no path matched, it was never called, and
absence read as compliance. The count now comes from what was PROVED (`verified`), not from
`len(REQUIRED_ORDER)`, and a configured rule that reached no file fails the gate. The empty-set
refusal is `check-portability.py`'s existing pattern; five of the six sibling gates already had one.

The four new self-test cases go through `scan_repo` against real temporary trees. That is the point:
the existing cases run the classifier over in-memory strings, and three deliberate mutations of it
were all caught while the wiring defect walked past them. Both new guards are themselves
mutation-tested — disabling either fails the self-test.

## 5. sampleRate

`delaySamples` is the **only** route from the rate into an allocation: `wetRing` and `dryRingSize`
both size from it, and every other size in `prepare` comes from `maxBlock` or `numChans`, which have
always been clamped. `maxLookaheadSamples` is a ceil of a product with the rate, so a negative rate
made it negative and `setSize` threw out of `prepareToPlay`, across the wrapper's C ABI where an
exception is a crash rather than a diagnostic.

Railed at the derived delay rather than at `sr`, deliberately: `sr` is also read by the oversampler
latch and the two `setRate` calls, and clamping it there would change what a rate of 0 does today for
no defect anyone has shown. `jmax (0, ...)` returns every positive rate's value unchanged.

## 6. bench.cpp

`line.substr (line.find (':') + 2)` assumed a colon AND a space. Three failures, measured:

| line | before | now |
|---|---|---|
| `model name\t: Intel(R) Xeon` | `"Intel(R) Xeon"` | unchanged |
| `model name:` | **throws `std::out_of_range`** | `""` → scan continues |
| `model name` | `"odel name"` (silently wrong) | `""` → scan continues |
| `model name\t:x` | `""` (value lost) | `"x"` |

Verified through the real path: `AnabasisBench` built and run, and it reports this machine's model
name exactly as before.

## 7. `--assert-discriminating` — no change, and the evidence for it

Six probe invocations; the flag is on the Linux one only. Ran the probe here: **112 configuration
pairs compared, 4 collapsed, 0 undeclared**. All four are the single declared pair
(`field: mix 100, links 0` == `field: mix 100, links 100`) appearing once per (rate, block) group —
an ALGORITHMIC identity, since at 100 % mix the links parameter is inert. Nothing about it is
wrapper- or platform-dependent.

Two further facts argue against a blind change. The discrimination comparison RUNS and PRINTS on
every lane; only the `return 3` is gated on the flag, so a collapse anywhere is visible in that
lane's log and simply is not fatal there. And the cross-wrapper hazard this observation worries about
is already handled by design: the probe matches configurations by DISPLAY NAME, not by parameter id,
because JUCE's VST3 client hashes ids into Steinberg `ParamID`s — the comment at
`tools/channel_probe.cpp:122-129` records that matching by id was the vacuity that ran all eight
configurations at their defaults.

No platform-specific collapse was demonstrated, and none can be from this machine. Left unchanged.

## 8. Build-off tool targets — an intentional boundary

`ANABASIS_BUILD_BENCH` and `ANABASIS_BUILD_PROBE` are both OFF by default. `codeql.yml` configures
`-DANABASIS_BUILD_STANDALONE=OFF` and `msvc.yml` the same, so neither scanner's codemodel contains
`tools/channel_probe.cpp`, `tools/engine_repro.cpp` or `tests/bench.cpp` — confirmed again here, a
scratch configure needed both options ON explicitly before the files appeared in the compile database.

They are not unbuilt, only unscanned: `build.yml:618` configures `-DANABASIS_BUILD_BENCH=ON
-DANABASIS_BUILD_PROBE=ON`, so they compile in CI under the project's warning gates. The boundary is
coherent — the deep scanners analyse the shipping configuration, and none of this code ships. Left
unchanged; the one actionable thing the gap produced, the `bench.cpp` parser, is fixed above.
