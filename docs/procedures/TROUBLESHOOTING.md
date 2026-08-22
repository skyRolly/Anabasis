# TROUBLESHOOTING.md

Common failures and where to look. Build/test/CI details are in `BUILD.md`, `TESTING.md`,
`CI_CD.md`.

## Build / configure

| Symptom | Likely cause | Fix |
|---|---|---|
| CMake cannot find a compiler or Ninja | build deps missing | `scripts/setup-linux.sh` (Ubuntu) |
| `FetchContent` fails to clone JUCE | no network / restricted sandbox | use a local checkout: `-DANABASIS_JUCE_PATH=/path/to/JUCE`; the build otherwise needs `github.com` |
| Missing EGL headers on Linux | **JUCE 9 creates Linux GL contexts via EGL, not GLX** | install `libegl-dev` (already in `setup-linux.sh`) |
| `libwebkit2gtk-4.1-dev` unavailable | the name moves between releases — `4.0` on older Ubuntu, `libwebkitgtk-6.0-dev` on Debian trixie and later | it is a **`full`-profile extra** that nothing here compiles (`JUCE_WEB_BROWSER=0` on every target), so `scripts/setup-linux.sh headless` skips it entirely; otherwise install your release's name by hand |
| `Unable to locate package libfreetype6-dev` | Debian trixie (the `gcc:16` container base) dropped that spelling; Ubuntu keeps it only as a `Provides:` | the scripts use `libfreetype-dev`, which is a real package on both — update a local copy that still says `6` |
| Link errors with duplicated JUCE symbols | the DSP core was made a `STATIC` library | it must be an `INTERFACE` library so its sources compile into each final target (`BUILD.md`) |
| A new wrapper/GUI file builds in the plugin but not in the state tests | the source list was edited in only one place | both targets read the same CMake source-list variable — restore that (`BUILD.md`) |

## Validation (pluginval)

| Symptom | Likely cause | Fix |
|---|---|---|
| `Anabasis.vst3 not found` | not built, or built in a different config | run `scripts/build.sh` first |
| Editor tests fail on a headless Linux box | no display | the script uses `xvfb-run` when available — install `xvfb` |
| Windows step passes suspiciously fast, output garbled | pluginval is a GUI-subsystem app; the call operator does not wait | use `run-pluginval.ps1`, which launches via `System.Diagnostics.Process` and `WaitForExit()` |
| Passes deterministic, fails randomise | an order- or value-dependent defect — usually state restoration | do **not** dismiss it; the randomise mode exists precisely to find these |
| Repeated crash, exit ≥ 128 | a signal crash; retried 3× | if it survives the retries it is treated as a failure — investigate rather than raising the retry count |

## Runtime / DAW

| Symptom | Likely cause | Where to look |
|---|---|---|
| Plugin does not appear in the DAW | scan cache / blocklist | force a rescan and clear any failed-scan entry |
| Timing shifts when the plugin is inserted | reported latency does not match the actual delay | `DSP_POLICY.md` invariant 2; check that oversampling/lookahead changes are **latched**, not applied mid-block |
| A click when changing oversampling or toggling bypass | a discrete switch not crossfaded/ducked | `DSP_POLICY.md` invariant 8 |
| Output exceeds the ceiling | the final safety clamp is being bypassed, or true-peak detection is running below 4× | `DSP_POLICY.md` invariants 3 and 4 — this is a release blocker, not a tuning issue |
| Audible pumping/breathing on static material | the adaptive engine is reacting too fast, or hysteresis is too small | `MODE_AND_ADAPTATION_POLICY.md` invariant 3; verify with **Freeze** engaged — if Freeze removes it, it is adaptation |
| The sound changes when switching Simple ⇄ Advanced | macro-layer contract violation | `MODE_AND_ADAPTATION_POLICY.md` invariant 2 — this is a Hard Stop, not a bug to patch quietly |
| Zipper noise on a parameter move | an unsmoothed parameter | every parameter reaching the DSP is smoothed (`CODE_STYLE.md`) |
| Crackling that disappears with oversampling off | denormals, or per-block work that scales with the oversampling factor | `ScopedNoDenormals` active for the whole block; flush envelope state to zero below a threshold |

## "But it sounds louder, so it's better"

It is not. Compare **loudness-matched** — use the plugin's own loudness-compensated monitoring and
loudness-matched bypass, and use delta monitoring to hear what is being removed. An uncompensated
A/B cannot answer the question (`DEVELOPMENT_BRIEF.md` §3; `DEVELOPMENT.md` §"Judging your own
work honestly").
