# DEPENDENCY_POLICY.md

Repository Governance Policy. Third-party dependency locking and upgrade safety.

## Current dependencies

| Dependency | Pin | Mechanism | Status |
|---|---|---|---|
| **JUCE** | **9.0.1**, pinned by the tag's **IMMUTABLE commit SHA** `e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8` | CMake `FetchContent` (`GIT_SHALLOW`), overridable via `-DANABASIS_JUCE_PATH` | Decided (OQ-001, 2026-07-30) at **9.0.0** / `f8f8864…`, written into `CMakeLists.txt` as `ANABASIS_JUCE_VERSION` + `ANABASIS_JUCE_TAG` at P1; moved to 9.0.1 by **ADR-0028, 2026-08-16** (compliance log below) |
| **pluginval** | latest release (downloaded) | `scripts/run-pluginval.sh` / `.ps1` | pinning it is a tracked improvement, not yet done |
| **C++ standard** | **C++23** | `CMAKE_CXX_STANDARD 23` (a hard `set()`, not a cache variable — an override must fail rather than configure), `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF` | `DEVELOPMENT_BRIEF.md` §2.1 set 20 with a canary; **ADR-0030, 2026-08-22** raised the baseline, removed the `ANABASIS_CXX_STANDARD` seam and retired the canary job, closing OQ-006 |
| **Clang (Linux CI)** | **major 22**, upstream stable | `ANABASIS_CLANG_VERSION` in `.github/workflows/build.yml`; installed by `scripts/setup-llvm-apt.sh` from apt.llvm.org, signing key pinned by identity | **ADR-0031, 2026-08-22.** Not a Dependabot ecosystem; maintained by this row and that ADR. Ubuntu's own archives stop two majors short and ship no `libclang_rt.rtsan`, which the `realtime` job cannot run without |
| Linux system libs | distro packages | `scripts/setup-linux.sh` (ALSA, JACK, X11, FreeType, GTK/WebKit, mesa, **EGL — required by JUCE 9's Linux GL context path**, xvfb) | scaffolded |
| GitHub Actions | **every ref pinned to a COMMIT SHA**, with the version in a trailing comment | `.github/workflows/*` + `.github/actions/*` + Dependabot (`github-actions` ecosystem, split by semver impact) | Floating majors until 0.1.6; **SHA-pinned 2026-08-22** (migration audit item A2-15) — see below |

### Action refs: SHA-pinned since 0.2.0 (2026-08-22)

**Every `uses:` ref in `.github/` names a commit SHA**, with the version it corresponds to in a
trailing comment — Dependabot's own convention, which it reads and rewrites. Local refs
(`./.github/workflows/build.yml`, `./.github/actions/setup-linux-build`) are this repository and are
not dependencies.

**This reverses the position recorded here until 0.1.6**, which read: *"Floating majors are a
deliberate trade — Dependabot tracks them and a first-party GitHub action re-pointing a major tag
maliciously is not the threat model this repository defends against. The one third-party action is
SHA-pinned because it is."* The 0.2.0 migration audit re-took that decision (item A2-15) on an
internal-consistency argument the original framing did not weigh: **JUCE is pinned by immutable
commit SHA precisely "so the dependency cannot change under a re-pointed tag" — and JUCE never sees
a token, while every one of these actions runs on the runner with the job's credentials.** A
repointed `v7` was the more privileged hole of the two. The threat model is not "a first-party
action turns malicious"; it is "a tag is a mutable pointer, and this repository already decided
mutable pointers are unacceptable for the dependency that cannot reach a secret".

Two prior findings are preserved because pinning is what settles them:

- **`actions/dependency-review-action@v5` resolved through a BRANCH, not a tag** — that repository's
  tags run `v4.9.0` → `v5.0.0` with no bare `v5` tag, so `refs/heads/v5` was what the ref matched:
  strictly less immutable than the others, on the one workflow **not** gated behind `preflight` and
  therefore running on every PR. It is now a SHA and the distinction is moot.
- **`microsoft/msvc-code-analysis-action` is pinned AHEAD of its last release** and is the one entry
  Dependabot is told to ignore; `.github/dependabot.yml` carries the reasoning, and
  `.github/workflows/msvc.yml` carries the reason for the pin itself.

The cost is update volume: a bare major moves only when the major does, a SHA moves on every
release. `.github/dependabot.yml` splits the updates by semver impact so patch bumps land on a green
run without a major blocking them.

## Version-lock reasoning

- **JUCE is pinned to an exact IMMUTABLE COMMIT**, not a branch, `latest`, or a mutable tag
  *name*. A tag name can be re-pointed upstream; a SHA cannot. JUCE is the framework for the
  entire DSP (oversampling, filters, `dsp::AudioBlock`), the parameter system (APVTS), the GUI,
  and the plugin-format wrappers — an unpinned bump can silently change DSP behaviour, reported
  latency, the editor/X11 embedding path, and the parameter/state ABI. The pin makes builds
  reproducible and keeps audited behaviour stable.
  Two cache variables carry it: `ANABASIS_JUCE_VERSION` (human-readable) and `ANABASIS_JUCE_TAG`
  (the SHA).
- **`GIT_SHALLOW` + a commit SHA needs verifying at P1 — it is a documented CMake trap.** CMake's
  own `ExternalProject`/`FetchContent` documentation states that `GIT_SHALLOW` expects `GIT_TAG` to
  name a **branch or tag**. Fetching an arbitrary SHA shallowly works only where the server permits
  `uploadpack.allowReachableSHA1InWant` (GitHub does today) *and* CMake is new enough to attempt
  it. Otherwise the likely outcomes are a silent fallback to a full clone (slow, not wrong) or a
  hard configure failure against a mirror that disallows it (wrong, and confusing). The SHA pin is
  not negotiable — it is what makes the dependency immutable — so if the combination misbehaves,
  drop `GIT_SHALLOW`, never the SHA. Check this explicitly when `CMakeLists.txt` lands rather than
  discovering it on someone else's machine.
- **The pin was shared with Anamorph on purpose, and as of 2026-08-16 it is not.** Both products
  sat on JUCE 9.0.0 at the same commit, so a JUCE-attributable behaviour difference between them
  was impossible by construction, one dependency audit covered both, and a Level-5 audition of one
  was a meaningful baseline for the other. The corollary was stated here as a rule — a future bump
  is a **product-family decision**, not a per-repo one, because bumping only Anabasis re-introduces
  exactly the divergence the pin removes. **ADR-0028 bumped only Anabasis**, on the owner's
  directive and with the divergence named rather than glossed: Anamorph is a read-only reference
  from this repository (`AI_AGENT_POLICY.md` §Cross-repository constraint), so no change here can
  carry the sibling along. Until Anamorph follows, three of the four properties above are
  suspended — the shared audit, the transferable audition, and the impossibility of a
  JUCE-attributable difference. What limits the cost is measured rather than assumed: of the
  fifteen JUCE modules Anabasis links, **six are byte-identical between the two tags apart from
  their module-declaration version string, and they are the six that decide how the plug-in sounds
  and what a host sees** — `juce_dsp`, `juce_audio_basics`, `juce_audio_processors`,
  `juce_data_structures`, `juce_audio_utils` and `juce_audio_plugin_client`. So no DSP, parameter,
  state or wrapper behaviour can differ between the products for this reason; the nine that did
  change are GUI, platform-native, file-format and web-view surfaces. ADR-0028 tabulates each with
  its diff size and why it is or is not reachable here, and carries the re-convergence obligation.
  **RE-CONVERGED, 2026-08-30 (observed during the 0.2.6 parity audit):** Anamorph's `main`
  (`fd78c3b`) now pins the **same** tag — `ANAMORPH_JUCE_VERSION "9.0.1"`, commit
  `e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8`, its `CMakeLists.txt:52-53` — so the suspension above
  is lifted: both products again sit on one JUCE commit, a JUCE-attributable behaviour difference
  between them is again impossible by construction, and the shared-audit and transferable-audition
  properties hold. The obligation ADR-0028 carried is discharged by the sibling's own move; the
  family rule stands for the next bump.
- **C++23 is the baseline since ADR-0030 (0.2.0), and modules are still not used** — they remain a
  build-system liability in plugin projects, which is the half of ADR-0008 decision B5 that ADR-0030
  did NOT supersede. The **library** rule is unchanged and still binds: a C++23 library feature
  (`std::expected`, `std::mdspan`, `std::float32_t`, `std::print` in test tooling) is admitted only
  behind feature-test macros (`__cpp_lib_expected`, `__has_include(<mdspan>)`, …) **and a thin
  first-party abstraction**, because the three shipped standard libraries do not implement them on
  the same schedule. Language features carry no such caveat: every CI compiler implements the
  standard the build now requires. C++26 is not targeted.
- ~~**The C++23 canary CI job is early warning, never a gate.**~~ **Retired by ADR-0030.** The
  canary asked "does tomorrow's baseline still compile?"; tomorrow's baseline is today's, and every
  job on all three platforms compiles it as a blocking check. OQ-006 is closed.

## Upgrade rules

1. A JUCE version bump — or a C++ standard baseline change — is a **Build System change** →
   `ARCHITECTURE_REVIEW_GATE.md` + an ADR.
2. After any bump: full self-tests + pluginval at the phase strictness in **both modes** ×3 on all
   three OSes, **and** a manual audition (Level 5). A JUCE change can move DSP/latency/editor
   behaviour invisibly to the headless gate.
3. Re-verify `RELEASE_COMPATIBILITY_CHECKLIST.md` (latency reporting, session reload) after a bump.
4. Prefer the offline path (`-DANABASIS_JUCE_PATH`) for reproducibility in restricted CI.
5. The `JUCE_*` compile definitions (no webview, no curl, no splash, strict ref-counted pointer)
   are part of the dependency contract; changing them is a build change.
6. **Re-verify third-party attribution after any JUCE bump** — the licence inventory is derived
   from the pinned tree, and components vendored inside JUCE's own dependencies do not all appear
   in JUCE's top-level licence file (`RELEASE_POLICY.md`).
7. **Re-read the code that reasons about JUCE INTERNALS, not just the code that calls JUCE.** A
   handful of sites here are correct because of an ordering or a guard inside JUCE that no header
   promises, and each cites `juce_*.cpp` by line against the pinned tree. A bump can move those
   lines without breaking a single call, so nothing fails and the reasoning quietly stops holding.
   The register, to be walked after every bump:

   > **Walked at the 9.0.1 bump (2026-08-16, ADR-0028), and every row survives for a stronger
   > reason than re-reading it.** `juce_PopupMenu.cpp`, `juce_Component.cpp`,
   > `juce_MouseInputSource.cpp`, `juce_ModalComponentManager.cpp`, `juce_TooltipWindow.cpp`,
   > `juce_Button.cpp`, `juce_LookAndFeel_V2.cpp` and `juce_LookAndFeel_V4.cpp` — the files every
   > row below reasons about — are **byte-identical between `f8f8864…` and `e18f7f5…`**, so no
   > assumption here can have moved, and no `juce_*.cpp:line` citation anywhere else in the tree
   > can have shifted either. The one file in that neighbourhood that DID change,
   > `juce_gui_basics/detail/juce_MouseInputSourceImpl.h`, changed `getTargetForGesture` alone
   > (magnify-gesture positioning under global scaling); the `isMouseOver` row turns on
   > `setComponentUnderMouse`, which is untouched. This is what walking the register looks like
   > when the answer is clean: the check is the diff between the two trees, not a re-reading of
   > the prose.

   | Site | What it assumes about JUCE | How it fails if the assumption moves |
   |---|---|---|
   | `PluginEditor.cpp` — `showPresetMenu` counts the preset menu itself (`presetMenusOpen`) | A PARENTED `PopupMenu::MenuWindow` binds the DEFAULT look-and-feel before it is added to its parent, so `preparePopupMenuWindow` never reaches `AnabasisLookAndFeel::onPopupMenuWindowCreated` | The hook starts firing and the window is tracked twice — once by the counter, once in `openMenus`. Bounded: the shield still raises and lowers once, and `dismissTrackedPopupMenus`'s second loop skips an already-exited window on `isCurrentlyModal`. What is left is a doubled count while a menu is open |
   | `PluginEditor.cpp` — `showPresetMenu` raises the shield BEFORE `showMenuAsync` | `addChildComponent` APPENDS, so a menu window created afterwards sits in FRONT of the already-raised shield even though both are always-on-top | **The worst one in this table.** If JUCE ever re-sorted always-on-top children on insert, the shield would sit in front of the preset menu and make it unclickable. No test covers it — a headless suite cannot open a modal menu — so this row IS the coverage |
   | `PluginEditor.cpp` — `dismissOrphanedPopupMenus` re-delivers nothing after `exitModalState` | JUCE re-delivers the mouse-down that exited a modal loop | A dismissal click could act twice, or not reach the control beneath |
   | `PluginEditor.cpp` — `timerCallback`'s `presetMenusOpen` ghost-heal | `PopupMenu::showWithOptionalCallback` calls `enterModalState` for EVERY menu, a parented one included, so "no modal child" really does mean "no preset menu on screen" | **The dangerous direction of this table.** If a parented menu ever stopped entering the modal state, the counter would be zeroed ~83 ms after the menu opened — two 24 Hz ticks — LOWERING THE SHIELD while the menu is still on screen, which re-exposes the exact defect the shield exists to close. The heal was added to stop a lost completion callback stranding the shield; this is its cost |
   | `PluginEditor.cpp` — `healGhostTrackedPopupMenus` prunes a tracked window | A `MenuWindow` is hidden-then-DELETED, never hidden-then-re-shown: `showMenuAsync` enters the modal state with `deleteWhenDismissed = true`, so `ModalComponentManager` destroys it | The heal drops a window that is merely hidden and would be shown again — permanently, since it also detaches the component listener. The shield would not rise for it and `dismissTrackedPopupMenus` would not cancel it at teardown, leaving exactly the orphaned window the mechanism exists to remove. The two-tick grace does not help: a re-shown window is a ghost for both of them |
   | `PluginEditor.cpp` — `popupShield` raised, and TOOLTIP/HOVER targeting | While the shield intercepts, `getComponentAt` resolves it for every pointer position. `TooltipWindow::getTipFor` therefore sees the shield and offers no tip; the 24 Hz `hov` sweep and the animation tick both go through `Component::isMouseOver`, which consults `cachedMouseInsideComponent` ONLY off the message thread and on it iterates the mouse sources asking `getComponentUnderMouse()` — a pointer `setComponentUnderMouse` re-points unconditionally, since the modality early-return sits in `internalMouseEnter/Exit` and skips the callbacks and the cached flag, not the source's tracking. So combo hover art DROPS for the life of the menu, by design, and eases back on the first tick after dismissal | This row asserted the opposite until 2026-08-14 — "hover art survives the raise" — which is a claim about the wrong one of two `isMouseOver` branches, so a bump that CHANGED the behaviour would have looked like agreement. What actually breaks it: `isMouseOver` reading the cached flag on the message thread too (hover would then stick ON, since nothing clears the flag for a blocked component), or `findComponentAt` learning to skip mouse-intercepting siblings (hover would never drop and the tooltip half of this row would go with it) |
   | `LookAndFeel.h` — `drawResizableFrame` suppresses the parented pop-up's doubled edge | `paintOverChildren` calls it with a UNIFORM border equal to `getPopupMenuBorderSize()`, and only when a parent component is set | A non-uniform or differently-sized border stops matching and the doubled edge returns |
   | `PluginEditor.h` — `GatedTooltipWindow` | `TooltipWindow::timerCallback`'s early-show branch ignores `millisecondsBeforeTipAppears` | Tips appear on a different schedule than the gate assumes |
   | `PluginEditor.cpp` — `animToggle`/`tooltipsToggle` bound to `onStateChange` | `Button::valueChanged` calls `setToggleState (v, dontSendNotification, sendNotification)`, so a value arriving through the `referTo`-bound `juce::Value` fires the STATE callback and NOT the click one | The two mirrors stop following a project load: the widget shows the stored value while `uiAnimOn`/`tooltipsOn` keep the previous session's, until the user clicks the switch. `tooltipsOn` is the sole authority on whether a tip appears at all, so it decides the whole feature, not a delay. Nothing fails to compile and no call site changes |

## Adding a dependency

Anabasis does not add a third-party library casually. Before any new dependency:

1. State its **licence** and get owner approval (`DEVELOPMENT_BRIEF.md` §13). Copyleft
   (GPL/AGPL) is excluded by the closed-source product model.
2. Justify why JUCE + first-party code cannot do it — every added dependency is another thing to
   pin, audit, attribute and re-verify on every bump.
3. Anything on the audio path must satisfy `REALTIME_AUDIO_POLICY.md` — an allocating or locking
   library is disqualified regardless of its licence.
4. Pin it by immutable revision, the same way JUCE is pinned.

Dependabot covers the `github-actions` ecosystem only; CMake `FetchContent` is not a Dependabot
ecosystem, and a JUCE bump is deliberately manual and review-gated.

## Compliance log

| Date | Bump | ADR | Rule-2 verification |
|---|---|---|---|
| **2026-08-16** | **JUCE 9.0.0 `f8f8864…` → 9.0.1 `e18f7f5…`** (owner directive; version 0.1.5) | [ADR-0028](../architecture/design-decisions/ADR-0028-juce-901-pin.md) | **Headless half done, audition half OWED.** Linux x86-64 / GCC 13.3 / Release, fresh build tree: both suites green at the same counts as the old pin (1141 checks, 0 failures) and pluginval at the `build.yml` strictness in both modes ×3 with the editor under xvfb. `AnabasisChannelProbe` printed per-channel output for 33 configurations against the built VST3 bundle at both pins and the two runs agree **digit for digit at nine decimal places**, the macro-with-editor scenario included. Also clean at the new pin: valgrind memcheck on both suites (0 errors from 0 contexts each) and the full Clang leg — build, first-party warning gate, the `SIMDRegister` compile canary, both suites, and the probe against the **Clang-LTO'd** bundle, which agrees with the GCC one digit for digit. The other two OSes are CI's half of rule 2 and land with this commit's run. **Rule 2's Level-5 manual audition has NOT been performed** — it is a human sign-off, it is what this rule exists to demand of a JUCE change, and no measurement above substitutes for it. Rules 3, 6 and 7 are discharged in ADR-0028 §Verification |

Every future bump appends a row here with its ADR and the rule-2 verification result. A row whose
rule-2 cell claims a Level-5 audition must name who performed it and when; the first row does not,
because nobody has.
